///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6a-dirty)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "widgets/wx_html_report_panel.h"

#include "dialog_relink_footprints_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_RELINK_FOOTPRINTS_BASE::DIALOG_RELINK_FOOTPRINTS_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( -1,-1 ), wxDefaultSize );

	wxBoxSizer* bMainSizer;
	bMainSizer = new wxBoxSizer( wxVERTICAL );

	wxStaticBoxSizer* sbScope;
	sbScope = new wxStaticBoxSizer( new wxStaticBox( this, wxID_ANY, _("Scope") ), wxVERTICAL );

	m_rbSelected = new wxRadioButton( sbScope->GetStaticBox(), wxID_ANY, _("Relink selected footprints"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP );
	m_rbSelected->SetToolTip( _("Only relink footprints that are currently selected") );

	sbScope->Add( m_rbSelected, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );

	m_rbAll = new wxRadioButton( sbScope->GetStaticBox(), wxID_ANY, _("Relink all footprints"), wxDefaultPosition, wxDefaultSize, 0 );
	m_rbAll->SetValue( true );
	m_rbAll->SetToolTip( _("Relink all footprints on the board") );

	sbScope->Add( m_rbAll, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );


	bMainSizer->Add( sbScope, 0, wxEXPAND|wxTOP|wxRIGHT|wxLEFT, 10 );

	wxBoxSizer* bLowerSizer;
	bLowerSizer = new wxBoxSizer( wxVERTICAL );

	bLowerSizer->SetMinSize( wxSize( 660,300 ) );
	m_messagePanel = new WX_HTML_REPORT_PANEL( this, wxID_ANY, wxDefaultPosition, wxSize( -1,-1 ), wxTAB_TRAVERSAL );
	bLowerSizer->Add( m_messagePanel, 1, wxEXPAND | wxALL, 5 );


	bMainSizer->Add( bLowerSizer, 1, wxEXPAND|wxTOP|wxRIGHT|wxLEFT, 10 );

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

	// Connect Events
	m_rbSelected->Connect( wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler( DIALOG_RELINK_FOOTPRINTS_BASE::OnScopeChanged ), NULL, this );
	m_rbAll->Connect( wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler( DIALOG_RELINK_FOOTPRINTS_BASE::OnScopeChanged ), NULL, this );
	m_sdbSizerOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_RELINK_FOOTPRINTS_BASE::OnRelinkClick ), NULL, this );
}

DIALOG_RELINK_FOOTPRINTS_BASE::~DIALOG_RELINK_FOOTPRINTS_BASE()
{
	// Disconnect Events
	m_rbSelected->Disconnect( wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler( DIALOG_RELINK_FOOTPRINTS_BASE::OnScopeChanged ), NULL, this );
	m_rbAll->Disconnect( wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler( DIALOG_RELINK_FOOTPRINTS_BASE::OnScopeChanged ), NULL, this );
	m_sdbSizerOK->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_RELINK_FOOTPRINTS_BASE::OnRelinkClick ), NULL, this );

}
