///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_idx_review_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG__IDX_REVIEW_BASE::DIALOG__IDX_REVIEW_BASE( wxWindow* parent, wxWindowID id, const wxString& title,
                                                  const wxPoint& pos, const wxSize& size, long style ) :
        wxDialog( parent, id, title, pos, size, style )
{
    this->SetSizeHints( wxDefaultSize, wxDefaultSize );

    wxBoxSizer* bSizer1;
    bSizer1 = new wxBoxSizer( wxVERTICAL );

    m_splitter2 = new wxSplitterWindow( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D );
    m_splitter2->Connect( wxEVT_IDLE, wxIdleEventHandler( DIALOG__IDX_REVIEW_BASE::m_splitter2OnIdle ), NULL, this );

    m_panel1 = new wxPanel( m_splitter2, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panel1->SetMinSize( wxSize( 300, -1 ) );

    wxBoxSizer* bSizer5;
    bSizer5 = new wxBoxSizer( wxVERTICAL );

    m_splitter21 = new wxSplitterWindow( m_panel1, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D );
    m_splitter21->Connect( wxEVT_IDLE, wxIdleEventHandler( DIALOG__IDX_REVIEW_BASE::m_splitter21OnIdle ), NULL, this );

    m_panel41 = new wxPanel( m_splitter21, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    wxBoxSizer* bSizer8;
    bSizer8 = new wxBoxSizer( wxVERTICAL );

    m_panelPreview = new PCB_DRAW_PANEL_GAL( this, -1, wxPoint( 0, 0 ), wxDefaultSize, m_galOptions,
                                             EDA_DRAW_PANEL_GAL::GAL_TYPE_CAIRO );
    m_panelPreview->SetMinSize( wxSize( 640, 280 ) );

    bSizer8->Add( m_panelPreview, 1, wxALL | wxEXPAND, 5 );


    m_panel41->SetSizer( bSizer8 );
    m_panel41->Layout();
    bSizer8->Fit( m_panel41 );
    m_panel2 = new wxPanel( m_splitter21, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    wxBoxSizer* bSizer15;
    bSizer15 = new wxBoxSizer( wxVERTICAL );

    m_changeGrid = new wxGrid( m_panel2, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );

    // Grid
    m_changeGrid->CreateGrid( 0, 0 );
    m_changeGrid->EnableEditing( true );
    m_changeGrid->EnableGridLines( true );
    m_changeGrid->EnableDragGridSize( false );
    m_changeGrid->SetMargins( 0, 0 );

    // Columns
    m_changeGrid->AutoSizeColumns();
    m_changeGrid->EnableDragColMove( false );
    m_changeGrid->EnableDragColSize( true );
    m_changeGrid->SetColLabelAlignment( wxALIGN_CENTER, wxALIGN_CENTER );

    // Rows
    m_changeGrid->EnableDragRowSize( true );
    m_changeGrid->SetRowLabelAlignment( wxALIGN_CENTER, wxALIGN_CENTER );

    // Label Appearance

    // Cell Defaults
    m_changeGrid->SetDefaultCellAlignment( wxALIGN_LEFT, wxALIGN_TOP );
    bSizer15->Add( m_changeGrid, 1, wxALL | wxEXPAND, 5 );


    m_panel2->SetSizer( bSizer15 );
    m_panel2->Layout();
    bSizer15->Fit( m_panel2 );
    m_splitter21->SplitHorizontally( m_panel41, m_panel2, 0 );
    bSizer5->Add( m_splitter21, 1, wxEXPAND, 5 );

    wxBoxSizer* bSizer3;
    bSizer3 = new wxBoxSizer( wxHORIZONTAL );

    m_staticText1 = new wxStaticText(
            m_panel1, wxID_ANY,
            _( "To avoid desynchronization, do not rollback / undo changes after applying the IDX change" ),
            wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText1->Wrap( -1 );
    bSizer3->Add( m_staticText1, 1, wxALIGN_CENTER | wxALL, 5 );

    m_acceptAll = new wxToggleButton( m_panel1, wxID_ANY, _( "Accept all" ), wxDefaultPosition, wxDefaultSize, 0 );
    bSizer3->Add( m_acceptAll, 0, wxALL, 5 );

    m_rejectAll = new wxToggleButton( m_panel1, wxID_ANY, _( "Reject All" ), wxDefaultPosition, wxDefaultSize, 0 );

    m_rejectAll->SetBitmap( wxNullBitmap );
    bSizer3->Add( m_rejectAll, 0, wxALL, 5 );

    m_sdbSizer1 = new wxStdDialogButtonSizer();
    m_sdbSizer1Apply = new wxButton( m_panel1, wxID_APPLY );
    m_sdbSizer1->AddButton( m_sdbSizer1Apply );
    m_sdbSizer1Cancel = new wxButton( m_panel1, wxID_CANCEL );
    m_sdbSizer1->AddButton( m_sdbSizer1Cancel );
    m_sdbSizer1->Realize();

    bSizer3->Add( m_sdbSizer1, 1, wxEXPAND, 5 );


    bSizer5->Add( bSizer3, 0, wxEXPAND, 5 );


    m_panel1->SetSizer( bSizer5 );
    m_panel1->Layout();
    bSizer5->Fit( m_panel1 );
    m_panel4 = new wxPanel( m_splitter2, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panel4->SetMinSize( wxSize( 200, -1 ) );

    wxBoxSizer* bSizer7;
    bSizer7 = new wxBoxSizer( wxVERTICAL );

    m_staticText3 =
            new wxStaticText( m_panel4, wxID_ANY, _( "Collaboration Folder:" ), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText3->Wrap( -1 );
    bSizer7->Add( m_staticText3, 0, wxALL, 5 );

    wxBoxSizer* bSizer16;
    bSizer16 = new wxBoxSizer( wxHORIZONTAL );

    m_collaborationFolderInput =
            new wxTextCtrl( m_panel4, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer16->Add( m_collaborationFolderInput, 1, wxALL | wxEXPAND, 5 );

    m_bmToggleBtn1 = new wxBitmapToggleButton( m_panel4, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer16->Add( m_bmToggleBtn1, 0, wxALL, 5 );


    bSizer7->Add( bSizer16, 0, wxEXPAND, 5 );

    m_button1 = new wxButton( m_panel4, wxID_ANY, _( "Configure IDX..." ), wxDefaultPosition, wxDefaultSize, 0 );
    bSizer7->Add( m_button1, 0, wxALL, 5 );

    m_staticline1 = new wxStaticLine( m_panel4, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer7->Add( m_staticline1, 0, wxEXPAND | wxALL, 5 );

    m_staticText4 = new wxStaticText( m_panel4, wxID_ANY, _( "IDX history:" ), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText4->Wrap( -1 );
    bSizer7->Add( m_staticText4, 0, wxALL, 5 );

    m_history = new wxDataViewListCtrl( m_panel4, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES );
    bSizer7->Add( m_history, 1, wxALL | wxEXPAND, 5 );


    m_panel4->SetSizer( bSizer7 );
    m_panel4->Layout();
    bSizer7->Fit( m_panel4 );
    m_splitter2->SplitVertically( m_panel1, m_panel4, 380 );
    bSizer1->Add( m_splitter2, 1, wxEXPAND, 5 );


    this->SetSizer( bSizer1 );
    this->Layout();

    this->Centre( wxBOTH );

    // Connect Events
    m_changeGrid->Connect( wxEVT_GRID_CELL_LEFT_CLICK, wxGridEventHandler( DIALOG__IDX_REVIEW_BASE::OnGridCellClick ),
                           NULL, this );
    m_acceptAll->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED,
                          wxCommandEventHandler( DIALOG__IDX_REVIEW_BASE::OnAcceptAll ), NULL, this );
    m_rejectAll->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED,
                          wxCommandEventHandler( DIALOG__IDX_REVIEW_BASE::OnRejectAll ), NULL, this );
    m_sdbSizer1Apply->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG__IDX_REVIEW_BASE::OnApply ),
                               NULL, this );
    m_sdbSizer1Cancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                                wxCommandEventHandler( DIALOG__IDX_REVIEW_BASE::OnCancel ), NULL, this );
    m_collaborationFolderInput->Connect( wxEVT_COMMAND_TEXT_UPDATED,
                                         wxCommandEventHandler( DIALOG__IDX_REVIEW_BASE::OnCollaborationFolderText ),
                                         NULL, this );
    m_bmToggleBtn1->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED,
                             wxCommandEventHandler( DIALOG__IDX_REVIEW_BASE::onBrowseButton ), NULL, this );
    m_history->Connect( wxEVT_COMMAND_DATAVIEW_ITEM_ACTIVATED,
                        wxDataViewEventHandler( DIALOG__IDX_REVIEW_BASE::onItemActivated ), NULL, this );
}

DIALOG__IDX_REVIEW_BASE::~DIALOG__IDX_REVIEW_BASE()
{
}
