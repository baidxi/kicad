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

#ifndef IDX_FOUNDATION_HEADER_H
#define IDX_FOUNDATION_HEADER_H

namespace idx_foundation
{

class EDMD_COLLABORATION_OBJECT;
class EDMD_DATASET;
class EDMD_DATASET_BODY;
class EDMD_DESIGNATOR;
class EDMD_EXTENSIBLE_OBJECT;
class EDMD_HEADER;
class EDMD_IDENTIFIER;
class EDMD_NAME;
class EDMD_OBJECT;
class EDMD_PROCESS_INSTRUCTION;
class EDMD_SYSTEM;
} // namespace idx_foundation

#include <idx/idx_base.h>
#include <idx/administration.h>
#include <idx/property.h>
#include <wx/xml/xml.h>
#include <memory>


namespace idx_foundation
{

class EDMD_OBJECT : public IDX_BASE
{
public:
    EDMD_OBJECT( wxXmlNode aNode ) : IDX_BASE()
    {
        wxString isAttributeChanged;
        aNode.GetAttribute( "id", &m_id );
        aNode.GetAttribute( "isAttributeChanged", &isAttributeChanged );
        isAttributeChanged == "false" ? false : true;
    };
    wxString m_id;
    bool     m_isAttributeChanged;
};


class EDMD_EXTENSIBLE_OBJECT : public EDMD_OBJECT
{
public:
    EDMD_EXTENSIBLE_OBJECT( wxXmlNode aNode ) : EDMD_OBJECT( aNode )
    {
        wxXmlNode* child = aNode.GetChildren();

        while( child )
        {
            const wxString content = child->GetNodeContent();
            const wxString name = child->GetName();

            if( name == "foundation:Name" )
                m_name = content;
            else if( name == "foundation:Description" )
                m_description = content;
            else if( name == "foundation:DisplayName" )
                m_displayName.emplace_back( *child );
            else if( name == "foundation:DisplayDescription" )
                m_displayDescription.emplace_back( *child );
            else if( name == "foundation:UserProperty" )
                m_userProperty.emplace_back( *child );
            else
            {
            }
            child = child->GetNext();
        }
    };

    wxString                                      m_name = "";
    wxString                                      m_description = "";
    std::vector<EDMD_NAME>                        m_displayDescription;
    std::vector<EDMD_NAME>                        m_displayName;
    std::vector<idx_property::EDMD_USER_PROPERTY> m_userProperty;
};


class EDMD_COLLABORATION_OBJECT : public EDMD_EXTENSIBLE_OBJECT
{
public:
    EDMD_COLLABORATION_OBJECT( wxXmlNode aNode ) : EDMD_EXTENSIBLE_OBJECT( aNode ) {};
};


class EDMD_SYSTEM : public EDMD_COLLABORATION_OBJECT
{
public:
    enum class SYSTEM_TYPE
    {
        UNDEFINED = 0,
        MANUFACTURER,
        PLM,
        ECAD,
        MCAD,
        PROXY,
        LIBRARY,
        COMMONSTANDARD,
        FACTORYSTANDARD,
    };

    EDMD_SYSTEM( wxXmlNode aNode ) : EDMD_COLLABORATION_OBJECT( aNode )
    {
        wxXmlNode* child = aNode.GetChildren();

        while( child )
        {
            const wxString content = child->GetNodeContent();
            const wxString name = child->GetName();

            if( name == "foundation:GlobalName" )
                m_globalName = content;
            else if( name == "foundation:WebServiceURI" )
                m_webServiceURI = content;
            else if( name == "foundation:Revision" )
                m_revision = content;
            else if( name == "foundation:SystemName" )
                m_systemName = content;
            else if( name == "foundation:InstanceName" )
                m_instanceName = content;
            else if( name == "foundation:systemOwner" )
                m_systemOwner = content;
            else if( name == "foundation:NeedsSelfContainedMessages" )
            {
                m_needsSelfContainedMessages = content == "true" ? true : false;
            }
            else
            {
            }
            child = child->GetNext();
        }
    };

