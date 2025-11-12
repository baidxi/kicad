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

#include "plm_cache_manager.h"

#include <paths.h>
#include <nlohmann/json.hpp>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/sstream.h>
#include <wx/wfstream.h>

#include <algorithm>
#include <functional>
#include <iomanip>
#include <sstream>


namespace
{

// Directory names for cache structure
const wxString PLM_CACHE_SUBDIR = wxS( "plm-cache" );
const wxString METADATA_SUBDIR = wxS( "metadata" );
const wxString FILES_SUBDIR = wxS( "files" );
const wxString PREVIEWS_SUBDIR = wxS( "previews" );


/**
 * Convert a hash value to a hex string.
 */
wxString HashToHexString( size_t aHash )
{
    std::ostringstream ss;
    ss << std::hex << std::setfill( '0' ) << std::setw( 16 ) << aHash;
    return wxString::FromUTF8( ss.str() );
}


/**
 * Normalize a URL for consistent hashing.
 *
 * Removes trailing slashes and converts to lowercase.
 */
wxString NormalizeUrl( const wxString& aUrl )
{
    wxString normalized = aUrl.Lower();

    while( normalized.EndsWith( wxS( "/" ) ) )
        normalized.RemoveLast();

    return normalized;
}


/**
 * Recursively calculate directory size.
 */
size_t CalculateDirSize( const wxString& aPath )
{
    size_t totalSize = 0;

    if( !wxDir::Exists( aPath ) )
        return 0;

    wxDir dir( aPath );

    if( !dir.IsOpened() )
        return 0;

    wxString filename;
    bool cont = dir.GetFirst( &filename );

    while( cont )
    {
        wxString fullPath = aPath + wxFileName::GetPathSeparator() + filename;

        if( wxFileName::DirExists( fullPath ) )
        {
            totalSize += CalculateDirSize( fullPath );
        }
        else if( wxFileName::FileExists( fullPath ) )
        {
            wxFileName fn( fullPath );
            totalSize += fn.GetSize().GetValue();
        }

        cont = dir.GetNext( &filename );
    }

    return totalSize;
}


/**
 * Recursively count files in directory.
 */
size_t CountDirFiles( const wxString& aPath )
{
    size_t count = 0;

    if( !wxDir::Exists( aPath ) )
        return 0;

    wxDir dir( aPath );

    if( !dir.IsOpened() )
        return 0;

    wxString filename;
    bool cont = dir.GetFirst( &filename );

    while( cont )
    {
        wxString fullPath = aPath + wxFileName::GetPathSeparator() + filename;

        if( wxFileName::DirExists( fullPath ) )
        {
            count += CountDirFiles( fullPath );
        }
        else if( wxFileName::FileExists( fullPath ) )
        {
            ++count;
        }

        cont = dir.GetNext( &filename );
    }

    return count;
}


/**
 * Recursively remove a directory.
 */
bool RemoveDirRecursive( const wxString& aPath )
{
    if( !wxDir::Exists( aPath ) )
        return true;

    wxDir dir( aPath );

    if( !dir.IsOpened() )
        return false;

    wxString filename;
    bool cont = dir.GetFirst( &filename );

    while( cont )
    {
        wxString fullPath = aPath + wxFileName::GetPathSeparator() + filename;

        if( wxFileName::DirExists( fullPath ) )
        {
            if( !RemoveDirRecursive( fullPath ) )
                return false;
        }
        else if( wxFileName::FileExists( fullPath ) )
        {
            if( !wxRemoveFile( fullPath ) )
                return false;
        }

        cont = dir.GetNext( &filename );
    }

    return wxRmdir( aPath );
}

} // anonymous namespace


PLM_CACHE_MANAGER::PLM_CACHE_MANAGER()
{
}


PLM_CACHE_MANAGER::~PLM_CACHE_MANAGER()
{
}


bool PLM_CACHE_MANAGER::Initialize( const PLM_CACHE_CONFIG& aConfig )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    m_config = aConfig;

    wxString userCachePath = PATHS::GetUserCachePath();

    if( userCachePath.IsEmpty() )
    {
        wxLogError( wxS( "PLM: Unable to get user cache path" ) );
        return false;
    }

    m_cacheRootDir = userCachePath + wxFileName::GetPathSeparator() + PLM_CACHE_SUBDIR;

    if( !EnsureDirectoryExists( m_cacheRootDir ) )
    {
        wxLogError( wxS( "PLM: Unable to create cache root directory: %s" ), m_cacheRootDir );
        return false;
    }

    if( !m_config.cache_directory.IsEmpty() )
    {
        m_cacheRootDir = m_config.cache_directory;

        if( !EnsureDirectoryExists( m_cacheRootDir ) )
        {
            wxLogError( wxS( "PLM: Unable to create custom cache directory: %s" ), m_cacheRootDir );
            return false;
        }
    }

    m_initialized = true;
    return true;
}


wxString PLM_CACHE_MANAGER::GetServerCacheDir( const wxString& aServerUrl )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return wxEmptyString;

    wxString serverHash = HashServerUrl( aServerUrl );
    wxString serverDir = m_cacheRootDir + wxFileName::GetPathSeparator() + serverHash;

    EnsureDirectoryExists( serverDir );

    return serverDir;
}


wxString PLM_CACHE_MANAGER::GetProjectCacheDir( const wxString& aServerUrl,
                                                  const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    wxString serverDir = GetServerCacheDir( aServerUrl );

    if( serverDir.IsEmpty() )
        return wxEmptyString;

    wxString projectHash = HashProjectId( aProjectId );
    wxString projectDir = serverDir + wxFileName::GetPathSeparator() + projectHash;

    EnsureDirectoryExists( projectDir );

    return projectDir;
}


wxString PLM_CACHE_MANAGER::GetMetadataCacheDir( const wxString& aServerUrl,
                                                   const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    wxString projectDir = GetProjectCacheDir( aServerUrl, aProjectId );

    if( projectDir.IsEmpty() )
        return wxEmptyString;

    wxString metadataDir = projectDir + wxFileName::GetPathSeparator() + METADATA_SUBDIR;

    EnsureDirectoryExists( metadataDir );

    return metadataDir;
}


