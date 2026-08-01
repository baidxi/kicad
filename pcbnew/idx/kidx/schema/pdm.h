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

#include <idx/kidx/schema/idx_base.h>
#include <idx/kidx/schema/property.h>
#include <idx/kidx/schema/foundation.h>

namespace idx_geometry
{
class EDMD_POINT;
class EDMD_CURVE;
} // namespace idx_geometry

namespace idx_pdm
{

// Forward‑declarations of classes
class EDMD_3D_MODEL;
class EDMD_ASSEMBLY_COMPONENT;
class EDMD_BEND;
class EDMD_BEND_TYPE;
class EDMD_CIRCULAR_BEND_TYPE;
class EDMD_FUNCTIONAL_ITEM_SHAPE;
class EDMD_INTER_STRATUM_FEATURE;
class EDMD_ITEM;
class EDMD_ITEM_INSTANCE;
class EDMD_ITEM_SHAPE;
class EDMD_KEEP_CONSTRAINT;
class EDMD_KEEP_IN;
class EDMD_KEEP_OUT;
class EDMD_NON_FEATURE_ITEM_SHAPE;
class EDMD_PACKAGE_PIN;
class EDMD_SHAPE_DESCRIPTION;
class EDMD_SHAPE_ELEMENT;
class EDMD_STRATUM;
class EDMD_STRATUM_TECHNOLOGY;
class EDMD_TRANSFORMATION;

// Enums
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
    EDMD_SHAPE_DESCRIPTION() {};
    EDMD_SHAPE_DESCRIPTION( IDX_XML_NODE& aNode, bool aTopLevel );
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;


    SHAPE_DESCRIPTION_TYPE                  m_shapeDescriptionType;
    std::shared_ptr<EDMD_SHAPE_DESCRIPTION> m_boundingBox;

private:
    std::string m_boundingBoxRef;
};

class EDMD_TRANSFORMATION : public idx_foundation::EDMD_OBJECT
{
public:
    enum class TYPE
    {
        D2 = 2,
        D3 = 3
    };

    EDMD_TRANSFORMATION() {};
    EDMD_TRANSFORMATION( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool Parse( IDX_XML_NODE& aNode ) override;
    void Write( IDX_XML_NODE& aNode ) const override;


    double m_xx = 1;
    double m_xy = 0;
    double m_xz = 0;
    double m_yx = 0;
    double m_yy = 1;
    double m_yz = 0;
    double m_zx = 0;
    double m_zy = 0;
    double m_zz = 1;

    std::string                m_xx_str = "";
    std::string                m_xy_str = "";
    std::optional<std::string> m_xz_str = "";
    std::string                m_yx_str = "";
    std::string                m_yy_str = "";
    std::optional<std::string> m_yz_str = "";
    std::optional<std::string> m_zx_str = "";
    std::optional<std::string> m_zy_str = "";
    std::optional<std::string> m_zz_str = "";

    idx_property::EDMD_LENGTH_PROPERTY                m_tx;
    idx_property::EDMD_LENGTH_PROPERTY                m_ty;
    std::optional<idx_property::EDMD_LENGTH_PROPERTY> m_tz;
    TYPE                                              m_type;
};

class EDMD_SHAPE_ELEMENT : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_SHAPE_ELEMENT() {};
    EDMD_SHAPE_ELEMENT( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    bool                                    m_inverted;
    SHAPE_ELEMENT_TYPE                      m_shapeElementType;
    EDMD_TRANSFORMATION                     m_shapeDescriptionTransformation;
    std::shared_ptr<EDMD_SHAPE_DESCRIPTION> m_definingShape;

private:
    std::string m_definingShapeRef;
};

class EDMD_ITEM_SHAPE : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_ITEM_SHAPE() {};
    EDMD_ITEM_SHAPE( IDX_XML_NODE& aNode, bool aTopLevel );

    void         print( int aIndex = 0, int aMaxIndex = 5 );
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;


    std::vector<std::shared_ptr<EDMD_SHAPE_ELEMENT>> m_shapeElement;
    std::vector<std::string>                         m_shapeElementRef;

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
        MECHANICAL       // NO LONGER USED BY IDX
    };

    EDMD_FUNCTIONAL_ITEM_SHAPE() {};
    EDMD_FUNCTIONAL_ITEM_SHAPE( IDX_XML_NODE& aNode, bool aTopLevel = true );

    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

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
    EDMD_STRATUM_TECHNOLOGY() {};
    EDMD_STRATUM_TECHNOLOGY( IDX_XML_NODE& aNode, bool aTopLevel = true );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    LAYER_POSITION_TYPE     m_layerPositionType;
    STRATUM_TECHNOLOGY_TYPE m_stratumTechnologyType;
    LAYER_PURPOSE           m_layerPurpose;

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
    enum class TYPE
    {
        DOCUMENTATION = 0,
        DESIGN
    };

    EDMD_STRATUM() {};
    EDMD_STRATUM( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;


    std::vector<std::shared_ptr<EDMD_STRATUM_TECHNOLOGY>> m_stratumTechnology;
    STRATUM_SURFACE_DESIGNATION                           m_stratumSurfaceDesignation;
    TYPE                                                  m_type = TYPE::DESIGN;

private:
    std::vector<std::string> m_stratumTechnologyRef;
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
    EDMD_KEEP_CONSTRAINT() {};
    EDMD_KEEP_CONSTRAINT( IDX_XML_NODE& aNode, bool aTopLevel );
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    std::vector<KEEP_CONSTRAINT_PURPOSE>                                  m_purpose;
    std::vector<std::shared_ptr<idx_property::EDMD_CONSTRAINED_PROPERTY>> m_keepConstraintCondition;

private:
};


