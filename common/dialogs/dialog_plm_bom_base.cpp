///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6a-dirty)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_plm_bom_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_PLM_BOM_BASE::DIALOG_PLM_BOM_BASE( wxWindow* parent, wxWindowID id, const wxString& title,
                                            const wxPoint& pos, const wxSize& size,
                                            long style ) :
        DIALOG_SHIM( parent, id, title, pos, size, style )
{
    this->SetSizeHints( wxSize( 800, 500 ), wxDefaultSize );

    bMainSizer = new wxBoxSizer( wxVERTICAL );

    // Toolbar panel
    m_toolbarPanel = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                  wxTAB_TRAVERSAL );
    wxBoxSizer* bToolbarSizer;
    bToolbarSizer = new wxBoxSizer( wxHORIZONTAL );

    m_bomTypeLabel = new wxStaticText( m_toolbarPanel, wxID_ANY, _("BOM Type:"),
                                       wxDefaultPosition, wxDefaultSize, 0 );
    m_bomTypeLabel->Wrap( -1 );
    bToolbarSizer->Add( m_bomTypeLabel, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5 );

    m_bomTypeChoice = new wxChoice( m_toolbarPanel, wxID_ANY, wxDefaultPosition,
                                    wxDefaultSize, 0, NULL, 0 );
    bToolbarSizer->Add( m_bomTypeChoice, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5 );

    bToolbarSizer->Add( 0, 0, 1, wxEXPAND, 5 );

    m_createBomBtn = new wxButton( m_toolbarPanel, wxID_ANY, _("Create BOM"),
                                   wxDefaultPosition, wxDefaultSize, 0 );
    bToolbarSizer->Add( m_createBomBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5 );

    m_deleteBomBtn = new wxButton( m_toolbarPanel, wxID_ANY, _("Delete BOM"),
                                   wxDefaultPosition, wxDefaultSize, 0 );
    bToolbarSizer->Add( m_deleteBomBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5 );

    m_diffBtn = new wxButton( m_toolbarPanel, wxID_ANY, _("Compare..."),
                              wxDefaultPosition, wxDefaultSize, 0 );
    bToolbarSizer->Add( m_diffBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5 );

    m_validateBtn = new wxButton( m_toolbarPanel, wxID_ANY, _("Validate"),
                                  wxDefaultPosition, wxDefaultSize, 0 );
    bToolbarSizer->Add( m_validateBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5 );

    m_toolbarPanel->SetSizer( bToolbarSizer );
    m_toolbarPanel->Layout();
    bToolbarSizer->Fit( m_toolbarPanel );
    bMainSizer->Add( m_toolbarPanel, 0, wxEXPAND | wxALL, 5 );

    // Splitter: BOM list on top, validation on bottom
    m_splitter = new wxSplitterWindow( this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                       wxSP_3D | wxSP_LIVE_UPDATE );
    m_splitter->SetSashGravity( 0.7 );
    m_splitter->Connect( wxEVT_IDLE,
                         wxIdleEventHandler( DIALOG_PLM_BOM_BASE::m_splitterOnIdle ),
                         NULL, this );

    // BOM list panel
    m_listPanel = new wxPanel( m_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                               wxTAB_TRAVERSAL );
    wxBoxSizer* bListSizer;
    bListSizer = new wxBoxSizer( wxVERTICAL );

    m_bomListCtrl = new wxDataViewListCtrl( m_listPanel, wxID_ANY, wxDefaultPosition,
                                            wxDefaultSize, wxDV_ROW_LINES | wxDV_SINGLE );
    bListSizer->Add( m_bomListCtrl, 1, wxEXPAND | wxALL, 5 );

    m_listPanel->SetSizer( bListSizer );
    m_listPanel->Layout();
    bListSizer->Fit( m_listPanel );

    // Validation panel
    m_validationPanel = new wxPanel( m_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                     wxTAB_TRAVERSAL );
    wxBoxSizer* bValidationSizer;
    bValidationSizer = new wxBoxSizer( wxVERTICAL );

    m_validationLabel = new wxStaticText( m_validationPanel, wxID_ANY,
                                          _("Validation Results:"),
                                          wxDefaultPosition, wxDefaultSize, 0 );
    m_validationLabel->Wrap( -1 );
    bValidationSizer->Add( m_validationLabel, 0, wxALL, 5 );

    m_validationListCtrl = new wxDataViewListCtrl( m_validationPanel, wxID_ANY,
                                                    wxDefaultPosition, wxDefaultSize,
                                                    wxDV_ROW_LINES | wxDV_SINGLE );
    bValidationSizer->Add( m_validationListCtrl, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5 );

    m_validationPanel->SetSizer( bValidationSizer );
    m_validationPanel->Layout();
    bValidationSizer->Fit( m_validationPanel );

    m_splitter->SplitHorizontally( m_listPanel, m_validationPanel, -200 );
    bMainSizer->Add( m_splitter, 1, wxEXPAND | wxLEFT | wxRIGHT, 5 );

    // Standard dialog buttons
    m_sdbSizer = new wxStdDialogButtonSizer();
    m_sdbSizerOK = new wxButton( this, wxID_OK );
    m_sdbSizer->AddButton( m_sdbSizerOK );
    m_sdbSizerCancel = new wxButton( this, wxID_CANCEL );
    m_sdbSizer->AddButton( m_sdbSizerCancel );
    m_sdbSizer->Realize();

    bMainSizer->Add( m_sdbSizer, 0, wxEXPAND | wxALL, 10 );

    this->SetSizer( bMainSizer );
    this->Layout();
    bMainSizer->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    m_bomTypeChoice->Connect( wxEVT_COMMAND_CHOICE_SELECTED,
                              wxCommandEventHandler( DIALOG_PLM_BOM_BASE::onBomTypeChanged ),
                              NULL, this );
    m_createBomBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                             wxCommandEventHandler( DIALOG_PLM_BOM_BASE::onCreateBom ),
                             NULL, this );
    m_deleteBomBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                             wxCommandEventHandler( DIALOG_PLM_BOM_BASE::onDeleteBom ),
                             NULL, this );
    m_diffBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                        wxCommandEventHandler( DIALOG_PLM_BOM_BASE::onDiffBom ),
                        NULL, this );
    m_validateBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                            wxCommandEventHandler( DIALOG_PLM_BOM_BASE::onValidateBom ),
                            NULL, this );
    m_bomListCtrl->Connect( wxEVT_DATAVIEW_SELECTION_CHANGED,
                            wxDataViewEventHandler( DIALOG_PLM_BOM_BASE::onBomItemSelected ),
                            NULL, this );
}

DIALOG_PLM_BOM_BASE::~DIALOG_PLM_BOM_BASE()
{
    // Disconnect Events
    m_bomTypeChoice->Disconnect( wxEVT_COMMAND_CHOICE_SELECTED,
                                 wxCommandEventHandler( DIALOG_PLM_BOM_BASE::onBomTypeChanged ),
                                 NULL, this );
    m_createBomBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED,
                                wxCommandEventHandler( DIALOG_PLM_BOM_BASE::onCreateBom ),
                                NULL, this );
    m_deleteBomBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED,
                                wxCommandEventHandler( DIALOG_PLM_BOM_BASE::onDeleteBom ),
                                NULL, this );
    m_diffBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED,
                           wxCommandEventHandler( DIALOG_PLM_BOM_BASE::onDiffBom ),
                           NULL, this );
    m_validateBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED,
                               wxCommandEventHandler( DIALOG_PLM_BOM_BASE::onValidateBom ),
                               NULL, this );
    m_bomListCtrl->Disconnect( wxEVT_DATAVIEW_SELECTION_CHANGED,
                               wxDataViewEventHandler( DIALOG_PLM_BOM_BASE::onBomItemSelected ),
                               NULL, this );
}
