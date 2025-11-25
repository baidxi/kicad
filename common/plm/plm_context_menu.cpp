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

#include "plm_context_menu.h"
#include "plm_project_binding.h"
#include "plm_dirty_tracker.h"
#include "plm_lock_tracker.h"

#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/log.h>


PLM_CONTEXT_MENU::PLM_CONTEXT_MENU()
{
}


PLM_CONTEXT_MENU::~PLM_CONTEXT_MENU()
{
}


wxMenu* PLM_CONTEXT_MENU::CreateSubmenu( wxMenu* aParentMenu,
                                          const std::vector<wxString>& aFilePaths,
                                          const wxString& aProjectPath )
{
    if( aFilePaths.empty() )
        return nullptr;

    PLM_MENU_PERMISSIONS perms = GetPermissions( aFilePaths, aProjectPath );
    wxMenu* submenu = new wxMenu();

    // File operations
    addMenuItem( submenu, ID_PLM_CHECKOUT, _( "Check Out" ),
                 _( "Get latest version from PLM server" ), perms.can_checkout );

    addMenuItem( submenu, ID_PLM_CHECKIN, _( "Check In..." ),
                 _( "Submit changes to PLM server" ), perms.can_checkin );

    submenu->AppendSeparator();

    addMenuItem( submenu, ID_PLM_LOCK, _( "Lock" ),
                 _( "Lock file for exclusive editing" ), perms.can_lock );

    addMenuItem( submenu, ID_PLM_UNLOCK, _( "Unlock" ),
                 _( "Release lock on file" ), perms.can_unlock );

    submenu->AppendSeparator();

    addMenuItem( submenu, ID_PLM_HISTORY, _( "View History..." ),
                 _( "Show revision history" ), perms.can_view_history );

    addMenuItem( submenu, ID_PLM_COMPARE, _( "Compare with Server..." ),
                 _( "Compare local version with server" ), perms.can_compare );

    addMenuItem( submenu, ID_PLM_REVERT, _( "Revert to Server Version" ),
                 _( "Discard local changes and restore server version" ), perms.can_revert );

    submenu->AppendSeparator();

    addMenuItem( submenu, ID_PLM_SHOW_IN_BROWSER, _( "Show in PLM Browser" ),
                 _( "Open PLM browser to this item" ), perms.can_show_browser );

    addMenuItem( submenu, ID_PLM_REFRESH, _( "Refresh Status" ),
                 _( "Refresh PLM status" ), perms.can_refresh );

    aParentMenu->AppendSeparator();
    aParentMenu->AppendSubMenu( submenu, _( "PLM" ) );

    return submenu;
}


wxMenu* PLM_CONTEXT_MENU::CreateProjectSubmenu( wxMenu* aParentMenu,
                                                 const wxString& aProjectPath )
{
    wxMenu* submenu = new wxMenu();
    bool isManaged = IsProjectManaged( aProjectPath );

    addMenuItem( submenu, ID_PLM_BIND_PROJECT, _( "Bind to PLM..." ),
                 _( "Connect this project to a PLM system" ), !isManaged );

    addMenuItem( submenu, ID_PLM_UNBIND_PROJECT, _( "Remove PLM Binding" ),
                 _( "Disconnect this project from PLM" ), isManaged );

    submenu->AppendSeparator();

    addMenuItem( submenu, ID_PLM_CONFIGURE, _( "PLM Settings..." ),
                 _( "Configure PLM integration" ), true );

    aParentMenu->AppendSeparator();
    aParentMenu->AppendSubMenu( submenu, _( "PLM" ) );

    return submenu;
}


PLM_MENU_PERMISSIONS PLM_CONTEXT_MENU::GetPermissions( const std::vector<wxString>& aFilePaths,
                                                        const wxString& aProjectPath ) const
{
    PLM_MENU_PERMISSIONS perms;

    if( aProjectPath.IsEmpty() || aFilePaths.empty() )
        return perms;

    bool projectManaged = IsProjectManaged( aProjectPath );

    if( !projectManaged )
    {
        perms.can_bind = true;
        return perms;
    }

    perms.can_unbind = true;
    perms.can_show_browser = true;
    perms.can_view_history = true;

    // Check each file's state to determine permissions
    bool anyLocked = false;
    bool anyLockedByMe = false;
    bool anyModified = false;
    bool allManaged = true;

    for( const wxString& filePath : aFilePaths )
    {
        if( !IsFileManaged( filePath, aProjectPath ) )
        {
            allManaged = false;
            continue;
        }

        PLM_ICON_STATE state = GetFileState( filePath, aProjectPath );

        switch( state )
        {
        case PLM_ICON_STATE::LOCKED_BY_ME:
            anyLockedByMe = true;
            anyLocked = true;
            break;

        case PLM_ICON_STATE::LOCKED_BY_OTHER:
            anyLocked = true;
            break;

        case PLM_ICON_STATE::MODIFIED:
            anyModified = true;
            break;

        default:
            break;
        }
    }

    if( allManaged && !aFilePaths.empty() )
    {
        // Can checkout if nothing is locked by us
        perms.can_checkout = !anyLockedByMe;

        // Can checkin if we have locks or modifications
        perms.can_checkin = anyLockedByMe || anyModified;

        // Can lock if nothing is locked
        perms.can_lock = !anyLocked;

        // Can unlock if we have locks
        perms.can_unlock = anyLockedByMe;

        // Can compare and revert if managed
        perms.can_compare = true;
        perms.can_revert = anyModified;
    }

    return perms;
}


