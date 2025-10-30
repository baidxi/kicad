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

#ifndef IDX_PDM_H
#define IDX_PDM_H

namespace idx_pdm
{
class EDMD_3DMODEL;
class EDMD_ASSEMBLY_COMPONENT;
class EDMD_BEND;
class EDMD_BEND_TYPE;
class EDMD_CIRCULAR_BEND_TYPE;
class EDMD_FUNCTIONAL_ITEM_SHAPE;
class EDMD_INTERSTRATUM_FEATURE;
class EDMD_ITEM;
class EDMD_ITEM_INSTANCE;
class EDMD_ITEM_SHAPE;
class EDMD_KEEP_CONSTRAINT;
class EDMD_KEEPIN;
class EDMD_KEEPOUT;
class EDMD_NON_FEATURE_ITEM_SHAPE;
class EDMD_PACKAGE_PIN;
class EDMD_SHAPE_DESCRIPTION;
class EDMD_SHAPE_ELEMENT;
class EDMD_STRATUM;
class EDMD_STRATUM_TECHNOLOGY;
class EDMD_TRANSFORMATION;
} // namespace idx_pdm

#include <idx/idx_base.h>
#include <idx/property.h>
#include <idx/foundation.h>

namespace idx_pdm
{

enum class SHAPE_DESCRIPTION_TYPE
{
    GEOMETRIC_MODEL = 0,
    EXTERNAL_GEOMETRIC_MODEL
};

enum class SHAPE_ELEMENT_TYPE
{
    NON_FEATURE_SHAPE_ELEMENT = 0,
    FEATURE_SHAPE_ELEMENT,
    PART_FEATURE,
    PART_MOUNTING_FEATURE,
    COMPONENT_TERMINATION_PASSAGE_TEMPLATE_TERMINAL,
    COMPONENT_TERMINATION_PASSAGE_TEMPLATE_INTERFACE_TERMINAL,
    COMPONENT_TERMINATION_PASSAGE_TEMPLATE_JOIN_TERMINAL,
    PRINTED_PART_CROSS_SECTION_TEMPLATE_TERMINAL,
    PRINTED_PART_TEMPLATE_INTERFACE_TERMINAL,
    PRINTED_PART_TEMPLATE_JOIN_TERMINAL,
    STRUCTURED_PRINTED_PART_TEMPLATE_TERMINAL,
    PRINTED_PART_TEMPLATE_TERMINAL,
    PART_TOOLING_FEATURE,
    FIDUCIAL_PART_FEATURE,
    THERMAL_FEATURE,
    TEST_POINT_PART_FEATURE,
    CONNECTION_ZONE,
    COMPONENT_TERMINAL,
    BARE_DIE_COMPONENT_TERMINAL,
    LAND,
    PACKAGE,
    PACKAGE_TERMINAL,
    VIA_TERMINAL
};

class EDMD_SHAPE_DESCRIPTION : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_SHAPE_DESCRIPTION() : EDMD_COLLABORATION_OBJECT() {};
    EDMD_SHAPE_DESCRIPTION( wxXmlNode& aNode ) : EDMD_COLLABORATION_OBJECT( aNode )
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
    };

    SHAPE_DESCRIPTION_TYPE m_shapeDescriptionType = SHAPE_DESCRIPTION_TYPE::GEOMETRIC_MODEL;
    std::shared_ptr<EDMD_SHAPE_DESCRIPTION> m_boundingBox;

private:
    wxString m_boudingBoxRef;
};

class EDMD_TRANSFORMATION : public idx_foundation::EDMD_OBJECT
{
public:

    enum class TYPE
    {
        D2=2,
        D3=3
    };

