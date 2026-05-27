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
 * @file plm_context_menu.h
 * @brief PLM context menu helper for project tree integration.
 */

#ifndef PLM_CONTEXT_MENU_H
#define PLM_CONTEXT_MENU_H

#include <kicommon.h>
#include <plm/plm_types.h>
#include <plm/plm_icon_provider.h>

#include <wx/menu.h>

#include <functional>
#include <vector>


/**
 * PLM context menu action IDs.
 *
 * These IDs are used for menu items in the PLM submenu. They should be
 * added to the application's event table with appropriate handlers.
 */
enum PLM_CONTEXT_MENU_ID
{
    ID_PLM_CHECKOUT = 15000,    ///< Check out from PLM server
    ID_PLM_CHECKIN,             ///< Check in to PLM server
    ID_PLM_LOCK,                ///< Lock file for editing
    ID_PLM_UNLOCK,              ///< Release lock on file
    ID_PLM_HISTORY,             ///< View revision history
    ID_PLM_COMPARE,             ///< Compare with server version
    ID_PLM_REVERT,              ///< Revert to server version
    ID_PLM_SHOW_IN_BROWSER,     ///< Open PLM browser to this item
    ID_PLM_REFRESH,             ///< Refresh PLM status
    ID_PLM_BIND_PROJECT,        ///< Bind project to PLM
    ID_PLM_UNBIND_PROJECT,      ///< Remove PLM binding
    ID_PLM_CONFIGURE,           ///< Configure PLM settings

    ID_PLM_MENU_END             ///< End marker for menu range
};


/**
 * PLM context menu action permissions.
 *
 * Determines which actions are available based on current PLM state.
 */
struct PLM_MENU_PERMISSIONS
{
    bool can_checkout = false;      ///< Can check out from server
    bool can_checkin = false;       ///< Can check in to server
    bool can_lock = false;          ///< Can acquire lock
    bool can_unlock = false;        ///< Can release lock
    bool can_view_history = false;  ///< Can view revision history
    bool can_compare = false;       ///< Can compare versions
    bool can_revert = false;        ///< Can revert to server version
    bool can_show_browser = false;  ///< Can show in PLM browser
    bool can_refresh = true;        ///< Can refresh status
    bool can_bind = false;          ///< Can bind project to PLM
    bool can_unbind = false;        ///< Can remove PLM binding
    bool can_configure = true;      ///< Can open configuration
};


/**
 * PLM action callback signature.
 *
 * @param aFilePaths List of file paths the action applies to.
 */
using PLM_MENU_ACTION_CALLBACK = std::function<void( const std::vector<wxString>& aFilePaths )>;

/**
 * Lock state query callback signature.
 *
 * Used to query lock state from the application's lock tracker.
 *
 * @param aProjectId PLM project ID to query.
 * @return Icon state representing lock status.
 */
using PLM_LOCK_STATE_CALLBACK = std::function<PLM_ICON_STATE( const wxString& aProjectId )>;


/**
 * Helper class for creating and managing PLM context menus.
 *
 * Provides utilities to create a PLM submenu that can be added to any
 * context menu in KiCad applications. The menu items are automatically
 * enabled/disabled based on the current PLM state and user permissions.
 */
class KICOMMON_API PLM_CONTEXT_MENU
{
public:
    PLM_CONTEXT_MENU();
    ~PLM_CONTEXT_MENU();

    /**
     * Create a PLM submenu for the given file selection.
     *
     * Creates a menu with appropriate items enabled based on the files'
     * PLM status and the user's permissions.
     *
     * @param aParentMenu Parent menu to append submenu to.
     * @param aFilePaths Selected file paths.
     * @param aProjectPath Current project path.
     * @return Pointer to the created submenu, owned by parent.
     */
    wxMenu* CreateSubmenu( wxMenu* aParentMenu,
                           const std::vector<wxString>& aFilePaths,
                           const wxString& aProjectPath );

    /**
     * Get the PLM submenu for project-level actions.
     *
     * Creates a menu with project-level PLM actions like bind/unbind.
     *
     * @param aParentMenu Parent menu to append submenu to.
     * @param aProjectPath Current project path.
     * @return Pointer to the created submenu, owned by parent.
     */
    wxMenu* CreateProjectSubmenu( wxMenu* aParentMenu, const wxString& aProjectPath );

    /**
     * Calculate permissions for the given file selection.
     *
     * Queries PLM state and user permissions to determine which actions
     * are available for the selected files.
     *
     * @param aFilePaths Selected file paths.
     * @param aProjectPath Current project path.
     * @return Permissions structure.
     */
    PLM_MENU_PERMISSIONS GetPermissions( const std::vector<wxString>& aFilePaths,
                                          const wxString& aProjectPath ) const;