wxString PLM_CACHE_MANAGER::GetFileCacheDir( const wxString& aServerUrl,
                                               const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    wxString projectDir = GetProjectCacheDir( aServerUrl, aProjectId );

    if( projectDir.IsEmpty() )
        return wxEmptyString;

    wxString fileDir = projectDir + wxFileName::GetPathSeparator() + FILES_SUBDIR;

    EnsureDirectoryExists( fileDir );

    return fileDir;
}


wxString PLM_CACHE_MANAGER::GetPreviewCacheDir( const wxString& aServerUrl,
                                                  const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    wxString projectDir = GetProjectCacheDir( aServerUrl, aProjectId );

    if( projectDir.IsEmpty() )
        return wxEmptyString;

    wxString previewDir = projectDir + wxFileName::GetPathSeparator() + PREVIEWS_SUBDIR;

    EnsureDirectoryExists( previewDir );

    return previewDir;
}


bool PLM_CACHE_MANAGER::HasProjectCache( const wxString& aServerUrl,
                                           const wxString& aProjectId ) const
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return false;

    wxString serverHash = HashServerUrl( aServerUrl );
    wxString projectHash = HashProjectId( aProjectId );

    wxString projectDir = m_cacheRootDir + wxFileName::GetPathSeparator() + serverHash
                          + wxFileName::GetPathSeparator() + projectHash;

    return wxDir::Exists( projectDir );
}


wxString PLM_CACHE_MANAGER::HashServerUrl( const wxString& aServerUrl )
{
    wxString normalized = NormalizeUrl( aServerUrl );
    size_t hash = std::hash<std::wstring>{}( normalized.ToStdWstring() );
    return HashToHexString( hash );
}


wxString PLM_CACHE_MANAGER::HashProjectId( const wxString& aProjectId )
{
    size_t hash = std::hash<std::wstring>{}( aProjectId.ToStdWstring() );
    return HashToHexString( hash );
}


wxString PLM_CACHE_MANAGER::GenerateCacheKey( const wxString& aEndpoint,
                                                const std::vector<wxString>& aParams )
{
    wxString combined = aEndpoint;

    for( const wxString& param : aParams )
        combined += wxS( "|" ) + param;

    size_t hash = std::hash<std::wstring>{}( combined.ToStdWstring() );
    return HashToHexString( hash );
}


bool PLM_CACHE_MANAGER::RemoveProjectCache( const wxString& aServerUrl,
                                              const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return false;

    wxString serverHash = HashServerUrl( aServerUrl );
    wxString projectHash = HashProjectId( aProjectId );

    wxString projectDir = m_cacheRootDir + wxFileName::GetPathSeparator() + serverHash
                          + wxFileName::GetPathSeparator() + projectHash;

    return RemoveDirectory( projectDir );
}


bool PLM_CACHE_MANAGER::RemoveServerCache( const wxString& aServerUrl )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return false;

    wxString serverHash = HashServerUrl( aServerUrl );
    wxString serverDir = m_cacheRootDir + wxFileName::GetPathSeparator() + serverHash;

    return RemoveDirectory( serverDir );
}


size_t PLM_CACHE_MANAGER::CleanupOrphanedCaches(
        const std::set<wxString>& aActiveServers,
        const std::map<wxString, std::set<wxString>>& aActiveProjects )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return 0;

    size_t removed = 0;

    // Build set of active server hashes
    std::set<wxString> activeServerHashes;

    for( const wxString& server : aActiveServers )
        activeServerHashes.insert( HashServerUrl( server ) );

    // Build map of server hash to active project hashes
    std::map<wxString, std::set<wxString>> activeProjectHashes;

    for( const auto& [server, projects] : aActiveProjects )
    {
        wxString serverHash = HashServerUrl( server );

        for( const wxString& project : projects )
            activeProjectHashes[serverHash].insert( HashProjectId( project ) );
    }

    // Scan cache directory for servers
    wxDir rootDir( m_cacheRootDir );

    if( !rootDir.IsOpened() )
        return 0;

    wxString serverDirName;
    bool cont = rootDir.GetFirst( &serverDirName, wxEmptyString, wxDIR_DIRS );

    while( cont )
    {
        wxString serverDirPath =
                m_cacheRootDir + wxFileName::GetPathSeparator() + serverDirName;

        // If server is not active, remove entire server cache
        if( activeServerHashes.find( serverDirName ) == activeServerHashes.end() )
        {
            if( RemoveDirectory( serverDirPath ) )
                ++removed;

            cont = rootDir.GetNext( &serverDirName );
            continue;
        }

        // Server is active, check projects
        wxDir serverDir( serverDirPath );

        if( serverDir.IsOpened() )
        {
            auto projectIt = activeProjectHashes.find( serverDirName );
            const std::set<wxString>& activeProjects =
                    ( projectIt != activeProjectHashes.end() ) ? projectIt->second
                                                                : std::set<wxString>();

            wxString projectDirName;
            bool projCont = serverDir.GetFirst( &projectDirName, wxEmptyString, wxDIR_DIRS );

            while( projCont )
            {
                if( activeProjects.find( projectDirName ) == activeProjects.end() )
                {
                    wxString projectPath =
                            serverDirPath + wxFileName::GetPathSeparator() + projectDirName;

                    if( RemoveDirectory( projectPath ) )
                        ++removed;
                }

                projCont = serverDir.GetNext( &projectDirName );
            }
        }

        cont = rootDir.GetNext( &serverDirName );
    }

    return removed;
}


bool PLM_CACHE_MANAGER::ClearAllCaches()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return false;

    bool success = RemoveDirectory( m_cacheRootDir );

    if( success )
        EnsureDirectoryExists( m_cacheRootDir );

    m_stats.Reset();

    return success;
}


size_t PLM_CACHE_MANAGER::GetCacheSize() const
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return 0;

    return CalculateDirectorySize( m_cacheRootDir );
}


size_t PLM_CACHE_MANAGER::GetCacheFileCount() const
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return 0;

    return CountFiles( m_cacheRootDir );
}