    EDMD_TRANSFORMATION() : idx_foundation::EDMD_OBJECT() {};
    EDMD_TRANSFORMATION( wxXmlNode& aNode ) : idx_foundation::EDMD_OBJECT( aNode ) {

         wxXmlNode* child = aNode.GetChildren();

        while( child )
        {
            const wxString content = child->GetNodeContent();
            const wxString name = child->GetName();

            if( name == "pdm:xx" )
                content.ToCDouble( &m_xx );
            else if( name == "pdm:xy" )
                content.ToCDouble( &m_xy );
            else if( name == "pdm:xz" )
                content.ToCDouble( &m_xz );
            else if( name == "pdm:yx" )
                content.ToCDouble( &m_yx );
            else if( name == "pdm:yy" )
                content.ToCDouble( &m_yy );
            else if( name == "pdm:yz" )
                content.ToCDouble( &m_yz );
            else if( name == "pdm:zx" )
                content.ToCDouble( &m_zx );
            else if( name == "pdm:zy" )
                content.ToCDouble( &m_zy );
            else if( name == "pdm:zz" )
                content.ToCDouble( &m_zz );
            else if( name == "pdm:tx" )
                m_tx = idx_property::EDMD_LENGTH_PROPERTY( *child );
            else if( name == "pdm:ty" )
                m_ty = idx_property::EDMD_LENGTH_PROPERTY( *child );
            else if( name == "pdm:tz" )
                m_tz = idx_property::EDMD_LENGTH_PROPERTY( *child );
            else if ( name == "pdm:TransformationType" )
            {
                if( content == "d2" )
                    m_type = TYPE::D2;
                else if ( content == "d3" )
                    m_type = TYPE::D3;
                else
                    std::cout << "unknown transformation type : " << content << std::endl;
                
            }

            child = child->GetNext();
        }

    };

    double m_xx = 1;
    double m_xy = 0;
    double m_xz = 0;
    double m_yx = 0;
    double m_yy = 1;
    double m_yz = 0;
    double m_zx = 0;
    double m_zy = 0;
    double m_zz = 1;
    idx_property::EDMD_LENGTH_PROPERTY m_tx;
    idx_property::EDMD_LENGTH_PROPERTY m_ty;
    idx_property::EDMD_LENGTH_PROPERTY m_tz;
    TYPE m_type = TYPE::D2;
};


