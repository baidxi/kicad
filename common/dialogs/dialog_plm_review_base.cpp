///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6a-dirty)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_plm_review_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_PLM_REVIEW_BASE::DIALOG_PLM_REVIEW_BASE( wxWindow* parent, wxWindowID id,
                                                  const wxString& title, const wxPoint& pos,
                                                  const wxSize& size, long style ) :
        DIALOG_SHIM( parent, id, title, pos, size, style )
{
    this->SetSizeHints( wxSize( 900, 600 ), wxDefaultSize );

    wxBoxSizer* bMainSizer;
    bMainSizer = new wxBoxSizer( wxVERTICAL );

    m_splitter = new wxSplitterWindow( this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                       wxSP_3D | wxSP_LIVE_UPDATE );
    m_splitter->SetSashGravity( 0.3 );
    m_splitter->Connect( wxEVT_IDLE,
                         wxIdleEventHandler( DIALOG_PLM_REVIEW_BASE::m_splitterOnIdle ),
                         NULL, this );

    // Left panel: review list
    m_leftPanel = new wxPanel( m_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                               wxTAB_TRAVERSAL );
    wxBoxSizer* bLeftSizer;
    bLeftSizer = new wxBoxSizer( wxVERTICAL );

    m_reviewListLabel = new wxStaticText( m_leftPanel, wxID_ANY, _("Reviews:"),
                                          wxDefaultPosition, wxDefaultSize, 0 );
    m_reviewListLabel->Wrap( -1 );
    bLeftSizer->Add( m_reviewListLabel, 0, wxALL, 5 );

    m_reviewListCtrl = new wxListCtrl( m_leftPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                       wxLC_REPORT | wxLC_SINGLE_SEL );
    bLeftSizer->Add( m_reviewListCtrl, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5 );

    m_newReviewBtn = new wxButton( m_leftPanel, wxID_ANY, _("New Review..."),
                                   wxDefaultPosition, wxDefaultSize, 0 );
    bLeftSizer->Add( m_newReviewBtn, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5 );

    m_leftPanel->SetSizer( bLeftSizer );
    m_leftPanel->Layout();
    bLeftSizer->Fit( m_leftPanel );

    // Right panel: review details
    m_rightPanel = new wxPanel( m_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                wxTAB_TRAVERSAL );
    bDetailsSizer = new wxBoxSizer( wxVERTICAL );

    // Header fields
    wxFlexGridSizer* fgHeaderSizer;
    fgHeaderSizer = new wxFlexGridSizer( 0, 2, 5, 10 );
    fgHeaderSizer->AddGrowableCol( 1 );
    fgHeaderSizer->SetFlexibleDirection( wxBOTH );
    fgHeaderSizer->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    wxStaticText* titleLbl = new wxStaticText( m_rightPanel, wxID_ANY, _("Title:"),
                                               wxDefaultPosition, wxDefaultSize, 0 );
    titleLbl->Wrap( -1 );
    titleLbl->SetFont( titleLbl->GetFont().Bold() );
    fgHeaderSizer->Add( titleLbl, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );

    m_titleLabel = new wxStaticText( m_rightPanel, wxID_ANY, wxEmptyString,
                                     wxDefaultPosition, wxDefaultSize, 0 );
    m_titleLabel->Wrap( -1 );
    fgHeaderSizer->Add( m_titleLabel, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND );

    wxStaticText* statusLbl = new wxStaticText( m_rightPanel, wxID_ANY, _("Status:"),
                                                wxDefaultPosition, wxDefaultSize, 0 );
    statusLbl->Wrap( -1 );
    fgHeaderSizer->Add( statusLbl, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );

    m_statusLabel = new wxStaticText( m_rightPanel, wxID_ANY, wxEmptyString,
                                      wxDefaultPosition, wxDefaultSize, 0 );
    m_statusLabel->Wrap( -1 );
    fgHeaderSizer->Add( m_statusLabel, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND );

    wxStaticText* authorLbl = new wxStaticText( m_rightPanel, wxID_ANY, _("Author:"),
                                                wxDefaultPosition, wxDefaultSize, 0 );
    authorLbl->Wrap( -1 );
    fgHeaderSizer->Add( authorLbl, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );

    m_authorLabel = new wxStaticText( m_rightPanel, wxID_ANY, wxEmptyString,
                                      wxDefaultPosition, wxDefaultSize, 0 );
    m_authorLabel->Wrap( -1 );
    fgHeaderSizer->Add( m_authorLabel, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND );

    wxStaticText* dateLbl = new wxStaticText( m_rightPanel, wxID_ANY, _("Created:"),
                                              wxDefaultPosition, wxDefaultSize, 0 );
    dateLbl->Wrap( -1 );
    fgHeaderSizer->Add( dateLbl, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );

    m_dateLabel = new wxStaticText( m_rightPanel, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize, 0 );
    m_dateLabel->Wrap( -1 );
    fgHeaderSizer->Add( m_dateLabel, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND );

    bDetailsSizer->Add( fgHeaderSizer, 0, wxEXPAND | wxALL, 10 );

    // Reviewers section
    m_reviewersLabel = new wxStaticText( m_rightPanel, wxID_ANY, _("Reviewers:"),
                                         wxDefaultPosition, wxDefaultSize, 0 );
    m_reviewersLabel->Wrap( -1 );
    m_reviewersLabel->SetFont( m_reviewersLabel->GetFont().Bold() );
    bDetailsSizer->Add( m_reviewersLabel, 0, wxLEFT | wxRIGHT | wxTOP, 10 );

    m_reviewersScrolled = new wxScrolledWindow( m_rightPanel, wxID_ANY, wxDefaultPosition,
                                                wxDefaultSize, wxHSCROLL | wxVSCROLL );
    m_reviewersScrolled->SetScrollRate( 5, 5 );
    m_reviewersScrolled->SetMinSize( wxSize( -1, 80 ) );

    bReviewersSizer = new wxBoxSizer( wxVERTICAL );

    m_reviewersScrolled->SetSizer( bReviewersSizer );
    m_reviewersScrolled->Layout();
    bReviewersSizer->Fit( m_reviewersScrolled );
    bDetailsSizer->Add( m_reviewersScrolled, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10 );

    // Comments section
    m_commentsLabel = new wxStaticText( m_rightPanel, wxID_ANY, _("Comments:"),
                                        wxDefaultPosition, wxDefaultSize, 0 );
    m_commentsLabel->Wrap( -1 );
    m_commentsLabel->SetFont( m_commentsLabel->GetFont().Bold() );
    bDetailsSizer->Add( m_commentsLabel, 0, wxLEFT | wxRIGHT, 10 );

    m_commentsScrolled = new wxScrolledWindow( m_rightPanel, wxID_ANY, wxDefaultPosition,
                                               wxDefaultSize, wxHSCROLL | wxVSCROLL );
    m_commentsScrolled->SetScrollRate( 5, 5 );
    m_commentsScrolled->SetMinSize( wxSize( -1, 120 ) );

    bCommentsSizer = new wxBoxSizer( wxVERTICAL );

    m_commentsScrolled->SetSizer( bCommentsSizer );
    m_commentsScrolled->Layout();
    bCommentsSizer->Fit( m_commentsScrolled );
    bDetailsSizer->Add( m_commentsScrolled, 1, wxEXPAND | wxLEFT | wxRIGHT, 10 );

    // Add comment input
    wxBoxSizer* bCommentInputSizer;
    bCommentInputSizer = new wxBoxSizer( wxHORIZONTAL );

    m_commentInput = new wxTextCtrl( m_rightPanel, wxID_ANY, wxEmptyString,
                                     wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE );
    m_commentInput->SetMinSize( wxSize( -1, 50 ) );
    bCommentInputSizer->Add( m_commentInput, 1, wxEXPAND | wxRIGHT, 5 );

    m_postCommentBtn = new wxButton( m_rightPanel, wxID_ANY, _("Post"),
                                     wxDefaultPosition, wxDefaultSize, 0 );
    bCommentInputSizer->Add( m_postCommentBtn, 0, wxALIGN_BOTTOM );

    bDetailsSizer->Add( bCommentInputSizer, 0, wxEXPAND | wxALL, 10 );

    // Decision buttons
    m_decisionPanel = new wxPanel( m_rightPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   wxTAB_TRAVERSAL );
    wxBoxSizer* bDecisionSizer;
    bDecisionSizer = new wxBoxSizer( wxHORIZONTAL );

    m_approveBtn = new wxButton( m_decisionPanel, wxID_ANY, _("Approve"),
                                 wxDefaultPosition, wxDefaultSize, 0 );
    bDecisionSizer->Add( m_approveBtn, 0, wxRIGHT, 5 );

    m_requestChangesBtn = new wxButton( m_decisionPanel, wxID_ANY, _("Request Changes"),
                                        wxDefaultPosition, wxDefaultSize, 0 );
    bDecisionSizer->Add( m_requestChangesBtn, 0, wxRIGHT, 5 );

    m_rejectBtn = new wxButton( m_decisionPanel, wxID_ANY, _("Reject"),
                                wxDefaultPosition, wxDefaultSize, 0 );
    bDecisionSizer->Add( m_rejectBtn, 0, wxRIGHT, 5 );

    bDecisionSizer->AddStretchSpacer();

    m_mergeBtn = new wxButton( m_decisionPanel, wxID_ANY, _("Merge"),
                               wxDefaultPosition, wxDefaultSize, 0 );
    bDecisionSizer->Add( m_mergeBtn, 0 );

    m_decisionPanel->SetSizer( bDecisionSizer );
    m_decisionPanel->Layout();
    bDecisionSizer->Fit( m_decisionPanel );
    bDetailsSizer->Add( m_decisionPanel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10 );

    m_rightPanel->SetSizer( bDetailsSizer );
    m_rightPanel->Layout();
    bDetailsSizer->Fit( m_rightPanel );

    m_splitter->SplitVertically( m_leftPanel, m_rightPanel, 300 );
    bMainSizer->Add( m_splitter, 1, wxEXPAND | wxALL, 5 );

    // Standard dialog buttons
    m_sdbSizer = new wxStdDialogButtonSizer();
    m_sdbSizerOK = new wxButton( this, wxID_OK );
    m_sdbSizer->AddButton( m_sdbSizerOK );
    m_sdbSizer->Realize();

    bMainSizer->Add( m_sdbSizer, 0, wxEXPAND | wxALL, 10 );

    this->SetSizer( bMainSizer );
    this->Layout();
    bMainSizer->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    m_reviewListCtrl->Connect( wxEVT_LIST_ITEM_SELECTED,
                               wxListEventHandler( DIALOG_PLM_REVIEW_BASE::onReviewSelected ),
                               NULL, this );
    m_newReviewBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                             wxCommandEventHandler( DIALOG_PLM_REVIEW_BASE::onNewReview ),
                             NULL, this );
    m_postCommentBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                               wxCommandEventHandler( DIALOG_PLM_REVIEW_BASE::onPostComment ),
                               NULL, this );
    m_approveBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                           wxCommandEventHandler( DIALOG_PLM_REVIEW_BASE::onApprove ),
                           NULL, this );
    m_requestChangesBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                                  wxCommandEventHandler(
                                          DIALOG_PLM_REVIEW_BASE::onRequestChanges ),
                                  NULL, this );
    m_rejectBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                          wxCommandEventHandler( DIALOG_PLM_REVIEW_BASE::onReject ),
                          NULL, this );
    m_mergeBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                         wxCommandEventHandler( DIALOG_PLM_REVIEW_BASE::onMerge ),
                         NULL, this );
}

