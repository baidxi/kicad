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
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/sizer.h>
#include <wx/panel.h>
#include <wx/splitter.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_PLM_DIFF_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_PLM_DIFF_BASE : public DIALOG_SHIM
{
	private:

	protected:
		wxStaticText* m_comparisonLabel;
		wxChoice* m_viewModeChoice;
		wxButton* m_prevChangeBtn;
		wxButton* m_nextChangeBtn;
		wxStaticText* m_changeCountLabel;
		wxSplitterWindow* m_splitter;
		wxPanel* m_leftPanel;
		wxStaticText* m_leftRevisionLabel;
		wxPanel* m_rightPanel;
		wxStaticText* m_rightRevisionLabel;
		wxStaticText* m_addedLabel;
		wxStaticText* m_removedLabel;
		wxStaticText* m_modifiedLabel;
		wxStdDialogButtonSizer* m_sdbSizer;
		wxButton* m_sdbSizerCancel;

		// Virtual event handlers, override them in your derived class
		virtual void OnViewModeChanged( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnPrevChange( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnNextChange( wxCommandEvent& event ) { event.Skip(); }


	public:

		DIALOG_PLM_DIFF_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("PLM - Revision Comparison"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );

		~DIALOG_PLM_DIFF_BASE();

		void m_splitterOnIdle( wxIdleEvent& )
		{
			m_splitter->SetSashPosition( 0 );
			m_splitter->Disconnect( wxEVT_IDLE, wxIdleEventHandler( DIALOG_PLM_DIFF_BASE::m_splitterOnIdle ), NULL, this );
		}

};

