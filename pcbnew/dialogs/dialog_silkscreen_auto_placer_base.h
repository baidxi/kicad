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
#include <wx/radiobut.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <wx/choice.h>
#include <wx/button.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_SILKSCREEN_AUTO_PLACER_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_SILKSCREEN_AUTO_PLACER_BASE : public DIALOG_SHIM
{
	private:

	protected:
		wxRadioButton* m_rbPlaceAll;
		wxRadioButton* m_rbPlaceSel;
		wxRadioButton* m_rbFailCenter;
		wxRadioButton* m_rbFailOffBoard;
		wxRadioButton* m_rbFailRestore;
		wxTextCtrl* m_allowSilkUnder;
		wxCheckBox* m_cbAvoidVias;
		wxStaticText* m_stRotation;
		wxChoice* m_choiceRotation;
		wxCheckBox* m_cbTryAltRotation;
		wxCheckBox* m_cbFixedSize;
		wxStaticText* m_stFixedSize;
		wxTextCtrl* m_tcFixedSize;
		wxStaticText* m_stFixedSizeUnits;
		wxCheckBox* m_cbFixedWidth;
		wxStaticText* m_stFixedWidth;
		wxTextCtrl* m_tcFixedWidth;
		wxStaticText* m_stFixedWidthUnits;
		wxCheckBox* m_cbTopCenter;
		wxCheckBox* m_cbCenterRight;
		wxCheckBox* m_cbBotCenter;
		wxCheckBox* m_cbCenterLeft;
		wxCheckBox* m_cbTopLeft;
		wxCheckBox* m_cbTopRight;
		wxCheckBox* m_cbBotLeft;
		wxCheckBox* m_cbBotRight;
		wxStaticText* m_stPosDelta;
		wxTextCtrl* m_tcPosDelta;
		wxStaticText* m_stPosDeltaUnits;
		wxStdDialogButtonSizer* m_sdbSizer;
		wxButton* m_sdbSizerOK;
		wxButton* m_sdbSizerCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onFixedSizeCheck( wxCommandEvent& event ) { event.Skip(); }
		virtual void onFixedWidthCheck( wxCommandEvent& event ) { event.Skip(); }


	public:

		DIALOG_SILKSCREEN_AUTO_PLACER_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Auto-Place Silkscreen"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );

		~DIALOG_SILKSCREEN_AUTO_PLACER_BASE();

};

