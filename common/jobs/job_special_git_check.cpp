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

#include <jobs/job_special_git_check.h>
#include <jobs/job_registry.h>
#include <i18n_utility.h>

JOB_SPECIAL_GIT_CHECK::JOB_SPECIAL_GIT_CHECK() :
        JOB( "special_git_check", false ),
        m_requireCommit( true ),
        m_requirePush( true )
{
    m_params.emplace_back( new JOB_PARAM<bool>( "require_commit", &m_requireCommit, m_requireCommit ) );
    m_params.emplace_back( new JOB_PARAM<bool>( "require_push", &m_requirePush, m_requirePush ) );
}

wxString JOB_SPECIAL_GIT_CHECK::GetDefaultDescription() const
{
    return wxString( "Check Git commit and push status" );
}

REGISTER_JOB( special_git_check, _HKI( "Special: Check Git Commit and Push" ), KIWAY::KIWAY_FACE_COUNT,
              JOB_SPECIAL_GIT_CHECK );
