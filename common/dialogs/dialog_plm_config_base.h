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
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/statbox.h>
#include <wx/button.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/checkbox.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_PLM_CONFIG_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_PLM_CONFIG_BASE : public DIALOG_SHIM
{
	private:

	protected:
		wxStaticText* m_infoLabel;
		wxChoice* m_pluginChoice;
		wxStaticText* m_pluginDescLabel;
		wxTextCtrl* m_serverUrlCtrl;
		wxTextCtrl* m_defaultFolderCtrl;
		wxButton* m_testConnectionBtn;
		wxStaticText* m_connectionStatusLabel;
		wxCheckBox* m_useDefaultCredsCheckbox;
		wxTextCtrl* m_usernameCtrl;
		wxTextCtrl* m_passwordCtrl;
		wxStdDialogButtonSizer* m_sdbSizer;
		wxButton* m_sdbSizerOK;
		wxButton* m_sdbSizerCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onPluginSelected( wxCommandEvent& event ) { event.Skip(); }
		virtual void onTestConnection( wxCommandEvent& event ) { event.Skip(); }
		virtual void onUseDefaultCredsChanged( wxCommandEvent& event ) { event.Skip(); }


	public:

		DIALOG_PLM_CONFIG_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("PLM - Server Configuration"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );

		~DIALOG_PLM_CONFIG_BASE();

};

