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
#include <idx/computational.h>
#include <idx/geometry.h>
#include <idx/administration.h>
#include <idx/annotation.h>
#include <iostream>


namespace idx_foundation
{

// -----------------------------------------------------
// EDMD_OBJECT
// -----------------------------------------------------

EDMD_OBJECT::EDMD_OBJECT() :
        IDX_BASE()
{
}

EDMD_OBJECT::EDMD_OBJECT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        IDX_BASE()
{
    std::string isAttributeChanged;
    m_id = aNode.GetAttribute( "id", "" );

    isAttributeChanged = aNode.GetAttribute( "isAttributeChanged", "" );

    if( isAttributeChanged == "true" )
        m_isAttributeChanged = LOGIC_TRUE;
    else if( isAttributeChanged == "false" )
        m_isAttributeChanged = LOGIC_FALSE;
    else
        m_isAttributeChanged = UNSPECIFIED;
}

bool EDMD_OBJECT::Parse( IDX_XML_NODE& aNode )
{
    return false;
}

bool EDMD_OBJECT::Resolve( IDX_REGISTRY& aRegistry )
{
    return IDX_BASE::Resolve( aRegistry );
}

bool EDMD_OBJECT::operator==( const EDMD_OBJECT& rhs )
{
    return m_id == rhs.m_id;
}

// -----------------------------------------------------
// EDMD_EXTENSIBLE_OBJECT
// -----------------------------------------------------

EDMD_EXTENSIBLE_OBJECT::EDMD_EXTENSIBLE_OBJECT() :
        EDMD_OBJECT()
{
}

EDMD_EXTENSIBLE_OBJECT::EDMD_EXTENSIBLE_OBJECT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_EXTENSIBLE_OBJECT::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "foundation:Name" ) )
    {
        m_name = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "foundation:Description" ) )
    {
        m_description = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "foundation:DisplayName" ) )
    {
        m_displayName.emplace_back( aNode, true );
        return true;
    }
    else if( IDX_keyword_compare( name, "foundation:DisplayDescription" ) )
    {
        m_displayDescription.emplace_back( aNode, true );
        return true;
    }
    else if( IDX_keyword_compare( name, "foundation:UserProperty" ) )
    {
        m_userProperty.emplace_back( aNode, true );
        return true;
    }

    return this->EDMD_OBJECT::Parse( aNode );
}


idx_property::EDMD_USER_PROPERTY* EDMD_EXTENSIBLE_OBJECT::GetUserProperty( const std::string& aKey )
{
    for( auto& prop : m_userProperty )
    {
        for( const auto& key : prop.m_keys )
        {
            if( key.m_objectName == aKey )
                return &prop;
        }
    }
    return nullptr;
}

bool EDMD_EXTENSIBLE_OBJECT::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_OBJECT::Resolve( aRegistry );
}

// -----------------------------------------------------
// EDMD_COLLABORATION_OBJECT
// -----------------------------------------------------

EDMD_COLLABORATION_OBJECT::EDMD_COLLABORATION_OBJECT() :
        EDMD_EXTENSIBLE_OBJECT()
{
}

EDMD_COLLABORATION_OBJECT::EDMD_COLLABORATION_OBJECT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_EXTENSIBLE_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_COLLABORATION_OBJECT::Parse( IDX_XML_NODE& aNode )
{
    return this->EDMD_EXTENSIBLE_OBJECT::Parse( aNode );
}

bool EDMD_COLLABORATION_OBJECT::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_EXTENSIBLE_OBJECT::Resolve( aRegistry );
}

// -----------------------------------------------------
// EDMD_SYSTEM
// -----------------------------------------------------

EDMD_SYSTEM::EDMD_SYSTEM( IDX_XML_NODE& aNode ) :
        EDMD_COLLABORATION_OBJECT( aNode, false )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "foundation:GlobalName" ) )
            m_globalName = content;
        else if( IDX_keyword_compare( name, "foundation:WebServiceURI" ) )
            m_webServiceURI = content;
        else if( IDX_keyword_compare( name, "foundation:Revision" ) )
            m_revision = content;
        else if( IDX_keyword_compare( name, "foundation:SystemName" ) )
            m_systemName = content;
        else if( IDX_keyword_compare( name, "foundation:InstanceName" ) )
            m_instanceName = content;
        else if( IDX_keyword_compare( name, "foundation:systemOwner" ) )
            m_systemOwner = content;
        else if( IDX_keyword_compare( name, "foundation:NeedsSelfContainedMessages" ) )
            m_needsSelfContainedMessages = ( IDX_keyword_compare( content, "true" ) );

        child = child->GetNext();
    }
}

bool EDMD_SYSTEM::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );
}

// -----------------------------------------------------
// EDMD_HEADER
// -----------------------------------------------------

