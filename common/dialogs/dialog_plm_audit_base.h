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
#include <wx/datectrl.h>
#include <wx/dateevt.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/dataview.h>
#include <wx/textctrl.h>
#include <wx/collpane.h>
#include <wx/button.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_PLM_AUDIT_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_PLM_AUDIT_BASE : public DIALOG_SHIM
{
	private:

	protected:
		// Filter bar
		wxStaticText* m_fromLabel;
		wxDatePickerCtrl* m_fromDatePicker;
		wxStaticText* m_toLabel;
		wxDatePickerCtrl* m_toDatePicker;
		wxStaticText* m_userFilterLabel;
		wxChoice* m_userFilter;
		wxStaticText* m_eventTypeLabel;
		wxChoice* m_eventTypeFilter;
		wxStaticText* m_projectFilterLabel;
		wxChoice* m_projectFilter;

		// Main event list
		wxDataViewListCtrl* m_eventList;

		// Detail panel (collapsible)
		wxCollapsiblePane* m_detailPane;
		wxTextCtrl* m_detailText;

		// Bottom bar
		wxButton* m_refreshBtn;
		wxButton* m_exportCsvBtn;
		wxStaticText* m_recordCountLabel;
		wxButton* m_closeBtn;

		// Virtual event handlers, override them in your derived class
		virtual void onFilterChanged( wxCommandEvent& event ) { event.Skip(); }
		virtual void onDateChanged( wxDateEvent& event ) { event.Skip(); }
		virtual void onEventSelected( wxDataViewEvent& event ) { event.Skip(); }
		virtual void onRefresh( wxCommandEvent& event ) { event.Skip(); }
		virtual void onExportCsv( wxCommandEvent& event ) { event.Skip(); }
		virtual void onClose( wxCommandEvent& event ) { event.Skip(); }


	public:

		DIALOG_PLM_AUDIT_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Audit Log"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxMAXIMIZE_BOX );

		~DIALOG_PLM_AUDIT_BASE();

};
