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

#include "plm_notification_ui.h"

#include <wx/intl.h>
#include <wx/log.h>
#include <wx/notifmsg.h>


PLM_NOTIFICATION_UI::PLM_NOTIFICATION_UI() :
        m_parent( nullptr ),
        m_subscriptionId( 0 )
{
}


PLM_NOTIFICATION_UI::~PLM_NOTIFICATION_UI()
{
    Shutdown();
}


void PLM_NOTIFICATION_UI::Initialize( wxFrame* aParent )
{
    m_parent = aParent;
    subscribeToEvents();
}


void PLM_NOTIFICATION_UI::Shutdown()
{
    unsubscribeFromEvents();
    m_parent = nullptr;
}


void PLM_NOTIFICATION_UI::ShowNotification( const PLM_UI_NOTIFICATION& aNotification )
{
    if( !m_prefs.enabled )
        return;

    if( !shouldShowNotification( aNotification.type ) )
        return;

    // Use wxNotificationMessage for cross-platform notifications
    int flags = wxICON_INFORMATION;

    switch( aNotification.level )
    {
    case PLM_NOTIFICATION_LEVEL::WARNING:
        flags = wxICON_WARNING;
        break;

    case PLM_NOTIFICATION_LEVEL::ERROR_MSG:
        flags = wxICON_ERROR;
        break;

    case PLM_NOTIFICATION_LEVEL::SUCCESS:
        flags = wxICON_INFORMATION;
        break;

    case PLM_NOTIFICATION_LEVEL::INFO:
    default:
        flags = wxICON_INFORMATION;
        break;
    }

    wxNotificationMessage notification( aNotification.title, aNotification.message, m_parent,
                                         flags );

    // Show with timeout if specified
    if( aNotification.timeout_ms > 0 )
        notification.Show( aNotification.timeout_ms / 1000 );
    else
        notification.Show();

    wxLogMessage( wxS( "PLM Notification: [%s] %s" ), aNotification.title, aNotification.message );
}


std::optional<PLM_UI_NOTIFICATION> PLM_NOTIFICATION_UI::CreateFromEvent( const PLM_EVENT& aEvent )
{
    PLM_UI_NOTIFICATION notification;
    notification.project_id = aEvent.project_id;
    notification.timestamp = aEvent.timestamp_ms;
    notification.timeout_ms = m_prefs.default_timeout_ms;

    switch( aEvent.type )
    {
    case PLM_EVENT_TYPE::SERVER_CONNECTED:
        notification.type = PLM_UI_NOTIFICATION_TYPE::CONNECTION_CHANGED;
        notification.level = PLM_NOTIFICATION_LEVEL::SUCCESS;
        notification.title = _( "PLM Connected" );
        notification.message = aEvent.message.IsEmpty() ? _( "Connected to PLM server" )
                                                        : aEvent.message;
        break;

    case PLM_EVENT_TYPE::SERVER_DISCONNECTED:
        notification.type = PLM_UI_NOTIFICATION_TYPE::CONNECTION_CHANGED;
        notification.level = PLM_NOTIFICATION_LEVEL::INFO;
        notification.title = _( "PLM Disconnected" );
        notification.message = _( "Disconnected from PLM server" );
        break;

    case PLM_EVENT_TYPE::LOCK_ACQUIRED:
        notification.type = PLM_UI_NOTIFICATION_TYPE::LOCK_ACQUIRED;
        notification.level = PLM_NOTIFICATION_LEVEL::SUCCESS;
        notification.title = _( "Lock Acquired" );
        notification.message = aEvent.message.IsEmpty() ? _( "Project lock acquired" )
                                                        : aEvent.message;
        break;

    case PLM_EVENT_TYPE::LOCK_RELEASED:
        notification.type = PLM_UI_NOTIFICATION_TYPE::LOCK_RELEASED;
        notification.level = PLM_NOTIFICATION_LEVEL::INFO;
        notification.title = _( "Lock Released" );
        notification.message = aEvent.message.IsEmpty() ? _( "Project lock released" )
                                                        : aEvent.message;
        break;

    case PLM_EVENT_TYPE::LOCK_STOLEN:
        notification.type = PLM_UI_NOTIFICATION_TYPE::LOCK_STOLEN;
        notification.level = PLM_NOTIFICATION_LEVEL::WARNING;
        notification.title = _( "Lock Stolen" );
        notification.message = aEvent.message.IsEmpty()
                                       ? _( "Your lock was taken by another user" )
                                       : aEvent.message;
        notification.timeout_ms = 0; // Don't auto-dismiss important notifications
        break;

    case PLM_EVENT_TYPE::LOCK_EXPIRED:
        notification.type = PLM_UI_NOTIFICATION_TYPE::LOCK_EXPIRED;
        notification.level = PLM_NOTIFICATION_LEVEL::WARNING;
        notification.title = _( "Lock Expired" );
        notification.message = aEvent.message.IsEmpty() ? _( "Your lock has expired" )
                                                        : aEvent.message;
        notification.timeout_ms = 0;
        break;

    case PLM_EVENT_TYPE::STATE_SYNC_COMPLETED:
        notification.type = PLM_UI_NOTIFICATION_TYPE::SYNC_COMPLETED;
        notification.level = PLM_NOTIFICATION_LEVEL::SUCCESS;
        notification.title = _( "Sync Complete" );
        notification.message = aEvent.message.IsEmpty() ? _( "Synchronization completed" )
                                                        : aEvent.message;
        break;

    default:
        return std::nullopt;
    }

    return notification;
}


