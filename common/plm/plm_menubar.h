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
 * @file plm_menubar.h
 * @brief PLM menu bar integration helper.
 */

#ifndef PLM_MENUBAR_H
#define PLM_MENUBAR_H

#include <kicommon.h>

#include <wx/menu.h>

#include <functional>


/**
 * PLM menu bar item IDs.
 */
enum PLM_MENU_ID
{
    ID_PLM_MENU_CONNECT = 15100,        ///< Connect to PLM server
    ID_PLM_MENU_DISCONNECT,             ///< Disconnect from PLM server
    ID_PLM_MENU_BROWSER,                ///< Open PLM browser dialog
    ID_PLM_MENU_STATUS_PANEL,           ///< Toggle PLM status panel
    ID_PLM_MENU_CHECKOUT_PROJECT,       ///< Check out entire project
    ID_PLM_MENU_CHECKIN_PROJECT,        ///< Check in entire project
    ID_PLM_MENU_LOCK_PROJECT,           ///< Lock project for editing
    ID_PLM_MENU_UNLOCK_PROJECT,         ///< Unlock project
    ID_PLM_MENU_REFRESH,                ///< Refresh PLM status
    ID_PLM_MENU_SETTINGS,               ///< Open PLM settings

    ID_PLM_MENU_END                     ///< End marker
};


/**
 * PLM menu callback type.
 */
using PLM_MENU_CALLBACK = std::function<void( int aId )>;


/**
 * PLM connection state for menu enablement.
 */
struct PLM_MENU_STATE
{
    bool connected = false;         ///< Connected to PLM server
    bool project_managed = false;   ///< Current project is PLM-managed
    bool can_checkout = false;      ///< Can check out project
    bool can_checkin = false;       ///< Can check in project
    bool can_lock = false;          ///< Can lock project
    bool can_unlock = false;        ///< Can unlock project
};


/**
 * Helper class for creating PLM menu bar entries.
 *
 * Creates a "PLM" top-level menu with standard PLM operations.
 */
class KICOMMON_API PLM_MENUBAR
{
public:
    PLM_MENUBAR();
    ~PLM_MENUBAR();

    /**
     * Create the PLM menu.
     *
     * @return New menu owned by caller.
     */
    wxMenu* CreateMenu();

    /**
     * Update menu item states based on current PLM status.
     *
     * @param aMenu Menu to update.
     * @param aState Current PLM state.
     */
    void UpdateMenuState( wxMenu* aMenu, const PLM_MENU_STATE& aState );

    /**
     * Get the PLM menu title.
     *
     * @return Localized menu title.
     */
    static wxString GetMenuTitle();

    /**
     * Set callback for menu actions.
     *
     * The callback receives the menu item ID.
     */
    void SetCallback( PLM_MENU_CALLBACK aCallback ) { m_callback = aCallback; }

    /**
     * Execute a menu action by ID.
     *
     * @param aId Menu item ID.
     */
    void ExecuteAction( int aId );

    /**
     * Check if PLM menu should be shown.
     *
     * PLM menu is shown if PLM integration is enabled in settings.
     *
     * @return True if menu should be shown.
     */
    static bool ShouldShowMenu();

private:
    PLM_MENU_CALLBACK m_callback;
};


#endif // PLM_MENUBAR_H
