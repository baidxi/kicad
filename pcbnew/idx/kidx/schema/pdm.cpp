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

#include <idx/kidx/schema/pdm.h>
#include <idx/kidx/schema/geometry.h>
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
        else if( IDX_keyword_compare( name, "pdm:BoundingBox" ) )
        {
            m_boundingBoxRef = content;
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
    else if( IDX_keyword_compare( name, "pdm:BoundingBox" ) )
        m_boundingBoxRef = content;
    else
        return this->EDMD_COLLABORATION_OBJECT::Parse( aNode );

    return true;
}


void EDMD_SHAPE_DESCRIPTION::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_COLLABORATION_OBJECT::Write( aNode );

    // Write ShapeDescriptionType
    std::string typeStr;
    switch( m_shapeDescriptionType )
    {
    case SHAPE_DESCRIPTION_TYPE::GEOMETRIC_MODEL: typeStr = "GeometricModel"; break;
    case SHAPE_DESCRIPTION_TYPE::EXTERNAL_GEOMETRIC_MODEL: typeStr = "ExternalGeometricModel"; break;
    default: typeStr = "Unknown"; break;
    }
    if( !typeStr.empty() )
        aNode.AppendChild( "pdm:ShapeDescriptionType" )->SetText( typeStr );

    // Write BoundingBox
    if( m_boundingBox )
    {
        std::shared_ptr<IDX_XML_NODE> bbNode = aNode.AppendChild( "pdm:BoundingBox" );
        m_boundingBox->Write( *bbNode );
    }
    else if( !m_boundingBoxRef.empty() )
    {
        aNode.AppendChild( "pdm:BoundingBox" )->SetText( m_boundingBoxRef );
    }
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
    {
        m_xx = parseDouble( content );
        m_xx_str = content;
    }
    else if( IDX_keyword_compare( name, "pdm:xy" ) )
    {
        m_xy = parseDouble( content );
        m_xy_str = content;
    }
    else if( IDX_keyword_compare( name, "pdm:xz" ) )
    {
        m_xz = parseDouble( content );
        m_xz_str = content;
    }
    else if( IDX_keyword_compare( name, "pdm:yx" ) )
    {
        m_yx = parseDouble( content );
        m_yx_str = content;
    }
    else if( IDX_keyword_compare( name, "pdm:yy" ) )
    {
        m_yy = parseDouble( content );
        m_yy_str = content;
    }
    else if( IDX_keyword_compare( name, "pdm:yz" ) )
    {
        m_yz = parseDouble( content );
        m_yz_str = content;
    }
    else if( IDX_keyword_compare( name, "pdm:zx" ) )
    {
        m_zx = parseDouble( content );
        m_zx_str = content;
    }
    else if( IDX_keyword_compare( name, "pdm:zy" ) )
    {
        m_zy = parseDouble( content );
        m_zy_str = content;
    }
    else if( IDX_keyword_compare( name, "pdm:zz" ) )
    {
        m_zz = parseDouble( content );
        m_zz_str = content;
    }
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


void EDMD_TRANSFORMATION::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_OBJECT::Write( aNode );

    switch( m_type )
    {
    case TYPE::D2: aNode.AppendChild( "pdm:TransformationType" )->SetText( "d2" ); break;
    case TYPE::D3: aNode.AppendChild( "pdm:TransformationType" )->SetText( "d3" ); break;
    }

    WriteValueChild( aNode, "pdm:xx", m_xx_str );
    WriteValueChild( aNode, "pdm:xy", m_xy_str );
    WriteValueChild( aNode, "pdm:xz", m_xz_str );

    WriteValueChild( aNode, "pdm:yx", m_yx_str );
    WriteValueChild( aNode, "pdm:yy", m_yy_str );
    WriteValueChild( aNode, "pdm:yz", m_yz_str );

    WriteValueChild( aNode, "pdm:zx", m_zx_str );
    WriteValueChild( aNode, "pdm:zy", m_zy_str );
    WriteValueChild( aNode, "pdm:zz", m_zz_str );

    WriteChild( aNode, "pdm:tx", m_tx );
    WriteChild( aNode, "pdm:ty", m_ty );
    WriteChild( aNode, "pdm:tz", m_tz );
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

