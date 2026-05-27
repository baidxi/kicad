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

#include "plm_lib_provider.h"
#include "plm_lib_version_tracker.h"

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/stdpaths.h>

#include <chrono>


PLM_LIB_PROVIDER::PLM_LIB_PROVIDER() : m_cache( nullptr ), m_versionTracker( nullptr )
{
    wxFileName cacheDir( wxStandardPaths::Get().GetUserDataDir(), wxEmptyString );
    cacheDir.AppendDir( wxS( "plm-libs" ) );
    m_cacheDir = cacheDir.GetPath();
}


PLM_LIB_PROVIDER::~PLM_LIB_PROVIDER()
{
}


void PLM_LIB_PROVIDER::SetCacheManager( PLM_PART_CACHE* aCache )
{
    m_cache = aCache;
}


void PLM_LIB_PROVIDER::SetDownloadCallback( PLM_LIB_DOWNLOAD_CALLBACK aCallback )
{
    m_downloadCallback = aCallback;
}


void PLM_LIB_PROVIDER::SetValidateCallback( PLM_LIB_VALIDATE_CALLBACK aCallback )
{
    m_validateCallback = aCallback;
}


void PLM_LIB_PROVIDER::SetVersionTracker( PLM_LIB_VERSION_TRACKER* aTracker )
{
    m_versionTracker = aTracker;
}


PLM_LIB_QUERY_RESULT PLM_LIB_PROVIDER::GetSymbolLibrary( const wxString& aLibId,
                                                          bool aForceRefresh )
{
    return getLibrary( aLibId, PLM_LIB_TYPE::SYMBOL, aForceRefresh );
}


PLM_LIB_QUERY_RESULT PLM_LIB_PROVIDER::GetFootprintLibrary( const wxString& aLibId,
                                                             bool aForceRefresh )
{
    return getLibrary( aLibId, PLM_LIB_TYPE::FOOTPRINT, aForceRefresh );
}


PLM_LIB_QUERY_RESULT PLM_LIB_PROVIDER::Get3DModel( const wxString& aModelId, bool aForceRefresh )
{
    return getLibrary( aModelId, PLM_LIB_TYPE::MODEL_3D, aForceRefresh );
}


PLM_LIB_QUERY_RESULT PLM_LIB_PROVIDER::getLibrary( const wxString& aLibId, PLM_LIB_TYPE aLibType,
                                                    bool aForceRefresh )
{
    if( !m_downloadCallback )
        return PLM_LIB_QUERY_RESULT::Failure( wxS( "No download callback configured" ) );

    wxString cacheKey = aLibId + wxS( ":" ) + wxString::Format( wxS( "%d" ), (int) aLibType );

    // Pin (if any) invalidates stale slots below; the cache key omits
    // revision so a repin reuses the slot rather than accumulating one
    // entry per requested revision.
    wxString pinnedRevision;

    if( m_versionTracker )
    {
        std::optional<PLM_LIB_VERSION_INFO> info = m_versionTracker->GetVersionInfo( aLibId );

        if( info && info->pinned )
            pinnedRevision = info->revision;
    }

    // Check cache first unless forcing refresh
    if( !aForceRefresh )
    {
        auto it = m_cacheIndex.find( cacheKey );

        if( it != m_cacheIndex.end() )
        {
            const CacheEntry& entry = it->second;

            // Empty pin accepts any cached revision; otherwise the slot is
            // stale after a repin and we fall through to download.
            bool revisionMatchesPin = pinnedRevision.IsEmpty()
                                      || pinnedRevision == entry.revision;

            if( wxFileName::FileExists( entry.local_path ) && revisionMatchesPin )
            {
                // Validate if we have a validation callback and haven't recently validated
                bool needsValidation = m_validateCallback && !entry.validated;

                if( needsValidation )
                {
                    bool isValid = m_validateCallback( aLibId, entry.revision );

                    if( isValid )
                    {
                        // Update validated flag
                        m_cacheIndex[cacheKey].validated = true;
                        return PLM_LIB_QUERY_RESULT::Success( entry.local_path, entry.revision,
                                                              true );
                    }

                    // Cache is stale, need to download
                }
                else
                {
                    return PLM_LIB_QUERY_RESULT::Success( entry.local_path, entry.revision, true );
                }
            }
        }
    }

    // Ensure cache directory exists
    wxString cacheSubdir = m_cacheDir + wxFileName::GetPathSeparator() + GetCacheSubdir( aLibType );
    wxFileName::Mkdir( cacheSubdir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );

    // Generate local path
    wxString localPath = getCachePath( aLibId, aLibType );

    PLM_LIB_DOWNLOAD_RESULT dlResult =
            m_downloadCallback( aLibId, aLibType, pinnedRevision, localPath );

    if( !dlResult.success )
    {
        wxString msg = dlResult.error_message.IsEmpty()
                               ? wxS( "Failed to download library from PLM" )
                               : dlResult.error_message;
        return PLM_LIB_QUERY_RESULT::Failure( msg );
    }

    // Verify download succeeded
    if( !wxFileName::FileExists( localPath ) )
        return PLM_LIB_QUERY_RESULT::Failure( wxS( "Download completed but file not found" ) );

    // Update cache index
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    int64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>( duration ).count();

    CacheEntry entry;
    entry.local_path = localPath;
    entry.revision = dlResult.revision;
    entry.timestamp = timestamp;
    entry.validated = true;

    m_cacheIndex[cacheKey] = entry;

    return PLM_LIB_QUERY_RESULT::Success( localPath, entry.revision, false );
}