    /**
     * Check if a project is bound to PLM.
     *
     * @param aProjectPath Path to the project.
     * @return True if project is PLM-managed.
     */
    bool IsProjectManaged( const wxString& aProjectPath ) const;

    /**
     * Check if a file is tracked by PLM.
     *
     * @param aFilePath Path to the file.
     * @param aProjectPath Path to the project.
     * @return True if file is PLM-tracked.
     */
    bool IsFileManaged( const wxString& aFilePath, const wxString& aProjectPath ) const;

    /**
     * Get the PLM state for a file.
     *
     * @param aFilePath Path to the file.
     * @param aProjectPath Path to the project.
     * @return PLM icon state representing file status.
     */
    PLM_ICON_STATE GetFileState( const wxString& aFilePath, const wxString& aProjectPath ) const;

    /**
     * Set callback for checkout action.
     */
    void SetCheckoutCallback( PLM_MENU_ACTION_CALLBACK aCallback ) { m_checkoutCb = aCallback; }

    /**
     * Set callback for checkin action.
     */
    void SetCheckinCallback( PLM_MENU_ACTION_CALLBACK aCallback ) { m_checkinCb = aCallback; }

    /**
     * Set callback for lock action.
     */
    void SetLockCallback( PLM_MENU_ACTION_CALLBACK aCallback ) { m_lockCb = aCallback; }

    /**
     * Set callback for unlock action.
     */
    void SetUnlockCallback( PLM_MENU_ACTION_CALLBACK aCallback ) { m_unlockCb = aCallback; }

    /**
     * Set callback for history action.
     */
    void SetHistoryCallback( PLM_MENU_ACTION_CALLBACK aCallback ) { m_historyCb = aCallback; }

    /**
     * Set callback for compare action.
     */
    void SetCompareCallback( PLM_MENU_ACTION_CALLBACK aCallback ) { m_compareCb = aCallback; }

    /**
     * Set callback for revert action.
     */
    void SetRevertCallback( PLM_MENU_ACTION_CALLBACK aCallback ) { m_revertCb = aCallback; }

    /**
     * Set callback for show in browser action.
     */
    void SetShowBrowserCallback( PLM_MENU_ACTION_CALLBACK aCallback ) { m_browserCb = aCallback; }

    /**
     * Set callback for refresh action.
     */
    void SetRefreshCallback( PLM_MENU_ACTION_CALLBACK aCallback ) { m_refreshCb = aCallback; }

    /**
     * Set callback for bind project action.
     */
    void SetBindCallback( PLM_MENU_ACTION_CALLBACK aCallback ) { m_bindCb = aCallback; }

    /**
     * Set callback for unbind project action.
     */
    void SetUnbindCallback( PLM_MENU_ACTION_CALLBACK aCallback ) { m_unbindCb = aCallback; }

    /**
     * Set callback for configure action.
     */
    void SetConfigureCallback( PLM_MENU_ACTION_CALLBACK aCallback ) { m_configureCb = aCallback; }

    /**
     * Set callback for querying lock state.
     *
     * The callback should query the application's PLM_LOCK_TRACKER and return
     * the appropriate icon state for the given project ID.
     */
    void SetLockStateCallback( PLM_LOCK_STATE_CALLBACK aCallback ) { m_lockStateCb = aCallback; }

    /**
     * Execute an action by menu ID.
     *
     * @param aId Menu item ID.
     * @param aFilePaths Files to operate on.
     */
    void ExecuteAction( int aId, const std::vector<wxString>& aFilePaths );

private:
    void addMenuItem( wxMenu* aMenu, int aId, const wxString& aLabel,
                      const wxString& aHelp, bool aEnabled );

    PLM_MENU_ACTION_CALLBACK m_checkoutCb;
    PLM_MENU_ACTION_CALLBACK m_checkinCb;
    PLM_MENU_ACTION_CALLBACK m_lockCb;
    PLM_MENU_ACTION_CALLBACK m_unlockCb;
    PLM_MENU_ACTION_CALLBACK m_historyCb;
    PLM_MENU_ACTION_CALLBACK m_compareCb;
    PLM_MENU_ACTION_CALLBACK m_revertCb;
    PLM_MENU_ACTION_CALLBACK m_browserCb;
    PLM_MENU_ACTION_CALLBACK m_refreshCb;
    PLM_MENU_ACTION_CALLBACK m_bindCb;
    PLM_MENU_ACTION_CALLBACK m_unbindCb;
    PLM_MENU_ACTION_CALLBACK m_configureCb;
    PLM_LOCK_STATE_CALLBACK  m_lockStateCb;
};


#endif // PLM_CONTEXT_MENU_H
