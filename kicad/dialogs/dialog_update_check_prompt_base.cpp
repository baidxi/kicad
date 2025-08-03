///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.0.0-0-g0efcecf-dirty)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_update_check_prompt_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_UPDATE_CHECK_PROMPT_BASE::DIALOG_UPDATE_CHECK_PROMPT_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );

	wxBoxSizer* bSizerMain;
	bSizerMain = new wxBoxSizer( wxVERTICAL );

	wxFlexGridSizer* fgSizer4;
	fgSizer4 = new wxFlexGridSizer( 0, 2, 10, 0 );
	fgSizer4->SetFlexibleDirection( wxBOTH );
	fgSizer4->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	wxBoxSizer* bSizer4;
	bSizer4 = new wxBoxSizer( wxVERTICAL );

	m_messageLine1 = new wxStaticText( this, wxID_ANY, _("On startup, check for updates to:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_messageLine1->Wrap( -1 );
	bSizer4->Add( m_messageLine1, 0, wxALL, 5 );

	m_cbKiCadUpdates = new wxCheckBox( this, wxID_ANY, _("KiCad"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer4->Add( m_cbKiCadUpdates, 0, wxALL, 5 );

	m_cbPCMUpdates = new wxCheckBox( this, wxID_ANY, _("Plugin and Content Manager Addons"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer4->Add( m_cbPCMUpdates, 0, wxALL, 5 );


	fgSizer4->Add( bSizer4, 1, wxEXPAND|wxRIGHT, 5 );


	fgSizer4->Add( 0, 0, 1, wxEXPAND, 5 );

	m_sdbSizer = new wxStdDialogButtonSizer();
	m_sdbSizerApply = new wxButton( this, wxID_APPLY );
	m_sdbSizer->AddButton( m_sdbSizerApply );
	m_sdbSizer->Realize();

	fgSizer4->Add( m_sdbSizer, 1, wxEXPAND, 5 );


	bSizerMain->Add( fgSizer4, 1, wxEXPAND|wxALL, 5 );


	this->SetSizer( bSizerMain );
	this->Layout();
	bSizerMain->Fit( this );

	this->Centre( wxBOTH );

	// Connect Events
	m_sdbSizerApply->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_UPDATE_CHECK_PROMPT_BASE::onApplyButton ), NULL, this );
}

DIALOG_UPDATE_CHECK_PROMPT_BASE::~DIALOG_UPDATE_CHECK_PROMPT_BASE()
{
	// Disconnect Events
	m_sdbSizerApply->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_UPDATE_CHECK_PROMPT_BASE::onApplyButton ), NULL, this );

}
