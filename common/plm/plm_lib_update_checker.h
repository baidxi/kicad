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
 * @file plm_lib_update_checker.h
 * @brief PLM library update detection and notification.
 */

#ifndef PLM_LIB_UPDATE_CHECKER_H
#define PLM_LIB_UPDATE_CHECKER_H

#include <kicommon.h>
#include "plm_lib_reference.h"
#include "plm_lib_version_tracker.h"

#include <wx/string.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <vector>


/**
 * Information about an available library update.
 */
struct PLM_LIB_UPDATE_INFO
{
    wxString      lib_id;            ///< Library identifier
    wxString      lib_name;          ///< Display name
    PLM_LIB_TYPE  lib_type;          ///< Library type
    wxString      current_revision;  ///< Currently installed revision
    wxString      latest_revision;   ///< Available revision on server
    wxString      change_summary;    ///< Summary of what changed
    int64_t       update_timestamp;  ///< When update was released
    bool          is_pinned;         ///< True if library is pinned
    bool          is_critical;       ///< True if update is critical

    PLM_LIB_UPDATE_INFO() :
            lib_type( PLM_LIB_TYPE::SYMBOL ),
            update_timestamp( 0 ),
            is_pinned( false ),
            is_critical( false )
    {
    }

    bool HasUpdate() const
    {
        return !latest_revision.IsEmpty() && latest_revision != current_revision;
    }
};


/**
 * Report of all available updates.
 */
struct PLM_LIB_UPDATE_REPORT
{
    std::vector<PLM_LIB_UPDATE_INFO> updates;      ///< Libraries with updates
    size_t                           total_checked; ///< Total libraries checked
    size_t                           up_to_date;    ///< Libraries already current
    size_t                           pinned;        ///< Pinned libraries (won't update)
    size_t                           errors;        ///< Errors during check
    int64_t                          check_time_ms; ///< Time taken for check

    PLM_LIB_UPDATE_REPORT() :
            total_checked( 0 ), up_to_date( 0 ), pinned( 0 ), errors( 0 ), check_time_ms( 0 )
    {
    }

    bool HasUpdates() const { return !updates.empty(); }

    size_t GetUpdateCount() const { return updates.size(); }

    size_t GetCriticalCount() const
    {
        size_t count = 0;

        for( const auto& u : updates )
        {
            if( u.is_critical )
                count++;
        }

        return count;
    }

    std::vector<PLM_LIB_UPDATE_INFO> GetUnpinnedUpdates() const
    {
        std::vector<PLM_LIB_UPDATE_INFO> result;

        for( const auto& u : updates )
        {
            if( !u.is_pinned )
                result.push_back( u );
        }

        return result;
    }
};


/**
 * Callback for getting latest revision from server.
 *
 * @param aLibId Library identifier.
 * @return Latest revision string, or empty on error.
 */
using PLM_GET_REVISION_CALLBACK = std::function<wxString( const wxString& aLibId )>;


/**
 * Callback for update check completion.
 *
 * @param aReport Update report.
 */
using PLM_UPDATE_CHECK_CALLBACK = std::function<void( const PLM_LIB_UPDATE_REPORT& aReport )>;


/**
 * Callback for update notification.
 *
 * Called when updates are found, allowing UI to show notification.
 *
 * @param aUpdateCount Number of updates available.
 * @param aCriticalCount Number of critical updates.
 */
using PLM_UPDATE_NOTIFY_CALLBACK = std::function<void( size_t aUpdateCount,
                                                        size_t aCriticalCount )>;


