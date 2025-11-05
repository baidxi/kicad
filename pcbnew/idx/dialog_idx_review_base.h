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
#include <wx/tglbtn.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/sizer.h>
#include <wx/grid.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG__IDX_REVIEW_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG__IDX_REVIEW_BASE : public wxDialog
{
	private:

	protected:
		PCB_DRAW_PANEL_GAL* m_panelPreview;
		KIGFX::GAL_DISPLAY_OPTIONS m_galOptions;
		wxToggleButton* m_acceptAll;
		wxToggleButton* m_rejectAll;
		wxGrid* m_changeGrid;
		wxStaticText* m_staticText1;
		wxStdDialogButtonSizer* m_sdbSizer1;
		wxButton* m_sdbSizer1Apply;
		wxButton* m_sdbSizer1Cancel;

		// Virtual event handlers, override them in your derived class
		virtual void OnAcceptAll( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnRejectAll( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnGridCellClick( wxGridEvent& event ) { event.Skip(); }


	public:

		DIALOG__IDX_REVIEW_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("IDX import review"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );

		~DIALOG__IDX_REVIEW_BASE();

};

