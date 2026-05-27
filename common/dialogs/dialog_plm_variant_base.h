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
#include <wx/button.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/dataview.h>
#include <wx/panel.h>
#include <wx/splitter.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_PLM_VARIANT_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_PLM_VARIANT_BASE : public DIALOG_SHIM
{
	private:

	protected:
		wxSplitterWindow* m_splitter;

		// Left panel - variant list
		wxPanel* m_variantListPanel;
		wxStaticText* m_variantListLabel;
		wxListBox* m_variantList;
		wxButton* m_addVariantBtn;
		wxButton* m_deleteVariantBtn;

		// Right panel - variant detail
		wxPanel* m_detailPanel;
		wxStaticText* m_nameLabel;
		wxTextCtrl* m_nameCtrl;
		wxStaticText* m_descriptionLabel;
		wxTextCtrl* m_descriptionCtrl;
		wxStaticText* m_overridesLabel;
		wxDataViewListCtrl* m_overridesList;
		wxButton* m_addOverrideBtn;
		wxButton* m_removeOverrideBtn;

		// Bottom buttons
		wxStdDialogButtonSizer* m_sdbSizer;
		wxButton* m_sdbSizerOK;
		wxButton* m_sdbSizerCancel;
		wxButton* m_sdbSizerApply;

		// Virtual event handlers, override them in your derived class
		virtual void onVariantSelected( wxCommandEvent& event ) { event.Skip(); }
		virtual void onAddVariant( wxCommandEvent& event ) { event.Skip(); }
		virtual void onDeleteVariant( wxCommandEvent& event ) { event.Skip(); }
		virtual void onAddOverride( wxCommandEvent& event ) { event.Skip(); }
		virtual void onRemoveOverride( wxCommandEvent& event ) { event.Skip(); }


	public:

		DIALOG_PLM_VARIANT_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Variant Configuration"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxMAXIMIZE_BOX );

		~DIALOG_PLM_VARIANT_BASE();

		void m_splitterOnIdle( wxIdleEvent& )
		{
			m_splitter->SetSashPosition( 250 );
			m_splitter->Disconnect( wxEVT_IDLE, wxIdleEventHandler( DIALOG_PLM_VARIANT_BASE::m_splitterOnIdle ), NULL, this );
		}

};
