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

#include <idx/kidx/schema/idx_base.h>
#include <wx/xml/xml.h>
#include <memory>
#include <map>


namespace idx_foundation
{

/**
 * @brief Base class for EDMD objects.
 */
class EDMD_OBJECT : public IDX_BASE
{
public:
    EDMD_OBJECT();
    EDMD_OBJECT( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;

    bool operator==( const EDMD_OBJECT& rhs );

protected:
    virtual bool Parse( IDX_XML_NODE& aNode ) override;

public:
    std::string         m_id;
    enum OPTIONAL_LOGIC m_isAttributeChanged = UNSPECIFIED;
};


class EDMD_EXTENSIBLE_OBJECT : public EDMD_OBJECT
{
public:
    EDMD_EXTENSIBLE_OBJECT();
    EDMD_EXTENSIBLE_OBJECT( IDX_XML_NODE& aNode, bool aTopLevel );

    std::shared_ptr<idx_property::EDMD_USER_PROPERTY> GetUserProperty( const std::string& aKey );
    virtual bool                                      Resolve( IDX_REGISTRY& aRegistry ) override;


    std::string                                                    m_name;
    std::string                                                    m_description;
    std::vector<std::shared_ptr<EDMD_NAME>>                        m_displayDescription;
    std::vector<std::shared_ptr<EDMD_NAME>>                        m_displayName;
    std::vector<std::shared_ptr<idx_property::EDMD_USER_PROPERTY>> m_userProperty;

protected:
    virtual bool Parse( IDX_XML_NODE& aNode ) override;
};


class EDMD_COLLABORATION_OBJECT : public EDMD_EXTENSIBLE_OBJECT
{
public:
    EDMD_COLLABORATION_OBJECT();
    EDMD_COLLABORATION_OBJECT( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    virtual bool Parse( IDX_XML_NODE& aNode ) override;
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

    EDMD_SYSTEM( IDX_XML_NODE& aNode );
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;

    SYSTEM_TYPE m_systemType = SYSTEM_TYPE::UNDEFINED;
    std::string m_globalName;
    std::string m_webServiceURI;
    std::string m_revision;
    std::string m_systemName;
    std::string m_instanceName;
    std::string m_systemOwner;
    bool        m_needsSelfContainedMessages = false;
};


class EDMD_HEADER : public EDMD_EXTENSIBLE_OBJECT
{
public:
    EDMD_HEADER( IDX_XML_NODE& aNode, bool aTopLevel );
    EDMD_HEADER() {};


    virtual bool Parse( IDX_XML_NODE& aNode ) override;

    std::string m_creatorName;
    std::string m_creatorCompany;
    std::string m_creatorSystem;
    std::string m_postProcessor;
    std::string m_postProcessorVersion;

    std::string m_systemRef;
    std::string m_creatorRef;
    std::string m_globalUnitLengthRef;
    std::string m_globalUnitAmperageRef;
    std::string m_globalUnitAngleRef;
    std::string m_globalUnitCapacitanceRef;
    std::string m_globalUnitPowerRef;
    std::string m_globalUnitResistanceRef;
    std::string m_globalUnitVoltageRef;
    std::string m_globalUnitWeightRef;
    std::string m_globalUnitWorkRef;

    std::shared_ptr<idx_administration::EDMD_PERSON> m_creator;
    std::string                                      m_creationDateTime;
    std::string                                      m_modifiedDateTime;
    bool                                             isSelfContained = true;
};


class EDMD_DATASET_BODY : public IDX_BASE
{
public:
    EDMD_DATASET_BODY( IDX_XML_NODE& aNode );
    EDMD_DATASET_BODY() {};

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    virtual bool Parse( IDX_XML_NODE& aNode ) override;


    IDX_REGISTRY m_registry;

