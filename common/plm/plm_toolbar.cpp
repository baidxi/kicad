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

#include "plm_toolbar.h"
#include "plm_plugin_config.h"

#include <bitmaps.h>

#include <wx/intl.h>
#include <wx/log.h>


PLM_TOOLBAR::PLM_TOOLBAR()
{
}


PLM_TOOLBAR::~PLM_TOOLBAR()
{
}


void PLM_TOOLBAR::AddButtons( wxToolBar* aToolBar )
{
    if( !aToolBar )
        return;

    aToolBar->AddSeparator();

    // PLM Browser button
    aToolBar->AddTool( ID_PLM_TB_BROWSER, wxEmptyString,
                       KiBitmap( BITMAPS::directory_browser ),
                       _( "Open PLM Browser" ) );

    // Connect/Disconnect toggle
    aToolBar->AddTool( ID_PLM_TB_CONNECT, wxEmptyString,
                       KiBitmap( BITMAPS::net_highlight ),
                       _( "Connect to PLM Server" ) );

    // Sync button
    aToolBar->AddTool( ID_PLM_TB_SYNC, wxEmptyString,
                       KiBitmap( BITMAPS::refresh ),
                       _( "Synchronize with PLM Server" ) );
}


void PLM_TOOLBAR::UpdateState( wxToolBar* aToolBar, const PLM_TOOLBAR_STATE& aState )
{
    if( !aToolBar )
        return;

    // Update browser button - enabled when connected
    aToolBar->EnableTool( ID_PLM_TB_BROWSER, aState.connected );

    // Update connect button tooltip based on state
    wxString connectTip = aState.connected ? _( "Disconnect from PLM Server" )
                                           : _( "Connect to PLM Server" );
    aToolBar->SetToolShortHelp( ID_PLM_TB_CONNECT, connectTip );

    // Update sync button - enabled when connected and project is managed
    aToolBar->EnableTool( ID_PLM_TB_SYNC, aState.connected && aState.project_managed );

    // Update sync button tooltip with status
    wxString syncTip = GetTooltip( ID_PLM_TB_SYNC, aState );
    aToolBar->SetToolShortHelp( ID_PLM_TB_SYNC, syncTip );
}


wxString PLM_TOOLBAR::GetTooltip( int aId, const PLM_TOOLBAR_STATE& aState ) const
{
    switch( aId )
    {
    case ID_PLM_TB_BROWSER:
        return _( "Open PLM Browser" );

    case ID_PLM_TB_CONNECT:
        return aState.connected ? _( "Disconnect from PLM Server" )
                                : _( "Connect to PLM Server" );

    case ID_PLM_TB_SYNC:
    {
        wxString tip = _( "Synchronize with PLM Server" );

        if( !aState.connected )
        {
            tip += wxS( " - " ) + _( "Not connected" );
        }
        else if( !aState.project_managed )
        {
            tip += wxS( " - " ) + _( "Project not managed" );
        }
        else
        {
            wxString stateText = PLM_ICON_PROVIDER::GetStateText( aState.sync_state );
            tip += wxS( " - " ) + stateText;
        }

        return tip;
    }

    default:
        return wxEmptyString;
    }
}


void PLM_TOOLBAR::ExecuteAction( int aId )
{
    if( m_callback )
        m_callback( aId );
    else
        wxLogWarning( wxS( "PLM: No callback set for toolbar action %d" ), aId );
}


bool PLM_TOOLBAR::ShouldShowButtons()
{
    PLM_PLUGIN_CONFIG config;

    if( !config.Load() )
        return false;

    // Show PLM toolbar buttons if any plugins are configured
    return !config.GetConfiguredPluginIds().empty();
}