class EDMD_SHAPE_ELEMENT : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_SHAPE_ELEMENT() : idx_foundation::EDMD_COLLABORATION_OBJECT() {};
    EDMD_SHAPE_ELEMENT( wxXmlNode& aNode ) : idx_foundation::EDMD_COLLABORATION_OBJECT( aNode )
    {
        wxXmlNode* child = aNode.GetChildren();

        while( child )
        {
            const wxString content = child->GetNodeContent();
            const wxString name = child->GetName();

            if( name == "pdm:ShapeElementType" )
            {
                if( content == "NonFeatureShapeElement" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::NON_FEATURE_SHAPE_ELEMENT;
                else if( content == "FeatureShapeElement" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::FEATURE_SHAPE_ELEMENT;
                else if( content == "PartFeature" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::PART_FEATURE;
                else if( content == "PartMountingFeature" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::PART_MOUNTING_FEATURE;
                else if( content == "ComponentTerminationPassageTemplateTerminal" )
                    m_shapeElementType =
                            SHAPE_ELEMENT_TYPE::COMPONENT_TERMINATION_PASSAGE_TEMPLATE_TERMINAL;
                else if( content == "ComponentTerminationPassageTemplateInterfaceTerminal" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::
                            COMPONENT_TERMINATION_PASSAGE_TEMPLATE_INTERFACE_TERMINAL;
                else if( content == "ComponentTerminationPassageTemplateJoinTerminal" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::
                            COMPONENT_TERMINATION_PASSAGE_TEMPLATE_JOIN_TERMINAL;
                else if( content == "PrintedPartCrossSectionTemplateTerminal" )
                    m_shapeElementType =
                            SHAPE_ELEMENT_TYPE::PRINTED_PART_CROSS_SECTION_TEMPLATE_TERMINAL;
                else if( content == "PrintedPartTemplateInterfaceTerminal" )
                    m_shapeElementType =
                            SHAPE_ELEMENT_TYPE::PRINTED_PART_TEMPLATE_INTERFACE_TERMINAL;
                else if( content == "PrintedPartTemplateJoinTerminal" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::PRINTED_PART_TEMPLATE_JOIN_TERMINAL;
                else if( content == "StructuredPrintedPartTemplateTerminal" )
                    m_shapeElementType =
                            SHAPE_ELEMENT_TYPE::STRUCTURED_PRINTED_PART_TEMPLATE_TERMINAL;
                else if( content == "PrintedPartTemplateTerminal" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::PRINTED_PART_TEMPLATE_TERMINAL;
                else if( content == "PartToolingFeature" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::PART_TOOLING_FEATURE;
                else if( content == "FiducialPartFeature" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::FIDUCIAL_PART_FEATURE;
                else if( content == "ThermalFeature" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::THERMAL_FEATURE;
                else if( content == "TestPointPartFeature" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::TEST_POINT_PART_FEATURE;
                else if( content == "ConnectionZone" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::CONNECTION_ZONE;
                else if( content == "ComponentTerminal" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::COMPONENT_TERMINAL;
                else if( content == "BareDieComponentTerminal" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::BARE_DIE_COMPONENT_TERMINAL;
                else if( content == "Land" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::LAND;
                else if( content == "Package" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::PACKAGE;
                else if( content == "PackageTerminal" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::PACKAGE_TERMINAL;
                else if( content == "ViaTerminal" )
                    m_shapeElementType = SHAPE_ELEMENT_TYPE::VIA_TERMINAL;
            }
            else if( name == "pdm::Inverted" )
            {
                m_inverted == ( content == "true" ) ? true : false;
            }
            else if( name == "pdm:DefiningShape" )
            {
                m_definingShapeRef = content;
            }
            child = child->GetNext();
        }
    };


    bool Resolve( IDX_REGISTRY& aRegistry )
    {
        bool result = idx_foundation::EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );

        result &=
                resolve<EDMD_SHAPE_DESCRIPTION>( m_definingShapeRef, &m_definingShape, aRegistry );

        return result;
    };

    bool                                    m_inverted = false;
    SHAPE_ELEMENT_TYPE                      m_shapeElementType;
    EDMD_TRANSFORMATION                     m_shapeDescriptionTransformation;
    std::shared_ptr<EDMD_SHAPE_DESCRIPTION> m_definingShape;

private:
    wxString m_definingShapeRef;
};


class EDMD_ITEM_SHAPE : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_ITEM_SHAPE() : idx_foundation::EDMD_COLLABORATION_OBJECT() {};
    EDMD_ITEM_SHAPE( wxXmlNode& aNode ) : idx_foundation::EDMD_COLLABORATION_OBJECT( aNode )
    {
        wxXmlNode* child = aNode.GetChildren();

        while( child )
        {
            const wxString content = child->GetNodeContent();
            const wxString name = child->GetName();

            if( name == "pdm:ShapeElement" )
            {
                std::cout << "SHAPE ELEMENT " << content << std::endl;
                m_shapeElementRef.push_back( content );
            }
            child = child->GetNext();
        }
    };


    void print( int aIndex=0, int aMaxIndex=5 )
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


    bool Resolve( IDX_REGISTRY& aRegistry )
    {
        bool result = true;
        result &= idx_foundation::EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );
        result &= resolve<idx_pdm::EDMD_SHAPE_ELEMENT>( m_shapeElementRef, m_shapeElement, aRegistry );
        for ( wxString ref : m_shapeElementRef )
            std::cout << "resolved " << ref << ": " << result << std::endl;
        return result;
    };

    std::vector<std::shared_ptr<EDMD_SHAPE_ELEMENT>> m_shapeElement;

    std::vector<wxString> m_shapeElementRef;
private:
};


class EDMD_FUNCTIONAL_ITEM_SHAPE : public EDMD_ITEM_SHAPE
{
public:
    enum class TYPE
    {
        RIGID_AREA = 0,
        FLEXIBLE_AREA,
        STIFFENER,
        USER_AREA,
        PLACEMENT_GROUP, // NO LONGER USED BY IDX
        MECHANICAL,      // NO LONGER USED BY IDX
    };

    EDMD_FUNCTIONAL_ITEM_SHAPE() :
            EDMD_ITEM_SHAPE() {};
    EDMD_FUNCTIONAL_ITEM_SHAPE( wxXmlNode& aNode ) :
            EDMD_ITEM_SHAPE( aNode )
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
    };


    bool Resolve( IDX_REGISTRY& aRegistry )
    {
        bool result = EDMD_ITEM_SHAPE::Resolve( aRegistry );
        return result;
    };


    TYPE m_type;

private:
};


enum class STRATUM_TECHNOLOGY_TYPE
{
    DESIGN = 0,
    DOCUMENTATION
};

enum class LAYER_PURPOSE
{
    SOLDERMASK = 0,
    SOLDERPASTE,
    SILKSCREEN,
    GENERIC_LAYER,
    GLUE,
    GLUEMASK,
    PASTEMASK,
    OTHER_SIGNAL,
    LANDS_ONLY,
    POWER_OR_GROUND,
    EMBEDDED_PASSIVE_CAPACITOR_DIELECTRIC,
    DIELECTRIC,
    EMBEDDED_PASSIVE_RESISTOR
};


enum class LAYER_POSITION_TYPE
{
    PRIMARY = 0,
    SECONDARY,
    ALL,
    EXTERNAL,
    INTERNAL
};

class EDMD_STRATUM_TECHNOLOGY : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_STRATUM_TECHNOLOGY() : idx_foundation::EDMD_COLLABORATION_OBJECT() {};
    EDMD_STRATUM_TECHNOLOGY( wxXmlNode& aNode ) : idx_foundation::EDMD_COLLABORATION_OBJECT( aNode )
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
    };

    LAYER_POSITION_TYPE     m_layerPositionType = LAYER_POSITION_TYPE::PRIMARY;
    STRATUM_TECHNOLOGY_TYPE m_stratumTechnologyType = STRATUM_TECHNOLOGY_TYPE::DESIGN;
    LAYER_PURPOSE           m_layerPurpose = LAYER_PURPOSE::SILKSCREEN;

private:
};


enum class STRATUM_SURFACE_DESIGNATION
{
    AVERAGE_SURFACE = 0,
    PRIMARY_SURFACE,
    SECONDARY_SURFACE
};


class EDMD_STRATUM : public EDMD_ITEM_SHAPE
{
public:
    EDMD_STRATUM() : EDMD_ITEM_SHAPE() {};
    EDMD_STRATUM( wxXmlNode& aNode ) : EDMD_ITEM_SHAPE( aNode )
    {
        wxXmlNode* child = aNode.GetChildren();
        while( child )
        {
            const wxString content = child->GetNodeContent();
            const wxString name = child->GetName();

            if( name == "pdm:StratumTechnology" )
            {
                m_stratumTechnologyRef.push_back( content );
            }
            else if( name == "pdm:StratumType" )
            {
            }
            else if( name == "pdm:StratumSurfaceDesignation" )
            {
                if( content == "AverageSurface" )
                    m_stratumSurfaceDesignation = STRATUM_SURFACE_DESIGNATION::AVERAGE_SURFACE;
                if( content == "PrimarySurface" )
                    m_stratumSurfaceDesignation = STRATUM_SURFACE_DESIGNATION::PRIMARY_SURFACE;
                if( content == "SecondarySurface" )
                    m_stratumSurfaceDesignation = STRATUM_SURFACE_DESIGNATION::SECONDARY_SURFACE;
            }
            child = child->GetNext();
        }
    };


    bool Resolve( IDX_REGISTRY& aRegistry )
    {
        bool result = EDMD_ITEM_SHAPE::Resolve( aRegistry );
        result &= resolve<EDMD_STRATUM_TECHNOLOGY>( m_stratumTechnologyRef, m_stratumTechnology,
                                                    aRegistry );

        return result;
    };

    std::vector<std::shared_ptr<EDMD_STRATUM_TECHNOLOGY>> m_stratumTechnology;
    STRATUM_SURFACE_DESIGNATION                           m_stratumSurfaceDesignation;

private:
    std::vector<wxString> m_stratumTechnologyRef;
};

enum class KEEP_CONSTRAINT_PURPOSE
{
    THERMAL,
    GENERIC_CLEARANCE,
    SHOCK,
    VIBRATION,
    EMC,
    COMPONENT_PLACEMENT,
    ROUTE,
    VIA,
    TESTPOINT,
    VALUELIMIT,
    PLANE,
    OTHER
};

class EDMD_KEEP_CONSTRAINT : public EDMD_ITEM_SHAPE
{
public:
    EDMD_KEEP_CONSTRAINT() : EDMD_ITEM_SHAPE() {};
    EDMD_KEEP_CONSTRAINT( wxXmlNode& aNode ) : EDMD_ITEM_SHAPE( aNode )
    {
        wxXmlNode* child = aNode.GetChildren();

        while( child )
        {
            const wxString content = child->GetNodeContent();
            const wxString name = child->GetName();

            if( name == "pdm:Purpose" )
            {
                if( content == "Thermal" )
                    m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::THERMAL );
                if( content == "GenericClearance" )
                    m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::GENERIC_CLEARANCE );
                if( content == "Shock" )
                    m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::SHOCK );
                if( content == "Vibration" )
                    m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::VIBRATION );
                if( content == "ElectromagneticCompatibility" )
                    m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::EMC );
                if( content == "ComponentPlacement" )
                    m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::COMPONENT_PLACEMENT );
                if( content == "Route" )
                    m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::ROUTE );
                if( content == "Via" )
                    m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::VIA );
                if( content == "TestPoint" )
                    m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::TESTPOINT );
                if( content == "ValueLimit" )
                    m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::VALUELIMIT );
                if( content == "Plane" )
                    m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::PLANE );
                if( content == "Other" )
                    m_purpose.push_back( KEEP_CONSTRAINT_PURPOSE::OTHER );
            }
            else if( name == "pdm:KeepConstraintCondition" )
            {
                m_keepConstraintCondition.emplace_back( *child );
            }
            child = child->GetNext();
        }
    };


    std::vector<KEEP_CONSTRAINT_PURPOSE>                 m_purpose;
    std::vector<idx_property::EDMD_CONSTRAINED_PROPERTY> m_keepConstraintCondition;

private:
};


