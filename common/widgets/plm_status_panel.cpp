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

#include <widgets/plm_status_panel.h>

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/statbmp.h>
#include <wx/button.h>
#include <wx/bmpbuttn.h>
#include <wx/datetime.h>

#include <bitmaps.h>


PLM_STATUS_PANEL::PLM_STATUS_PANEL( wxWindow* aParent ) :
        wxPanel( aParent, wxID_ANY ),
        m_subscriptionId( 0 ),
        m_connectionIcon( nullptr ),
        m_connectionLabel( nullptr ),
        m_serverLabel( nullptr ),
        m_lockIcon( nullptr ),
        m_lockLabel( nullptr ),
        m_syncIcon( nullptr ),
        m_syncLabel( nullptr ),
        m_lastSyncLabel( nullptr ),
        m_lockButton( nullptr ),
        m_unlockButton( nullptr ),
        m_refreshButton( nullptr ),
        m_browseButton( nullptr ),
        m_connected( false ),
        m_lockState( PLM_ICON_STATE::UNMANAGED ),
        m_syncState( PLM_ICON_STATE::UNMANAGED )
{
    createControls();
    layoutControls();
    subscribeToEvents();

    SetConnectionStatus( false );
    SetLockStatus( PLM_ICON_STATE::UNMANAGED );
    SetSyncStatus( PLM_ICON_STATE::UNMANAGED );
}


PLM_STATUS_PANEL::~PLM_STATUS_PANEL()
{
    unsubscribeFromEvents();
}


void PLM_STATUS_PANEL::createControls()
{
    PLM_ICON_PROVIDER& iconProvider = PLM_ICON_PROVIDER::GetInstance();

    // Connection status
    m_connectionIcon = new wxStaticBitmap( this, wxID_ANY, iconProvider.GetIcon( PLM_ICON_STATE::UNMANAGED, 16 ) );
    m_connectionLabel = new wxStaticText( this, wxID_ANY, _( "Disconnected" ) );
    m_serverLabel = new wxStaticText( this, wxID_ANY, wxEmptyString );

    // Lock status
    m_lockIcon = new wxStaticBitmap( this, wxID_ANY, iconProvider.GetIcon( PLM_ICON_STATE::UNMANAGED, 16 ) );
    m_lockLabel = new wxStaticText( this, wxID_ANY, _( "Not Managed" ) );

    // Sync status
    m_syncIcon = new wxStaticBitmap( this, wxID_ANY, iconProvider.GetIcon( PLM_ICON_STATE::UNMANAGED, 16 ) );
    m_syncLabel = new wxStaticText( this, wxID_ANY, _( "Not Managed" ) );
    m_lastSyncLabel = new wxStaticText( this, wxID_ANY, wxEmptyString );
    m_lastSyncLabel->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    // Action buttons
    m_lockButton = new wxButton( this, wxID_ANY, _( "Lock" ) );
    m_unlockButton = new wxButton( this, wxID_ANY, _( "Unlock" ) );
    m_refreshButton = new wxBitmapButton( this, wxID_ANY, KiBitmap( BITMAPS::refresh ) );
    m_browseButton = new wxBitmapButton( this, wxID_ANY, KiBitmap( BITMAPS::directory_browser ) );

    m_lockButton->SetToolTip( _( "Lock project for editing" ) );
    m_unlockButton->SetToolTip( _( "Release lock on project" ) );
    m_refreshButton->SetToolTip( _( "Refresh PLM status" ) );
    m_browseButton->SetToolTip( _( "Open PLM browser" ) );

    // Bind events
    m_lockButton->Bind( wxEVT_BUTTON, &PLM_STATUS_PANEL::onLockButton, this );
    m_unlockButton->Bind( wxEVT_BUTTON, &PLM_STATUS_PANEL::onUnlockButton, this );
    m_refreshButton->Bind( wxEVT_BUTTON, &PLM_STATUS_PANEL::onRefreshButton, this );
    m_browseButton->Bind( wxEVT_BUTTON, &PLM_STATUS_PANEL::onBrowseButton, this );
}


