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

EDMD_SHAPE_DESCRIPTION::EDMD_SHAPE_DESCRIPTION( wxXmlNode& aNode ) : EDMD_COLLABORATION_OBJECT( aNode, false )
    {
        wxXmlNode* child = aNode.GetChildren();

        while( child )
        {
            const wxString content = child->GetNodeContent();
            const wxString name = child->GetName();

            if( name == "pdm:ShapeDescriptionType" )
            {
                if( content == "GeometricModel" )
                    m_shapeDescriptionType = SHAPE_DESCRIPTION_TYPE::GEOMETRIC_MODEL;
                else if( content == "ExternalGeometricModel" )
                    m_shapeDescriptionType = SHAPE_DESCRIPTION_TYPE::EXTERNAL_GEOMETRIC_MODEL;
            }
            else if( name == "pdm::BoundingBox" )
            {
                m_boudingBoxRef = content;
            }
            child = child->GetNext();
        }
    }


// -----------------------------------------------------
// EDMD_TRANSFORMATION
// -----------------------------------------------------


EDMD_TRANSFORMATION::EDMD_TRANSFORMATION( wxXmlNode& aNode, bool aTopLevel ) : idx_foundation::EDMD_OBJECT( aNode, false )
{

    if( !aTopLevel )
        return;

    wxXmlNode* child = aNode.GetChildren();

    while (child)
    {
        if ( !this->EDMD_TRANSFORMATION::Parse( *child ) )
            this->unknownKeywordIDX( child->GetName() );

        child = child->GetNext();
    }
}


bool EDMD_TRANSFORMATION::Parse( wxXmlNode& aNode )
{
    const wxString content = aNode.GetNodeContent();
    const wxString name = aNode.GetName();

    if( name == "pdm:xx" )
    {
        content.ToCDouble( &m_xx );
        return true;
    }
    else if( name == "pdm:xy" )
    {
        content.ToCDouble( &m_xy );
        return true;
    }
    else if( name == "pdm:xz" )
    {
        content.ToCDouble( &m_xz );
        return true;
    }
    else if( name == "pdm:yx" )
    {
        content.ToCDouble( &m_yx );
        return true;
    }
    else if( name == "pdm:yy" )
    {
        content.ToCDouble( &m_yy );
        return true;
    }
    else if( name == "pdm:yz" )
    {
        content.ToCDouble( &m_yz );
        return true;
    }
    else if( name == "pdm:zx" )
    {
        content.ToCDouble( &m_zx );
        return true;
    }
    else if( name == "pdm:zy" )
    {
        content.ToCDouble( &m_zy );
        return true;
    }
    else if( name == "pdm:zz" )
    {
        content.ToCDouble( &m_zz );
        return true;
    }
    else if( name == "pdm:tx" )
    {
        m_tx = idx_property::EDMD_LENGTH_PROPERTY( aNode );
        return true;
    }
    else if( name == "pdm:ty" )
    {
        m_ty = idx_property::EDMD_LENGTH_PROPERTY( aNode );
        return true;
    }
    else if( name == "pdm:tz" )
    {
        m_tz = idx_property::EDMD_LENGTH_PROPERTY( aNode );
        return true;
    }
    else if ( name == "pdm:TransformationType" )
    {
        if( content == "d2" )
        {
            m_type = TYPE::D2;
            return true;
        }
        else if ( content == "d3" )
        {
            m_type = TYPE::D3;
            return true;
        }
        else
            return false;           
    }
    
    return this->EDMD_OBJECT::Parse( aNode );
}


// -----------------------------------------------------
// EDMD_SHAPE_ELEMENT
// -----------------------------------------------------

EDMD_SHAPE_ELEMENT::EDMD_SHAPE_ELEMENT( wxXmlNode& aNode, bool aTopLevel ) : idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{

    if ( !aTopLevel )
        return;
    
    wxXmlNode* child = aNode.GetChildren();

    while (child)
    {
        if ( !this->EDMD_SHAPE_ELEMENT::Parse( *child ) )
            this->unknownKeywordIDX( child->GetName() );

        child = child->GetNext();
    }   
};


