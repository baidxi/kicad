/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2025 Dmitry Rezvanov <dmitry.rezvanov@yandex.ru>
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dialog_gitcheck_job_settings.h"
#include <jobs/job_special_git_check.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/string.h>

DIALOG_GITCHECK_JOB_SETTINGS::DIALOG_GITCHECK_JOB_SETTINGS( wxWindow* aParent,
                                                             JOB_SPECIAL_GIT_CHECK* aJob ) :
        DIALOG_SHIM( aParent, wxID_ANY, _( "Git Check Job Settings" ) ),
        m_job( aJob ),
        m_cbRequireCommit( nullptr ),
        m_cbRequirePush( nullptr )
{
    wxBoxSizer* mainSizer = new wxBoxSizer( wxVERTICAL );

    wxStaticText* infoText = new wxStaticText( this, wxID_ANY,
            _( "Configure Git check requirements for this job:" ) );
    mainSizer->Add( infoText, 0, wxALL, 5 );

    m_cbRequireCommit = new wxCheckBox( this, wxID_ANY,
            _( "Require commit: Check if project has uncommitted changes and show commit dialog if needed" ) );
    mainSizer->Add( m_cbRequireCommit, 0, wxALL, 5 );

    m_cbRequirePush = new wxCheckBox( this, wxID_ANY,
            _( "Require push: Check if project has unpushed commits and push if needed" ) );
    mainSizer->Add( m_cbRequirePush, 0, wxALL, 5 );

    wxSizer* buttonSizer = CreateButtonSizer( wxOK | wxCANCEL );
    mainSizer->Add( buttonSizer, 0, wxEXPAND | wxALL, 5 );

    SetSizer( mainSizer );
    mainSizer->SetSizeHints( this );

    Bind( wxEVT_COMMAND_BUTTON_CLICKED, &DIALOG_GITCHECK_JOB_SETTINGS::onOK, this, wxID_OK );

    TransferDataToWindow();
}


DIALOG_GITCHECK_JOB_SETTINGS::~DIALOG_GITCHECK_JOB_SETTINGS()
{
}


bool DIALOG_GITCHECK_JOB_SETTINGS::TransferDataToWindow()
{
    m_cbRequireCommit->SetValue( m_job->m_requireCommit );
    m_cbRequirePush->SetValue( m_job->m_requirePush );
    return true;
}


bool DIALOG_GITCHECK_JOB_SETTINGS::TransferDataFromWindow()
{
    m_job->m_requireCommit = m_cbRequireCommit->GetValue();
    m_job->m_requirePush = m_cbRequirePush->GetValue();
    return true;
}


void DIALOG_GITCHECK_JOB_SETTINGS::onOK( wxCommandEvent& aEvent )
{
    if( TransferDataFromWindow() )
    {
        EndModal( wxID_OK );
    }
}