void PLM_STATUS_PANEL::layoutControls()
{
    wxBoxSizer* mainSizer = new wxBoxSizer( wxVERTICAL );

    // Connection row
    wxBoxSizer* connSizer = new wxBoxSizer( wxHORIZONTAL );
    connSizer->Add( m_connectionIcon, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5 );
    connSizer->Add( m_connectionLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10 );
    connSizer->Add( m_serverLabel, 1, wxALIGN_CENTER_VERTICAL );
    mainSizer->Add( connSizer, 0, wxEXPAND | wxALL, 5 );

    // Lock row
    wxBoxSizer* lockSizer = new wxBoxSizer( wxHORIZONTAL );
    lockSizer->Add( m_lockIcon, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5 );
    lockSizer->Add( m_lockLabel, 1, wxALIGN_CENTER_VERTICAL );
    mainSizer->Add( lockSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5 );

    // Sync row
    wxBoxSizer* syncSizer = new wxBoxSizer( wxHORIZONTAL );
    syncSizer->Add( m_syncIcon, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5 );
    syncSizer->Add( m_syncLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10 );
    syncSizer->Add( m_lastSyncLabel, 1, wxALIGN_CENTER_VERTICAL );
    mainSizer->Add( syncSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5 );

    // Button row
    wxBoxSizer* buttonSizer = new wxBoxSizer( wxHORIZONTAL );
    buttonSizer->Add( m_lockButton, 0, wxRIGHT, 5 );
    buttonSizer->Add( m_unlockButton, 0, wxRIGHT, 10 );
    buttonSizer->AddStretchSpacer();
    buttonSizer->Add( m_refreshButton, 0, wxRIGHT, 5 );
    buttonSizer->Add( m_browseButton, 0 );
    mainSizer->Add( buttonSizer, 0, wxEXPAND | wxALL, 5 );

    SetSizer( mainSizer );
}


void PLM_STATUS_PANEL::SetProject( const wxString& aProjectPath )
{
    m_projectPath = aProjectPath;
    RefreshStatus();
}


void PLM_STATUS_PANEL::ClearProject()
{
    m_projectPath.Clear();
    SetConnectionStatus( false );
    SetLockStatus( PLM_ICON_STATE::UNMANAGED );
    SetSyncStatus( PLM_ICON_STATE::UNMANAGED );
    EnableActions( false );
}


void PLM_STATUS_PANEL::SetConnectionStatus( bool aConnected, const wxString& aServerName )
{
    m_connected = aConnected;

    if( aConnected )
    {
        updateStateIcon( m_connectionIcon, PLM_ICON_STATE::CLEAN );
        m_connectionLabel->SetLabel( _( "Connected" ) );
        m_serverLabel->SetLabel( aServerName );
    }
    else
    {
        updateStateIcon( m_connectionIcon, PLM_ICON_STATE::ERROR_STATE );
        m_connectionLabel->SetLabel( _( "Disconnected" ) );
        m_serverLabel->SetLabel( wxEmptyString );
    }

    Layout();
}


void PLM_STATUS_PANEL::SetLockStatus( PLM_ICON_STATE aState, const wxString& aOwner )
{
    m_lockState = aState;
    updateStateIcon( m_lockIcon, aState );

    wxString label = PLM_ICON_PROVIDER::GetStateText( aState );

    if( !aOwner.IsEmpty() )
        label += wxString::Format( wxS( " (%s)" ), aOwner );

    m_lockLabel->SetLabel( label );

    // Update button states
    bool canLock = ( aState == PLM_ICON_STATE::CLEAN || aState == PLM_ICON_STATE::MODIFIED );
    bool canUnlock = ( aState == PLM_ICON_STATE::LOCKED_BY_ME );

    m_lockButton->Enable( m_connected && canLock );
    m_unlockButton->Enable( m_connected && canUnlock );

    Layout();
}


void PLM_STATUS_PANEL::SetSyncStatus( PLM_ICON_STATE aState, int64_t aLastSyncTime )
{
    m_syncState = aState;
    updateStateIcon( m_syncIcon, aState );
    m_syncLabel->SetLabel( PLM_ICON_PROVIDER::GetStateText( aState ) );

    if( aLastSyncTime > 0 )
        m_lastSyncLabel->SetLabel( formatTimestamp( aLastSyncTime ) );
    else
        m_lastSyncLabel->SetLabel( wxEmptyString );

    Layout();
}


void PLM_STATUS_PANEL::RefreshStatus()
{
    // TODO: Query PLM system for current status
    // For now, this is a placeholder that would integrate with PLM_PROJECT_BINDING_MANAGER
}


