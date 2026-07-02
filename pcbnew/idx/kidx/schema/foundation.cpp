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

    isAttributeChanged = aNode.GetAttribute( "IsAttributeChanged", "" );

    if( !isAttributeChanged.empty() )
        m_isAttributeChanged = parseBool( isAttributeChanged );
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


void EDMD_OBJECT::Write( IDX_XML_NODE& aNode ) const
{
    if( m_isAttributeChanged.has_value() )
        aNode.SetAttribute( "IsAttributeChanged", m_isAttributeChanged.value() );
}

void EDMD_OBJECT::WriteValueChild( IDX_XML_NODE& aNode, const std::string aChildName, const std::string aValue ) const
{
    if( !aValue.empty() )
        aNode.AppendChild( aChildName )->SetText( aValue );
}

void EDMD_OBJECT::WriteValueChild( IDX_XML_NODE& aNode, const std::string aChildName,
                                   const std::optional<std::string> aValue ) const
{
    if( aValue.has_value() )
        WriteValueChild( aNode, aChildName, aValue.value() );
}


void EDMD_OBJECT::WriteChild( IDX_XML_NODE& aNode, const std::string aChildName, const EDMD_OBJECT& aChild ) const
{
    aChild.Write( *( aNode.AppendChild( aChildName ) ) );
}

void EDMD_OBJECT::WriteChild( IDX_XML_NODE& aNode, const std::string aChildName,
                              const std::optional<EDMD_OBJECT>& aChild ) const
{
    if( aChild.has_value() )
        aChild.value().Write( *( aNode.AppendChild( aChildName ) ) );
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

void EDMD_EXTENSIBLE_OBJECT::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_OBJECT::Write( aNode );

    WriteValueChild( aNode, "foundation:Name", m_name );
    WriteValueChild( aNode, "foundation:Description", m_description );

    for( auto name : m_displayName )
    {
        if( name )
            WriteChild( aNode, "foundation:DisplayName", *name );
    }
    for( auto name : m_displayDescription )
    {
        if( name )
            WriteChild( aNode, "foundation:DisplayDescription", *name );
    }
    for( auto name : m_userProperty )
    {
        if( name )
            WriteChild( aNode, "foundation:UserProperty", *name );
    }
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


void EDMD_COLLABORATION_OBJECT::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_EXTENSIBLE_OBJECT::Write( aNode );
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


void EDMD_HEADER::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_EXTENSIBLE_OBJECT::Write( aNode );

    WriteValueChild( aNode, "foundation:CreatorName", m_creatorName );
    WriteValueChild( aNode, "foundation:CreatorCompany", m_creatorCompany );
    WriteValueChild( aNode, "foundation:CreatorSystem", m_creatorSystem );
    WriteValueChild( aNode, "foundation:PostProcessor", m_postProcessor );
    WriteValueChild( aNode, "foundation:PostProcessorVersion", m_postProcessorVersion );
    WriteValueChild( aNode, "foundation:CreationDateTime", m_creationDateTime );
    WriteValueChild( aNode, "foundation:ModifiedDateTime", m_modifiedDateTime );
    WriteValueChild( aNode, "foundation:Creator", m_creatorRef );
    WriteValueChild( aNode, "foundation:System", m_systemRef );
    WriteValueChild( aNode, "foundation:GlobalUnitLength", m_globalUnitLengthRef );
    WriteValueChild( aNode, "foundation:GlobalUnitAmperage", m_globalUnitAmperageRef );
    WriteValueChild( aNode, "foundation:GlobalUnitAngle", m_globalUnitAngleRef );
    WriteValueChild( aNode, "foundation:GlobalUnitCapacitance", m_globalUnitCapacitanceRef );
    WriteValueChild( aNode, "foundation:GlobalUnitPower", m_globalUnitPowerRef );
    WriteValueChild( aNode, "foundation:GlobalUnitResistance", m_globalUnitResistanceRef );
    WriteValueChild( aNode, "foundation:GlobalUnitVoltage", m_globalUnitVoltageRef );
    WriteValueChild( aNode, "foundation:GlobalUnitWeight", m_globalUnitWeightRef );
    WriteValueChild( aNode, "foundation:GlobalUnitWork", m_globalUnitWorkRef );
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
        m_registry.registerObject<idx_foundation::EDMD_SYSTEM>( s->m_id, s );
        m_systems.push_back( s );
    }
    else if( IDX_keyword_compare( name, "foundation:History" ) )
    {
        std::shared_ptr<idx_annotation::EDMD_HISTORY> history = std::make_shared<idx_annotation::EDMD_HISTORY>( aNode );
        m_history.push_back( history );
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
        std::shared_ptr<idx_geometry::EDMD_CURVE_SET_2D> s = std::make_shared<idx_geometry::EDMD_CURVE_SET_2D>( aNode );
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
        std::shared_ptr<idx_pdm::EDMD_KEEP_OUT> s = std::make_shared<idx_pdm::EDMD_KEEP_OUT>( aNode, true );
        m_registry.registerObject<idx_pdm::EDMD_ITEM_SHAPE>( s->m_id, s );
        m_keeps.push_back( s );
    }
    else if( IDX_keyword_compare( name, "foundation:KeepIn" ) )
    {
        std::shared_ptr<idx_pdm::EDMD_KEEP_IN> s = std::make_shared<idx_pdm::EDMD_KEEP_IN>( aNode, true );
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
        std::shared_ptr<idx_pdm::EDMD_INTER_STRATUM_FEATURE> s =
                std::make_shared<idx_pdm::EDMD_INTER_STRATUM_FEATURE>( aNode, true );
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
    else if( IDX_keyword_compare( name, "pdm:PackagePin" ) )
    {
        std::shared_ptr<idx_pdm::EDMD_PACKAGE_PIN> s = std::make_shared<idx_pdm::EDMD_PACKAGE_PIN>( aNode, true );
        m_registry.registerObject<idx_pdm::EDMD_PACKAGE_PIN>( s->m_id, s );
        m_packagePins.push_back( s );
    }
    else if( IDX_keyword_compare( name, "pdm:3DModel" ) )
    {
        std::shared_ptr<idx_pdm::EDMD_3D_MODEL> s = std::make_shared<idx_pdm::EDMD_3D_MODEL>( aNode, true );
        m_registry.registerObject<idx_pdm::EDMD_3D_MODEL>( s->m_id, s );
        m_3dModels.push_back( s );
    }
    else if( IDX_keyword_compare( name, "pdm:Bend" ) )
    {
        std::shared_ptr<idx_pdm::EDMD_BEND> s = std::make_shared<idx_pdm::EDMD_BEND>( aNode, true );
        m_registry.registerObject<idx_pdm::EDMD_ITEM_SHAPE>( s->m_id, s );
        m_bends.push_back( s );
    }
    else if( IDX_keyword_compare( name, "pdm:BendType" ) )
    {
        std::shared_ptr<idx_pdm::EDMD_BEND_TYPE> s = std::make_shared<idx_pdm::EDMD_BEND_TYPE>( aNode, true );
        m_registry.registerObject<idx_pdm::EDMD_BEND_TYPE>( s->m_id, s );
        m_bendTypes.push_back( s );
    }
    else if( IDX_keyword_compare( name, "pdm:CircularBendType" ) )
    {
        std::shared_ptr<idx_pdm::EDMD_CIRCULAR_BEND_TYPE> s =
                std::make_shared<idx_pdm::EDMD_CIRCULAR_BEND_TYPE>( aNode );
        m_registry.registerObject<idx_pdm::EDMD_CIRCULAR_BEND_TYPE>( s->m_id, s );
        m_circularBendTypes.push_back( s );
    }
    else
    {
        return false;
    }

    return true;
}

