/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
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
 * along with this program; if not, you can find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may write to the Free Software Foundation, Inc.
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "administration.h"

namespace idx_administration
{

bool EDMD_PERSON::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "administration:SurName" ) )
            m_surname = content;
        else if( IDX_keyword_compare( name, "administration:GivenName" ) )
            m_givenName = content;
        else if( IDX_keyword_compare( name, "administration:Initials" ) )
            m_initials = content;
        else if( IDX_keyword_compare( name, "administration:EMailAddress" ) )
            m_emailAddress = content;
        else if( IDX_keyword_compare( name, "administration:PhoneNumber" ) )
            m_phoneNumber = content;
        else if( IDX_keyword_compare( name, "administration:FaxNumber" ) )
            m_faxNumber = content;
        else
            return this->EDMD_COLLABORATION_OBJECT::Parse( aNode );

        child = child->GetNext();
    }

    return true;
}


void EDMD_PERSON::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_COLLABORATION_OBJECT::Write( aNode );

    if( !m_surname.empty() )
        aNode.AppendChild( "administration:SurName" )->SetText( m_surname );
    if( !m_givenName.empty() )
        aNode.AppendChild( "administration:GivenName" )->SetText( m_givenName );
    if( !m_initials.empty() )
        aNode.AppendChild( "administration:Initials" )->SetText( m_initials );
    if( !m_emailAddress.empty() )
        aNode.AppendChild( "administration:EMailAddress" )->SetText( m_emailAddress );
    if( !m_phoneNumber.empty() )
        aNode.AppendChild( "administration:PhoneNumber" )->SetText( m_phoneNumber );
    if( !m_faxNumber.empty() )
        aNode.AppendChild( "administration:FaxNumber" )->SetText( m_faxNumber );
}


bool EDMD_ROLE::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "administration:Actor" ) )
            m_actor = content;
        else if( IDX_keyword_compare( name, "administration:RoleName" ) )
            m_roleName.m_objectName = content;
        else
            return this->EDMD_COLLABORATION_OBJECT::Parse( aNode );

        child = child->GetNext();
    }

    return true;
}

void EDMD_ROLE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_COLLABORATION_OBJECT::Write( aNode );

    if( !m_actor.empty() )
        aNode.AppendChild( "administration:Actor" )->SetText( m_actor );
    if( !m_roleName.m_objectName.empty() )
        aNode.AppendChild( "administration:RoleName" )->SetText( m_roleName.m_objectName );
}


bool EDMD_ROLE_ON_OBJECT::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "administration:RoleType" ) )
            m_roleType = content;
        else if( IDX_keyword_compare( name, "administration:Category" ) )
            m_category = content;
        else if( IDX_keyword_compare( name, "administration:GenericCategory" ) )
            m_genericCategory = content;
        else if( IDX_keyword_compare( name, "administration:Function" ) )
            m_function = content;
        else if( IDX_keyword_compare( name, "administration:GenericFunction" ) )
            m_genericFunction = content;
        else
            return this->EDMD_ROLE::Parse( aNode );

        child = child->GetNext();
    }

    return true;
}

void EDMD_ROLE_ON_OBJECT::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_ROLE::Write( aNode );

    if( !m_roleType.empty() )
        aNode.AppendChild( "administration:RoleType" )->SetText( m_roleType );
    if( !m_category.empty() )
        aNode.AppendChild( "administration:Category" )->SetText( m_category );
    if( !m_genericCategory.empty() )
        aNode.AppendChild( "administration:GenericCategory" )->SetText( m_genericCategory );
    if( !m_function.empty() )
        aNode.AppendChild( "administration:Function" )->SetText( m_function );
    if( !m_genericFunction.empty() )
        aNode.AppendChild( "administration:GenericFunction" )->SetText( m_genericFunction );
}


