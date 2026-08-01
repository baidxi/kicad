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

#include <idx/kidx/schema/external.h>
#include <iostream>

namespace idx_external
{

// ------------------- EDMD_EXTERNAL_SOURCE -------------------

EDMD_EXTERNAL_SOURCE::EDMD_EXTERNAL_SOURCE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_EXTERNAL_SOURCE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "MimeType" ) )
    {
        m_mimeType = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "Location" ) )
    {
        m_location = content;
        return true;
    }
    else
        return this->EDMD_COLLABORATION_OBJECT::Parse( aNode );
    return true;
}


void EDMD_EXTERNAL_SOURCE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_COLLABORATION_OBJECT::Write( aNode );

    WriteValueChild( aNode, "MimeType", m_mimeType );
    WriteValueChild( aNode, "Location", m_location );
}
// ------------------- EDMD_EXTERNAL_GEOMETRIC_MODEL -------------------

EDMD_EXTERNAL_GEOMETRIC_MODEL::EDMD_EXTERNAL_GEOMETRIC_MODEL( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_pdm::EDMD_SHAPE_DESCRIPTION( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_EXTERNAL_GEOMETRIC_MODEL::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "ExternalSource" ) )
    {
        m_externalSourceRef = content;
        return true;
    }
    else
        return this->EDMD_SHAPE_DESCRIPTION::Parse( aNode );
    return true;
}


void EDMD_EXTERNAL_GEOMETRIC_MODEL::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_SHAPE_DESCRIPTION::Write( aNode );

    WriteValueChild( aNode, "ExternalSource", m_externalSourceRef );
}

} // namespace idx_external
