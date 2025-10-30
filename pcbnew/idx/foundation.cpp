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

#include "foundation.h"
#include <idx/property.h>
#include <iostream>


namespace idx_foundation
{

// -----------------------------------------------------
// EDMD_OBJECT
// -----------------------------------------------------

EDMD_OBJECT::EDMD_OBJECT() : IDX_BASE() {}

EDMD_OBJECT::EDMD_OBJECT(wxXmlNode& aNode) : IDX_BASE()
{
    wxString isAttributeChanged;
    aNode.GetAttribute("id", &m_id);
    aNode.GetAttribute("isAttributeChanged", &isAttributeChanged);
    m_isAttributeChanged = (isAttributeChanged == "false");
}

bool EDMD_OBJECT::Resolve(IDX_REGISTRY& aRegistry)
{
    return IDX_BASE::Resolve(aRegistry);
}

bool EDMD_OBJECT::operator==(const EDMD_OBJECT& rhs)
{
    return m_id == rhs.m_id;
}

// -----------------------------------------------------
// EDMD_EXTENSIBLE_OBJECT
// -----------------------------------------------------

EDMD_EXTENSIBLE_OBJECT::EDMD_EXTENSIBLE_OBJECT() : EDMD_OBJECT() {}

EDMD_EXTENSIBLE_OBJECT::EDMD_EXTENSIBLE_OBJECT(wxXmlNode& aNode)
    : EDMD_OBJECT(aNode)
{
    wxXmlNode* child = aNode.GetChildren();

    while (child)
    {
        const wxString content = child->GetNodeContent();
        const wxString name = child->GetName();

        if (name == "foundation:Name")
            m_name = content;
        else if (name == "foundation:Description")
            m_description = content;
        else if (name == "foundation:DisplayName")
            m_displayName.emplace_back(*child);
        else if (name == "foundation:DisplayDescription")
            m_displayDescription.emplace_back(*child);
        else if (name == "foundation:UserProperty")
            m_userProperty.emplace_back(*child);

        child = child->GetNext();
    }
}

idx_property::EDMD_USER_PROPERTY*
EDMD_EXTENSIBLE_OBJECT::GetUserProperty(const wxString& aKey)
{
    for (auto& prop : m_userProperty)
    {
        for (const auto& key : prop.m_keys)
        {
            if (key.m_objectName == aKey)
                return &prop;
        }
    }
    return nullptr;
}

bool EDMD_EXTENSIBLE_OBJECT::Resolve(IDX_REGISTRY& aRegistry)
{
    return EDMD_OBJECT::Resolve(aRegistry);
}

// -----------------------------------------------------
// EDMD_COLLABORATION_OBJECT
// -----------------------------------------------------

EDMD_COLLABORATION_OBJECT::EDMD_COLLABORATION_OBJECT() : EDMD_EXTENSIBLE_OBJECT() {}

EDMD_COLLABORATION_OBJECT::EDMD_COLLABORATION_OBJECT(wxXmlNode& aNode)
    : EDMD_EXTENSIBLE_OBJECT(aNode)
{
}

bool EDMD_COLLABORATION_OBJECT::Resolve(IDX_REGISTRY& aRegistry)
{
    return EDMD_EXTENSIBLE_OBJECT::Resolve(aRegistry);
}

// -----------------------------------------------------
// EDMD_SYSTEM
// -----------------------------------------------------

EDMD_SYSTEM::EDMD_SYSTEM(wxXmlNode& aNode) : EDMD_COLLABORATION_OBJECT(aNode)
{
    wxXmlNode* child = aNode.GetChildren();

    while (child)
    {
        const wxString content = child->GetNodeContent();
        const wxString name = child->GetName();

        if (name == "foundation:GlobalName")
            m_globalName = content;
        else if (name == "foundation:WebServiceURI")
            m_webServiceURI = content;
        else if (name == "foundation:Revision")
            m_revision = content;
        else if (name == "foundation:SystemName")
            m_systemName = content;
        else if (name == "foundation:InstanceName")
            m_instanceName = content;
        else if (name == "foundation:systemOwner")
            m_systemOwner = content;
        else if (name == "foundation:NeedsSelfContainedMessages")
            m_needsSelfContainedMessages = (content == "true");

        child = child->GetNext();
    }
}

bool EDMD_SYSTEM::Resolve(IDX_REGISTRY& aRegistry)
{    
    return EDMD_COLLABORATION_OBJECT::Resolve(aRegistry);
}

// -----------------------------------------------------
// EDMD_HEADER
// -----------------------------------------------------

EDMD_HEADER::EDMD_HEADER(wxXmlNode& aNode) : EDMD_EXTENSIBLE_OBJECT(aNode)
{
    wxXmlNode* child = aNode.GetChildren();

    while (child)
    {
        const wxString content = child->GetNodeContent();
        const wxString name = child->GetName();

        if (name == "foundation:Description")
            m_description = content;
        else if (name == "foundation:CreatorName")
            m_creatorName = content;
        else if (name == "foundation:CreatorCompany")
            m_creatorCompany = content;
        else if (name == "foundation:CreatorSystem")
            m_creatorSystem = content;
        else if (name == "foundation:PostProcessor")
            m_postProcessor = content;
        else if (name == "foundation:PostProcessorVersion")
            m_postProcessorVersion = content;

        child = child->GetNext();
    }
}

// -----------------------------------------------------
// EDMD_DATASET_BODY
// -----------------------------------------------------

EDMD_DATASET_BODY::EDMD_DATASET_BODY() : IDX_BASE() {}

// -----------------------------------------------------
// EDMD_DATASET
// -----------------------------------------------------

EDMD_DATASET::EDMD_DATASET(wxXmlNode& aNode)
    : EDMD_OBJECT(aNode), m_header(aNode)
{
}

// -----------------------------------------------------
// EDMD_DATASETBODY
// -----------------------------------------------------

EDMD_DATASETBODY::EDMD_DATASETBODY(wxXmlNode& aNode)
    : EDMD_OBJECT(aNode)
{
}

// -----------------------------------------------------
// EDMD_DESIGNATOR
// -----------------------------------------------------

EDMD_DESIGNATOR::EDMD_DESIGNATOR() : EDMD_EXTENSIBLE_OBJECT() {}

EDMD_DESIGNATOR::EDMD_DESIGNATOR(wxXmlNode& aNode)
    : EDMD_EXTENSIBLE_OBJECT(aNode)
{
    wxXmlNode* child = aNode.GetChildren();

    while (child)
    {
        const wxString content = child->GetNodeContent();
        const wxString name = child->GetName();

        if (name == "foundation:SystemScope")
            m_systemScopeRef = content;

        child = child->GetNext();
    }
}

// -----------------------------------------------------
// EDMD_IDENTIFIER
// -----------------------------------------------------

EDMD_IDENTIFIER::EDMD_IDENTIFIER(wxXmlNode& aNode) : EDMD_DESIGNATOR(aNode) {}

// -----------------------------------------------------
// EDMD_NAME
// -----------------------------------------------------

EDMD_NAME::EDMD_NAME() : EDMD_DESIGNATOR() {}

EDMD_NAME::EDMD_NAME(wxXmlNode& aNode) : EDMD_DESIGNATOR(aNode)
{
    wxXmlNode* child = aNode.GetChildren();

    while (child)
    {
        const wxString content = child->GetNodeContent();
        const wxString name = child->GetName();

        if (name == "foundation:ObjectName")
            m_objectName = content;

        child = child->GetNext();
    }
}

} // namespace idx_foundation