void EDMD_ITEM_SHAPE::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_COLLABORATION_OBJECT::Write( aNode );

    for( auto se : m_shapeElement )
        WriteValueChild( aNode, "pdm:ShapeElement", se->m_id );
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
    m_xsiType = "pdm:EDMDStratum";

    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_STRATUM::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "pdm:StratumTechnology" ) )
        m_stratumTechnologyRef.push_back( content );
    else if( IDX_keyword_compare( name, "pdm:StratumType" ) )
    {
        if( IDX_keyword_compare( content, "DocumentationLayerStratum" ) )
            m_type = TYPE::DOCUMENTATION;
        else if( IDX_keyword_compare( content, "DesignLayerStratum" ) )
            m_type = TYPE::DESIGN;
        else
            invalidValueIDX( name, content );
    }
    else if( IDX_keyword_compare( name, "pdm:StratumSurfaceDesignation" ) )
    {
        if( IDX_keyword_compare( content, "AverageSurface" ) )
            m_stratumSurfaceDesignation = STRATUM_SURFACE_DESIGNATION::AVERAGE_SURFACE;
        else if( IDX_keyword_compare( content, "PrimarySurface" ) )
            m_stratumSurfaceDesignation = STRATUM_SURFACE_DESIGNATION::PRIMARY_SURFACE;

        else if( IDX_keyword_compare( content, "SecondarySurface" ) )
            m_stratumSurfaceDesignation = STRATUM_SURFACE_DESIGNATION::SECONDARY_SURFACE;
        else
            invalidValueIDX( name, content );
    }
    else
        return this->EDMD_ITEM_SHAPE::Parse( aNode );

    return true;
}


void EDMD_STRATUM::Write( IDX_XML_NODE& aNode ) const
{
    aNode.SetName( "foundation:Stratum" );
    this->EDMD_ITEM_SHAPE::Write( aNode );


    std::string value;

    switch( m_type )
    {
    case TYPE::DOCUMENTATION: value = "DocumentationLayerStratum"; break;
    case TYPE::DESIGN: value = "DesignLayerStratum"; break;
    }

    WriteValueChild( aNode, "pdm:StratumType", value );

    switch( m_stratumSurfaceDesignation )
    {
    case STRATUM_SURFACE_DESIGNATION::PRIMARY_SURFACE: value = "PrimarySurface"; break;
    case STRATUM_SURFACE_DESIGNATION::SECONDARY_SURFACE: value = "SecondarySurface"; break;
    case STRATUM_SURFACE_DESIGNATION::AVERAGE_SURFACE: value = "AverageSurface"; break;
    }

    WriteValueChild( aNode, "pdm:StratumSurfaceDesignation", value );
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
// EDMD_KEEP_OUT
// -----------------------------------------------------


EDMD_KEEP_OUT::EDMD_KEEP_OUT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_KEEP_CONSTRAINT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


// -----------------------------------------------------
// EDMD_KEEP_IN
// -----------------------------------------------------


EDMD_KEEP_IN::EDMD_KEEP_IN( IDX_XML_NODE& aNode, bool aTopLevel ) :
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
    m_xsiType = "pdm:EDMDItemInstance";

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

EDMD_ITEM_INSTANCE::TYPE EDMD_ITEM_INSTANCE::GetType() const
{
    if( auto type = GetUserProperty( "TYPE" ) )
    {
        if( type->m_value == "BOARDOUTLINE" )
            return TYPE::BOARD_OUTLINE;
    }


    auto interstratum = std::dynamic_pointer_cast<idx_pdm::EDMD_INTER_STRATUM_FEATURE>( m_item->m_shape );
    if( interstratum )
    {
        using FT = idx_pdm::INTERSTRATUM_FEATURE_TYPE;
        switch( interstratum->m_interstratumFeatureType )
        {
        case FT::BOARD_CUTOUT: return TYPE::BOARD_OUTLINE;
        case FT::CUTOUT: return TYPE::NPTH;
        case FT::PLATED_CUTOUT: return TYPE::PTH;
        default: break;
        }
    }

    if( m_item && m_item->m_shape )
    {
        auto keep = std::dynamic_pointer_cast<idx_pdm::EDMD_KEEP_CONSTRAINT>( m_item->m_shape );
        if( keep )
            return TYPE::KEEP_OUT;
    }

    if( !m_name.empty() )
        return TYPE::COMPONENT;

    return TYPE::OTHER;
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


void EDMD_ITEM_INSTANCE::Write( IDX_XML_NODE& aNode ) const
{
    if( m_item )
        WriteValueChild( aNode, "pdm:Item", m_item->m_id );

    this->EDMD_EXTENSIBLE_OBJECT::Write( aNode );


    WriteChild( aNode, "pdm:InstanceName", m_instanceName );
    WriteChild( aNode, "pdm:Transformation", m_transformation );


    /*
    else if( IDX_keyword_compare( name, "pdm:InstanceUserAreaLayerName" ) )
        m_instanceUserAreaLayerName.emplace_back( aNode, true );
    else if( IDX_keyword_compare( name, "pdm:zOffset" ) )
        m_zOffset = parseDouble( content );
    else if( IDX_keyword_compare( name, "foundation:Name" ) )
        m_name = content;
    */
}

// -----------------------------------------------------
// EDMD_ITEM
// -----------------------------------------------------

EDMD_ITEM::EDMD_ITEM( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_COLLABORATION_OBJECT( aNode, false )
{
    m_xsiType = "foundation:Item";

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
    }
    else if( IDX_keyword_compare( name, "pdm:Identifier" ) )
        m_identifier.emplace_back( aNode, true );
    else if( IDX_keyword_compare( name, "pdm:PackageName" ) )
        m_packageName.emplace_back( aNode, true );
    else if( IDX_keyword_compare( name, "pdm:ItemInstance" ) )
        m_itemInstance.emplace_back( std::make_shared<idx_pdm::EDMD_ITEM_INSTANCE>( aNode, true ) );
    else if( IDX_keyword_compare( name, "pdm:Shape" ) )
        m_shapeRef = content;
    else if( IDX_keyword_compare( name, "pdm:Accept" ) )
        m_baseline = idx_property::EDMD_LOGIC_PROPERTY( aNode, true );
    else if( IDX_keyword_compare( name, "pdm:BaseLine" ) )
        m_baseline = idx_property::EDMD_LOGIC_PROPERTY( aNode, true );
    else if( IDX_keyword_compare( name, "pdm:ReferenceName" ) )
        m_referenceName = content;
    else if( IDX_keyword_compare( name, "pdm:AssembleToName" ) )
        m_assembleToName = content;
    else
        return this->EDMD_COLLABORATION_OBJECT::Parse( aNode );

    return true;
}


void EDMD_ITEM::print( int aIndex, int aMaxIndex )
{
}

bool EDMD_ITEM::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = true;

    result &= idx_foundation::EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );

    if( !m_shapeRef.empty() )
    {
        bool tmp = resolve<EDMD_ITEM_SHAPE>( m_shapeRef, &m_shape, aRegistry, true );

        if( !tmp )
        {
            result &= resolve<EDMD_FUNCTIONAL_ITEM_SHAPE>( m_shapeRef, &m_shape, aRegistry );
        }
    }

    for( auto& instance : m_itemInstance )
        result &= instance->Resolve( aRegistry );

    if( m_shape )
        result &= m_shape->Resolve( aRegistry );

    return result;
}


