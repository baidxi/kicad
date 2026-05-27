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

#include "project_tree_plm.h"
#include "project_tree.h"
#include "project_tree_pane.h"
#include "project_tree_item.h"

#include <wx/filename.h>
#include <wx/log.h>

#include <stack>


PROJECT_TREE_PLM::PROJECT_TREE_PLM( PROJECT_TREE_PANE* /* aTreePane */, PROJECT_TREE* aTree ) :
        m_tree( aTree ),
        m_enabled( false ),
        m_subscriptionId( 0 )
{
}


PROJECT_TREE_PLM::~PROJECT_TREE_PLM()
{
    OnProjectClosed();
}


void PROJECT_TREE_PLM::OnProjectOpened( const wxString& aProjectPath )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    m_projectPath = aProjectPath;
    m_fileStatus.clear();

    PLM_PROJECT_BINDING binding;
    PLM_PROJECT_BINDING_MANAGER& bindingMgr = PLM_PROJECT_BINDING_MANAGER::GetInstance();

    if( bindingMgr.GetBinding( aProjectPath, binding ) && binding.IsValid() )
    {
        m_enabled = true;
        subscribeToEvents();
    }
    else
    {
        m_enabled = false;
    }
}


void PROJECT_TREE_PLM::OnProjectClosed()
{
    std::lock_guard<std::mutex> lock( m_mutex );

    unsubscribeFromEvents();
    m_enabled = false;
    m_projectPath.Clear();
    m_fileStatus.clear();
}


bool PROJECT_TREE_PLM::IsEnabled() const
{
    std::lock_guard<std::mutex> lock( m_mutex );
    return m_enabled;
}


PLM_ICON_STATE PROJECT_TREE_PLM::GetFileStatus( const wxString& aFilePath ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    if( !m_enabled )
        return PLM_ICON_STATE::UNMANAGED;

    wxString normalized = normalizePath( aFilePath );
    auto it = m_fileStatus.find( normalized );

    if( it != m_fileStatus.end() )
        return it->second.icon_state;

    return PLM_ICON_STATE::UNMANAGED;
}


bool PROJECT_TREE_PLM::GetFileStatusInfo( const wxString& aFilePath,
                                          PROJECT_TREE_PLM_STATUS& aStatus ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    if( !m_enabled )
        return false;

    wxString normalized = normalizePath( aFilePath );
    auto it = m_fileStatus.find( normalized );

    if( it != m_fileStatus.end() )
    {
        aStatus = it->second;
        return true;
    }

    return false;
}


wxString PROJECT_TREE_PLM::GetStatusTooltip( const wxString& aFilePath ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    if( !m_enabled )
        return wxEmptyString;

    wxString normalized = normalizePath( aFilePath );
    auto it = m_fileStatus.find( normalized );

    if( it != m_fileStatus.end() )
        return it->second.tooltip;

    return wxEmptyString;
}


bool PROJECT_TREE_PLM::HasFilesNeedingAttention() const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    for( const auto& [path, status] : m_fileStatus )
    {
        if( status.needs_attention )
            return true;
    }

    return false;
}


std::vector<wxString> PROJECT_TREE_PLM::GetFilesNeedingAttention() const
{
    std::lock_guard<std::mutex> lock( m_mutex );
    std::vector<wxString> result;

    for( const auto& [path, status] : m_fileStatus )
    {
        if( status.needs_attention )
            result.push_back( status.file_path );
    }

    return result;
}


void PROJECT_TREE_PLM::UpdateAllIcons()
{
    if( !m_tree || !m_enabled )
        return;

    std::lock_guard<std::mutex> lock( m_mutex );

    wxTreeItemId root = m_tree->GetRootItem();

    if( !root.IsOk() )
        return;

    std::stack<wxTreeItemId> items;
    items.push( root );

    while( !items.empty() )
    {
        wxTreeItemId current = items.top();
        items.pop();

        PROJECT_TREE_ITEM* itemData =
                dynamic_cast<PROJECT_TREE_ITEM*>( m_tree->GetItemData( current ) );

        if( itemData )
        {
            wxString filePath = itemData->GetFileName();
            wxString normalized = normalizePath( filePath );

            auto it = m_fileStatus.find( normalized );

            if( it != m_fileStatus.end() )
            {
                setItemTooltip( current, it->second.tooltip );
            }
        }

        if( m_tree->ItemHasChildren( current ) )
        {
            wxTreeItemIdValue cookie;
            wxTreeItemId child = m_tree->GetFirstChild( current, cookie );

            while( child.IsOk() )
            {
                items.push( child );
                child = m_tree->GetNextChild( current, cookie );
            }
        }
    }
}


void PROJECT_TREE_PLM::UpdateFileIcon( const wxString& aFilePath )
{
    if( !m_tree || !m_enabled )
        return;

    wxTreeItemId item = findTreeItem( aFilePath );

    if( item.IsOk() )
    {
        std::lock_guard<std::mutex> lock( m_mutex );

        wxString normalized = normalizePath( aFilePath );
        auto it = m_fileStatus.find( normalized );

        if( it != m_fileStatus.end() )
        {
            setItemTooltip( item, it->second.tooltip );
        }
    }
}


void PROJECT_TREE_PLM::RefreshStatus()
{
    if( !m_enabled )
        return;

    // TODO: Query PLM system for current status of all tracked files
    // For now, just update icons based on cached status
    UpdateAllIcons();
}


void PROJECT_TREE_PLM::OnFileSaved( const wxString& aFilePath )
{
    if( !m_enabled )
        return;

    updateFileStatus( aFilePath );
    UpdateFileIcon( aFilePath );
}


