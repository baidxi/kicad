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
#include <wx/textctrl.h>
#include <wx/splitter.h>
#include <wx/button.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_PLM_HISTORY_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_PLM_HISTORY_BASE : public DIALOG_SHIM
{
	private:

	protected:
		wxSplitterWindow* m_splitter;
		wxPanel* m_revisionListPanel;
		wxStaticText* m_revisionListLabel;
		wxDataViewListCtrl* m_revisionList;
		wxPanel* m_detailsPanel;
		wxStaticText* m_detailsLabel;
		wxStaticText* m_revisionIdLabel;
		wxStaticText* m_revisionIdValue;
		wxStaticText* m_authorLabel;
		wxStaticText* m_authorValue;
		wxStaticText* m_dateLabel;
		wxStaticText* m_dateValue;
		wxStaticText* m_commentLabel;
		wxTextCtrl* m_commentText;
		wxBoxSizer* bButtonSizer;
		wxButton* m_compareBtn;
		wxButton* m_downloadBtn;
		wxButton* m_closeBtn;

		// Virtual event handlers, override them in your derived class
		virtual void onRevisionSelected( wxDataViewEvent& event ) { event.Skip(); }
		virtual void onCompare( wxCommandEvent& event ) { event.Skip(); }
		virtual void onDownload( wxCommandEvent& event ) { event.Skip(); }
		virtual void onClose( wxCommandEvent& event ) { event.Skip(); }


	public:

		DIALOG_PLM_HISTORY_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Revision History"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxMAXIMIZE_BOX );

		~DIALOG_PLM_HISTORY_BASE();

		void m_splitterOnIdle( wxIdleEvent& )
		{
			m_splitter->SetSashPosition( 0 );
			m_splitter->Disconnect( wxEVT_IDLE, wxIdleEventHandler( DIALOG_PLM_HISTORY_BASE::m_splitterOnIdle ), NULL, this );
		}

};

