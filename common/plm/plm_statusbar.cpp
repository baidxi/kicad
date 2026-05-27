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

#include "plm_statusbar.h"
#include "plm_plugin_config.h"

#include <wx/intl.h>
#include <wx/log.h>


PLM_STATUSBAR::PLM_STATUSBAR()
{
}


PLM_STATUSBAR::~PLM_STATUSBAR()
{
}


void PLM_STATUSBAR::UpdateFields( wxStatusBar* aStatusBar, int aBaseField,
                                   const PLM_STATUSBAR_STATE& aState )
{
    if( !aStatusBar )
        return;

    int fieldCount = aStatusBar->GetFieldsCount();

    // Update connection field
    int connField = aBaseField + PLM_SB_CONNECTION;

    if( connField < fieldCount )
    {
        aStatusBar->SetStatusText( GetFieldText( PLM_SB_CONNECTION, aState ), connField );
    }

    // Update lock field
    int lockField = aBaseField + PLM_SB_LOCK;

    if( lockField < fieldCount )
    {
        aStatusBar->SetStatusText( GetFieldText( PLM_SB_LOCK, aState ), lockField );
    }

    // Update sync field
    int syncField = aBaseField + PLM_SB_SYNC;

    if( syncField < fieldCount )
    {
        aStatusBar->SetStatusText( GetFieldText( PLM_SB_SYNC, aState ), syncField );
    }
}


wxString PLM_STATUSBAR::GetFieldText( PLM_STATUSBAR_FIELD aField,
                                       const PLM_STATUSBAR_STATE& aState ) const
{
    switch( aField )
    {
    case PLM_SB_CONNECTION:
        if( aState.connected )
        {
            if( aState.server_name.IsEmpty() )
                return _( "PLM: Connected" );
            else
                return wxString::Format( _( "PLM: %s" ), aState.server_name );
        }
        else
        {
            return _( "PLM: Disconnected" );
        }

    case PLM_SB_LOCK:
        if( !aState.connected )
            return wxEmptyString;

        if( aState.lock_state == PLM_ICON_STATE::UNMANAGED )
            return wxEmptyString;

        if( aState.lock_state == PLM_ICON_STATE::LOCKED_BY_ME )
            return _( "Locked" );

        if( aState.lock_state == PLM_ICON_STATE::LOCKED_BY_OTHER )
        {
            if( aState.lock_owner.IsEmpty() )
                return _( "Locked by other" );
            else
                return wxString::Format( _( "Locked by %s" ), aState.lock_owner );
        }

        return PLM_ICON_PROVIDER::GetStateText( aState.lock_state );

    case PLM_SB_SYNC:
        if( !aState.connected )
            return wxEmptyString;

        if( aState.sync_state == PLM_ICON_STATE::UNMANAGED )
            return wxEmptyString;

        return PLM_ICON_PROVIDER::GetStateText( aState.sync_state );

    default:
        return wxEmptyString;
    }
}


wxString PLM_STATUSBAR::GetFieldTooltip( PLM_STATUSBAR_FIELD aField,
                                          const PLM_STATUSBAR_STATE& aState ) const
{
    switch( aField )
    {
    case PLM_SB_CONNECTION:
        if( aState.connected )
            return _( "Click to disconnect from PLM server" );
        else
            return _( "Click to connect to PLM server" );

    case PLM_SB_LOCK:
        if( aState.lock_state == PLM_ICON_STATE::LOCKED_BY_ME )
            return _( "Click to release lock" );
        else if( aState.lock_state == PLM_ICON_STATE::CLEAN )
            return _( "Click to acquire lock" );
        else
            return wxEmptyString;

    case PLM_SB_SYNC:
        if( aState.sync_state == PLM_ICON_STATE::MODIFIED )
            return _( "Click to check in changes" );
        else if( aState.sync_state == PLM_ICON_STATE::OUTDATED )
            return _( "Click to update from server" );
        else
            return _( "Click to refresh status" );

    default:
        return wxEmptyString;
    }
}


std::vector<int> PLM_STATUSBAR::GetFieldWidths()
{
    // Suggested widths for PLM status bar fields
    return { 150, 120, 100 };  // Connection, Lock, Sync
}


void PLM_STATUSBAR::HandleClick( int aBaseField, int aClickedField )
{
    int plmField = aClickedField - aBaseField;

    if( plmField >= 0 && plmField < PLM_SB_FIELD_COUNT )
    {
        if( m_clickCb )
            m_clickCb( static_cast<PLM_STATUSBAR_FIELD>( plmField ) );
    }
}


bool PLM_STATUSBAR::ShouldShow()
{
    PLM_PLUGIN_CONFIG config;

    if( !config.Load() )
        return false;

    // Show PLM status bar if any plugins are configured
    return !config.GetConfiguredPluginIds().empty();
}
