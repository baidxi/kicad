///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_idx_review_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG__IDX_REVIEW_BASE::DIALOG__IDX_REVIEW_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );

	wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer( wxVERTICAL );

	m_panelPreview = new PCB_DRAW_PANEL_GAL( this, -1, wxPoint( 0, 0 ), wxDefaultSize, m_galOptions, EDA_DRAW_PANEL_GAL::GAL_TYPE_CAIRO);
	m_panelPreview->SetMinSize( wxSize( 640,280 ) );

	bSizer1->Add( m_panelPreview, 1, wxALL|wxEXPAND, 5 );

	wxBoxSizer* bSizer2;
	bSizer2 = new wxBoxSizer( wxHORIZONTAL );


	bSizer2->Add( 0, 0, 1, wxEXPAND, 5 );

	m_acceptAll = new wxToggleButton( this, wxID_ANY, _("Accept all"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer2->Add( m_acceptAll, 0, wxALL, 5 );

	m_rejectAll = new wxToggleButton( this, wxID_ANY, _("Reject All"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer2->Add( m_rejectAll, 0, wxALL, 5 );


	bSizer1->Add( bSizer2, 0, wxEXPAND, 5 );

	m_changeGrid = new wxGrid( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );

	// Grid
	m_changeGrid->CreateGrid( 0, 0 );
	m_changeGrid->EnableEditing( true );
	m_changeGrid->EnableGridLines( true );
	m_changeGrid->EnableDragGridSize( false );
	m_changeGrid->SetMargins( 0, 0 );

	// Columns
	m_changeGrid->EnableDragColMove( false );
	m_changeGrid->EnableDragColSize( true );
	m_changeGrid->SetColLabelAlignment( wxALIGN_CENTER, wxALIGN_CENTER );

	// Rows
	m_changeGrid->EnableDragRowSize( true );
	m_changeGrid->SetRowLabelAlignment( wxALIGN_CENTER, wxALIGN_CENTER );

	// Label Appearance

	// Cell Defaults
	m_changeGrid->SetDefaultCellAlignment( wxALIGN_LEFT, wxALIGN_TOP );
	bSizer1->Add( m_changeGrid, 1, wxALL|wxEXPAND, 5 );

	wxBoxSizer* bSizer3;
	bSizer3 = new wxBoxSizer( wxHORIZONTAL );

	m_staticText1 = new wxStaticText( this, wxID_ANY, _("To avoid desynchronization, do not rollback / undo changes after applying the IDX change"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText1->Wrap( -1 );
	bSizer3->Add( m_staticText1, 0, wxALIGN_CENTER|wxALL, 5 );

	m_sdbSizer1 = new wxStdDialogButtonSizer();
	m_sdbSizer1Apply = new wxButton( this, wxID_APPLY );
	m_sdbSizer1->AddButton( m_sdbSizer1Apply );
	m_sdbSizer1Cancel = new wxButton( this, wxID_CANCEL );
	m_sdbSizer1->AddButton( m_sdbSizer1Cancel );
	m_sdbSizer1->Realize();

	bSizer3->Add( m_sdbSizer1, 1, wxEXPAND, 5 );


	bSizer1->Add( bSizer3, 0, wxEXPAND, 5 );


	this->SetSizer( bSizer1 );
	this->Layout();
	bSizer1->Fit( this );

	this->Centre( wxBOTH );

	// Connect Events
	m_acceptAll->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( DIALOG__IDX_REVIEW_BASE::OnAcceptAll ), NULL, this );
	m_rejectAll->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( DIALOG__IDX_REVIEW_BASE::OnRejectAll ), NULL, this );
	m_changeGrid->Connect( wxEVT_GRID_CELL_LEFT_CLICK, wxGridEventHandler( DIALOG__IDX_REVIEW_BASE::OnGridCellClick ), NULL, this );
}

DIALOG__IDX_REVIEW_BASE::~DIALOG__IDX_REVIEW_BASE()
{
}
