///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6a-dirty)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_plm_release_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_PLM_RELEASE_BASE::DIALOG_PLM_RELEASE_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 700,550 ), wxDefaultSize );

	wxBoxSizer* bMainSizer;
	bMainSizer = new wxBoxSizer( wxVERTICAL );

	m_wizard = new wxSimplebook( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );

	// Page 1 - Release Info
	m_pageInfo = new wxPanel( m_wizard, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bInfoSizer;
	bInfoSizer = new wxBoxSizer( wxVERTICAL );

	wxFlexGridSizer* fgInfoSizer;
	fgInfoSizer = new wxFlexGridSizer( 0, 2, 5, 0 );
	fgInfoSizer->AddGrowableCol( 1 );
	fgInfoSizer->SetFlexibleDirection( wxBOTH );
	fgInfoSizer->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	m_versionLabel = new wxStaticText( m_pageInfo, wxID_ANY, _("Version:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_versionLabel->Wrap( -1 );
	fgInfoSizer->Add( m_versionLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_versionCtrl = new wxTextCtrl( m_pageInfo, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	fgInfoSizer->Add( m_versionCtrl, 0, wxALL|wxEXPAND, 5 );

	m_schemeLabel = new wxStaticText( m_pageInfo, wxID_ANY, _("Version Scheme:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_schemeLabel->Wrap( -1 );
	fgInfoSizer->Add( m_schemeLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	wxArrayString m_schemeChoiceChoices;
	m_schemeChoice = new wxChoice( m_pageInfo, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_schemeChoiceChoices, 0 );
	m_schemeChoice->SetSelection( 0 );
	fgInfoSizer->Add( m_schemeChoice, 0, wxALL|wxEXPAND, 5 );

	m_titleLabel = new wxStaticText( m_pageInfo, wxID_ANY, _("Title:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_titleLabel->Wrap( -1 );
	fgInfoSizer->Add( m_titleLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_titleCtrl = new wxTextCtrl( m_pageInfo, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	fgInfoSizer->Add( m_titleCtrl, 0, wxALL|wxEXPAND, 5 );

	bInfoSizer->Add( fgInfoSizer, 0, wxEXPAND|wxALL, 5 );

	m_notesLabel = new wxStaticText( m_pageInfo, wxID_ANY, _("Release Notes:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_notesLabel->Wrap( -1 );
	bInfoSizer->Add( m_notesLabel, 0, wxLEFT|wxRIGHT|wxTOP, 10 );

	m_notesCtrl = new wxTextCtrl( m_pageInfo, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,150 ), wxTE_MULTILINE|wxTE_WORDWRAP );
	bInfoSizer->Add( m_notesCtrl, 1, wxEXPAND|wxALL, 10 );

	m_pageInfo->SetSizer( bInfoSizer );
	m_pageInfo->Layout();
	bInfoSizer->Fit( m_pageInfo );
	m_wizard->AddPage( m_pageInfo, wxEmptyString, true );

	// Page 2 - Validation Checks
	m_pageChecks = new wxPanel( m_wizard, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bChecksSizer;
	bChecksSizer = new wxBoxSizer( wxVERTICAL );

	m_checksLabel = new wxStaticText( m_pageChecks, wxID_ANY, _("Pre-release validation checks:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_checksLabel->Wrap( -1 );
	bChecksSizer->Add( m_checksLabel, 0, wxEXPAND|wxALL, 10 );

	m_checksList = new wxDataViewListCtrl( m_pageChecks, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES|wxDV_VERT_RULES );
	bChecksSizer->Add( m_checksList, 1, wxEXPAND|wxALL, 10 );

	m_pageChecks->SetSizer( bChecksSizer );
	m_pageChecks->Layout();
	bChecksSizer->Fit( m_pageChecks );
	m_wizard->AddPage( m_pageChecks, wxEmptyString, false );

	// Page 3 - Manufacturing Outputs
	m_pageOutputs = new wxPanel( m_wizard, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bOutputsSizer;
	bOutputsSizer = new wxBoxSizer( wxVERTICAL );

	m_outputsLabel = new wxStaticText( m_pageOutputs, wxID_ANY, _("Select manufacturing outputs to include:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_outputsLabel->Wrap( -1 );
	bOutputsSizer->Add( m_outputsLabel, 0, wxEXPAND|wxALL, 10 );

	wxBoxSizer* bCheckboxSizer;
	bCheckboxSizer = new wxBoxSizer( wxVERTICAL );

	m_chkGerber = new wxCheckBox( m_pageOutputs, wxID_ANY, _("Gerber fabrication files"), wxDefaultPosition, wxDefaultSize, 0 );
	m_chkGerber->SetValue( true );
	bCheckboxSizer->Add( m_chkGerber, 0, wxALL, 5 );

	m_chkDrill = new wxCheckBox( m_pageOutputs, wxID_ANY, _("Drill files"), wxDefaultPosition, wxDefaultSize, 0 );
	m_chkDrill->SetValue( true );
	bCheckboxSizer->Add( m_chkDrill, 0, wxALL, 5 );

	m_chkBom = new wxCheckBox( m_pageOutputs, wxID_ANY, _("Bill of Materials (BOM)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_chkBom->SetValue( true );
	bCheckboxSizer->Add( m_chkBom, 0, wxALL, 5 );

	m_chkPnP = new wxCheckBox( m_pageOutputs, wxID_ANY, _("Pick and Place data"), wxDefaultPosition, wxDefaultSize, 0 );
	m_chkPnP->SetValue( true );
	bCheckboxSizer->Add( m_chkPnP, 0, wxALL, 5 );

	m_chkPdf = new wxCheckBox( m_pageOutputs, wxID_ANY, _("PDF documentation"), wxDefaultPosition, wxDefaultSize, 0 );
	bCheckboxSizer->Add( m_chkPdf, 0, wxALL, 5 );

	m_chk3D = new wxCheckBox( m_pageOutputs, wxID_ANY, _("3D model export"), wxDefaultPosition, wxDefaultSize, 0 );
	bCheckboxSizer->Add( m_chk3D, 0, wxALL, 5 );

	bOutputsSizer->Add( bCheckboxSizer, 0, wxEXPAND|wxLEFT, 20 );

	m_pageOutputs->SetSizer( bOutputsSizer );
	m_pageOutputs->Layout();
	bOutputsSizer->Fit( m_pageOutputs );
	m_wizard->AddPage( m_pageOutputs, wxEmptyString, false );

	// Page 4 - Summary
	m_pageSummary = new wxPanel( m_wizard, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSummarySizer;
	bSummarySizer = new wxBoxSizer( wxVERTICAL );

	m_summaryLabel = new wxStaticText( m_pageSummary, wxID_ANY, _("Release summary:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_summaryLabel->Wrap( -1 );
	bSummarySizer->Add( m_summaryLabel, 0, wxEXPAND|wxALL, 10 );

	m_summaryText = new wxTextCtrl( m_pageSummary, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_READONLY|wxTE_WORDWRAP );
	bSummarySizer->Add( m_summaryText, 1, wxEXPAND|wxALL, 10 );

	m_pageSummary->SetSizer( bSummarySizer );
	m_pageSummary->Layout();
	bSummarySizer->Fit( m_pageSummary );
	m_wizard->AddPage( m_pageSummary, wxEmptyString, false );

	bMainSizer->Add( m_wizard, 1, wxEXPAND|wxALL, 5 );

	wxBoxSizer* bButtonSizer;
	bButtonSizer = new wxBoxSizer( wxHORIZONTAL );

	m_backBtn = new wxButton( this, wxID_ANY, _("< Back"), wxDefaultPosition, wxDefaultSize, 0 );
	m_backBtn->Enable( false );
	bButtonSizer->Add( m_backBtn, 0, wxALL, 5 );

	m_nextBtn = new wxButton( this, wxID_ANY, _("Next >"), wxDefaultPosition, wxDefaultSize, 0 );
	bButtonSizer->Add( m_nextBtn, 0, wxALL, 5 );

	bButtonSizer->Add( 0, 0, 1, wxEXPAND, 5 );

	m_cancelBtn = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
	bButtonSizer->Add( m_cancelBtn, 0, wxALL, 5 );

	bMainSizer->Add( bButtonSizer, 0, wxEXPAND|wxALL, 5 );

	this->SetSizer( bMainSizer );
	this->Layout();
	bMainSizer->Fit( this );

	this->Centre( wxBOTH );

	// Connect Events
	m_backBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_RELEASE_BASE::onBack ), NULL, this );
	m_nextBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_RELEASE_BASE::onNext ), NULL, this );
	m_cancelBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_RELEASE_BASE::onCancel ), NULL, this );
}

DIALOG_PLM_RELEASE_BASE::~DIALOG_PLM_RELEASE_BASE()
{
	// Disconnect Events
	m_backBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_RELEASE_BASE::onBack ), NULL, this );
	m_nextBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_RELEASE_BASE::onNext ), NULL, this );
	m_cancelBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_RELEASE_BASE::onCancel ), NULL, this );

}
