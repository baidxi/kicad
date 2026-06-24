///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6-dirty)
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
#include <wx/statbox.h>
#include <wx/radiobox.h>
#include <wx/button.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_EDIT_SUBSHEET_PAGENUM_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_EDIT_SUBSHEET_PAGENUM_BASE : public DIALOG_SHIM
{
	private:

	protected:
		wxStaticText* m_staticTextDescription;
		wxTextCtrl* m_textName;
		wxCheckBox* m_checkDefaultName;
		wxRadioBox* m_radioOrientation;
		wxRadioBox* m_radioPosition;
		wxRadioBox* m_radioAction;
		wxStdDialogButtonSizer* m_sdbSizerStdButtons;
		wxButton* m_sdbSizerStdButtonsOK;
		wxButton* m_sdbSizerStdButtonsApply;
		wxButton* m_sdbSizerStdButtonsCancel;

		// Virtual event handlers, override them in your derived class
		virtual void OnChangedDefaultName( wxCommandEvent& event ) { event.Skip(); }


	public:

		DIALOG_EDIT_SUBSHEET_PAGENUM_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Add/Remove page numbering of the sub-sheets"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxCAPTION );

		~DIALOG_EDIT_SUBSHEET_PAGENUM_BASE();

};