bool EDMD_SHAPE_ELEMENT::Parse(wxXmlNode& aNode)
{
    const wxString name = aNode.GetName();
    const wxString content = aNode.GetNodeContent();

    if (name == "pdm:ShapeElementType")
    {
        if (content == "NonFeatureShapeElement")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::NON_FEATURE_SHAPE_ELEMENT;
        else if (content == "FeatureShapeElement")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::FEATURE_SHAPE_ELEMENT;
        else if (content == "PartFeature")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PART_FEATURE;
        else if (content == "PartMountingFeature")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PART_MOUNTING_FEATURE;
        else if (content == "ComponentTerminationPassageTemplateTerminal")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::COMPONENT_TERMINATION_PASSAGE_TEMPLATE_TERMINAL;
        else if (content == "ComponentTerminationPassageTemplateInterfaceTerminal")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::COMPONENT_TERMINATION_PASSAGE_TEMPLATE_INTERFACE_TERMINAL;
        else if (content == "ComponentTerminationPassageTemplateJoinTerminal")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::COMPONENT_TERMINATION_PASSAGE_TEMPLATE_JOIN_TERMINAL;
        else if (content == "PrintedPartCrossSectionTemplateTerminal")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PRINTED_PART_CROSS_SECTION_TEMPLATE_TERMINAL;
        else if (content == "PrintedPartTemplateInterfaceTerminal")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PRINTED_PART_TEMPLATE_INTERFACE_TERMINAL;
        else if (content == "PrintedPartTemplateJoinTerminal")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PRINTED_PART_TEMPLATE_JOIN_TERMINAL;
        else if (content == "StructuredPrintedPartTemplateTerminal")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::STRUCTURED_PRINTED_PART_TEMPLATE_TERMINAL;
        else if (content == "PrintedPartTemplateTerminal")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PRINTED_PART_TEMPLATE_TERMINAL;
        else if (content == "PartToolingFeature")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PART_TOOLING_FEATURE;
        else if (content == "FiducialPartFeature")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::FIDUCIAL_PART_FEATURE;
        else if (content == "ThermalFeature")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::THERMAL_FEATURE;
        else if (content == "TestPointPartFeature")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::TEST_POINT_PART_FEATURE;
        else if (content == "ConnectionZone")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::CONNECTION_ZONE;
        else if (content == "ComponentTerminal")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::COMPONENT_TERMINAL;
        else if (content == "BareDieComponentTerminal")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::BARE_DIE_COMPONENT_TERMINAL;
        else if (content == "Land")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::LAND;
        else if (content == "Package")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PACKAGE;
        else if (content == "PackageTerminal")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::PACKAGE_TERMINAL;
        else if (content == "ViaTerminal")
            m_shapeElementType = SHAPE_ELEMENT_TYPE::VIA_TERMINAL;
        else
            return false;

        return true;
    }
    else if (name == "pdm:Inverted")
    {
        m_inverted = (content == "true" || content == "1");
        return true;
    }
    else if (name == "pdm:DefiningShape")
    {
        m_definingShapeRef = content;
        // ✅ m_definingShapeRef modified
        return true;
    }

    // ✅ Fallback to base class parser if no member modified
    return this->EDMD_COLLABORATION_OBJECT::Parse(aNode);
}



bool EDMD_SHAPE_ELEMENT::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = idx_foundation::EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );

    result &=
            resolve<EDMD_SHAPE_DESCRIPTION>( m_definingShapeRef, &m_definingShape, aRegistry );

    return result;
};


// -----------------------------------------------------
// EDMD_ITEM_SHAPE
// -----------------------------------------------------

EDMD_ITEM_SHAPE::EDMD_ITEM_SHAPE( wxXmlNode& aNode, bool aTopLevel ) : idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{

    if ( !aTopLevel )
        return;
    
    wxXmlNode* child = aNode.GetChildren();

    while (child)
    {
        if ( !this->EDMD_ITEM_SHAPE::Parse( *child ) )
            this->unknownKeywordIDX( child->GetName() );

        child = child->GetNext();
    }   
};


