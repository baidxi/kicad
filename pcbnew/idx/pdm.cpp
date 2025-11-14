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

#include <idx/pdm.h>
#include <iostream>

namespace idx_pdm
{

// -----------------------------------------------------
// EDMD_SHAPE_DESCRIPTION
// -----------------------------------------------------

EDMD_SHAPE_DESCRIPTION::EDMD_SHAPE_DESCRIPTION( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_COLLABORATION_OBJECT( aNode, false )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "pdm:ShapeDescriptionType" ) )
        {
            if( IDX_keyword_compare( content, "GeometricModel" ) )
                m_shapeDescriptionType = SHAPE_DESCRIPTION_TYPE::GEOMETRIC_MODEL;
            else if( IDX_keyword_compare( content, "ExternalGeometricModel" ) )
                m_shapeDescriptionType = SHAPE_DESCRIPTION_TYPE::EXTERNAL_GEOMETRIC_MODEL;
        }
        else if( IDX_keyword_compare( name, "pdm::BoundingBox" ) )
        {
            m_boudingBoxRef = content;
        }
        child = child->GetNext();
    }
}


bool EDMD_SHAPE_DESCRIPTION::Parse( IDX_XML_NODE& aNode )
{
    const std::string content = aNode.GetNodeContent();
    const std::string name = aNode.GetName();

    if( IDX_keyword_compare( name, "pdm:ShapeDescriptionType" ) )
    {
        if( IDX_keyword_compare( content, "GeometricModel" ) )
            m_shapeDescriptionType = SHAPE_DESCRIPTION_TYPE::GEOMETRIC_MODEL;
        else if( IDX_keyword_compare( content, "ExternalGeometricModel" ) )
            m_shapeDescriptionType = SHAPE_DESCRIPTION_TYPE::EXTERNAL_GEOMETRIC_MODEL;
        else
            invalidValueIDX( name, content );
    }
    else if( IDX_keyword_compare( name, "pdm::BoundingBox" ) )
        m_boudingBoxRef = content;
    else
        return this->EDMD_COLLABORATION_OBJECT::Parse( aNode );

    return true;
}

// -----------------------------------------------------
// EDMD_TRANSFORMATION
// -----------------------------------------------------


