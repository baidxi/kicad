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
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/panel.h>
#include <wx/textctrl.h>
#include <wx/splitter.h>
#include <wx/scrolwin.h>
#include <wx/button.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_PLM_ECO_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_PLM_ECO_BASE : public DIALOG_SHIM
{
	private:

	protected:
		wxStaticText* m_infoLabel;
		wxSplitterWindow* m_splitter;
		wxListBox* m_ecoList;
		wxPanel* m_detailsPanel;
		wxStaticText* m_ecoNumberLabel;
		wxStaticText* m_ecoTitleLabel;
		wxStaticText* m_ecoOwnerLabel;
		wxStaticText* m_ecoStatusLabel;
		wxStaticText* m_ecoDueDateLabel;
		wxTextCtrl* m_ecoDescriptionCtrl;
		wxStaticText* m_fieldsLabel;
		wxScrolledWindow* m_fieldsScrolled;
		wxBoxSizer* bFieldsScrollSizer;
		wxStdDialogButtonSizer* m_sdbSizer;
		wxButton* m_sdbSizerOK;
		wxButton* m_sdbSizerCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onEcoSelected( wxCommandEvent& event ) { event.Skip(); }


	public:

		DIALOG_PLM_ECO_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("PLM - Select Engineering Change Order"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );

		~DIALOG_PLM_ECO_BASE();

		void m_splitterOnIdle( wxIdleEvent& )
		{
			m_splitter->SetSashPosition( 0 );
			m_splitter->Disconnect( wxEVT_IDLE, wxIdleEventHandler( DIALOG_PLM_ECO_BASE::m_splitterOnIdle ), NULL, this );
		}

};