size_t PLM_CACHE_MANAGER::EnforceSizeLimits()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return 0;

    size_t maxSizeBytes = m_config.MaxSizeBytes();

    // If max size is 0, limits are disabled
    if( maxSizeBytes == 0 && m_config.max_entries == 0 )
        return 0;

    size_t currentSize = GetCacheSize();
    size_t currentCount = m_cacheIndex.size();
    size_t removed = 0;

    // Check if we need to enforce limits
    bool needsSizeReduction = ( maxSizeBytes > 0 && currentSize > maxSizeBytes );
    bool needsCountReduction = ( m_config.max_entries > 0 && currentCount > m_config.max_entries );

    if( !needsSizeReduction && !needsCountReduction )
        return 0;

    // Sort by LRU (oldest first) for eviction
    SortIndexByLRU();

    // Evict entries until within limits
    while( !m_cacheIndex.empty() )
    {
        currentSize = GetCacheSize();
        currentCount = m_cacheIndex.size();

        needsSizeReduction = ( maxSizeBytes > 0 && currentSize > maxSizeBytes );
        needsCountReduction = ( m_config.max_entries > 0 && currentCount > m_config.max_entries );

        if( !needsSizeReduction && !needsCountReduction )
            break;

        // Remove the oldest entry (first in sorted list)
        CacheIndexEntry& entry = m_cacheIndex.front();

        if( wxFileName::FileExists( entry.filePath ) )
        {
            wxRemoveFile( entry.filePath );
            m_stats.RecordEviction();
            ++removed;
        }

        m_cacheIndex.erase( m_cacheIndex.begin() );
    }

    if( removed > 0 )
    {
        m_stats.entry_count = m_cacheIndex.size();
        m_stats.size_bytes = GetCacheSize();
    }

    return removed;
}


size_t PLM_CACHE_MANAGER::RemoveExpiredEntries()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized || !m_config.use_ttl )
        return 0;

    int64_t now = std::time( nullptr );
    size_t removed = 0;

    // Collect expired entries
    std::vector<wxString> expiredFiles;

    for( const CacheIndexEntry& entry : m_cacheIndex )
    {
        if( entry.expiresAt > 0 && entry.expiresAt <= now )
            expiredFiles.push_back( entry.filePath );
    }

    // Remove expired files
    for( const wxString& filePath : expiredFiles )
    {
        if( wxFileName::FileExists( filePath ) )
        {
            if( wxRemoveFile( filePath ) )
            {
                RemoveFromIndex( filePath );
                m_stats.RecordEviction();
                ++removed;
            }
        }
        else
        {
            // File already gone, just remove from index
            RemoveFromIndex( filePath );
            ++removed;
        }
    }

    if( removed > 0 )
    {
        m_stats.entry_count = m_cacheIndex.size();
        m_stats.size_bytes = GetCacheSize();
    }

    return removed;
}


size_t PLM_CACHE_MANAGER::RunPeriodicCleanup()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return 0;

    size_t totalRemoved = 0;

    // Step 1: Remove expired entries
    totalRemoved += RemoveExpiredEntries();

    // Step 2: Enforce size limits (LRU eviction)
    totalRemoved += EnforceSizeLimits();

    // Step 3: Verify integrity (remove orphaned index entries)
    totalRemoved += VerifyCacheIntegrity();

    // Step 4: Update statistics
    m_stats.size_bytes = GetCacheSize();
    m_stats.entry_count = m_cacheIndex.size();

    // Find oldest entry timestamp
    m_stats.oldest_entry = 0;

    for( const CacheIndexEntry& entry : m_cacheIndex )
    {
        if( m_stats.oldest_entry == 0 || entry.lastAccessed < m_stats.oldest_entry )
            m_stats.oldest_entry = entry.lastAccessed;
    }

    // Save updated index if anything changed
    if( totalRemoved > 0 )
        SaveCacheIndex();

    return totalRemoved;
}


bool PLM_CACHE_MANAGER::EnsureDirectoryExists( const wxString& aPath )
{
    if( wxDir::Exists( aPath ) )
        return true;

    return wxFileName::Mkdir( aPath, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );
}


bool PLM_CACHE_MANAGER::RemoveDirectory( const wxString& aPath )
{
    return RemoveDirRecursive( aPath );
}


size_t PLM_CACHE_MANAGER::CalculateDirectorySize( const wxString& aPath ) const
{
    return CalculateDirSize( aPath );
}


size_t PLM_CACHE_MANAGER::CountFiles( const wxString& aPath ) const
{
    return CountDirFiles( aPath );
}


//-----------------------------------------------------------------------------
// Metadata Cache Operations
//-----------------------------------------------------------------------------

wxString PLM_CACHE_MANAGER::GetMetadataFilePath( const wxString& aServerUrl,
                                                   const wxString& aProjectId,
                                                   const wxString& aCacheKey )
{
    wxString metadataDir = GetMetadataCacheDir( aServerUrl, aProjectId );

    if( metadataDir.IsEmpty() )
        return wxEmptyString;

    return metadataDir + wxFileName::GetPathSeparator() + aCacheKey + wxS( ".json" );
}


bool PLM_CACHE_MANAGER::ReadCacheEntry( const wxString& aFilePath, PLM_CACHE_ENTRY& aEntry )
{
    if( !wxFileName::FileExists( aFilePath ) )
        return false;

    wxFileInputStream fileStream( aFilePath );

    if( !fileStream.IsOk() )
        return false;

    wxString content;
    wxStringOutputStream stringStream( &content );
    fileStream.Read( stringStream );

    try
    {
        nlohmann::json j = nlohmann::json::parse( content.ToStdString() );

        aEntry.key = wxString::FromUTF8( j.value( "key", "" ) );
        aEntry.data = wxString::FromUTF8( j.value( "data", "" ) );
        aEntry.etag = wxString::FromUTF8( j.value( "etag", "" ) );
        aEntry.content_type = wxString::FromUTF8( j.value( "content_type", "application/json" ) );
        aEntry.expires_at = j.value( "expires_at", static_cast<int64_t>( 0 ) );
        aEntry.last_validated = j.value( "last_validated", static_cast<int64_t>( 0 ) );
        aEntry.created_at = j.value( "created_at", static_cast<int64_t>( 0 ) );
        aEntry.size_bytes = j.value( "size_bytes", static_cast<size_t>( 0 ) );

        return true;
    }
    catch( const nlohmann::json::exception& e )
    {
        wxLogWarning( wxS( "PLM: Failed to parse cache entry %s: %s" ), aFilePath,
                      wxString::FromUTF8( e.what() ) );
        return false;
    }
}


