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
#include <wx/choice.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/dataview.h>
#include <wx/panel.h>
#include <wx/splitter.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_PLM_BOM_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_PLM_BOM_BASE : public DIALOG_SHIM
{
private:

protected:
    wxBoxSizer* bMainSizer;
    wxPanel* m_toolbarPanel;
    wxStaticText* m_bomTypeLabel;
    wxChoice* m_bomTypeChoice;
    wxButton* m_createBomBtn;
    wxButton* m_deleteBomBtn;
    wxButton* m_diffBtn;
    wxButton* m_validateBtn;
    wxSplitterWindow* m_splitter;
    wxPanel* m_listPanel;
    wxDataViewListCtrl* m_bomListCtrl;
    wxPanel* m_validationPanel;
    wxStaticText* m_validationLabel;
    wxDataViewListCtrl* m_validationListCtrl;
    wxStdDialogButtonSizer* m_sdbSizer;
    wxButton* m_sdbSizerOK;
    wxButton* m_sdbSizerCancel;

    // Virtual event handlers, override them in your derived class
    virtual void onBomTypeChanged( wxCommandEvent& event ) { event.Skip(); }
    virtual void onCreateBom( wxCommandEvent& event ) { event.Skip(); }
    virtual void onDeleteBom( wxCommandEvent& event ) { event.Skip(); }
    virtual void onDiffBom( wxCommandEvent& event ) { event.Skip(); }
    virtual void onValidateBom( wxCommandEvent& event ) { event.Skip(); }
    virtual void onBomItemSelected( wxDataViewEvent& event ) { event.Skip(); }

public:

    DIALOG_PLM_BOM_BASE( wxWindow* parent, wxWindowID id = wxID_ANY,
                          const wxString& title = _("PLM - Bill of Materials"),
                          const wxPoint& pos = wxDefaultPosition,
                          const wxSize& size = wxSize( -1,-1 ),
                          long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );

    ~DIALOG_PLM_BOM_BASE();

    void m_splitterOnIdle( wxIdleEvent& )
    {
        m_splitter->SetSashPosition( -200 );
        m_splitter->Disconnect( wxEVT_IDLE,
                                wxIdleEventHandler( DIALOG_PLM_BOM_BASE::m_splitterOnIdle ),
                                NULL, this );
    }
};
