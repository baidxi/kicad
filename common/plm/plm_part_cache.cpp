/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "plm_part_cache.h"
#include "plm_cache_manager.h"

#include <nlohmann/json.hpp>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/sstream.h>
#include <wx/wfstream.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <ctime>


namespace
{

const wxString LIBRARY_INDEX_FILE = wxS( "library_index.json" );
const wxString SYMBOLS_SUBDIR = wxS( "symbols" );
const wxString FOOTPRINTS_SUBDIR = wxS( "footprints" );

} // anonymous namespace


bool PLM_CACHED_LIBRARY::NeedsValidation( int64_t aMaxAge ) const
{
    if( !is_valid )
        return true;

    int64_t now = std::time( nullptr );
    int64_t age = now - last_validated;

    return age >= aMaxAge;
}


bool PLM_CACHED_LIBRARY::FileExists() const
{
    return !local_path.IsEmpty() && wxFileName::FileExists( local_path );
}


PLM_PART_CACHE::PLM_PART_CACHE( PLM_CACHE_MANAGER* aCacheManager ) :
        m_cacheManager( aCacheManager )
{
}


PLM_PART_CACHE::~PLM_PART_CACHE()
{
    if( m_initialized )
        SaveIndex();
}


bool PLM_PART_CACHE::Initialize( const wxString& aServerUrl, const wxString& aProjectId )
{
    if( !m_cacheManager || !m_cacheManager->IsInitialized() )
        return false;

    m_serverUrl = aServerUrl;
    m_projectId = aProjectId;

    // Get the library cache directory from cache manager
    wxString libCacheDir = GetLibraryCacheDir();

    if( libCacheDir.IsEmpty() )
        return false;

    // Create subdirectories for symbols and footprints
    wxString symbolsDir = libCacheDir + wxFileName::GetPathSeparator() + SYMBOLS_SUBDIR;
    wxString footprintsDir = libCacheDir + wxFileName::GetPathSeparator() + FOOTPRINTS_SUBDIR;

    if( !wxDir::Exists( symbolsDir ) )
        wxFileName::Mkdir( symbolsDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );

    if( !wxDir::Exists( footprintsDir ) )
        wxFileName::Mkdir( footprintsDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );

    // Load existing index if present
    LoadIndex();

    m_initialized = true;
    return true;
}


wxString PLM_PART_CACHE::GetLibraryCacheDir() const
{
    if( !m_cacheManager )
        return wxEmptyString;

    return m_cacheManager->GetFileCacheDir( m_serverUrl, m_projectId );
}


wxString PLM_PART_CACHE::MakeCacheKey( const wxString& aLibraryId, PLM_LIBRARY_TYPE aType ) const
{
    wxString typePrefix = ( aType == PLM_LIBRARY_TYPE::SYMBOL ) ? wxS( "sym_" ) : wxS( "fp_" );
    return typePrefix + aLibraryId;
}


bool PLM_PART_CACHE::LoadIndex()
{
    wxString libCacheDir = GetLibraryCacheDir();

    if( libCacheDir.IsEmpty() )
        return false;

    wxString indexPath = libCacheDir + wxFileName::GetPathSeparator() + LIBRARY_INDEX_FILE;

    if( !wxFileName::FileExists( indexPath ) )
        return true;  // No index yet, not an error

    wxFileInputStream fileStream( indexPath );

    if( !fileStream.IsOk() )
        return false;

    wxString content;
    wxStringOutputStream stringStream( &content );
    fileStream.Read( stringStream );

    try
    {
        nlohmann::json j = nlohmann::json::parse( content.ToStdString() );

        m_libraries.clear();

        for( const auto& item : j["libraries"] )
        {
            PLM_CACHED_LIBRARY lib;
            lib.library_id = wxString::FromUTF8( item.value( "library_id", "" ) );
            lib.library_name = wxString::FromUTF8( item.value( "library_name", "" ) );
            lib.type = ( item.value( "type", "symbol" ) == "footprint" )
                               ? PLM_LIBRARY_TYPE::FOOTPRINT
                               : PLM_LIBRARY_TYPE::SYMBOL;
            lib.version = wxString::FromUTF8( item.value( "version", "" ) );
            lib.checksum = wxString::FromUTF8( item.value( "checksum", "" ) );
            lib.local_path = wxString::FromUTF8( item.value( "local_path", "" ) );
            lib.cached_at = item.value( "cached_at", static_cast<int64_t>( 0 ) );
            lib.last_validated = item.value( "last_validated", static_cast<int64_t>( 0 ) );
            lib.last_accessed = item.value( "last_accessed", lib.cached_at );
            lib.size_bytes = item.value( "size_bytes", static_cast<size_t>( 0 ) );
            lib.is_valid = item.value( "is_valid", false );

            // Verify file exists
            if( !lib.FileExists() )
                lib.is_valid = false;

            wxString key = MakeCacheKey( lib.library_id, lib.type );
            m_libraries[key] = lib;
        }

        return true;
    }
    catch( const nlohmann::json::exception& e )
    {
        wxLogWarning( wxS( "PLM: Failed to load library index: %s" ),
                      wxString::FromUTF8( e.what() ) );
        m_libraries.clear();
        return false;
    }
}