bool PLM_CACHE_MANAGER::WriteCacheEntry( const wxString& aFilePath,
                                           const PLM_CACHE_ENTRY& aEntry )
{
    try
    {
        nlohmann::json j;
        j["key"] = aEntry.key.ToStdString();
        j["data"] = aEntry.data.ToStdString();
        j["etag"] = aEntry.etag.ToStdString();
        j["content_type"] = aEntry.content_type.ToStdString();
        j["expires_at"] = aEntry.expires_at;
        j["last_validated"] = aEntry.last_validated;
        j["created_at"] = aEntry.created_at;
        j["size_bytes"] = aEntry.size_bytes;

        std::string jsonStr = j.dump( 2 );

        wxFileOutputStream fileStream( aFilePath );

        if( !fileStream.IsOk() )
            return false;

        fileStream.Write( jsonStr.c_str(), jsonStr.size() );

        return fileStream.IsOk();
    }
    catch( const nlohmann::json::exception& e )
    {
        wxLogError( wxS( "PLM: Failed to write cache entry %s: %s" ), aFilePath,
                    wxString::FromUTF8( e.what() ) );
        return false;
    }
}


bool PLM_CACHE_MANAGER::StoreMetadata( const wxString& aServerUrl, const wxString& aProjectId,
                                         const wxString& aCacheKey, const wxString& aData,
                                         const wxString& aEtag, int64_t aTtlSeconds )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return false;

    wxString filePath = GetMetadataFilePath( aServerUrl, aProjectId, aCacheKey );

    if( filePath.IsEmpty() )
        return false;

    PLM_CACHE_ENTRY entry;
    entry.key = aCacheKey;
    entry.data = aData;
    entry.etag = aEtag;
    entry.content_type = wxS( "application/json" );
    entry.created_at = std::time( nullptr );
    entry.last_validated = entry.created_at;
    entry.size_bytes = aData.length();

    int64_t ttl = ( aTtlSeconds > 0 ) ? aTtlSeconds : m_config.fallback_ttl_seconds;
    entry.SetTTL( ttl );

    if( WriteCacheEntry( filePath, entry ) )
    {
        m_stats.size_bytes += entry.size_bytes;
        ++m_stats.entry_count;
        return true;
    }

    return false;
}


bool PLM_CACHE_MANAGER::RetrieveMetadata( const wxString& aServerUrl, const wxString& aProjectId,
                                            const wxString& aCacheKey, PLM_CACHE_ENTRY& aEntry )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
    {
        m_stats.RecordMiss();
        return false;
    }

    wxString filePath = GetMetadataFilePath( aServerUrl, aProjectId, aCacheKey );

    if( filePath.IsEmpty() )
    {
        m_stats.RecordMiss();
        return false;
    }

    if( !ReadCacheEntry( filePath, aEntry ) )
    {
        m_stats.RecordMiss();
        return false;
    }

    if( m_config.use_ttl && aEntry.IsExpired() )
    {
        m_stats.RecordMiss();
        return false;
    }

    m_stats.RecordHit();
    return true;
}


bool PLM_CACHE_MANAGER::HasValidMetadata( const wxString& aServerUrl, const wxString& aProjectId,
                                            const wxString& aCacheKey, wxString& aEtag )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return false;

    wxString filePath = GetMetadataFilePath( aServerUrl, aProjectId, aCacheKey );

    if( filePath.IsEmpty() || !wxFileName::FileExists( filePath ) )
        return false;

    PLM_CACHE_ENTRY entry;

    if( !ReadCacheEntry( filePath, entry ) )
    {
        // Corrupt cache file, remove it
        wxRemoveFile( filePath );
        return false;
    }

    if( m_config.use_ttl && entry.IsExpired() )
        return false;

    aEtag = entry.etag;
    return true;
}


bool PLM_CACHE_MANAGER::InvalidateMetadata( const wxString& aServerUrl,
                                              const wxString& aProjectId,
                                              const wxString& aCacheKey )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return false;

    wxString filePath = GetMetadataFilePath( aServerUrl, aProjectId, aCacheKey );

    if( filePath.IsEmpty() || !wxFileName::FileExists( filePath ) )
        return false;

    if( wxRemoveFile( filePath ) )
    {
        m_stats.RecordEviction();
        return true;
    }

    return false;
}


size_t PLM_CACHE_MANAGER::InvalidateProjectMetadata( const wxString& aServerUrl,
                                                       const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return 0;

    wxString metadataDir = GetMetadataCacheDir( aServerUrl, aProjectId );

    if( metadataDir.IsEmpty() || !wxDir::Exists( metadataDir ) )
        return 0;

    size_t removed = 0;
    wxDir dir( metadataDir );

    if( !dir.IsOpened() )
        return 0;

    wxString filename;
    bool cont = dir.GetFirst( &filename, wxS( "*.json" ), wxDIR_FILES );

    while( cont )
    {
        wxString fullPath = metadataDir + wxFileName::GetPathSeparator() + filename;

        if( wxRemoveFile( fullPath ) )
        {
            ++removed;
            m_stats.RecordEviction();
        }

        cont = dir.GetNext( &filename );
    }

    return removed;
}


size_t PLM_CACHE_MANAGER::InvalidateServerMetadata( const wxString& aServerUrl )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return 0;

    wxString serverHash = HashServerUrl( aServerUrl );
    wxString serverDir = m_cacheRootDir + wxFileName::GetPathSeparator() + serverHash;

    if( !wxDir::Exists( serverDir ) )
        return 0;

    size_t totalRemoved = 0;
    wxDir dir( serverDir );

    if( !dir.IsOpened() )
        return 0;

    // Iterate through all project directories under this server
    wxString projectDirName;
    bool cont = dir.GetFirst( &projectDirName, wxEmptyString, wxDIR_DIRS );

    while( cont )
    {
        wxString metadataDir = serverDir + wxFileName::GetPathSeparator() + projectDirName
                               + wxFileName::GetPathSeparator() + wxS( "metadata" );

        if( wxDir::Exists( metadataDir ) )
        {
            wxDir metaDir( metadataDir );

            if( metaDir.IsOpened() )
            {
                wxString filename;
                bool fileCont = metaDir.GetFirst( &filename, wxS( "*.json" ), wxDIR_FILES );

                while( fileCont )
                {
                    wxString fullPath =
                            metadataDir + wxFileName::GetPathSeparator() + filename;

                    if( wxRemoveFile( fullPath ) )
                    {
                        ++totalRemoved;
                        m_stats.RecordEviction();
                    }

                    fileCont = metaDir.GetNext( &filename );
                }
            }
        }

        cont = dir.GetNext( &projectDirName );
    }

    return totalRemoved;
}


