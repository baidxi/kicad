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
 * @file plm_event_bus.h
 * @brief Event bus for PLM system events.
 *
 * This header defines the PLM event bus which provides a centralized
 * publish/subscribe mechanism for PLM-related events throughout the
 * application. Components can subscribe to specific event types and
 * receive notifications when those events occur.
 */

#ifndef PLM_EVENT_BUS_H
#define PLM_EVENT_BUS_H

#include <kicommon.h>
#include "plm_audit.h"
#include "plm_notifications.h"

#include <wx/string.h>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <vector>


/**
 * Types of events that can be published on the PLM event bus.
 */
enum class PLM_EVENT_TYPE
{
    // Authentication events
    AUTH_LOGIN = 0,             ///< User logged in
    AUTH_LOGOUT = 1,            ///< User logged out
    AUTH_SESSION_EXPIRED = 2,   ///< Session expired
    AUTH_TOKEN_REFRESHED = 3,   ///< Token was refreshed

    // Lock events
    LOCK_ACQUIRED = 10,         ///< Lock acquired on project
    LOCK_RELEASED = 11,         ///< Lock released
    LOCK_STOLEN = 12,           ///< Lock was forcefully taken
    LOCK_EXPIRED = 13,          ///< Lock expired

    // State events
    STATE_CHANGED = 20,         ///< Project state changed
    STATE_SYNC_STARTED = 21,    ///< Sync operation started
    STATE_SYNC_COMPLETED = 22,  ///< Sync operation completed
    STATE_SYNC_FAILED = 23,     ///< Sync operation failed

    // Activity events
    ACTIVITY_UPDATED = 30,      ///< Activity cache updated
    ACTIVITY_NEW = 31,          ///< New activity recorded

    // Project events
    PROJECT_BOUND = 40,         ///< Project bound to PLM
    PROJECT_UNBOUND = 41,       ///< Project unbound from PLM
    PROJECT_DIRTY = 42,         ///< Project has local modifications
    PROJECT_CLEAN = 43,         ///< Project matches server state

    // Plugin events
    PLUGIN_LOADED = 50,         ///< PLM plugin loaded
    PLUGIN_UNLOADED = 51,       ///< PLM plugin unloaded
    PLUGIN_ERROR = 52,          ///< Plugin encountered error

    // Server events
    SERVER_CONNECTED = 60,      ///< Connected to PLM server
    SERVER_DISCONNECTED = 61,   ///< Disconnected from server
    SERVER_NOTIFICATION = 62    ///< Received server notification
};


/**
 * A PLM event containing information about what occurred.
 */
struct PLM_EVENT
{
    PLM_EVENT_TYPE type;            ///< Event type
    wxString       project_id;      ///< Affected project (if any)
    wxString       user_id;         ///< User who triggered event (if any)
    wxString       message;         ///< Human-readable message
    wxString       details;         ///< Additional JSON details
    int64_t        timestamp_ms;    ///< When event occurred
    bool           success = true;  ///< Whether operation succeeded

    PLM_EVENT() : type( PLM_EVENT_TYPE::AUTH_LOGIN ), timestamp_ms( 0 ) {}

    PLM_EVENT( PLM_EVENT_TYPE aType ) : type( aType ), timestamp_ms( 0 )
    {
        SetTimestampNow();
    }

    PLM_EVENT( PLM_EVENT_TYPE aType, const wxString& aProjectId ) :
            type( aType ),
            project_id( aProjectId ),
            timestamp_ms( 0 )
    {
        SetTimestampNow();
    }

    void SetTimestampNow()
    {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>( duration ).count();
    }

    /**
     * Create event from a server notification.
     */
    static PLM_EVENT FromNotification( const PLM_NOTIFICATION& aNotification )
    {
        PLM_EVENT event;
        event.project_id = aNotification.project_id;
        event.user_id = aNotification.user_id;
        event.message = aNotification.message;
        event.timestamp_ms = aNotification.timestamp;

        switch( aNotification.type )
        {
        case PLM_NOTIFICATION_TYPE::LOCK_ACQUIRED:
            event.type = PLM_EVENT_TYPE::LOCK_ACQUIRED;
            break;
        case PLM_NOTIFICATION_TYPE::LOCK_RELEASED:
            event.type = PLM_EVENT_TYPE::LOCK_RELEASED;
            break;
        case PLM_NOTIFICATION_TYPE::STATE_CHANGED:
            event.type = PLM_EVENT_TYPE::STATE_CHANGED;
            break;
        default:
            event.type = PLM_EVENT_TYPE::SERVER_NOTIFICATION;
            break;
        }

        return event;
    }

