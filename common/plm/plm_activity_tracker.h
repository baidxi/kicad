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
 * @file plm_activity_tracker.h
 * @brief Activity tracking for PLM operations.
 *
 * This header defines the activity tracker that caches and displays recent
 * PLM operations for a project. Activity is retrieved from the plugin and
 * cached locally for UI display.
 */

#ifndef PLM_ACTIVITY_TRACKER_H
#define PLM_ACTIVITY_TRACKER_H

#include "plm_audit.h"

#include <wx/string.h>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>


class PLM_PLUGIN;


/**
 * Configuration for activity tracking.
 */
struct PLM_ACTIVITY_CONFIG
{
    size_t  max_entries = 100;      ///< Maximum entries to cache per project
    int64_t auto_refresh_ms = 0;    ///< Auto-refresh interval (0 = disabled)
    bool    include_errors = true;  ///< Include error events
    bool    include_system = false; ///< Include system events (sync, cache)

    PLM_ACTIVITY_CONFIG() = default;

    static PLM_ACTIVITY_CONFIG Default()
    {
        return PLM_ACTIVITY_CONFIG();
    }

    static PLM_ACTIVITY_CONFIG Verbose()
    {
        PLM_ACTIVITY_CONFIG cfg;
        cfg.max_entries = 500;
        cfg.include_system = true;
        return cfg;
    }
};


/**
 * A simplified activity entry for display purposes.
 *
 * Distilled from PLM_AUDIT_EVENT with only the fields needed for UI.
 */
struct PLM_ACTIVITY_ENTRY
{
    wxString user_name;         ///< Display name of user who performed action
    wxString action;            ///< Human-readable action description
    int64_t  timestamp_ms = 0;  ///< When the action occurred
    wxString project_name;      ///< Name of affected project (if any)
    bool     is_error = false;  ///< Whether this represents an error

    PLM_ACTIVITY_ENTRY() = default;

    PLM_ACTIVITY_ENTRY( const wxString& aUser, const wxString& aAction, int64_t aTimestamp ) :
            user_name( aUser ),
            action( aAction ),
            timestamp_ms( aTimestamp )
    {
    }

    /**
     * Create an activity entry from an audit event.
     * @param aEvent The audit event to convert.
     * @return Activity entry for display.
     */
    static PLM_ACTIVITY_ENTRY FromAuditEvent( const PLM_AUDIT_EVENT& aEvent )
    {
        PLM_ACTIVITY_ENTRY entry;
        entry.user_name = aEvent.user_name.IsEmpty() ? aEvent.user_id : aEvent.user_name;
        entry.action = aEvent.GetEventTypeName();
        entry.timestamp_ms = aEvent.timestamp_ms;
        entry.project_name = aEvent.project_name;
        entry.is_error = aEvent.IsError();
        return entry;
    }
};


/**
 * Callback for activity refresh completion.
 *
 * @param aProjectId Project that was refreshed.
 * @param aSuccess Whether refresh succeeded.
 * @param aNewCount Number of new entries retrieved.
 */
using PLM_ACTIVITY_REFRESH_CALLBACK = std::function<void( const wxString& aProjectId,
                                                           bool aSuccess,
                                                           size_t aNewCount )>;

/**
 * Callback for activity change notifications.
 *
 * @param aProjectId Project with new activity.
 * @param aEntry The new activity entry.
 */
using PLM_ACTIVITY_CHANGE_CALLBACK = std::function<void( const wxString& aProjectId,
                                                          const PLM_ACTIVITY_ENTRY& aEntry )>;

/**
 * Function type for fetching activity from plugin.
 *
 * @param aProjectId Project to fetch activity for.
 * @param aFilter Filter to apply.
 * @return Vector of audit events.
 */
using PLM_ACTIVITY_FETCH_FUNC = std::function<std::vector<PLM_AUDIT_EVENT>(
        const wxString& aProjectId, const PLM_AUDIT_FILTER& aFilter )>;


/**
 * Tracks and caches PLM activity for display.
 *
 * The activity tracker maintains a cache of recent PLM operations per project.
 * Activity can be pushed to the tracker (from notifications) or pulled via
 * explicit refresh. The cache is limited in size to prevent memory bloat.
 *
 * Thread-safe: All public methods are protected by mutex.
 *
 * Usage:
 * @code
 * PLM_ACTIVITY_TRACKER tracker;
 *
 * // Configure tracker
 * tracker.SetConfig( PLM_ACTIVITY_CONFIG::Default() );
 *
 * // Set fetch function to query plugin
 * tracker.SetFetchFunction( [&plugin]( const wxString& projectId,
 *                                       const PLM_AUDIT_FILTER& filter ) {
 *     return plugin->GetProjectActivity( projectId, filter );
 * });
 *
 * // Subscribe to changes
 * tracker.SetChangeCallback( []( const wxString& projectId,
 *                                 const PLM_ACTIVITY_ENTRY& entry ) {
 *     // Update UI
 * });
 *
 * // Refresh activity for a project
 * tracker.RefreshActivity( projectId );
 *
 * // Get cached activity
 * auto activity = tracker.GetActivity( projectId );
 * @endcode
 */
class PLM_ACTIVITY_TRACKER
{
public:
    PLM_ACTIVITY_TRACKER();
    ~PLM_ACTIVITY_TRACKER() = default;

