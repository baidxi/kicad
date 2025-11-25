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
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/panel.h>
#include <wx/dataview.h>
#include <wx/checkbox.h>
#include <wx/simplebook.h>
#include <wx/button.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_PLM_RELEASE_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_PLM_RELEASE_BASE : public DIALOG_SHIM
{
	private:

	protected:
		wxSimplebook* m_wizard;

		// Page 1 - Release Info
		wxPanel* m_pageInfo;
		wxStaticText* m_versionLabel;
		wxTextCtrl* m_versionCtrl;
		wxStaticText* m_schemeLabel;
		wxChoice* m_schemeChoice;
		wxStaticText* m_titleLabel;
		wxTextCtrl* m_titleCtrl;
		wxStaticText* m_notesLabel;
		wxTextCtrl* m_notesCtrl;

		// Page 2 - Validation Checks
		wxPanel* m_pageChecks;
		wxStaticText* m_checksLabel;
		wxDataViewListCtrl* m_checksList;

		// Page 3 - Manufacturing Outputs
		wxPanel* m_pageOutputs;
		wxStaticText* m_outputsLabel;
		wxCheckBox* m_chkGerber;
		wxCheckBox* m_chkDrill;
		wxCheckBox* m_chkBom;
		wxCheckBox* m_chkPnP;
		wxCheckBox* m_chkPdf;
		wxCheckBox* m_chk3D;

		// Page 4 - Summary
		wxPanel* m_pageSummary;
		wxStaticText* m_summaryLabel;
		wxTextCtrl* m_summaryText;

		// Navigation buttons
		wxButton* m_backBtn;
		wxButton* m_nextBtn;
		wxButton* m_cancelBtn;

		// Virtual event handlers, override them in your derived class
		virtual void onBack( wxCommandEvent& event ) { event.Skip(); }
		virtual void onNext( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCancel( wxCommandEvent& event ) { event.Skip(); }


	public:

		DIALOG_PLM_RELEASE_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Release Wizard"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxMAXIMIZE_BOX );

		~DIALOG_PLM_RELEASE_BASE();

};