    /**
     * Create event from an audit event.
     */
    static PLM_EVENT FromAuditEvent( const PLM_AUDIT_EVENT& aAudit )
    {
        PLM_EVENT event;
        event.project_id = aAudit.project_id;
        event.user_id = aAudit.user_id;
        event.message = aAudit.GetEventTypeName();
        event.timestamp_ms = aAudit.timestamp_ms;
        event.success = aAudit.success;

        switch( aAudit.event_type )
        {
        case PLM_AUDIT_EVENT_TYPE::LOGIN:
            event.type = PLM_EVENT_TYPE::AUTH_LOGIN;
            break;
        case PLM_AUDIT_EVENT_TYPE::LOGOUT:
            event.type = PLM_EVENT_TYPE::AUTH_LOGOUT;
            break;
        case PLM_AUDIT_EVENT_TYPE::SESSION_EXPIRED:
            event.type = PLM_EVENT_TYPE::AUTH_SESSION_EXPIRED;
            break;
        case PLM_AUDIT_EVENT_TYPE::TOKEN_REFRESHED:
            event.type = PLM_EVENT_TYPE::AUTH_TOKEN_REFRESHED;
            break;
        case PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED:
            event.type = PLM_EVENT_TYPE::LOCK_ACQUIRED;
            break;
        case PLM_AUDIT_EVENT_TYPE::LOCK_RELEASED:
            event.type = PLM_EVENT_TYPE::LOCK_RELEASED;
            break;
        case PLM_AUDIT_EVENT_TYPE::LOCK_FORCED:
            event.type = PLM_EVENT_TYPE::LOCK_STOLEN;
            break;
        case PLM_AUDIT_EVENT_TYPE::LOCK_EXPIRED:
            event.type = PLM_EVENT_TYPE::LOCK_EXPIRED;
            break;
        case PLM_AUDIT_EVENT_TYPE::STATE_CHANGED:
            event.type = PLM_EVENT_TYPE::STATE_CHANGED;
            break;
        case PLM_AUDIT_EVENT_TYPE::SYNC_STARTED:
            event.type = PLM_EVENT_TYPE::STATE_SYNC_STARTED;
            break;
        case PLM_AUDIT_EVENT_TYPE::SYNC_COMPLETED:
            event.type = PLM_EVENT_TYPE::STATE_SYNC_COMPLETED;
            break;
        case PLM_AUDIT_EVENT_TYPE::PLUGIN_LOADED:
            event.type = PLM_EVENT_TYPE::PLUGIN_LOADED;
            break;
        case PLM_AUDIT_EVENT_TYPE::PLUGIN_UNLOADED:
            event.type = PLM_EVENT_TYPE::PLUGIN_UNLOADED;
            break;
        default:
            event.type = PLM_EVENT_TYPE::ACTIVITY_NEW;
            break;
        }

        return event;
    }

    /**
     * Get human-readable event type name.
     */
    wxString GetTypeName() const
    {
        switch( type )
        {
        case PLM_EVENT_TYPE::AUTH_LOGIN:          return wxS( "Login" );
        case PLM_EVENT_TYPE::AUTH_LOGOUT:         return wxS( "Logout" );
        case PLM_EVENT_TYPE::AUTH_SESSION_EXPIRED: return wxS( "Session Expired" );
        case PLM_EVENT_TYPE::AUTH_TOKEN_REFRESHED: return wxS( "Token Refreshed" );
        case PLM_EVENT_TYPE::LOCK_ACQUIRED:       return wxS( "Lock Acquired" );
        case PLM_EVENT_TYPE::LOCK_RELEASED:       return wxS( "Lock Released" );
        case PLM_EVENT_TYPE::LOCK_STOLEN:         return wxS( "Lock Stolen" );
        case PLM_EVENT_TYPE::LOCK_EXPIRED:        return wxS( "Lock Expired" );
        case PLM_EVENT_TYPE::STATE_CHANGED:       return wxS( "State Changed" );
        case PLM_EVENT_TYPE::STATE_SYNC_STARTED:  return wxS( "Sync Started" );
        case PLM_EVENT_TYPE::STATE_SYNC_COMPLETED: return wxS( "Sync Completed" );
        case PLM_EVENT_TYPE::STATE_SYNC_FAILED:   return wxS( "Sync Failed" );
        case PLM_EVENT_TYPE::ACTIVITY_UPDATED:    return wxS( "Activity Updated" );
        case PLM_EVENT_TYPE::ACTIVITY_NEW:        return wxS( "New Activity" );
        case PLM_EVENT_TYPE::PROJECT_BOUND:       return wxS( "Project Bound" );
        case PLM_EVENT_TYPE::PROJECT_UNBOUND:     return wxS( "Project Unbound" );
        case PLM_EVENT_TYPE::PROJECT_DIRTY:       return wxS( "Project Modified" );
        case PLM_EVENT_TYPE::PROJECT_CLEAN:       return wxS( "Project Clean" );
        case PLM_EVENT_TYPE::PLUGIN_LOADED:       return wxS( "Plugin Loaded" );
        case PLM_EVENT_TYPE::PLUGIN_UNLOADED:     return wxS( "Plugin Unloaded" );
        case PLM_EVENT_TYPE::PLUGIN_ERROR:        return wxS( "Plugin Error" );
        case PLM_EVENT_TYPE::SERVER_CONNECTED:    return wxS( "Server Connected" );
        case PLM_EVENT_TYPE::SERVER_DISCONNECTED: return wxS( "Server Disconnected" );
        case PLM_EVENT_TYPE::SERVER_NOTIFICATION: return wxS( "Server Notification" );
        default:                                  return wxS( "Unknown" );
        }
    }