EDMD_HEADER::EDMD_HEADER( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_EXTENSIBLE_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_HEADER::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();


    if( IDX_keyword_compare( name, "foundation:Description" ) )
        m_description = content;
    else if( IDX_keyword_compare( name, "foundation:CreatorName" ) )
        m_creatorName = content;
    else if( IDX_keyword_compare( name, "foundation:CreatorCompany" ) )
        m_creatorCompany = content;
    else if( IDX_keyword_compare( name, "foundation:CreatorSystem" ) )
        m_creatorSystem = content;
    else if( IDX_keyword_compare( name, "foundation:PostProcessor" ) )
        m_postProcessor = content;
    else if( IDX_keyword_compare( name, "foundation:PostProcessorVersion" ) )
        m_postProcessorVersion = content;
    else if( IDX_keyword_compare( name, "foundation:UserProperty" ) )
        m_userProperty.emplace_back( aNode, true );
    else if( IDX_keyword_compare( name, "foundation:Creator" ) )
        m_creatorRef = content;
    else if( IDX_keyword_compare( name, "foundation:System" ) )
        m_systemRef = content;
    else if( IDX_keyword_compare( name, "foundation:GlobalUnitLength" ) )
        m_globalUnitLengthRef = content;
    else if( IDX_keyword_compare( name, "foundation:GlobalUnitAmperage" ) )
        m_globalUnitAmperageRef = content;
    else if( IDX_keyword_compare( name, "foundation:GlobalUnitAngle" ) )
        m_globalUnitAngleRef = content;
    else if( IDX_keyword_compare( name, "foundation:GlobalUnitCapacitance" ) )
        m_globalUnitCapacitanceRef = content;
    else if( IDX_keyword_compare( name, "foundation:GlobalUnitPower" ) )
        m_globalUnitPowerRef = content;
    else if( IDX_keyword_compare( name, "foundation:GlobalUnitResistance" ) )
        m_globalUnitResistanceRef = content;
    else if( IDX_keyword_compare( name, "foundation:GlobalUnitVoltage" ) )
        m_globalUnitVoltageRef = content;
    else if( IDX_keyword_compare( name, "foundation:GlobalUnitWeight" ) )
        m_globalUnitWeightRef = content;
    else if( IDX_keyword_compare( name, "foundation:GlobalUnitWork" ) )
        m_globalUnitWorkRef = content;
    else
        return this->EDMD_EXTENSIBLE_OBJECT::Parse( aNode );

    return true;
}

// -----------------------------------------------------
// EDMD_DATASET_BODY
// -----------------------------------------------------

EDMD_DATASET_BODY::EDMD_DATASET_BODY( IDX_XML_NODE& aNode ) :
        IDX_BASE()
{
    ParseChildren( aNode );
}