size_t PLM_CACHE_MANAGER::InvalidateCascade( const wxString& aServerUrl, const wxString& aProjectId,
                                              const wxString& aCacheKeyPrefix )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return 0;

    wxString metadataDir = GetMetadataCacheDir( aServerUrl, aProjectId );

    if( metadataDir.IsEmpty() || !wxDir::Exists( metadataDir ) )
        return 0;

    size_t removed = 0;
    wxDir dir( metadataDir );

    if( !dir.IsOpened() )
        return 0;

    // Collect files to remove since we can't modify the directory while iterating
    std::vector<wxString> filesToRemove;
    wxString filename;
    bool cont = dir.GetFirst( &filename, wxS( "*.json" ), wxDIR_FILES );

    while( cont )
    {
        // Remove the .json extension to get the cache key
        wxString cacheKey = filename;

        if( cacheKey.EndsWith( wxS( ".json" ) ) )
            cacheKey = cacheKey.Left( cacheKey.length() - 5 );

        // Check if this key starts with the prefix
        if( cacheKey.StartsWith( aCacheKeyPrefix ) )
            filesToRemove.push_back( metadataDir + wxFileName::GetPathSeparator() + filename );

        cont = dir.GetNext( &filename );
    }

    // Now remove the collected files
    for( const wxString& path : filesToRemove )
    {
        if( wxRemoveFile( path ) )
        {
            ++removed;
            m_stats.RecordEviction();
        }
    }

    return removed;
}


size_t PLM_CACHE_MANAGER::InvalidateByPattern( const wxString& aServerUrl,
                                                const wxString& aProjectId,
                                                const wxString& aPattern )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized || aPattern.IsEmpty() )
        return 0;

    wxString metadataDir = GetMetadataCacheDir( aServerUrl, aProjectId );

    if( metadataDir.IsEmpty() || !wxDir::Exists( metadataDir ) )
        return 0;

    size_t removed = 0;
    wxDir dir( metadataDir );

    if( !dir.IsOpened() )
        return 0;

    // Collect files to remove since we can't modify the directory while iterating
    std::vector<wxString> filesToRemove;
    wxString filename;
    bool cont = dir.GetFirst( &filename, wxS( "*.json" ), wxDIR_FILES );

    while( cont )
    {
        // Remove the .json extension to get the cache key
        wxString cacheKey = filename;

        if( cacheKey.EndsWith( wxS( ".json" ) ) )
            cacheKey = cacheKey.Left( cacheKey.length() - 5 );

        // Check if this key contains the pattern
        if( cacheKey.Contains( aPattern ) )
            filesToRemove.push_back( metadataDir + wxFileName::GetPathSeparator() + filename );

        cont = dir.GetNext( &filename );
    }

    // Now remove the collected files
    for( const wxString& path : filesToRemove )
    {
        if( wxRemoveFile( path ) )
        {
            ++removed;
            m_stats.RecordEviction();
        }
    }

    return removed;
}


bool PLM_CACHE_MANAGER::TouchMetadata( const wxString& aServerUrl, const wxString& aProjectId,
                                         const wxString& aCacheKey )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return false;

    wxString filePath = GetMetadataFilePath( aServerUrl, aProjectId, aCacheKey );

    if( filePath.IsEmpty() || !wxFileName::FileExists( filePath ) )
        return false;

    PLM_CACHE_ENTRY entry;

    if( !ReadCacheEntry( filePath, entry ) )
        return false;

    entry.MarkValidated();

    if( m_config.use_ttl )
        entry.SetTTL( m_config.fallback_ttl_seconds );

    m_stats.RecordValidation( true );

    return WriteCacheEntry( filePath, entry );
}


//-----------------------------------------------------------------------------
// ETag and Conditional Request Support
//-----------------------------------------------------------------------------

PLM_CACHE_MANAGER::CACHE_LOOKUP_RESULT PLM_CACHE_MANAGER::ConditionalLookup(
        const wxString& aServerUrl, const wxString& aProjectId, const wxString& aCacheKey,
        PLM_CACHE_ENTRY& aEntry )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
    {
        m_stats.RecordMiss();
        return CACHE_LOOKUP_RESULT::MISS;
    }

    wxString filePath = GetMetadataFilePath( aServerUrl, aProjectId, aCacheKey );

    if( filePath.IsEmpty() || !wxFileName::FileExists( filePath ) )
    {
        m_stats.RecordMiss();
        return CACHE_LOOKUP_RESULT::MISS;
    }

    if( !ReadCacheEntry( filePath, aEntry ) )
    {
        // Corrupt cache file, remove it
        wxRemoveFile( filePath );
        m_stats.RecordMiss();
        return CACHE_LOOKUP_RESULT::MISS;
    }

    // Check if entry needs revalidation based on TTL or ETag
    if( m_config.use_etag && aEntry.HasETag() )
    {
        // If we have an ETag, check if we need to revalidate
        if( aEntry.NeedsRevalidation( m_config.revalidation_interval ) )
        {
            // Entry exists but should be revalidated with server
            return CACHE_LOOKUP_RESULT::NEEDS_VALIDATION;
        }
    }

    if( m_config.use_ttl && aEntry.IsExpired() )
    {
        // TTL expired, but we still have the data for potential revalidation
        if( aEntry.HasETag() )
        {
            return CACHE_LOOKUP_RESULT::NEEDS_VALIDATION;
        }

        // No ETag, treat as miss
        m_stats.RecordMiss();
        return CACHE_LOOKUP_RESULT::MISS;
    }

    m_stats.RecordHit();
    return CACHE_LOOKUP_RESULT::HIT;
}


wxString PLM_CACHE_MANAGER::GetCachedETag( const wxString& aServerUrl, const wxString& aProjectId,
                                            const wxString& aCacheKey )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return wxEmptyString;

    wxString filePath = GetMetadataFilePath( aServerUrl, aProjectId, aCacheKey );

    if( filePath.IsEmpty() || !wxFileName::FileExists( filePath ) )
        return wxEmptyString;

    PLM_CACHE_ENTRY entry;

    if( !ReadCacheEntry( filePath, entry ) )
        return wxEmptyString;

    return entry.etag;
}


bool PLM_CACHE_MANAGER::HandleNotModified( const wxString& aServerUrl, const wxString& aProjectId,
                                             const wxString& aCacheKey )
{
    // This is essentially the same as TouchMetadata, but with explicit
    // semantics for HTTP 304 handling
    return TouchMetadata( aServerUrl, aProjectId, aCacheKey );
}


