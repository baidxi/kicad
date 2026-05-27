///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6a-dirty)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_plm_conflict_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_PLM_CONFLICT_BASE::DIALOG_PLM_CONFLICT_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 800,600 ), wxDefaultSize );

	wxBoxSizer* bMainSizer;
	bMainSizer = new wxBoxSizer( wxVERTICAL );

	m_infoLabel = new wxStaticText( this, wxID_ANY, _("Conflicts were detected between your local changes and the server version. Choose which version to keep for each item."), wxDefaultPosition, wxDefaultSize, 0 );
	m_infoLabel->Wrap( -1 );
	bMainSizer->Add( m_infoLabel, 0, wxEXPAND|wxALL, 10 );

	m_mainSplitter = new wxSplitterWindow( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_LIVE_UPDATE );
	m_mainSplitter->Connect( wxEVT_IDLE, wxIdleEventHandler( DIALOG_PLM_CONFLICT_BASE::m_mainSplitterOnIdle ), NULL, this );
	m_mainSplitter->SetMinimumPaneSize( 200 );

	m_conflictListPanel = new wxPanel( m_mainSplitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bConflictListSizer;
	bConflictListSizer = new wxBoxSizer( wxVERTICAL );

	m_conflictListLabel = new wxStaticText( m_conflictListPanel, wxID_ANY, _("Conflicting Items:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_conflictListLabel->Wrap( -1 );
	bConflictListSizer->Add( m_conflictListLabel, 0, wxEXPAND|wxALL, 5 );

	m_conflictList = new wxDataViewListCtrl( m_conflictListPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES|wxDV_VERT_RULES );
	bConflictListSizer->Add( m_conflictList, 1, wxEXPAND|wxALL, 5 );


	m_conflictListPanel->SetSizer( bConflictListSizer );
	m_conflictListPanel->Layout();
	bConflictListSizer->Fit( m_conflictListPanel );
	m_previewPanel = new wxPanel( m_mainSplitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bPreviewSizer;
	bPreviewSizer = new wxBoxSizer( wxVERTICAL );

	m_previewLabel = new wxStaticText( m_previewPanel, wxID_ANY, _("Preview:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_previewLabel->Wrap( -1 );
	bPreviewSizer->Add( m_previewLabel, 0, wxEXPAND|wxALL, 5 );

	m_previewContent = new wxPanel( m_previewPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SUNKEN|wxTAB_TRAVERSAL );
	bPreviewContentSizer = new wxBoxSizer( wxVERTICAL );

	m_previewPlaceholder = new wxStaticText( m_previewContent, wxID_ANY, _("Diff view placeholder - will be implemented in derived class"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL );
	m_previewPlaceholder->Wrap( -1 );
	bPreviewContentSizer->Add( m_previewPlaceholder, 1, wxEXPAND|wxALL, 5 );


	m_previewContent->SetSizer( bPreviewContentSizer );
	m_previewContent->Layout();
	bPreviewContentSizer->Fit( m_previewContent );
	bPreviewSizer->Add( m_previewContent, 1, wxEXPAND|wxALL, 5 );


	m_previewPanel->SetSizer( bPreviewSizer );
	m_previewPanel->Layout();
	bPreviewSizer->Fit( m_previewPanel );
	m_mainSplitter->SplitVertically( m_conflictListPanel, m_previewPanel, 0 );
	bMainSizer->Add( m_mainSplitter, 1, wxEXPAND|wxALL, 5 );

	m_sdbSizer = new wxStdDialogButtonSizer();
	m_sdbSizerOK = new wxButton( this, wxID_OK );
	m_sdbSizer->AddButton( m_sdbSizerOK );
	m_sdbSizerCancel = new wxButton( this, wxID_CANCEL );
	m_sdbSizer->AddButton( m_sdbSizerCancel );
	m_sdbSizer->Realize();

	bMainSizer->Add( m_sdbSizer, 0, wxEXPAND|wxALL, 5 );


	this->SetSizer( bMainSizer );
	this->Layout();
	bMainSizer->Fit( this );

	this->Centre( wxBOTH );

	// Connect Events
	m_conflictList->Connect( wxEVT_COMMAND_DATAVIEW_SELECTION_CHANGED, wxDataViewEventHandler( DIALOG_PLM_CONFLICT_BASE::onConflictSelected ), NULL, this );
}

DIALOG_PLM_CONFLICT_BASE::~DIALOG_PLM_CONFLICT_BASE()
{
	// Disconnect Events
	m_conflictList->Disconnect( wxEVT_COMMAND_DATAVIEW_SELECTION_CHANGED, wxDataViewEventHandler( DIALOG_PLM_CONFLICT_BASE::onConflictSelected ), NULL, this );

}
