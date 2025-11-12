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

#include "plm_sync_state.h"

#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/log.h>


PLM_SYNC_STATE_MANAGER::PLM_SYNC_STATE_MANAGER()
{
}


wxString PLM_SYNC_STATE_MANAGER::NormalizePath( const wxString& aPath )
{
    wxFileName fn( aPath );
    fn.Normalize( wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE );
    return fn.GetFullPath();
}


void PLM_SYNC_STATE_MANAGER::InitProject( const wxString& aProjectPath,
                                           const wxString& aLocalRevision )
{
    wxString key = NormalizePath( aProjectPath );

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        PLM_SYNC_INFO& info = m_projects[key];
        info.project_path = key;
        info.local_revision = aLocalRevision;
        info.server_revision = aLocalRevision; // Assume in sync initially
        info.state = PLM_SYNC_STATE::IN_SYNC;
        info.last_sync = std::time( nullptr );
        info.has_local_changes = false;
        info.has_server_changes = false;
    }

    wxLogMessage( wxS( "PLM: Initialized sync state for %s at revision %s" ),
                  aProjectPath, aLocalRevision );
}


void PLM_SYNC_STATE_MANAGER::RemoveProject( const wxString& aProjectPath )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    wxString key = NormalizePath( aProjectPath );
    m_projects.erase( key );
}


void PLM_SYNC_STATE_MANAGER::RemoveAllProjects()
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_projects.clear();
}


bool PLM_SYNC_STATE_MANAGER::IsTracking( const wxString& aProjectPath ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );
    wxString key = NormalizePath( aProjectPath );
    return m_projects.find( key ) != m_projects.end();
}


PLM_SYNC_STATE PLM_SYNC_STATE_MANAGER::GetState( const wxString& aProjectPath ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizePath( aProjectPath );
    auto it = m_projects.find( key );

    if( it == m_projects.end() )
        return PLM_SYNC_STATE::UNKNOWN;

    return it->second.state;
}


bool PLM_SYNC_STATE_MANAGER::GetSyncInfo( const wxString& aProjectPath,
                                           PLM_SYNC_INFO& aInfo ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizePath( aProjectPath );
    auto it = m_projects.find( key );

    if( it == m_projects.end() )
        return false;

    aInfo = it->second;
    return true;
}


std::vector<wxString> PLM_SYNC_STATE_MANAGER::GetTrackedProjects() const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    std::vector<wxString> projects;
    projects.reserve( m_projects.size() );

    for( const auto& [key, info] : m_projects )
        projects.push_back( key );

    return projects;
}


std::vector<wxString> PLM_SYNC_STATE_MANAGER::GetProjectsByState( PLM_SYNC_STATE aState ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    std::vector<wxString> projects;

    for( const auto& [key, info] : m_projects )
    {
        if( info.state == aState )
            projects.push_back( key );
    }

    return projects;
}


std::vector<wxString> PLM_SYNC_STATE_MANAGER::GetProjectsNeedingSync() const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    std::vector<wxString> projects;

    for( const auto& [key, info] : m_projects )
    {
        if( info.NeedsSync() )
            projects.push_back( key );
    }

    return projects;
}


void PLM_SYNC_STATE_MANAGER::RecalculateState( PLM_SYNC_INFO& aInfo )
{
    PLM_SYNC_STATE oldState = aInfo.state;

    if( aInfo.has_local_changes && aInfo.has_server_changes )
        aInfo.state = PLM_SYNC_STATE::CONFLICT;
    else if( aInfo.has_local_changes )
        aInfo.state = PLM_SYNC_STATE::LOCAL_MODIFIED;
    else if( aInfo.has_server_changes )
        aInfo.state = PLM_SYNC_STATE::SERVER_MODIFIED;
    else
        aInfo.state = PLM_SYNC_STATE::IN_SYNC;

    // Don't downgrade from CONFLICT unless explicitly cleared
    if( oldState == PLM_SYNC_STATE::CONFLICT && aInfo.state != PLM_SYNC_STATE::CONFLICT )
    {
        if( aInfo.has_local_changes || aInfo.has_server_changes )
            aInfo.state = PLM_SYNC_STATE::CONFLICT;
    }
}


void PLM_SYNC_STATE_MANAGER::OnLocalModified( const wxString& aProjectPath )
{
    wxString key = NormalizePath( aProjectPath );
    PLM_SYNC_STATE newState;
    PLM_SYNC_STATE oldState;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_projects.find( key );

        if( it == m_projects.end() )
            return;

        oldState = it->second.state;
        it->second.has_local_changes = true;
        RecalculateState( it->second );
        newState = it->second.state;
    }

    if( oldState != newState )
    {
        wxLogMessage( wxS( "PLM: Sync state changed for %s: %s -> %s" ),
                      aProjectPath, GetStateDisplayName( oldState ),
                      GetStateDisplayName( newState ) );
        FireStateChange( aProjectPath, newState );
    }
}


void PLM_SYNC_STATE_MANAGER::OnCheckin( const wxString& aProjectPath,
                                         const wxString& aNewRevision )
{
    wxString key = NormalizePath( aProjectPath );
    PLM_SYNC_STATE newState;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_projects.find( key );

        if( it == m_projects.end() )
            return;

        it->second.local_revision = aNewRevision;
        it->second.server_revision = aNewRevision;
        it->second.has_local_changes = false;
        it->second.has_server_changes = false;
        it->second.state = PLM_SYNC_STATE::IN_SYNC;
        it->second.last_sync = std::time( nullptr );
        newState = it->second.state;
    }

    wxLogMessage( wxS( "PLM: Checkin complete for %s at revision %s" ),
                  aProjectPath, aNewRevision );
    FireStateChange( aProjectPath, newState );
}


