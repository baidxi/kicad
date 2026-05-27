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
#include <wx/panel.h>
#include <wx/splitter.h>
#include <wx/button.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_PLM_CONFLICT_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_PLM_CONFLICT_BASE : public DIALOG_SHIM
{
	private:

	protected:
		wxStaticText* m_infoLabel;
		wxSplitterWindow* m_mainSplitter;
		wxPanel* m_conflictListPanel;
		wxStaticText* m_conflictListLabel;
		wxDataViewListCtrl* m_conflictList;
		wxPanel* m_previewPanel;
		wxStaticText* m_previewLabel;
		wxPanel* m_previewContent;
		wxBoxSizer* bPreviewContentSizer;
		wxStaticText* m_previewPlaceholder;
		wxStdDialogButtonSizer* m_sdbSizer;
		wxButton* m_sdbSizerOK;
		wxButton* m_sdbSizerCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onConflictSelected( wxDataViewEvent& event ) { event.Skip(); }


	public:

		DIALOG_PLM_CONFLICT_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Resolve PLM Conflicts"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxMAXIMIZE_BOX );

		~DIALOG_PLM_CONFLICT_BASE();

		void m_mainSplitterOnIdle( wxIdleEvent& )
		{
			m_mainSplitter->SetSashPosition( 0 );
			m_mainSplitter->Disconnect( wxEVT_IDLE, wxIdleEventHandler( DIALOG_PLM_CONFLICT_BASE::m_mainSplitterOnIdle ), NULL, this );
		}

};

