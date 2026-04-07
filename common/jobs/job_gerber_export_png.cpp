/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
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

#include <jobs/job_registry.h>
#include <jobs/job_gerber_export_png.h>
#include <i18n_utility.h>


JOB_GERBER_EXPORT_PNG::JOB_GERBER_EXPORT_PNG() :
        JOB( "gerber_export_png", false )
{
    m_params.emplace_back( new JOB_PARAM<int>( "dpi", &m_dpi, m_dpi ) );
    m_params.emplace_back( new JOB_PARAM<int>( "width", &m_width, m_width ) );
    m_params.emplace_back( new JOB_PARAM<int>( "height", &m_height, m_height ) );
    m_params.emplace_back( new JOB_PARAM<bool>( "antialias", &m_antialias, m_antialias ) );
    m_params.emplace_back(
            new JOB_PARAM<bool>( "transparent_background", &m_transparentBackground, m_transparentBackground ) );
}


wxString JOB_GERBER_EXPORT_PNG::GetDefaultDescription() const
{
    return _( "Gerber Export PNG" );
}


wxString JOB_GERBER_EXPORT_PNG::GetSettingsDialogTitle() const
{
    return _( "Gerber Export PNG Job Settings" );
}


REGISTER_JOB( gerber_export_png, _HKI( "Gerber: Export PNG" ), KIWAY::FACE_GERBVIEW, JOB_GERBER_EXPORT_PNG );
