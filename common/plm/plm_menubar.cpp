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

#include "plm_menubar.h"
#include "plm_plugin_config.h"

#include <bitmaps.h>
#include <kiplatform/ui.h>

#include <wx/intl.h>
#include <wx/log.h>


PLM_MENUBAR::PLM_MENUBAR()
{
}


PLM_MENUBAR::~PLM_MENUBAR()
{
}


wxMenu* PLM_MENUBAR::CreateMenu()
{
    wxMenu* menu = new wxMenu();

    // Connection items
    menu->Append( ID_PLM_MENU_CONNECT, _( "Connect to Server..." ),
                  _( "Connect to PLM server" ) );
    menu->Append( ID_PLM_MENU_DISCONNECT, _( "Disconnect" ),
                  _( "Disconnect from PLM server" ) );

    menu->AppendSeparator();

    // Browser and panels
    menu->Append( ID_PLM_MENU_BROWSER, _( "Open PLM Browser..." ),
                  _( "Open the PLM browser dialog" ) );
    menu->Append( ID_PLM_MENU_STATUS_PANEL, _( "PLM Status Panel" ),
                  _( "Show or hide PLM status panel" ), wxITEM_CHECK );

    menu->AppendSeparator();

    // Project operations submenu
    wxMenu* projectMenu = new wxMenu();
    projectMenu->Append( ID_PLM_MENU_CHECKOUT_PROJECT, _( "Check Out Project" ),
                         _( "Get latest project files from server" ) );
    projectMenu->Append( ID_PLM_MENU_CHECKIN_PROJECT, _( "Check In Project..." ),
                         _( "Submit project changes to server" ) );
    projectMenu->AppendSeparator();
    projectMenu->Append( ID_PLM_MENU_LOCK_PROJECT, _( "Lock Project" ),
                         _( "Lock project for exclusive editing" ) );
    projectMenu->Append( ID_PLM_MENU_UNLOCK_PROJECT, _( "Unlock Project" ),
                         _( "Release lock on project" ) );

    menu->AppendSubMenu( projectMenu, _( "Project" ) );

    menu->AppendSeparator();

    // Refresh and settings
    menu->Append( ID_PLM_MENU_REFRESH, _( "Refresh Status" ),
                  _( "Refresh PLM status for current project" ) );

    menu->AppendSeparator();

    menu->Append( ID_PLM_MENU_SETTINGS, _( "PLM Settings..." ),
                  _( "Configure PLM integration" ) );

    return menu;
}


void PLM_MENUBAR::UpdateMenuState( wxMenu* aMenu, const PLM_MENU_STATE& aState )
{
    if( !aMenu )
        return;

    // Connection items
    wxMenuItem* connectItem = aMenu->FindItem( ID_PLM_MENU_CONNECT );

    if( connectItem )
        connectItem->Enable( !aState.connected );

    wxMenuItem* disconnectItem = aMenu->FindItem( ID_PLM_MENU_DISCONNECT );

    if( disconnectItem )
        disconnectItem->Enable( aState.connected );

    // Browser requires connection
    wxMenuItem* browserItem = aMenu->FindItem( ID_PLM_MENU_BROWSER );

    if( browserItem )
        browserItem->Enable( aState.connected );

    // Project operations require connection and managed project
    wxMenuItem* checkoutItem = aMenu->FindItem( ID_PLM_MENU_CHECKOUT_PROJECT );

    if( checkoutItem )
        checkoutItem->Enable( aState.connected && aState.can_checkout );

    wxMenuItem* checkinItem = aMenu->FindItem( ID_PLM_MENU_CHECKIN_PROJECT );

    if( checkinItem )
        checkinItem->Enable( aState.connected && aState.can_checkin );

    wxMenuItem* lockItem = aMenu->FindItem( ID_PLM_MENU_LOCK_PROJECT );

    if( lockItem )
        lockItem->Enable( aState.connected && aState.can_lock );

    wxMenuItem* unlockItem = aMenu->FindItem( ID_PLM_MENU_UNLOCK_PROJECT );

    if( unlockItem )
        unlockItem->Enable( aState.connected && aState.can_unlock );

    // Refresh requires connection and managed project
    wxMenuItem* refreshItem = aMenu->FindItem( ID_PLM_MENU_REFRESH );

    if( refreshItem )
        refreshItem->Enable( aState.connected && aState.project_managed );
}


wxString PLM_MENUBAR::GetMenuTitle()
{
    return _( "&PLM" );
}


void PLM_MENUBAR::ExecuteAction( int aId )
{
    if( m_callback )
        m_callback( aId );
    else
        wxLogWarning( wxS( "PLM: No callback set for menu action %d" ), aId );
}


bool PLM_MENUBAR::ShouldShowMenu()
{
    PLM_PLUGIN_CONFIG config;

    if( !config.Load() )
        return false;

    // Show PLM menu if any plugins are configured
    return !config.GetConfiguredPluginIds().empty();
}