void EDMD_ITEM::Write( IDX_XML_NODE& aNode ) const
{
    aNode.SetName( m_xsiType );
    aNode.SetAttribute( "id", m_id );

    this->EDMD_COLLABORATION_OBJECT::Write( aNode );


    switch( m_itemType )
    {
    case ITEM_TYPE::SINGLE: WriteValueChild( aNode, "pdm:ItemType", std::string( "single" ) ); break;
    case ITEM_TYPE::ASSEMBLY: WriteValueChild( aNode, "pdm:ItemType", std::string( "assembly" ) ); break;
    }

    for( auto id : m_identifier )
        WriteChild( aNode, "pdm:Identifier", id );

    for( auto pn : m_packageName )
        WriteChild( aNode, "pdm:PackageName", pn );

    for( auto ii : m_itemInstance )
    {
        if( ii )
            WriteChild( aNode, "pdm:ItemInstance", *ii );
    }

    WriteValueChild( aNode, "pdm:ReferenceName", m_referenceName );

    WriteChild( aNode, "pdm:BaseLine", m_baseline );

    if( m_shape )
        WriteValueChild( aNode, "pdm:Shape", m_shape->m_id );

    // Write Accept property if it has been set
    if( m_accept.m_isChanged.has_value() && m_accept.m_isChanged.value() )
        WriteValueChild( aNode, "property:Accept", std::string( m_accept.m_value > 0.5 ? "true" : "false" ) );
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


void EDMD_ASSEMBLY_COMPONENT::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_ITEM_SHAPE::Write( aNode );

    // Write AssemblyComponentType
    std::string typeStr;
    switch( m_assemblyComponentType )
    {
    case ASSEMBLY_COMPONENT_TYPE::ASSEMBLY_GROUP_COMPONENT: typeStr = "AssemblyGroupComponent"; break;
    case ASSEMBLY_COMPONENT_TYPE::LAMINATE_COMPONENT: typeStr = "LaminateComponent"; break;
    case ASSEMBLY_COMPONENT_TYPE::THERMAL_COMPONENT: typeStr = "ThermalComponent"; break;
    case ASSEMBLY_COMPONENT_TYPE::PHYSICAL: typeStr = "Physical"; break;
    case ASSEMBLY_COMPONENT_TYPE::PADSTACK: typeStr = "Padstack"; break;
    default: typeStr = "Unknown"; break;
    }
    if( !typeStr.empty() )
        aNode.AppendChild( "pdm:AssemblyComponentType" )->SetText( typeStr );
}


