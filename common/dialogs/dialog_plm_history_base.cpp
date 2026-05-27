///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6a-dirty)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_plm_history_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_PLM_HISTORY_BASE::DIALOG_PLM_HISTORY_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 800,500 ), wxDefaultSize );

	wxBoxSizer* bMainSizer;
	bMainSizer = new wxBoxSizer( wxVERTICAL );

	m_splitter = new wxSplitterWindow( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_LIVE_UPDATE );
	m_splitter->Connect( wxEVT_IDLE, wxIdleEventHandler( DIALOG_PLM_HISTORY_BASE::m_splitterOnIdle ), NULL, this );
	m_splitter->SetMinimumPaneSize( 250 );

	m_revisionListPanel = new wxPanel( m_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bRevisionListSizer;
	bRevisionListSizer = new wxBoxSizer( wxVERTICAL );

	m_revisionListLabel = new wxStaticText( m_revisionListPanel, wxID_ANY, _("Revisions:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_revisionListLabel->Wrap( -1 );
	bRevisionListSizer->Add( m_revisionListLabel, 0, wxEXPAND|wxALL, 5 );

	m_revisionList = new wxDataViewListCtrl( m_revisionListPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES|wxDV_VERT_RULES|wxDV_MULTIPLE );
	bRevisionListSizer->Add( m_revisionList, 1, wxEXPAND|wxALL, 5 );


	m_revisionListPanel->SetSizer( bRevisionListSizer );
	m_revisionListPanel->Layout();
	bRevisionListSizer->Fit( m_revisionListPanel );
	m_detailsPanel = new wxPanel( m_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bDetailsSizer;
	bDetailsSizer = new wxBoxSizer( wxVERTICAL );

	m_detailsLabel = new wxStaticText( m_detailsPanel, wxID_ANY, _("Revision Details:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_detailsLabel->Wrap( -1 );
	bDetailsSizer->Add( m_detailsLabel, 0, wxEXPAND|wxALL, 5 );

	wxFlexGridSizer* fgDetailsSizer;
	fgDetailsSizer = new wxFlexGridSizer( 3, 2, 0, 0 );
	fgDetailsSizer->AddGrowableCol( 1 );
	fgDetailsSizer->SetFlexibleDirection( wxBOTH );
	fgDetailsSizer->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	m_revisionIdLabel = new wxStaticText( m_detailsPanel, wxID_ANY, _("Revision:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_revisionIdLabel->Wrap( -1 );
	fgDetailsSizer->Add( m_revisionIdLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_revisionIdValue = new wxStaticText( m_detailsPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_revisionIdValue->Wrap( -1 );
	fgDetailsSizer->Add( m_revisionIdValue, 0, wxALIGN_CENTER_VERTICAL|wxALL|wxEXPAND, 5 );

	m_authorLabel = new wxStaticText( m_detailsPanel, wxID_ANY, _("Author:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_authorLabel->Wrap( -1 );
	fgDetailsSizer->Add( m_authorLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_authorValue = new wxStaticText( m_detailsPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_authorValue->Wrap( -1 );
	fgDetailsSizer->Add( m_authorValue, 0, wxALIGN_CENTER_VERTICAL|wxALL|wxEXPAND, 5 );

	m_dateLabel = new wxStaticText( m_detailsPanel, wxID_ANY, _("Date:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_dateLabel->Wrap( -1 );
	fgDetailsSizer->Add( m_dateLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_dateValue = new wxStaticText( m_detailsPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_dateValue->Wrap( -1 );
	fgDetailsSizer->Add( m_dateValue, 0, wxALIGN_CENTER_VERTICAL|wxALL|wxEXPAND, 5 );


	bDetailsSizer->Add( fgDetailsSizer, 0, wxEXPAND|wxLEFT|wxRIGHT, 5 );

	m_commentLabel = new wxStaticText( m_detailsPanel, wxID_ANY, _("Comment:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_commentLabel->Wrap( -1 );
	bDetailsSizer->Add( m_commentLabel, 0, wxEXPAND|wxALL, 5 );

	m_commentText = new wxTextCtrl( m_detailsPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_READONLY|wxTE_WORDWRAP );
	bDetailsSizer->Add( m_commentText, 1, wxEXPAND|wxALL, 5 );


	m_detailsPanel->SetSizer( bDetailsSizer );
	m_detailsPanel->Layout();
	bDetailsSizer->Fit( m_detailsPanel );
	m_splitter->SplitVertically( m_revisionListPanel, m_detailsPanel, 0 );
	bMainSizer->Add( m_splitter, 1, wxEXPAND|wxALL, 5 );

	bButtonSizer = new wxBoxSizer( wxHORIZONTAL );

	m_compareBtn = new wxButton( this, wxID_ANY, _("Compare Selected"), wxDefaultPosition, wxDefaultSize, 0 );
	m_compareBtn->Enable( false );

	bButtonSizer->Add( m_compareBtn, 0, wxALL, 5 );

	m_downloadBtn = new wxButton( this, wxID_ANY, _("Download Revision"), wxDefaultPosition, wxDefaultSize, 0 );
	m_downloadBtn->Enable( false );

	bButtonSizer->Add( m_downloadBtn, 0, wxALL, 5 );


	bButtonSizer->Add( 0, 0, 1, wxEXPAND, 5 );

	m_closeBtn = new wxButton( this, wxID_CLOSE, _("Close"), wxDefaultPosition, wxDefaultSize, 0 );
	bButtonSizer->Add( m_closeBtn, 0, wxALL, 5 );


	bMainSizer->Add( bButtonSizer, 0, wxEXPAND|wxALL, 5 );


	this->SetSizer( bMainSizer );
	this->Layout();
	bMainSizer->Fit( this );

	this->Centre( wxBOTH );

	// Connect Events
	m_revisionList->Connect( wxEVT_COMMAND_DATAVIEW_SELECTION_CHANGED, wxDataViewEventHandler( DIALOG_PLM_HISTORY_BASE::onRevisionSelected ), NULL, this );
	m_compareBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_HISTORY_BASE::onCompare ), NULL, this );
	m_downloadBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_HISTORY_BASE::onDownload ), NULL, this );
	m_closeBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_HISTORY_BASE::onClose ), NULL, this );
}

DIALOG_PLM_HISTORY_BASE::~DIALOG_PLM_HISTORY_BASE()
{
	// Disconnect Events
	m_revisionList->Disconnect( wxEVT_COMMAND_DATAVIEW_SELECTION_CHANGED, wxDataViewEventHandler( DIALOG_PLM_HISTORY_BASE::onRevisionSelected ), NULL, this );
	m_compareBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_HISTORY_BASE::onCompare ), NULL, this );
	m_downloadBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_HISTORY_BASE::onDownload ), NULL, this );
	m_closeBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_HISTORY_BASE::onClose ), NULL, this );

}
