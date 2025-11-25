///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6a-dirty)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_plm_variant_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_PLM_VARIANT_BASE::DIALOG_PLM_VARIANT_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 800,500 ), wxDefaultSize );

	wxBoxSizer* bMainSizer;
	bMainSizer = new wxBoxSizer( wxVERTICAL );

	m_splitter = new wxSplitterWindow( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_LIVE_UPDATE );
	m_splitter->Connect( wxEVT_IDLE, wxIdleEventHandler( DIALOG_PLM_VARIANT_BASE::m_splitterOnIdle ), NULL, this );
	m_splitter->SetMinimumPaneSize( 200 );

	// Left panel - variant list
	m_variantListPanel = new wxPanel( m_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bVariantListSizer;
	bVariantListSizer = new wxBoxSizer( wxVERTICAL );

	m_variantListLabel = new wxStaticText( m_variantListPanel, wxID_ANY, _("Variants:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_variantListLabel->Wrap( -1 );
	bVariantListSizer->Add( m_variantListLabel, 0, wxEXPAND|wxALL, 5 );

	m_variantList = new wxListBox( m_variantListPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_SINGLE );
	bVariantListSizer->Add( m_variantList, 1, wxEXPAND|wxALL, 5 );

	wxBoxSizer* bVariantBtnSizer;
	bVariantBtnSizer = new wxBoxSizer( wxHORIZONTAL );

	m_addVariantBtn = new wxButton( m_variantListPanel, wxID_ANY, _("Add"), wxDefaultPosition, wxDefaultSize, 0 );
	bVariantBtnSizer->Add( m_addVariantBtn, 0, wxALL, 5 );

	m_deleteVariantBtn = new wxButton( m_variantListPanel, wxID_ANY, _("Delete"), wxDefaultPosition, wxDefaultSize, 0 );
	m_deleteVariantBtn->Enable( false );
	bVariantBtnSizer->Add( m_deleteVariantBtn, 0, wxALL, 5 );

	bVariantListSizer->Add( bVariantBtnSizer, 0, wxEXPAND, 5 );

	m_variantListPanel->SetSizer( bVariantListSizer );
	m_variantListPanel->Layout();
	bVariantListSizer->Fit( m_variantListPanel );

	// Right panel - variant detail
	m_detailPanel = new wxPanel( m_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bDetailSizer;
	bDetailSizer = new wxBoxSizer( wxVERTICAL );

	wxFlexGridSizer* fgDetailSizer;
	fgDetailSizer = new wxFlexGridSizer( 0, 2, 5, 0 );
	fgDetailSizer->AddGrowableCol( 1 );
	fgDetailSizer->SetFlexibleDirection( wxBOTH );
	fgDetailSizer->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	m_nameLabel = new wxStaticText( m_detailPanel, wxID_ANY, _("Name:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_nameLabel->Wrap( -1 );
	fgDetailSizer->Add( m_nameLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_nameCtrl = new wxTextCtrl( m_detailPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	fgDetailSizer->Add( m_nameCtrl, 0, wxALL|wxEXPAND, 5 );

	m_descriptionLabel = new wxStaticText( m_detailPanel, wxID_ANY, _("Description:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_descriptionLabel->Wrap( -1 );
	fgDetailSizer->Add( m_descriptionLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_descriptionCtrl = new wxTextCtrl( m_detailPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	fgDetailSizer->Add( m_descriptionCtrl, 0, wxALL|wxEXPAND, 5 );

	bDetailSizer->Add( fgDetailSizer, 0, wxEXPAND|wxALL, 5 );

	m_overridesLabel = new wxStaticText( m_detailPanel, wxID_ANY, _("BOM Overrides:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_overridesLabel->Wrap( -1 );
	bDetailSizer->Add( m_overridesLabel, 0, wxEXPAND|wxLEFT|wxRIGHT|wxTOP, 10 );

	m_overridesList = new wxDataViewListCtrl( m_detailPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES|wxDV_VERT_RULES );
	bDetailSizer->Add( m_overridesList, 1, wxEXPAND|wxALL, 5 );

	wxBoxSizer* bOverrideBtnSizer;
	bOverrideBtnSizer = new wxBoxSizer( wxHORIZONTAL );

	m_addOverrideBtn = new wxButton( m_detailPanel, wxID_ANY, _("Add Override"), wxDefaultPosition, wxDefaultSize, 0 );
	bOverrideBtnSizer->Add( m_addOverrideBtn, 0, wxALL, 5 );

	m_removeOverrideBtn = new wxButton( m_detailPanel, wxID_ANY, _("Remove Override"), wxDefaultPosition, wxDefaultSize, 0 );
	m_removeOverrideBtn->Enable( false );
	bOverrideBtnSizer->Add( m_removeOverrideBtn, 0, wxALL, 5 );

	bDetailSizer->Add( bOverrideBtnSizer, 0, wxEXPAND, 5 );

	m_detailPanel->SetSizer( bDetailSizer );
	m_detailPanel->Layout();
	bDetailSizer->Fit( m_detailPanel );

	m_splitter->SplitVertically( m_variantListPanel, m_detailPanel, 250 );
	bMainSizer->Add( m_splitter, 1, wxEXPAND|wxALL, 5 );

	m_sdbSizer = new wxStdDialogButtonSizer();
	m_sdbSizerOK = new wxButton( this, wxID_OK );
	m_sdbSizer->AddButton( m_sdbSizerOK );
	m_sdbSizerCancel = new wxButton( this, wxID_CANCEL );
	m_sdbSizer->AddButton( m_sdbSizerCancel );
	m_sdbSizerApply = new wxButton( this, wxID_APPLY );
	m_sdbSizer->AddButton( m_sdbSizerApply );
	m_sdbSizer->Realize();

	bMainSizer->Add( m_sdbSizer, 0, wxALL|wxEXPAND, 5 );

	this->SetSizer( bMainSizer );
	this->Layout();
	bMainSizer->Fit( this );

	this->Centre( wxBOTH );

	// Connect Events
	m_variantList->Connect( wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler( DIALOG_PLM_VARIANT_BASE::onVariantSelected ), NULL, this );
	m_addVariantBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_VARIANT_BASE::onAddVariant ), NULL, this );
	m_deleteVariantBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_VARIANT_BASE::onDeleteVariant ), NULL, this );
	m_addOverrideBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_VARIANT_BASE::onAddOverride ), NULL, this );
	m_removeOverrideBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_VARIANT_BASE::onRemoveOverride ), NULL, this );
}

DIALOG_PLM_VARIANT_BASE::~DIALOG_PLM_VARIANT_BASE()
{
	// Disconnect Events
	m_variantList->Disconnect( wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler( DIALOG_PLM_VARIANT_BASE::onVariantSelected ), NULL, this );
	m_addVariantBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_VARIANT_BASE::onAddVariant ), NULL, this );
	m_deleteVariantBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_VARIANT_BASE::onDeleteVariant ), NULL, this );
	m_addOverrideBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_VARIANT_BASE::onAddOverride ), NULL, this );
	m_removeOverrideBtn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DIALOG_PLM_VARIANT_BASE::onRemoveOverride ), NULL, this );

}