    /**
     * Check if this is an authentication-related event.
     */
    bool IsAuthEvent() const
    {
        return type >= PLM_EVENT_TYPE::AUTH_LOGIN && type <= PLM_EVENT_TYPE::AUTH_TOKEN_REFRESHED;
    }

    /**
     * Check if this is a lock-related event.
     */
    bool IsLockEvent() const
    {
        return type >= PLM_EVENT_TYPE::LOCK_ACQUIRED && type <= PLM_EVENT_TYPE::LOCK_EXPIRED;
    }

    /**
     * Check if this is a state/sync-related event.
     */
    bool IsStateEvent() const
    {
        return type >= PLM_EVENT_TYPE::STATE_CHANGED && type <= PLM_EVENT_TYPE::STATE_SYNC_FAILED;
    }

    /**
     * Check if this is a project-related event.
     */
    bool IsProjectEvent() const
    {
        return type >= PLM_EVENT_TYPE::PROJECT_BOUND && type <= PLM_EVENT_TYPE::PROJECT_CLEAN;
    }
};


/**
 * Callback type for event subscribers.
 *
 * @param aEvent The event that occurred.
 */
using PLM_EVENT_CALLBACK = std::function<void( const PLM_EVENT& aEvent )>;


/**
 * Unique identifier for a subscription.
 */
using PLM_SUBSCRIPTION_ID = uint64_t;


/**
 * Filter for event subscriptions.
 *
 * Allows subscribers to receive only specific event types.
 */
struct PLM_EVENT_FILTER
{
    std::set<PLM_EVENT_TYPE> event_types;   ///< Event types to receive (empty = all)
    wxString                 project_id;    ///< Filter by project (empty = all)
    bool                     success_only = false;  ///< Only successful events

    PLM_EVENT_FILTER() = default;

    /**
     * Create filter for specific event types.
     */
    static PLM_EVENT_FILTER ForTypes( std::initializer_list<PLM_EVENT_TYPE> aTypes )
    {
        PLM_EVENT_FILTER filter;
        filter.event_types = std::set<PLM_EVENT_TYPE>( aTypes );
        return filter;
    }

    /**
     * Create filter for a specific project.
     */
    static PLM_EVENT_FILTER ForProject( const wxString& aProjectId )
    {
        PLM_EVENT_FILTER filter;
        filter.project_id = aProjectId;
        return filter;
    }

    /**
     * Create filter for authentication events.
     */
    static PLM_EVENT_FILTER AuthEvents()
    {
        return ForTypes( { PLM_EVENT_TYPE::AUTH_LOGIN,
                           PLM_EVENT_TYPE::AUTH_LOGOUT,
                           PLM_EVENT_TYPE::AUTH_SESSION_EXPIRED,
                           PLM_EVENT_TYPE::AUTH_TOKEN_REFRESHED } );
    }

    /**
     * Create filter for lock events.
     */
    static PLM_EVENT_FILTER LockEvents()
    {
        return ForTypes( { PLM_EVENT_TYPE::LOCK_ACQUIRED,
                           PLM_EVENT_TYPE::LOCK_RELEASED,
                           PLM_EVENT_TYPE::LOCK_STOLEN,
                           PLM_EVENT_TYPE::LOCK_EXPIRED } );
    }

    /**
     * Create filter for state events.
     */
    static PLM_EVENT_FILTER StateEvents()
    {
        return ForTypes( { PLM_EVENT_TYPE::STATE_CHANGED,
                           PLM_EVENT_TYPE::STATE_SYNC_STARTED,
                           PLM_EVENT_TYPE::STATE_SYNC_COMPLETED,
                           PLM_EVENT_TYPE::STATE_SYNC_FAILED } );
    }

