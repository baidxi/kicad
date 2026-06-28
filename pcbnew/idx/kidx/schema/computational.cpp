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

#include "computational.h"

namespace idx_computational
{

// -----------------------------
// EDMD_PROCESS_INSTRUCTION_SEND_CHANGES
// -----------------------------
EDMD_PROCESS_INSTRUCTION_SEND_CHANGES::EDMD_PROCESS_INSTRUCTION_SEND_CHANGES() :
        idx_foundation::EDMD_PROCESS_INSTRUCTION()
{
}

EDMD_PROCESS_INSTRUCTION_SEND_CHANGES::EDMD_PROCESS_INSTRUCTION_SEND_CHANGES( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_PROCESS_INSTRUCTION( aNode, false )
{
    m_processType = idx_foundation::EDMD_PROCESS_INSTRUCTION::PROCESS_TYPE::SEND_CHANGES;
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_PROCESS_INSTRUCTION_SEND_CHANGES::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "computational:Actor" ) )
    {
        m_actorRef = content;
    }
    else if( IDX_keyword_compare( name, "computational:Changes" ) )
    {
        m_transaction = std::make_shared<EDMD_TRANSACTION>( aNode, true );
        return true;
    }
    else
        return this->EDMD_PROCESS_INSTRUCTION::Parse( aNode );
    return true;
}

// -----------------------------
// EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION
// -----------------------------
EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION::EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION() :
        idx_foundation::EDMD_PROCESS_INSTRUCTION()
{
}

EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION::EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION( IDX_XML_NODE& aNode,
                                                                                                    bool aTopLevel ) :
        idx_foundation::EDMD_PROCESS_INSTRUCTION( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}


// -----------------------------
// EDMD_PROCESS_INSTRUCTION_SEND_INFO
// -----------------------------
EDMD_PROCESS_INSTRUCTION_SEND_INFO::EDMD_PROCESS_INSTRUCTION_SEND_INFO() :
        idx_foundation::EDMD_PROCESS_INSTRUCTION()
{
}

EDMD_PROCESS_INSTRUCTION_SEND_INFO::EDMD_PROCESS_INSTRUCTION_SEND_INFO( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_PROCESS_INSTRUCTION( aNode, false )
{
    m_processType = idx_foundation::EDMD_PROCESS_INSTRUCTION::PROCESS_TYPE::BASELINE;
    if( aTopLevel )
        ParseChildren( aNode );
}


// -----------------------------
// EDMD_ABSTRACT_CHANGE
// -----------------------------
EDMD_ABSTRACT_CHANGE::EDMD_ABSTRACT_CHANGE() :
        idx_foundation::EDMD_EXTENSIBLE_OBJECT()
{
}

EDMD_ABSTRACT_CHANGE::EDMD_ABSTRACT_CHANGE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_EXTENSIBLE_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}


bool EDMD_ABSTRACT_CHANGE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "computational:Actor" ) )
    {
        m_actorRef = content;
        return true;
    }
    return this->EDMD_EXTENSIBLE_OBJECT::Parse( aNode );
}

// -----------------------------
// EDMD_SERVICE_RESULT
// -----------------------------
EDMD_SERVICE_RESULT::EDMD_SERVICE_RESULT() :
        idx_foundation::EDMD_EXTENSIBLE_OBJECT()
{
}


// -----------------------------
// EDMD_TRANSACTION
// -----------------------------
EDMD_TRANSACTION::EDMD_TRANSACTION() :
        EDMD_ABSTRACT_CHANGE()
{
}

EDMD_TRANSACTION::EDMD_TRANSACTION( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_ABSTRACT_CHANGE( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}


bool EDMD_TRANSACTION::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "computational:Change" ) )
    {
        std::string type = aNode.GetAttribute( "xsi:type", "" );
        // Default to EDMDChange if xsi:type is missing or matches
        bool isEDMDChange = type.empty() || IDX_keyword_compare( type, "computational:EDMDChange" )
                            || type.find( "EDMDChange" ) != std::string::npos;

        if( isEDMDChange )
            m_changes.push_back( std::make_shared<EDMD_CHANGE>( aNode, true ) );
        else
            invalidValueIDX( name, type );
        return true;
    }
    return this->EDMD_ABSTRACT_CHANGE::Parse( aNode );
}


// -----------------------------
// EDMD_CHANGE
// -----------------------------
EDMD_CHANGE::EDMD_CHANGE() :
        EDMD_ABSTRACT_CHANGE()
{
}

EDMD_CHANGE::EDMD_CHANGE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_ABSTRACT_CHANGE( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}


bool EDMD_CHANGE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "computational:NewItem" ) )
    {
        m_newItem = std::make_shared<idx_foundation::EDMD_IDENTIFIER>( aNode, true );
        return true;
    }
    else if( IDX_keyword_compare( name, "computational:PredecessorItem" ) )
    {
        m_predecessorItem = std::make_shared<idx_foundation::EDMD_IDENTIFIER>( aNode, true );
        return true;
    }
    else if( IDX_keyword_compare( name, "computational:DeletedInstanceName" ) )
    {
        m_deletedInstanceName.emplace_back( std::make_shared<idx_foundation::EDMD_NAME>( aNode, true ) );
        return true;
    }
    return this->EDMD_ABSTRACT_CHANGE::Parse( aNode );
}


} // namespace idx_computational
