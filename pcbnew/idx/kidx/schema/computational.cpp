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


void EDMD_PROCESS_INSTRUCTION_SEND_CHANGES::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_PROCESS_INSTRUCTION::Write( aNode );

    WriteValueChild( aNode, std::string( "computational:Actor" ), m_actorRef );

    if( m_transaction )
    {
        std::shared_ptr<IDX_XML_NODE> changesNode = aNode.AppendChild( std::string( "computational:Changes" ) );
        m_transaction->Write( *changesNode );
    }
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
    m_processType = idx_foundation::EDMD_PROCESS_INSTRUCTION::PROCESS_TYPE::SEND_INFORMATION;
    if( aTopLevel )
        ParseChildren( aNode );
}


bool EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "Actor" ) )
    {
        m_actorRef = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "InformationType" ) )
    {
        // Parse information type - this is an array type in schema
        // For now, just store the content as a string
        // TODO: properly parse the array
        // For simplicity, we'll just parse the first value
        if( content.find( "Identification" ) != std::string::npos )
            m_informationType.push_back( INFORMATION_TYPE::IDENTIFICATION );
        else if( content.find( "Structure" ) != std::string::npos )
            m_informationType.push_back( INFORMATION_TYPE::STRUCTURE );
        else if( content.find( "Classification" ) != std::string::npos )
            m_informationType.push_back( INFORMATION_TYPE::CLASSIFICATION );
        else if( content.find( "Geometry" ) != std::string::npos )
            m_informationType.push_back( INFORMATION_TYPE::GEOMETRY );
        else if( content.find( "External" ) != std::string::npos )
            m_informationType.push_back( INFORMATION_TYPE::EXTERNAL );
        else if( content.find( "Ownership" ) != std::string::npos )
            m_informationType.push_back( INFORMATION_TYPE::OWNERSHIP );
        else if( content.find( "All" ) != std::string::npos )
            m_informationType.push_back( INFORMATION_TYPE::ALL );
        return true;
    }

    return this->EDMD_PROCESS_INSTRUCTION::Parse( aNode );
}


void EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_PROCESS_INSTRUCTION::Write( aNode );

    WriteValueChild( aNode, std::string( "Actor" ), m_actorRef );

    // Write InformationType
    std::string infoTypeStr;
    for( size_t i = 0; i < m_informationType.size(); ++i )
    {
        if( i > 0 )
            infoTypeStr += " ";
        switch( m_informationType[i] )
        {
        case INFORMATION_TYPE::IDENTIFICATION: infoTypeStr += "Identification"; break;
        case INFORMATION_TYPE::STRUCTURE: infoTypeStr += "Structure"; break;
        case INFORMATION_TYPE::CLASSIFICATION: infoTypeStr += "Classification"; break;
        case INFORMATION_TYPE::GEOMETRY: infoTypeStr += "Geometry"; break;
        case INFORMATION_TYPE::EXTERNAL: infoTypeStr += "External"; break;
        case INFORMATION_TYPE::OWNERSHIP: infoTypeStr += "Ownership"; break;
        case INFORMATION_TYPE::ALL: infoTypeStr += "All"; break;
        }
    }
    if( !infoTypeStr.empty() )
        WriteValueChild( aNode, std::string( "InformationType" ), infoTypeStr );
}


// -----------------------------
// EDMD_PROCESS_INSTRUCTION_SEND_INFORMATION
// -----------------------------
EDMD_PROCESS_INSTRUCTION_SEND_INFORMATION::EDMD_PROCESS_INSTRUCTION_SEND_INFORMATION() :
        idx_foundation::EDMD_PROCESS_INSTRUCTION()
{
}

EDMD_PROCESS_INSTRUCTION_SEND_INFORMATION::EDMD_PROCESS_INSTRUCTION_SEND_INFORMATION( IDX_XML_NODE& aNode,
                                                                                      bool          aTopLevel ) :
        idx_foundation::EDMD_PROCESS_INSTRUCTION( aNode, false )
{
    m_processType = idx_foundation::EDMD_PROCESS_INSTRUCTION::PROCESS_TYPE::SEND_INFORMATION;
    if( aTopLevel )
        ParseChildren( aNode );
}


bool EDMD_PROCESS_INSTRUCTION_SEND_INFORMATION::Parse( IDX_XML_NODE& aNode )
{
    return this->EDMD_PROCESS_INSTRUCTION::Parse( aNode );
}

void EDMD_PROCESS_INSTRUCTION_SEND_INFORMATION::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_PROCESS_INSTRUCTION::Write( aNode );
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


void EDMD_ABSTRACT_CHANGE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_EXTENSIBLE_OBJECT::Write( aNode );

    WriteValueChild( aNode, std::string( "computational:Actor" ), m_actorRef );
}

// -----------------------------
// EDMD_SERVICE_RESULT
// -----------------------------
EDMD_SERVICE_RESULT::EDMD_SERVICE_RESULT() :
        idx_foundation::EDMD_EXTENSIBLE_OBJECT()
{
}


