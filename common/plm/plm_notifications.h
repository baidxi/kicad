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

/**
 * @file plm_notifications.h
 * @brief Push notification structures for the PLM client interface.
 *
 * This header defines structures and listener classes for receiving
 * real-time notifications from the PLM server about file changes,
 * lock events, and state transitions.
 */

#ifndef PLM_NOTIFICATIONS_H
#define PLM_NOTIFICATIONS_H

#include "plm_types.h"

#include <wx/string.h>
#include <cstdint>


/**
 * Type of notification event from the PLM server.
 */
enum class PLM_NOTIFICATION_TYPE
{
    LOCK_ACQUIRED = 0,      ///< File was locked by a user
    LOCK_RELEASED = 1,      ///< File lock was released
    STATE_CHANGED = 2,      ///< File state changed (e.g., released)
    REVISION_CREATED = 3,   ///< New revision was created
    FILE_MODIFIED = 4,      ///< File content was modified
    FILE_DELETED = 5,       ///< File was deleted
    ATTRIBUTE_CHANGED = 6,  ///< File attributes were modified
    COMMENT_ADDED = 7,      ///< Comment was added to file
    ECO_STATUS_CHANGED = 8  ///< ECO status changed
};


/**
 * A notification event from the PLM server.
 *
 * Contains all information about a change event that occurred
 * on the server, allowing clients to update their local state.
 */
struct PLM_NOTIFICATION
{
    PLM_NOTIFICATION_TYPE type;          ///< Type of notification
    wxString              project_id;    ///< Affected project ID
    wxString              user_id;       ///< User who triggered the change
    wxString              user_name;     ///< Display name of user
    wxString              message;       ///< Human-readable description
    wxString              revision_id;   ///< New revision ID (for revision events)
    PLM_STATE_CATEGORY    new_state;     ///< New state (for state change events)
    int64_t               timestamp = 0; ///< When the event occurred (Unix epoch)

    PLM_NOTIFICATION() : type( PLM_NOTIFICATION_TYPE::FILE_MODIFIED ),
                         new_state( PLM_STATE_CATEGORY::EDITABLE )
    {
    }

    PLM_NOTIFICATION( PLM_NOTIFICATION_TYPE aType, const wxString& aProjectId ) :
            type( aType ),
            project_id( aProjectId ),
            new_state( PLM_STATE_CATEGORY::EDITABLE )
    {
    }

    /**
     * Create a lock acquired notification.
     */
    static PLM_NOTIFICATION LockAcquired( const wxString& aProjectId,
                                           const wxString& aUserId,
                                           const wxString& aUserName )
    {
        PLM_NOTIFICATION n( PLM_NOTIFICATION_TYPE::LOCK_ACQUIRED, aProjectId );
        n.user_id = aUserId;
        n.user_name = aUserName;
        n.message = wxString::Format( wxS( "%s locked the file" ), aUserName );
        return n;
    }

    /**
     * Create a lock released notification.
     */
    static PLM_NOTIFICATION LockReleased( const wxString& aProjectId,
                                           const wxString& aUserId,
                                           const wxString& aUserName )
    {
        PLM_NOTIFICATION n( PLM_NOTIFICATION_TYPE::LOCK_RELEASED, aProjectId );
        n.user_id = aUserId;
        n.user_name = aUserName;
        n.message = wxString::Format( wxS( "%s released the lock" ), aUserName );
        return n;
    }

    /**
     * Create a state changed notification.
     */
    static PLM_NOTIFICATION StateChanged( const wxString&    aProjectId,
                                           PLM_STATE_CATEGORY aNewState,
                                           const wxString&    aUserId,
                                           const wxString&    aUserName )
    {
        PLM_NOTIFICATION n( PLM_NOTIFICATION_TYPE::STATE_CHANGED, aProjectId );
        n.new_state = aNewState;
        n.user_id = aUserId;
        n.user_name = aUserName;
        return n;
    }

    /**
     * Create a revision created notification.
     */
    static PLM_NOTIFICATION RevisionCreated( const wxString& aProjectId,
                                              const wxString& aRevisionId,
                                              const wxString& aUserId,
                                              const wxString& aUserName )
    {
        PLM_NOTIFICATION n( PLM_NOTIFICATION_TYPE::REVISION_CREATED, aProjectId );
        n.revision_id = aRevisionId;
        n.user_id = aUserId;
        n.user_name = aUserName;
        n.message = wxString::Format( wxS( "%s created revision %s" ), aUserName, aRevisionId );
        return n;
    }