bool EDMD_ROLE_ON_ITEM::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "administration:Context" ) )
            m_contextRef = content;
        else
            return this->EDMD_ROLE_ON_OBJECT::Parse( aNode );

        child = child->GetNext();
    }

    return true;
}

void EDMD_ROLE_ON_ITEM::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_ROLE_ON_OBJECT::Write( aNode );

    if( !m_contextRef.empty() )
        aNode.AppendChild( "administration:Context" )->SetText( m_contextRef );
}


bool EDMD_ROLE_ON_ITEM_INSTANCE::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "administration:Context" ) )
            m_contextRef = content;
        else
            return this->EDMD_ROLE_ON_OBJECT::Parse( aNode );

        child = child->GetNext();
    }

    return true;
}

void EDMD_ROLE_ON_ITEM_INSTANCE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_ROLE_ON_OBJECT::Write( aNode );

    if( !m_contextRef.empty() )
        aNode.AppendChild( "administration:Context" )->SetText( m_contextRef );
}


bool EDMD_ROLE_ON_CONSTRAINT::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "administration:Context" ) )
            m_contextRef = content;
        else
            return this->EDMD_ROLE_ON_OBJECT::Parse( aNode );

        child = child->GetNext();
    }

    return true;
}

void EDMD_ROLE_ON_CONSTRAINT::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_ROLE_ON_OBJECT::Write( aNode );

    if( !m_contextRef.empty() )
        aNode.AppendChild( "administration:Context" )->SetText( m_contextRef );
}


bool EDMD_ROLE_IN_ORGANISATION::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "administration:Actor" ) )
            m_actor = content;
        else if( IDX_keyword_compare( name, "administration:RoleName" ) )
            m_roleName.m_objectName = content;
        else if( IDX_keyword_compare( name, "administration:Context" ) )
            m_contextRef = content;
        else
            return false;

        child = child->GetNext();
    }

    return true;
}

void EDMD_ROLE_IN_ORGANISATION::Write( IDX_XML_NODE& aNode ) const
{
    if( !m_actor.empty() )
        aNode.AppendChild( "administration:Actor" )->SetText( m_actor );
    if( !m_roleName.m_objectName.empty() )
        aNode.AppendChild( "administration:RoleName" )->SetText( m_roleName.m_objectName );
    if( !m_contextRef.empty() )
        aNode.AppendChild( "administration:Context" )->SetText( m_contextRef );
}


// ------------------- EDMD_ORGANISATION -------------------

EDMD_ORGANISATION::EDMD_ORGANISATION( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_ORGANISATION::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "administration:OrganisationName" ) )
        m_organisationName = content;
    else if( IDX_keyword_compare( name, "administration:Site" ) )
        m_site = content;
    else if( IDX_keyword_compare( name, "administration:StreetNumber" ) )
        m_streetNumber = content;
    else if( IDX_keyword_compare( name, "administration:Street" ) )
        m_street = content;
    else if( IDX_keyword_compare( name, "administration:PCode" ) )
        m_PCode = content;
    else if( IDX_keyword_compare( name, "administration:Country" ) )
        m_country = content;
    else if( IDX_keyword_compare( name, "administration:URI" ) )
        m_uri = content;
    else
        return this->EDMD_COLLABORATION_OBJECT::Parse( aNode );

    return true;
}

void EDMD_ORGANISATION::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_COLLABORATION_OBJECT::Write( aNode );

    WriteValueChild( aNode, "administration:OrganisationName", m_organisationName );
    WriteValueChild( aNode, "administration:Site", m_site );
    WriteValueChild( aNode, "administration:StreetNumber", m_streetNumber );
    WriteValueChild( aNode, "administration:Street", m_street );
    WriteValueChild( aNode, "administration:PCode", m_PCode );
    WriteValueChild( aNode, "administration:Country", m_country );
    WriteValueChild( aNode, "administration:URI", m_uri );
}

bool EDMD_ORGANISATION::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );
}

} // namespace idx_administration