bool PLM_LIB_PROVIDER::IsCached( const wxString& aLibId, PLM_LIB_TYPE aLibType ) const
{
    wxString cacheKey = aLibId + wxS( ":" ) + wxString::Format( wxS( "%d" ), (int) aLibType );
    auto     it = m_cacheIndex.find( cacheKey );

    if( it == m_cacheIndex.end() )
        return false;

    return wxFileName::FileExists( it->second.local_path );
}


bool PLM_LIB_PROVIDER::ValidateCache( const wxString& aLibId, PLM_LIB_TYPE aLibType )
{
    if( !m_validateCallback )
        return true;

    wxString cacheKey = aLibId + wxS( ":" ) + wxString::Format( wxS( "%d" ), (int) aLibType );
    auto     it = m_cacheIndex.find( cacheKey );

    if( it == m_cacheIndex.end() )
        return false;

    bool isValid = m_validateCallback( aLibId, it->second.revision );

    if( isValid )
        m_cacheIndex[cacheKey].validated = true;

    return isValid;
}


void PLM_LIB_PROVIDER::InvalidateCache( const wxString& aLibId, PLM_LIB_TYPE aLibType )
{
    wxString cacheKey = aLibId + wxS( ":" ) + wxString::Format( wxS( "%d" ), (int) aLibType );
    auto     it = m_cacheIndex.find( cacheKey );

    if( it != m_cacheIndex.end() )
        it->second.validated = false;
}


wxString PLM_LIB_PROVIDER::GetCachedPath( const wxString& aLibId, PLM_LIB_TYPE aLibType ) const
{
    wxString cacheKey = aLibId + wxS( ":" ) + wxString::Format( wxS( "%d" ), (int) aLibType );
    auto     it = m_cacheIndex.find( cacheKey );

    if( it != m_cacheIndex.end() )
        return it->second.local_path;

    return wxEmptyString;
}


wxString PLM_LIB_PROVIDER::GetCachedRevision( const wxString& aLibId, PLM_LIB_TYPE aLibType ) const
{
    wxString cacheKey = aLibId + wxS( ":" ) + wxString::Format( wxS( "%d" ), (int) aLibType );
    auto     it = m_cacheIndex.find( cacheKey );

    if( it != m_cacheIndex.end() )
        return it->second.revision;

    return wxEmptyString;
}


size_t PLM_LIB_PROVIDER::PrefetchLibraries( const std::vector<PLM_LIB_REFERENCE>& aReferences )
{
    size_t successCount = 0;

    for( const auto& ref : aReferences )
    {
        auto result = getLibrary( ref.lib_id, ref.lib_type, false );

        if( result.success )
            successCount++;
    }

    return successCount;
}


void PLM_LIB_PROVIDER::ClearCache()
{
    // Remove cached files
    for( const auto& [key, entry] : m_cacheIndex )
    {
        if( wxFileName::FileExists( entry.local_path ) )
            wxRemoveFile( entry.local_path );
    }

    m_cacheIndex.clear();
}


size_t PLM_LIB_PROVIDER::GetCacheCount() const
{
    return m_cacheIndex.size();
}


wxString PLM_LIB_PROVIDER::GetLibraryExtension( PLM_LIB_TYPE aLibType )
{
    switch( aLibType )
    {
    case PLM_LIB_TYPE::SYMBOL:    return wxS( ".kicad_sym" );
    case PLM_LIB_TYPE::FOOTPRINT: return wxS( ".pretty" );
    case PLM_LIB_TYPE::MODEL_3D:  return wxS( ".step" );
    default:                      return wxEmptyString;
    }
}


wxString PLM_LIB_PROVIDER::GetCacheSubdir( PLM_LIB_TYPE aLibType )
{
    switch( aLibType )
    {
    case PLM_LIB_TYPE::SYMBOL:    return wxS( "symbols" );
    case PLM_LIB_TYPE::FOOTPRINT: return wxS( "footprints" );
    case PLM_LIB_TYPE::MODEL_3D:  return wxS( "3dmodels" );
    default:                      return wxS( "other" );
    }
}


wxString PLM_LIB_PROVIDER::getCachePath( const wxString& aLibId, PLM_LIB_TYPE aLibType ) const
{
    return m_cacheDir + wxFileName::GetPathSeparator() + GetCacheSubdir( aLibType )
           + wxFileName::GetPathSeparator() + generateCacheFilename( aLibId, aLibType );
}


wxString PLM_LIB_PROVIDER::generateCacheFilename( const wxString& aLibId,
                                                   PLM_LIB_TYPE aLibType ) const
{
    // Create safe filename from lib ID by replacing path separators
    wxString safeName = aLibId;
    safeName.Replace( wxS( "/" ), wxS( "_" ) );
    safeName.Replace( wxS( "\\" ), wxS( "_" ) );
    safeName.Replace( wxS( ":" ), wxS( "_" ) );

    return safeName + GetLibraryExtension( aLibType );
}
