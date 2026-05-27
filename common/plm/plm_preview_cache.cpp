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

#include "plm_preview_cache.h"
#include "plm_cache_manager.h"

#include <nlohmann/json.hpp>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/log.h>
#include <wx/mstream.h>
#include <wx/sstream.h>
#include <wx/wfstream.h>

#include <algorithm>
#include <ctime>


namespace
{

const wxString PREVIEW_INDEX_FILE = wxS( "preview_index.json" );

} // anonymous namespace


bool PLM_CACHED_PREVIEW::FileExists() const
{
    return !local_path.IsEmpty() && wxFileName::FileExists( local_path );
}


PLM_PREVIEW_CACHE::PLM_PREVIEW_CACHE( PLM_CACHE_MANAGER* aCacheManager ) :
        m_cacheManager( aCacheManager )
{
}


PLM_PREVIEW_CACHE::~PLM_PREVIEW_CACHE()
{
    if( m_initialized )
        SaveIndex();
}


bool PLM_PREVIEW_CACHE::Initialize( const wxString& aServerUrl, const wxString& aProjectId,
                                     const PLM_PREVIEW_CACHE_CONFIG& aConfig )
{
    if( !m_cacheManager || !m_cacheManager->IsInitialized() )
        return false;

    m_serverUrl = aServerUrl;
    m_projectId = aProjectId;
    m_config = aConfig;

    wxString previewDir = GetPreviewCacheDir();

    if( previewDir.IsEmpty() )
        return false;

    // Load existing index if present
    LoadIndex();

    // Update stats
    m_stats.entry_count = m_previews.size();
    m_stats.current_size = GetTotalSize();

    m_initialized = true;
    return true;
}


wxString PLM_PREVIEW_CACHE::GetPreviewCacheDir() const
{
    if( !m_cacheManager )
        return wxEmptyString;

    return m_cacheManager->GetPreviewCacheDir( m_serverUrl, m_projectId );
}


wxString PLM_PREVIEW_CACHE::GetPreviewFilePath( const wxString& aFileId ) const
{
    wxString previewDir = GetPreviewCacheDir();

    if( previewDir.IsEmpty() )
        return wxEmptyString;

    // Use file ID hash for filename to handle special characters
    wxString filename = PLM_CACHE_MANAGER::HashProjectId( aFileId ) + wxS( ".png" );

    return previewDir + wxFileName::GetPathSeparator() + filename;
}


bool PLM_PREVIEW_CACHE::LoadIndex()
{
    wxString previewDir = GetPreviewCacheDir();

    if( previewDir.IsEmpty() )
        return false;

    wxString indexPath = previewDir + wxFileName::GetPathSeparator() + PREVIEW_INDEX_FILE;

    if( !wxFileName::FileExists( indexPath ) )
        return true;  // No index yet

    wxFileInputStream fileStream( indexPath );

    if( !fileStream.IsOk() )
        return false;

    wxString content;
    wxStringOutputStream stringStream( &content );
    fileStream.Read( stringStream );

    try
    {
        nlohmann::json j = nlohmann::json::parse( content.ToStdString() );

        m_previews.clear();
        m_lruOrder.clear();

        for( const auto& item : j["previews"] )
        {
            PLM_CACHED_PREVIEW preview;
            preview.file_id = wxString::FromUTF8( item.value( "file_id", "" ) );
            preview.local_path = wxString::FromUTF8( item.value( "local_path", "" ) );

            std::string sourceStr = item.value( "source", "unknown" );

            if( sourceStr == "server" )
                preview.source = PLM_PREVIEW_SOURCE::SERVER;
            else if( sourceStr == "local" )
                preview.source = PLM_PREVIEW_SOURCE::LOCAL;
            else
                preview.source = PLM_PREVIEW_SOURCE::UNKNOWN;

            preview.width = item.value( "width", 0 );
            preview.height = item.value( "height", 0 );
            preview.size_bytes = item.value( "size_bytes", static_cast<size_t>( 0 ) );
            preview.cached_at = item.value( "cached_at", static_cast<int64_t>( 0 ) );
            preview.last_accessed = item.value( "last_accessed", static_cast<int64_t>( 0 ) );
            preview.file_version = wxString::FromUTF8( item.value( "file_version", "" ) );
            preview.is_valid = item.value( "is_valid", false );

            // Verify file exists
            if( !preview.FileExists() )
                preview.is_valid = false;

            if( preview.is_valid )
            {
                m_previews[preview.file_id] = preview;
                m_lruOrder.push_back( preview.file_id );
            }
        }

        // Sort LRU order by last_accessed (most recent first)
        m_lruOrder.sort( [this]( const wxString& a, const wxString& b )
        {
            auto itA = m_previews.find( a );
            auto itB = m_previews.find( b );

            if( itA == m_previews.end() || itB == m_previews.end() )
                return false;

            return itA->second.last_accessed > itB->second.last_accessed;
        } );

        return true;
    }
    catch( const nlohmann::json::exception& e )
    {
        wxLogWarning( wxS( "PLM: Failed to load preview index: %s" ),
                      wxString::FromUTF8( e.what() ) );
        m_previews.clear();
        m_lruOrder.clear();
        return false;
    }
}