bool PLM_CACHE_MANAGER::HandleFreshResponse( const wxString& aServerUrl,
                                               const wxString& aProjectId,
                                               const wxString& aCacheKey, const wxString& aData,
                                               const wxString& aEtag, int64_t aTtlSeconds )
{
    // This is essentially StoreMetadata with explicit semantics for HTTP 200 handling
    return StoreMetadata( aServerUrl, aProjectId, aCacheKey, aData, aEtag, aTtlSeconds );
}


bool PLM_CACHE_MANAGER::SupportsETagValidation( const wxString& aServerUrl,
                                                  const wxString& aProjectId,
                                                  const wxString& aCacheKey )
{
    if( !m_config.use_etag )
        return false;

    wxString etag = GetCachedETag( aServerUrl, aProjectId, aCacheKey );
    return !etag.IsEmpty();
}


//-----------------------------------------------------------------------------
// TTL and Force Refresh Support
//-----------------------------------------------------------------------------

bool PLM_CACHE_MANAGER::StoreTypedMetadata( const wxString& aServerUrl, const wxString& aProjectId,
                                              const wxString& aCacheKey, const wxString& aData,
                                              PLM_CACHE_TYPE aCacheType, const wxString& aEtag )
{
    int64_t ttl = m_config.GetTTLForType( aCacheType );
    return StoreMetadata( aServerUrl, aProjectId, aCacheKey, aData, aEtag, ttl );
}


bool PLM_CACHE_MANAGER::RetrieveMetadataEx( const wxString& aServerUrl, const wxString& aProjectId,
                                              const wxString& aCacheKey, PLM_CACHE_ENTRY& aEntry,
                                              bool aForceRefresh )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
    {
        m_stats.RecordMiss();
        return false;
    }

    wxString filePath = GetMetadataFilePath( aServerUrl, aProjectId, aCacheKey );

    if( filePath.IsEmpty() )
    {
        m_stats.RecordMiss();
        return false;
    }

    if( !ReadCacheEntry( filePath, aEntry ) )
    {
        m_stats.RecordMiss();
        return false;
    }

    // If force refresh, we return the entry but mark it as needing validation
    // The caller should use the entry's ETag for a conditional request
    if( aForceRefresh )
    {
        // Don't count as hit since we're forcing revalidation
        return true;
    }

    if( m_config.use_ttl && aEntry.IsExpired() )
    {
        m_stats.RecordMiss();
        return false;
    }

    m_stats.RecordHit();
    return true;
}


bool PLM_CACHE_MANAGER::ForceRefresh( const wxString& aServerUrl, const wxString& aProjectId,
                                        const wxString& aCacheKey )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return false;

    wxString filePath = GetMetadataFilePath( aServerUrl, aProjectId, aCacheKey );

    if( filePath.IsEmpty() || !wxFileName::FileExists( filePath ) )
        return false;

    PLM_CACHE_ENTRY entry;

    if( !ReadCacheEntry( filePath, entry ) )
        return false;

    // Set expiration to now, forcing revalidation on next access
    entry.expires_at = std::time( nullptr );

    return WriteCacheEntry( filePath, entry );
}


size_t PLM_CACHE_MANAGER::ForceRefreshProject( const wxString& aServerUrl,
                                                 const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return 0;

    wxString metadataDir = GetMetadataCacheDir( aServerUrl, aProjectId );

    if( metadataDir.IsEmpty() || !wxDir::Exists( metadataDir ) )
        return 0;

    size_t count = 0;
    wxDir dir( metadataDir );

    if( !dir.IsOpened() )
        return 0;

    wxString filename;
    bool cont = dir.GetFirst( &filename, wxS( "*.json" ), wxDIR_FILES );

    while( cont )
    {
        wxString fullPath = metadataDir + wxFileName::GetPathSeparator() + filename;

        PLM_CACHE_ENTRY entry;

        if( ReadCacheEntry( fullPath, entry ) )
        {
            entry.expires_at = std::time( nullptr );

            if( WriteCacheEntry( fullPath, entry ) )
                ++count;
        }

        cont = dir.GetNext( &filename );
    }

    return count;
}


bool PLM_CACHE_MANAGER::IsExpired( const wxString& aServerUrl, const wxString& aProjectId,
                                     const wxString& aCacheKey )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return true;

    wxString filePath = GetMetadataFilePath( aServerUrl, aProjectId, aCacheKey );

    if( filePath.IsEmpty() || !wxFileName::FileExists( filePath ) )
        return true;

    PLM_CACHE_ENTRY entry;

    if( !ReadCacheEntry( filePath, entry ) )
        return true;

    return entry.IsExpired();
}


int64_t PLM_CACHE_MANAGER::GetRemainingTTL( const wxString& aServerUrl, const wxString& aProjectId,
                                              const wxString& aCacheKey )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return -1;

    wxString filePath = GetMetadataFilePath( aServerUrl, aProjectId, aCacheKey );

    if( filePath.IsEmpty() || !wxFileName::FileExists( filePath ) )
        return -1;

    PLM_CACHE_ENTRY entry;

    if( !ReadCacheEntry( filePath, entry ) )
        return -1;

    if( entry.expires_at == 0 )
        return -1;

    int64_t now = std::time( nullptr );
    int64_t remaining = entry.expires_at - now;

    return ( remaining > 0 ) ? remaining : -1;
}


//-----------------------------------------------------------------------------
// Statistics
//-----------------------------------------------------------------------------

void PLM_CACHE_MANAGER::RefreshStats()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return;

    // Preserve hit/miss/validation counters
    uint64_t savedHits = m_stats.hits;
    uint64_t savedMisses = m_stats.misses;
    uint64_t savedValidations = m_stats.validations;
    uint64_t savedValidationHits = m_stats.validation_hits;
    uint64_t savedEvictions = m_stats.evictions;

    // Update size and count from actual cache state
    m_stats.size_bytes = GetCacheSize();
    m_stats.entry_count = m_cacheIndex.size();

    // Restore preserved counters
    m_stats.hits = savedHits;
    m_stats.misses = savedMisses;
    m_stats.validations = savedValidations;
    m_stats.validation_hits = savedValidationHits;
    m_stats.evictions = savedEvictions;

    // Update oldest entry timestamp
    m_stats.oldest_entry = 0;

    for( const CacheIndexEntry& entry : m_cacheIndex )
    {
        if( m_stats.oldest_entry == 0 || entry.lastAccessed < m_stats.oldest_entry )
            m_stats.oldest_entry = entry.lastAccessed;
    }
}


