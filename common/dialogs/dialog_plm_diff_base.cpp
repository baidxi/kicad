///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6a-dirty)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_plm_diff_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_PLM_DIFF_BASE::DIALOG_PLM_DIFF_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 900,600 ), wxDefaultSize );

	wxBoxSizer* bMainSizer;
	bMainSizer = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* bToolbarSizer;
	bToolbarSizer = new wxBoxSizer( wxHORIZONTAL );

	m_comparisonLabel = new wxStaticText( this, wxID_ANY, _("Comparing:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_comparisonLabel->Wrap( -1 );
	bToolbarSizer->Add( m_comparisonLabel, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );


	bToolbarSizer->Add( 0, 0, 1, wxEXPAND, 5 );

	wxStaticText* m_staticText1;
	m_staticText1 = new wxStaticText( this, wxID_ANY, _("View Mode:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText1->Wrap( -1 );
	bToolbarSizer->Add( m_staticText1, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

	wxString m_viewModeChoiceChoices[] = { _("Side by Side"), _("Overlay"), _("Highlight Changes") };
	int m_viewModeChoiceNChoices = sizeof( m_viewModeChoiceChoices ) / sizeof( wxString );
	m_viewModeChoice = new wxChoice( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_viewModeChoiceNChoices, m_viewModeChoiceChoices, 0 );
	m_viewModeChoice->SetSelection( 0 );
	bToolbarSizer->Add( m_viewModeChoice, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

	m_prevChangeBtn = new wxButton( this, wxID_ANY, _("Previous Change"), wxDefaultPosition, wxDefaultSize, 0 );
	bToolbarSizer->Add( m_prevChangeBtn, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

	m_nextChangeBtn = new wxButton( this, wxID_ANY, _("Next Change"), wxDefaultPosition, wxDefaultSize, 0 );
	bToolbarSizer->Add( m_nextChangeBtn, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

	m_changeCountLabel = new wxStaticText( this, wxID_ANY, _("Change 0 of 0"), wxDefaultPosition, wxDefaultSize, 0 );
	m_changeCountLabel->Wrap( -1 );
	bToolbarSizer->Add( m_changeCountLabel, 0, wxALIGN_CENTER_VERTICAL, 5 );


	bMainSizer->Add( bToolbarSizer, 0, wxEXPAND|wxALL, 10 );

	m_splitter = new wxSplitterWindow( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D );
	m_splitter->Connect( wxEVT_IDLE, wxIdleEventHandler( DIALOG_PLM_DIFF_BASE::m_splitterOnIdle ), NULL, this );
	m_splitter->SetMinimumPaneSize( 200 );

	m_leftPanel = new wxPanel( m_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bLeftPanelSizer;
	bLeftPanelSizer = new wxBoxSizer( wxVERTICAL );

	m_leftRevisionLabel = new wxStaticText( m_leftPanel, wxID_ANY, _("Left Revision"), wxDefaultPosition, wxDefaultSize, 0 );
	m_leftRevisionLabel->Wrap( -1 );
	bLeftPanelSizer->Add( m_leftRevisionLabel, 0, wxALL|wxALIGN_CENTER, 5 );


	m_leftPanel->SetSizer( bLeftPanelSizer );
	m_leftPanel->Layout();
	bLeftPanelSizer->Fit( m_leftPanel );
	m_rightPanel = new wxPanel( m_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bRightPanelSizer;
	bRightPanelSizer = new wxBoxSizer( wxVERTICAL );

	m_rightRevisionLabel = new wxStaticText( m_rightPanel, wxID_ANY, _("Right Revision"), wxDefaultPosition, wxDefaultSize, 0 );
	m_rightRevisionLabel->Wrap( -1 );
	bRightPanelSizer->Add( m_rightRevisionLabel, 0, wxALL|wxALIGN_CENTER, 5 );


	m_rightPanel->SetSizer( bRightPanelSizer );
	m_rightPanel->Layout();
	bRightPanelSizer->Fit( m_rightPanel );
	m_splitter->SplitVertically( m_leftPanel, m_rightPanel, 0 );
	bMainSizer->Add( m_splitter, 1, wxEXPAND|wxLEFT|wxRIGHT, 5 );

	wxBoxSizer* bLegendSizer;
	bLegendSizer = new wxBoxSizer( wxHORIZONTAL );

	m_addedLabel = new wxStaticText( this, wxID_ANY, _("Added"), wxDefaultPosition, wxDefaultSize, 0 );
	m_addedLabel->Wrap( -1 );
	m_addedLabel->SetForegroundColour( wxColour( 0, 128, 0 ) );

	bLegendSizer->Add( m_addedLabel, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

	m_removedLabel = new wxStaticText( this, wxID_ANY, _("Removed"), wxDefaultPosition, wxDefaultSize, 0 );
	m_removedLabel->Wrap( -1 );
	m_removedLabel->SetForegroundColour( wxColour( 192, 0, 0 ) );

	bLegendSizer->Add( m_removedLabel, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

	m_modifiedLabel = new wxStaticText( this, wxID_ANY, _("Modified"), wxDefaultPosition, wxDefaultSize, 0 );
	m_modifiedLabel->Wrap( -1 );
	m_modifiedLabel->SetForegroundColour( wxColour( 192, 192, 0 ) );

	bLegendSizer->Add( m_modifiedLabel, 0, wxALIGN_CENTER_VERTICAL, 5 );


	bMainSizer->Add( bLegendSizer, 0, wxEXPAND|wxLEFT|wxRIGHT, 10 );

	m_sdbSizer = new wxStdDialogButtonSizer();
	m_sdbSizerCancel = new wxButton( this, wxID_CANCEL );
	m_sdbSizer->AddButton( m_sdbSizerCancel );
	m_sdbSizer->Realize();

	bMainSizer->Add( m_sdbSizer, 0, wxEXPAND|wxALL, 5 );


	this->SetSizer( bMainSizer );
	this->Layout();
	bMainSizer->Fit( this );

	this->Centre( wxBOTH );

	// Connect Events
	m_viewModeChoice->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( DIALOG_PLM_DIFF_BASE::OnViewModeChanged ), NULL, this );
	m_prevChangeBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_DIFF_BASE::OnPrevChange ), NULL, this );
	m_nextChangeBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_DIFF_BASE::OnNextChange ), NULL, this );
}

DIALOG_PLM_DIFF_BASE::~DIALOG_PLM_DIFF_BASE()
{
	// Disconnect Events
	m_viewModeChoice->Disconnect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( DIALOG_PLM_DIFF_BASE::OnViewModeChanged ), NULL, this );
	m_prevChangeBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_DIFF_BASE::OnPrevChange ), NULL, this );
	m_nextChangeBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_DIFF_BASE::OnNextChange ), NULL, this );

}
