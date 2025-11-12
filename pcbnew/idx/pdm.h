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

#include <idx/idx_base.h>
#include <idx/property.h>
#include <idx/foundation.h>

namespace idx_pdm
{

// Forward‑declarations of classes
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
    EDMD_SHAPE_DESCRIPTION( IDX_XML_NODE& aNode );

    SHAPE_DESCRIPTION_TYPE                  m_shapeDescriptionType;
    std::shared_ptr<EDMD_SHAPE_DESCRIPTION> m_boundingBox;

private:
    std::string m_boudingBoxRef;
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

    bool Parse( IDX_XML_NODE& aNode ) override;


    double                             m_xx = 1;
    double                             m_xy = 0;
    double                             m_xz = 0;
    double                             m_yx = 0;
    double                             m_yy = 1;
    double                             m_yz = 0;
    double                             m_zx = 0;
    double                             m_zy = 0;
    double                             m_zz = 1;
    idx_property::EDMD_LENGTH_PROPERTY m_tx;
    idx_property::EDMD_LENGTH_PROPERTY m_ty;
    idx_property::EDMD_LENGTH_PROPERTY m_tz;
    TYPE                               m_type;
};

class EDMD_SHAPE_ELEMENT : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_SHAPE_ELEMENT() {};
    EDMD_SHAPE_ELEMENT( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;

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

    std::vector<std::shared_ptr<EDMD_SHAPE_ELEMENT>> m_shapeElement;

private:
    std::vector<std::string> m_shapeElementRef;
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
    EDMD_FUNCTIONAL_ITEM_SHAPE( IDX_XML_NODE& aNode );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;

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
    EDMD_STRATUM_TECHNOLOGY( IDX_XML_NODE& aNode );

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
    EDMD_STRATUM() {};
    EDMD_STRATUM( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;


    std::vector<std::shared_ptr<EDMD_STRATUM_TECHNOLOGY>> m_stratumTechnology;
    STRATUM_SURFACE_DESIGNATION                           m_stratumSurfaceDesignation;

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
    bool Parse( IDX_XML_NODE& aNode ) override;

    std::vector<KEEP_CONSTRAINT_PURPOSE>                                  m_purpose;
    std::vector<std::shared_ptr<idx_property::EDMD_CONSTRAINED_PROPERTY>> m_keepConstraintCondition;

private:
};


class EDMD_KEEPOUT : public EDMD_KEEP_CONSTRAINT
{
public:
    EDMD_KEEPOUT() {};
    EDMD_KEEPOUT( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override { return this->EDMD_KEEP_CONSTRAINT::Parse( aNode ); };
};


class EDMD_KEEPIN : public EDMD_KEEP_CONSTRAINT
{
public:
    EDMD_KEEPIN() {};
    EDMD_KEEPIN( IDX_XML_NODE& aNode, bool aTopLevel );


    bool Parse( IDX_XML_NODE& aNode ) override { return this->EDMD_KEEP_CONSTRAINT::Parse( aNode ); };
};

class EDMD_ITEM_INSTANCE : public idx_foundation::EDMD_EXTENSIBLE_OBJECT
{
public:
    EDMD_ITEM_INSTANCE() {};
    EDMD_ITEM_INSTANCE( IDX_XML_NODE& aNode, bool aTopLevel );

    void         print( int aIndex = 0, int aMaxIndex = 5 );
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;


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

    ITEM_TYPE                                        m_itemType;
    std::vector<idx_foundation::EDMD_IDENTIFIER>     m_identifier;
    std::vector<idx_foundation::EDMD_NAME>           m_packageName;
    std::vector<std::shared_ptr<EDMD_ITEM_INSTANCE>> m_itemInstance;
    std::shared_ptr<EDMD_ITEM_SHAPE>                 m_shape;
    //ShapeTransformation
    idx_property::EDMD_LOGIC_PROPERTY m_accept;
    //ClearanceState
    idx_property::EDMD_LOGIC_PROPERTY m_baseline;

private:
    std::string m_shapeRef;
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

    ASSEMBLY_COMPONENT_TYPE m_assemblyComponentType;

private:
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

class EDMD_INTERSTRATUM_FEATURE : public EDMD_ITEM_SHAPE
{
public:
    EDMD_INTERSTRATUM_FEATURE() {};
    EDMD_INTERSTRATUM_FEATURE( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;

    std::vector<std::shared_ptr<EDMD_STRATUM>> m_stratum;
    INTERSTRATUM_FEATURE_TYPE                  m_interstratumFeatureType;

private:
    std::vector<std::string> m_stratumRef;
};

} // namespace idx_pdm

#endif // IDX_PDM_H