class EDMD_ITEM_INSTANCE : public idx_foundation::EDMD_EXTENSIBLE_OBJECT
{
public:
    EDMD_ITEM_INSTANCE() : EDMD_EXTENSIBLE_OBJECT() {};
    EDMD_ITEM_INSTANCE( wxXmlNode& aNode ) : EDMD_EXTENSIBLE_OBJECT( aNode )
    {
        wxXmlNode* child = aNode.GetChildren();

        while( child )
        {
            const wxString content = child->GetNodeContent();
            const wxString name = child->GetName();

            if ( name == "pdm:InstanceName" )
                m_instanceName = idx_foundation::EDMD_NAME( *child );
            else if ( name == "pdm:Transformation" )
                m_transformation = idx_pdm::EDMD_TRANSFORMATION( *child );
            else if ( name == "pdm:Item" )
                m_itemRef = content;
            else if ( name == "pdm:InstanceUserAreaLayerName" )
                m_instanceUserAreaLayerName.emplace_back( *child );
            else if ( name == "pdm:zOffset")
                content.ToCDouble( &m_zOffset );
            else if ( name == "foundation:Name" )
            {
                m_name = content;
            }
            else
            {
            }
            child = child->GetNext();
        }

    };


    void print( int aIndex=0, int aMaxIndex=5 )
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

