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

#include <idx/idx_base.h>
#include <wx/xml/xml.h>
#include <memory>
#include <map>

namespace idx_administration
{
class EDMD_PERSON;
}

namespace idx_property
{
class EDMD_USER_PROPERTY;
}


namespace idx_geometry
{
class EDMD_POINT;
}

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

/**
 * @brief Base class for EDMD objects.
 */
class EDMD_OBJECT : public IDX_BASE
{
public:
    EDMD_OBJECT();
    EDMD_OBJECT(wxXmlNode& aNode);

    bool Resolve(IDX_REGISTRY& aRegistry);

    bool operator==(const EDMD_OBJECT& rhs);

protected:
    std::map<wxString, std::shared_ptr<idx_foundation::EDMD_OBJECT>*> m_pointsToLink;

public:
    wxString m_id;
    bool     m_isAttributeChanged = false;
};


class EDMD_EXTENSIBLE_OBJECT : public EDMD_OBJECT
{
public:
    EDMD_EXTENSIBLE_OBJECT();
    EDMD_EXTENSIBLE_OBJECT(wxXmlNode& aNode);

    idx_property::EDMD_USER_PROPERTY* GetUserProperty(const wxString& aKey);
    bool Resolve(IDX_REGISTRY& aRegistry);

    wxString                                      m_name;
    wxString                                      m_description;
    std::vector<EDMD_NAME>                        m_displayDescription;
    std::vector<EDMD_NAME>                        m_displayName;
    std::vector<idx_property::EDMD_USER_PROPERTY> m_userProperty;
};


class EDMD_COLLABORATION_OBJECT : public EDMD_EXTENSIBLE_OBJECT
{
public:
    EDMD_COLLABORATION_OBJECT();
    EDMD_COLLABORATION_OBJECT(wxXmlNode& aNode);

    bool Resolve(IDX_REGISTRY& aRegistry);
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

    EDMD_SYSTEM(wxXmlNode& aNode);
    bool Resolve(IDX_REGISTRY& aRegistry);

    SYSTEM_TYPE m_systemType = SYSTEM_TYPE::UNDEFINED;
    wxString    m_globalName;
    wxString    m_webServiceURI;
    wxString    m_revision;
    wxString    m_systemName;
    wxString    m_instanceName;
    wxString    m_systemOwner;
    bool        m_needsSelfContainedMessages = false;
};


class EDMD_HEADER : public EDMD_EXTENSIBLE_OBJECT
{
public:
    EDMD_HEADER(wxXmlNode& aNode);

    wxString m_creatorName;
    wxString m_creatorCompany;
    wxString m_creatorSystem;
    wxString m_postProcessor;
    wxString m_postProcessorVersion;
    std::shared_ptr<idx_administration::EDMD_PERSON> m_creator;
    wxDateTime                                       m_CreationDateTime;
    wxDateTime                                       m_ModifiedDateTime;
    bool                                             isSelfContained = true;
};


class EDMD_DATASET_BODY : public IDX_BASE
{
public:
    EDMD_DATASET_BODY();
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
    EDMD_DATASET(wxXmlNode& aNode);

    EDMD_HEADER                           m_header;
    EDMD_DATASET_BODY                     m_body;
    std::vector<EDMD_PROCESS_INSTRUCTION> m_processInstruction;
};


class EDMD_DATASETBODY : public EDMD_OBJECT
{
public:
    EDMD_DATASETBODY(wxXmlNode& aNode);
};


class EDMD_DESIGNATOR : public EDMD_EXTENSIBLE_OBJECT
{
public:
    EDMD_DESIGNATOR();
    EDMD_DESIGNATOR(wxXmlNode& aNode);

    wxString m_systemScopeRef;
    bool     m_persistant = false;
    wxString m_originatorId;
    bool     m_isOriginator = false;
};


class EDMD_IDENTIFIER : public EDMD_DESIGNATOR
{
public:
    EDMD_IDENTIFIER(wxXmlNode& aNode);

    wxString                                      m_number;
    wxString                                      m_version;
    wxString                                      m_revision;
    wxString                                      m_sequence;
    std::vector<EDMD_NAME>                        m_name;
    std::vector<EDMD_NAME>                        m_displayName;
    std::vector<idx_property::EDMD_USER_PROPERTY> m_userProperty;
};


class EDMD_NAME : public EDMD_DESIGNATOR
{
public:
    EDMD_NAME();
    EDMD_NAME(wxXmlNode& aNode);

    wxString m_objectName;
};

} // namespace idx_foundation

#endif // IDX_FOUNDATION_HEADER_H
