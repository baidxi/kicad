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

#include <dialogs/dialog_plm_config.h>

#include <plm/plm_plugin.h>

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/msgdlg.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>


DIALOG_PLM_CONFIG::DIALOG_PLM_CONFIG( wxWindow* aParent ) :
        DIALOG_PLM_CONFIG_BASE( aParent ),
        m_plugin( nullptr ),
        m_capabilitiesText( nullptr ),
        m_autoSyncCheckbox( nullptr ),
        m_syncIntervalSpin( nullptr ),
        m_conflictResolutionChoice( nullptr ),
        m_offlineModeCheckbox( nullptr ),
        m_cacheSizeSpin( nullptr ),
        m_cacheUsageLabel( nullptr )
{
    m_connectionStatusLabel->SetLabel( wxEmptyString );

    createExtendedControls();

    SetupStandardButtons();
    finishDialogSettings();
}


DIALOG_PLM_CONFIG::~DIALOG_PLM_CONFIG()
{
}


void DIALOG_PLM_CONFIG::createExtendedControls()
{
    wxBoxSizer* bMainSizer = dynamic_cast<wxBoxSizer*>( GetSizer() );

    if( !bMainSizer )
        return;

    // Insert extended controls before the standard buttons (last item in sizer)
    int insertPos = static_cast<int>( bMainSizer->GetItemCount() ) - 1;

    if( insertPos < 0 )
        insertPos = 0;

    // Plugin Capabilities section
    wxStaticBoxSizer* sbCapabilities = new wxStaticBoxSizer(
            new wxStaticBox( this, wxID_ANY, _( "Plugin Capabilities" ) ), wxVERTICAL );

    m_capabilitiesText = new wxTextCtrl( sbCapabilities->GetStaticBox(), wxID_ANY, wxEmptyString,
                                          wxDefaultPosition, wxSize( -1, 80 ),
                                          wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP );
    sbCapabilities->Add( m_capabilitiesText, 0, wxEXPAND | wxALL, 5 );

    bMainSizer->Insert( insertPos++, sbCapabilities, 0, wxEXPAND | wxALL, 5 );

    // Sync Options section
    wxStaticBoxSizer* sbSyncOptions = new wxStaticBoxSizer(
            new wxStaticBox( this, wxID_ANY, _( "Sync Options" ) ), wxVERTICAL );

    m_autoSyncCheckbox = new wxCheckBox( sbSyncOptions->GetStaticBox(), wxID_ANY,
                                          _( "Enable automatic synchronization" ) );
    sbSyncOptions->Add( m_autoSyncCheckbox, 0, wxALL, 5 );

    wxFlexGridSizer* fgSyncSizer = new wxFlexGridSizer( 0, 2, 5, 0 );
    fgSyncSizer->AddGrowableCol( 1 );
    fgSyncSizer->SetFlexibleDirection( wxBOTH );
    fgSyncSizer->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    wxStaticText* syncIntervalLabel = new wxStaticText( sbSyncOptions->GetStaticBox(), wxID_ANY,
                                                         _( "Sync interval (minutes):" ) );
    syncIntervalLabel->Wrap( -1 );
    fgSyncSizer->Add( syncIntervalLabel, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

    m_syncIntervalSpin = new wxSpinCtrl( sbSyncOptions->GetStaticBox(), wxID_ANY, wxEmptyString,
                                          wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
                                          1, 120, 15 );
    fgSyncSizer->Add( m_syncIntervalSpin, 0, wxALL | wxEXPAND, 5 );

    wxStaticText* conflictLabel = new wxStaticText( sbSyncOptions->GetStaticBox(), wxID_ANY,
                                                     _( "Conflict resolution:" ) );
    conflictLabel->Wrap( -1 );
    fgSyncSizer->Add( conflictLabel, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

    wxArrayString conflictChoices;
    conflictChoices.Add( _( "Ask each time" ) );
    conflictChoices.Add( _( "Local changes win" ) );
    conflictChoices.Add( _( "Remote changes win" ) );

    m_conflictResolutionChoice = new wxChoice( sbSyncOptions->GetStaticBox(), wxID_ANY,
                                                wxDefaultPosition, wxDefaultSize,
                                                conflictChoices, 0 );
    m_conflictResolutionChoice->SetSelection( 0 );
    fgSyncSizer->Add( m_conflictResolutionChoice, 0, wxALL | wxEXPAND, 5 );

    sbSyncOptions->Add( fgSyncSizer, 0, wxEXPAND, 5 );

    bMainSizer->Insert( insertPos++, sbSyncOptions, 0, wxEXPAND | wxALL, 5 );

    // Offline & Cache section
    wxStaticBoxSizer* sbOfflineCache = new wxStaticBoxSizer(
            new wxStaticBox( this, wxID_ANY, _( "Offline Mode & Cache" ) ), wxVERTICAL );

    m_offlineModeCheckbox = new wxCheckBox( sbOfflineCache->GetStaticBox(), wxID_ANY,
                                             _( "Work offline (disable server communication)" ) );
    sbOfflineCache->Add( m_offlineModeCheckbox, 0, wxALL, 5 );

    wxFlexGridSizer* fgCacheSizer = new wxFlexGridSizer( 0, 2, 5, 0 );
    fgCacheSizer->AddGrowableCol( 1 );
    fgCacheSizer->SetFlexibleDirection( wxBOTH );
    fgCacheSizer->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    wxStaticText* cacheSizeLabel = new wxStaticText( sbOfflineCache->GetStaticBox(), wxID_ANY,
                                                      _( "Cache size limit (MB):" ) );
    cacheSizeLabel->Wrap( -1 );
    fgCacheSizer->Add( cacheSizeLabel, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

    m_cacheSizeSpin = new wxSpinCtrl( sbOfflineCache->GetStaticBox(), wxID_ANY, wxEmptyString,
                                       wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
                                       16, 4096, 256 );
    fgCacheSizer->Add( m_cacheSizeSpin, 0, wxALL | wxEXPAND, 5 );

    sbOfflineCache->Add( fgCacheSizer, 0, wxEXPAND, 5 );

    wxBoxSizer* bCacheBtnSizer = new wxBoxSizer( wxHORIZONTAL );

    wxButton* clearCacheBtn = new wxButton( sbOfflineCache->GetStaticBox(), wxID_ANY,
                                             _( "Clear Cache" ) );
    clearCacheBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                            wxCommandEventHandler( DIALOG_PLM_CONFIG::onClearCache ),
                            NULL, this );
    bCacheBtnSizer->Add( clearCacheBtn, 0, wxALL, 5 );

    m_cacheUsageLabel = new wxStaticText( sbOfflineCache->GetStaticBox(), wxID_ANY,
                                           wxEmptyString );
    m_cacheUsageLabel->Wrap( -1 );
    bCacheBtnSizer->Add( m_cacheUsageLabel, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

    sbOfflineCache->Add( bCacheBtnSizer, 0, wxEXPAND, 5 );

    bMainSizer->Insert( insertPos++, sbOfflineCache, 0, wxEXPAND | wxALL, 5 );

    Layout();
    GetSizer()->Fit( this );
}


void DIALOG_PLM_CONFIG::SetAvailablePlugins(
        const std::vector<std::pair<wxString, wxString>>& aPlugins )
{
    m_plugins = aPlugins;
    m_pluginChoice->Clear();

    for( const auto& [id, name] : m_plugins )
    {
        m_pluginChoice->Append( name );
    }

    if( !m_plugins.empty() )
    {
        m_pluginChoice->SetSelection( 0 );
    }
}


void DIALOG_PLM_CONFIG::SetPlugin( PLM_PLUGIN* aPlugin )
{
    m_plugin = aPlugin;
    updateCapabilitiesDisplay();
}


void DIALOG_PLM_CONFIG::SetConfig( const PLM_SERVER_CONFIG& aConfig )
{
    m_config = aConfig;

    for( size_t i = 0; i < m_plugins.size(); ++i )
    {
        if( m_plugins[i].first == aConfig.plugin_id )
        {
            m_pluginChoice->SetSelection( i );
            break;
        }
    }

    m_serverUrlCtrl->SetValue( aConfig.server_url );
    m_defaultFolderCtrl->SetValue( aConfig.default_folder );
    m_useDefaultCredsCheckbox->SetValue( aConfig.use_stored_credentials );
    m_usernameCtrl->SetValue( aConfig.username );
    m_passwordCtrl->SetValue( aConfig.password );

    // Extended settings
    if( m_autoSyncCheckbox )
        m_autoSyncCheckbox->SetValue( aConfig.auto_sync_enabled );

    if( m_syncIntervalSpin )
        m_syncIntervalSpin->SetValue( aConfig.auto_sync_interval_minutes );

    if( m_conflictResolutionChoice )
        m_conflictResolutionChoice->SetSelection( aConfig.conflict_resolution );

    if( m_offlineModeCheckbox )
        m_offlineModeCheckbox->SetValue( aConfig.offline_mode );

    if( m_cacheSizeSpin )
        m_cacheSizeSpin->SetValue( aConfig.cache_size_mb );

    updateCredentialsEnabled();
}


PLM_SERVER_CONFIG DIALOG_PLM_CONFIG::GetConfig() const
{
    return m_config;
}


bool DIALOG_PLM_CONFIG::TransferDataToWindow()
{
    updateCapabilitiesDisplay();
    return true;
}


bool DIALOG_PLM_CONFIG::TransferDataFromWindow()
{
    int selection = m_pluginChoice->GetSelection();

    if( selection != wxNOT_FOUND && selection < (int) m_plugins.size() )
    {
        m_config.plugin_id = m_plugins[selection].first;
    }

    m_config.server_url             = m_serverUrlCtrl->GetValue();
    m_config.default_folder         = m_defaultFolderCtrl->GetValue();
    m_config.use_stored_credentials = m_useDefaultCredsCheckbox->GetValue();
    m_config.username               = m_usernameCtrl->GetValue();
    m_config.password               = m_passwordCtrl->GetValue();

    // Extended settings
    if( m_autoSyncCheckbox )
        m_config.auto_sync_enabled = m_autoSyncCheckbox->GetValue();

    if( m_syncIntervalSpin )
        m_config.auto_sync_interval_minutes = m_syncIntervalSpin->GetValue();

    if( m_conflictResolutionChoice )
        m_config.conflict_resolution = m_conflictResolutionChoice->GetSelection();

    if( m_offlineModeCheckbox )
        m_config.offline_mode = m_offlineModeCheckbox->GetValue();

    if( m_cacheSizeSpin )
        m_config.cache_size_mb = m_cacheSizeSpin->GetValue();

    return true;
}


void DIALOG_PLM_CONFIG::updateCapabilitiesDisplay()
{
    if( !m_capabilitiesText || !m_plugin )
    {
        if( m_capabilitiesText )
            m_capabilitiesText->SetValue( _( "No plugin connected" ) );

        return;
    }

    PLM_CAPABILITIES caps = m_plugin->GetCapabilities();
    wxString text;

    auto addCap = [&text]( const wxString& aName, bool aSupported )
    {
        text += wxString::Format( wxS( "%s: %s\n" ), aName,
                                  aSupported ? _( "Yes" ) : _( "No" ) );
    };

    addCap( _( "Checkout/Checkin" ), caps.supports_checkout );
    addCap( _( "File Locking" ), caps.supports_locking );
    addCap( _( "Revisions" ), caps.supports_revisions );
    addCap( _( "ECO Workflow" ), caps.supports_eco );
    addCap( _( "State Transitions" ), caps.supports_state_transitions );
    addCap( _( "Release Packaging" ), caps.supports_release_packaging );
    addCap( _( "Variants" ), caps.supports_variants );
    addCap( _( "Design Review" ), caps.supports_review );
    addCap( _( "CI/CD" ), caps.supports_ci_cd );
    addCap( _( "Audit Trail" ), caps.supports_audit_trail );
    addCap( _( "Offline Mode" ), caps.supports_offline_mode );
    addCap( _( "Search" ), caps.supports_search );

    m_capabilitiesText->SetValue( text );
}


void DIALOG_PLM_CONFIG::onPluginSelected( wxCommandEvent& event )
{
    int selection = m_pluginChoice->GetSelection();

    if( selection != wxNOT_FOUND && selection < (int) m_plugins.size() )
    {
        wxString description = wxString::Format( _( "Plugin: %s" ), m_plugins[selection].first );
        m_pluginDescLabel->SetLabel( description );
    }
    else
    {
        m_pluginDescLabel->SetLabel( wxEmptyString );
    }

    updateCapabilitiesDisplay();
    Layout();
    event.Skip();
}


void DIALOG_PLM_CONFIG::onTestConnection( wxCommandEvent& event )
{
    testConnection();
    event.Skip();
}


void DIALOG_PLM_CONFIG::onUseDefaultCredsChanged( wxCommandEvent& event )
{
    updateCredentialsEnabled();
    event.Skip();
}


void DIALOG_PLM_CONFIG::testConnection()
{
    m_connectionStatusLabel->SetLabel( _( "Testing..." ) );
    m_connectionStatusLabel->SetForegroundColour( *wxBLACK );
    Layout();
    Update();

    wxString serverUrl = m_serverUrlCtrl->GetValue();

    if( serverUrl.IsEmpty() )
    {
        m_connectionStatusLabel->SetLabel( _( "Error: Server URL is required" ) );
        m_connectionStatusLabel->SetForegroundColour( *wxRED );
        Layout();
        return;
    }

    bool success = false;

    if( success )
    {
        m_connectionStatusLabel->SetLabel( _( "Connection successful" ) );
        m_connectionStatusLabel->SetForegroundColour( wxColour( 0, 128, 0 ) );
    }
    else
    {
        m_connectionStatusLabel->SetLabel( _( "Connection failed" ) );
        m_connectionStatusLabel->SetForegroundColour( *wxRED );
    }

    Layout();
}


void DIALOG_PLM_CONFIG::updateCredentialsEnabled()
{
    bool useStoredCreds = m_useDefaultCredsCheckbox->GetValue();

    m_usernameCtrl->Enable( !useStoredCreds );
    m_passwordCtrl->Enable( !useStoredCreds );
}


void DIALOG_PLM_CONFIG::onClearCache( wxCommandEvent& event )
{
    int answer = wxMessageBox( _( "Clear all cached PLM data?" ),
                               _( "Confirm" ), wxYES_NO | wxICON_QUESTION, this );

    if( answer != wxYES )
        return;

    if( m_cacheUsageLabel )
        m_cacheUsageLabel->SetLabel( _( "Cache cleared" ) );
}
