/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2024 KiCad Developers, see AUTHORS.txt for contributors.
 * @author Fabien Corona <f.corona<at>laposte.net>
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

#include <idx/idx_base.h>
#include <typeinfo>

// Here we define some helper functions, we don't want to use std::string.
// Hopefuly, this code will be shared with open sources MCAD softwares

bool IDX_keyword_compare( std::string a, std::string b )
{
    // case sensitive compare
    return a.compare(b) == 0;
}


bool IDX_BASE::Resolve( IDX_REGISTRY& aRegistry ) { return false; };

void IDX_BASE::ParseChildren( IDX_XML_NODE& node )
{
    for (auto child = node.GetChildren(); child; child = child->GetNext())
    {
        if (!child)
            continue;

        if (!this->Parse(*child))
            this->unknownKeywordIDX(child->GetName());
    }
}



void IDX_BASE::unknownKeywordIDX( std::string aName )
{
    this->Report("unknown keyword: " + aName + " for class " + typeid(*this).name(), WARNING);
}


void IDX_BASE::invalidValueIDX( std::string aKeyWord, std::string aValue )
{
    this->Report( "Invalid value " + aValue + " for " + aKeyWord , WARNING );
}