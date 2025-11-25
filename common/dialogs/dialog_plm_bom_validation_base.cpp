///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6a-dirty)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_plm_bom_validation_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_PLM_BOM_VALIDATION_BASE::DIALOG_PLM_BOM_VALIDATION_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 900,600 ), wxDefaultSize );

	wxBoxSizer* bMainSizer;
	bMainSizer = new wxBoxSizer( wxVERTICAL );

	m_infoLabel = new wxStaticText( this, wxID_ANY, _("Validation complete. Review the results below and accept suggestions as needed."), wxDefaultPosition, wxDefaultSize, 0 );
	m_infoLabel->Wrap( -1 );
	bMainSizer->Add( m_infoLabel, 0, wxEXPAND|wxTOP|wxLEFT|wxRIGHT, 10 );

	m_bomList = new wxDataViewListCtrl( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES|wxDV_VERT_RULES|wxDV_SINGLE );
	bMainSizer->Add( m_bomList, 1, wxEXPAND|wxALL, 10 );

	wxStaticBoxSizer* sbSummarySizer;
	sbSummarySizer = new wxStaticBoxSizer( new wxStaticBox( this, wxID_ANY, _("Summary") ), wxHORIZONTAL );

	m_totalCountLabel = new wxStaticText( sbSummarySizer->GetStaticBox(), wxID_ANY, _("Total: 0"), wxDefaultPosition, wxDefaultSize, 0 );
	m_totalCountLabel->Wrap( -1 );
	sbSummarySizer->Add( m_totalCountLabel, 1, wxALIGN_CENTER_VERTICAL|wxALL, 10 );

	m_validCountLabel = new wxStaticText( sbSummarySizer->GetStaticBox(), wxID_ANY, _("Valid: 0"), wxDefaultPosition, wxDefaultSize, 0 );
	m_validCountLabel->Wrap( -1 );
	sbSummarySizer->Add( m_validCountLabel, 1, wxALIGN_CENTER_VERTICAL|wxALL, 10 );

	m_issueCountLabel = new wxStaticText( sbSummarySizer->GetStaticBox(), wxID_ANY, _("Issues: 0"), wxDefaultPosition, wxDefaultSize, 0 );
	m_issueCountLabel->Wrap( -1 );
	sbSummarySizer->Add( m_issueCountLabel, 1, wxALIGN_CENTER_VERTICAL|wxALL, 10 );


	bMainSizer->Add( sbSummarySizer, 0, wxEXPAND|wxLEFT|wxRIGHT, 10 );

	wxBoxSizer* bButtonSizer;
	bButtonSizer = new wxBoxSizer( wxHORIZONTAL );

	m_acceptAllBtn = new wxButton( this, wxID_ANY, _("Accept All Suggestions"), wxDefaultPosition, wxDefaultSize, 0 );
	bButtonSizer->Add( m_acceptAllBtn, 0, wxALL, 5 );

	m_acceptSelectedBtn = new wxButton( this, wxID_ANY, _("Accept Selected"), wxDefaultPosition, wxDefaultSize, 0 );
	bButtonSizer->Add( m_acceptSelectedBtn, 0, wxALL, 5 );


	bMainSizer->Add( bButtonSizer, 0, wxEXPAND|wxLEFT|wxRIGHT, 10 );

	m_sdbSizer = new wxStdDialogButtonSizer();
	m_sdbSizerCancel = new wxButton( this, wxID_CANCEL );
	m_sdbSizer->AddButton( m_sdbSizerCancel );
	m_sdbSizer->Realize();

	bMainSizer->Add( m_sdbSizer, 0, wxALL|wxEXPAND, 5 );


	this->SetSizer( bMainSizer );
	this->Layout();
	bMainSizer->Fit( this );

	this->Centre( wxBOTH );

	// Connect Events
	m_bomList->Connect( wxEVT_COMMAND_DATAVIEW_SELECTION_CHANGED, wxDataViewEventHandler( DIALOG_PLM_BOM_VALIDATION_BASE::onBomItemSelected ), NULL, this );
	m_acceptAllBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_BOM_VALIDATION_BASE::onAcceptAll ), NULL, this );
	m_acceptSelectedBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_BOM_VALIDATION_BASE::onAcceptSelected ), NULL, this );
}

DIALOG_PLM_BOM_VALIDATION_BASE::~DIALOG_PLM_BOM_VALIDATION_BASE()
{
	// Disconnect Events
	m_bomList->Disconnect( wxEVT_COMMAND_DATAVIEW_SELECTION_CHANGED, wxDataViewEventHandler( DIALOG_PLM_BOM_VALIDATION_BASE::onBomItemSelected ), NULL, this );
	m_acceptAllBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_BOM_VALIDATION_BASE::onAcceptAll ), NULL, this );
	m_acceptSelectedBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_BOM_VALIDATION_BASE::onAcceptSelected ), NULL, this );

}