void PLM_NOTIFICATION_UI::DismissAll()
{
    std::lock_guard<std::mutex> lock( m_mutex );

    while( !m_queue.empty() )
        m_queue.pop();
}


void PLM_NOTIFICATION_UI::DismissForProject( const wxString& aProjectId )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    std::queue<PLM_UI_NOTIFICATION> filtered;

    while( !m_queue.empty() )
    {
        PLM_UI_NOTIFICATION notif = m_queue.front();
        m_queue.pop();

        if( notif.project_id != aProjectId )
            filtered.push( notif );
    }

    m_queue = std::move( filtered );
}


void PLM_NOTIFICATION_UI::SetPreferences( const PLM_NOTIFICATION_PREFS& aPrefs )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_prefs = aPrefs;
}


void PLM_NOTIFICATION_UI::SetEnabled( bool aEnable )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_prefs.enabled = aEnable;
}


void PLM_NOTIFICATION_UI::subscribeToEvents()
{
    if( m_subscriptionId != 0 )
        return;

    PLM_EVENT_FILTER filter;

    m_subscriptionId = PLM_EVENT_BUS::GetInstance().Subscribe(
            [this]( const PLM_EVENT& event )
            {
                onPlmEvent( event );
            },
            filter );
}


void PLM_NOTIFICATION_UI::unsubscribeFromEvents()
{
    if( m_subscriptionId != 0 )
    {
        PLM_EVENT_BUS::GetInstance().Unsubscribe( m_subscriptionId );
        m_subscriptionId = 0;
    }
}


void PLM_NOTIFICATION_UI::onPlmEvent( const PLM_EVENT& aEvent )
{
    auto notification = CreateFromEvent( aEvent );

    if( notification )
        ShowNotification( *notification );
}


