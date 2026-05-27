///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6a-dirty)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_plm_login_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_PLM_LOGIN_BASE::DIALOG_PLM_LOGIN_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 400,200 ), wxDefaultSize );

	wxBoxSizer* bMainSizer;
	bMainSizer = new wxBoxSizer( wxVERTICAL );

	wxFlexGridSizer* fgSizer;
	fgSizer = new wxFlexGridSizer( 0, 2, 0, 0 );
	fgSizer->AddGrowableCol( 1 );
	fgSizer->SetFlexibleDirection( wxBOTH );
	fgSizer->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	m_serverUrlLabel = new wxStaticText( this, wxID_ANY, _("Server URL:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_serverUrlLabel->Wrap( -1 );
	fgSizer->Add( m_serverUrlLabel, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxALL, 5 );

	m_serverUrlCtrl = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_serverUrlCtrl->SetMinSize( wxSize( 300,-1 ) );

	fgSizer->Add( m_serverUrlCtrl, 0, wxALL|wxEXPAND, 5 );

	m_usernameLabel = new wxStaticText( this, wxID_ANY, _("Username:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_usernameLabel->Wrap( -1 );
	fgSizer->Add( m_usernameLabel, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxALL, 5 );

	m_usernameCtrl = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_usernameCtrl->SetMinSize( wxSize( 300,-1 ) );

	fgSizer->Add( m_usernameCtrl, 0, wxALL|wxEXPAND, 5 );

	m_passwordLabel = new wxStaticText( this, wxID_ANY, _("Password:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_passwordLabel->Wrap( -1 );
	fgSizer->Add( m_passwordLabel, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxALL, 5 );

	m_passwordCtrl = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD );
	m_passwordCtrl->SetMinSize( wxSize( 300,-1 ) );

	fgSizer->Add( m_passwordCtrl, 0, wxALL|wxEXPAND, 5 );


	fgSizer->Add( 0, 0, 0, wxEXPAND, 5 );

	m_rememberCheckbox = new wxCheckBox( this, wxID_ANY, _("Remember credentials"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer->Add( m_rememberCheckbox, 0, wxALL, 5 );


	bMainSizer->Add( fgSizer, 1, wxEXPAND|wxALL, 10 );

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
}

DIALOG_PLM_LOGIN_BASE::~DIALOG_PLM_LOGIN_BASE()
{
}