wxString PLM_CACHE_MANAGER::GetDiagnosticReport() const
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    wxString report;
    report += wxS( "PLM Cache Diagnostic Report\n" );
    report += wxS( "===========================\n\n" );

    // Basic info
    report += wxString::Format( wxS( "Cache Root: %s\n" ), m_cacheRootDir );
    report += wxString::Format( wxS( "Initialized: %s\n" ), m_initialized ? wxS( "Yes" ) : wxS( "No" ) );
    report += wxS( "\n" );

    // Statistics from PLM_CACHE_STATS
    report += m_stats.GetDiagnosticReport();
    report += wxS( "\n" );

    // Configuration
    report += wxS( "Configuration:\n" );
    report += wxString::Format( wxS( "  Max Size: %zu MB\n" ), m_config.max_size_mb );
    report += wxString::Format( wxS( "  Max Entries: %zu\n" ), m_config.max_entries );
    report += wxString::Format( wxS( "  Default TTL: %lld seconds\n" ),
                                static_cast<long long>( m_config.fallback_ttl_seconds ) );
    report += wxString::Format( wxS( "  Use ETag: %s\n" ), m_config.use_etag ? wxS( "Yes" ) : wxS( "No" ) );
    report += wxString::Format( wxS( "  Use TTL: %s\n" ), m_config.use_ttl ? wxS( "Yes" ) : wxS( "No" ) );
    report += wxS( "\n" );

    // Health status
    report += wxString::Format( wxS( "Health Status: %s\n" ), GetHealthStatus() );

    return report;
}


wxString PLM_CACHE_MANAGER::GetHealthStatus() const
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return wxS( "not initialized" );

    double hitRate = m_stats.HitRate();
    size_t maxBytes = m_config.MaxSizeBytes();

    // Check if cache is over capacity
    if( maxBytes > 0 && m_stats.size_bytes > maxBytes )
        return wxS( "over capacity" );

    // Evaluate based on hit rate
    if( m_stats.TotalRequests() < 10 )
        return wxS( "insufficient data" );

    if( hitRate >= 70.0 )
        return wxS( "healthy" );
    else if( hitRate >= 40.0 )
        return wxS( "degraded" );
    else
        return wxS( "poor" );
}


void PLM_CACHE_MANAGER::LogStats() const
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    wxLogMessage( wxS( "PLM Cache Statistics:" ) );
    wxLogMessage( wxS( "  Entries: %zu, Size: %s" ),
                  m_stats.entry_count, m_stats.GetSizeString() );
    wxLogMessage( wxS( "  Hit Rate: %.1f%% (%llu/%llu)" ),
                  m_stats.HitRate(),
                  static_cast<unsigned long long>( m_stats.hits ),
                  static_cast<unsigned long long>( m_stats.TotalRequests() ) );
    wxLogMessage( wxS( "  Validation Efficiency: %.1f%%" ), m_stats.ValidationEfficiency() );
    wxLogMessage( wxS( "  Evictions: %llu" ),
                  static_cast<unsigned long long>( m_stats.evictions ) );
    wxLogMessage( wxS( "  Health: %s" ), GetHealthStatus() );
}


//-----------------------------------------------------------------------------
// Persistence
//-----------------------------------------------------------------------------

wxString PLM_CACHE_MANAGER::GetCacheIndexPath() const
{
    if( m_cacheRootDir.IsEmpty() )
        return wxEmptyString;

    return m_cacheRootDir + wxFileName::GetPathSeparator() + wxS( "cache_index.json" );
}


bool PLM_CACHE_MANAGER::SaveCacheIndex()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return false;

    wxString indexPath = GetCacheIndexPath();

    if( indexPath.IsEmpty() )
        return false;

    try
    {
        nlohmann::json j;
        j["version"] = 1;
        j["saved_at"] = std::time( nullptr );
        j["entries"] = nlohmann::json::array();

        for( const CacheIndexEntry& entry : m_cacheIndex )
        {
            nlohmann::json item;
            item["server_url"] = entry.serverUrl.ToStdString();
            item["project_id"] = entry.projectId.ToStdString();
            item["cache_key"] = entry.cacheKey.ToStdString();
            item["file_path"] = entry.filePath.ToStdString();
            item["expires_at"] = entry.expiresAt;
            item["last_accessed"] = entry.lastAccessed;

            j["entries"].push_back( item );
        }

        // Also save statistics
        j["stats"]["hits"] = m_stats.hits;
        j["stats"]["misses"] = m_stats.misses;
        j["stats"]["validations"] = m_stats.validations;
        j["stats"]["validation_hits"] = m_stats.validation_hits;
        j["stats"]["evictions"] = m_stats.evictions;

        std::string jsonStr = j.dump( 2 );

        wxFileOutputStream fileStream( indexPath );

        if( !fileStream.IsOk() )
            return false;

        fileStream.Write( jsonStr.c_str(), jsonStr.size() );

        return fileStream.IsOk();
    }
    catch( const nlohmann::json::exception& e )
    {
        wxLogError( wxS( "PLM: Failed to save cache index: %s" ),
                    wxString::FromUTF8( e.what() ) );
        return false;
    }
}


