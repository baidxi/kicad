///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_edit_subsheet_pagenum_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_EDIT_SUBSHEET_PAGENUM_BASE::DIALOG_EDIT_SUBSHEET_PAGENUM_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 100,-1 ), wxDefaultSize );

	wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer( wxVERTICAL );

	m_staticTextDescription = new wxStaticText( this, wxID_ANY, _("Select an action for subsheet numbers:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextDescription->Wrap( -1 );
	bSizer1->Add( m_staticTextDescription, 0, wxALL, 5 );

	wxStaticBoxSizer* sbSizer4;
	sbSizer4 = new wxStaticBoxSizer( new wxStaticBox( this, wxID_ANY, _("Descripton") ), wxHORIZONTAL );

	m_textName = new wxTextCtrl( sbSizer4->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	sbSizer4->Add( m_textName, 0, wxALL, 5 );

	m_checkDefaultName = new wxCheckBox( sbSizer4->GetStaticBox(), wxID_ANY, _("Default"), wxDefaultPosition, wxDefaultSize, 0 );
	m_checkDefaultName->SetValue(true);
	sbSizer4->Add( m_checkDefaultName, 0, wxALL, 5 );


	bSizer1->Add( sbSizer4, 1, wxEXPAND, 5 );

	wxString m_radioOrientationChoices[] = { _("Top left"), _("Top center"), _("Top right"), _("Mid left"), _("Mid center"), _("Mid right"), _("Bottom left"), _("Bottom center"), _("Bottom right") };
	int m_radioOrientationNChoices = sizeof( m_radioOrientationChoices ) / sizeof( wxString );
	m_radioOrientation = new wxRadioBox( this, wxID_ANY, _("Orientation"), wxDefaultPosition, wxDefaultSize, m_radioOrientationNChoices, m_radioOrientationChoices, 3, wxRA_SPECIFY_COLS );
	m_radioOrientation->SetSelection( 0 );
	bSizer1->Add( m_radioOrientation, 0, wxALL|wxEXPAND, 5 );

	wxString m_radioPositionChoices[] = { _("Inside"), _("Outside") };
	int m_radioPositionNChoices = sizeof( m_radioPositionChoices ) / sizeof( wxString );
	m_radioPosition = new wxRadioBox( this, wxID_ANY, _("Position"), wxDefaultPosition, wxDefaultSize, m_radioPositionNChoices, m_radioPositionChoices, 1, wxRA_SPECIFY_ROWS );
	m_radioPosition->SetSelection( 0 );
	bSizer1->Add( m_radioPosition, 0, wxALL|wxEXPAND, 5 );

	wxString m_radioActionChoices[] = { _("Add"), _("Remove") };
	int m_radioActionNChoices = sizeof( m_radioActionChoices ) / sizeof( wxString );
	m_radioAction = new wxRadioBox( this, wxID_ANY, _("Action"), wxDefaultPosition, wxDefaultSize, m_radioActionNChoices, m_radioActionChoices, 1, wxRA_SPECIFY_ROWS );
	m_radioAction->SetSelection( 0 );
	bSizer1->Add( m_radioAction, 0, wxALL|wxEXPAND, 5 );

	m_sdbSizerStdButtons = new wxStdDialogButtonSizer();
	m_sdbSizerStdButtonsOK = new wxButton( this, wxID_OK );
	m_sdbSizerStdButtons->AddButton( m_sdbSizerStdButtonsOK );
	m_sdbSizerStdButtonsApply = new wxButton( this, wxID_APPLY );
	m_sdbSizerStdButtons->AddButton( m_sdbSizerStdButtonsApply );
	m_sdbSizerStdButtonsCancel = new wxButton( this, wxID_CANCEL );
	m_sdbSizerStdButtons->AddButton( m_sdbSizerStdButtonsCancel );
	m_sdbSizerStdButtons->Realize();

	bSizer1->Add( m_sdbSizerStdButtons, 1, wxEXPAND, 5 );


	this->SetSizer( bSizer1 );
	this->Layout();
	bSizer1->Fit( this );

	this->Centre( wxBOTH );

	// Connect Events
	m_checkDefaultName->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( DIALOG_EDIT_SUBSHEET_PAGENUM_BASE::OnChangedDefaultName ), NULL, this );
}

DIALOG_EDIT_SUBSHEET_PAGENUM_BASE::~DIALOG_EDIT_SUBSHEET_PAGENUM_BASE()
{
}