EDMD_TRANSFORMATION::EDMD_TRANSFORMATION( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_TRANSFORMATION::Parse( IDX_XML_NODE& aNode )
{
    const std::string content = aNode.GetNodeContent();
    const std::string name = aNode.GetName();

    if( IDX_keyword_compare( name, "pdm:xx" ) )
        m_xx = parseDouble( content );
    else if( IDX_keyword_compare( name, "pdm:xy" ) )
        m_xy = parseDouble( content );
    else if( IDX_keyword_compare( name, "pdm:xz" ) )
        m_xz = parseDouble( content );
    else if( IDX_keyword_compare( name, "pdm:yx" ) )
        m_yx = parseDouble( content );
    else if( IDX_keyword_compare( name, "pdm:yy" ) )
        m_yy = parseDouble( content );
    else if( IDX_keyword_compare( name, "pdm:yz" ) )
        m_yz = parseDouble( content );
    else if( IDX_keyword_compare( name, "pdm:zx" ) )
        m_zx = parseDouble( content );
    else if( IDX_keyword_compare( name, "pdm:zy" ) )
        m_zy = parseDouble( content );
    else if( IDX_keyword_compare( name, "pdm:zz" ) )
        m_zz = parseDouble( content );
    else if( IDX_keyword_compare( name, "pdm:tx" ) )
        m_tx = idx_property::EDMD_LENGTH_PROPERTY( aNode, true );
    else if( IDX_keyword_compare( name, "pdm:ty" ) )
        m_ty = idx_property::EDMD_LENGTH_PROPERTY( aNode, true );
    else if( IDX_keyword_compare( name, "pdm:tz" ) )
        m_tz = idx_property::EDMD_LENGTH_PROPERTY( aNode, true );
    else if( IDX_keyword_compare( name, "pdm:TransformationType" ) )
    {
        if( IDX_keyword_compare( content, "d2" ) )
            m_type = TYPE::D2;
        else if( IDX_keyword_compare( content, "d3" ) )
            m_type = TYPE::D3;
        else
            invalidValueIDX( name, content );
    }
    else
        return this->EDMD_OBJECT::Parse( aNode );
    return true;
}


// -----------------------------------------------------
// EDMD_SHAPE_ELEMENT
// -----------------------------------------------------

EDMD_SHAPE_ELEMENT::EDMD_SHAPE_ELEMENT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_SHAPE_ELEMENT::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "pdm:ShapeElementType" ) )
    {
        if( IDX_keyword_compare( content, "NonFeatureShapeElement" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::NON_FEATURE_SHAPE_ELEMENT;
        else if( IDX_keyword_compare( content, "FeatureShapeElement" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::FEATURE_SHAPE_ELEMENT;
        else if( IDX_keyword_compare( content, "PartFeature" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PART_FEATURE;
        else if( IDX_keyword_compare( content, "PartMountingFeature" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PART_MOUNTING_FEATURE;
        else if( IDX_keyword_compare( content, "ComponentTerminationPassageTemplateTerminal" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::COMPONENT_TERMINATION_PASSAGE_TEMPLATE_TERMINAL;
        else if( IDX_keyword_compare( content, "ComponentTerminationPassageTemplateInterfaceTerminal" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::COMPONENT_TERMINATION_PASSAGE_TEMPLATE_INTERFACE_TERMINAL;
        else if( IDX_keyword_compare( content, "ComponentTerminationPassageTemplateJoinTerminal" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::COMPONENT_TERMINATION_PASSAGE_TEMPLATE_JOIN_TERMINAL;
        else if( IDX_keyword_compare( content, "PrintedPartCrossSectionTemplateTerminal" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PRINTED_PART_CROSS_SECTION_TEMPLATE_TERMINAL;
        else if( IDX_keyword_compare( content, "PrintedPartTemplateInterfaceTerminal" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PRINTED_PART_TEMPLATE_INTERFACE_TERMINAL;
        else if( IDX_keyword_compare( content, "PrintedPartTemplateJoinTerminal" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PRINTED_PART_TEMPLATE_JOIN_TERMINAL;
        else if( IDX_keyword_compare( content, "StructuredPrintedPartTemplateTerminal" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::STRUCTURED_PRINTED_PART_TEMPLATE_TERMINAL;
        else if( IDX_keyword_compare( content, "PrintedPartTemplateTerminal" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PRINTED_PART_TEMPLATE_TERMINAL;
        else if( IDX_keyword_compare( content, "PartToolingFeature" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PART_TOOLING_FEATURE;
        else if( IDX_keyword_compare( content, "FiducialPartFeature" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::FIDUCIAL_PART_FEATURE;
        else if( IDX_keyword_compare( content, "ThermalFeature" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::THERMAL_FEATURE;
        else if( IDX_keyword_compare( content, "TestPointPartFeature" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::TEST_POINT_PART_FEATURE;
        else if( IDX_keyword_compare( content, "ConnectionZone" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::CONNECTION_ZONE;
        else if( IDX_keyword_compare( content, "ComponentTerminal" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::COMPONENT_TERMINAL;
        else if( IDX_keyword_compare( content, "BareDieComponentTerminal" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::BARE_DIE_COMPONENT_TERMINAL;
        else if( IDX_keyword_compare( content, "Land" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::LAND;
        else if( IDX_keyword_compare( content, "Package" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PACKAGE;
        else if( IDX_keyword_compare( content, "PackageTerminal" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PACKAGE_TERMINAL;
        else if( IDX_keyword_compare( content, "ViaTerminal" ) )
            m_shapeElementType = SHAPE_ELEMENT_TYPE::VIA_TERMINAL;
        else
        {
            invalidValueIDX( name, content );
        }

        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:Inverted" ) )
    {
        m_inverted = ( IDX_keyword_compare( content, "true" ) );
        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:DefiningShape" ) )
    {
        m_definingShapeRef = content;
        // ✅ m_definingShapeRef modified
        return true;
    }

    // ✅ Fallback to base class parser if no member modified
    return this->EDMD_COLLABORATION_OBJECT::Parse( aNode );
}


bool EDMD_SHAPE_ELEMENT::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = idx_foundation::EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );

    result &= resolve<EDMD_SHAPE_DESCRIPTION>( m_definingShapeRef, &m_definingShape, aRegistry );

    return result;
};


// -----------------------------------------------------
// EDMD_ITEM_SHAPE
// -----------------------------------------------------

EDMD_ITEM_SHAPE::EDMD_ITEM_SHAPE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_ITEM_SHAPE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "pdm:ShapeElement" ) )
    {
        m_shapeElementRef.push_back( content );
        return true;
    }

    return this->EDMD_COLLABORATION_OBJECT::Parse( aNode );
}


void EDMD_ITEM_SHAPE::print( int aIndex, int aMaxIndex )
{
}


bool EDMD_ITEM_SHAPE::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = true;
    result &= idx_foundation::EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );
    result &= resolve<idx_pdm::EDMD_SHAPE_ELEMENT>( m_shapeElementRef, m_shapeElement, aRegistry );
    return result;
}


// -----------------------------------------------------
// EDMD_ITEM_SHAPE
// -----------------------------------------------------


EDMD_FUNCTIONAL_ITEM_SHAPE::EDMD_FUNCTIONAL_ITEM_SHAPE( IDX_XML_NODE& aNode ) :
        EDMD_ITEM_SHAPE( aNode, false )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "pdm:EDMDFunctionalItemShapeType" ) )
        {
            if( IDX_keyword_compare( content, "RigidArea" ) )
                m_type = TYPE::RIGID_AREA;
            else if( IDX_keyword_compare( content, "FlexibleArea" ) )
                m_type = TYPE::FLEXIBLE_AREA;
            else if( IDX_keyword_compare( content, "Stiffener" ) )
                m_type = TYPE::STIFFENER;
            else if( IDX_keyword_compare( content, "UserArea" ) )
                m_type = TYPE::USER_AREA;
            else if( IDX_keyword_compare( content, "PlacementGroupArea" ) )
                m_type = TYPE::PLACEMENT_GROUP;
            else if( IDX_keyword_compare( content, "MechanicalItem" ) )
                m_type = TYPE::MECHANICAL;
        }
        child = child->GetNext();
    }
}


bool EDMD_FUNCTIONAL_ITEM_SHAPE::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = EDMD_ITEM_SHAPE::Resolve( aRegistry );
    return result;
}


// -----------------------------------------------------
// EDMD_STRATUM_TECHNOLOGY
// -----------------------------------------------------


EDMD_STRATUM_TECHNOLOGY::EDMD_STRATUM_TECHNOLOGY( IDX_XML_NODE& aNode ) :
        idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "pdm:StratumTechnologyType" ) )
        {
            if( IDX_keyword_compare( content, "Design" ) )
                m_stratumTechnologyType = STRATUM_TECHNOLOGY_TYPE::DESIGN;
            else if( IDX_keyword_compare( content, "Documentation" ) )
                m_stratumTechnologyType = STRATUM_TECHNOLOGY_TYPE::DOCUMENTATION;
        }
        else if( IDX_keyword_compare( name, "pdm:LayerPurpose" ) )
        {
            if( IDX_keyword_compare( content, "SolderMask" ) )
                m_layerPurpose = LAYER_PURPOSE::SOLDERMASK;
            else if( IDX_keyword_compare( content, "SolderPaste" ) )
                m_layerPurpose = LAYER_PURPOSE::SOLDERPASTE;
            else if( IDX_keyword_compare( content, "SilkScreen" ) )
                m_layerPurpose = LAYER_PURPOSE::SILKSCREEN;
            else if( IDX_keyword_compare( content, "GenericLayer" ) )
                m_layerPurpose = LAYER_PURPOSE::GENERIC_LAYER;
            else if( IDX_keyword_compare( content, "Glue" ) )
                m_layerPurpose = LAYER_PURPOSE::GLUE;
            else if( IDX_keyword_compare( content, "GlueMask" ) )
                m_layerPurpose = LAYER_PURPOSE::GLUEMASK;
            else if( IDX_keyword_compare( content, "PasteMask" ) )
                m_layerPurpose = LAYER_PURPOSE::PASTEMASK;
            else if( IDX_keyword_compare( content, "OtherSignal" ) )
                m_layerPurpose = LAYER_PURPOSE::OTHER_SIGNAL;
            else if( IDX_keyword_compare( content, "LandsOnly" ) )
                m_layerPurpose = LAYER_PURPOSE::LANDS_ONLY;
            else if( IDX_keyword_compare( content, "PowerOrGround" ) )
                m_layerPurpose = LAYER_PURPOSE::POWER_OR_GROUND;
            else if( IDX_keyword_compare( content, "EmbeddedPassiveCapacitorDielectric" ) )
                m_layerPurpose = LAYER_PURPOSE::EMBEDDED_PASSIVE_CAPACITOR_DIELECTRIC;
            else if( IDX_keyword_compare( content, "Dielectric" ) )
                m_layerPurpose = LAYER_PURPOSE::DIELECTRIC;
            else if( IDX_keyword_compare( content, "EmbeddedPassiveResistor" ) )
                m_layerPurpose = LAYER_PURPOSE::EMBEDDED_PASSIVE_RESISTOR;
        }
        else if( IDX_keyword_compare( name, "pdm:LayerPositionType" ) )
        {
            if( IDX_keyword_compare( content, "Primary" ) )
                m_layerPositionType = LAYER_POSITION_TYPE::PRIMARY;
            else if( IDX_keyword_compare( content, "Secondary" ) )
                m_layerPositionType = LAYER_POSITION_TYPE::SECONDARY;
            else if( IDX_keyword_compare( content, "All" ) )
                m_layerPositionType = LAYER_POSITION_TYPE::ALL;
            else if( IDX_keyword_compare( content, "External" ) )
                m_layerPositionType = LAYER_POSITION_TYPE::EXTERNAL;
            else if( IDX_keyword_compare( content, "Internal" ) )
                m_layerPositionType = LAYER_POSITION_TYPE::INTERNAL;
        }

        child = child->GetNext();
    }
}


// -----------------------------------------------------
// EDMD_STRATUM
// -----------------------------------------------------


EDMD_STRATUM::EDMD_STRATUM( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_ITEM_SHAPE( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_STRATUM::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "pdm:StratumTechnology" ) )
    {
        m_stratumTechnologyRef.push_back( content );
        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:StratumType" ) )
    {
        return false;
    }
    else if( IDX_keyword_compare( name, "pdm:StratumSurfaceDesignation" ) )
    {
        if( IDX_keyword_compare( content, "AverageSurface" ) )
        {
            m_stratumSurfaceDesignation = STRATUM_SURFACE_DESIGNATION::AVERAGE_SURFACE;
            return true;
        }
        else if( IDX_keyword_compare( content, "PrimarySurface" ) )
        {
            m_stratumSurfaceDesignation = STRATUM_SURFACE_DESIGNATION::PRIMARY_SURFACE;
            return true;
        }
        else if( IDX_keyword_compare( content, "SecondarySurface" ) )
        {
            m_stratumSurfaceDesignation = STRATUM_SURFACE_DESIGNATION::SECONDARY_SURFACE;
            return true;
        }
        return false;
    }

    return this->EDMD_ITEM_SHAPE::Parse( aNode );
}

bool EDMD_STRATUM::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = EDMD_ITEM_SHAPE::Resolve( aRegistry );
    result &= resolve<EDMD_STRATUM_TECHNOLOGY>( m_stratumTechnologyRef, m_stratumTechnology, aRegistry );

    return result;
}

// -----------------------------------------------------
// EDMD_KEEP_CONSTRAINT
// -----------------------------------------------------


EDMD_KEEP_CONSTRAINT::EDMD_KEEP_CONSTRAINT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_ITEM_SHAPE( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_KEEP_CONSTRAINT::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "pdm:Purpose" ) )
    {
        if( IDX_keyword_compare( content, "Thermal" ) )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::THERMAL );
        else if( IDX_keyword_compare( content, "GenericClearance" ) )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::GENERIC_CLEARANCE );
        else if( IDX_keyword_compare( content, "Shock" ) )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::SHOCK );
        else if( IDX_keyword_compare( content, "Vibration" ) )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::VIBRATION );
        else if( IDX_keyword_compare( content, "ElectromagneticCompatibility" ) )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::EMC );
        else if( IDX_keyword_compare( content, "ComponentPlacement" ) )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::COMPONENT_PLACEMENT );
        else if( IDX_keyword_compare( content, "Route" ) )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::ROUTE );
        else if( IDX_keyword_compare( content, "Via" ) )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::VIA );
        else if( IDX_keyword_compare( content, "TestPoint" ) )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::TESTPOINT );
        else if( IDX_keyword_compare( content, "ValueLimit" ) )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::VALUELIMIT );
        else if( IDX_keyword_compare( content, "Plane" ) )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::PLANE );
        else if( IDX_keyword_compare( content, "Other" ) )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::OTHER );
        else
            invalidValueIDX( name, content );
    }
    else if( IDX_keyword_compare( name, "pdm:KeepConstraintCondition" ) )
        m_keepConstraintCondition.emplace_back(
                std::make_shared<idx_property::EDMD_CONSTRAINED_PROPERTY>( aNode, true ) );
    else
        return this->EDMD_ITEM_SHAPE::Parse( aNode );
    return true;
}


// -----------------------------------------------------
// EDMD_KEEPOUT
// -----------------------------------------------------


EDMD_KEEPOUT::EDMD_KEEPOUT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_KEEP_CONSTRAINT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


// -----------------------------------------------------
// EDMD_KEEPIN
// -----------------------------------------------------


EDMD_KEEPIN::EDMD_KEEPIN( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_KEEP_CONSTRAINT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


// -----------------------------------------------------
// EDMD_ITEM_INSTANCE
// -----------------------------------------------------


EDMD_ITEM_INSTANCE::EDMD_ITEM_INSTANCE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_EXTENSIBLE_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_ITEM_INSTANCE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "pdm:InstanceName" ) )
        m_instanceName = idx_foundation::EDMD_NAME( aNode, true );
    else if( IDX_keyword_compare( name, "pdm:Transformation" ) )
        m_transformation = idx_pdm::EDMD_TRANSFORMATION( aNode, true );
    else if( IDX_keyword_compare( name, "pdm:Item" ) )
        m_itemRef = content;
    else if( IDX_keyword_compare( name, "pdm:InstanceUserAreaLayerName" ) )
        m_instanceUserAreaLayerName.emplace_back( aNode, true );
    else if( IDX_keyword_compare( name, "pdm:zOffset" ) )
        m_zOffset = parseDouble( content );
    else if( IDX_keyword_compare( name, "foundation:Name" ) )
        m_name = content;
    else
        return this->EDMD_EXTENSIBLE_OBJECT::Parse( aNode );
    return true;
}


void EDMD_ITEM_INSTANCE::print( int aIndex, int aMaxIndex )
{
}

bool EDMD_ITEM_INSTANCE::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = true;
    result &= idx_foundation::EDMD_EXTENSIBLE_OBJECT::Resolve( aRegistry );
    result &= resolve<idx_pdm::EDMD_ITEM>( m_itemRef, &m_item, aRegistry );
    return result;
}


// -----------------------------------------------------
// EDMD_ITEM
// -----------------------------------------------------

EDMD_ITEM::EDMD_ITEM( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_COLLABORATION_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_ITEM::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "pdm:ItemType" ) )
    {
        if( IDX_keyword_compare( content, "single" ) )
            m_itemType = ITEM_TYPE::SINGLE;
        else if( IDX_keyword_compare( content, "assembly" ) )
            m_itemType = ITEM_TYPE::ASSEMBLY;
        else
            invalidValueIDX( name, content );
        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:Identifier" ) )
    {
        m_identifier.emplace_back( aNode, true );
        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:PackageName" ) )
    {
        m_packageName.emplace_back( aNode, true );
        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:ItemInstance" ) )
    {
        m_itemInstance.emplace_back( std::make_shared<idx_pdm::EDMD_ITEM_INSTANCE>( aNode, true ) );
        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:Shape" ) )
    {
        m_shapeRef = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:BaseLine" ) )
    {
        m_baseline = idx_property::EDMD_LOGIC_PROPERTY( aNode, true );
        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:Accept" ) )
    {
        m_baseline = idx_property::EDMD_LOGIC_PROPERTY( aNode, true );
        return true;
    }

    return this->EDMD_COLLABORATION_OBJECT::Parse( aNode );
}


void EDMD_ITEM::print( int aIndex, int aMaxIndex )
{
}

bool EDMD_ITEM::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = idx_foundation::EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );

    if( !m_shapeRef.empty() )
    {
        result = resolve<EDMD_ITEM_SHAPE>( m_shapeRef, &m_shape, aRegistry, true );
        if( !result )
            result = resolve<EDMD_FUNCTIONAL_ITEM_SHAPE>( m_shapeRef, &m_shape, aRegistry );
    }

    for( auto& instance : m_itemInstance )
        result &= instance->Resolve( aRegistry );

    if( m_shape )
        result &= m_shape->Resolve( aRegistry );

    return result;
}


// -----------------------------------------------------
// EDMD_ASSEMBLY_COMPONENT
// -----------------------------------------------------

EDMD_ASSEMBLY_COMPONENT::EDMD_ASSEMBLY_COMPONENT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_ITEM_SHAPE( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_ASSEMBLY_COMPONENT::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();


    if( IDX_keyword_compare( name, "pdm:AssemblyComponentType" ) )
    {
        if( IDX_keyword_compare( content, "AssemblyGroupComponent" ) )
            m_assemblyComponentType = ASSEMBLY_COMPONENT_TYPE::ASSEMBLY_GROUP_COMPONENT;
        else if( IDX_keyword_compare( content, "LaminateComponent" ) )
            m_assemblyComponentType = ASSEMBLY_COMPONENT_TYPE::LAMINATE_COMPONENT;
        else if( IDX_keyword_compare( content, "ThermalComponent" ) )
            m_assemblyComponentType = ASSEMBLY_COMPONENT_TYPE::THERMAL_COMPONENT;
        else if( IDX_keyword_compare( content, "Physical" ) )
            m_assemblyComponentType = ASSEMBLY_COMPONENT_TYPE::PHYSICAL;
        else if( IDX_keyword_compare( content, "Padstack" ) )
            m_assemblyComponentType = ASSEMBLY_COMPONENT_TYPE::PADSTACK;
        else
            return false;
        return true;
    }

    return this->EDMD_ITEM_SHAPE::Parse( aNode );
}

bool EDMD_ASSEMBLY_COMPONENT::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = idx_foundation::EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );

    //result &=
    //          resolve<EDMD_SHAPE_DESCRIPTION>( m_shapeRef, &m_shape, aRegistry );

    return result;
}


// -----------------------------------------------------
// EDMD_INTERSTRATUM_FEATURE
// -----------------------------------------------------

EDMD_INTERSTRATUM_FEATURE::EDMD_INTERSTRATUM_FEATURE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_ITEM_SHAPE( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};

bool EDMD_INTERSTRATUM_FEATURE::Parse( IDX_XML_NODE& aNode )
{
    const std::string content = aNode.GetNodeContent();
    const std::string name = aNode.GetName();

    if( IDX_keyword_compare( name, "pdm:InterStratumFeatureType" ) )
    {
        if( IDX_keyword_compare( content, "UnsupportedPassage" ) )
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::UNSUPPORTED_PASSAGE;
        else if( IDX_keyword_compare( content, "StratumFeatureTemplateComponent" ) )
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::STRATUM_FEATURE_TEMPLATE_COMPONENT;
        else if( IDX_keyword_compare( content, "CutoutEdgeSegment" ) )
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::CUTOUT_EDGE_SEGMENT;
        else if( IDX_keyword_compare( content, "PlatedCutoutEdgeSegment" ) )
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::PLATED_CUTOUT_EDGE_SEGMENT;
        else if( IDX_keyword_compare( content, "Cutout" ) )
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::CUTOUT;
        else if( IDX_keyword_compare( content, "PlatedInterStratumFeature" ) )
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::PLATED_INTERSTRATUM_FEATURE;
        else if( IDX_keyword_compare( content, "PlatedPassage" ) )
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::PLATED_PASSAGE;
        else if( IDX_keyword_compare( content, "MilledCutout" ) )
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::MILLED_CUTOUT;
        else if( IDX_keyword_compare( content, "PlatedCutout" ) )
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::PLATED_CUTOUT;
        else if( IDX_keyword_compare( content, "Via" ) )
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::VIA;
        else if( IDX_keyword_compare( content, "PhysicalConnectivityInterruptingCutout" ) )
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::PHYSICAL_CONNECTIVITY_INTERRUPTING_CUTOUT;
        else if( IDX_keyword_compare( content, "ComponentTerminationPassage" ) )
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::COMPONENT_TERMINATION_PASSAGE;
        else if( IDX_keyword_compare( content, "FilledVia" ) )
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::FILLED_VIA;
        else if( IDX_keyword_compare( content, "MechanicalComponent" ) )
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::MECHANICAL_COMPONENT;
        else if( IDX_keyword_compare( content, "CustomElectroMechanicalComponent" ) )
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::CUSTOM_ELECTROMECHANICAL_COMPONENT;
        else if( IDX_keyword_compare( content, "BoardCutout" ) )
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::BOARD_CUTOUT;
        else
            invalidValueIDX( name, content );

        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:Stratum" ) )
    {
        m_stratumRef.push_back( content );
        return true;
    }

    return this->EDMD_ITEM_SHAPE::Parse( aNode );
}


bool EDMD_INTERSTRATUM_FEATURE::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = idx_pdm::EDMD_ITEM_SHAPE::Resolve( aRegistry );

    result &= resolve<EDMD_STRATUM>( m_stratumRef, m_stratum, aRegistry );

    return result;
}


} // namespace idx_pdm