bool PLM_NOTIFICATION_UI::shouldShowNotification( PLM_UI_NOTIFICATION_TYPE aType ) const
{
    switch( aType )
    {
    case PLM_UI_NOTIFICATION_TYPE::CONNECTION_CHANGED:
        return m_prefs.show_connection;

    case PLM_UI_NOTIFICATION_TYPE::LOCK_ACQUIRED:
    case PLM_UI_NOTIFICATION_TYPE::LOCK_RELEASED:
    case PLM_UI_NOTIFICATION_TYPE::LOCK_STOLEN:
    case PLM_UI_NOTIFICATION_TYPE::LOCK_EXPIRED:
        return m_prefs.show_lock;

    case PLM_UI_NOTIFICATION_TYPE::SYNC_CONFLICT:
    case PLM_UI_NOTIFICATION_TYPE::SYNC_COMPLETED:
    case PLM_UI_NOTIFICATION_TYPE::STATE_CHANGED:
        return m_prefs.show_sync;

    case PLM_UI_NOTIFICATION_TYPE::CHECKIN_SUCCESS:
    case PLM_UI_NOTIFICATION_TYPE::CHECKIN_FAILED:
    case PLM_UI_NOTIFICATION_TYPE::CHECKOUT_SUCCESS:
    case PLM_UI_NOTIFICATION_TYPE::CHECKOUT_FAILED:
        return m_prefs.show_checkin;

    default:
        return true;
    }
}


wxString PLM_NOTIFICATION_UI::GetNotificationTitle( PLM_UI_NOTIFICATION_TYPE aType )
{
    switch( aType )
    {
    case PLM_UI_NOTIFICATION_TYPE::CONNECTION_CHANGED:
        return _( "Connection Status" );

    case PLM_UI_NOTIFICATION_TYPE::LOCK_ACQUIRED:
        return _( "Lock Acquired" );

    case PLM_UI_NOTIFICATION_TYPE::LOCK_RELEASED:
        return _( "Lock Released" );

    case PLM_UI_NOTIFICATION_TYPE::LOCK_STOLEN:
        return _( "Lock Stolen" );

    case PLM_UI_NOTIFICATION_TYPE::LOCK_EXPIRED:
        return _( "Lock Expired" );

    case PLM_UI_NOTIFICATION_TYPE::SYNC_CONFLICT:
        return _( "Sync Conflict" );

    case PLM_UI_NOTIFICATION_TYPE::SYNC_COMPLETED:
        return _( "Sync Complete" );

    case PLM_UI_NOTIFICATION_TYPE::STATE_CHANGED:
        return _( "State Changed" );

    case PLM_UI_NOTIFICATION_TYPE::CHECKIN_SUCCESS:
        return _( "Check-in Complete" );

    case PLM_UI_NOTIFICATION_TYPE::CHECKIN_FAILED:
        return _( "Check-in Failed" );

    case PLM_UI_NOTIFICATION_TYPE::CHECKOUT_SUCCESS:
        return _( "Check-out Complete" );

    case PLM_UI_NOTIFICATION_TYPE::CHECKOUT_FAILED:
        return _( "Check-out Failed" );

    default:
        return _( "PLM Notification" );
    }
}


PLM_NOTIFICATION_LEVEL PLM_NOTIFICATION_UI::GetNotificationLevel( PLM_UI_NOTIFICATION_TYPE aType )
{
    switch( aType )
    {
    case PLM_UI_NOTIFICATION_TYPE::LOCK_STOLEN:
    case PLM_UI_NOTIFICATION_TYPE::LOCK_EXPIRED:
    case PLM_UI_NOTIFICATION_TYPE::SYNC_CONFLICT:
        return PLM_NOTIFICATION_LEVEL::WARNING;

    case PLM_UI_NOTIFICATION_TYPE::CHECKIN_FAILED:
    case PLM_UI_NOTIFICATION_TYPE::CHECKOUT_FAILED:
        return PLM_NOTIFICATION_LEVEL::ERROR_MSG;

    case PLM_UI_NOTIFICATION_TYPE::LOCK_ACQUIRED:
    case PLM_UI_NOTIFICATION_TYPE::SYNC_COMPLETED:
    case PLM_UI_NOTIFICATION_TYPE::CHECKIN_SUCCESS:
    case PLM_UI_NOTIFICATION_TYPE::CHECKOUT_SUCCESS:
        return PLM_NOTIFICATION_LEVEL::SUCCESS;

    default:
        return PLM_NOTIFICATION_LEVEL::INFO;
    }
}
