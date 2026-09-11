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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "dialog_gitcheck_job_settings.h"
#include <jobs/job_special_git_check.h>


DIALOG_GITCHECK_JOB_SETTINGS::DIALOG_GITCHECK_JOB_SETTINGS( wxWindow* aParent,
                                                            JOB_SPECIAL_GIT_CHECK* aJob ) :
        DIALOG_GITCHECK_JOB_SETTINGS_BASE( aParent ),
        m_job( aJob )
{
    SetupStandardButtons();
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