bool EDMD_ITEM_SHAPE::Parse( wxXmlNode& aNode )
{
    const wxString name = aNode.GetName();
    const wxString content = aNode.GetNodeContent();

    if( name == "pdm:ShapeElement" )
    {
        m_shapeElementRef.push_back( content );
        return true;
    }

    return this->EDMD_OBJECT::Parse( aNode );
}


void EDMD_ITEM_SHAPE::print( int aIndex, int aMaxIndex )
{
    if ( aIndex >= aMaxIndex )
        return;
        
    for ( int i = 0; i < aIndex ; i++ )
        std::cout << "\t";
    
    std::cout << "--> ";

    std:: cout << m_name << " ( " << wxString::FromUTF8( typeid( *this ).name()  ) << " )" << std::endl;

    for ( int i = 0; i < aIndex ; i++ )
        std::cout << "\t";        
    std::cout << "    shape elements: ";

    for ( auto shapeelement: m_shapeElement)
    {
        std::cout << shapeelement->m_name;
    }
    std::cout << std::endl;

}


bool EDMD_ITEM_SHAPE::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = true;
    result &= idx_foundation::EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );
    result &= resolve<idx_pdm::EDMD_SHAPE_ELEMENT>( m_shapeElementRef, m_shapeElement, aRegistry );
    for ( wxString ref : m_shapeElementRef )
        std::cout << "resolved " << ref << ": " << result << std::endl;
    return result;
}


// -----------------------------------------------------
// EDMD_ITEM_SHAPE
// -----------------------------------------------------


