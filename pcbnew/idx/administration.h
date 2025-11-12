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


#ifndef IDX_ADMINISTRATION_H
#define IDX_ADMINISTRATION_H
#include <idx/idx_base.h>
#include <idx/foundation.h>
#include <memory>


namespace idx_administration
{

class EDMD_ACTOR : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_ACTOR() :
            idx_foundation::EDMD_COLLABORATION_OBJECT() {};
    EDMD_ACTOR( IDX_XML_NODE& aNode ) :
            idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false ) {};
};
/**
     * @brief Representation of a natural person
     * 
     */

class EDMD_PERSON : public EDMD_ACTOR
{
public:
    EDMD_PERSON( std::string aSurname, std::string aGivenName, std::string aInitials, std::string aEmailAddress,
                 std::string aPhoneNumber, std::string aFaxNumber ) :
            EDMD_ACTOR(),
            m_surname( aSurname ),
            m_givenName( aGivenName ),
            m_initials( aInitials ),
            m_emailAddress( aEmailAddress ),
            m_phoneNumber( aPhoneNumber ),
            m_faxNumber( aFaxNumber ) {};

    EDMD_PERSON( IDX_XML_NODE& aNode ) :
            EDMD_ACTOR( aNode )
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
            else if( IDX_keyword_compare( name, "adminitration:EMailAddress" ) )
                m_emailAddress = content;
            else if( IDX_keyword_compare( name, "adminitration:PhoneNumber" ) )
                m_phoneNumber = content;
            else if( IDX_keyword_compare( name, "administration:FaxNumber" ) )
                m_faxNumber = content;
            else
            {
            }
            child = child->GetNext();
        }
    }

    std::string m_surname;
    std::string m_givenName;
    std::string m_initials;
    std::string m_emailAddress;
    std::string m_phoneNumber;
    std::string m_faxNumber;
};

/**
     * @brief Represents an organisation or site
     * 
     */
class EDMD_ORGANISATION
{
public:
    EDMD_ORGANISATION( std::string aOrganisationNAme, std::string aSite, std::string aStreetNumber, std::string aStreet,
                       std::string aPCcode, std::string aCountry, std::string aUri ) :
            m_organisationName( aOrganisationNAme ),
            m_site( aSite ),
            m_streetNumber( aStreetNumber ),
            m_street( aStreet ),
            m_PCcode( aPCcode ),
            m_country( aCountry ),
            m_uri( aUri ) {};

    const std::string m_organisationName;
    const std::string m_site;
    const std::string m_streetNumber;
    const std::string m_street;
    const std::string m_PCcode;
    const std::string m_country;
    const std::string m_uri;
};

/**
     * @brief Describes a role of a person in an organisation or IT system
     * 
     * Context has to refer to an EDMDOrganisation or an EDMDSystem
     */
class EDMD_ROLEINORGANISATION
{
public:
    EDMD_ROLEINORGANISATION( std::shared_ptr<EDMD_ORGANISATION> aContext ) :
            m_context( aContext ) {};

    std::shared_ptr<EDMD_ORGANISATION> m_context;
};

/**
     * @brief Abstract super type for Roles
     * 
     */
class EDMD_ROLE : public IDX_BASE
{
public:
    EDMD_ROLE( std::string aActor = "" ) :
            IDX_BASE(),
            m_actor( aActor ),
            m_roleName() {};

    EDMD_ROLE( IDX_XML_NODE& aNode ) :
            IDX_BASE()
    {
        auto child = aNode.GetChildren();

        while( child )
        {
            const std::string content = child->GetNodeContent();
            const std::string name = child->GetName();

            if( IDX_keyword_compare( name, "administration:Actor" ) )
                m_actor = content;
            else if( IDX_keyword_compare( name, "administration:RoleName" ) )
                m_actor = content;
            else
            {
            }
            child = child->GetNext();
        }
    }


    std::string               m_actor;
    idx_foundation::EDMD_NAME m_roleName;
};


class EDMD_ROLE_ON_OBJECT : public EDMD_ROLE
{
public:
    EDMD_ROLE_ON_OBJECT() :
            EDMD_ROLE() {};

    EDMD_ROLE_ON_OBJECT( IDX_XML_NODE& aNode ) :
            EDMD_ROLE( aNode )
    {
        auto child = aNode.GetChildren();

        return;
        while( child )
        {
            const std::string content = child->GetNodeContent();
            const std::string name = child->GetName();

            if( IDX_keyword_compare( name, "administration:Actor" ) )
                m_actor = content;
            else if( IDX_keyword_compare( name, "administration:RoleName" ) )
                m_actor = content;
            else
            {
            }
            child = child->GetNext();
        }
    }

    enum class ROLE_ON_ITEM_TYPE
    {
        OWNER = 0,
        LISTENER,
        NOOWNER,
        NOLISTENER
    };


    ROLE_ON_ITEM_TYPE m_roleOnItemType;
};

class ADMINISTRATION : public IDX_BASE
{
public:
    ADMINISTRATION() :
            IDX_BASE() {};
};

} // namespace idx_administration
#endif