    /**
     * Check if this notification affects a specific project.
     * @param aProjectId Project ID to check.
     * @return True if notification is for this project.
     */
    bool IsForProject( const wxString& aProjectId ) const
    {
        return project_id == aProjectId;
    }

    /**
     * Check if this is a lock-related notification.
     * @return True if lock acquired or released.
     */
    bool IsLockEvent() const
    {
        return type == PLM_NOTIFICATION_TYPE::LOCK_ACQUIRED ||
               type == PLM_NOTIFICATION_TYPE::LOCK_RELEASED;
    }

    /**
     * Get human-readable notification type name.
     * @return Type name string.
     */
    wxString GetTypeName() const
    {
        switch( type )
        {
        case PLM_NOTIFICATION_TYPE::LOCK_ACQUIRED:     return wxS( "Lock Acquired" );
        case PLM_NOTIFICATION_TYPE::LOCK_RELEASED:     return wxS( "Lock Released" );
        case PLM_NOTIFICATION_TYPE::STATE_CHANGED:     return wxS( "State Changed" );
        case PLM_NOTIFICATION_TYPE::REVISION_CREATED:  return wxS( "Revision Created" );
        case PLM_NOTIFICATION_TYPE::FILE_MODIFIED:     return wxS( "File Modified" );
        case PLM_NOTIFICATION_TYPE::FILE_DELETED:      return wxS( "File Deleted" );
        case PLM_NOTIFICATION_TYPE::ATTRIBUTE_CHANGED: return wxS( "Attribute Changed" );
        case PLM_NOTIFICATION_TYPE::COMMENT_ADDED:     return wxS( "Comment Added" );
        case PLM_NOTIFICATION_TYPE::ECO_STATUS_CHANGED: return wxS( "ECO Status Changed" );
        default:                                       return wxS( "Unknown" );
        }
    }
};


/**
 * Abstract listener for PLM notifications.
 *
 * Implement this interface to receive real-time notifications about
 * file changes from the PLM server. Register with the plugin using
 * SubscribeNotifications().
 */
class PLM_NOTIFICATION_LISTENER
{
public:
    /**
     * Virtual destructor for proper cleanup.
     */
    virtual ~PLM_NOTIFICATION_LISTENER() = default;

    /**
     * Called when a notification is received.
     *
     * This method is called on a background thread. Implementations
     * should post events to the main thread if UI updates are needed.
     *
     * @param aNotification The notification event.
     */
    virtual void OnNotification( const PLM_NOTIFICATION& aNotification ) = 0;

    /**
     * Called when the notification connection is established.
     *
     * Indicates that the subscription is active and notifications
     * will be received.
     */
    virtual void OnConnected() {}

    /**
     * Called when the notification connection is lost.
     *
     * May be called due to network issues or server shutdown.
     * The plugin will attempt to reconnect automatically.
     *
     * @param aReason Reason for disconnection.
     */
    virtual void OnDisconnected( const wxString& aReason ) {}

    /**
     * Called when an error occurs with notifications.
     *
     * @param aError Error message.
     */
    virtual void OnError( const wxString& aError ) {}

protected:
    PLM_NOTIFICATION_LISTENER() = default;
};


/**
 * Configuration for notification subscriptions.
 */
struct PLM_NOTIFICATION_CONFIG
{
    bool subscribe_locks = true;      ///< Receive lock events
    bool subscribe_states = true;     ///< Receive state change events
    bool subscribe_revisions = true;  ///< Receive revision events
    bool subscribe_modifications = false; ///< Receive file modification events
    bool subscribe_comments = false;  ///< Receive comment events
    bool subscribe_eco = false;       ///< Receive ECO events

    int reconnect_delay_ms = 5000;    ///< Delay before reconnection attempt
    int max_reconnect_attempts = 10;  ///< Maximum reconnection attempts

    PLM_NOTIFICATION_CONFIG() = default;

    /**
     * Create config subscribing to all events.
     * @return Config with all subscriptions enabled.
     */
    static PLM_NOTIFICATION_CONFIG All()
    {
        PLM_NOTIFICATION_CONFIG cfg;
        cfg.subscribe_modifications = true;
        cfg.subscribe_comments = true;
        cfg.subscribe_eco = true;
        return cfg;
    }

    /**
     * Create config for minimal subscriptions.
     * @return Config with only essential events.
     */
    static PLM_NOTIFICATION_CONFIG Minimal()
    {
        PLM_NOTIFICATION_CONFIG cfg;
        cfg.subscribe_locks = true;
        cfg.subscribe_states = true;
        cfg.subscribe_revisions = false;
        return cfg;
    }
};


#endif // PLM_NOTIFICATIONS_H
