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
#include <idx/kidx/schema/property.h>
#include <idx/kidx/schema/computational.h>
#include <idx/kidx/schema/geometry.h>
#include <idx/kidx/schema/administration.h>
#include <idx/kidx/schema/annotation.h>
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
        m_name = content;
    else if( IDX_keyword_compare( name, "foundation:Description" ) )
        m_description = content;
    else if( IDX_keyword_compare( name, "foundation:DisplayName" ) )
        m_displayName.emplace_back( std::make_shared<EDMD_NAME>( aNode, true ) );
    else if( IDX_keyword_compare( name, "foundation:DisplayDescription" ) )
        m_displayName.emplace_back( std::make_shared<EDMD_NAME>( aNode, true ) );
    else if( IDX_keyword_compare( name, "foundation:UserProperty" ) )
        m_userProperty.emplace_back( std::make_shared<idx_property::EDMD_USER_PROPERTY>( aNode, true ) );
    else
        return this->EDMD_OBJECT::Parse( aNode );
    return true;
}


std::shared_ptr<idx_property::EDMD_USER_PROPERTY>
EDMD_EXTENSIBLE_OBJECT::GetUserProperty( const std::string& aKey ) const
{
    for( auto prop : m_userProperty )
    {
        for( const auto& key : prop->m_keys )
        {
            if( key->m_objectName == aKey )
                return prop;
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
    else if( IDX_keyword_compare( name, "foundation:CreationDateTime" ) )
        m_creationDateTime = content;
    else if( IDX_keyword_compare( name, "foundation:ModifiedDateTime" ) )
        m_modifiedDateTime = content;
    else if( IDX_keyword_compare( name, "foundation:PostProcessor" ) )
        m_postProcessor = content;
    else if( IDX_keyword_compare( name, "foundation:PostProcessorVersion" ) )
        m_postProcessorVersion = content;
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
    /*
    else if( IDX_keyword_compare( name, "foundation:ExternalSource" ) )
    {
        // TODO
    }
        */
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


bool EDMD_DATASET_BODY::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = true;

    for( auto& curve : m_curves )
        result &= curve->Resolve( aRegistry );

    for( auto& curveset : m_curvesets )
        result &= curveset->Resolve( aRegistry );

    for( auto& shapeE : m_shapeElements )
        result &= shapeE->Resolve( aRegistry );

    for( auto& assembly : m_assemblies )
        result &= assembly->Resolve( aRegistry );

    for( auto& stratum : m_stratums )
        result &= stratum->Resolve( aRegistry );

    for( auto& interstratum : m_interStratums )
        result &= interstratum->Resolve( aRegistry );

    for( auto& keep : m_keeps )
        result &= keep->Resolve( aRegistry );

    for( auto& item : m_items )
        result &= item->Resolve( aRegistry );

    for( auto& fs : m_functionalItemShapes )
        result &= fs->Resolve( aRegistry );

    return result;
}

// -----------------------------------------------------
// EDMD_DATASET
// -----------------------------------------------------

EDMD_DATASET::EDMD_DATASET( IDX_XML_NODE& aNode, bool aHeaderOnly ) :
        EDMD_OBJECT( aNode, false )
{
    m_headerOnly = aHeaderOnly;
    ParseChildren( aNode );
}


bool EDMD_DATASET::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( m_headerOnly && !IDX_keyword_compare( name, "foundation:Header" )
        && !IDX_keyword_compare( name, "foundation:ProcessInstruction" ) )
        return true;


    if( IDX_keyword_compare( name, "foundation:Header" ) )
        m_header = idx_foundation::EDMD_HEADER( aNode, true );
    else if( IDX_keyword_compare( name, "foundation:Body" ) )
        m_body = idx_foundation::EDMD_DATASET_BODY( aNode );
    else if( IDX_keyword_compare( name, "foundation:ProcessInstruction" ) )
    {
        std::string type = aNode.GetAttribute( "xsi:type", "" );

        if( IDX_keyword_compare( type, "computational:EDMDProcessInstructionSendInformation" ) )
        {
            if( !m_headerOnly )
            {
                m_processInstruction.push_back(

                        std::make_shared<idx_computational::EDMD_PROCESS_INSTRUCTION_SEND_INFO>( aNode, true ) );
            }
            m_baseline = true;
        }

        else if( IDX_keyword_compare( type, "computational:EDMDProcessInstructionSendChanges" ) )
        {
            if( !m_headerOnly )
            {
                m_processInstruction.push_back(
                        std::make_shared<idx_computational::EDMD_PROCESS_INSTRUCTION_SEND_CHANGES>( aNode, true ) );
            }
        }
        else
            invalidValueIDX( name, type );
    }
    else
        return this->EDMD_OBJECT::Parse( aNode );
    return true;
}


bool EDMD_DATASET::Resolve()
{
    return m_body.Resolve( m_body.m_registry );
}


void EDMD_DATASET::Prepare()
{
    for( const std::shared_ptr<idx_pdm::EDMD_ITEM>& i : m_body.m_items )
    {
        for( const std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>& inst : i->m_itemInstance )
        {
            if( auto type = inst->GetUserProperty( "TYPE" ) )
            {
                if( type->m_value == "BOARDOUTLINE" )
                {
                    m_body.m_boardOutline.push_back( inst );
                    continue;
                }
            }

            auto interstratum = std::dynamic_pointer_cast<idx_pdm::EDMD_INTERSTRATUM_FEATURE>( inst->m_item->m_shape );
            if( interstratum )
            {
                using FT = idx_pdm::INTERSTRATUM_FEATURE_TYPE;
                switch( interstratum->m_interstratumFeatureType )
                {
                case FT::BOARD_CUTOUT: m_body.m_boardOutline.push_back( inst ); continue;
                case FT::CUTOUT: m_body.m_NPTH.push_back( inst ); continue;
                case FT::PLATED_CUTOUT: m_body.m_PTH.push_back( inst ); continue;
                default: break;
                }
            }

            if( inst->m_item && inst->m_item->m_shape )
            {
                auto keep = std::dynamic_pointer_cast<idx_pdm::EDMD_KEEP_CONSTRAINT>( inst->m_item->m_shape );
                if( keep )
                {
                    m_body.m_keepZones.push_back( inst );
                    continue;
                }
            }

            if( !inst->m_name.empty() )
            {
                m_body.m_components.push_back( inst );
                continue;
            }

            m_body.m_others.push_back( inst );
        }
    }
}


bool EDMD_DATASET::IsBaseline()
{
    return m_baseline;
}

double EDMD_DATASET::GetBoardThickness() const
{
    double result = 0;

    for( auto inst : GetBoardOutlineElements() )
    {
        // Check for board outline
        if( auto prop = inst->GetUserProperty( "THICKNESS" ) )
        {
            try
            {
                double thickness = prop->parseDouble( prop->m_value );

                if( thickness < 0.0 )
                    // Solid Edge uses 0 for incremental changes when thickness does not change
                    Report( "Invalid board thickness - negative", IDX_REPORT_LEVEL::IDX_ERROR );
                else
                    result += thickness;
            }
            catch( const std::invalid_argument& )
            {
                Report( "Invalid board thickness - nan", IDX_REPORT_LEVEL::IDX_ERROR );
            }
            catch( const std::out_of_range& )
            {
                Report( "Invalid board thickness - out of range", IDX_REPORT_LEVEL::IDX_ERROR );
            }
        }
    }

    return result;
}


std::shared_ptr<idx_pdm::EDMD_ITEM> EDMD_DATASET::GetItem( const idx_foundation::EDMD_IDENTIFIER& aId ) const
{
    for( const auto& item : m_body.m_items )
    {
        for( const auto& identifier : item->m_identifier )
        {
            if( identifier == aId )
            {
                return item;
            }
        }
    }
    return nullptr;
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
        m_systemScopeRef = content;
    else
        return this->EDMD_EXTENSIBLE_OBJECT::Parse( aNode );

    return true;
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
