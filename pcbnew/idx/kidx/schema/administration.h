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
#include <idx/kidx/schema/idx_base.h>
#include <idx/kidx/schema/foundation.h>
#include <memory>


namespace idx_administration
{

/**
     * @brief Representation of a natural person
     * 
     */

class EDMD_PERSON : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_PERSON( std::string aSurname, std::string aGivenName, std::string aInitials, std::string aEmailAddress,
                 std::string aPhoneNumber, std::string aFaxNumber ) :
            idx_foundation::EDMD_COLLABORATION_OBJECT(),
            m_surname( aSurname ),
            m_givenName( aGivenName ),
            m_initials( aInitials ),
            m_emailAddress( aEmailAddress ),
            m_phoneNumber( aPhoneNumber ),
            m_faxNumber( aFaxNumber ) {};

    EDMD_PERSON( IDX_XML_NODE& aNode, bool aTopLevel = true ) :
            idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
    {
        if( aTopLevel )
            ParseChildren( aNode );
    }

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

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
class EDMD_ORGANISATION : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_ORGANISATION() :
            idx_foundation::EDMD_COLLABORATION_OBJECT()
    {
    }
    EDMD_ORGANISATION( IDX_XML_NODE& aNode, bool aTopLevel );

    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;

    std::string m_organisationName;
    std::string m_site;
    std::string m_streetNumber;
    std::string m_street;
    std::string m_PCode;
    std::string m_country;
    std::string m_uri;
};

/**
     * @brief Describes a role of a person in an organisation or IT system
     * 
     * Context has to refer to an EDMDOrganisation or an EDMDSystem
     */
class EDMD_ROLE_IN_ORGANISATION : public IDX_BASE
{
public:
    EDMD_ROLE_IN_ORGANISATION( std::shared_ptr<EDMD_ORGANISATION> aContext ) :
            m_context( aContext ) {};

    EDMD_ROLE_IN_ORGANISATION( IDX_XML_NODE& aNode, bool aTopLevel = true ) :
            IDX_BASE()
    {
        if( aTopLevel )
            ParseChildren( aNode );
    }

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const;

    std::string                        m_actor;
    idx_foundation::EDMD_NAME          m_roleName;
    std::string                        m_contextRef;
    std::shared_ptr<EDMD_ORGANISATION> m_context;
};

// Enums for role types
enum class ROLE_ON_ITEM_CATEGORY
{
    ORGANISATIONAL = 0,
    MECHANICAL,
    ELECTRICAL,
    GENERIC
};

enum class ROLE_ON_ITEM_TYPE
{
    OWNER = 0,
    LISTENER,
    NOOWNER,
    NOLISTENER
};

enum class ROLE_ON_ITEM_FUNCTION
{
    PROJECT_MANAGEMENT = 0,
    DESIGN,
    DIGITAL_MOCKUP,
    LAYOUT,
    THERMAL,
    EMC
};

/**
     * @brief Abstract super type for Roles
     * 
     */
class EDMD_ROLE : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_ROLE( std::string aActor = "" ) :
            idx_foundation::EDMD_COLLABORATION_OBJECT(),
            m_actor( aActor ),
            m_roleName()
    {
    }

    EDMD_ROLE( IDX_XML_NODE& aNode, bool aTopLevel = true ) :
            idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
    {
        if( aTopLevel )
            ParseChildren( aNode );
    }

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    std::string               m_actor;
    idx_foundation::EDMD_NAME m_roleName;
};


class EDMD_ROLE_ON_OBJECT : public EDMD_ROLE
{
public:
    EDMD_ROLE_ON_OBJECT() :
            EDMD_ROLE()
    {
    }

    EDMD_ROLE_ON_OBJECT( IDX_XML_NODE& aNode, bool aTopLevel = true ) :
            EDMD_ROLE( aNode, aTopLevel )
    {
        // Handled by parent constructor
    }

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    std::string m_roleType;
    std::string m_category;
    std::string m_genericCategory;
    std::string m_function;
    std::string m_genericFunction;
};

class EDMD_ROLE_ON_ITEM : public EDMD_ROLE_ON_OBJECT
{
public:
    EDMD_ROLE_ON_ITEM() :
            EDMD_ROLE_ON_OBJECT()
    {
    }

    EDMD_ROLE_ON_ITEM( IDX_XML_NODE& aNode, bool aTopLevel = true ) :
            EDMD_ROLE_ON_OBJECT( aNode, aTopLevel )
    {
        // Handled by parent constructor
    }

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    std::string m_contextRef;
};

class EDMD_ROLE_ON_ITEM_INSTANCE : public EDMD_ROLE_ON_OBJECT
{
public:
    EDMD_ROLE_ON_ITEM_INSTANCE() :
            EDMD_ROLE_ON_OBJECT()
    {
    }

    EDMD_ROLE_ON_ITEM_INSTANCE( IDX_XML_NODE& aNode, bool aTopLevel = true ) :
            EDMD_ROLE_ON_OBJECT( aNode, aTopLevel )
    {
        // Handled by parent constructor
    }

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    std::string m_contextRef;
};

class EDMD_ROLE_ON_CONSTRAINT : public EDMD_ROLE_ON_OBJECT
{
public:
    EDMD_ROLE_ON_CONSTRAINT() :
            EDMD_ROLE_ON_OBJECT()
    {
    }

    EDMD_ROLE_ON_CONSTRAINT( IDX_XML_NODE& aNode, bool aTopLevel = true ) :
            EDMD_ROLE_ON_OBJECT( aNode, aTopLevel )
    {
        // Handled by parent constructor
    }

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    std::string m_contextRef;
};

class ADMINISTRATION : public IDX_BASE
{
public:
    ADMINISTRATION() :
            IDX_BASE() {};
};

} // namespace idx_administration
#endif