EDMD_FUNCTIONAL_ITEM_SHAPE::EDMD_FUNCTIONAL_ITEM_SHAPE( wxXmlNode& aNode ) :
            EDMD_ITEM_SHAPE( aNode, false )
{
    wxXmlNode* child = aNode.GetChildren();

    while( child )
    {
        const wxString content = child->GetNodeContent();
        const wxString name = child->GetName();

        if( name == "pdm:EDMDFunctionalItemShapeType" )
        {
            if( content == "RigidArea" )
                m_type = TYPE::RIGID_AREA;
            else if( content == "FlexibleArea" )
                m_type = TYPE::FLEXIBLE_AREA;
            else if( content == "Stiffener" )
                m_type = TYPE::STIFFENER;
            else if( content == "UserArea" )
                m_type = TYPE::USER_AREA;
            else if( content == "PlacementGroupArea" )
                m_type = TYPE::PLACEMENT_GROUP;
            else if( content == "MechanicalItem" )
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


EDMD_STRATUM_TECHNOLOGY::EDMD_STRATUM_TECHNOLOGY( wxXmlNode& aNode ) : idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
    wxXmlNode* child = aNode.GetChildren();

    while( child )
    {
        const wxString content = child->GetNodeContent();
        const wxString name = child->GetName();

        if( name == "pdm:StratumTechnologyType" )
        {
            if( content == "Design" )
                m_stratumTechnologyType = STRATUM_TECHNOLOGY_TYPE::DESIGN;
            else if( content == "Documentation" )
                m_stratumTechnologyType = STRATUM_TECHNOLOGY_TYPE::DOCUMENTATION;
        }
        else if( name == "pdm:LayerPurpose" )
        {
            if( content == "SolderMask" )
                m_layerPurpose = LAYER_PURPOSE::SOLDERMASK;
            else if( content == "SolderPaste" )
                m_layerPurpose = LAYER_PURPOSE::SOLDERPASTE;
            else if( content == "SilkScreen" )
                m_layerPurpose = LAYER_PURPOSE::SILKSCREEN;
            else if( content == "GenericLayer" )
                m_layerPurpose = LAYER_PURPOSE::GENERIC_LAYER;
            else if( content == "Glue" )
                m_layerPurpose = LAYER_PURPOSE::GLUE;
            else if( content == "GlueMask" )
                m_layerPurpose = LAYER_PURPOSE::GLUEMASK;
            else if( content == "PasteMask" )
                m_layerPurpose = LAYER_PURPOSE::PASTEMASK;
            else if( content == "OtherSignal" )
                m_layerPurpose = LAYER_PURPOSE::OTHER_SIGNAL;
            else if( content == "LandsOnly" )
                m_layerPurpose = LAYER_PURPOSE::LANDS_ONLY;
            else if( content == "PowerOrGround" )
                m_layerPurpose = LAYER_PURPOSE::POWER_OR_GROUND;
            else if( content == "EmbeddedPassiveCapacitorDielectric" )
                m_layerPurpose = LAYER_PURPOSE::EMBEDDED_PASSIVE_CAPACITOR_DIELECTRIC;
            else if( content == "Dielectric" )
                m_layerPurpose = LAYER_PURPOSE::DIELECTRIC;
            else if( content == "EmbeddedPassiveResistor" )
                m_layerPurpose = LAYER_PURPOSE::EMBEDDED_PASSIVE_RESISTOR;
        }
        else if( name == "pdm:LayerPositionType" )
        {
            if( content == "Primary" )
                m_layerPositionType = LAYER_POSITION_TYPE::PRIMARY;
            else if( content == "Secondary" )
                m_layerPositionType = LAYER_POSITION_TYPE::SECONDARY;
            else if( content == "All" )
                m_layerPositionType = LAYER_POSITION_TYPE::ALL;
            else if( content == "External" )
                m_layerPositionType = LAYER_POSITION_TYPE::EXTERNAL;
            else if( content == "Internal" )
                m_layerPositionType = LAYER_POSITION_TYPE::INTERNAL;
        }

        child = child->GetNext();
    }
}


// -----------------------------------------------------
// EDMD_STRATUM
// -----------------------------------------------------


EDMD_STRATUM::EDMD_STRATUM( wxXmlNode& aNode, bool aTopLevel ) : EDMD_ITEM_SHAPE( aNode, false )
{
    
    if ( !aTopLevel )
        return;
    
    wxXmlNode* child = aNode.GetChildren();

    while (child)
    {
        if ( !this->EDMD_ITEM_SHAPE::Parse( *child ) )
            this->unknownKeywordIDX( child->GetName() );

        child = child->GetNext();
    }   
    
}


bool EDMD_STRATUM::Parse( wxXmlNode& aNode )
{
    const wxString name = aNode.GetName();
    const wxString content = aNode.GetNodeContent();

    if( name == "pdm:StratumTechnology" )
    {
        m_stratumTechnologyRef.push_back( content );
        return true;
    }
    else if( name == "pdm:StratumType" )
    {
        return false;
    }
    else if( name == "pdm:StratumSurfaceDesignation" )
    {
        if( content == "AverageSurface" )
        {
            m_stratumSurfaceDesignation = STRATUM_SURFACE_DESIGNATION::AVERAGE_SURFACE;
            return true;
        }
        else if( content == "PrimarySurface" )
        {
            m_stratumSurfaceDesignation = STRATUM_SURFACE_DESIGNATION::PRIMARY_SURFACE;    
            return true;
        }
        else if( content == "SecondarySurface" )
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
    result &= resolve<EDMD_STRATUM_TECHNOLOGY>( m_stratumTechnologyRef, m_stratumTechnology,
                                                aRegistry );

    return result;
}

// -----------------------------------------------------
// EDMD_KEEP_CONSTRAINT
// -----------------------------------------------------


EDMD_KEEP_CONSTRAINT::EDMD_KEEP_CONSTRAINT( wxXmlNode& aNode, bool aTopLevel ) : EDMD_ITEM_SHAPE( aNode, false )
{
    
    if ( !aTopLevel )
        return;
    
    wxXmlNode* child = aNode.GetChildren();

    while (child)
    {
        if ( !this->EDMD_KEEP_CONSTRAINT::Parse( *child ) )
            this->unknownKeywordIDX( child->GetName() );

        child = child->GetNext();
    }   
    
}


bool EDMD_KEEP_CONSTRAINT::Parse( wxXmlNode& aNode )
{
    const wxString name = aNode.GetName();
    const wxString content = aNode.GetNodeContent();

    if( name == "pdm:Purpose" )
    {
        if( content == "Thermal" )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::THERMAL );
        else if( content == "GenericClearance" )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::GENERIC_CLEARANCE );
        else if( content == "Shock" )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::SHOCK );
        else if( content == "Vibration" )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::VIBRATION );
        else if( content == "ElectromagneticCompatibility" )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::EMC );
        else if( content == "ComponentPlacement" )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::COMPONENT_PLACEMENT );
        else if( content == "Route" )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::ROUTE );
        else if( content == "Via" )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::VIA );
        else if( content == "TestPoint" )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::TESTPOINT );
        else if( content == "ValueLimit" )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::VALUELIMIT );
        else if( content == "Plane" )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::PLANE );
        else if( content == "Other" )
            m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::OTHER );
        else
            return false;
        return true;
    }
    else if( name == "pdm:KeepConstraintCondition" )
    {
        m_keepConstraintCondition.emplace_back( aNode, true );
        return true;
    }

    return this->EDMD_ITEM_SHAPE::Parse( aNode );
}