    bool Resolve( IDX_REGISTRY& aRegistry )
    {
        bool result = idx_foundation::EDMD_EXTENSIBLE_OBJECT::Resolve( aRegistry );
        bool tmp;
        tmp &=
                resolve<idx_pdm::EDMD_ITEM>( m_itemRef, &m_item, aRegistry );


        return result && tmp;
    };

    idx_foundation::EDMD_NAME m_instanceName;
    idx_pdm::EDMD_TRANSFORMATION m_transformation;
    std::shared_ptr<idx_pdm::EDMD_ITEM> m_item;
    std::vector<idx_foundation::EDMD_NAME> m_instanceUserAreaLayerName;
    wxString m_name = "XXX";
    double m_zOffset = 0;

private:
    wxString m_itemRef;
};


enum class ITEM_TYPE
{
    SINGLE=0,
    ASSEMBLY
};


class EDMD_ITEM : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_ITEM() : EDMD_COLLABORATION_OBJECT() {};
    EDMD_ITEM( wxXmlNode& aNode ) : EDMD_COLLABORATION_OBJECT( aNode )
    {
        wxXmlNode* child = aNode.GetChildren();

        while( child )
        {
            const wxString content = child->GetNodeContent();
            const wxString name = child->GetName();

            if( name == "pdm:ItemType" )
            {
                if( content == "Single" )
                    m_itemType = ITEM_TYPE::SINGLE;
                if( content == "Assembly" )
                    m_itemType = ITEM_TYPE::ASSEMBLY;
            }
        else if( name == "pdm:Identifier" )
            {
                m_identifier.emplace_back( *child );
            }
        else if( name == "pdm:PackageName" )
            {
                m_packageName.emplace_back( *child );
            }
        else if( name == "pdm:ItemInstance" )
            {
                m_itemInstance.emplace_back( std::make_shared<idx_pdm::EDMD_ITEM_INSTANCE>( *child ) );
            }
        else if( name == "pdm:Shape" )
            {                       
                m_shapeRef = content;
            }
            child = child->GetNext();
        }
    };

    void print( int aIndex=0, int aMaxIndex=5 )
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