bool PLM_CACHE_MANAGER::LoadCacheIndex()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return false;

    wxString indexPath = GetCacheIndexPath();

    if( indexPath.IsEmpty() )
        return false;

    if( !wxFileName::FileExists( indexPath ) )
        return true;  // No index yet is not an error

    wxFileInputStream fileStream( indexPath );

    if( !fileStream.IsOk() )
        return false;

    wxString content;
    wxStringOutputStream stringStream( &content );
    fileStream.Read( stringStream );

    try
    {
        nlohmann::json j = nlohmann::json::parse( content.ToStdString() );

        m_cacheIndex.clear();

        // Load entries
        for( const auto& item : j["entries"] )
        {
            CacheIndexEntry entry;
            entry.serverUrl = wxString::FromUTF8( item.value( "server_url", "" ) );
            entry.projectId = wxString::FromUTF8( item.value( "project_id", "" ) );
            entry.cacheKey = wxString::FromUTF8( item.value( "cache_key", "" ) );
            entry.filePath = wxString::FromUTF8( item.value( "file_path", "" ) );
            entry.expiresAt = item.value( "expires_at", static_cast<int64_t>( 0 ) );
            entry.lastAccessed = item.value( "last_accessed", static_cast<int64_t>( 0 ) );

            // Verify file exists
            if( wxFileName::FileExists( entry.filePath ) )
                m_cacheIndex.push_back( entry );
        }

        // Load statistics
        if( j.contains( "stats" ) )
        {
            const auto& stats = j["stats"];
            m_stats.hits = stats.value( "hits", static_cast<uint64_t>( 0 ) );
            m_stats.misses = stats.value( "misses", static_cast<uint64_t>( 0 ) );
            m_stats.validations = stats.value( "validations", static_cast<uint64_t>( 0 ) );
            m_stats.validation_hits = stats.value( "validation_hits", static_cast<uint64_t>( 0 ) );
            m_stats.evictions = stats.value( "evictions", static_cast<uint64_t>( 0 ) );
        }

        m_stats.entry_count = m_cacheIndex.size();

        return true;
    }
    catch( const nlohmann::json::exception& e )
    {
        wxLogWarning( wxS( "PLM: Failed to parse cache index, rebuilding: %s" ),
                      wxString::FromUTF8( e.what() ) );
        m_cacheIndex.clear();

        // Try to rebuild from disk
        RebuildCacheIndex();
        return true;
    }
}


size_t PLM_CACHE_MANAGER::VerifyCacheIntegrity()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return 0;

    size_t removedCount = 0;

    // Remove entries with missing files
    auto it = m_cacheIndex.begin();

    while( it != m_cacheIndex.end() )
    {
        if( !wxFileName::FileExists( it->filePath ) )
        {
            it = m_cacheIndex.erase( it );
            ++removedCount;
        }
        else
        {
            ++it;
        }
    }

    if( removedCount > 0 )
    {
        m_stats.entry_count = m_cacheIndex.size();
        SaveCacheIndex();
    }

    return removedCount;
}


size_t PLM_CACHE_MANAGER::ScanProjectCacheDir( const wxString& aServerHash,
                                                 const wxString& aProjectHash )
{
    wxString projectDir = m_cacheRootDir + wxFileName::GetPathSeparator() + aServerHash
                          + wxFileName::GetPathSeparator() + aProjectHash;

    wxString metadataDir = projectDir + wxFileName::GetPathSeparator() + wxS( "metadata" );

    if( !wxDir::Exists( metadataDir ) )
        return 0;

    size_t count = 0;
    wxDir dir( metadataDir );

    if( !dir.IsOpened() )
        return 0;

    wxString filename;
    bool cont = dir.GetFirst( &filename, wxS( "*.json" ), wxDIR_FILES );

    while( cont )
    {
        wxString fullPath = metadataDir + wxFileName::GetPathSeparator() + filename;

        PLM_CACHE_ENTRY cacheEntry;

        if( ReadCacheEntry( fullPath, cacheEntry ) )
        {
            CacheIndexEntry indexEntry;
            indexEntry.serverUrl = aServerHash;  // We only have hash, not original URL
            indexEntry.projectId = aProjectHash;
            indexEntry.cacheKey = cacheEntry.key;
            indexEntry.filePath = fullPath;
            indexEntry.expiresAt = cacheEntry.expires_at;
            indexEntry.lastAccessed = cacheEntry.last_validated;

            m_cacheIndex.push_back( indexEntry );
            ++count;
        }

        cont = dir.GetNext( &filename );
    }

    return count;
}


size_t PLM_CACHE_MANAGER::RebuildCacheIndex()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_initialized )
        return 0;

    m_cacheIndex.clear();

    if( !wxDir::Exists( m_cacheRootDir ) )
        return 0;

    size_t totalCount = 0;
    wxDir rootDir( m_cacheRootDir );

    if( !rootDir.IsOpened() )
        return 0;

    // Scan server directories
    wxString serverHash;
    bool serverCont = rootDir.GetFirst( &serverHash, wxEmptyString, wxDIR_DIRS );

    while( serverCont )
    {
        // Skip the index file
        if( serverHash == wxS( "cache_index.json" ) )
        {
            serverCont = rootDir.GetNext( &serverHash );
            continue;
        }

        wxString serverDir = m_cacheRootDir + wxFileName::GetPathSeparator() + serverHash;
        wxDir serverDirObj( serverDir );

        if( serverDirObj.IsOpened() )
        {
            // Scan project directories
            wxString projectHash;
            bool projectCont = serverDirObj.GetFirst( &projectHash, wxEmptyString, wxDIR_DIRS );

            while( projectCont )
            {
                totalCount += ScanProjectCacheDir( serverHash, projectHash );
                projectCont = serverDirObj.GetNext( &projectHash );
            }
        }

        serverCont = rootDir.GetNext( &serverHash );
    }

    m_stats.entry_count = m_cacheIndex.size();

    // Save the rebuilt index
    SaveCacheIndex();

    wxLogMessage( wxS( "PLM: Rebuilt cache index with %zu entries" ), totalCount );

    return totalCount;
}


//-----------------------------------------------------------------------------
// Helper Methods for Cleanup
//-----------------------------------------------------------------------------

void PLM_CACHE_MANAGER::SortIndexByLRU()
{
    std::sort( m_cacheIndex.begin(), m_cacheIndex.end(),
               []( const CacheIndexEntry& a, const CacheIndexEntry& b )
               {
                   // Sort by last accessed time, oldest first
                   return a.lastAccessed < b.lastAccessed;
               } );
}


void PLM_CACHE_MANAGER::UpdateIndexAccessTime( const wxString& aFilePath )
{
    for( CacheIndexEntry& entry : m_cacheIndex )
    {
        if( entry.filePath == aFilePath )
        {
            entry.lastAccessed = std::time( nullptr );
            return;
        }
    }
}


bool PLM_CACHE_MANAGER::RemoveFromIndex( const wxString& aFilePath )
{
    auto it = std::find_if( m_cacheIndex.begin(), m_cacheIndex.end(),
                            [&aFilePath]( const CacheIndexEntry& entry )
                            {
                                return entry.filePath == aFilePath;
                            } );

    if( it != m_cacheIndex.end() )
    {
        m_cacheIndex.erase( it );
        return true;
    }

    return false;
}
