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

#include <idx/kidx/schema/grouping.h>
#include <iostream>

namespace idx_grouping
{

// Helper function to parse item constraint type from string
EDMD_ITEM_CONSTRAINT_TYPE parseItemConstraintType( const std::string& value )
{
    if( IDX_keyword_compare( value, "relating" ) )
        return EDMD_ITEM_CONSTRAINT_TYPE::RELATING;
    else if( IDX_keyword_compare( value, "placement" ) )
        return EDMD_ITEM_CONSTRAINT_TYPE::PLACEMENT;
    else if( IDX_keyword_compare( value, "rotation" ) )
        return EDMD_ITEM_CONSTRAINT_TYPE::ROTATION;
    return EDMD_ITEM_CONSTRAINT_TYPE::RELATING;
}

// Helper function to write item constraint type to XML
void writeItemConstraintType( IDX_XML_NODE& aNode, const std::string& name, EDMD_ITEM_CONSTRAINT_TYPE value )
{
    const char* str = "relating";
    switch( value )
    {
        case EDMD_ITEM_CONSTRAINT_TYPE::RELATING:
            str = "relating";
            break;
        case EDMD_ITEM_CONSTRAINT_TYPE::PLACEMENT:
            str = "placement";
            break;
        case EDMD_ITEM_CONSTRAINT_TYPE::ROTATION:
            str = "rotation";
            break;
    }
    aNode.AppendChild( name )->SetText( str );
}

// ------------------- EDMD_GROUP -------------------

EDMD_GROUP::EDMD_GROUP( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_GROUP::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "grouping:Related" ) )
        {
            m_related.push_back( content );
        }
        else if( IDX_keyword_compare( name, "grouping:Owner" ) )
        {
            m_owner = content;
        }
        else
        {
            // Try base class parsing
            if( !this->idx_foundation::EDMD_COLLABORATION_OBJECT::Parse( *child ) )
                unknownKeywordIDX( name );
        }
        child = child->GetNext();
    }
    return true;
}

void EDMD_GROUP::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_COLLABORATION_OBJECT::Write( aNode );

    // Write Related elements
    for( const auto& related : m_related )
    {
        aNode.AppendChild( "grouping:Related" )->SetText( related );
    }

    // Write Owner element if set
    if( !m_owner.empty() )
    {
        aNode.AppendChild( "grouping:Owner" )->SetText( m_owner );
    }
}

// ------------------- EDMD_ITEM_CONSTRAINT -------------------

EDMD_ITEM_CONSTRAINT::EDMD_ITEM_CONSTRAINT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_GROUP( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_ITEM_CONSTRAINT::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "grouping:ItemConstraintType" ) )
        {
            m_itemConstraintType = parseItemConstraintType( content );
        }
        else
        {
            // Try base class parsing
            if( !this->EDMD_GROUP::Parse( *child ) )
                unknownKeywordIDX( name );
        }
        child = child->GetNext();
    }
    return true;
}

void EDMD_ITEM_CONSTRAINT::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_GROUP::Write( aNode );

    // Write ItemConstraintType
    writeItemConstraintType( aNode, "grouping:ItemConstraintType", m_itemConstraintType );
}

// ------------------- EDMD_GENERAL_CLASSIFICATION -------------------

EDMD_GENERAL_CLASSIFICATION::EDMD_GENERAL_CLASSIFICATION( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_GROUP( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_GENERAL_CLASSIFICATION::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "grouping:Identifier" ) )
        {
            m_identifier.emplace_back( *child, true );
        }
        else
        {
            // Try base class parsing
            if( !this->EDMD_GROUP::Parse( *child ) )
                unknownKeywordIDX( name );
        }
        child = child->GetNext();
    }
    return true;
}

void EDMD_GENERAL_CLASSIFICATION::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_GROUP::Write( aNode );

    // Write Identifier elements
    for( const auto& identifier : m_identifier )
    {
        auto identNode = aNode.AppendChild( "grouping:Identifier" );
        identifier.Write( *identNode );
    }
}

} // namespace idx_grouping
