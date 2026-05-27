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
#include <wx/dataview.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/button.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_PLM_BOM_VALIDATION_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_PLM_BOM_VALIDATION_BASE : public DIALOG_SHIM
{
	private:

	protected:
		wxStaticText* m_infoLabel;
		wxDataViewListCtrl* m_bomList;
		wxStaticText* m_totalCountLabel;
		wxStaticText* m_validCountLabel;
		wxStaticText* m_issueCountLabel;
		wxButton* m_acceptAllBtn;
		wxButton* m_acceptSelectedBtn;
		wxStdDialogButtonSizer* m_sdbSizer;
		wxButton* m_sdbSizerCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onBomItemSelected( wxDataViewEvent& event ) { event.Skip(); }
		virtual void onAcceptAll( wxCommandEvent& event ) { event.Skip(); }
		virtual void onAcceptSelected( wxCommandEvent& event ) { event.Skip(); }


	public:

		DIALOG_PLM_BOM_VALIDATION_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("PLM - BOM Validation Results"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxMAXIMIZE_BOX );

		~DIALOG_PLM_BOM_VALIDATION_BASE();

};

