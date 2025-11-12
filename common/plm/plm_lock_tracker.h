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
 * @file plm_lock_tracker.h
 * @brief Lock state tracking for PLM-managed projects.
 *
 * This header defines the PLM_LOCK_TRACKER class which tracks lock state
 * for open projects and provides events on lock state changes.
 */

#ifndef PLM_LOCK_TRACKER_H
#define PLM_LOCK_TRACKER_H

#include "plm_lock.h"
#include "plm_types.h"

#include <wx/string.h>
#include <functional>
#include <map>
#include <mutex>
#include <vector>


/**
 * Event types for lock state changes.
 */
enum class PLM_LOCK_EVENT
{
    LOCK_ACQUIRED,      ///< Current user acquired lock
    LOCK_RELEASED,      ///< Lock was released (by anyone)
    LOCK_STOLEN,        ///< Lock was force-acquired by another user
    LOCK_EXPIRED,       ///< Lock has expired
    LOCK_BLOCKED        ///< Lock operation was blocked
};


/**
 * Callback type for lock state change events.
 *
 * @param aProjectId Project ID whose lock state changed.
 * @param aEvent The type of lock event.
 * @param aLockInfo Updated lock information.
 */
using PLM_LOCK_EVENT_CALLBACK = std::function<void( const wxString& aProjectId,
                                                     PLM_LOCK_EVENT aEvent,
                                                     const PLM_LOCK_INFO& aLockInfo )>;

/**
 * Callback type for polling lock state from the server.
 *
 * @param aProjectId Project ID to check.
 * @return Current lock information from server.
 */
using PLM_LOCK_POLL_CALLBACK = std::function<PLM_LOCK_INFO( const wxString& aProjectId )>;


/**
 * Tracked lock entry with metadata.
 */
struct PLM_TRACKED_LOCK
{
    wxString      project_id;           ///< Project ID being tracked
    PLM_LOCK_INFO lock_info;            ///< Current lock state
    int64_t       last_checked = 0;     ///< Last time lock was verified
    bool          is_monitored = false; ///< True if actively monitoring for changes

    PLM_TRACKED_LOCK() = default;

    explicit PLM_TRACKED_LOCK( const wxString& aProjectId, const PLM_LOCK_INFO& aInfo = PLM_LOCK_INFO() ) :
            project_id( aProjectId ),
            lock_info( aInfo ),
            last_checked( std::time( nullptr ) )
    {
    }
};


/**
 * Tracks lock state for PLM-managed projects.
 *
 * Maintains current lock state for open projects and provides events
 * when lock state changes. Works with or without server notifications
 * by supporting optional polling.
 *
 * Thread-safe: All public methods are protected by mutex.
 *
 * Usage:
 * @code
 * PLM_LOCK_TRACKER tracker;
 *
 * // Set event callback for UI updates
 * tracker.SetEventCallback( []( const wxString& projectId, PLM_LOCK_EVENT event,
 *                               const PLM_LOCK_INFO& info ) {
 *     // Update UI based on lock state change
 * });
 *
 * // Start tracking a project
 * tracker.StartTracking( "project-123" );
 *
 * // Update lock state (from explicit lock/unlock)
 * tracker.UpdateLockState( "project-123", newLockInfo );
 *
 * // Check lock state
 * PLM_LOCK_INFO info;
 * if( tracker.GetLockState( "project-123", info ) )
 * {
 *     if( info.CanModify() )
 *         // Allow editing
 * }
 *
 * // Stop tracking when project is closed
 * tracker.StopTracking( "project-123" );
 * @endcode
 */
class PLM_LOCK_TRACKER
{
public:
    PLM_LOCK_TRACKER();
    ~PLM_LOCK_TRACKER() = default;

    // Non-copyable
    PLM_LOCK_TRACKER( const PLM_LOCK_TRACKER& ) = delete;
    PLM_LOCK_TRACKER& operator=( const PLM_LOCK_TRACKER& ) = delete;

    //-------------------------------------------------------------------------
    // Tracking Management
    //-------------------------------------------------------------------------

    /**
     * Start tracking lock state for a project.
     *
     * @param aProjectId Project ID to track.
     * @param aInitialInfo Optional initial lock state.
     */
    void StartTracking( const wxString& aProjectId,
                        const PLM_LOCK_INFO& aInitialInfo = PLM_LOCK_INFO() );

    /**
     * Stop tracking lock state for a project.
     *
     * @param aProjectId Project ID to stop tracking.
     */
    void StopTracking( const wxString& aProjectId );

