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

#include "plm_lib_version_tracker.h"

#include <nlohmann/json.hpp>

#include <wx/filename.h>
#include <wx/ffile.h>
#include <wx/log.h>

#include <chrono>


PLM_LIB_VERSION_TRACKER::PLM_LIB_VERSION_TRACKER()
{
}


PLM_LIB_VERSION_TRACKER::~PLM_LIB_VERSION_TRACKER()
{
}


void PLM_LIB_VERSION_TRACKER::SetServerVersionCallback( PLM_SERVER_VERSION_CALLBACK aCallback )
{
    m_serverCallback = aCallback;
}


void PLM_LIB_VERSION_TRACKER::TrackLibrary( const PLM_LIB_VERSION_INFO& aInfo )
{
    if( aInfo.lib_id.IsEmpty() )
        return;

    PLM_LIB_VERSION_INFO info = aInfo;

    if( info.checkout_timestamp == 0 )
    {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        info.checkout_timestamp =
                std::chrono::duration_cast<std::chrono::milliseconds>( duration ).count();
    }

    m_tracked[aInfo.lib_id] = info;
}


void PLM_LIB_VERSION_TRACKER::UntrackLibrary( const wxString& aLibId )
{
    m_tracked.erase( aLibId );
}


bool PLM_LIB_VERSION_TRACKER::IsTracked( const wxString& aLibId ) const
{
    return m_tracked.find( aLibId ) != m_tracked.end();
}


std::optional<PLM_LIB_VERSION_INFO> PLM_LIB_VERSION_TRACKER::GetVersionInfo(
        const wxString& aLibId ) const
{
    auto it = m_tracked.find( aLibId );

    if( it != m_tracked.end() )
        return it->second;

    return std::nullopt;
}


std::vector<PLM_LIB_VERSION_INFO> PLM_LIB_VERSION_TRACKER::GetAllTracked() const
{
    std::vector<PLM_LIB_VERSION_INFO> result;
    result.reserve( m_tracked.size() );

    for( const auto& [id, info] : m_tracked )
        result.push_back( info );

    return result;
}


size_t PLM_LIB_VERSION_TRACKER::GetTrackedCount() const
{
    return m_tracked.size();
}


void PLM_LIB_VERSION_TRACKER::SetPinned( const wxString& aLibId, bool aPinned )
{
    auto it = m_tracked.find( aLibId );

    if( it != m_tracked.end() )
        it->second.pinned = aPinned;
}


bool PLM_LIB_VERSION_TRACKER::IsPinned( const wxString& aLibId ) const
{
    auto it = m_tracked.find( aLibId );

    if( it != m_tracked.end() )
        return it->second.pinned;

    return false;
}


void PLM_LIB_VERSION_TRACKER::UpdateRevision( const wxString& aLibId,
                                               const wxString& aNewRevision,
                                               int64_t aNewTimestamp )
{
    auto it = m_tracked.find( aLibId );

    if( it != m_tracked.end() )
    {
        it->second.revision = aNewRevision;

        if( aNewTimestamp > 0 )
            it->second.server_timestamp = aNewTimestamp;

        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        it->second.checkout_timestamp =
                std::chrono::duration_cast<std::chrono::milliseconds>( duration ).count();
    }
}


PLM_LIB_VERSION_DIFF PLM_LIB_VERSION_TRACKER::CheckForUpdate( const wxString& aLibId )
{
    PLM_LIB_VERSION_DIFF diff;
    diff.lib_id = aLibId;
    diff.update_available = false;

    auto it = m_tracked.find( aLibId );

    if( it == m_tracked.end() )
        return diff;

    diff.local_revision = it->second.revision;
    diff.is_pinned = it->second.pinned;

    if( !m_serverCallback )
        return diff;

    wxString serverRev = m_serverCallback( aLibId );

    if( serverRev.IsEmpty() )
        return diff;

    diff.server_revision = serverRev;
    diff.update_available = ( serverRev != it->second.revision );

    return diff;
}


std::vector<PLM_LIB_VERSION_DIFF> PLM_LIB_VERSION_TRACKER::CheckAllForUpdates()
{
    std::vector<PLM_LIB_VERSION_DIFF> results;
    results.reserve( m_tracked.size() );

    for( const auto& [id, info] : m_tracked )
        results.push_back( CheckForUpdate( id ) );

    return results;
}


void PLM_LIB_VERSION_TRACKER::CheckAllForUpdatesAsync( PLM_VERSION_CHECK_CALLBACK aCallback )
{
    if( !aCallback )
        return;

    for( const auto& [id, info] : m_tracked )
    {
        PLM_LIB_VERSION_DIFF diff = CheckForUpdate( id );
        aCallback( id, diff );
    }
}


