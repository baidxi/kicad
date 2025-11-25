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
 * @file plm_notification_ui.h
 * @brief PLM notification UI system.
 */

#ifndef PLM_NOTIFICATION_UI_H
#define PLM_NOTIFICATION_UI_H

#include <kicommon.h>
#include <plm/plm_event_bus.h>

#include <wx/frame.h>

#include <functional>
#include <queue>
#include <mutex>


/**
 * PLM notification severity levels.
 */
enum class PLM_NOTIFICATION_LEVEL
{
    INFO,       ///< Informational notification
    WARNING,    ///< Warning notification
    ERROR_MSG,  ///< Error notification
    SUCCESS     ///< Success notification
};


/**
 * PLM UI notification types.
 */
enum class PLM_UI_NOTIFICATION_TYPE
{
    CONNECTION_CHANGED,     ///< Connection status changed
    LOCK_ACQUIRED,          ///< Lock acquired
    LOCK_RELEASED,          ///< Lock released
    LOCK_STOLEN,            ///< Lock was stolen by another user
    LOCK_EXPIRED,           ///< Lock expired
    SYNC_CONFLICT,          ///< Sync conflict detected
    SYNC_COMPLETED,         ///< Sync completed
    STATE_CHANGED,          ///< General state change
    CHECKIN_SUCCESS,        ///< Check-in succeeded
    CHECKIN_FAILED,         ///< Check-in failed
    CHECKOUT_SUCCESS,       ///< Check-out succeeded
    CHECKOUT_FAILED         ///< Check-out failed
};


/**
 * PLM UI notification data.
 */
struct PLM_UI_NOTIFICATION
{
    PLM_UI_NOTIFICATION_TYPE type;     ///< Notification type
    PLM_NOTIFICATION_LEVEL level;      ///< Severity level
    wxString title;                    ///< Notification title
    wxString message;                  ///< Notification message
    wxString project_id;               ///< Related project ID
    int64_t timestamp = 0;             ///< Timestamp (Unix epoch ms)
    bool dismissable = true;           ///< Can be dismissed by user
    int timeout_ms = 5000;             ///< Auto-dismiss timeout (0 = no auto-dismiss)
};


/**
 * PLM UI notification action callback.
 *
 * Called when user clicks on notification.
 */
using PLM_UI_NOTIFICATION_CALLBACK = std::function<void( const PLM_UI_NOTIFICATION& aNotification )>;


/**
 * PLM notification preferences.
 */
struct PLM_NOTIFICATION_PREFS
{
    bool enabled = true;                        ///< Notifications enabled
    bool show_connection = true;                ///< Show connection notifications
    bool show_lock = true;                      ///< Show lock notifications
    bool show_sync = true;                      ///< Show sync notifications
    bool show_checkin = true;                   ///< Show check-in notifications
    bool play_sound = false;                    ///< Play sound for notifications
    int default_timeout_ms = 5000;              ///< Default notification timeout
};


/**
 * PLM notification UI manager.
 *
 * Manages PLM notifications including toast/banner display, notification
 * queueing, and preferences. Subscribes to PLM_EVENT_BUS for automatic
 * event-to-notification conversion.
 */
class KICOMMON_API PLM_NOTIFICATION_UI
{
public:
    PLM_NOTIFICATION_UI();
    ~PLM_NOTIFICATION_UI();

    /**
     * Initialize notification system.
     *
     * @param aParent Parent window for notification display.
     */
    void Initialize( wxFrame* aParent );

    /**
     * Shutdown notification system.
     */
    void Shutdown();

    /**
     * Show a notification.
     *
     * @param aNotification Notification to show.
     */
    void ShowNotification( const PLM_UI_NOTIFICATION& aNotification );

    /**
     * Create notification from PLM event.
     *
     * @param aEvent PLM event.
     * @return Notification, or nullopt if event shouldn't produce notification.
     */
    std::optional<PLM_UI_NOTIFICATION> CreateFromEvent( const PLM_EVENT& aEvent );

    /**
     * Dismiss all visible notifications.
     */
    void DismissAll();

    /**
     * Dismiss notifications for a specific project.
     *
     * @param aProjectId Project ID.
     */
    void DismissForProject( const wxString& aProjectId );

    /**
     * Get notification preferences.
     *
     * @return Current preferences.
     */
    const PLM_NOTIFICATION_PREFS& GetPreferences() const { return m_prefs; }

    /**
     * Set notification preferences.
     *
     * @param aPrefs New preferences.
     */
    void SetPreferences( const PLM_NOTIFICATION_PREFS& aPrefs );

    /**
     * Check if notifications are enabled.
     *
     * @return True if enabled.
     */
    bool IsEnabled() const { return m_prefs.enabled; }

    /**
     * Enable or disable notifications.
     *
     * @param aEnable True to enable.
     */
    void SetEnabled( bool aEnable );

    /**
     * Set callback for notification clicks.
     */
    void SetClickCallback( PLM_UI_NOTIFICATION_CALLBACK aCallback ) { m_clickCb = aCallback; }

    /**
     * Get localized title for notification type.
     *
     * @param aType Notification type.
     * @return Localized title.
     */
    static wxString GetNotificationTitle( PLM_UI_NOTIFICATION_TYPE aType );

    /**
     * Get appropriate level for notification type.
     *
     * @param aType Notification type.
     * @return Severity level.
     */
    static PLM_NOTIFICATION_LEVEL GetNotificationLevel( PLM_UI_NOTIFICATION_TYPE aType );

private:
    void subscribeToEvents();
    void unsubscribeFromEvents();
    void onPlmEvent( const PLM_EVENT& aEvent );
    bool shouldShowNotification( PLM_UI_NOTIFICATION_TYPE aType ) const;

    wxFrame*                        m_parent;
    PLM_SUBSCRIPTION_ID             m_subscriptionId;
    PLM_NOTIFICATION_PREFS          m_prefs;
    PLM_UI_NOTIFICATION_CALLBACK    m_clickCb;
    std::queue<PLM_UI_NOTIFICATION> m_queue;
    mutable std::mutex          m_mutex;
};


#endif // PLM_NOTIFICATION_UI_H
