///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6a-dirty)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_plm_config_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_PLM_CONFIG_BASE::DIALOG_PLM_CONFIG_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 500,-1 ), wxDefaultSize );

	wxBoxSizer* bMainSizer;
	bMainSizer = new wxBoxSizer( wxVERTICAL );

	m_infoLabel = new wxStaticText( this, wxID_ANY, _("Configure PLM plugin and server connection settings."), wxDefaultPosition, wxDefaultSize, 0 );
	m_infoLabel->Wrap( -1 );
	bMainSizer->Add( m_infoLabel, 0, wxALL|wxEXPAND, 10 );

	wxFlexGridSizer* fgPluginSizer;
	fgPluginSizer = new wxFlexGridSizer( 0, 2, 5, 0 );
	fgPluginSizer->AddGrowableCol( 1 );
	fgPluginSizer->SetFlexibleDirection( wxBOTH );
	fgPluginSizer->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	wxStaticText* m_pluginLabel;
	m_pluginLabel = new wxStaticText( this, wxID_ANY, _("PLM Plugin:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_pluginLabel->Wrap( -1 );
	fgPluginSizer->Add( m_pluginLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	wxArrayString m_pluginChoiceChoices;
	m_pluginChoice = new wxChoice( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_pluginChoiceChoices, 0 );
	m_pluginChoice->SetSelection( 0 );
	fgPluginSizer->Add( m_pluginChoice, 0, wxALL|wxEXPAND, 5 );


	bMainSizer->Add( fgPluginSizer, 0, wxEXPAND|wxTOP|wxRIGHT|wxLEFT, 5 );

	m_pluginDescLabel = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_pluginDescLabel->Wrap( -1 );
	bMainSizer->Add( m_pluginDescLabel, 0, wxEXPAND|wxALL, 5 );

	wxStaticBoxSizer* sbServerSettings;
	sbServerSettings = new wxStaticBoxSizer( new wxStaticBox( this, wxID_ANY, _("Server Settings") ), wxVERTICAL );

	wxFlexGridSizer* fgServerSizer;
	fgServerSizer = new wxFlexGridSizer( 0, 2, 5, 0 );
	fgServerSizer->AddGrowableCol( 1 );
	fgServerSizer->SetFlexibleDirection( wxBOTH );
	fgServerSizer->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	wxStaticText* m_serverUrlLabel;
	m_serverUrlLabel = new wxStaticText( sbServerSettings->GetStaticBox(), wxID_ANY, _("Server URL:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_serverUrlLabel->Wrap( -1 );
	fgServerSizer->Add( m_serverUrlLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_serverUrlCtrl = new wxTextCtrl( sbServerSettings->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	fgServerSizer->Add( m_serverUrlCtrl, 0, wxALL|wxEXPAND, 5 );

	wxStaticText* m_defaultFolderLabel;
	m_defaultFolderLabel = new wxStaticText( sbServerSettings->GetStaticBox(), wxID_ANY, _("Default Project Folder:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_defaultFolderLabel->Wrap( -1 );
	fgServerSizer->Add( m_defaultFolderLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_defaultFolderCtrl = new wxTextCtrl( sbServerSettings->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	fgServerSizer->Add( m_defaultFolderCtrl, 0, wxALL|wxEXPAND, 5 );


	sbServerSettings->Add( fgServerSizer, 0, wxEXPAND, 5 );


	bMainSizer->Add( sbServerSettings, 0, wxEXPAND|wxALL, 5 );

	wxStaticBoxSizer* sbConnectionTest;
	sbConnectionTest = new wxStaticBoxSizer( new wxStaticBox( this, wxID_ANY, _("Connection Test") ), wxVERTICAL );

	m_testConnectionBtn = new wxButton( sbConnectionTest->GetStaticBox(), wxID_ANY, _("Test Connection"), wxDefaultPosition, wxDefaultSize, 0 );
	sbConnectionTest->Add( m_testConnectionBtn, 0, wxALL, 5 );

	m_connectionStatusLabel = new wxStaticText( sbConnectionTest->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_connectionStatusLabel->Wrap( -1 );
	sbConnectionTest->Add( m_connectionStatusLabel, 0, wxALL|wxEXPAND, 5 );


	bMainSizer->Add( sbConnectionTest, 0, wxEXPAND|wxALL, 5 );

	wxStaticBoxSizer* sbAuthentication;
	sbAuthentication = new wxStaticBoxSizer( new wxStaticBox( this, wxID_ANY, _("Authentication") ), wxVERTICAL );

	m_useDefaultCredsCheckbox = new wxCheckBox( sbAuthentication->GetStaticBox(), wxID_ANY, _("Use stored credentials"), wxDefaultPosition, wxDefaultSize, 0 );
	sbAuthentication->Add( m_useDefaultCredsCheckbox, 0, wxALL, 5 );

	wxFlexGridSizer* fgAuthSizer;
	fgAuthSizer = new wxFlexGridSizer( 0, 2, 5, 0 );
	fgAuthSizer->AddGrowableCol( 1 );
	fgAuthSizer->SetFlexibleDirection( wxBOTH );
	fgAuthSizer->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	wxStaticText* m_usernameLabel;
	m_usernameLabel = new wxStaticText( sbAuthentication->GetStaticBox(), wxID_ANY, _("Username:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_usernameLabel->Wrap( -1 );
	fgAuthSizer->Add( m_usernameLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_usernameCtrl = new wxTextCtrl( sbAuthentication->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	fgAuthSizer->Add( m_usernameCtrl, 0, wxALL|wxEXPAND, 5 );

	wxStaticText* m_passwordLabel;
	m_passwordLabel = new wxStaticText( sbAuthentication->GetStaticBox(), wxID_ANY, _("Password:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_passwordLabel->Wrap( -1 );
	fgAuthSizer->Add( m_passwordLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_passwordCtrl = new wxTextCtrl( sbAuthentication->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD );
	fgAuthSizer->Add( m_passwordCtrl, 0, wxALL|wxEXPAND, 5 );


	sbAuthentication->Add( fgAuthSizer, 0, wxEXPAND, 5 );


	bMainSizer->Add( sbAuthentication, 0, wxEXPAND|wxALL, 5 );

	m_sdbSizer = new wxStdDialogButtonSizer();
	m_sdbSizerOK = new wxButton( this, wxID_OK );
	m_sdbSizer->AddButton( m_sdbSizerOK );
	m_sdbSizerCancel = new wxButton( this, wxID_CANCEL );
	m_sdbSizer->AddButton( m_sdbSizerCancel );
	m_sdbSizer->Realize();

	bMainSizer->Add( m_sdbSizer, 0, wxALL|wxEXPAND, 5 );


	this->SetSizer( bMainSizer );
	this->Layout();
	bMainSizer->Fit( this );

	this->Centre( wxBOTH );

	// Connect Events
	m_pluginChoice->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( DIALOG_PLM_CONFIG_BASE::onPluginSelected ), NULL, this );
	m_testConnectionBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_CONFIG_BASE::onTestConnection ), NULL, this );
	m_useDefaultCredsCheckbox->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( DIALOG_PLM_CONFIG_BASE::onUseDefaultCredsChanged ), NULL, this );
}

DIALOG_PLM_CONFIG_BASE::~DIALOG_PLM_CONFIG_BASE()
{
	// Disconnect Events
	m_pluginChoice->Disconnect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( DIALOG_PLM_CONFIG_BASE::onPluginSelected ), NULL, this );
	m_testConnectionBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_CONFIG_BASE::onTestConnection ), NULL, this );
	m_useDefaultCredsCheckbox->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( DIALOG_PLM_CONFIG_BASE::onUseDefaultCredsChanged ), NULL, this );

}