class EDMD_KEEP_OUT : public EDMD_KEEP_CONSTRAINT
{
public:
    EDMD_KEEP_OUT() {};
    EDMD_KEEP_OUT( IDX_XML_NODE& aNode, bool aTopLevel );
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override { return this->EDMD_KEEP_CONSTRAINT::Parse( aNode ); };
    virtual void Write( IDX_XML_NODE& aNode ) const override;
};


class EDMD_KEEP_IN : public EDMD_KEEP_CONSTRAINT
{
public:
    EDMD_KEEP_IN() {};
    EDMD_KEEP_IN( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override { return this->EDMD_KEEP_CONSTRAINT::Parse( aNode ); };
    virtual void Write( IDX_XML_NODE& aNode ) const override;
};

class EDMD_ITEM_INSTANCE : public idx_foundation::EDMD_EXTENSIBLE_OBJECT
{
public:
    EDMD_ITEM_INSTANCE() {};
    EDMD_ITEM_INSTANCE( IDX_XML_NODE& aNode, bool aTopLevel );


    enum class TYPE
    {
        BOARD_OUTLINE = 0,
        KEEP_IN,
        KEEP_OUT,
        NPTH,
        PTH,
        COMPONENT,
        OTHER
    };


    void         print( int aIndex = 0, int aMaxIndex = 5 );
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;
    TYPE         GetType() const;
    virtual void Write( IDX_XML_NODE& aNode ) const override;


    idx_foundation::EDMD_NAME              m_instanceName;
    EDMD_TRANSFORMATION                    m_transformation;
    std::shared_ptr<EDMD_ITEM>             m_item;
    std::vector<idx_foundation::EDMD_NAME> m_instanceUserAreaLayerName;
    std::string                            m_name;
    double                                 m_zOffset;

private:
    std::string m_itemRef;
};

enum class ITEM_TYPE
{
    SINGLE = 0,
    ASSEMBLY
};

class EDMD_ITEM : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_ITEM() {};
    EDMD_ITEM( IDX_XML_NODE& aNode, bool aTopLevel );

    void         print( int aIndex = 0, int aMaxIndex = 5 );
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;

    ITEM_TYPE                                                m_itemType;
    std::vector<idx_foundation::EDMD_IDENTIFIER>             m_identifier;
    std::vector<idx_foundation::EDMD_NAME>                   m_packageName;
    std::vector<std::shared_ptr<EDMD_ITEM_INSTANCE>>         m_itemInstance;
    std::shared_ptr<EDMD_ITEM_SHAPE>                         m_shape;
    EDMD_TRANSFORMATION                                        m_shapeTransformation;
    idx_property::EDMD_LOGIC_PROPERTY                        m_accept;
    std::vector<idx_property::EDMD_CLEARANCE_STATE_PROPERTY> m_clearanceState;
    idx_property::EDMD_LOGIC_PROPERTY                        m_baseline;
    std::string                                              m_referenceName;
    std::string                                              m_assembleToName;
    std::vector<std::shared_ptr<EDMD_PACKAGE_PIN>>           m_packagePin;

    virtual void Write( IDX_XML_NODE& aNode ) const override;

private:
    std::string              m_shapeRef;
    std::string              m_3dModelRef;
    std::vector<std::string> m_packagePinRef;
};

enum class ASSEMBLY_COMPONENT_TYPE
{
    ASSEMBLY_GROUP_COMPONENT = 0,
    LAMINATE_COMPONENT,
    THERMAL_COMPONENT,
    PHYSICAL,
    PADSTACK
};

class EDMD_ASSEMBLY_COMPONENT : public EDMD_ITEM_SHAPE
{
public:
    EDMD_ASSEMBLY_COMPONENT() {};
    EDMD_ASSEMBLY_COMPONENT( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    ASSEMBLY_COMPONENT_TYPE m_assemblyComponentType;

private:
};


// -----------------------------------------------------
// EDMD_PACKAGE_PIN
// -----------------------------------------------------

class EDMD_PACKAGE_PIN : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_PACKAGE_PIN() {};
    EDMD_PACKAGE_PIN( IDX_XML_NODE& aNode, bool aTopLevel );
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    std::string m_pointRef;
    std::string m_shapeRef;
    std::string m_pin_number;
    bool        m_primary = false;