// -----------------------------------------------------
// EDMD_KEEPOUT
// -----------------------------------------------------


EDMD_KEEPOUT::EDMD_KEEPOUT( wxXmlNode& aNode, bool aTopLevel ) : EDMD_KEEP_CONSTRAINT( aNode, false )
{
    if ( !aTopLevel )
        return;
    
    wxXmlNode* child = aNode.GetChildren();

    while (child)
    {
        if ( !this->EDMD_KEEP_CONSTRAINT::Parse( *child ) )
            this->unknownKeywordIDX( child->GetName() );

        child = child->GetNext();
    }   
}


// -----------------------------------------------------
// EDMD_KEEPIN
// -----------------------------------------------------


EDMD_KEEPIN::EDMD_KEEPIN( wxXmlNode& aNode, bool aTopLevel ) : EDMD_KEEP_CONSTRAINT( aNode, false )
{
    if ( !aTopLevel )
        return;
    
    wxXmlNode* child = aNode.GetChildren();

    while (child)
    {
        if ( !this->EDMD_KEEP_CONSTRAINT::Parse( *child ) )
            this->unknownKeywordIDX( child->GetName() );

        child = child->GetNext();
    }   
}


// -----------------------------------------------------
// EDMD_ITEM_INSTANCE
// -----------------------------------------------------


EDMD_ITEM_INSTANCE::EDMD_ITEM_INSTANCE( wxXmlNode& aNode, bool aTopLevel ) : EDMD_EXTENSIBLE_OBJECT( aNode, false )
{

    if ( !aTopLevel )
        return;
    
    wxXmlNode* child = aNode.GetChildren();

    while (child)
    {
        if ( !this->EDMD_ITEM_INSTANCE::Parse( *child ) )
            this->unknownKeywordIDX( child->GetName() );

        child = child->GetNext();
    }   
};


bool EDMD_ITEM_INSTANCE::Parse( wxXmlNode& aNode )
{
    const wxString name = aNode.GetName();
    const wxString content = aNode.GetNodeContent();

    if ( name == "pdm:InstanceName" )
    {
        m_instanceName = idx_foundation::EDMD_NAME( aNode, true );
        return true;
    }
    else if ( name == "pdm:Transformation" )
    {
        m_transformation = idx_pdm::EDMD_TRANSFORMATION( aNode, true );
        return true;
    }
    else if ( name == "pdm:Item" )
    {
        m_itemRef = content;
        return true;
    }
    else if ( name == "pdm:InstanceUserAreaLayerName" )
    {
        m_instanceUserAreaLayerName.emplace_back( aNode, true );
        return true;
    }
    else if ( name == "pdm:zOffset")
    {
        content.ToCDouble( &m_zOffset );
        return true;
    }
    else if ( name == "foundation:Name" )
    {
        m_name = content;
        return true;
    }

    return this->EDMD_EXTENSIBLE_OBJECT::Parse( aNode );
}


