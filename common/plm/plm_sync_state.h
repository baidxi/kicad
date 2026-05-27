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
 * @file plm_sync_state.h
 * @brief Sync state management for PLM-managed projects.
 *
 * This header defines the PLM_SYNC_STATE_MANAGER class which tracks
 * synchronization state between local files and the PLM server.
 */

#ifndef PLM_SYNC_STATE_H
#define PLM_SYNC_STATE_H

#include "plm_types.h"

#include <wx/string.h>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <vector>


/**
 * Synchronization state for a project.
 */
enum class PLM_SYNC_STATE
{
    IN_SYNC,            ///< Local matches server
    LOCAL_MODIFIED,     ///< Local has changes not on server
    SERVER_MODIFIED,    ///< Server has changes not local
    CONFLICT,           ///< Both local and server have changes
    UNKNOWN             ///< State cannot be determined
};


/**
 * Information about a project's sync state.
 */
struct PLM_SYNC_INFO
{
    wxString       project_path;        ///< Local project path
    PLM_SYNC_STATE state = PLM_SYNC_STATE::UNKNOWN;
    wxString       local_revision;      ///< Current local revision
    wxString       server_revision;     ///< Latest server revision
    int64_t        last_sync = 0;       ///< Last successful sync time
    int64_t        last_checked = 0;    ///< Last time server was checked
    bool           has_local_changes = false;   ///< Local modifications exist
    bool           has_server_changes = false;  ///< Server has newer revision

    PLM_SYNC_INFO() = default;

    explicit PLM_SYNC_INFO( const wxString& aPath ) : project_path( aPath ) {}

    /**
     * Check if sync is needed.
     * @return True if local or server has changes.
     */
    bool NeedsSync() const
    {
        return state == PLM_SYNC_STATE::LOCAL_MODIFIED ||
               state == PLM_SYNC_STATE::SERVER_MODIFIED ||
               state == PLM_SYNC_STATE::CONFLICT;
    }

    /**
     * Check if there's a conflict.
     * @return True if both local and server have changes.
     */
    bool HasConflict() const { return state == PLM_SYNC_STATE::CONFLICT; }

    /**
     * Check if local push is needed.
     * @return True if local has changes that need to be pushed.
     */
    bool NeedsPush() const
    {
        return state == PLM_SYNC_STATE::LOCAL_MODIFIED ||
               state == PLM_SYNC_STATE::CONFLICT;
    }

    /**
     * Check if pull from server is needed.
     * @return True if server has changes to pull.
     */
    bool NeedsPull() const
    {
        return state == PLM_SYNC_STATE::SERVER_MODIFIED ||
               state == PLM_SYNC_STATE::CONFLICT;
    }
};


/**
 * Callback type for sync state change notifications.
 *
 * @param aProjectPath Local project path.
 * @param aNewState New sync state.
 */
using PLM_SYNC_STATE_CALLBACK = std::function<void( const wxString& aProjectPath,
                                                     PLM_SYNC_STATE aNewState )>;

/**
 * Callback type for checking server revision.
 *
 * @param aProjectPath Local project path.
 * @return Latest server revision, or empty on error.
 */
using PLM_SERVER_CHECK_CALLBACK = std::function<wxString( const wxString& aProjectPath )>;


/**
 * Manages synchronization state for PLM-managed projects.
 *
 * Tracks whether local files are in sync with the server by comparing
 * revisions and detecting local modifications. Provides events for
 * state changes.
 *
 * Thread-safe: All public methods are protected by mutex.
 *
 * Usage:
 * @code
 * PLM_SYNC_STATE_MANAGER syncMgr;
 *
 * // Set callbacks
 * syncMgr.SetStateCallback( []( const wxString& path, PLM_SYNC_STATE state ) {
 *     // Update UI
 * });
 *
 * syncMgr.SetServerCheckCallback( [&plugin]( const wxString& path ) {
 *     return plugin->GetLatestRevision( path );
 * });
 *
 * // Initialize sync state
 * syncMgr.InitProject( projectPath, localRevision );
 *
 * // Notify of local modification
 * syncMgr.OnLocalModified( projectPath );
 *
 * // Check server for updates
 * syncMgr.CheckServer( projectPath );
 *
 * // Get sync state
 * PLM_SYNC_INFO info;
 * if( syncMgr.GetSyncInfo( projectPath, info ) )
 * {
 *     if( info.NeedsSync() )
 *         // Show sync button
 * }
 * @endcode
 */
class PLM_SYNC_STATE_MANAGER
{
public:
    PLM_SYNC_STATE_MANAGER();
    ~PLM_SYNC_STATE_MANAGER() = default;

    // Non-copyable
    PLM_SYNC_STATE_MANAGER( const PLM_SYNC_STATE_MANAGER& ) = delete;
    PLM_SYNC_STATE_MANAGER& operator=( const PLM_SYNC_STATE_MANAGER& ) = delete;

    //-------------------------------------------------------------------------
    // Project Management
    //-------------------------------------------------------------------------