EDMD_SERVICE_RESULT::EDMD_SERVICE_RESULT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_EXTENSIBLE_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}


bool EDMD_SERVICE_RESULT::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "Ok" ) )
    {
        m_ok = ( content == "true" || content == "1" );
        return true;
    }
    else if( IDX_keyword_compare( name, "ErrorCategory" ) )
    {
        if( IDX_keyword_compare( content, "NotAuthorized" ) )
            m_errorCategory = EDMD_ERROR_CATEGORY::NOT_AUTHORIZED;
        else if( IDX_keyword_compare( content, "NotAuthenticated" ) )
            m_errorCategory = EDMD_ERROR_CATEGORY::NOT_AUTHENTICATED;
        else if( IDX_keyword_compare( content, "InvalidMessage" ) )
            m_errorCategory = EDMD_ERROR_CATEGORY::INVALID_MESSAGE;
        else if( IDX_keyword_compare( content, "InvalidXMLFormat" ) )
            m_errorCategory = EDMD_ERROR_CATEGORY::INVALID_XML_FORMAT;
        else if( IDX_keyword_compare( content, "InvalidFormatConstraints" ) )
            m_errorCategory = EDMD_ERROR_CATEGORY::INVALID_FORMAT_CONSTRAINTS;
        else if( IDX_keyword_compare( content, "SystemError" ) )
            m_errorCategory = EDMD_ERROR_CATEGORY::SYSTEM_ERROR;
        else
            m_errorCategory = EDMD_ERROR_CATEGORY::OTHER;
        return true;
    }
    else if( IDX_keyword_compare( name, "ErrorMessageId" ) )
    {
        m_errorMessageId = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "ErrorMessageText" ) )
    {
        m_errorMessageText = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "DeveloperDetails" ) )
    {
        m_developerDetails = content;
        return true;
    }

    return this->EDMD_EXTENSIBLE_OBJECT::Parse( aNode );
}


void EDMD_SERVICE_RESULT::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_EXTENSIBLE_OBJECT::Write( aNode );

    WriteValueChild( aNode, std::string( "Ok" ), std::string( m_ok ? "true" : "false" ) );

    // Write ErrorCategory
    std::string errorCatStr;
    switch( m_errorCategory )
    {
    case EDMD_ERROR_CATEGORY::NOT_AUTHORIZED: errorCatStr = "NotAuthorized"; break;
    case EDMD_ERROR_CATEGORY::NOT_AUTHENTICATED: errorCatStr = "NotAuthenticated"; break;
    case EDMD_ERROR_CATEGORY::INVALID_MESSAGE: errorCatStr = "InvalidMessage"; break;
    case EDMD_ERROR_CATEGORY::INVALID_XML_FORMAT: errorCatStr = "InvalidXMLFormat"; break;
    case EDMD_ERROR_CATEGORY::INVALID_FORMAT_CONSTRAINTS: errorCatStr = "InvalidFormatConstraints"; break;
    case EDMD_ERROR_CATEGORY::SYSTEM_ERROR: errorCatStr = "SystemError"; break;
    default: errorCatStr = "Other"; break;
    }
    WriteValueChild( aNode, std::string( "ErrorCategory" ), errorCatStr );

    WriteValueChild( aNode, std::string( "ErrorMessageId" ), m_errorMessageId );
    WriteValueChild( aNode, std::string( "ErrorMessageText" ), m_errorMessageText );
    WriteValueChild( aNode, std::string( "DeveloperDetails" ), m_developerDetails );
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


bool EDMD_TRANSACTION::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = true;
    for( auto& change : m_changes )
    {
        if( !change->Resolve( aRegistry ) )
            result = false;
    }
    return this->EDMD_ABSTRACT_CHANGE::Resolve( aRegistry ) && result;
}

void EDMD_TRANSACTION::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_ABSTRACT_CHANGE::Write( aNode );

    for( auto change : m_changes )
    {
        std::shared_ptr<IDX_XML_NODE> changeNode = aNode.AppendChild( std::string( "computational:Change" ) );
        change->Write( *changeNode );
    }
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


void EDMD_CHANGE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_ABSTRACT_CHANGE::Write( aNode );

    if( m_newItem )
    {
        std::shared_ptr<IDX_XML_NODE> newItemNode = aNode.AppendChild( std::string( "computational:NewItem" ) );
        m_newItem->Write( *newItemNode );
    }

    if( m_predecessorItem )
    {
        std::shared_ptr<IDX_XML_NODE> predNode = aNode.AppendChild( std::string( "computational:PredecessorItem" ) );
        m_predecessorItem->Write( *predNode );
    }

    for( const auto& name : m_deletedInstanceName )
    {
        std::shared_ptr<IDX_XML_NODE> delNode = aNode.AppendChild( std::string( "computational:DeletedInstanceName" ) );
        name->Write( *delNode );
    }
}


} // namespace idx_computational
