///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.10.1-0-g8feb16b3)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_plm_browser_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_PLM_BROWSER_BASE::DIALOG_PLM_BROWSER_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
    this->SetSizeHints( wxSize( 1200,800 ), wxDefaultSize );

    bMainSizer = new wxBoxSizer( wxVERTICAL );

    m_mainPanel = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    wxBoxSizer* bSizerMain;
    bSizerMain = new wxBoxSizer( wxVERTICAL );

    m_placeholderText = new wxStaticText( m_mainPanel, wxID_ANY, _("PLM Browser - UI will be constructed programmatically"), wxDefaultPosition, wxDefaultSize, 0 );
    m_placeholderText->Wrap( -1 );
    bSizerMain->Add( m_placeholderText, 0, wxALL, 5 );


    m_mainPanel->SetSizer( bSizerMain );
    m_mainPanel->Layout();
    bSizerMain->Fit( m_mainPanel );
    bMainSizer->Add( m_mainPanel, 1, wxEXPAND, 5 );

    m_statusBar = new wxStatusBar( this, wxID_ANY, wxSTB_SIZEGRIP );
    m_statusBar->SetFieldsCount( 2 );
    bMainSizer->Add( m_statusBar, 0, wxEXPAND, 0 );

    this->SetSizer( bMainSizer );
    this->Layout();
    bMainSizer->Fit( this );

    this->Centre( wxBOTH );
}

DIALOG_PLM_BROWSER_BASE::~DIALOG_PLM_BROWSER_BASE()
{
}