bool PLM_PART_CACHE::SaveIndex()
{
    wxString libCacheDir = GetLibraryCacheDir();

    if( libCacheDir.IsEmpty() )
        return false;

    wxString indexPath = libCacheDir + wxFileName::GetPathSeparator() + LIBRARY_INDEX_FILE;

    try
    {
        nlohmann::json j;
        j["version"] = 1;
        j["libraries"] = nlohmann::json::array();

        for( const auto& [key, lib] : m_libraries )
        {
            nlohmann::json item;
            item["library_id"] = lib.library_id.ToStdString();
            item["library_name"] = lib.library_name.ToStdString();
            item["type"] = ( lib.type == PLM_LIBRARY_TYPE::FOOTPRINT ) ? "footprint" : "symbol";
            item["version"] = lib.version.ToStdString();
            item["checksum"] = lib.checksum.ToStdString();
            item["local_path"] = lib.local_path.ToStdString();
            item["cached_at"] = lib.cached_at;
            item["last_validated"] = lib.last_validated;
            item["last_accessed"] = lib.last_accessed;
            item["size_bytes"] = lib.size_bytes;
            item["is_valid"] = lib.is_valid;

            j["libraries"].push_back( item );
        }

        std::string jsonStr = j.dump( 2 );

        wxFileOutputStream fileStream( indexPath );

        if( !fileStream.IsOk() )
            return false;

        fileStream.Write( jsonStr.c_str(), jsonStr.size() );

        return fileStream.IsOk();
    }
    catch( const nlohmann::json::exception& e )
    {
        wxLogError( wxS( "PLM: Failed to save library index: %s" ),
                    wxString::FromUTF8( e.what() ) );
        return false;
    }
}


bool PLM_PART_CACHE::CacheLibrary( const wxString& aLibraryId, const wxString& aLibraryName,
                                    PLM_LIBRARY_TYPE aType, const wxString& aData,
                                    const wxString& aVersion, const wxString& aChecksum )
{
    if( !m_initialized )
        return false;

    wxString libCacheDir = GetLibraryCacheDir();

    if( libCacheDir.IsEmpty() )
        return false;

    // Determine subdirectory based on type
    wxString subdir =
            ( aType == PLM_LIBRARY_TYPE::SYMBOL ) ? SYMBOLS_SUBDIR : FOOTPRINTS_SUBDIR;
    wxString extension = ( aType == PLM_LIBRARY_TYPE::SYMBOL ) ? wxS( "kicad_sym" )
                                                               : wxS( "kicad_mod" );

    wxString filename = aLibraryId + wxS( "." ) + extension;
    wxString localPath = libCacheDir + wxFileName::GetPathSeparator() + subdir
                         + wxFileName::GetPathSeparator() + filename;

    // Write library file
    wxFileOutputStream fileStream( localPath );

    if( !fileStream.IsOk() )
    {
        wxLogError( wxS( "PLM: Failed to create library file: %s" ), localPath );
        return false;
    }

    std::string dataStr = aData.ToStdString();
    fileStream.Write( dataStr.c_str(), dataStr.size() );

    if( !fileStream.IsOk() )
    {
        wxLogError( wxS( "PLM: Failed to write library file: %s" ), localPath );
        return false;
    }

    // Create cache entry
    PLM_CACHED_LIBRARY lib( aLibraryId, aLibraryName, aType );
    lib.version = aVersion;
    lib.checksum = aChecksum;
    lib.local_path = localPath;
    lib.cached_at = std::time( nullptr );
    lib.last_validated = lib.cached_at;
    lib.last_accessed = lib.cached_at;
    lib.size_bytes = aData.length();
    lib.is_valid = true;

    wxString key = MakeCacheKey( aLibraryId, aType );
    m_libraries[key] = lib;

    // Enforce size limit after adding
    EnforceSizeLimit();

    // Save index
    SaveIndex();

    return true;
}


