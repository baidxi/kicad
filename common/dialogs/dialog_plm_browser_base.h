///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.10.1-0-g8feb16b3)
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
#include <wx/sizer.h>
#include <wx/panel.h>
#include <wx/statusbr.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_PLM_BROWSER_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_PLM_BROWSER_BASE : public DIALOG_SHIM
{
private:

protected:
    wxBoxSizer* bMainSizer;
    wxPanel* m_mainPanel;
    wxStaticText* m_placeholderText;
    wxStatusBar* m_statusBar;

public:

    DIALOG_PLM_BROWSER_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("PLM Browser"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxMAXIMIZE_BOX );

    ~DIALOG_PLM_BROWSER_BASE();

};
