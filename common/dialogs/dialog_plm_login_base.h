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
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_PLM_LOGIN_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_PLM_LOGIN_BASE : public DIALOG_SHIM
{
	private:

	protected:
		wxStaticText* m_serverUrlLabel;
		wxTextCtrl* m_serverUrlCtrl;
		wxStaticText* m_usernameLabel;
		wxTextCtrl* m_usernameCtrl;
		wxStaticText* m_passwordLabel;
		wxTextCtrl* m_passwordCtrl;
		wxCheckBox* m_rememberCheckbox;
		wxStdDialogButtonSizer* m_sdbSizer;
		wxButton* m_sdbSizerOK;
		wxButton* m_sdbSizerCancel;

	public:

		DIALOG_PLM_LOGIN_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("PLM Login"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE );

		~DIALOG_PLM_LOGIN_BASE();

};

