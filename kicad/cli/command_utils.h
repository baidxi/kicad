/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2024 Ethan Chien <liangtie.qian@gmail.com>
 * Copyright (C) 2024 KiCad Developers, see AUTHORS.txt for contributors.
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

#ifndef COMMAND_UTILS_H
#define COMMAND_UTILS_H
#include <magic_enum.hpp>
#include <wx/string.h>

template <typename T>
inline wxString enumString()
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
inline std::vector<std::string> enumChoices()
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
inline bool getToEnum( const std::string& aInput, T& aOutput )
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

#endif
