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
 * @file project_tree_plm.h
 * @brief PLM integration helper for project tree pane.
 *
 * Provides PLM status tracking and icon updates for files in the
 * project tree. Subscribes to PLM events and updates tree item
 * status accordingly.
 */

#ifndef PROJECT_TREE_PLM_H
#define PROJECT_TREE_PLM_H

#include <plm/plm_event_bus.h>
#include <plm/plm_icon_provider.h>
#include <plm/plm_project_binding.h>
#include <plm/plm_dirty_tracker.h>

#include <wx/string.h>
#include <wx/treectrl.h>
#include <map>
#include <mutex>


class PROJECT_TREE;
class PROJECT_TREE_PANE;


/**
 * PLM status information for a tree item.
 */
struct PROJECT_TREE_PLM_STATUS
{
    wxString        file_path;
    PLM_ICON_STATE  icon_state = PLM_ICON_STATE::UNMANAGED;
    wxString        tooltip;
    bool            needs_attention = false;

    PROJECT_TREE_PLM_STATUS() = default;

    explicit PROJECT_TREE_PLM_STATUS( const wxString& aPath ) : file_path( aPath ) {}
};


/**
 * Integrates PLM status into the project tree.
 *
 * Tracks PLM status for files in the tree and provides icons and tooltips.
 * Subscribes to PLM events and updates status automatically.
 *
 * Usage:
 * @code
 * PROJECT_TREE_PLM plmIntegration( treePane, tree );
 *
 * // Initialize when project opens
 * plmIntegration.OnProjectOpened( projectPath );
 *
 * // Update icons after tree rebuild
 * plmIntegration.UpdateAllIcons();
 *
 * // Get status for a file
 * PLM_ICON_STATE state = plmIntegration.GetFileStatus( filePath );
 *
 * // Cleanup on project close
 * plmIntegration.OnProjectClosed();
 * @endcode
 */
class PROJECT_TREE_PLM
{
public:
    PROJECT_TREE_PLM( PROJECT_TREE_PANE* aTreePane, PROJECT_TREE* aTree );
    ~PROJECT_TREE_PLM();

    // Non-copyable
    PROJECT_TREE_PLM( const PROJECT_TREE_PLM& ) = delete;
    PROJECT_TREE_PLM& operator=( const PROJECT_TREE_PLM& ) = delete;

    //-------------------------------------------------------------------------
    // Lifecycle
    //-------------------------------------------------------------------------

    /**
     * Called when a project is opened.
     *
     * Loads PLM binding and initializes status tracking.
     *
     * @param aProjectPath Path to the project file.
     */
    void OnProjectOpened( const wxString& aProjectPath );

    /**
     * Called when the project is closed.
     *
     * Clears status tracking and unsubscribes from events.
     */
    void OnProjectClosed();

    /**
     * Check if PLM is enabled for the current project.
     *
     * @return True if project is bound to PLM.
     */
    bool IsEnabled() const;

    //-------------------------------------------------------------------------
    // Status Queries
    //-------------------------------------------------------------------------

    /**
     * Get PLM icon state for a file.
     *
     * @param aFilePath Path to the file.
     * @return Icon state for the file.
     */
    PLM_ICON_STATE GetFileStatus( const wxString& aFilePath ) const;

    /**
     * Get full status information for a file.
     *
     * @param aFilePath Path to the file.
     * @param aStatus Output status information.
     * @return True if status found.
     */
    bool GetFileStatusInfo( const wxString& aFilePath, PROJECT_TREE_PLM_STATUS& aStatus ) const;

    /**
     * Get tooltip text for a file's PLM status.
     *
     * @param aFilePath Path to the file.
     * @return Tooltip text, or empty if not managed.
     */
    wxString GetStatusTooltip( const wxString& aFilePath ) const;

    /**
     * Check if any files need attention.
     *
     * @return True if any files have conflicts, are locked by others, etc.
     */
    bool HasFilesNeedingAttention() const;

    /**
     * Get list of files needing attention.
     *
     * @return Vector of file paths that need attention.
     */
    std::vector<wxString> GetFilesNeedingAttention() const;

    //-------------------------------------------------------------------------
    // Icon Updates
    //-------------------------------------------------------------------------

    /**
     * Update PLM status icons for all tree items.
     *
     * Call after tree rebuild or PLM state changes.
     */
    void UpdateAllIcons();

    /**
     * Update PLM status icon for a specific file.
     *
     * @param aFilePath Path to the file.
     */
    void UpdateFileIcon( const wxString& aFilePath );

    /**
     * Refresh status from PLM system.
     *
     * Queries PLM for current status and updates icons.
     */
    void RefreshStatus();

    //-------------------------------------------------------------------------
    // Event Handlers
    //-------------------------------------------------------------------------

    /**
     * Handle file saved notification.
     *
     * @param aFilePath Path to the saved file.
     */
    void OnFileSaved( const wxString& aFilePath );

    /**
     * Handle file deleted notification.
     *
     * @param aFilePath Path to the deleted file.
     */
    void OnFileDeleted( const wxString& aFilePath );

private:
    /**
     * Subscribe to PLM events.
     */
    void subscribeToEvents();

    /**
     * Unsubscribe from PLM events.
     */
    void unsubscribeFromEvents();

    /**
     * Handle PLM event.
     */
    void onPlmEvent( const PLM_EVENT& aEvent );

    /**
     * Update status for a file based on current state.
     */
    void updateFileStatus( const wxString& aFilePath );

    /**
     * Find tree item for a file path.
     */
    wxTreeItemId findTreeItem( const wxString& aFilePath ) const;

    /**
     * Set item tooltip in tree.
     */
    void setItemTooltip( wxTreeItemId aItem, const wxString& aTooltip );

    /**
     * Normalize path for map keys.
     */
    static wxString normalizePath( const wxString& aPath );

    PROJECT_TREE*                                 m_tree;
    wxString                                      m_projectPath;
    bool                                          m_enabled;
    PLM_SUBSCRIPTION_ID                           m_subscriptionId;

    mutable std::mutex                            m_mutex;
    std::map<wxString, PROJECT_TREE_PLM_STATUS>   m_fileStatus;
};


#endif // PROJECT_TREE_PLM_H