    /**
     * Initialize sync state for a project.
     *
     * @param aProjectPath Local project path.
     * @param aLocalRevision Current local revision.
     */
    void InitProject( const wxString& aProjectPath, const wxString& aLocalRevision );

    /**
     * Remove a project from tracking.
     *
     * @param aProjectPath Local project path.
     */
    void RemoveProject( const wxString& aProjectPath );

    /**
     * Remove all projects from tracking.
     */
    void RemoveAllProjects();

    /**
     * Check if a project is being tracked.
     *
     * @param aProjectPath Local project path.
     * @return True if project is tracked.
     */
    bool IsTracking( const wxString& aProjectPath ) const;

    //-------------------------------------------------------------------------
    // State Queries
    //-------------------------------------------------------------------------

    /**
     * Get sync state for a project.
     *
     * @param aProjectPath Local project path.
     * @return Sync state, or UNKNOWN if not tracked.
     */
    PLM_SYNC_STATE GetState( const wxString& aProjectPath ) const;

    /**
     * Get full sync information for a project.
     *
     * @param aProjectPath Local project path.
     * @param aInfo Output sync information.
     * @return True if project is tracked.
     */
    bool GetSyncInfo( const wxString& aProjectPath, PLM_SYNC_INFO& aInfo ) const;

    /**
     * Get all tracked project paths.
     *
     * @return Vector of project paths.
     */
    std::vector<wxString> GetTrackedProjects() const;

    /**
     * Get projects by sync state.
     *
     * @param aState State to filter by.
     * @return Vector of project paths with the specified state.
     */
    std::vector<wxString> GetProjectsByState( PLM_SYNC_STATE aState ) const;

    /**
     * Get projects that need synchronization.
     *
     * @return Vector of project paths that need sync.
     */
    std::vector<wxString> GetProjectsNeedingSync() const;

    //-------------------------------------------------------------------------
    // State Updates
    //-------------------------------------------------------------------------

    /**
     * Notify of local modification.
     *
     * Call this when local files are modified.
     *
     * @param aProjectPath Local project path.
     */
    void OnLocalModified( const wxString& aProjectPath );

    /**
     * Notify that local changes were saved to server.
     *
     * Call this after successful checkin.
     *
     * @param aProjectPath Local project path.
     * @param aNewRevision New revision after checkin.
     */
    void OnCheckin( const wxString& aProjectPath, const wxString& aNewRevision );

    /**
     * Notify that server changes were pulled.
     *
     * Call this after successful checkout/update.
     *
     * @param aProjectPath Local project path.
     * @param aNewRevision New local revision after update.
     */
    void OnCheckout( const wxString& aProjectPath, const wxString& aNewRevision );

    /**
     * Check server for updates.
     *
     * Uses the server check callback to get latest revision.
     *
     * @param aProjectPath Local project path.
     * @return True if check was successful.
     */
    bool CheckServer( const wxString& aProjectPath );

    /**
     * Check server for all tracked projects.
     */
    void CheckAllServers();

    /**
     * Update server revision directly.
     *
     * Use this when revision is known without making a server call.
     *
     * @param aProjectPath Local project path.
     * @param aServerRevision Latest server revision.
     */
    void UpdateServerRevision( const wxString& aProjectPath, const wxString& aServerRevision );

    /**
     * Mark a conflict.
     *
     * @param aProjectPath Local project path.
     */
    void MarkConflict( const wxString& aProjectPath );

    /**
     * Clear a conflict after resolution.
     *
     * @param aProjectPath Local project path.
     * @param aNewRevision Revision after conflict resolution.
     */
    void ClearConflict( const wxString& aProjectPath, const wxString& aNewRevision );

    //-------------------------------------------------------------------------
    // Callbacks
    //-------------------------------------------------------------------------

    /**
     * Set callback for state changes.
     *
     * @param aCallback State change callback function.
     */
    void SetStateCallback( PLM_SYNC_STATE_CALLBACK aCallback );

    /**
     * Set callback for checking server revision.
     *
     * @param aCallback Server check callback function.
     */
    void SetServerCheckCallback( PLM_SERVER_CHECK_CALLBACK aCallback );

    //-------------------------------------------------------------------------
    // Utilities
    //-------------------------------------------------------------------------

    /**
     * Get display name for a sync state.
     *
     * @param aState The state.
     * @return Human-readable state name.
     */
    static wxString GetStateDisplayName( PLM_SYNC_STATE aState );

private:
    /**
     * Normalize a path for consistent keys.
     */
    static wxString NormalizePath( const wxString& aPath );

    /**
     * Fire a state change event.
     */
    void FireStateChange( const wxString& aProjectPath, PLM_SYNC_STATE aNewState );

    /**
     * Recalculate sync state from current info.
     */
    void RecalculateState( PLM_SYNC_INFO& aInfo );

    mutable std::mutex                    m_mutex;
    std::map<wxString, PLM_SYNC_INFO>     m_projects;
    PLM_SYNC_STATE_CALLBACK               m_stateCallback;
    PLM_SERVER_CHECK_CALLBACK             m_serverCheckCallback;
};


#endif // PLM_SYNC_STATE_H
