///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_gitcheck_job_settings_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_GITCHECK_JOB_SETTINGS_BASE::DIALOG_GITCHECK_JOB_SETTINGS_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );

	wxBoxSizer* bSizerMain;
	bSizerMain = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* bSizerOptions;
	bSizerOptions = new wxBoxSizer( wxVERTICAL );

	m_cbRequireCommit = new wxCheckBox( this, wxID_ANY, _("Require the project to be committed"), wxDefaultPosition, wxDefaultSize, 0 );
	m_cbRequireCommit->SetToolTip( _("If the project has uncommitted changes, show the commit dialog before the job continues.") );

	bSizerOptions->Add( m_cbRequireCommit, 0, wxALL, 5 );

	m_cbRequirePush = new wxCheckBox( this, wxID_ANY, _("Require the project to be pushed"), wxDefaultPosition, wxDefaultSize, 0 );
	m_cbRequirePush->SetToolTip( _("If the current branch has unpushed commits, push them before the job continues.") );

	bSizerOptions->Add( m_cbRequirePush, 0, wxALL, 5 );


	bSizerMain->Add( bSizerOptions, 1, wxALL|wxEXPAND, 5 );

	m_sdbSizer1 = new wxStdDialogButtonSizer();
	m_sdbSizer1OK = new wxButton( this, wxID_OK );
	m_sdbSizer1->AddButton( m_sdbSizer1OK );
	m_sdbSizer1Cancel = new wxButton( this, wxID_CANCEL );
	m_sdbSizer1->AddButton( m_sdbSizer1Cancel );
	m_sdbSizer1->Realize();

	bSizerMain->Add( m_sdbSizer1, 0, wxALL|wxEXPAND, 5 );


	this->SetSizer( bSizerMain );
	this->Layout();
	bSizerMain->Fit( this );

	this->Centre( wxBOTH );
}

DIALOG_GITCHECK_JOB_SETTINGS_BASE::~DIALOG_GITCHECK_JOB_SETTINGS_BASE()
{
}
