///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/intl.h>
#include <pcb_base_frame.h>
#include <pcb_draw_panel_gal.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/string.h>
#include <wx/sizer.h>
#include <wx/panel.h>
#include <wx/grid.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/tglbtn.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/statline.h>
#include <wx/dataview.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG__IDX_REVIEW_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG__IDX_REVIEW_BASE : public wxDialog
{
private:
protected:
    wxSplitterWindow*          m_splitter2;
    wxPanel*                   m_panel1;
    wxSplitterWindow*          m_splitter21;
    wxPanel*                   m_panel41;
    PCB_DRAW_PANEL_GAL*        m_panelPreview;
    KIGFX::GAL_DISPLAY_OPTIONS m_galOptions;
    wxPanel*                   m_panel2;
    wxGrid*                    m_changeGrid;
    wxStaticText*              m_staticText1;
    wxToggleButton*            m_acceptAll;
    wxToggleButton*            m_rejectAll;
    wxStdDialogButtonSizer*    m_sdbSizer1;
    wxButton*                  m_sdbSizer1Apply;
    wxButton*                  m_sdbSizer1Cancel;
    wxPanel*                   m_panel4;
    wxStaticText*              m_staticText3;
    wxTextCtrl*                m_collaborationFolderInput;
    wxBitmapToggleButton*      m_bmToggleBtn1;
    wxButton*                  m_button1;
    wxStaticLine*              m_staticline1;
    wxStaticText*              m_staticText4;
    wxDataViewListCtrl*        m_history;

    // Virtual event handlers, override them in your derived class
    virtual void OnGridCellClick( wxGridEvent& event ) { event.Skip(); }
    virtual void OnAcceptAll( wxCommandEvent& event ) { event.Skip(); }
    virtual void OnRejectAll( wxCommandEvent& event ) { event.Skip(); }
    virtual void OnApply( wxCommandEvent& event ) { event.Skip(); }
    virtual void OnCancel( wxCommandEvent& event ) { event.Skip(); }
    virtual void OnCollaborationFolderText( wxCommandEvent& event ) { event.Skip(); }
    virtual void onBrowseButton( wxCommandEvent& event ) { event.Skip(); }
    virtual void onItemActivated( wxDataViewEvent& event ) { event.Skip(); }


public:
    DIALOG__IDX_REVIEW_BASE( wxWindow* parent, wxWindowID id = wxID_ANY,
                             const wxString& title = _( "IDX import review" ), const wxPoint& pos = wxDefaultPosition,
                             const wxSize& size = wxSize( 663, 570 ),
                             long          style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER );

    ~DIALOG__IDX_REVIEW_BASE();

    void m_splitter2OnIdle( wxIdleEvent& )
    {
        m_splitter2->SetSashPosition( 380 );
        m_splitter2->Disconnect( wxEVT_IDLE, wxIdleEventHandler( DIALOG__IDX_REVIEW_BASE::m_splitter2OnIdle ), NULL,
                                 this );
    }

    void m_splitter21OnIdle( wxIdleEvent& )
    {
        m_splitter21->SetSashPosition( 0 );
        m_splitter21->Disconnect( wxEVT_IDLE, wxIdleEventHandler( DIALOG__IDX_REVIEW_BASE::m_splitter21OnIdle ), NULL,
                                  this );
    }
};