std::vector<wxString> PLM_LIB_VERSION_TRACKER::GetLibrariesWithUpdates( bool aIncludePinned )
{
    std::vector<wxString> result;

    auto diffs = CheckAllForUpdates();

    for( const auto& diff : diffs )
    {
        if( diff.update_available )
        {
            if( aIncludePinned || !diff.is_pinned )
                result.push_back( diff.lib_id );
        }
    }

    return result;
}


void PLM_LIB_VERSION_TRACKER::Clear()
{
    m_tracked.clear();
}


wxString PLM_LIB_VERSION_TRACKER::ToJson() const
{
    nlohmann::json j;
    j["version"] = 1;
    j["libraries"] = nlohmann::json::array();

    for( const auto& [id, info] : m_tracked )
    {
        nlohmann::json lib;
        lib["lib_id"] = info.lib_id.ToStdString();
        lib["lib_name"] = info.lib_name.ToStdString();
        lib["revision"] = info.revision.ToStdString();
        lib["revision_display"] = info.revision_display.ToStdString();
        lib["local_path"] = info.local_path.ToStdString();
        lib["checkout_timestamp"] = info.checkout_timestamp;
        lib["server_timestamp"] = info.server_timestamp;
        lib["pinned"] = info.pinned;

        j["libraries"].push_back( lib );
    }

    return wxString::FromUTF8( j.dump( 2 ) );
}


bool PLM_LIB_VERSION_TRACKER::FromJson( const wxString& aJson )
{
    try
    {
        nlohmann::json j = nlohmann::json::parse( aJson.ToStdString() );

        if( !j.contains( "version" ) || j["version"].get<int>() > 1 )
            return false;

        m_tracked.clear();

        if( j.contains( "libraries" ) && j["libraries"].is_array() )
        {
            for( const auto& lib : j["libraries"] )
            {
                PLM_LIB_VERSION_INFO info;

                if( lib.contains( "lib_id" ) )
                    info.lib_id = wxString::FromUTF8( lib["lib_id"].get<std::string>() );

                if( lib.contains( "lib_name" ) )
                    info.lib_name = wxString::FromUTF8( lib["lib_name"].get<std::string>() );

                if( lib.contains( "revision" ) )
                    info.revision = wxString::FromUTF8( lib["revision"].get<std::string>() );

                if( lib.contains( "revision_display" ) )
                    info.revision_display = wxString::FromUTF8(
                            lib["revision_display"].get<std::string>() );

                if( lib.contains( "local_path" ) )
                    info.local_path = wxString::FromUTF8( lib["local_path"].get<std::string>() );

                if( lib.contains( "checkout_timestamp" ) )
                    info.checkout_timestamp = lib["checkout_timestamp"].get<int64_t>();

                if( lib.contains( "server_timestamp" ) )
                    info.server_timestamp = lib["server_timestamp"].get<int64_t>();

                if( lib.contains( "pinned" ) )
                    info.pinned = lib["pinned"].get<bool>();

                if( !info.lib_id.IsEmpty() )
                    m_tracked[info.lib_id] = info;
            }
        }

        return true;
    }
    catch( const nlohmann::json::exception& e )
    {
        wxLogError( wxS( "Failed to parse library version JSON: %s" ), e.what() );
        return false;
    }
}


bool PLM_LIB_VERSION_TRACKER::SaveToFile( const wxString& aPath ) const
{
    wxString json = ToJson();

    wxFFile file( aPath, wxS( "w" ) );

    if( !file.IsOpened() )
    {
        wxLogError( wxS( "Failed to open file for writing: %s" ), aPath );
        return false;
    }

    if( !file.Write( json ) )
    {
        wxLogError( wxS( "Failed to write library versions to file: %s" ), aPath );
        return false;
    }

    return true;
}


bool PLM_LIB_VERSION_TRACKER::LoadFromFile( const wxString& aPath )
{
    if( !wxFileName::FileExists( aPath ) )
        return false;

    wxFFile file( aPath, wxS( "r" ) );

    if( !file.IsOpened() )
    {
        wxLogError( wxS( "Failed to open library version file: %s" ), aPath );
        return false;
    }

    wxString json;

    if( !file.ReadAll( &json ) )
    {
        wxLogError( wxS( "Failed to read library version file: %s" ), aPath );
        return false;
    }

    return FromJson( json );
}


wxString PLM_LIB_VERSION_TRACKER::GetDefaultPath( const wxString& aProjectPath )
{
    wxFileName fn( aProjectPath );
    fn.SetName( fn.GetName() + wxS( "-plm-libs" ) );
    fn.SetExt( wxS( "json" ) );
    return fn.GetFullPath();
}