DIALOG_PLM_REVIEW_BASE::~DIALOG_PLM_REVIEW_BASE()
{
    // Disconnect Events
    m_reviewListCtrl->Disconnect( wxEVT_LIST_ITEM_SELECTED,
                                  wxListEventHandler(
                                          DIALOG_PLM_REVIEW_BASE::onReviewSelected ),
                                  NULL, this );
    m_newReviewBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED,
                                wxCommandEventHandler( DIALOG_PLM_REVIEW_BASE::onNewReview ),
                                NULL, this );
    m_postCommentBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED,
                                  wxCommandEventHandler( DIALOG_PLM_REVIEW_BASE::onPostComment ),
                                  NULL, this );
    m_approveBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED,
                              wxCommandEventHandler( DIALOG_PLM_REVIEW_BASE::onApprove ),
                              NULL, this );
    m_requestChangesBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED,
                                     wxCommandEventHandler(
                                             DIALOG_PLM_REVIEW_BASE::onRequestChanges ),
                                     NULL, this );
    m_rejectBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED,
                             wxCommandEventHandler( DIALOG_PLM_REVIEW_BASE::onReject ),
                             NULL, this );
    m_mergeBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED,
                            wxCommandEventHandler( DIALOG_PLM_REVIEW_BASE::onMerge ),
                            NULL, this );
}