void EDMD_DATASET_BODY::BuildXML( std::shared_ptr<IDX_XML_NODE> aNode )
{
    if( !aNode )
        return;

    aNode->SetName( "foundation:Body" );
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

    for( auto& packagePin : m_packagePins )
        result &= packagePin->Resolve( aRegistry );

    for( auto& model3d : m_3dModels )
        result &= model3d->Resolve( aRegistry );

    for( auto& bend : m_bends )
        result &= bend->Resolve( aRegistry );

    for( auto& bendType : m_bendTypes )
        result &= bendType->Resolve( aRegistry );

    for( auto& circularBendType : m_circularBendTypes )
        result &= circularBendType->Resolve( aRegistry );

    return result;
}

// -----------------------------------------------------
// EDMD_DATASET
// -----------------------------------------------------

EDMD_DATASET::EDMD_DATASET() :
        EDMD_OBJECT()
{
    // Default constructor - initialize members
    m_baseline = false;
    m_headerOnly = false;
}


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

        // Check for full namespace URI or prefix-based type
        bool isSendInfo = ( type.find( "SendInformation" ) != std::string::npos )
                          || IDX_keyword_compare( type, "computational:EDMDProcessInstructionSendInformation" );
        bool isSendChanges = ( type.find( "SendChanges" ) != std::string::npos )
                             || IDX_keyword_compare( type, "computational:EDMDProcessInstructionSendChanges" );
        bool isRequestInfo =
                ( type.find( "RequestForInformation" ) != std::string::npos )
                || IDX_keyword_compare( type, "computational:EDMDProcessInstructionRequestForInformation" );

        if( isSendInfo )
        {
            if( !m_headerOnly )
            {
                m_processInstruction.push_back(
                        std::make_shared<idx_computational::EDMD_PROCESS_INSTRUCTION_SEND_INFO>( aNode, true ) );
            }
            m_baseline = true;
        }
        else if( isSendChanges )
        {
            if( !m_headerOnly )
            {
                m_processInstruction.push_back(
                        std::make_shared<idx_computational::EDMD_PROCESS_INSTRUCTION_SEND_CHANGES>( aNode, true ) );
            }
        }
        else if( isRequestInfo )
        {
            if( !m_headerOnly )
            {
                m_processInstruction.push_back(
                        std::make_shared<idx_computational::EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION>( aNode,
                                                                                                               true ) );
            }
        }
        else if( !type.empty() )
            invalidValueIDX( name, type );
    }
    else
        return this->EDMD_OBJECT::Parse( aNode );
    return true;
}