bool EDMD_DATASET_BODY::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "foundation:System" ) )
    {
        std::shared_ptr<idx_foundation::EDMD_SYSTEM> s = std::make_shared<idx_foundation::EDMD_SYSTEM>( aNode );
        m_systems.push_back( s );
    }
    else if( IDX_keyword_compare( name, "foundation:History" ) )
    {
        std::shared_ptr<idx_annotation::EDMD_HISTORY> s = std::make_shared<idx_annotation::EDMD_HISTORY>( aNode );
    }
    else if( IDX_keyword_compare( name, "foundation:Person" ) )
    {
        std::shared_ptr<idx_administration::EDMD_PERSON> s = std::make_shared<idx_administration::EDMD_PERSON>( aNode );
        m_actors.push_back( s );
    }
    else if( IDX_keyword_compare( name, "foundation:UnitLength" ) )
    {
        std::shared_ptr<idx_property::EDMD_UNIT> s = std::make_shared<idx_property::EDMD_UNIT>( aNode, true );
        m_units.push_back( s );
    }
    else if( IDX_keyword_compare( name, "foundation:CartesianPoint" ) )
    {
        std::shared_ptr<idx_geometry::EDMD_CARTESIAN_POINT> s =
                std::make_shared<idx_geometry::EDMD_CARTESIAN_POINT>( aNode, true );
        m_registry.registerObject<idx_geometry::EDMD_POINT>( s->m_id, s );
    }
    else if( IDX_keyword_compare( name, "foundation:PolyLine" ) )
    {
        std::shared_ptr<idx_geometry::EDMD_POLYLINE> s = std::make_shared<idx_geometry::EDMD_POLYLINE>( aNode, true );
        m_registry.registerObject<idx_geometry::EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT>( s->m_id, s );
        m_curves.push_back( s );
    }
    else if( IDX_keyword_compare( name, "foundation:CircleCenter" ) )
    {
        std::shared_ptr<idx_geometry::EDMD_CIRCLE_CENTER> s =
                std::make_shared<idx_geometry::EDMD_CIRCLE_CENTER>( aNode );
        m_registry.registerObject<idx_geometry::EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT>( s->m_id, s );
        m_curves.push_back( s );
    }
    else if( IDX_keyword_compare( name, "foundation:Arc" ) )
    {
        std::shared_ptr<idx_geometry::EDMD_ARC> s = std::make_shared<idx_geometry::EDMD_ARC>( aNode );
        m_registry.registerObject<idx_geometry::EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT>( s->m_id, s );
        m_curves.push_back( s );
    }
    else if( IDX_keyword_compare( name, "foundation:CurveSet2d" ) )
    {
        std::shared_ptr<idx_geometry::EDMD_CURVESET2D> s = std::make_shared<idx_geometry::EDMD_CURVESET2D>( aNode );
        m_registry.registerObject<idx_pdm::EDMD_SHAPE_DESCRIPTION>( s->m_id, s );
        m_curvesets.push_back( s );
    }
    else if( IDX_keyword_compare( name, "foundation:ShapeElement" ) )
    {
        std::shared_ptr<idx_pdm::EDMD_SHAPE_ELEMENT> s = std::make_shared<idx_pdm::EDMD_SHAPE_ELEMENT>( aNode, true );
        m_registry.registerObject<idx_pdm::EDMD_SHAPE_ELEMENT>( s->m_id, s );
        m_shapeElements.push_back( s );
    }
    else if( IDX_keyword_compare( name, "foundation:Stratum" ) )
    {
        std::shared_ptr<idx_pdm::EDMD_STRATUM> s = std::make_shared<idx_pdm::EDMD_STRATUM>( aNode, true );
        m_registry.registerObject<idx_pdm::EDMD_ITEM_SHAPE>( s->m_id, s );
        m_stratums.push_back( s );
    }
    else if( IDX_keyword_compare( name, "foundation:KeepOut" ) )
    {
        std::shared_ptr<idx_pdm::EDMD_KEEPOUT> s = std::make_shared<idx_pdm::EDMD_KEEPOUT>( aNode, true );
        m_registry.registerObject<idx_pdm::EDMD_ITEM_SHAPE>( s->m_id, s );
        m_keeps.push_back( s );
    }
    else if( IDX_keyword_compare( name, "foundation:KeepIn" ) )
    {
        std::shared_ptr<idx_pdm::EDMD_KEEPIN> s = std::make_shared<idx_pdm::EDMD_KEEPIN>( aNode, true );
        m_registry.registerObject<idx_pdm::EDMD_ITEM_SHAPE>( s->m_id, s );
        m_keeps.push_back( s );
    }
    else if( IDX_keyword_compare( name, "foundation:Item" ) )
    {
        std::shared_ptr<idx_pdm::EDMD_ITEM> s = std::make_shared<idx_pdm::EDMD_ITEM>( aNode, true );
        m_registry.registerObject<idx_pdm::EDMD_ITEM>( s->m_id, s );
        m_items.push_back( s );
    }
    else if( IDX_keyword_compare( name, "foundation:AssemblyComponent" ) )
    {
        std::shared_ptr<idx_pdm::EDMD_ASSEMBLY_COMPONENT> s =
                std::make_shared<idx_pdm::EDMD_ASSEMBLY_COMPONENT>( aNode, true );
        m_registry.registerObject<idx_pdm::EDMD_ITEM_SHAPE>( s->m_id, s );
        m_assemblies.push_back( s );
    }
    else if( IDX_keyword_compare( name, "foundation:InterStratumFeature" ) )
    {
        std::shared_ptr<idx_pdm::EDMD_INTERSTRATUM_FEATURE> s =
                std::make_shared<idx_pdm::EDMD_INTERSTRATUM_FEATURE>( aNode, true );
        m_registry.registerObject<idx_pdm::EDMD_ITEM_SHAPE>( s->m_id, s );
        m_interStratums.push_back( s );
    }

    else if( IDX_keyword_compare( name, "foundation:FunctionalItemShape" ) )
    {
        std::shared_ptr<idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE> s =
                std::make_shared<idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE>( aNode );
        m_registry.registerObject<idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE>( s->m_id, s );
        m_functionalItemShapes.push_back( s );
    }
    else
    {
        return false;
    }

    return true;
}


// -----------------------------------------------------
// EDMD_DATASET
// -----------------------------------------------------

EDMD_DATASET::EDMD_DATASET( IDX_XML_NODE& aNode ) :
        EDMD_OBJECT( aNode, false )
{
    ParseChildren( aNode );
}