    /**
     * Check if an event matches this filter.
     */
    bool Matches( const PLM_EVENT& aEvent ) const
    {
        if( !event_types.empty() && event_types.find( aEvent.type ) == event_types.end() )
            return false;

        if( !project_id.IsEmpty() && aEvent.project_id != project_id )
            return false;

        if( success_only && !aEvent.success )
            return false;

        return true;
    }
};


/**
 * Central event bus for PLM events.
 *
 * Provides a publish/subscribe mechanism for PLM events. Components can
 * subscribe to events with optional filtering and will receive callbacks
 * when matching events are published.
 *
 * Thread-safe: All public methods are protected by mutex. Callbacks are
 * invoked while holding the mutex, so subscribers should avoid blocking
 * operations and should not call back into the event bus.
 *
 * Usage:
 * @code
 * PLM_EVENT_BUS& bus = PLM_EVENT_BUS::GetInstance();
 *
 * // Subscribe to all events
 * auto id1 = bus.Subscribe( []( const PLM_EVENT& event ) {
 *     wxLogMessage( "Event: %s", event.GetTypeName() );
 * });
 *
 * // Subscribe to lock events only
 * auto id2 = bus.Subscribe(
 *     []( const PLM_EVENT& event ) { UpdateLockUI( event ); },
 *     PLM_EVENT_FILTER::LockEvents()
 * );
 *
 * // Publish an event
 * PLM_EVENT event( PLM_EVENT_TYPE::LOCK_ACQUIRED, "project-123" );
 * bus.Publish( event );
 *
 * // Unsubscribe when done
 * bus.Unsubscribe( id1 );
 * bus.Unsubscribe( id2 );
 * @endcode
 */
class KICOMMON_API PLM_EVENT_BUS
{
public:
    /**
     * Get the singleton event bus instance.
     *
     * @return Reference to the global event bus.
     */
    static PLM_EVENT_BUS& GetInstance();

    // Non-copyable singleton
    PLM_EVENT_BUS( const PLM_EVENT_BUS& ) = delete;
    PLM_EVENT_BUS& operator=( const PLM_EVENT_BUS& ) = delete;

    //-------------------------------------------------------------------------
    // Subscription Management
    //-------------------------------------------------------------------------

    /**
     * Subscribe to events.
     *
     * @param aCallback Callback to invoke when events occur.
     * @param aFilter Optional filter for event types.
     * @return Subscription ID for later unsubscription.
     */
    PLM_SUBSCRIPTION_ID Subscribe( PLM_EVENT_CALLBACK aCallback,
                                    const PLM_EVENT_FILTER& aFilter = PLM_EVENT_FILTER() );

    /**
     * Unsubscribe from events.
     *
     * @param aId Subscription ID returned from Subscribe().
     */
    void Unsubscribe( PLM_SUBSCRIPTION_ID aId );

    /**
     * Unsubscribe all handlers.
     *
     * Removes all subscriptions. Use with caution.
     */
    void UnsubscribeAll();

    /**
     * Get the number of active subscriptions.
     *
     * @return Number of subscribers.
     */
    size_t GetSubscriberCount() const;

    //-------------------------------------------------------------------------
    // Event Publishing
    //-------------------------------------------------------------------------

    /**
     * Publish an event to all matching subscribers.
     *
     * @param aEvent Event to publish.
     */
    void Publish( const PLM_EVENT& aEvent );

    /**
     * Publish an event constructed from parameters.
     *
     * @param aType Event type.
     * @param aProjectId Optional project ID.
     * @param aMessage Optional message.
     */
    void Publish( PLM_EVENT_TYPE aType,
                  const wxString& aProjectId = wxEmptyString,
                  const wxString& aMessage = wxEmptyString );

    /**
     * Publish event from a server notification.
     *
     * @param aNotification Server notification to convert and publish.
     */
    void PublishNotification( const PLM_NOTIFICATION& aNotification );

    /**
     * Publish event from an audit event.
     *
     * @param aAudit Audit event to convert and publish.
     */
    void PublishAuditEvent( const PLM_AUDIT_EVENT& aAudit );

    //-------------------------------------------------------------------------
    // Statistics
    //-------------------------------------------------------------------------

    /**
     * Get total number of events published.
     *
     * @return Event count since startup.
     */
    uint64_t GetPublishCount() const;

    /**
     * Reset statistics.
     */
    void ResetStats();

private:
    PLM_EVENT_BUS();
    ~PLM_EVENT_BUS() = default;

    struct Subscription
    {
        PLM_SUBSCRIPTION_ID  id;
        PLM_EVENT_CALLBACK   callback;
        PLM_EVENT_FILTER     filter;
    };

    mutable std::mutex            m_mutex;
    std::vector<Subscription>     m_subscriptions;
    PLM_SUBSCRIPTION_ID           m_nextId;
    uint64_t                      m_publishCount;
};


#endif // PLM_EVENT_BUS_H
