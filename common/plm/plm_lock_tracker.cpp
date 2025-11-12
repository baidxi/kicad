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

#include "plm_lock_tracker.h"

#include <wx/log.h>


PLM_LOCK_TRACKER::PLM_LOCK_TRACKER()
{
}


void PLM_LOCK_TRACKER::StartTracking( const wxString& aProjectId, const PLM_LOCK_INFO& aInitialInfo )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    m_trackedLocks[aProjectId] = PLM_TRACKED_LOCK( aProjectId, aInitialInfo );
    m_trackedLocks[aProjectId].is_monitored = true;

    wxLogMessage( wxS( "PLM: Started tracking lock state for %s" ), aProjectId );
}


void PLM_LOCK_TRACKER::StopTracking( const wxString& aProjectId )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    auto it = m_trackedLocks.find( aProjectId );

    if( it != m_trackedLocks.end() )
    {
        m_trackedLocks.erase( it );
        wxLogMessage( wxS( "PLM: Stopped tracking lock state for %s" ), aProjectId );
    }
}


void PLM_LOCK_TRACKER::StopTrackingAll()
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_trackedLocks.clear();
    wxLogMessage( wxS( "PLM: Stopped tracking all lock states" ) );
}


bool PLM_LOCK_TRACKER::IsTracking( const wxString& aProjectId ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );
    return m_trackedLocks.find( aProjectId ) != m_trackedLocks.end();
}


std::vector<wxString> PLM_LOCK_TRACKER::GetTrackedProjects() const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    std::vector<wxString> projects;
    projects.reserve( m_trackedLocks.size() );

    for( const auto& [id, tracked] : m_trackedLocks )
        projects.push_back( id );

    return projects;
}


bool PLM_LOCK_TRACKER::GetLockState( const wxString& aProjectId, PLM_LOCK_INFO& aInfo ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    auto it = m_trackedLocks.find( aProjectId );

    if( it == m_trackedLocks.end() )
        return false;

    aInfo = it->second.lock_info;
    return true;
}


bool PLM_LOCK_TRACKER::IsLockedByMe( const wxString& aProjectId ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    auto it = m_trackedLocks.find( aProjectId );

    if( it == m_trackedLocks.end() )
        return false;

    return it->second.lock_info.is_locked && it->second.lock_info.locked_by_me;
}


bool PLM_LOCK_TRACKER::IsLockedByOther( const wxString& aProjectId ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    auto it = m_trackedLocks.find( aProjectId );

    if( it == m_trackedLocks.end() )
        return false;

    return it->second.lock_info.is_locked && !it->second.lock_info.locked_by_me;
}


bool PLM_LOCK_TRACKER::CanModify( const wxString& aProjectId ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    auto it = m_trackedLocks.find( aProjectId );

    if( it == m_trackedLocks.end() )
        return true; // Not tracked, assume can modify

    return it->second.lock_info.CanModify();
}


void PLM_LOCK_TRACKER::UpdateLockState( const wxString& aProjectId, const PLM_LOCK_INFO& aNewInfo )
{
    PLM_LOCK_INFO oldInfo;
    bool stateChanged = false;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_trackedLocks.find( aProjectId );

        if( it == m_trackedLocks.end() )
        {
            // Not tracking this project, start now
            m_trackedLocks[aProjectId] = PLM_TRACKED_LOCK( aProjectId, aNewInfo );
            return;
        }

        oldInfo = it->second.lock_info;

        // Check if state actually changed
        stateChanged = ( oldInfo.is_locked != aNewInfo.is_locked ) ||
                       ( oldInfo.locked_by_me != aNewInfo.locked_by_me ) ||
                       ( oldInfo.lock_owner != aNewInfo.lock_owner );

        it->second.lock_info = aNewInfo;
        it->second.last_checked = std::time( nullptr );
    }

    if( stateChanged )
    {
        PLM_LOCK_EVENT event = DetermineEventType( oldInfo, aNewInfo );
        FireEvent( aProjectId, event, aNewInfo );
    }
}


