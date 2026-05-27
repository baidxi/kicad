///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6a-dirty)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/intl.h>
#include "dialog_shim.h"
#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/splitter.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_PLM_REVIEW_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_PLM_REVIEW_BASE : public DIALOG_SHIM
{
private:

protected:
    wxSplitterWindow* m_splitter;
    wxPanel* m_leftPanel;
    wxStaticText* m_reviewListLabel;
    wxListCtrl* m_reviewListCtrl;
    wxButton* m_newReviewBtn;
    wxPanel* m_rightPanel;
    wxBoxSizer* bDetailsSizer;
    wxStaticText* m_titleLabel;
    wxStaticText* m_statusLabel;
    wxStaticText* m_authorLabel;
    wxStaticText* m_dateLabel;
    wxStaticText* m_reviewersLabel;
    wxScrolledWindow* m_reviewersScrolled;
    wxBoxSizer* bReviewersSizer;
    wxStaticText* m_commentsLabel;
    wxScrolledWindow* m_commentsScrolled;
    wxBoxSizer* bCommentsSizer;
    wxTextCtrl* m_commentInput;
    wxButton* m_postCommentBtn;
    wxPanel* m_decisionPanel;
    wxButton* m_approveBtn;
    wxButton* m_requestChangesBtn;
    wxButton* m_rejectBtn;
    wxButton* m_mergeBtn;
    wxStdDialogButtonSizer* m_sdbSizer;
    wxButton* m_sdbSizerOK;

    // Virtual event handlers, override them in your derived class
    virtual void onReviewSelected( wxListEvent& event ) { event.Skip(); }
    virtual void onNewReview( wxCommandEvent& event ) { event.Skip(); }
    virtual void onPostComment( wxCommandEvent& event ) { event.Skip(); }
    virtual void onApprove( wxCommandEvent& event ) { event.Skip(); }
    virtual void onRequestChanges( wxCommandEvent& event ) { event.Skip(); }
    virtual void onReject( wxCommandEvent& event ) { event.Skip(); }
    virtual void onMerge( wxCommandEvent& event ) { event.Skip(); }

public:

    DIALOG_PLM_REVIEW_BASE( wxWindow* parent, wxWindowID id = wxID_ANY,
                             const wxString& title = _("PLM - Design Reviews"),
                             const wxPoint& pos = wxDefaultPosition,
                             const wxSize& size = wxSize( -1,-1 ),
                             long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );

    ~DIALOG_PLM_REVIEW_BASE();

    void m_splitterOnIdle( wxIdleEvent& )
    {
        m_splitter->SetSashPosition( 300 );
        m_splitter->Disconnect( wxEVT_IDLE,
                                wxIdleEventHandler( DIALOG_PLM_REVIEW_BASE::m_splitterOnIdle ),
                                NULL, this );
    }
};