wxString PLM_PART_CACHE::GetLibraryPath( const wxString& aLibraryId,
                                          PLM_LIBRARY_TYPE aType ) const
{
    wxString key = MakeCacheKey( aLibraryId, aType );
    auto it = m_libraries.find( key );

    if( it == m_libraries.end() )
    {
        RecordMiss();
        return wxEmptyString;
    }

    if( !it->second.is_valid || !it->second.FileExists() )
    {
        RecordMiss();
        return wxEmptyString;
    }

    RecordHit();
    return it->second.local_path;
}


bool PLM_PART_CACHE::HasLibrary( const wxString& aLibraryId, PLM_LIBRARY_TYPE aType ) const
{
    wxString key = MakeCacheKey( aLibraryId, aType );
    auto it = m_libraries.find( key );

    if( it == m_libraries.end() )
        return false;

    return it->second.is_valid && it->second.FileExists();
}


bool PLM_PART_CACHE::GetLibraryInfo( const wxString& aLibraryId, PLM_LIBRARY_TYPE aType,
                                      PLM_CACHED_LIBRARY& aLibrary ) const
{
    wxString key = MakeCacheKey( aLibraryId, aType );
    auto it = m_libraries.find( key );

    if( it == m_libraries.end() )
        return false;

    aLibrary = it->second;
    return true;
}


std::vector<PLM_CACHED_LIBRARY> PLM_PART_CACHE::GetCachedLibraries() const
{
    std::vector<PLM_CACHED_LIBRARY> result;
    result.reserve( m_libraries.size() );

    for( const auto& [key, lib] : m_libraries )
        result.push_back( lib );

    return result;
}


std::vector<PLM_CACHED_LIBRARY> PLM_PART_CACHE::GetLibrariesByType( PLM_LIBRARY_TYPE aType ) const
{
    std::vector<PLM_CACHED_LIBRARY> result;

    for( const auto& [key, lib] : m_libraries )
    {
        if( lib.type == aType )
            result.push_back( lib );
    }

    return result;
}


wxString PLM_PART_CACHE::GetLibraryVersion( const wxString& aLibraryId,
                                             PLM_LIBRARY_TYPE aType ) const
{
    wxString key = MakeCacheKey( aLibraryId, aType );
    auto it = m_libraries.find( key );

    if( it == m_libraries.end() )
        return wxEmptyString;

    return it->second.version;
}


wxString PLM_PART_CACHE::GetLibraryChecksum( const wxString& aLibraryId,
                                              PLM_LIBRARY_TYPE aType ) const
{
    wxString key = MakeCacheKey( aLibraryId, aType );
    auto it = m_libraries.find( key );

    if( it == m_libraries.end() )
        return wxEmptyString;

    return it->second.checksum;
}


bool PLM_PART_CACHE::UpdateLibraryVersion( const wxString& aLibraryId, PLM_LIBRARY_TYPE aType,
                                            const wxString& aVersion, const wxString& aChecksum )
{
    wxString key = MakeCacheKey( aLibraryId, aType );
    auto it = m_libraries.find( key );

    if( it == m_libraries.end() )
        return false;

    it->second.version = aVersion;
    it->second.checksum = aChecksum;
    it->second.last_validated = std::time( nullptr );

    SaveIndex();

    return true;
}