bool Resolve( IDX_REGISTRY& aRegistry )
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
    };

    ITEM_TYPE m_itemType = ITEM_TYPE::SINGLE;
    std::vector<idx_foundation::EDMD_IDENTIFIER> m_identifier;
    std::vector<idx_foundation::EDMD_NAME> m_packageName;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> m_itemInstance;
    std::shared_ptr<idx_pdm::EDMD_ITEM_SHAPE> m_shape;

private:
    wxString m_shapeRef;
};

enum class ASSEMBLY_COMPONENT_TYPE
{
    ASSEMBLY_GROUP_COMPONENT = 0,
    LAMINATE_COMPONENT,
    THERMAL_COMPONENT,
    PHYSICAL,
    PADSTACK
};

class EDMD_ASSEMBLY_COMPONENT: public idx_pdm::EDMD_ITEM_SHAPE
{
public:
    EDMD_ASSEMBLY_COMPONENT() : EDMD_ITEM_SHAPE() {};
    EDMD_ASSEMBLY_COMPONENT( wxXmlNode& aNode ) : EDMD_ITEM_SHAPE( aNode )
    {
        wxXmlNode* child = aNode.GetChildren();

        while( child )
        {
            const wxString content = child->GetNodeContent();
            const wxString name = child->GetName();

            if( name == "pdm:AssemblyComponentType" )
            {
                if( content == "AssemblyGroupComponent" )
                    m_assemblyComponentType = ASSEMBLY_COMPONENT_TYPE::ASSEMBLY_GROUP_COMPONENT;
                if( content == "LaminateComponent" )
                    m_assemblyComponentType = ASSEMBLY_COMPONENT_TYPE::LAMINATE_COMPONENT;
                if( content == "ThermalComponent" )
                    m_assemblyComponentType = ASSEMBLY_COMPONENT_TYPE::THERMAL_COMPONENT;
                if( content == "Physical" )
                    m_assemblyComponentType = ASSEMBLY_COMPONENT_TYPE::PHYSICAL;
                if( content == "Padstack" )
                    m_assemblyComponentType = ASSEMBLY_COMPONENT_TYPE::PADSTACK;
            }
            child = child->GetNext();
        }
    };

bool Resolve( IDX_REGISTRY& aRegistry )
    {
        bool result = idx_foundation::EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );

        //result &=
        //          resolve<EDMD_SHAPE_DESCRIPTION>( m_shapeRef, &m_shape, aRegistry );

        return result;
    };


    ASSEMBLY_COMPONENT_TYPE m_assemblyComponentType;
