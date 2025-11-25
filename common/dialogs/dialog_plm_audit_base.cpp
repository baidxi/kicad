///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6a-dirty)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_plm_audit_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_PLM_AUDIT_BASE::DIALOG_PLM_AUDIT_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 900,600 ), wxDefaultSize );

	wxBoxSizer* bMainSizer;
	bMainSizer = new wxBoxSizer( wxVERTICAL );

	// Filter bar
	wxFlexGridSizer* fgFilterSizer;
	fgFilterSizer = new wxFlexGridSizer( 2, 6, 5, 5 );
	fgFilterSizer->AddGrowableCol( 1 );
	fgFilterSizer->AddGrowableCol( 3 );
	fgFilterSizer->AddGrowableCol( 5 );
	fgFilterSizer->SetFlexibleDirection( wxBOTH );
	fgFilterSizer->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	m_fromLabel = new wxStaticText( this, wxID_ANY, _("From:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_fromLabel->Wrap( -1 );
	fgFilterSizer->Add( m_fromLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_fromDatePicker = new wxDatePickerCtrl( this, wxID_ANY, wxDefaultDateTime, wxDefaultPosition, wxDefaultSize, wxDP_DROPDOWN );
	fgFilterSizer->Add( m_fromDatePicker, 0, wxALL|wxEXPAND, 5 );

	m_toLabel = new wxStaticText( this, wxID_ANY, _("To:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_toLabel->Wrap( -1 );
	fgFilterSizer->Add( m_toLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_toDatePicker = new wxDatePickerCtrl( this, wxID_ANY, wxDefaultDateTime, wxDefaultPosition, wxDefaultSize, wxDP_DROPDOWN );
	fgFilterSizer->Add( m_toDatePicker, 0, wxALL|wxEXPAND, 5 );

	m_userFilterLabel = new wxStaticText( this, wxID_ANY, _("User:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_userFilterLabel->Wrap( -1 );
	fgFilterSizer->Add( m_userFilterLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	wxArrayString m_userFilterChoices;
	m_userFilter = new wxChoice( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_userFilterChoices, 0 );
	m_userFilter->SetSelection( 0 );
	fgFilterSizer->Add( m_userFilter, 0, wxALL|wxEXPAND, 5 );

	m_eventTypeLabel = new wxStaticText( this, wxID_ANY, _("Event Type:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_eventTypeLabel->Wrap( -1 );
	fgFilterSizer->Add( m_eventTypeLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	wxArrayString m_eventTypeFilterChoices;
	m_eventTypeFilter = new wxChoice( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_eventTypeFilterChoices, 0 );
	m_eventTypeFilter->SetSelection( 0 );
	fgFilterSizer->Add( m_eventTypeFilter, 0, wxALL|wxEXPAND, 5 );

	m_projectFilterLabel = new wxStaticText( this, wxID_ANY, _("Project:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_projectFilterLabel->Wrap( -1 );
	fgFilterSizer->Add( m_projectFilterLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	wxArrayString m_projectFilterChoices;
	m_projectFilter = new wxChoice( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_projectFilterChoices, 0 );
	m_projectFilter->SetSelection( 0 );
	fgFilterSizer->Add( m_projectFilter, 0, wxALL|wxEXPAND, 5 );

	// Empty cells to fill the grid
	fgFilterSizer->Add( 0, 0, 1, wxEXPAND, 5 );
	fgFilterSizer->Add( 0, 0, 1, wxEXPAND, 5 );

	bMainSizer->Add( fgFilterSizer, 0, wxEXPAND|wxALL, 5 );

	// Main event list
	m_eventList = new wxDataViewListCtrl( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES|wxDV_VERT_RULES|wxDV_SINGLE );
	bMainSizer->Add( m_eventList, 1, wxEXPAND|wxALL, 5 );

	// Detail panel (collapsible)
	m_detailPane = new wxCollapsiblePane( this, wxID_ANY, _("Event Details"), wxDefaultPosition, wxDefaultSize, wxCP_DEFAULT_STYLE );
	m_detailPane->Collapse( true );

	wxBoxSizer* bDetailSizer;
	bDetailSizer = new wxBoxSizer( wxVERTICAL );

	m_detailText = new wxTextCtrl( m_detailPane->GetPane(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,120 ), wxTE_MULTILINE|wxTE_READONLY|wxTE_WORDWRAP );
	bDetailSizer->Add( m_detailText, 1, wxEXPAND|wxALL, 5 );

	m_detailPane->GetPane()->SetSizer( bDetailSizer );
	m_detailPane->GetPane()->Layout();
	bDetailSizer->Fit( m_detailPane->GetPane() );
	bMainSizer->Add( m_detailPane, 0, wxEXPAND|wxALL, 5 );

	// Bottom bar
	wxBoxSizer* bBottomSizer;
	bBottomSizer = new wxBoxSizer( wxHORIZONTAL );

	m_refreshBtn = new wxButton( this, wxID_ANY, _("Refresh"), wxDefaultPosition, wxDefaultSize, 0 );
	bBottomSizer->Add( m_refreshBtn, 0, wxALL, 5 );

	m_exportCsvBtn = new wxButton( this, wxID_ANY, _("Export CSV"), wxDefaultPosition, wxDefaultSize, 0 );
	bBottomSizer->Add( m_exportCsvBtn, 0, wxALL, 5 );

	bBottomSizer->Add( 0, 0, 1, wxEXPAND, 5 );

	m_recordCountLabel = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_recordCountLabel->Wrap( -1 );
	bBottomSizer->Add( m_recordCountLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	bBottomSizer->Add( 0, 0, 1, wxEXPAND, 5 );

	m_closeBtn = new wxButton( this, wxID_CLOSE, _("Close"), wxDefaultPosition, wxDefaultSize, 0 );
	bBottomSizer->Add( m_closeBtn, 0, wxALL, 5 );

	bMainSizer->Add( bBottomSizer, 0, wxEXPAND|wxALL, 5 );

	this->SetSizer( bMainSizer );
	this->Layout();
	bMainSizer->Fit( this );

	this->Centre( wxBOTH );

	// Connect Events
	m_userFilter->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( DIALOG_PLM_AUDIT_BASE::onFilterChanged ), NULL, this );
	m_eventTypeFilter->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( DIALOG_PLM_AUDIT_BASE::onFilterChanged ), NULL, this );
	m_projectFilter->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( DIALOG_PLM_AUDIT_BASE::onFilterChanged ), NULL, this );
	m_fromDatePicker->Connect( wxEVT_DATE_CHANGED, wxDateEventHandler( DIALOG_PLM_AUDIT_BASE::onDateChanged ), NULL, this );
	m_toDatePicker->Connect( wxEVT_DATE_CHANGED, wxDateEventHandler( DIALOG_PLM_AUDIT_BASE::onDateChanged ), NULL, this );
	m_eventList->Connect( wxEVT_COMMAND_DATAVIEW_SELECTION_CHANGED, wxDataViewEventHandler( DIALOG_PLM_AUDIT_BASE::onEventSelected ), NULL, this );
	m_refreshBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_AUDIT_BASE::onRefresh ), NULL, this );
	m_exportCsvBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_AUDIT_BASE::onExportCsv ), NULL, this );
	m_closeBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_AUDIT_BASE::onClose ), NULL, this );
}

DIALOG_PLM_AUDIT_BASE::~DIALOG_PLM_AUDIT_BASE()
{
	// Disconnect Events
	m_userFilter->Disconnect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( DIALOG_PLM_AUDIT_BASE::onFilterChanged ), NULL, this );
	m_eventTypeFilter->Disconnect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( DIALOG_PLM_AUDIT_BASE::onFilterChanged ), NULL, this );
	m_projectFilter->Disconnect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( DIALOG_PLM_AUDIT_BASE::onFilterChanged ), NULL, this );
	m_fromDatePicker->Disconnect( wxEVT_DATE_CHANGED, wxDateEventHandler( DIALOG_PLM_AUDIT_BASE::onDateChanged ), NULL, this );
	m_toDatePicker->Disconnect( wxEVT_DATE_CHANGED, wxDateEventHandler( DIALOG_PLM_AUDIT_BASE::onDateChanged ), NULL, this );
	m_eventList->Disconnect( wxEVT_COMMAND_DATAVIEW_SELECTION_CHANGED, wxDataViewEventHandler( DIALOG_PLM_AUDIT_BASE::onEventSelected ), NULL, this );
	m_refreshBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_AUDIT_BASE::onRefresh ), NULL, this );
	m_exportCsvBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_AUDIT_BASE::onExportCsv ), NULL, this );
	m_closeBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_AUDIT_BASE::onClose ), NULL, this );

}