bool EDMD_ASSEMBLY_COMPONENT::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = idx_pdm::EDMD_ITEM_SHAPE::Resolve( aRegistry );
    return result;
}


// -----------------------------------------------------
// EDMD_INTER_STRATUM_FEATURE
// -----------------------------------------------------

EDMD_INTER_STRATUM_FEATURE::EDMD_INTER_STRATUM_FEATURE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_ITEM_SHAPE( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};

bool EDMD_INTER_STRATUM_FEATURE::Parse( IDX_XML_NODE& aNode )
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


bool EDMD_INTER_STRATUM_FEATURE::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = idx_pdm::EDMD_ITEM_SHAPE::Resolve( aRegistry );

    result &= resolve<EDMD_STRATUM>( m_stratumRef, m_stratum, aRegistry );

    return result;
}


// -----------------------------------------------------
// EDMD_PACKAGE_PIN
// -----------------------------------------------------

EDMD_PACKAGE_PIN::EDMD_PACKAGE_PIN( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );

    // Handle attributes
    m_pin_number = aNode.GetAttribute( "pin_number", "" );
    m_primary = parseBool( aNode.GetAttribute( "primary", "false" ) );
}


bool EDMD_PACKAGE_PIN::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "pdm:Point" ) )
    {
        m_pointRef = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:Shape" ) )
    {
        m_shapeRef = content;
        return true;
    }
    else
        return this->EDMD_COLLABORATION_OBJECT::Parse( aNode );

    return true;
}


// -----------------------------------------------------
// EDMD_3D_MODEL
// -----------------------------------------------------

EDMD_3D_MODEL::EDMD_3D_MODEL( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}


bool EDMD_3D_MODEL::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "pdm:ModelIdentifier" ) )
    {
        m_modelIdentifier = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:ModelVersion" ) )
    {
        m_modelVersion = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:ModelLocation" ) )
    {
        m_modelLocation = content;
        return true;
    }
    else
        return this->EDMD_COLLABORATION_OBJECT::Parse( aNode );

    return true;
}


// -----------------------------------------------------
// EDMD_BEND
// -----------------------------------------------------

EDMD_BEND::EDMD_BEND( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_ITEM_SHAPE( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );

    // Handle SequenceNumber attribute
    std::string seqNum = aNode.GetAttribute( "SequenceNumber", "0" );
    if( !seqNum.empty() )
        m_sequenceNumber = parseInt( seqNum );
}


bool EDMD_BEND::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "pdm:BendType" ) )
    {
        m_bendTypeRef = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:SequenceNumber" ) )
    {
        m_sequenceNumber = parseInt( content );
        return true;
    }
    else
        return this->EDMD_ITEM_SHAPE::Parse( aNode );

    return true;
}


// -----------------------------------------------------
// EDMD_BEND_TYPE
// -----------------------------------------------------

EDMD_BEND_TYPE::EDMD_BEND_TYPE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}


bool EDMD_BEND_TYPE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "pdm:BendLine" ) )
    {
        m_bendLineRef = content;
        return true;
    }
    else
        return this->EDMD_COLLABORATION_OBJECT::Parse( aNode );

    return true;
}


// -----------------------------------------------------
// EDMD_CIRCULAR_BEND_TYPE
// -----------------------------------------------------

EDMD_CIRCULAR_BEND_TYPE::EDMD_CIRCULAR_BEND_TYPE( IDX_XML_NODE& aNode ) :
        EDMD_BEND_TYPE( aNode, false )
{
}