void PLM_STATUS_PANEL::EnableActions( bool aEnable )
{
    m_lockButton->Enable( aEnable );
    m_unlockButton->Enable( aEnable );
    m_refreshButton->Enable( aEnable );
    m_browseButton->Enable( aEnable );
}


bool PLM_STATUS_PANEL::Show( bool aShow )
{
    return wxPanel::Show( aShow );
}


void PLM_STATUS_PANEL::subscribeToEvents()
{
    if( m_subscriptionId != 0 )
        return;

    PLM_EVENT_FILTER filter;

    m_subscriptionId = PLM_EVENT_BUS::GetInstance().Subscribe(
            [this]( const PLM_EVENT& event )
            {
                onPlmEvent( event );
            },
            filter );
}


void PLM_STATUS_PANEL::unsubscribeFromEvents()
{
    if( m_subscriptionId != 0 )
    {
        PLM_EVENT_BUS::GetInstance().Unsubscribe( m_subscriptionId );
        m_subscriptionId = 0;
    }
}


void PLM_STATUS_PANEL::onPlmEvent( const PLM_EVENT& aEvent )
{
    // Handle events relevant to the current project
    if( !m_projectPath.IsEmpty() && aEvent.project_id != m_projectPath )
        return;

    switch( aEvent.type )
    {
    case PLM_EVENT_TYPE::SERVER_CONNECTED:
        SetConnectionStatus( true, aEvent.message );
        break;

    case PLM_EVENT_TYPE::SERVER_DISCONNECTED:
        SetConnectionStatus( false );
        break;

    case PLM_EVENT_TYPE::LOCK_ACQUIRED:
        SetLockStatus( PLM_ICON_STATE::LOCKED_BY_ME );
        break;

    case PLM_EVENT_TYPE::LOCK_RELEASED:
        SetLockStatus( PLM_ICON_STATE::CLEAN );
        break;

    case PLM_EVENT_TYPE::LOCK_STOLEN:
    case PLM_EVENT_TYPE::LOCK_EXPIRED:
        SetLockStatus( PLM_ICON_STATE::CLEAN );
        break;

    case PLM_EVENT_TYPE::STATE_CHANGED:
    case PLM_EVENT_TYPE::STATE_SYNC_COMPLETED:
        RefreshStatus();
        break;

    default:
        break;
    }
}


void PLM_STATUS_PANEL::updateStateIcon( wxStaticBitmap* aBitmap, PLM_ICON_STATE aState )
{
    if( aBitmap )
    {
        PLM_ICON_PROVIDER& provider = PLM_ICON_PROVIDER::GetInstance();
        aBitmap->SetBitmap( provider.GetIcon( aState, 16 ) );
    }
}


void PLM_STATUS_PANEL::onLockButton( wxCommandEvent& /* aEvent */ )
{
    if( m_lockCallback )
        m_lockCallback();
}


void PLM_STATUS_PANEL::onUnlockButton( wxCommandEvent& /* aEvent */ )
{
    if( m_unlockCallback )
        m_unlockCallback();
}


void PLM_STATUS_PANEL::onRefreshButton( wxCommandEvent& /* aEvent */ )
{
    if( m_refreshCallback )
        m_refreshCallback();
}


void PLM_STATUS_PANEL::onBrowseButton( wxCommandEvent& /* aEvent */ )
{
    if( m_browseCallback )
        m_browseCallback();
}


wxString PLM_STATUS_PANEL::formatTimestamp( int64_t aTimestamp )
{
    if( aTimestamp <= 0 )
        return wxEmptyString;

    wxDateTime dt( (time_t) aTimestamp );

    if( !dt.IsValid() )
        return wxEmptyString;

    wxDateTime today = wxDateTime::Today();

    if( dt.GetYear() == today.GetYear() && dt.GetMonth() == today.GetMonth()
        && dt.GetDay() == today.GetDay() )
    {
        return dt.Format( _( "Today %H:%M" ) );
    }

    return dt.Format( _( "%Y-%m-%d %H:%M" ) );
}


wxString PLM_STATUS_PANEL::formatLockOwner( const wxString& aOwner )
{
    if( aOwner.IsEmpty() )
        return wxEmptyString;

    return wxString::Format( _( "Locked by %s" ), aOwner );
}