void PROJECT_TREE_PLM::OnFileDeleted( const wxString& aFilePath )
{
    if( !m_enabled )
        return;

    {
        std::lock_guard<std::mutex> lock( m_mutex );
        wxString normalized = normalizePath( aFilePath );
        m_fileStatus.erase( normalized );
    }
}


void PROJECT_TREE_PLM::subscribeToEvents()
{
    if( m_subscriptionId != 0 )
        return;

    PLM_EVENT_FILTER filter = PLM_EVENT_FILTER::StateEvents();
    filter.project_id = m_projectPath;

    m_subscriptionId = PLM_EVENT_BUS::GetInstance().Subscribe(
            [this]( const PLM_EVENT& event )
            {
                onPlmEvent( event );
            },
            filter );
}


void PROJECT_TREE_PLM::unsubscribeFromEvents()
{
    if( m_subscriptionId != 0 )
    {
        PLM_EVENT_BUS::GetInstance().Unsubscribe( m_subscriptionId );
        m_subscriptionId = 0;
    }
}


void PROJECT_TREE_PLM::onPlmEvent( const PLM_EVENT& aEvent )
{
    switch( aEvent.type )
    {
    case PLM_EVENT_TYPE::STATE_CHANGED:
    case PLM_EVENT_TYPE::STATE_SYNC_COMPLETED:
    case PLM_EVENT_TYPE::PROJECT_DIRTY:
    case PLM_EVENT_TYPE::PROJECT_CLEAN:
        RefreshStatus();
        break;

    case PLM_EVENT_TYPE::LOCK_ACQUIRED:
    case PLM_EVENT_TYPE::LOCK_RELEASED:
    case PLM_EVENT_TYPE::LOCK_STOLEN:
    case PLM_EVENT_TYPE::LOCK_EXPIRED:
        RefreshStatus();
        break;

    default:
        break;
    }
}


void PROJECT_TREE_PLM::updateFileStatus( const wxString& aFilePath )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString normalized = normalizePath( aFilePath );
    PROJECT_TREE_PLM_STATUS status( aFilePath );

    // Get status from dirty tracker
    PLM_DIRTY_TRACKER& tracker = PLM_DIRTY_TRACKER::GetInstance();
    PLM_DIRTY_STATE dirtyState = tracker.GetState( aFilePath );

    switch( dirtyState )
    {
    case PLM_DIRTY_STATE::CLEAN:
        status.icon_state = PLM_ICON_STATE::CLEAN;
        status.tooltip = PLM_ICON_PROVIDER::GetStateTooltip( PLM_ICON_STATE::CLEAN );
        break;

    case PLM_DIRTY_STATE::MODIFIED:
        status.icon_state = PLM_ICON_STATE::MODIFIED;
        status.tooltip = PLM_ICON_PROVIDER::GetStateTooltip( PLM_ICON_STATE::MODIFIED );
        break;

    case PLM_DIRTY_STATE::NEW:
        status.icon_state = PLM_ICON_STATE::PENDING_UPLOAD;
        status.tooltip = PLM_ICON_PROVIDER::GetStateTooltip( PLM_ICON_STATE::PENDING_UPLOAD );
        break;

    case PLM_DIRTY_STATE::DELETED:
        status.icon_state = PLM_ICON_STATE::MODIFIED;
        status.tooltip = _( "File deleted locally" );
        break;

    case PLM_DIRTY_STATE::CONFLICT:
        status.icon_state = PLM_ICON_STATE::CONFLICT;
        status.tooltip = PLM_ICON_PROVIDER::GetStateTooltip( PLM_ICON_STATE::CONFLICT );
        status.needs_attention = true;
        break;

    default:
        status.icon_state = PLM_ICON_STATE::UNMANAGED;
        status.tooltip = wxEmptyString;
        break;
    }

    status.needs_attention = PLM_ICON_PROVIDER::StateNeedsAttention( status.icon_state );
    m_fileStatus[normalized] = status;
}


wxTreeItemId PROJECT_TREE_PLM::findTreeItem( const wxString& aFilePath ) const
{
    if( !m_tree )
        return wxTreeItemId();

    wxTreeItemId root = m_tree->GetRootItem();

    if( !root.IsOk() )
        return wxTreeItemId();

    wxString targetPath = normalizePath( aFilePath );

    std::stack<wxTreeItemId> items;
    items.push( root );

    while( !items.empty() )
    {
        wxTreeItemId current = items.top();
        items.pop();

        PROJECT_TREE_ITEM* itemData =
                dynamic_cast<PROJECT_TREE_ITEM*>( m_tree->GetItemData( current ) );

        if( itemData )
        {
            wxString itemPath = normalizePath( itemData->GetFileName() );

            if( itemPath == targetPath )
                return current;
        }

        if( m_tree->ItemHasChildren( current ) )
        {
            wxTreeItemIdValue cookie;
            wxTreeItemId child = m_tree->GetFirstChild( current, cookie );

            while( child.IsOk() )
            {
                items.push( child );
                child = m_tree->GetNextChild( current, cookie );
            }
        }
    }

    return wxTreeItemId();
}


void PROJECT_TREE_PLM::setItemTooltip( wxTreeItemId aItem, const wxString& aTooltip )
{
    // wxTreeCtrl doesn't have per-item tooltips in the base class
    // The tooltip would be shown via EVT_TREE_ITEM_GETTOOLTIP event handler
    // For now, we store the tooltip in our status map and it can be retrieved
    // when needed for display
}


wxString PROJECT_TREE_PLM::normalizePath( const wxString& aPath )
{
    wxFileName fn( aPath );
    fn.Normalize( wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE );
    return fn.GetFullPath();
}