void PLM_LOCK_TRACKER::OnLockAcquired( const wxString& aProjectId, const PLM_LOCK_INFO& aLockInfo )
{
    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_trackedLocks.find( aProjectId );

        if( it == m_trackedLocks.end() )
            m_trackedLocks[aProjectId] = PLM_TRACKED_LOCK( aProjectId, aLockInfo );
        else
            it->second.lock_info = aLockInfo;
    }

    wxLogMessage( wxS( "PLM: Lock acquired for %s" ), aProjectId );
    FireEvent( aProjectId, PLM_LOCK_EVENT::LOCK_ACQUIRED, aLockInfo );
}


void PLM_LOCK_TRACKER::OnLockReleased( const wxString& aProjectId )
{
    PLM_LOCK_INFO unlockedInfo = PLM_LOCK_INFO::Unlocked();

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_trackedLocks.find( aProjectId );

        if( it != m_trackedLocks.end() )
            it->second.lock_info = unlockedInfo;
    }

    wxLogMessage( wxS( "PLM: Lock released for %s" ), aProjectId );
    FireEvent( aProjectId, PLM_LOCK_EVENT::LOCK_RELEASED, unlockedInfo );
}


void PLM_LOCK_TRACKER::OnLockStolen( const wxString& aProjectId, const PLM_LOCK_INFO& aLockInfo )
{
    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_trackedLocks.find( aProjectId );

        if( it != m_trackedLocks.end() )
            it->second.lock_info = aLockInfo;
    }

    wxLogWarning( wxS( "PLM: Lock stolen for %s by %s" ), aProjectId, aLockInfo.lock_owner );
    FireEvent( aProjectId, PLM_LOCK_EVENT::LOCK_STOLEN, aLockInfo );
}


void PLM_LOCK_TRACKER::SetPollCallback( PLM_LOCK_POLL_CALLBACK aCallback )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_pollCallback = std::move( aCallback );
}


bool PLM_LOCK_TRACKER::PollLockState( const wxString& aProjectId )
{
    PLM_LOCK_POLL_CALLBACK callback;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        if( !m_pollCallback )
            return false;

        if( m_trackedLocks.find( aProjectId ) == m_trackedLocks.end() )
            return false;

        callback = m_pollCallback;
    }

    // Poll outside of lock to avoid deadlock
    PLM_LOCK_INFO newInfo = callback( aProjectId );
    UpdateLockState( aProjectId, newInfo );
    return true;
}


void PLM_LOCK_TRACKER::PollAllLockStates()
{
    std::vector<wxString> projects;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        if( !m_pollCallback )
            return;

        for( const auto& [id, tracked] : m_trackedLocks )
        {
            if( tracked.is_monitored )
                projects.push_back( id );
        }
    }

    for( const wxString& projectId : projects )
        PollLockState( projectId );
}


void PLM_LOCK_TRACKER::SetEventCallback( PLM_LOCK_EVENT_CALLBACK aCallback )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_eventCallback = std::move( aCallback );
}


void PLM_LOCK_TRACKER::FireEvent( const wxString& aProjectId, PLM_LOCK_EVENT aEvent,
                                   const PLM_LOCK_INFO& aLockInfo )
{
    PLM_LOCK_EVENT_CALLBACK callback;

    {
        std::lock_guard<std::mutex> lock( m_mutex );
        callback = m_eventCallback;
    }

    if( callback )
        callback( aProjectId, aEvent, aLockInfo );
}


PLM_LOCK_EVENT PLM_LOCK_TRACKER::DetermineEventType( const PLM_LOCK_INFO& aOldInfo,
                                                      const PLM_LOCK_INFO& aNewInfo ) const
{
    // Was unlocked, now locked by me
    if( !aOldInfo.is_locked && aNewInfo.is_locked && aNewInfo.locked_by_me )
        return PLM_LOCK_EVENT::LOCK_ACQUIRED;

    // Was locked, now unlocked
    if( aOldInfo.is_locked && !aNewInfo.is_locked )
        return PLM_LOCK_EVENT::LOCK_RELEASED;

    // Was locked by me, now locked by someone else
    if( aOldInfo.is_locked && aOldInfo.locked_by_me && aNewInfo.is_locked && !aNewInfo.locked_by_me )
        return PLM_LOCK_EVENT::LOCK_STOLEN;

    // Was unlocked or locked by other, now locked by another (different) user
    if( aNewInfo.is_locked && !aNewInfo.locked_by_me )
        return PLM_LOCK_EVENT::LOCK_BLOCKED;

    // Default to released if unclear
    return PLM_LOCK_EVENT::LOCK_RELEASED;
}
