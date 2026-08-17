/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2022 Mark Roszko <mark.roszko@gmail.com>
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

#include "command_pcb_export_base.h"
#include <bitset>
#include <string_utils.h>


CLI::PCB_EXPORT_BASE_COMMAND::PCB_EXPORT_BASE_COMMAND( const std::string& aName, IO_TYPE aInputType,
                                                       IO_TYPE aOutputType ) :
        COMMAND( aName )
{
    addCommonArgs( true, true, aInputType, aOutputType );
}


void CLI::PCB_EXPORT_BASE_COMMAND::addLayerArg()
{
    m_argParser.add_argument( "-l", ARG_LAYERS )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Comma separated list of untranslated layer names to include "
                                  "such as F.Cu,B.Cu" ) ) )
            .metavar( "LAYER_LIST" );
}


void CLI::PCB_EXPORT_BASE_COMMAND::addCommonLayersArg()
{
    m_argParser.add_argument( "--cl", ARG_COMMON_LAYERS )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Layers to include on each plot, comma separated list of "
                                  "untranslated layer names to include such as F.Cu,B.Cu" ) ) )
            .metavar( "COMMON_LAYER_LIST" );
}


void CLI::PCB_EXPORT_BASE_COMMAND::addDNPArgs()
{
    m_argParser.add_argument( ARG_HIDE_DNP_FPS_ON_FAB_LAYERS_SHORT, ARG_HIDE_DNP_FPS_ON_FAB_LAYERS )
            .help( UTF8STDSTR( _( ARG_HIDE_DNP_FPS_ON_FAB_LAYERS_DESC ) ) )
            .flag();

    m_argParser.add_argument( ARG_SKETCH_DNP_FPS_ON_FAB_LAYERS_SHORT, ARG_SKETCH_DNP_FPS_ON_FAB_LAYERS )
            .help( UTF8STDSTR( _( ARG_SKETCH_DNP_FPS_ON_FAB_LAYERS_DESC ) ) )
            .flag();

    m_argParser.add_argument( ARG_CROSSOUT_DNP_FPS_ON_FAB_LAYERS_SHORT, ARG_CROSSOUT_DNP_FPS_ON_FAB_LAYERS )
            .help( UTF8STDSTR( _( ARG_CROSSOUT_DNP_FPS_ON_FAB_LAYERS_DESC ) ) )
            .flag();
}


void CLI::PCB_EXPORT_BASE_COMMAND::addReadabilityArgs()
{
    m_argParser.add_argument( ARG_SKETCH_PADS_ON_FAB_LAYERS_SHORT, ARG_SKETCH_PADS_ON_FAB_LAYERS )
            .help( UTF8STDSTR( _( ARG_SKETCH_PADS_ON_FAB_LAYERS_DESC ) ) )
            .flag();

    m_argParser.add_argument( ARG_SKETCH_PAD_NUMBERS_SHORT, ARG_SKETCH_PAD_NUMBERS )
            .help( UTF8STDSTR( _( ARG_SKETCH_PAD_NUMBERS_DESC ) ) )
            .flag();

    m_argParser.add_argument( ARG_EXCLUDE_REFDES_SHORT, ARG_EXCLUDE_REFDES )
            .help( UTF8STDSTR( _( "Exclude the reference designator text" ) ) )
            .flag();

    m_argParser.add_argument( ARG_EXCLUDE_VALUE_SHORT, ARG_EXCLUDE_VALUE )
            .help( UTF8STDSTR( _( "Exclude the value text" ) ) )
            .flag();
}