bool PLM_CONTEXT_MENU::IsProjectManaged( const wxString& aProjectPath ) const
{
    if( aProjectPath.IsEmpty() )
        return false;

    PLM_PROJECT_BINDING_MANAGER& mgr = PLM_PROJECT_BINDING_MANAGER::GetInstance();
    return mgr.HasBinding( aProjectPath );
}


bool PLM_CONTEXT_MENU::IsFileManaged( const wxString& aFilePath,
                                       const wxString& aProjectPath ) const
{
    if( aFilePath.IsEmpty() || aProjectPath.IsEmpty() )
        return false;

    // A file is managed if the project is PLM-bound and the file is under the project
    if( !IsProjectManaged( aProjectPath ) )
        return false;

    // Check if file is under project directory
    wxFileName fn( aFilePath );
    wxFileName projFn( aProjectPath );

    wxString fileDir = fn.GetPath();
    wxString projDir = projFn.GetPath();

    return fileDir.StartsWith( projDir );
}


PLM_ICON_STATE PLM_CONTEXT_MENU::GetFileState( const wxString& aFilePath,
                                                const wxString& aProjectPath ) const
{
    if( !IsFileManaged( aFilePath, aProjectPath ) )
        return PLM_ICON_STATE::UNMANAGED;

    // Check dirty tracker for local modifications
    PLM_DIRTY_TRACKER& tracker = PLM_DIRTY_TRACKER::GetInstance();

    if( tracker.IsModified( aFilePath ) )
        return PLM_ICON_STATE::MODIFIED;

    // Query lock status through callback if available
    if( m_lockStateCb )
    {
        PLM_PROJECT_BINDING_MANAGER& mgr = PLM_PROJECT_BINDING_MANAGER::GetInstance();
        PLM_PROJECT_BINDING binding;

        if( mgr.GetBinding( aProjectPath, binding ) )
        {
            PLM_ICON_STATE lockState = m_lockStateCb( binding.project_id );

            if( lockState == PLM_ICON_STATE::LOCKED_BY_ME ||
                lockState == PLM_ICON_STATE::LOCKED_BY_OTHER )
            {
                return lockState;
            }
        }
    }

    return PLM_ICON_STATE::CLEAN;
}


void PLM_CONTEXT_MENU::ExecuteAction( int aId, const std::vector<wxString>& aFilePaths )
{
    switch( aId )
    {
    case ID_PLM_CHECKOUT:
        if( m_checkoutCb )
            m_checkoutCb( aFilePaths );
        break;

    case ID_PLM_CHECKIN:
        if( m_checkinCb )
            m_checkinCb( aFilePaths );
        break;

    case ID_PLM_LOCK:
        if( m_lockCb )
            m_lockCb( aFilePaths );
        break;

    case ID_PLM_UNLOCK:
        if( m_unlockCb )
            m_unlockCb( aFilePaths );
        break;

    case ID_PLM_HISTORY:
        if( m_historyCb )
            m_historyCb( aFilePaths );
        break;

    case ID_PLM_COMPARE:
        if( m_compareCb )
            m_compareCb( aFilePaths );
        break;

    case ID_PLM_REVERT:
        if( m_revertCb )
            m_revertCb( aFilePaths );
        break;

    case ID_PLM_SHOW_IN_BROWSER:
        if( m_browserCb )
            m_browserCb( aFilePaths );
        break;

    case ID_PLM_REFRESH:
        if( m_refreshCb )
            m_refreshCb( aFilePaths );
        break;

    case ID_PLM_BIND_PROJECT:
        if( m_bindCb )
            m_bindCb( aFilePaths );
        break;

    case ID_PLM_UNBIND_PROJECT:
        if( m_unbindCb )
            m_unbindCb( aFilePaths );
        break;

    case ID_PLM_CONFIGURE:
        if( m_configureCb )
            m_configureCb( aFilePaths );
        break;

    default:
        wxLogWarning( wxS( "PLM: Unknown menu action ID %d" ), aId );
        break;
    }
}


void PLM_CONTEXT_MENU::addMenuItem( wxMenu* aMenu, int aId, const wxString& aLabel,
                                     const wxString& aHelp, bool aEnabled )
{
    wxMenuItem* item = aMenu->Append( aId, aLabel, aHelp );
    item->Enable( aEnabled );
}