bool EDMD_DATASET::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();
    std::cout << "Parsing " << name << std::endl;

    if( IDX_keyword_compare( name, "foundation:Header" ) )
        m_header = idx_foundation::EDMD_HEADER( aNode, true );
    else if( IDX_keyword_compare( name, "foundation:Body" ) )
        m_body = idx_foundation::EDMD_DATASET_BODY( aNode );
    else if( IDX_keyword_compare( name, "foundation:ProcessInstruction" ) )
    {
        std::string type = aNode.GetAttribute( "xsi:type", "" );

        if( IDX_keyword_compare( type, "computational:EDMDProcessInstructionSendInformation" ) )
            m_processInstruction.push_back(
                    std::make_shared<idx_computational::EDMD_PROCESS_INSTRUCTION_SEND_INFO>( aNode, true ) );
        else if( IDX_keyword_compare( type, "computational:EDMDProcessInstructionSendChanges" ) )
            m_processInstruction.push_back(
                    std::make_shared<idx_computational::EDMD_PROCESS_INSTRUCTION_SEND_CHANGES>( aNode, true ) );
        else
            invalidValueIDX( name, type );
    }
    else
        return this->EDMD_OBJECT::Parse( aNode );
    return true;
}


bool EDMD_DATASET::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = true;

    for( auto& curve : m_body.m_curves )
        result &= curve->Resolve( m_body.m_registry );

    for( auto& curveset : m_body.m_curvesets )
        result &= curveset->Resolve( m_body.m_registry );

    for( auto& shapeE : m_body.m_shapeElements )
        result &= shapeE->Resolve( m_body.m_registry );

    for( auto& stratum : m_body.m_stratums )
        result &= stratum->Resolve( m_body.m_registry );

    for( auto& interstratum : m_body.m_interStratums )
        result &= interstratum->Resolve( m_body.m_registry );

    for( auto& keep : m_body.m_keeps )
        result &= keep->Resolve( m_body.m_registry );

    for( auto& item : m_body.m_items )
        result &= item->Resolve( m_body.m_registry );

    for( auto& fs : m_body.m_functionalItemShapes )
        result &= fs->Resolve( m_body.m_registry );

    return result;
}


// -----------------------------------------------------
// EDMD_DESIGNATOR
// -----------------------------------------------------

EDMD_DESIGNATOR::EDMD_DESIGNATOR() :
        EDMD_EXTENSIBLE_OBJECT()
{
}

EDMD_DESIGNATOR::EDMD_DESIGNATOR( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_EXTENSIBLE_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_DESIGNATOR::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();


    if( IDX_keyword_compare( name, "foundation:SystemScope" ) )
    {
        m_systemScopeRef = content;
        return true;
    }

    return this->EDMD_EXTENSIBLE_OBJECT::Parse( aNode );
}

// -----------------------------------------------------
// EDMD_IDENTIFIER
// -----------------------------------------------------

EDMD_IDENTIFIER::EDMD_IDENTIFIER( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_DESIGNATOR( aNode, false )
{
    if( aNode.GetAttribute( "Persistant", "false" ) == "true" )
        m_persistant = true;
    else
        m_persistant = false;

    if( aNode.GetAttribute( "IsOriginator", "false" ) == "true" )
        m_isOriginator = true;
    else
        m_isOriginator = false;

    m_originatorId = aNode.GetAttribute( "OriginatorId", "" );

    if( aTopLevel )
        ParseChildren( aNode );
}


bool EDMD_IDENTIFIER::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();


    if( IDX_keyword_compare( name, "foundation:Number" ) )
        m_number = content;
    else if( IDX_keyword_compare( name, "foundation:Version" ) )
        m_version = content;
    else if( IDX_keyword_compare( name, "foundation:Revision" ) )
        m_revision = content;
    else if( IDX_keyword_compare( name, "foundation:Sequence" ) )
        m_sequence = content;
    else
        return this->EDMD_DESIGNATOR::Parse( aNode );
    return true;
}
// -----------------------------------------------------
// EDMD_NAME
// -----------------------------------------------------

EDMD_NAME::EDMD_NAME() :
        EDMD_DESIGNATOR()
{
}

EDMD_NAME::EDMD_NAME( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_DESIGNATOR( aNode, false )
{
    if( aNode.GetAttribute( "Persistant", "false" ) == "true" )
        m_persistant = true;
    else
        m_persistant = false;

    if( aNode.GetAttribute( "IsOriginator", "false" ) == "true" )
        m_isOriginator = true;
    else
        m_isOriginator = false;

    m_originatorId = aNode.GetAttribute( "OriginatorId", "" );

    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_NAME::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();


    if( IDX_keyword_compare( name, "foundation:ObjectName" ) )
    {
        m_objectName = content;
        return true;
    }
    return this->EDMD_DESIGNATOR::Parse( aNode );
}


// -----------------------------------------------------
// EDMD_DESIGNATOR
// -----------------------------------------------------


EDMD_PROCESS_INSTRUCTION::EDMD_PROCESS_INSTRUCTION( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_EXTENSIBLE_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};

} // namespace idx_foundation
