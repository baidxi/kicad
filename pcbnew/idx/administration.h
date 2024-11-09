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
#include <memory>


namespace idx_administration
{

class EDMD_ACTOR
{
public:
    EDMD_ACTOR() {};
};
/**
     * @brief Representation of a natural person
     * 
     */

class EDMD_PERSON : EDMD_ACTOR
{
public:
    EDMD_PERSON( wxString aSurname, wxString aGivenName, wxString aInitials, wxString aEmailAddress,
                 wxString aPhoneNumber, wxString aFaxNumber ) :
            m_surname( aSurname ), m_givenName( aGivenName ), m_initials( aInitials ),
            m_emailAddress( aEmailAddress ), m_phoneNumber( aPhoneNumber ),
            m_faxNumber( aFaxNumber ), EDMD_ACTOR() {};

    const wxString m_surname;
    const wxString m_givenName;
    const wxString m_initials;
    const wxString m_emailAddress;
    const wxString m_phoneNumber;
    const wxString m_faxNumber;
};

/**
     * @brief Represents an organisation or site
     * 
     */
class EDMD_ORGANISATION
{
public:
    EDMD_ORGANISATION( wxString aOrganisationNAme, wxString aSite, wxString aStreetNumber,
                       wxString aStreet, wxString aPCcode, wxString aCountry, wxString aUri ) :
            m_organisationName( aOrganisationNAme ), m_site( aSite ),
            m_streetNumber( aStreetNumber ), m_street( aStreet ), m_PCcode( aPCcode ),
            m_country( aCountry ), m_uri( aUri ) {};

    const wxString m_organisationName;
    const wxString m_site;
    const wxString m_streetNumber;
    const wxString m_street;
    const wxString m_PCcode;
    const wxString m_country;
    const wxString m_uri;
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
class EDMD_ROLE
{
public:
    EDMD_ROLE( wxString aActor, wxString aRoleName ) :
            m_actor( aActor ), m_roleName( aRoleName ) {};

    const wxString m_actor;
    const wxString m_roleName;
};

class ADMINISTRATION : public IDX_BASE
{
public:
    ADMINISTRATION() :
            IDX_BASE() {

            };
};

} // namespace idx_administration
#endif