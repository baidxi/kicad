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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "command_sch_downgrade.h"
#include "jobs/job_sch_downgrade.h"
#include "cli/exit_codes.h"
#include <wx/crt.h>
#include <wx/file.h>

#define ARG_TARGET "--target"
#define ARG_FORCE "--force"

CLI::SCH_DOWNGRADE_COMMAND::SCH_DOWNGRADE_COMMAND() :
        COMMAND( "downgrade" )
{
    addCommonArgs( true, false, IO_TYPE::FILE, IO_TYPE::FILE );
    m_argParser.add_description(
            UTF8STDSTR( _( "Rewrite the schematic in place so an older KiCad version can open it" ) ) );

    m_argParser.add_argument( ARG_TARGET )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Target version name, e.g. \"KiCad 10.0\". Defaults to the newest." ) ) );

    m_argParser.add_argument( ARG_FORCE )
            .help( UTF8STDSTR( _( "Export even if some features must be dropped or approximated" ) ) )
            .flag();
}

int CLI::SCH_DOWNGRADE_COMMAND::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_SCH_DOWNGRADE> job = std::make_unique<JOB_SCH_DOWNGRADE>();

    job->m_filename = m_argInput;
    job->m_target = wxString::FromUTF8( m_argParser.get<std::string>( ARG_TARGET ) );
    job->m_force = m_argParser.get<bool>( ARG_FORCE );

    if( !wxFile::Exists( job->m_filename ) )
    {
        wxFprintf( stderr, _( "Schematic file does not exist or is not accessible\n" ) );
        return EXIT_CODES::ERR_INVALID_INPUT_FILE;
    }

    return aKiway.ProcessJob( KIWAY::FACE_SCH, job.get() );
}
