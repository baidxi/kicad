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

#include "command_pcb_downgrade.h"
#include "jobs/job_pcb_downgrade.h"
#include "cli/exit_codes.h"
#include <wx/crt.h>
#include <wx/file.h>
#include <wx/filename.h>

#define ARG_TARGET "--target"
#define ARG_FORCE "--force"

CLI::PCB_DOWNGRADE_COMMAND::PCB_DOWNGRADE_COMMAND() :
        COMMAND( "downgrade" )
{
    addCommonArgs( true, true, IO_TYPE::FILE, IO_TYPE::FILE );
    m_argParser.add_description( UTF8STDSTR( _( "Export a copy of the board readable by an older KiCad version" ) ) );

    m_argParser.add_argument( ARG_TARGET )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Target version name, e.g. \"KiCad 10.0\". Defaults to the newest." ) ) );

    m_argParser.add_argument( ARG_FORCE )
            .help( UTF8STDSTR( _( "Export even if some features must be dropped or approximated" ) ) )
            .flag();
}

int CLI::PCB_DOWNGRADE_COMMAND::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_PCB_DOWNGRADE> job = std::make_unique<JOB_PCB_DOWNGRADE>();

    job->m_filename = m_argInput;
    job->m_outputFile = m_argOutput;
    job->m_target = wxString::FromUTF8( m_argParser.get<std::string>( ARG_TARGET ) );
    job->m_force = m_argParser.get<bool>( ARG_FORCE );

    if( !wxFile::Exists( job->m_filename ) )
    {
        wxFprintf( stderr, _( "Board file does not exist or is not accessible\n" ) );
        return EXIT_CODES::ERR_INVALID_INPUT_FILE;
    }

    if( job->m_outputFile.IsEmpty() )
    {
        wxFprintf( stderr, _( "An output file is required. Use --output.\n" ) );
        return EXIT_CODES::ERR_ARGS;
    }

    // Never write over the input. A failed export removes its output, which here would be the
    // user's original board.
    wxFileName inFn( job->m_filename );
    wxFileName outFn( job->m_outputFile );
    inFn.MakeAbsolute();
    outFn.MakeAbsolute();

    if( inFn.SameAs( outFn ) )
    {
        wxFprintf( stderr, _( "The output file must differ from the input file.\n" ) );
        return EXIT_CODES::ERR_ARGS;
    }

    return aKiway.ProcessJob( KIWAY::FACE_PCB, job.get() );
}