/**
 * PLM library update checker.
 *
 * Checks for updates to PLM-managed libraries and generates reports.
 * Supports background checking with optional UI notifications.
 *
 * Usage:
 * @code
 * PLM_LIB_UPDATE_CHECKER checker;
 * checker.SetVersionTracker( &versionTracker );
 * checker.SetGetRevisionCallback( []( const wxString& libId ) {
 *     return plugin->GetLatestRevision( libId );
 * });
 *
 * // Check for updates
 * auto refs = refMgr.GetAllReferences();
 * auto report = checker.CheckForUpdates( refs );
 *
 * if( report.HasUpdates() )
 * {
 *     for( const auto& update : report.updates )
 *         wxLogMessage( "Update available for %s: %s -> %s",
 *                       update.lib_name, update.current_revision,
 *                       update.latest_revision );
 * }
 * @endcode
 */
class KICOMMON_API PLM_LIB_UPDATE_CHECKER
{
public:
    PLM_LIB_UPDATE_CHECKER();
    ~PLM_LIB_UPDATE_CHECKER();

    /**
     * Set version tracker for checking pinned state.
     *
     * @param aTracker Version tracker.
     */
    void SetVersionTracker( PLM_LIB_VERSION_TRACKER* aTracker );

    /**
     * Set callback for getting latest revisions.
     *
     * @param aCallback Callback function.
     */
    void SetGetRevisionCallback( PLM_GET_REVISION_CALLBACK aCallback );

    /**
     * Set callback for update notifications.
     *
     * Called when updates are found during background checks.
     *
     * @param aCallback Notification callback.
     */
    void SetNotifyCallback( PLM_UPDATE_NOTIFY_CALLBACK aCallback );

    /**
     * Check a single library for updates.
     *
     * @param aRef Library reference.
     * @return Update info.
     */
    PLM_LIB_UPDATE_INFO CheckLibrary( const PLM_LIB_REFERENCE& aRef );

    /**
     * Check all libraries for updates.
     *
     * @param aRefs Library references to check.
     * @return Update report.
     */
    PLM_LIB_UPDATE_REPORT CheckForUpdates( const std::vector<PLM_LIB_REFERENCE>& aRefs );

    /**
     * Start background update check.
     *
     * @param aRefs Library references to check.
     * @param aCallback Completion callback.
     */
    void CheckForUpdatesAsync( const std::vector<PLM_LIB_REFERENCE>& aRefs,
                                PLM_UPDATE_CHECK_CALLBACK aCallback );

    /**
     * Cancel background update check.
     */
    void CancelCheck();

    /**
     * Check if update check is running.
     *
     * @return True if checking.
     */
    bool IsChecking() const { return m_checking; }

    /**
     * Wait for background check to complete.
     */
    void WaitForCheck();

    /**
     * Enable/disable automatic notifications.
     *
     * @param aEnable True to enable.
     */
    void SetNotificationsEnabled( bool aEnable ) { m_notifyEnabled = aEnable; }

    /**
     * Check if notifications are enabled.
     *
     * @return True if enabled.
     */
    bool AreNotificationsEnabled() const { return m_notifyEnabled; }

    /**
     * Get last check report.
     *
     * @return Most recent update report.
     */
    const PLM_LIB_UPDATE_REPORT& GetLastReport() const { return m_lastReport; }

    /**
     * Check if there were updates in last check.
     *
     * @return True if updates available.
     */
    bool HasPendingUpdates() const { return m_lastReport.HasUpdates(); }

private:
    void runAsyncCheck( std::vector<PLM_LIB_REFERENCE> aRefs,
                         PLM_UPDATE_CHECK_CALLBACK aCallback );

    PLM_LIB_VERSION_TRACKER*       m_versionTracker;
    PLM_GET_REVISION_CALLBACK      m_getRevisionCb;
    PLM_UPDATE_NOTIFY_CALLBACK     m_notifyCb;
    PLM_LIB_UPDATE_REPORT          m_lastReport;

    std::atomic<bool>              m_checking;
    std::atomic<bool>              m_cancelRequested;
    bool                           m_notifyEnabled;
    std::unique_ptr<std::thread>   m_checkThread;
};


#endif // PLM_LIB_UPDATE_CHECKER_H