PLM_LIBRARY_VALIDATION PLM_PART_CACHE::ValidateLibrary( const wxString& aLibraryId,
                                                         PLM_LIBRARY_TYPE aType,
                                                         const wxString& aServerVersion,
                                                         const wxString& aServerChecksum )
{
    wxString key = MakeCacheKey( aLibraryId, aType );
    auto it = m_libraries.find( key );

    if( it == m_libraries.end() )
        return PLM_LIBRARY_VALIDATION::NotCached( aLibraryId );

    PLM_CACHED_LIBRARY& lib = it->second;

    // Check if file exists
    if( !lib.FileExists() )
    {
        lib.is_valid = false;
        SaveIndex();
        return PLM_LIBRARY_VALIDATION::NotCached( aLibraryId );
    }

    // Compare checksums first (most reliable)
    if( !aServerChecksum.IsEmpty() && !lib.checksum.IsEmpty() )
    {
        if( lib.checksum != aServerChecksum )
        {
            return PLM_LIBRARY_VALIDATION::NeedsUpdate( aLibraryId, aServerVersion,
                                                         aServerChecksum );
        }
    }

    // If no checksums, compare versions
    if( !aServerVersion.IsEmpty() && !lib.version.IsEmpty() )
    {
        if( lib.version != aServerVersion )
        {
            return PLM_LIBRARY_VALIDATION::NeedsUpdate( aLibraryId, aServerVersion,
                                                         aServerChecksum );
        }
    }

    // Cache is valid
    return PLM_LIBRARY_VALIDATION::Valid( aLibraryId );
}


void PLM_PART_CACHE::MarkValidated( const wxString& aLibraryId, PLM_LIBRARY_TYPE aType )
{
    wxString key = MakeCacheKey( aLibraryId, aType );
    auto it = m_libraries.find( key );

    if( it != m_libraries.end() )
    {
        it->second.last_validated = std::time( nullptr );
        it->second.is_valid = true;
        SaveIndex();
    }
}


bool PLM_PART_CACHE::InvalidateLibrary( const wxString& aLibraryId, PLM_LIBRARY_TYPE aType )
{
    wxString key = MakeCacheKey( aLibraryId, aType );
    auto it = m_libraries.find( key );

    if( it == m_libraries.end() )
        return false;

    // Remove the file
    if( it->second.FileExists() )
        wxRemoveFile( it->second.local_path );

    m_libraries.erase( it );
    SaveIndex();

    return true;
}


size_t PLM_PART_CACHE::InvalidateAllSymbols()
{
    size_t count = 0;
    std::vector<wxString> keysToRemove;

    for( const auto& [key, lib] : m_libraries )
    {
        if( lib.type == PLM_LIBRARY_TYPE::SYMBOL )
        {
            if( lib.FileExists() )
                wxRemoveFile( lib.local_path );

            keysToRemove.push_back( key );
            ++count;
        }
    }

    for( const wxString& key : keysToRemove )
        m_libraries.erase( key );

    if( count > 0 )
        SaveIndex();

    return count;
}


size_t PLM_PART_CACHE::InvalidateAllFootprints()
{
    size_t count = 0;
    std::vector<wxString> keysToRemove;

    for( const auto& [key, lib] : m_libraries )
    {
        if( lib.type == PLM_LIBRARY_TYPE::FOOTPRINT )
        {
            if( lib.FileExists() )
                wxRemoveFile( lib.local_path );

            keysToRemove.push_back( key );
            ++count;
        }
    }

    for( const wxString& key : keysToRemove )
        m_libraries.erase( key );

    if( count > 0 )
        SaveIndex();

    return count;
}


size_t PLM_PART_CACHE::InvalidateAll()
{
    size_t count = m_libraries.size();

    for( const auto& [key, lib] : m_libraries )
    {
        if( lib.FileExists() )
            wxRemoveFile( lib.local_path );
    }

    m_libraries.clear();
    SaveIndex();

    return count;
}


size_t PLM_PART_CACHE::GetTotalCacheSize() const
{
    size_t total = 0;

    for( const auto& [key, lib] : m_libraries )
        total += lib.size_bytes;

    return total;
}