void EDMD_DATASET::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_OBJECT::Write( aNode );

    // Set XML namespaces and schema location
    aNode.SetAttribute( "xmlns", "http://www.prostep.org/IDX/4.5" );
    aNode.SetAttribute( "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance" );
    aNode.SetAttribute( "xsi:schemaLocation", "http://www.prostep.org/IDX/4.5 IDX_4-5.xsd" );

    // Write header
    if( true ) // Always write header for now
    {
        std::shared_ptr<IDX_XML_NODE> headerNode = aNode.AppendChild( "foundation:Header" );
        m_header.Write( *headerNode );
    }

    // Write process instructions
    for( const auto& pi : m_processInstruction )
    {
        if( pi )
        {
            // Determine the correct node name based on process type
            std::string nodeName;
            switch( pi->m_processType )
            {
            case EDMD_PROCESS_INSTRUCTION::PROCESS_TYPE::SEND_INFORMATION:
                nodeName = "computational:EDMDProcessInstructionSendInformation";
                break;
            case EDMD_PROCESS_INSTRUCTION::PROCESS_TYPE::SEND_CHANGES:
                nodeName = "computational:EDMDProcessInstructionSendChanges";
                break;
            default: nodeName = "computational:EDMDProcessInstruction"; break;
            }

            std::shared_ptr<IDX_XML_NODE> piNode = aNode.AppendChild( nodeName );
            pi->Write( *piNode );

            // Add ProcessType attribute
            piNode->SetAttribute( "ProcessType",
                                  pi->m_processType == EDMD_PROCESS_INSTRUCTION::PROCESS_TYPE::SEND_INFORMATION
                                          ? "SendInformation"
                                          : "SendChanges" );
        }
    }

    // Write body
    std::shared_ptr<IDX_XML_NODE> bodyNode = aNode.AppendChild( "foundation:EDMDDataSetBody" );

    // Write items from the body
    for( const auto& item : m_body.m_items )
    {
        if( !item )
            continue;

        std::shared_ptr<IDX_XML_NODE> itemNode = bodyNode->AppendChild( "pdm:EDMDItem" );
        item->Write( *itemNode );
    }

}


bool EDMD_DATASET::ResolveAll()
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

            auto interstratum = std::dynamic_pointer_cast<idx_pdm::EDMD_INTER_STRATUM_FEATURE>( inst->m_item->m_shape );
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


void EDMD_DESIGNATOR::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_EXTENSIBLE_OBJECT::Write( aNode );

    // Write Persistant and IsOriginator attributes if they have values
    if( m_persistant.has_value() )
        aNode.SetAttribute( "Persistant", m_persistant ? "true" : "false" );

    if( m_isOriginator.has_value() )
        aNode.SetAttribute( "IsOriginator", m_isOriginator ? "true" : "false" );

    WriteValueChild( aNode, "foundation:SystemScope", m_systemScopeRef );
}


// -----------------------------------------------------
// EDMD_IDENTIFIER
// -----------------------------------------------------

EDMD_IDENTIFIER::EDMD_IDENTIFIER( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_DESIGNATOR( aNode, false )
{
    m_xsiType = "foundation:EDMDIdentifier";

    std::string value;
    value = aNode.GetAttribute( "Persistant", "" );

    if( !value.empty() )
        m_persistant = parseBool( value );

    value = aNode.GetAttribute( "IsOriginator", "" );

    if( !value.empty() )
        m_isOriginator = parseBool( value );


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


void EDMD_IDENTIFIER::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_DESIGNATOR::Write( aNode );

    WriteValueChild( aNode, "foundation:Number", m_number );
    WriteValueChild( aNode, "foundation:Version", m_version );
    WriteValueChild( aNode, "foundation:Revision", m_revision );
    WriteValueChild( aNode, "foundation:Sequence", m_sequence );
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
    m_xsiType = "foundation:EDMDName";
    std::string value;

    value = aNode.GetAttribute( "Persistant", "" );

    if( !value.empty() )
        m_persistant = parseBool( value );


    value = aNode.GetAttribute( "IsOriginator", "" );

    if( !value.empty() )
        m_isOriginator = parseBool( value );

    m_originatorId = aNode.GetAttribute( "OriginatorId", "" );

    if( aTopLevel )
        ParseChildren( aNode );
};


void EDMD_NAME::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_DESIGNATOR::Write( aNode );

    WriteValueChild( aNode, "foundation:ObjectName", m_objectName );
}


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