bool EDMD_CIRCULAR_BEND_TYPE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "pdm:InnerSide" ) )
    {
        if( IDX_keyword_compare( content, "Top" ) )
            m_innerSide = EDMDINNER_SIDE::TOP;
        else if( IDX_keyword_compare( content, "Bottom" ) )
            m_innerSide = EDMDINNER_SIDE::BOTTOM;
        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:InnerRadius" ) )
    {
        m_innerRadius = parseDouble( content );
        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:InnerAngle" ) )
    {
        m_innerAngle = parseDouble( content );
        return true;
    }
    else
        return this->EDMD_BEND_TYPE::Parse( aNode );

    return true;
}


// -----------------------------------------------------
// Resolve methods for new classes
// -----------------------------------------------------

bool EDMD_PACKAGE_PIN::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = idx_foundation::EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );

    result &= resolve<idx_geometry::EDMD_POINT>( m_pointRef, m_point, aRegistry );
    result &= resolve<idx_pdm::EDMD_ITEM_SHAPE>( m_shapeRef, m_shape, aRegistry );

    return result;
}


bool EDMD_BEND::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = idx_pdm::EDMD_ITEM_SHAPE::Resolve( aRegistry );

    result &= resolve<idx_pdm::EDMD_ITEM_SHAPE>( m_bendTypeRef, m_bendType, aRegistry );

    return result;
}


bool EDMD_BEND_TYPE::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = idx_foundation::EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );

    result &= resolve<idx_geometry::EDMD_CURVE>( m_bendLineRef, m_bendLine, aRegistry );

    return result;
}


// -----------------------------------------------------
// EDMD_NON_FEATURE_ITEM_SHAPE
// -----------------------------------------------------

EDMD_NON_FEATURE_ITEM_SHAPE::EDMD_NON_FEATURE_ITEM_SHAPE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_ITEM_SHAPE( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}


bool EDMD_NON_FEATURE_ITEM_SHAPE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "pdm:NonFeatureItemShapePurposePurpose" ) )
    {
        if( IDX_keyword_compare( content, "InfoText" ) )
            m_purpose = NON_FEATURE_ITEM_SHAPE_PURPOSE::INFO_TEXT;
        else if( IDX_keyword_compare( content, "InfoGeometry" ) )
            m_purpose = NON_FEATURE_ITEM_SHAPE_PURPOSE::INFO_GEOMETRY;
        else if( IDX_keyword_compare( content, "AuxiliaryGeometry" ) )
            m_purpose = NON_FEATURE_ITEM_SHAPE_PURPOSE::AUXILIARY_GEOMETRY;
        return true;
    }
    else if( IDX_keyword_compare( name, "pdm:RelatedItemShape" ) )
    {
        m_relatedItemShapeRef.push_back( content );
        return true;
    }
    else
        return this->EDMD_ITEM_SHAPE::Parse( aNode );

    return true;
}


bool EDMD_NON_FEATURE_ITEM_SHAPE::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = idx_pdm::EDMD_ITEM_SHAPE::Resolve( aRegistry );

    result &= resolve<idx_pdm::EDMD_ITEM_SHAPE>( m_relatedItemShapeRef, m_relatedItemShape, aRegistry );

    return result;
}


void EDMD_PACKAGE_PIN::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_COLLABORATION_OBJECT::Write( aNode );

    // Write Point reference or actual point
    if( m_point )
    {
        std::shared_ptr<IDX_XML_NODE> pointNode = aNode.AppendChild( "pdm:Point" );
        m_point->Write( *pointNode );
    }
    else if( !m_pointRef.empty() )
    {
        aNode.AppendChild( "pdm:Point" )->SetText( m_pointRef );
    }

    // Write Shape reference or actual shape
    if( m_shape )
    {
        std::shared_ptr<IDX_XML_NODE> shapeNode = aNode.AppendChild( "pdm:Shape" );
        m_shape->Write( *shapeNode );
    }
    else if( !m_shapeRef.empty() )
    {
        aNode.AppendChild( "pdm:Shape" )->SetText( m_shapeRef );
    }

    // Write pin number
    if( !m_pin_number.empty() )
        aNode.AppendChild( "pdm:PinNumber" )->SetText( m_pin_number );

    // Write primary flag
    if( m_primary )
        aNode.AppendChild( "pdm:Primary" )->SetText( "true" );
}


void EDMD_3D_MODEL::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_COLLABORATION_OBJECT::Write( aNode );

    if( !m_modelIdentifier.empty() )
        aNode.AppendChild( "pdm:ModelIdentifier" )->SetText( m_modelIdentifier );

    if( !m_modelVersion.empty() )
        aNode.AppendChild( "pdm:ModelVersion" )->SetText( m_modelVersion );

    if( !m_modelLocation.empty() )
        aNode.AppendChild( "pdm:ModelLocation" )->SetText( m_modelLocation );
}


} // namespace idx_pdm