    std::vector<std::shared_ptr<idx_administration::EDMD_ACTOR>>      m_actors;
    std::vector<std::shared_ptr<idx_foundation::EDMD_SYSTEM>>         m_systems;
    std::vector<std::shared_ptr<idx_property::EDMD_UNIT>>             m_units;
    std::vector<std::shared_ptr<idx_geometry::EDMD_POINT>>            m_points;
    std::vector<std::shared_ptr<idx_geometry::EDMD_CURVE>>            m_curves;
    std::vector<std::shared_ptr<idx_geometry::EDMD_CURVESET2D>>       m_curvesets;
    std::vector<std::shared_ptr<idx_pdm::EDMD_SHAPE_ELEMENT>>         m_shapeElements;
    std::vector<std::shared_ptr<idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE>> m_functionalItemShapes;
    std::vector<std::shared_ptr<idx_pdm::EDMD_STRATUM>>               m_stratums;
    std::vector<std::shared_ptr<idx_pdm::EDMD_INTERSTRATUM_FEATURE>>  m_interStratums;
    std::vector<std::shared_ptr<idx_pdm::EDMD_KEEP_CONSTRAINT>>       m_keeps;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM>>                  m_items;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ASSEMBLY_COMPONENT>>    m_assemblies;


    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> m_boardOutline;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> m_components;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> m_NPTH;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> m_PTH;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> m_others;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> m_keepZones;
};


class EDMD_PROCESS_INSTRUCTION : public EDMD_EXTENSIBLE_OBJECT
{
public:
    enum class PROCESS_TYPE
    {
        BASELINE = 0,
        SEND_CHANGES
    };
    EDMD_PROCESS_INSTRUCTION() :
            EDMD_EXTENSIBLE_OBJECT() {};
    EDMD_PROCESS_INSTRUCTION( IDX_XML_NODE& aNode, bool aTopLevel );
    std::shared_ptr<EDMD_OBJECT> m_subject;
    std::shared_ptr<EDMD_OBJECT> m_object;
    std::shared_ptr<EDMD_SYSTEM> m_targetSystem;
    PROCESS_TYPE                 m_processType;
};


class EDMD_DATASET : public EDMD_OBJECT
{
public:
    EDMD_DATASET();
    EDMD_DATASET( IDX_XML_NODE& aNode, bool aHeaderOnly = false );

    EDMD_HEADER                                            m_header;
    EDMD_DATASET_BODY                                      m_body;
    std::vector<std::shared_ptr<EDMD_PROCESS_INSTRUCTION>> m_processInstruction;

    bool Parse( IDX_XML_NODE& aNode );

    bool Resolve();
    void Prepare();

    bool IsBaseline();


    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> GetBoardOutlineElements() const
    {
        return m_body.m_boardOutline;
    }

    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> GetNPTH() const { return m_body.m_NPTH; }

    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> GetComponents() const { return m_body.m_components; }

    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> GetPTH() const { return m_body.m_PTH; }

    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> GetOthers() const { return m_body.m_others; }

    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> GetKeepZones() const { return m_body.m_keepZones; }


private:
    bool m_baseline = false;
    bool m_headerOnly;
};


class EDMD_DESIGNATOR : public EDMD_EXTENSIBLE_OBJECT
{
public:
    EDMD_DESIGNATOR();
    EDMD_DESIGNATOR( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Parse( IDX_XML_NODE& aNode ) override;

    std::string m_systemScopeRef;
    bool        m_persistant = false;
    std::string m_originatorId;
    bool        m_isOriginator = false;
};


class EDMD_IDENTIFIER : public EDMD_DESIGNATOR
{
public:
    EDMD_IDENTIFIER( IDX_XML_NODE& aNode, bool aTopLevel );


    virtual bool Parse( IDX_XML_NODE& aNode ) override;

    std::string                             m_number;
    std::string                             m_version;
    std::string                             m_revision;
    std::string                             m_sequence;
    std::vector<std::shared_ptr<EDMD_NAME>> m_name;
    std::vector<std::shared_ptr<EDMD_NAME>> m_displayName;
};


class EDMD_NAME : public EDMD_DESIGNATOR
{
public:
    EDMD_NAME();
    EDMD_NAME( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Parse( IDX_XML_NODE& aNode ) override;


    std::string m_objectName;
};

} // namespace idx_foundation

#endif // IDX_FOUNDATION_HEADER_H