    std::shared_ptr<idx_geometry::EDMD_POINT> m_point;
    std::shared_ptr<idx_pdm::EDMD_ITEM_SHAPE> m_shape;
};


// -----------------------------------------------------
// EDMD_3D_MODEL
// -----------------------------------------------------

class EDMD_3D_MODEL : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_3D_MODEL() {};
    EDMD_3D_MODEL( IDX_XML_NODE& aNode, bool aTopLevel );
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    std::string m_modelIdentifier;
    std::string m_modelVersion;
    std::string m_modelLocation;
};


// -----------------------------------------------------
// EDMD_BEND_TYPE enums
// -----------------------------------------------------

enum class EDMDINNER_SIDE
{
    TOP = 0,
    BOTTOM
};


// -----------------------------------------------------
// EDMD_BEND
// -----------------------------------------------------

class EDMD_BEND : public EDMD_ITEM_SHAPE
{
public:
    EDMD_BEND() {};
    EDMD_BEND( IDX_XML_NODE& aNode, bool aTopLevel );
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    std::string                      m_bendTypeRef;
    int                              m_sequenceNumber = 0;
    std::shared_ptr<EDMD_ITEM_SHAPE> m_bendType;
};


// -----------------------------------------------------
// EDMD_BEND_TYPE
// -----------------------------------------------------

class EDMD_BEND_TYPE : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_BEND_TYPE() {};
    EDMD_BEND_TYPE( IDX_XML_NODE& aNode, bool aTopLevel );
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    std::string                               m_bendLineRef;
    std::shared_ptr<idx_geometry::EDMD_CURVE> m_bendLine;
};


// -----------------------------------------------------
// EDMD_CIRCULAR_BEND_TYPE
// -----------------------------------------------------

class EDMD_CIRCULAR_BEND_TYPE : public EDMD_BEND_TYPE
{
public:
    EDMD_CIRCULAR_BEND_TYPE() {};
    EDMD_CIRCULAR_BEND_TYPE( IDX_XML_NODE& aNode );
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    EDMDINNER_SIDE m_innerSide = EDMDINNER_SIDE::TOP;
    double         m_innerRadius = 0.0;
    double         m_innerAngle = 0.0;
};


// -----------------------------------------------------
// EDMDNonFeatureItemShape missing types
// -----------------------------------------------------

enum class NON_FEATURE_ITEM_SHAPE_PURPOSE
{
    INFO_TEXT = 0,
    INFO_GEOMETRY,
    AUXILIARY_GEOMETRY
};


class EDMD_NON_FEATURE_ITEM_SHAPE : public EDMD_ITEM_SHAPE
{
public:
    EDMD_NON_FEATURE_ITEM_SHAPE() {};
    EDMD_NON_FEATURE_ITEM_SHAPE( IDX_XML_NODE& aNode, bool aTopLevel );
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    NON_FEATURE_ITEM_SHAPE_PURPOSE                m_purpose;
    std::vector<std::string>                      m_relatedItemShapeRef;
    std::vector<std::shared_ptr<EDMD_ITEM_SHAPE>> m_relatedItemShape;
};


// -----------------------------------------------------
// Geometry and Signal Types
// -----------------------------------------------------

enum class SIGNAL_TYPE
{
    SIGNAL = 0,
    POWER,
    GROUND,
    TEST,
    OTHER
};


enum class GEOMETRY_TYPE
{
    BOARD_OUTLINE = 0,
    COMPONENT_OUTLINE,
    PAD,
    TRACK,
    VIA,
    KEEP_OUT,
    KEEP_IN,
    AREA,
    TEXT,
    DRILL,
    HOLE,
    OTHER
};


enum class STRATUM_TYPE
{
    DOCUMENTATION_LAYER_STRATUM = 0,
    DESIGN_LAYER_STRATUM
};

enum class INTERSTRATUM_FEATURE_TYPE
{
    UNSUPPORTED_PASSAGE = 0,
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

class EDMD_INTER_STRATUM_FEATURE : public EDMD_ITEM_SHAPE
{
public:
    EDMD_INTER_STRATUM_FEATURE() {};
    EDMD_INTER_STRATUM_FEATURE( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;


    std::vector<std::shared_ptr<EDMD_STRATUM>> m_stratum;
    INTERSTRATUM_FEATURE_TYPE                  m_interstratumFeatureType;

private:
    std::vector<std::string> m_stratumRef;
};

} // namespace idx_pdm

#endif // IDX_PDM_H