void PLM_SYNC_STATE_MANAGER::OnCheckout( const wxString& aProjectPath,
                                          const wxString& aNewRevision )
{
    wxString key = NormalizePath( aProjectPath );
    PLM_SYNC_STATE newState;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_projects.find( key );

        if( it == m_projects.end() )
            return;

        it->second.local_revision = aNewRevision;
        it->second.server_revision = aNewRevision;
        it->second.has_local_changes = false;
        it->second.has_server_changes = false;
        it->second.state = PLM_SYNC_STATE::IN_SYNC;
        it->second.last_sync = std::time( nullptr );
        newState = it->second.state;
    }

    wxLogMessage( wxS( "PLM: Checkout complete for %s at revision %s" ),
                  aProjectPath, aNewRevision );
    FireStateChange( aProjectPath, newState );
}


bool PLM_SYNC_STATE_MANAGER::CheckServer( const wxString& aProjectPath )
{
    PLM_SERVER_CHECK_CALLBACK callback;
    wxString localRevision;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        if( !m_serverCheckCallback )
            return false;

        wxString key = NormalizePath( aProjectPath );
        auto it = m_projects.find( key );

        if( it == m_projects.end() )
            return false;

        callback = m_serverCheckCallback;
        localRevision = it->second.local_revision;
    }

    // Call callback outside of lock
    wxString serverRevision = callback( aProjectPath );

    if( serverRevision.IsEmpty() )
    {
        wxLogWarning( wxS( "PLM: Failed to check server revision for %s" ), aProjectPath );
        return false;
    }

    UpdateServerRevision( aProjectPath, serverRevision );
    return true;
}


void PLM_SYNC_STATE_MANAGER::CheckAllServers()
{
    std::vector<wxString> projects;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        if( !m_serverCheckCallback )
            return;

        for( const auto& [key, info] : m_projects )
            projects.push_back( key );
    }

    for( const wxString& path : projects )
        CheckServer( path );
}


void PLM_SYNC_STATE_MANAGER::UpdateServerRevision( const wxString& aProjectPath,
                                                    const wxString& aServerRevision )
{
    wxString key = NormalizePath( aProjectPath );
    PLM_SYNC_STATE newState;
    PLM_SYNC_STATE oldState;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_projects.find( key );

        if( it == m_projects.end() )
            return;

        oldState = it->second.state;
        it->second.server_revision = aServerRevision;
        it->second.last_checked = std::time( nullptr );

        // Determine if server has changes
        it->second.has_server_changes = ( aServerRevision != it->second.local_revision );
        RecalculateState( it->second );
        newState = it->second.state;
    }

    if( oldState != newState )
    {
        wxLogMessage( wxS( "PLM: Sync state changed for %s: %s -> %s" ),
                      aProjectPath, GetStateDisplayName( oldState ),
                      GetStateDisplayName( newState ) );
        FireStateChange( aProjectPath, newState );
    }
}


void PLM_SYNC_STATE_MANAGER::MarkConflict( const wxString& aProjectPath )
{
    wxString key = NormalizePath( aProjectPath );

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_projects.find( key );

        if( it == m_projects.end() )
            return;

        it->second.state = PLM_SYNC_STATE::CONFLICT;
        it->second.has_local_changes = true;
        it->second.has_server_changes = true;
    }

    wxLogWarning( wxS( "PLM: Conflict detected for %s" ), aProjectPath );
    FireStateChange( aProjectPath, PLM_SYNC_STATE::CONFLICT );
}


void PLM_SYNC_STATE_MANAGER::ClearConflict( const wxString& aProjectPath,
                                             const wxString& aNewRevision )
{
    wxString key = NormalizePath( aProjectPath );
    PLM_SYNC_STATE newState;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_projects.find( key );

        if( it == m_projects.end() )
            return;

        it->second.local_revision = aNewRevision;
        it->second.server_revision = aNewRevision;
        it->second.has_local_changes = false;
        it->second.has_server_changes = false;
        it->second.state = PLM_SYNC_STATE::IN_SYNC;
        it->second.last_sync = std::time( nullptr );
        newState = it->second.state;
    }

    wxLogMessage( wxS( "PLM: Conflict resolved for %s at revision %s" ),
                  aProjectPath, aNewRevision );
    FireStateChange( aProjectPath, newState );
}


void PLM_SYNC_STATE_MANAGER::SetStateCallback( PLM_SYNC_STATE_CALLBACK aCallback )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_stateCallback = std::move( aCallback );
}


void PLM_SYNC_STATE_MANAGER::SetServerCheckCallback( PLM_SERVER_CHECK_CALLBACK aCallback )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_serverCheckCallback = std::move( aCallback );
}


void PLM_SYNC_STATE_MANAGER::FireStateChange( const wxString& aProjectPath,
                                               PLM_SYNC_STATE aNewState )
{
    PLM_SYNC_STATE_CALLBACK callback;

    {
        std::lock_guard<std::mutex> lock( m_mutex );
        callback = m_stateCallback;
    }

    if( callback )
        callback( aProjectPath, aNewState );
}


wxString PLM_SYNC_STATE_MANAGER::GetStateDisplayName( PLM_SYNC_STATE aState )
{
    switch( aState )
    {
    case PLM_SYNC_STATE::IN_SYNC:         return _( "In Sync" );
    case PLM_SYNC_STATE::LOCAL_MODIFIED:  return _( "Local Changes" );
    case PLM_SYNC_STATE::SERVER_MODIFIED: return _( "Server Changes" );
    case PLM_SYNC_STATE::CONFLICT:        return _( "Conflict" );
    case PLM_SYNC_STATE::UNKNOWN:         return _( "Unknown" );
    }

    return _( "Unknown" );
}
