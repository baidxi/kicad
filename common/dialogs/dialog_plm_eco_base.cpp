///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6a-dirty)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_plm_eco_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_PLM_ECO_BASE::DIALOG_PLM_ECO_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 700,500 ), wxDefaultSize );

	wxBoxSizer* bMainSizer;
	bMainSizer = new wxBoxSizer( wxVERTICAL );

	m_infoLabel = new wxStaticText( this, wxID_ANY, _("Select an Engineering Change Order to associate with your check-in:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_infoLabel->Wrap( -1 );
	bMainSizer->Add( m_infoLabel, 0, wxALL|wxEXPAND, 10 );

	m_splitter = new wxSplitterWindow( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_LIVE_UPDATE );
	m_splitter->SetSashGravity( 0.3 );
	m_splitter->Connect( wxEVT_IDLE, wxIdleEventHandler( DIALOG_PLM_ECO_BASE::m_splitterOnIdle ), NULL, this );

	wxPanel* m_leftPanel;
	m_leftPanel = new wxPanel( m_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bLeftSizer;
	bLeftSizer = new wxBoxSizer( wxVERTICAL );

	m_ecoList = new wxListBox( m_leftPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 );
	bLeftSizer->Add( m_ecoList, 1, wxEXPAND|wxALL, 5 );


	m_leftPanel->SetSizer( bLeftSizer );
	m_leftPanel->Layout();
	bLeftSizer->Fit( m_leftPanel );
	m_detailsPanel = new wxPanel( m_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bDetailsSizer;
	bDetailsSizer = new wxBoxSizer( wxVERTICAL );

	wxFlexGridSizer* fgDetailsSizer;
	fgDetailsSizer = new wxFlexGridSizer( 0, 2, 5, 10 );
	fgDetailsSizer->AddGrowableCol( 1 );
	fgDetailsSizer->SetFlexibleDirection( wxBOTH );
	fgDetailsSizer->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	wxStaticText* m_staticText1;
	m_staticText1 = new wxStaticText( m_detailsPanel, wxID_ANY, _("ECO Number:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText1->Wrap( -1 );
	fgDetailsSizer->Add( m_staticText1, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 5 );

	m_ecoNumberLabel = new wxStaticText( m_detailsPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_ecoNumberLabel->Wrap( -1 );
	fgDetailsSizer->Add( m_ecoNumberLabel, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );

	wxStaticText* m_staticText2;
	m_staticText2 = new wxStaticText( m_detailsPanel, wxID_ANY, _("Title:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText2->Wrap( -1 );
	fgDetailsSizer->Add( m_staticText2, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 5 );

	m_ecoTitleLabel = new wxStaticText( m_detailsPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_ecoTitleLabel->Wrap( -1 );
	fgDetailsSizer->Add( m_ecoTitleLabel, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );

	wxStaticText* m_staticText3;
	m_staticText3 = new wxStaticText( m_detailsPanel, wxID_ANY, _("Owner:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText3->Wrap( -1 );
	fgDetailsSizer->Add( m_staticText3, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 5 );

	m_ecoOwnerLabel = new wxStaticText( m_detailsPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_ecoOwnerLabel->Wrap( -1 );
	fgDetailsSizer->Add( m_ecoOwnerLabel, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );

	wxStaticText* m_staticText4;
	m_staticText4 = new wxStaticText( m_detailsPanel, wxID_ANY, _("Status:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText4->Wrap( -1 );
	fgDetailsSizer->Add( m_staticText4, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 5 );

	m_ecoStatusLabel = new wxStaticText( m_detailsPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_ecoStatusLabel->Wrap( -1 );
	fgDetailsSizer->Add( m_ecoStatusLabel, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );

	wxStaticText* m_staticText5;
	m_staticText5 = new wxStaticText( m_detailsPanel, wxID_ANY, _("Due Date:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText5->Wrap( -1 );
	fgDetailsSizer->Add( m_staticText5, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 5 );

	m_ecoDueDateLabel = new wxStaticText( m_detailsPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_ecoDueDateLabel->Wrap( -1 );
	fgDetailsSizer->Add( m_ecoDueDateLabel, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );


	bDetailsSizer->Add( fgDetailsSizer, 0, wxALL, 10 );

	wxStaticText* m_staticText6;
	m_staticText6 = new wxStaticText( m_detailsPanel, wxID_ANY, _("Description:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText6->Wrap( -1 );
	bDetailsSizer->Add( m_staticText6, 0, wxEXPAND|wxLEFT|wxRIGHT, 10 );

	m_ecoDescriptionCtrl = new wxTextCtrl( m_detailsPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_READONLY|wxTE_WORDWRAP );
	bDetailsSizer->Add( m_ecoDescriptionCtrl, 1, wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM, 10 );


	m_detailsPanel->SetSizer( bDetailsSizer );
	m_detailsPanel->Layout();
	bDetailsSizer->Fit( m_detailsPanel );
	m_splitter->SplitVertically( m_leftPanel, m_detailsPanel, 0 );
	bMainSizer->Add( m_splitter, 1, wxEXPAND|wxLEFT|wxRIGHT, 10 );

	wxBoxSizer* bFieldsSizer;
	bFieldsSizer = new wxBoxSizer( wxVERTICAL );

	m_fieldsLabel = new wxStaticText( this, wxID_ANY, _("Required Fields for Check-in:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_fieldsLabel->Wrap( -1 );
	bFieldsSizer->Add( m_fieldsLabel, 0, wxALL, 5 );

	m_fieldsScrolled = new wxScrolledWindow( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	m_fieldsScrolled->SetScrollRate( 5, 5 );
	m_fieldsScrolled->SetMinSize( wxSize( -1,100 ) );

	bFieldsScrollSizer = new wxBoxSizer( wxVERTICAL );


	m_fieldsScrolled->SetSizer( bFieldsScrollSizer );
	m_fieldsScrolled->Layout();
	bFieldsScrollSizer->Fit( m_fieldsScrolled );
	bFieldsSizer->Add( m_fieldsScrolled, 0, wxEXPAND|wxALL, 5 );


	bMainSizer->Add( bFieldsSizer, 0, wxEXPAND|wxLEFT|wxRIGHT, 10 );

	m_sdbSizer = new wxStdDialogButtonSizer();
	m_sdbSizerOK = new wxButton( this, wxID_OK );
	m_sdbSizer->AddButton( m_sdbSizerOK );
	m_sdbSizerCancel = new wxButton( this, wxID_CANCEL );
	m_sdbSizer->AddButton( m_sdbSizerCancel );
	m_sdbSizer->Realize();

	bMainSizer->Add( m_sdbSizer, 0, wxEXPAND|wxALL, 10 );


	this->SetSizer( bMainSizer );
	this->Layout();
	bMainSizer->Fit( this );

	this->Centre( wxBOTH );

	// Connect Events
	m_ecoList->Connect( wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler( DIALOG_PLM_ECO_BASE::onEcoSelected ), NULL, this );
}

DIALOG_PLM_ECO_BASE::~DIALOG_PLM_ECO_BASE()
{
	// Disconnect Events
	m_ecoList->Disconnect( wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler( DIALOG_PLM_ECO_BASE::onEcoSelected ), NULL, this );

}