    /**
     * Stop tracking all projects.
     */
    void StopTrackingAll();

    /**
     * Check if a project is being tracked.
     *
     * @param aProjectId Project ID to check.
     * @return True if the project is being tracked.
     */
    bool IsTracking( const wxString& aProjectId ) const;

    /**
     * Get list of tracked project IDs.
     *
     * @return Vector of project IDs being tracked.
     */
    std::vector<wxString> GetTrackedProjects() const;

    //-------------------------------------------------------------------------
    // Lock State Access
    //-------------------------------------------------------------------------

    /**
     * Get current lock state for a project.
     *
     * @param aProjectId Project ID to check.
     * @param aInfo Output lock information.
     * @return True if project is tracked and info was retrieved.
     */
    bool GetLockState( const wxString& aProjectId, PLM_LOCK_INFO& aInfo ) const;

    /**
     * Check if current user holds the lock.
     *
     * @param aProjectId Project ID to check.
     * @return True if current user holds the lock.
     */
    bool IsLockedByMe( const wxString& aProjectId ) const;

    /**
     * Check if project is locked by another user.
     *
     * @param aProjectId Project ID to check.
     * @return True if locked by someone other than current user.
     */
    bool IsLockedByOther( const wxString& aProjectId ) const;

    /**
     * Check if modifications are allowed.
     *
     * @param aProjectId Project ID to check.
     * @return True if not tracked, not locked, or locked by current user.
     */
    bool CanModify( const wxString& aProjectId ) const;

    //-------------------------------------------------------------------------
    // Lock State Updates
    //-------------------------------------------------------------------------

    /**
     * Update lock state from explicit lock/unlock operation.
     *
     * Fires appropriate event if state changed.
     *
     * @param aProjectId Project ID.
     * @param aNewInfo New lock information.
     */
    void UpdateLockState( const wxString& aProjectId, const PLM_LOCK_INFO& aNewInfo );

    /**
     * Handle lock acquired event.
     *
     * Call this when the current user successfully acquires a lock.
     *
     * @param aProjectId Project ID.
     * @param aLockInfo Lock information.
     */
    void OnLockAcquired( const wxString& aProjectId, const PLM_LOCK_INFO& aLockInfo );

    /**
     * Handle lock released event.
     *
     * Call this when a lock is released (by anyone).
     *
     * @param aProjectId Project ID.
     */
    void OnLockReleased( const wxString& aProjectId );

    /**
     * Handle notification that another user acquired the lock.
     *
     * Call this when receiving a server notification that lock was stolen.
     *
     * @param aProjectId Project ID.
     * @param aLockInfo Lock information showing new owner.
     */
    void OnLockStolen( const wxString& aProjectId, const PLM_LOCK_INFO& aLockInfo );

    //-------------------------------------------------------------------------
    // Polling Support
    //-------------------------------------------------------------------------

    /**
     * Set callback for polling lock state.
     *
     * @param aCallback Function to poll lock state from server.
     */
    void SetPollCallback( PLM_LOCK_POLL_CALLBACK aCallback );

    /**
     * Poll lock state for a project.
     *
     * Uses the poll callback to check current state and fires event if changed.
     *
     * @param aProjectId Project ID to poll.
     * @return True if state was successfully polled.
     */
    bool PollLockState( const wxString& aProjectId );

    /**
     * Poll lock state for all tracked projects.
     *
     * Call this periodically if server doesn't support push notifications.
     */
    void PollAllLockStates();

    //-------------------------------------------------------------------------
    // Event Callbacks
    //-------------------------------------------------------------------------

    /**
     * Set callback for lock events.
     *
     * @param aCallback Event callback function.
     */
    void SetEventCallback( PLM_LOCK_EVENT_CALLBACK aCallback );

private:
    /**
     * Fire a lock event.
     */
    void FireEvent( const wxString& aProjectId, PLM_LOCK_EVENT aEvent,
                    const PLM_LOCK_INFO& aLockInfo );

    /**
     * Determine lock event type from state transition.
     */
    PLM_LOCK_EVENT DetermineEventType( const PLM_LOCK_INFO& aOldInfo,
                                        const PLM_LOCK_INFO& aNewInfo ) const;

    mutable std::mutex                       m_mutex;
    std::map<wxString, PLM_TRACKED_LOCK>     m_trackedLocks;
    PLM_LOCK_EVENT_CALLBACK                  m_eventCallback;
    PLM_LOCK_POLL_CALLBACK                   m_pollCallback;
};


#endif // PLM_LOCK_TRACKER_H
