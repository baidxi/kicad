/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2024 KiCad Developers, see AUTHORS.txt for contributors.
 * @author Fabien Corona <f.corona at laposte.net>
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
 * along with this program; if not, you may write to the Free Software Foundation, Inc.,
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <idx/kidx/schema/material.h>
#include <iostream>

namespace idx_material
{

// ------------------- EDMD_MATERIAL -------------------

EDMD_MATERIAL::EDMD_MATERIAL( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_MATERIAL::Parse( IDX_XML_NODE& aNode )
{
    // The material schema currently doesn't define any specific elements
    // beyond what's inherited from EDMDCollaborationObject.
    // However, we parse common material properties that might be present
    // in extended implementations or future versions.

    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        // Try to parse common material properties
        if( IDX_keyword_compare( name, "material:Name" ) || IDX_keyword_compare( name, "Name" ) )
        {
            m_name = content;
        }
        else if( IDX_keyword_compare( name, "material:Description" ) || IDX_keyword_compare( name, "Description" ) )
        {
            m_description = content;
        }
        else if( IDX_keyword_compare( name, "material:Type" ) || IDX_keyword_compare( name, "Type" ) )
        {
            m_type = content;
        }
        else if( IDX_keyword_compare( name, "material:Manufacturer" ) || IDX_keyword_compare( name, "Manufacturer" ) )
        {
            m_manufacturer = content;
        }
        else if( IDX_keyword_compare( name, "material:PartNumber" ) || IDX_keyword_compare( name, "PartNumber" ) )
        {
            m_partNumber = content;
        }
        else if( IDX_keyword_compare( name, "material:Color" ) || IDX_keyword_compare( name, "Color" ) )
        {
            m_color = content;
        }
        else
        {
            // Try base class parsing - this handles foundation elements
            if( !this->idx_foundation::EDMD_COLLABORATION_OBJECT::Parse( *child ) )
                unknownKeywordIDX( name );
        }
        child = child->GetNext();
    }
    return true;
}

void EDMD_MATERIAL::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_COLLABORATION_OBJECT::Write( aNode );

    // Write material-specific properties if they are set
    WriteValueChild( aNode, "material:Name", m_name );
    WriteValueChild( aNode, "material:Description", m_description );
    WriteValueChild( aNode, "material:Type", m_type );
    WriteValueChild( aNode, "material:Manufacturer", m_manufacturer );
    WriteValueChild( aNode, "material:PartNumber", m_partNumber );
    WriteValueChild( aNode, "material:Color", m_color );

    // Note: Numeric properties are not written as they may not be standard
    // in the current schema version, but the infrastructure is in place
    // for when they are needed.
}

} // namespace idx_material
