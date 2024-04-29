/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2023 Ethan Chien <liangtie.qian@gmail.com>
 * Copyright (C) 2023 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "command_pcb_convert.h"
#include "jobs/job_pcb_convert.h"


#include <cli/exit_codes.h>
#include "jobs/job_pcb_render.h"
#include <kiface_base.h>
#include <layer_ids.h>
#include <string_utils.h>
#include <wx/crt.h>
#include <magic_enum.hpp>

#include <macros.h>
#include <wx/tokenzr.h>
#include <math/vector3.h>

#define ARG_FROM "--from"
#define ARG_TO "--to"

template <typename T>
static wxString enumString()
{
    wxString str;
    auto     names = magic_enum::enum_names<T>();

    for( size_t i = 0; i < names.size(); i++ )
    {
        std::string name = { names[i].begin(), names[i].end() };

        if( i > 0 )
            str << ", ";

        std::transform( name.begin(), name.end(), name.begin(),
                        []( unsigned char c )
                        {
                            return std::tolower( c );
                        } );

        str << name;
    }

    return str;
}


template <typename T>
static std::vector<std::string> enumChoices()
{
    std::vector<std::string> out;

    for( auto& strView : magic_enum::enum_names<T>() )
    {
        std::string name = { strView.begin(), strView.end() };

        std::transform( name.begin(), name.end(), name.begin(),
                        []( unsigned char c )
                        {
                            return std::tolower( c );
                        } );

        out.emplace_back( name );
    }

    return out;
}


template <typename T>
static bool getToEnum( const std::string& aInput, T& aOutput )
{
    // If not specified, leave at default
    if( aInput.empty() )
        return true;

    if( auto opt = magic_enum::enum_cast<T>( aInput, magic_enum::case_insensitive ) )
    {
        aOutput = *opt;
        return true;
    }

    return false;
}

CLI::COMMAND_PCB_CONVERT::COMMAND_PCB_CONVERT() : COMMAND( "convert" )
{
    addCommonArgs( true, true, false, false );
    addDefineArg();

    m_argParser.add_description(
            UTF8STDSTR( _( "Convert a vendor's PCB documentation to another vendor's PCB documentation" ) ) );


    m_argParser.add_argument( ARG_FROM )
            .default_value( std::string( "altium_designer" ) )
            .add_choices( enumChoices<JOB_PCB_CONVERT::From>() )
            .metavar( "FROM" )
            .help( UTF8STDSTR( wxString::Format( _( "Accepted PCB format. Options: %s" ),
                                                 enumString<JOB_PCB_CONVERT::From>() ) ) );


    m_argParser.add_argument( ARG_TO )
            .default_value( std::string( "kicad_sexp" ) )
            .add_choices( enumChoices<JOB_PCB_CONVERT::To>() )
            .metavar( "TO" )
            .help( UTF8STDSTR( wxString::Format( _( "Converted format. Options: %s" ),
                                                 enumString<JOB_PCB_CONVERT::To>() ) ) );

}


int CLI::COMMAND_PCB_CONVERT::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_PCB_CONVERT> convertJob( new JOB_PCB_CONVERT( true ) );

    convertJob->m_outputFile = m_argOutput;
    convertJob->m_filename = m_argInput;
    convertJob->SetVarOverrides( m_argDefineVars );
    getToEnum( m_argParser.get<std::string>( ARG_FROM ), convertJob->m_from );
    getToEnum( m_argParser.get<std::string>( ARG_TO ), convertJob->m_to );
    int exitCode = aKiway.ProcessJob( KIWAY::FACE_PCB, convertJob.get() );
    return exitCode;
}