bool PLM_PREVIEW_CACHE::SaveIndex()
{
    wxString previewDir = GetPreviewCacheDir();

    if( previewDir.IsEmpty() )
        return false;

    wxString indexPath = previewDir + wxFileName::GetPathSeparator() + PREVIEW_INDEX_FILE;

    try
    {
        nlohmann::json j;
        j["version"] = 1;
        j["previews"] = nlohmann::json::array();

        for( const auto& [fileId, preview] : m_previews )
        {
            nlohmann::json item;
            item["file_id"] = preview.file_id.ToStdString();
            item["local_path"] = preview.local_path.ToStdString();

            switch( preview.source )
            {
            case PLM_PREVIEW_SOURCE::SERVER: item["source"] = "server"; break;
            case PLM_PREVIEW_SOURCE::LOCAL:  item["source"] = "local"; break;
            default:                         item["source"] = "unknown"; break;
            }

            item["width"] = preview.width;
            item["height"] = preview.height;
            item["size_bytes"] = preview.size_bytes;
            item["cached_at"] = preview.cached_at;
            item["last_accessed"] = preview.last_accessed;
            item["file_version"] = preview.file_version.ToStdString();
            item["is_valid"] = preview.is_valid;

            j["previews"].push_back( item );
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
        wxLogError( wxS( "PLM: Failed to save preview index: %s" ),
                    wxString::FromUTF8( e.what() ) );
        return false;
    }
}


bool PLM_PREVIEW_CACHE::GetPreview( const wxString& aFileId, wxBitmap& aBitmap )
{
    auto it = m_previews.find( aFileId );

    if( it == m_previews.end() || !it->second.is_valid )
    {
        ++m_stats.misses;
        return false;
    }

    PLM_CACHED_PREVIEW& preview = it->second;

    if( !preview.FileExists() )
    {
        preview.is_valid = false;
        ++m_stats.misses;
        return false;
    }

    wxImage image;

    if( !image.LoadFile( preview.local_path, wxBITMAP_TYPE_PNG ) )
    {
        preview.is_valid = false;
        ++m_stats.misses;
        return false;
    }

    aBitmap = wxBitmap( image );

    // Update LRU
    UpdateLRU( aFileId );
    preview.last_accessed = std::time( nullptr );

    ++m_stats.hits;
    return true;
}


wxString PLM_PREVIEW_CACHE::GetPreviewPath( const wxString& aFileId ) const
{
    auto it = m_previews.find( aFileId );

    if( it == m_previews.end() || !it->second.is_valid )
        return wxEmptyString;

    if( !it->second.FileExists() )
        return wxEmptyString;

    return it->second.local_path;
}


bool PLM_PREVIEW_CACHE::HasPreview( const wxString& aFileId ) const
{
    auto it = m_previews.find( aFileId );

    if( it == m_previews.end() )
        return false;

    return it->second.is_valid && it->second.FileExists();
}


bool PLM_PREVIEW_CACHE::GetPreviewInfo( const wxString& aFileId, PLM_CACHED_PREVIEW& aPreview ) const
{
    auto it = m_previews.find( aFileId );

    if( it == m_previews.end() )
        return false;

    aPreview = it->second;
    return true;
}


bool PLM_PREVIEW_CACHE::StoreServerPreview( const wxString& aFileId,
                                             const wxMemoryBuffer& aImageData,
                                             const wxString& aFileVersion )
{
    if( !m_initialized )
        return false;

    wxString filePath = GetPreviewFilePath( aFileId );

    if( filePath.IsEmpty() )
        return false;

    // Write image data to file
    wxFileOutputStream fileStream( filePath );

    if( !fileStream.IsOk() )
        return false;

    fileStream.Write( aImageData.GetData(), aImageData.GetDataLen() );

    if( !fileStream.IsOk() )
        return false;

    // Load image to get dimensions
    wxImage image;
    wxMemoryInputStream memStream( aImageData.GetData(), aImageData.GetDataLen() );

    int width = 0;
    int height = 0;

    if( image.LoadFile( memStream, wxBITMAP_TYPE_PNG ) )
    {
        width = image.GetWidth();
        height = image.GetHeight();
    }

    // Create cache entry
    PLM_CACHED_PREVIEW preview( aFileId, PLM_PREVIEW_SOURCE::SERVER );
    preview.local_path = filePath;
    preview.width = width;
    preview.height = height;
    preview.size_bytes = aImageData.GetDataLen();
    preview.cached_at = std::time( nullptr );
    preview.last_accessed = preview.cached_at;
    preview.file_version = aFileVersion;
    preview.is_valid = true;

    // Remove old entry if exists
    auto it = m_previews.find( aFileId );

    if( it != m_previews.end() )
        m_stats.current_size -= it->second.size_bytes;

    m_previews[aFileId] = preview;
    UpdateLRU( aFileId );

    m_stats.current_size += preview.size_bytes;
    m_stats.entry_count = m_previews.size();
    ++m_stats.server_fetches;

    // Enforce size limits
    EnforceSizeLimits();

    SaveIndex();

    return true;
}


bool PLM_PREVIEW_CACHE::StoreLocalPreview( const wxString& aFileId, const wxBitmap& aBitmap,
                                            const wxString& aFileVersion )
{
    if( !m_initialized || !aBitmap.IsOk() )
        return false;

    wxString filePath = GetPreviewFilePath( aFileId );

    if( filePath.IsEmpty() )
        return false;

    // Convert bitmap to image and save as PNG
    wxImage image = aBitmap.ConvertToImage();

    if( !image.SaveFile( filePath, wxBITMAP_TYPE_PNG ) )
        return false;

    // Get file size
    wxFileName fn( filePath );
    size_t fileSize = fn.GetSize().GetValue();

    // Create cache entry
    PLM_CACHED_PREVIEW preview( aFileId, PLM_PREVIEW_SOURCE::LOCAL );
    preview.local_path = filePath;
    preview.width = aBitmap.GetWidth();
    preview.height = aBitmap.GetHeight();
    preview.size_bytes = fileSize;
    preview.cached_at = std::time( nullptr );
    preview.last_accessed = preview.cached_at;
    preview.file_version = aFileVersion;
    preview.is_valid = true;

    // Remove old entry if exists
    auto it = m_previews.find( aFileId );

    if( it != m_previews.end() )
        m_stats.current_size -= it->second.size_bytes;

    m_previews[aFileId] = preview;
    UpdateLRU( aFileId );

    m_stats.current_size += preview.size_bytes;
    m_stats.entry_count = m_previews.size();
    ++m_stats.local_generates;

    // Enforce size limits
    EnforceSizeLimits();

    SaveIndex();

    return true;
}


bool PLM_PREVIEW_CACHE::NeedsUpdate( const wxString& aFileId,
                                      const wxString& aCurrentVersion ) const
{
    auto it = m_previews.find( aFileId );

    if( it == m_previews.end() || !it->second.is_valid )
        return true;

    if( aCurrentVersion.IsEmpty() || it->second.file_version.IsEmpty() )
        return false;

    return it->second.file_version != aCurrentVersion;
}


void PLM_PREVIEW_CACHE::UpdateLRU( const wxString& aFileId )
{
    // Remove from current position
    m_lruOrder.remove( aFileId );

    // Add to front (most recently used)
    m_lruOrder.push_front( aFileId );
}


bool PLM_PREVIEW_CACHE::EvictLRU()
{
    if( m_lruOrder.empty() )
        return false;

    // Get least recently used (back of list)
    wxString fileId = m_lruOrder.back();
    m_lruOrder.pop_back();

    auto it = m_previews.find( fileId );

    if( it == m_previews.end() )
        return false;

    // Remove file
    if( it->second.FileExists() )
        wxRemoveFile( it->second.local_path );

    m_stats.current_size -= it->second.size_bytes;
    m_previews.erase( it );
    ++m_stats.evictions;
    m_stats.entry_count = m_previews.size();

    return true;
}


size_t PLM_PREVIEW_CACHE::EnforceSizeLimits()
{
    size_t evicted = 0;

    // Enforce entry count limit
    while( m_previews.size() > m_config.max_entries && !m_lruOrder.empty() )
    {
        if( EvictLRU() )
            ++evicted;
        else
            break;
    }

    // Enforce size limit
    while( m_stats.current_size > m_config.MaxSizeBytes() && !m_lruOrder.empty() )
    {
        if( EvictLRU() )
            ++evicted;
        else
            break;
    }

    return evicted;
}


void PLM_PREVIEW_CACHE::TouchEntry( const wxString& aFileId )
{
    auto it = m_previews.find( aFileId );

    if( it != m_previews.end() )
    {
        it->second.last_accessed = std::time( nullptr );
        UpdateLRU( aFileId );
    }
}


bool PLM_PREVIEW_CACHE::InvalidatePreview( const wxString& aFileId )
{
    auto it = m_previews.find( aFileId );

    if( it == m_previews.end() )
        return false;

    // Remove file
    if( it->second.FileExists() )
        wxRemoveFile( it->second.local_path );

    m_stats.current_size -= it->second.size_bytes;
    m_lruOrder.remove( aFileId );
    m_previews.erase( it );
    m_stats.entry_count = m_previews.size();

    SaveIndex();

    return true;
}


size_t PLM_PREVIEW_CACHE::InvalidateAll()
{
    size_t count = m_previews.size();

    for( const auto& [fileId, preview] : m_previews )
    {
        if( preview.FileExists() )
            wxRemoveFile( preview.local_path );
    }

    m_previews.clear();
    m_lruOrder.clear();
    m_stats.current_size = 0;
    m_stats.entry_count = 0;

    SaveIndex();

    return count;
}


size_t PLM_PREVIEW_CACHE::GetTotalSize() const
{
    size_t total = 0;

    for( const auto& [fileId, preview] : m_previews )
        total += preview.size_bytes;

    return total;
}