void EDMD_ITEM_INSTANCE::print( int aIndex, int aMaxIndex )
{
    if ( aIndex >= aMaxIndex )
        return;
        
    for ( int i = 0; i < aIndex ; i++ )
        std::cout << "\t";
    
    std::cout << "--> ";

    std:: cout << m_name << " ( " << wxString::FromUTF8( typeid( *this ).name()  ) << " )" << std::endl;

    for ( int i = 0; i < aIndex ; i++ )
        std::cout << "\t";        
    std::cout << "    item ref: " << m_itemRef << std::endl;
}

bool EDMD_ITEM_INSTANCE::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = idx_foundation::EDMD_EXTENSIBLE_OBJECT::Resolve( aRegistry );
    bool tmp;
    tmp &=
            resolve<idx_pdm::EDMD_ITEM>( m_itemRef, &m_item, aRegistry );


    return result && tmp;
}


// -----------------------------------------------------
// EDMD_ITEM
// -----------------------------------------------------

EDMD_ITEM::EDMD_ITEM( wxXmlNode& aNode, bool aTopLevel ) : EDMD_COLLABORATION_OBJECT( aNode, false )
{

    if( !aTopLevel )
        return;

    wxXmlNode* child = aNode.GetChildren();

    while (child)
    {
        if ( !this->EDMD_ITEM::Parse( *child ) )
            this->unknownKeywordIDX( child->GetName() );

        child = child->GetNext();
    }
}


bool EDMD_ITEM::Parse( wxXmlNode& aNode )
{
    const wxString name = aNode.GetName();
    const wxString content = aNode.GetNodeContent();

   if( name == "pdm:ItemType" )
        {
            if( content == "single" )
                m_itemType = ITEM_TYPE::SINGLE;
            else if( content == "assembly" )
                m_itemType = ITEM_TYPE::ASSEMBLY;
            else
                return false;
            return true;
        }
    else if( name == "pdm:Identifier" )
        {
            m_identifier.emplace_back( aNode, true);
            return true;
        }
    else if( name == "pdm:PackageName" )
        {
            m_packageName.emplace_back( aNode, true );
            return true;
        }
    else if( name == "pdm:ItemInstance" )
        {
            m_itemInstance.emplace_back( std::make_shared<idx_pdm::EDMD_ITEM_INSTANCE>( aNode, true ) );
            return true;
        }
    else if( name == "pdm:Shape" )
        {                       
            m_shapeRef = content;
            return true;
        }

    return this->EDMD_COLLABORATION_OBJECT::Parse( aNode );
}


void EDMD_ITEM::print( int aIndex, int aMaxIndex )
{
    if ( aIndex >= aMaxIndex )
        return;

    for ( int i = 0; i < aIndex ; i++ )
        std::cout << "\t";
    
    std::cout << "--> ";

    std:: cout << m_id << " ( " << wxString::FromUTF8( typeid( *this ).name()  ) << " )" << std::endl;

    for ( auto inst: m_itemInstance )
    {
        inst->print( aIndex+1 );
    }
    if ( m_shape )
    {
        m_shape->print( aIndex+1 );
    }
}

bool EDMD_ITEM::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = idx_foundation::EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );
    bool result1, result2;

    result1 = resolve<EDMD_ITEM_SHAPE>( m_shapeRef, &m_shape, aRegistry, true );
    result2 = resolve<EDMD_FUNCTIONAL_ITEM_SHAPE>( m_shapeRef, &m_shape, aRegistry, true );


    if( !result1 && !result2 )
        result = false;
    for ( auto& instance : m_itemInstance )
        instance->Resolve( aRegistry );

    if (m_shape)
        result &= m_shape->Resolve( aRegistry );
    else
        std::cout << "no m_shape for " << m_name << std::endl;
    return result;
}



// -----------------------------------------------------
// EDMD_ASSEMBLY_COMPONENT
// -----------------------------------------------------

EDMD_ASSEMBLY_COMPONENT::EDMD_ASSEMBLY_COMPONENT( wxXmlNode& aNode, bool aTopLevel ) : EDMD_ITEM_SHAPE( aNode, false )
{
    if ( !aTopLevel )
        return;
    
    wxXmlNode* child = aNode.GetChildren();

    while (child)
    {
        if ( !this->EDMD_ASSEMBLY_COMPONENT::Parse( *child ) )
            this->unknownKeywordIDX( child->GetName() );

        child = child->GetNext();
    }   
};