    SYSTEM_TYPE m_systemType = SYSTEM_TYPE::UNDEFINED;
    wxString    m_globalName = "";
    wxString    m_webServiceURI = "";
    wxString    m_revision = "";
    wxString    m_systemName = "";
    wxString    m_instanceName = "";
    wxString    m_systemOwner = "";
    bool        m_needsSelfContainedMessages = false;
};


class EDMD_HEADER : public EDMD_EXTENSIBLE_OBJECT
{
public:
    EDMD_HEADER( wxXmlNode& aNode ) : EDMD_EXTENSIBLE_OBJECT( aNode )
    {
        wxXmlNode* child = aNode.GetChildren();

        while( child )
        {
            const wxString content = child->GetNodeContent();
            const wxString name = child->GetName();

            if( name == "foundation:Description" )
                m_description = content;
            else if( name == "foundation:CreatorName" )
                m_creatorName = content;
            else if( name == "foundation:CreatorCompany" )
                m_creatorCompany = content;
            else if( name == "foundation:CreatorSystem" )
                m_creatorSystem = content;
            else if( name == "foundation:PostProcessor" )
                m_postProcessor = content;
            else if( name == "foundation:PostProcessorVersion" )
                m_postProcessorVersion = content;
            else
            {
            }
            child = child->GetNext();
        }
    };

    wxString m_creatorName = "";
    wxString m_creatorCompany = "";
    wxString m_creatorSystem = "";
    wxString m_postProcessor = "";
    wxString m_postProcessorVersion = "";
    //std::shared_ptr<IDX_EDMD_SYSTEM> = nullptr;
    std::shared_ptr<idx_administration::EDMD_PERSON> m_creator;
    wxDateTime                                       m_CreationDateTime;
    wxDateTime                                       m_ModifiedDateTime;
    bool                                             isSelfContained = true;
};

class EDMD_DATASET_BODY : public IDX_BASE
{
public:
    EDMD_DATASET_BODY() : IDX_BASE() {};
};

class EDMD_PROCESS_INSTRUCTION : public EDMD_EXTENSIBLE_OBJECT
{
public:
    std::shared_ptr<EDMD_OBJECT> m_subject;
    std::shared_ptr<EDMD_OBJECT> m_object;
    std::shared_ptr<EDMD_SYSTEM> m_targetSystem;
};

class EDMD_DATASET : public EDMD_OBJECT
{
public:
    EDMD_DATASET( wxXmlNode aNode ) : m_header( aNode ), EDMD_OBJECT( aNode ) {};

    EDMD_HEADER                           m_header;
    EDMD_DATASET_BODY                     m_body;
    std::vector<EDMD_PROCESS_INSTRUCTION> m_processInstruction;
};

class EDMD_DATASETBODY : public EDMD_OBJECT
{
public:
    EDMD_DATASETBODY( wxXmlNode aNode ) : EDMD_OBJECT( aNode ) {};

    // To be completed
};

class EDMD_DESIGNATOR : public EDMD_EXTENSIBLE_OBJECT
{
public:
    EDMD_DESIGNATOR( wxXmlNode aNode ) : EDMD_EXTENSIBLE_OBJECT( aNode ) {};

    std::shared_ptr<EDMD_SYSTEM> m_systemScope;
    bool                         m_persistant = false;
    wxString                     m_originatorId = "";
    bool                         m_isOriginator = false;
};


class EDMD_IDENTIFIER : public EDMD_DESIGNATOR
{
public:
    EDMD_IDENTIFIER( wxXmlNode aNode ) : EDMD_DESIGNATOR( aNode ) {};

    wxString                                      m_number = "";
    wxString                                      m_version = "";
    wxString                                      m_revision = "";
    wxString                                      m_sequence = "";
    std::vector<EDMD_NAME>                        m_name;
    std::vector<EDMD_NAME>                        m_displayName;
    std::vector<idx_property::EDMD_USER_PROPERTY> m_userProperty;
};

class EDMD_NAME : public EDMD_DESIGNATOR
{
public:
    EDMD_NAME( wxXmlNode aNode ) : EDMD_DESIGNATOR( aNode ) {};
    wxString m_objectName;
};


} // namespace idx_foundation

#endif