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

#pragma once

#include <json_common.h>
#include <map>
#include <vector>
#include <wx/string.h>


/**
 * Parse a JSON array of pin-map objects into a symbol-pin to fp-pad(s) map.
 *
 * Each element must have the keys:
 *   "sym"  - the symbol pin number (string)
 *   "fp"   - the footprint pad number(s); may be a string or array of strings
 *
 * Elements missing either key are skipped silently.  Returns the same shape that
 * LIB_SYMBOL::SetPinMap consumes, so HTTP and database plugins can hand the result
 * straight to the symbol they construct.
 */
inline std::map<wxString, std::vector<wxString>> ParsePinMapJson( const nlohmann::json& aArray )
{
    std::map<wxString, std::vector<wxString>> assignments;

    for( const auto& pa : aArray )
    {
        if( !pa.contains( "sym" ) || !pa.contains( "fp" ) )
            continue;

        wxString              symbolPin = wxString::FromUTF8( pa["sym"].get<std::string>() );
        std::vector<wxString> fpPins;
        const auto&           fp = pa["fp"];

        if( fp.is_array() )
        {
            for( const auto& pad : fp )
                fpPins.push_back( wxString::FromUTF8( pad.get<std::string>() ) );
        }
        else if( fp.is_string() )
        {
            fpPins.push_back( wxString::FromUTF8( fp.get<std::string>() ) );
        }

        if( !symbolPin.empty() && !fpPins.empty() )
            assignments[symbolPin] = std::move( fpPins );
    }

    return assignments;
}