bool EDMD_ASSEMBLY_COMPONENT::Parse( wxXmlNode& aNode )
{
    const wxString name = aNode.GetName();
    const wxString content = aNode.GetNodeContent();


    if( name == "pdm:AssemblyComponentType" )
    {
        if( content == "AssemblyGroupComponent" )
            m_assemblyComponentType = ASSEMBLY_COMPONENT_TYPE::ASSEMBLY_GROUP_COMPONENT;
        else if( content == "LaminateComponent" )
            m_assemblyComponentType = ASSEMBLY_COMPONENT_TYPE::LAMINATE_COMPONENT;
        else if( content == "ThermalComponent" )
            m_assemblyComponentType = ASSEMBLY_COMPONENT_TYPE::THERMAL_COMPONENT;
        else if( content == "Physical" )
            m_assemblyComponentType = ASSEMBLY_COMPONENT_TYPE::PHYSICAL;
        else if( content == "Padstack" )
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

EDMD_INTERSTRATUM_FEATURE::EDMD_INTERSTRATUM_FEATURE( wxXmlNode& aNode, bool aTopLevel ) : EDMD_ITEM_SHAPE( aNode, false )
{

    if( !aTopLevel )
        return;

    wxXmlNode* child = aNode.GetChildren();

    while (child)
    {
        if ( !this->EDMD_INTERSTRATUM_FEATURE::Parse( *child ) )
            this->unknownKeywordIDX( child->GetName() );

        child = child->GetNext();
    }
}

bool EDMD_INTERSTRATUM_FEATURE::Parse(wxXmlNode& aNode)
{
    const wxString content = aNode.GetNodeContent();
    const wxString name = aNode.GetName();

    if (name == "pdm:InterStratumFeatureType")
    {
        if (content == "UnsupportedPassage")
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::UNSUPPORTED_PASSAGE;
        else if (content == "StratumFeatureTemplateComponent")
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::STRATUM_FEATURE_TEMPLATE_COMPONENT;
        else if (content == "CutoutEdgeSegment")
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::CUTOUT_EDGE_SEGMENT;
        else if (content == "PlatedCutoutEdgeSegment")
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::PLATED_CUTOUT_EDGE_SEGMENT;
        else if (content == "Cutout")
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::CUTOUT;
        else if (content == "PlatedInterStratumFeature")
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::PLATED_INTERSTRATUM_FEATURE;
        else if (content == "PlatedPassage")
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::PLATED_PASSAGE;
        else if (content == "MilledCutout")
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::MILLED_CUTOUT;
        else if (content == "PlatedCutout")
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::PLATED_CUTOUT;
        else if (content == "Via")
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::VIA;
        else if (content == "PhysicalConnectivityInterruptingCutout")
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::PHYSICAL_CONNECTIVITY_INTERRUPTING_CUTOUT;
        else if (content == "ComponentTerminationPassage")
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::COMPONENT_TERMINATION_PASSAGE;
        else if (content == "FilledVia")
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::FILLED_VIA;
        else if (content == "MechanicalComponent")
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::MECHANICAL_COMPONENT;
        else if (content == "CustomElectroMechanicalComponent")
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::CUSTOM_ELECTROMECHANICAL_COMPONENT;
        else if (content == "BoardCutout")
            m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::BOARD_CUTOUT;
        else
            return false;

        return true;
    }
    else if (name == "pdm:Stratum")
    {
        m_stratumRef.push_back(content);
        return true;
    }

    return this->EDMD_ITEM_SHAPE::Parse(aNode);
    
}


bool EDMD_INTERSTRATUM_FEATURE::Resolve( IDX_REGISTRY& aRegistry )
{        
    bool result = idx_pdm::EDMD_ITEM_SHAPE::Resolve( aRegistry );

    result &=
            resolve<EDMD_STRATUM>( m_stratumRef, m_stratum  , aRegistry );

    return result;
}


} // namespace idx_pdm