size_t PLM_PART_CACHE::GetLibraryCount() const
{
    return m_libraries.size();
}


size_t PLM_PART_CACHE::GetSymbolLibraryCount() const
{
    size_t count = 0;

    for( const auto& [key, lib] : m_libraries )
    {
        if( lib.type == PLM_LIBRARY_TYPE::SYMBOL )
            ++count;
    }

    return count;
}


size_t PLM_PART_CACHE::GetFootprintLibraryCount() const
{
    size_t count = 0;

    for( const auto& [key, lib] : m_libraries )
    {
        if( lib.type == PLM_LIBRARY_TYPE::FOOTPRINT )
            ++count;
    }

    return count;
}


void PLM_PART_CACHE::SetMaxCacheSize( size_t aMaxBytes )
{
    m_maxCacheSize = aMaxBytes;

    if( m_initialized && m_maxCacheSize > 0 )
        EnforceSizeLimit();
}


size_t PLM_PART_CACHE::EvictToSize( size_t aTargetBytes )
{
    size_t currentSize = GetTotalCacheSize();

    if( currentSize <= aTargetBytes )
        return 0;

    // Build a list sorted by last_accessed (oldest first)
    std::vector<std::pair<wxString, int64_t>> entries;
    entries.reserve( m_libraries.size() );

    for( const auto& [key, lib] : m_libraries )
        entries.emplace_back( key, lib.last_accessed );

    std::sort( entries.begin(), entries.end(),
               []( const auto& a, const auto& b ) { return a.second < b.second; } );

    size_t evicted = 0;

    for( const auto& [key, timestamp] : entries )
    {
        if( currentSize <= aTargetBytes )
            break;

        auto it = m_libraries.find( key );

        if( it == m_libraries.end() )
            continue;

        size_t libSize = it->second.size_bytes;

        // Remove the file
        if( it->second.FileExists() )
            wxRemoveFile( it->second.local_path );

        m_libraries.erase( it );
        currentSize -= libSize;
        ++evicted;
        ++m_evictionCount;
    }

    if( evicted > 0 )
        SaveIndex();

    return evicted;
}


size_t PLM_PART_CACHE::ClearCache()
{
    size_t count = InvalidateAll();

    // Reset statistics
    m_hitCount = 0;
    m_missCount = 0;
    m_evictionCount = 0;

    return count;
}


PLM_CACHE_STATISTICS PLM_PART_CACHE::GetStatistics() const
{
    PLM_CACHE_STATISTICS stats;

    stats.total_size_bytes = GetTotalCacheSize();
    stats.library_count = m_libraries.size();
    stats.symbol_count = GetSymbolLibraryCount();
    stats.footprint_count = GetFootprintLibraryCount();
    stats.max_size_bytes = m_maxCacheSize;
    stats.hit_count = m_hitCount;
    stats.miss_count = m_missCount;
    stats.eviction_count = m_evictionCount;

    int64_t oldest = INT64_MAX;
    int64_t newest = 0;

    for( const auto& [key, lib] : m_libraries )
    {
        if( lib.is_valid )
            ++stats.valid_count;
        else
            ++stats.invalid_count;

        if( lib.cached_at < oldest )
            oldest = lib.cached_at;

        if( lib.cached_at > newest )
            newest = lib.cached_at;
    }

    stats.oldest_entry = ( oldest == INT64_MAX ) ? 0 : oldest;
    stats.newest_entry = newest;

    return stats;
}


void PLM_PART_CACHE::ResetStatistics()
{
    m_hitCount = 0;
    m_missCount = 0;
    m_evictionCount = 0;
}


void PLM_PART_CACHE::EnforceSizeLimit()
{
    if( m_maxCacheSize == 0 )
        return;

    size_t currentSize = GetTotalCacheSize();

    if( currentSize > m_maxCacheSize )
        EvictToSize( m_maxCacheSize );
}


void PLM_PART_CACHE::TouchEntry( const wxString& aKey )
{
    auto it = m_libraries.find( aKey );

    if( it != m_libraries.end() )
        it->second.last_accessed = std::time( nullptr );
}