    // Non-copyable
    PLM_ACTIVITY_TRACKER( const PLM_ACTIVITY_TRACKER& ) = delete;
    PLM_ACTIVITY_TRACKER& operator=( const PLM_ACTIVITY_TRACKER& ) = delete;

    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------

    /**
     * Set the activity tracking configuration.
     *
     * @param aConfig Configuration options.
     */
    void SetConfig( const PLM_ACTIVITY_CONFIG& aConfig );

    /**
     * Get the current configuration.
     *
     * @return Current configuration.
     */
    PLM_ACTIVITY_CONFIG GetConfig() const;

    /**
     * Set the function used to fetch activity from the plugin.
     *
     * @param aFunc Fetch function.
     */
    void SetFetchFunction( PLM_ACTIVITY_FETCH_FUNC aFunc );

    //-------------------------------------------------------------------------
    // Activity Management
    //-------------------------------------------------------------------------

    /**
     * Refresh activity for a project.
     *
     * Fetches the latest activity from the plugin and updates the cache.
     * This is a synchronous operation.
     *
     * @param aProjectId Project to refresh activity for.
     * @return True if refresh succeeded.
     */
    bool RefreshActivity( const wxString& aProjectId );

    /**
     * Refresh activity asynchronously.
     *
     * @param aProjectId Project to refresh.
     * @param aCallback Completion callback (called on worker thread).
     */
    void RefreshActivityAsync( const wxString& aProjectId,
                               PLM_ACTIVITY_REFRESH_CALLBACK aCallback = nullptr );

    /**
     * Add an activity event to the cache.
     *
     * Call this when activity is pushed from the server via notifications.
     *
     * @param aProjectId Project the activity belongs to.
     * @param aEvent Audit event to add.
     */
    void AddActivity( const wxString& aProjectId, const PLM_AUDIT_EVENT& aEvent );

    /**
     * Add multiple activity events to the cache.
     *
     * @param aProjectId Project the activities belong to.
     * @param aEvents Events to add.
     */
    void AddActivities( const wxString& aProjectId,
                        const std::vector<PLM_AUDIT_EVENT>& aEvents );

    /**
     * Clear cached activity for a project.
     *
     * @param aProjectId Project to clear (empty = clear all).
     */
    void ClearActivity( const wxString& aProjectId = wxEmptyString );

    //-------------------------------------------------------------------------
    // Activity Queries
    //-------------------------------------------------------------------------

    /**
     * Get cached activity for a project.
     *
     * @param aProjectId Project to get activity for.
     * @param aLimit Maximum entries to return (0 = all cached).
     * @return Vector of activity entries, most recent first.
     */
    std::vector<PLM_ACTIVITY_ENTRY> GetActivity( const wxString& aProjectId,
                                                  size_t aLimit = 0 ) const;

    /**
     * Get all cached activity across all projects.
     *
     * @param aLimit Maximum entries to return (0 = all cached).
     * @return Vector of activity entries, most recent first.
     */
    std::vector<PLM_ACTIVITY_ENTRY> GetAllActivity( size_t aLimit = 0 ) const;

    /**
     * Get activity count for a project.
     *
     * @param aProjectId Project to count.
     * @return Number of cached entries.
     */
    size_t GetActivityCount( const wxString& aProjectId ) const;

    /**
     * Get total activity count across all projects.
     *
     * @return Total cached entries.
     */
    size_t GetTotalActivityCount() const;

    /**
     * Check if activity is cached for a project.
     *
     * @param aProjectId Project to check.
     * @return True if activity exists in cache.
     */
    bool HasActivity( const wxString& aProjectId ) const;

    /**
     * Get the timestamp of the most recent activity for a project.
     *
     * @param aProjectId Project to check.
     * @return Timestamp in milliseconds, or 0 if no activity.
     */
    int64_t GetLatestTimestamp( const wxString& aProjectId ) const;

    //-------------------------------------------------------------------------
    // Callbacks
    //-------------------------------------------------------------------------

    /**
     * Set callback for activity changes.
     *
     * Called when new activity is added to the cache.
     *
     * @param aCallback Change callback.
     */
    void SetChangeCallback( PLM_ACTIVITY_CHANGE_CALLBACK aCallback );

private:
    /**
     * Add entry to project cache, enforcing size limits.
     */
    void AddEntryToCache( const wxString& aProjectId, const PLM_ACTIVITY_ENTRY& aEntry );

    /**
     * Check if event should be included based on config.
     */
    bool ShouldIncludeEvent( const PLM_AUDIT_EVENT& aEvent ) const;

    /**
     * Fire change callback for new entry.
     */
    void FireChangeEvent( const wxString& aProjectId, const PLM_ACTIVITY_ENTRY& aEntry );

    mutable std::mutex                                      m_mutex;
    PLM_ACTIVITY_CONFIG                                     m_config;
    PLM_ACTIVITY_FETCH_FUNC                                 m_fetchFunc;
    PLM_ACTIVITY_CHANGE_CALLBACK                            m_changeCallback;
    std::map<wxString, std::deque<PLM_ACTIVITY_ENTRY>>      m_cache;
};


#endif // PLM_ACTIVITY_TRACKER_H