private:

};

enum class INTERSTRATUM_FEATURE_TYPE
{
    UNSUPPORTED_PASSAGE=0,
    STRATUM_FEATURE_TEMPLATE_COMPONENT,
    CUTOUT_EDGE_SEGMENT,
    PLATED_CUTOUT_EDGE_SEGMENT,
    CUTOUT,
    PLATED_INTERSTRATUM_FEATURE,
    PLATED_PASSAGE,
    MILLED_CUTOUT,
    PLATED_CUTOUT,
    VIA,
    PHYSICAL_CONNECTIVITY_INTERRUPTING_CUTOUT,
    PARTIALLY_PLATED_CUTOUT,
    COMPONENT_TERMINATION_PASSAGE,
    FILLED_VIA,
    MECHANICAL_COMPONENT,
    CUSTOM_ELECTROMECHANICAL_COMPONENT,
    BOARD_CUTOUT
};

class EDMD_INTERSTRATUM_FEATURE : public idx_pdm::EDMD_ITEM_SHAPE
{
public:
    EDMD_INTERSTRATUM_FEATURE() : EDMD_ITEM_SHAPE() {};
    EDMD_INTERSTRATUM_FEATURE( wxXmlNode& aNode ) : EDMD_ITEM_SHAPE( aNode )
    {
        wxXmlNode* child = aNode.GetChildren();

        while( child )
        {
            const wxString content = child->GetNodeContent();
            const wxString name = child->GetName();

            if( name == "pdm:InterStratumFeatureType" )
            {
                if( content == "UnsupportedPassage" )
                    m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::UNSUPPORTED_PASSAGE  ;
                else if( content == "StratumFeatureTemplateComponent" )
                    m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::STRATUM_FEATURE_TEMPLATE_COMPONENT;
                else if( content == "CutoutEdgeSegment" )
                    m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::CUTOUT_EDGE_SEGMENT;
                else if( content == "PlatedCutoutEdgeSegment" )
                    m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::PLATED_CUTOUT_EDGE_SEGMENT;
                else if( content == "Cutout" )
                    m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::CUTOUT;
                else if( content == "PlatedInterStratumFeature" )
                    m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::PLATED_INTERSTRATUM_FEATURE;
                else if( content == "PlatedPassage" )
                    m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::PLATED_PASSAGE;
                else if( content == "MilledCutout" )
                    m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::MILLED_CUTOUT;
                else if( content == "PlatedCutout" )
                    m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::PLATED_CUTOUT;
                else if( content == "Via" )
                    m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::VIA;
                else if( content == "PhysicalConnectivityInterruptingCutout" )
                    m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::PHYSICAL_CONNECTIVITY_INTERRUPTING_CUTOUT;
                else if( content == "ComponentTerminationPassage" )
                    m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::COMPONENT_TERMINATION_PASSAGE;
                else if( content == "FilledVia" )
                    m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::FILLED_VIA;
                else if( content == "MechanicalComponent" )
                    m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::MECHANICAL_COMPONENT;
                else if( content == "CustomElectroMechanicalComponent" )
                    m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::CUSTOM_ELECTROMECHANICAL_COMPONENT;
                else if( content == "BoardCutout" )
                    m_interstratumFeatureType = INTERSTRATUM_FEATURE_TYPE::BOARD_CUTOUT;    
            }
            else if ( name == "pdm:Stratum" )
            {
                m_stratumRef.push_back( content );
            }
            child = child->GetNext();
        }
    };

bool Resolve( IDX_REGISTRY& aRegistry )
    {        
        bool result = idx_pdm::EDMD_ITEM_SHAPE::Resolve( aRegistry );

        result &=
                resolve<EDMD_STRATUM>( m_stratumRef, m_stratum  , aRegistry );

        return result;
    };


    std::vector<std::shared_ptr<EDMD_STRATUM>> m_stratum ;
    INTERSTRATUM_FEATURE_TYPE m_interstratumFeatureType;
private:
    std::vector<wxString> m_stratumRef;

};



} // namespace idx_pdm

#endif