///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6a-dirty)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_silkscreen_auto_placer_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_SILKSCREEN_AUTO_PLACER_BASE::DIALOG_SILKSCREEN_AUTO_PLACER_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );

	wxBoxSizer* bMainSizer;
	bMainSizer = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* bTopSizer;
	bTopSizer = new wxBoxSizer( wxHORIZONTAL );

	wxBoxSizer* bLeftColumn;
	bLeftColumn = new wxBoxSizer( wxVERTICAL );

	wxStaticBoxSizer* sbFilterOptions;
	sbFilterOptions = new wxStaticBoxSizer( new wxStaticBox( this, wxID_ANY, _("Filter Options") ), wxVERTICAL );

	m_rbPlaceAll = new wxRadioButton( sbFilterOptions->GetStaticBox(), wxID_ANY, _("Place entire board"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP );
	m_rbPlaceAll->SetValue( true );
	sbFilterOptions->Add( m_rbPlaceAll, 0, wxALL, 5 );

	m_rbPlaceSel = new wxRadioButton( sbFilterOptions->GetStaticBox(), wxID_ANY, _("Place selected"), wxDefaultPosition, wxDefaultSize, 0 );
	sbFilterOptions->Add( m_rbPlaceSel, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );


	bLeftColumn->Add( sbFilterOptions, 0, wxEXPAND|wxALL, 5 );

	wxStaticBoxSizer* sbFailedOptions;
	sbFailedOptions = new wxStaticBoxSizer( new wxStaticBox( this, wxID_ANY, _("Failed Placement") ), wxVERTICAL );

	m_rbFailCenter = new wxRadioButton( sbFailedOptions->GetStaticBox(), wxID_ANY, _("Center over component"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP );
	sbFailedOptions->Add( m_rbFailCenter, 0, wxALL, 5 );

	m_rbFailOffBoard = new wxRadioButton( sbFailedOptions->GetStaticBox(), wxID_ANY, _("Place off board (bottom left)"), wxDefaultPosition, wxDefaultSize, 0 );
	sbFailedOptions->Add( m_rbFailOffBoard, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );

	m_rbFailRestore = new wxRadioButton( sbFailedOptions->GetStaticBox(), wxID_ANY, _("Restore original"), wxDefaultPosition, wxDefaultSize, 0 );
	m_rbFailRestore->SetValue( true );
	sbFailedOptions->Add( m_rbFailRestore, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );


	bLeftColumn->Add( sbFailedOptions, 0, wxEXPAND|wxALL, 5 );


	bTopSizer->Add( bLeftColumn, 0, wxEXPAND, 5 );

	wxStaticBoxSizer* sbAllowSilk;
	sbAllowSilk = new wxStaticBoxSizer( new wxStaticBox( this, wxID_ANY, _("Allow Silk Under Components") ), wxVERTICAL );

	m_allowSilkUnder = new wxTextCtrl( sbAllowSilk->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE );
	m_allowSilkUnder->SetToolTip( _("Enter reference designators (one per line) where silk overlap is acceptable") );
	m_allowSilkUnder->SetMinSize( wxSize( 200,100 ) );

	sbAllowSilk->Add( m_allowSilkUnder, 1, wxALL|wxEXPAND, 5 );


	bTopSizer->Add( sbAllowSilk, 1, wxEXPAND|wxALL, 5 );


	bMainSizer->Add( bTopSizer, 0, wxEXPAND, 5 );

	wxBoxSizer* bMiddleSizer;
	bMiddleSizer = new wxBoxSizer( wxHORIZONTAL );

	wxBoxSizer* bOptionsColumn;
	bOptionsColumn = new wxBoxSizer( wxVERTICAL );

	m_cbAvoidVias = new wxCheckBox( this, wxID_ANY, _("Avoid VIAs"), wxDefaultPosition, wxDefaultSize, 0 );
	m_cbAvoidVias->SetValue(true);
	bOptionsColumn->Add( m_cbAvoidVias, 0, wxALL, 5 );

	wxFlexGridSizer* fgRotation;
	fgRotation = new wxFlexGridSizer( 0, 2, 0, 0 );
	fgRotation->SetFlexibleDirection( wxBOTH );
	fgRotation->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	m_stRotation = new wxStaticText( this, wxID_ANY, _("Rotation strategy:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_stRotation->Wrap( -1 );
	fgRotation->Add( m_stRotation, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	wxString m_choiceRotationChoices[] = { _("KLC Style"), _("Match Component") };
	int m_choiceRotationNChoices = sizeof( m_choiceRotationChoices ) / sizeof( wxString );
	m_choiceRotation = new wxChoice( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choiceRotationNChoices, m_choiceRotationChoices, 0 );
	m_choiceRotation->SetSelection( 0 );
	fgRotation->Add( m_choiceRotation, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );


	bOptionsColumn->Add( fgRotation, 0, wxEXPAND, 5 );

	m_cbTryAltRotation = new wxCheckBox( this, wxID_ANY, _("Try altered rotation"), wxDefaultPosition, wxDefaultSize, 0 );
	m_cbTryAltRotation->SetValue(true);
	m_cbTryAltRotation->SetToolTip( _("Also try candidates at perpendicular angle") );

	bOptionsColumn->Add( m_cbTryAltRotation, 0, wxALL, 5 );

	wxFlexGridSizer* fgSizeWidth;
	fgSizeWidth = new wxFlexGridSizer( 0, 4, 0, 0 );
	fgSizeWidth->AddGrowableCol( 2 );
	fgSizeWidth->SetFlexibleDirection( wxBOTH );
	fgSizeWidth->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	m_cbFixedSize = new wxCheckBox( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	fgSizeWidth->Add( m_cbFixedSize, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );

	m_stFixedSize = new wxStaticText( this, wxID_ANY, _("Fixed size:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_stFixedSize->Wrap( -1 );
	fgSizeWidth->Add( m_stFixedSize, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

	m_tcFixedSize = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_tcFixedSize->Enable( false );

	fgSizeWidth->Add( m_tcFixedSize, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_stFixedSizeUnits = new wxStaticText( this, wxID_ANY, _("mm"), wxDefaultPosition, wxDefaultSize, 0 );
	m_stFixedSizeUnits->Wrap( -1 );
	fgSizeWidth->Add( m_stFixedSizeUnits, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

	m_cbFixedWidth = new wxCheckBox( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	fgSizeWidth->Add( m_cbFixedWidth, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );

	m_stFixedWidth = new wxStaticText( this, wxID_ANY, _("Fixed width:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_stFixedWidth->Wrap( -1 );
	fgSizeWidth->Add( m_stFixedWidth, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

	m_tcFixedWidth = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_tcFixedWidth->Enable( false );

	fgSizeWidth->Add( m_tcFixedWidth, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_stFixedWidthUnits = new wxStaticText( this, wxID_ANY, _("mm"), wxDefaultPosition, wxDefaultSize, 0 );
	m_stFixedWidthUnits->Wrap( -1 );
	fgSizeWidth->Add( m_stFixedWidthUnits, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );


	bOptionsColumn->Add( fgSizeWidth, 0, wxEXPAND, 5 );


	bMiddleSizer->Add( bOptionsColumn, 0, wxEXPAND|wxALL, 5 );

	wxBoxSizer* bPositionColumn;
	bPositionColumn = new wxBoxSizer( wxVERTICAL );

	wxStaticBoxSizer* sbPositions;
	sbPositions = new wxStaticBoxSizer( new wxStaticBox( this, wxID_ANY, _("Positions") ), wxVERTICAL );

	m_cbTopCenter = new wxCheckBox( sbPositions->GetStaticBox(), wxID_ANY, _("Top center"), wxDefaultPosition, wxDefaultSize, 0 );
	m_cbTopCenter->SetValue(true);
	sbPositions->Add( m_cbTopCenter, 0, wxALL, 5 );

	m_cbCenterRight = new wxCheckBox( sbPositions->GetStaticBox(), wxID_ANY, _("Center right"), wxDefaultPosition, wxDefaultSize, 0 );
	m_cbCenterRight->SetValue(true);
	sbPositions->Add( m_cbCenterRight, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );

	m_cbBotCenter = new wxCheckBox( sbPositions->GetStaticBox(), wxID_ANY, _("Bottom center"), wxDefaultPosition, wxDefaultSize, 0 );
	m_cbBotCenter->SetValue(true);
	sbPositions->Add( m_cbBotCenter, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );

	m_cbCenterLeft = new wxCheckBox( sbPositions->GetStaticBox(), wxID_ANY, _("Center left"), wxDefaultPosition, wxDefaultSize, 0 );
	m_cbCenterLeft->SetValue(true);
	sbPositions->Add( m_cbCenterLeft, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );

	m_cbTopLeft = new wxCheckBox( sbPositions->GetStaticBox(), wxID_ANY, _("Top left"), wxDefaultPosition, wxDefaultSize, 0 );
	sbPositions->Add( m_cbTopLeft, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );

	m_cbTopRight = new wxCheckBox( sbPositions->GetStaticBox(), wxID_ANY, _("Top right"), wxDefaultPosition, wxDefaultSize, 0 );
	sbPositions->Add( m_cbTopRight, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );

	m_cbBotLeft = new wxCheckBox( sbPositions->GetStaticBox(), wxID_ANY, _("Bottom left"), wxDefaultPosition, wxDefaultSize, 0 );
	sbPositions->Add( m_cbBotLeft, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );

	m_cbBotRight = new wxCheckBox( sbPositions->GetStaticBox(), wxID_ANY, _("Bottom right"), wxDefaultPosition, wxDefaultSize, 0 );
	sbPositions->Add( m_cbBotRight, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );


	bPositionColumn->Add( sbPositions, 0, wxEXPAND|wxALL, 5 );

	wxFlexGridSizer* fgDelta;
	fgDelta = new wxFlexGridSizer( 0, 3, 0, 0 );
	fgDelta->AddGrowableCol( 1 );
	fgDelta->SetFlexibleDirection( wxBOTH );
	fgDelta->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	m_stPosDelta = new wxStaticText( this, wxID_ANY, _("Position delta:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_stPosDelta->Wrap( -1 );
	fgDelta->Add( m_stPosDelta, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_tcPosDelta = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	fgDelta->Add( m_tcPosDelta, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_stPosDeltaUnits = new wxStaticText( this, wxID_ANY, _("mm"), wxDefaultPosition, wxDefaultSize, 0 );
	m_stPosDeltaUnits->Wrap( -1 );
	fgDelta->Add( m_stPosDeltaUnits, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );


	bPositionColumn->Add( fgDelta, 0, wxEXPAND|wxALL, 5 );


	bMiddleSizer->Add( bPositionColumn, 0, wxEXPAND, 5 );


	bMainSizer->Add( bMiddleSizer, 0, wxEXPAND, 5 );

	m_sdbSizer = new wxStdDialogButtonSizer();
	m_sdbSizerOK = new wxButton( this, wxID_OK );
	m_sdbSizer->AddButton( m_sdbSizerOK );
	m_sdbSizerCancel = new wxButton( this, wxID_CANCEL );
	m_sdbSizer->AddButton( m_sdbSizerCancel );
	m_sdbSizer->Realize();

	bMainSizer->Add( m_sdbSizer, 0, wxALL|wxEXPAND, 5 );


	this->SetSizer( bMainSizer );
	this->Layout();
	bMainSizer->Fit( this );

	this->Centre( wxBOTH );

	// Connect Events
	m_cbFixedSize->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( DIALOG_SILKSCREEN_AUTO_PLACER_BASE::onFixedSizeCheck ), NULL, this );
	m_cbFixedWidth->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( DIALOG_SILKSCREEN_AUTO_PLACER_BASE::onFixedWidthCheck ), NULL, this );
}

DIALOG_SILKSCREEN_AUTO_PLACER_BASE::~DIALOG_SILKSCREEN_AUTO_PLACER_BASE()
{
	// Disconnect Events
	m_cbFixedSize->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( DIALOG_SILKSCREEN_AUTO_PLACER_BASE::onFixedSizeCheck ), NULL, this );
	m_cbFixedWidth->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( DIALOG_SILKSCREEN_AUTO_PLACER_BASE::onFixedWidthCheck ), NULL, this );

}
