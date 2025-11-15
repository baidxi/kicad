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

#include <idx/kidx/schema/geometry.h>
#include <iostream>

namespace idx_geometry
{

// --------------------------------------------------------------------------
// EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT
// --------------------------------------------------------------------------

EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT::EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT() :
        idx_foundation::EDMD_COLLABORATION_OBJECT()
{
}

EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT::EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
}


bool EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT::Parse( IDX_XML_NODE& aNode )
{
    return this->idx_foundation::EDMD_COLLABORATION_OBJECT::Parse( aNode );
}

bool EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT::Resolve( IDX_REGISTRY& aRegistry )
{
    return idx_foundation::EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );
}

// --------------------------------------------------------------------------
// EDMD_CURVE
// --------------------------------------------------------------------------

EDMD_CURVE::EDMD_CURVE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT( aNode, false ),
        m_thickness()
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_CURVE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();


    if( name == "d2:Thickness" )
    {
        m_thickness = idx_property::EDMD_LENGTH_PROPERTY( aNode, true );
        return true;
    }

    return this->EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT::Parse( aNode );
}


EDMD_CURVE::~EDMD_CURVE() = default;

bool EDMD_CURVE::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT::Resolve( aRegistry );
}

// --------------------------------------------------------------------------
// EDMD_HYPERBOLA
// --------------------------------------------------------------------------

EDMD_HYPERBOLA::EDMD_HYPERBOLA( IDX_XML_NODE& aNode ) :
        EDMD_CURVE( aNode, false ),
        m_semiMajor( aNode, true ),
        m_semiMinor( aNode, true )
{
}

// --------------------------------------------------------------------------
// EDMD_TRIMMED_CURVE
// --------------------------------------------------------------------------

EDMD_TRIMMED_CURVE::EDMD_TRIMMED_CURVE( IDX_XML_NODE& aNode ) :
        EDMD_CURVE( aNode, false )
{
}

// --------------------------------------------------------------------------
// EDMD_LINE
// --------------------------------------------------------------------------

EDMD_LINE::EDMD_LINE( IDX_XML_NODE& aNode ) :
        EDMD_CURVE( aNode, false )
{
}

// --------------------------------------------------------------------------
// EDMD_BSPLINE_CURVE
// --------------------------------------------------------------------------

EDMD_BSPLINE_CURVE::EDMD_BSPLINE_CURVE( IDX_XML_NODE& aNode ) :
        EDMD_CURVE( aNode, false )
{
    m_curveForm = EDMD_BSPLINE_CURVE_FORM::UNSPECIFIED;
    m_closedCurve = false;
    m_selfIntersect = false;
}

// --------------------------------------------------------------------------
// EDMD_COMPOSITE_CURVE
// --------------------------------------------------------------------------

EDMD_COMPOSITE_CURVE::EDMD_COMPOSITE_CURVE( IDX_XML_NODE& aNode ) :
        EDMD_CURVE( aNode, false )
{
    //IDX_XML_NODE& child = aNode.GetChildren();
    //std::vector<std::string> refnames;
}

// --------------------------------------------------------------------------
// EDMD_POLYLINE
// --------------------------------------------------------------------------

EDMD_POLYLINE::EDMD_POLYLINE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_CURVE( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_POLYLINE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();


    if( IDX_keyword_compare( name, "d2:Point" ) )
    {
        m_pointRef.push_back( content );
        return true;
    }

    return this->EDMD_CURVE::Parse( aNode );
}

bool EDMD_POLYLINE::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = EDMD_CURVE::Resolve( aRegistry );
    result &= resolve<EDMD_POINT>( m_pointRef, m_point, aRegistry );
    return result;
}

// --------------------------------------------------------------------------
// EDMD_CURVESET2D
// --------------------------------------------------------------------------

EDMD_CURVESET2D::EDMD_CURVESET2D() :
        idx_pdm::EDMD_SHAPE_DESCRIPTION()
{
}

EDMD_CURVESET2D::EDMD_CURVESET2D( IDX_XML_NODE& aNode ) :
        idx_pdm::EDMD_SHAPE_DESCRIPTION( aNode, false )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "d2:LowerBound" ) )
        {
            m_lowerBound = idx_property::EDMD_LENGTH_PROPERTY( *child, true );
        }
        else if( IDX_keyword_compare( name, "d2:UpperBound" ) )
        {
            m_upperBound = idx_property::EDMD_LENGTH_PROPERTY( *child, true );
        }
        else if( IDX_keyword_compare( name, "d2:DetailedGeometricModelElement" ) )
        {
            m_modelElementRef.push_back( content );
        }
        child = child->GetNext();
    }
}

bool EDMD_CURVESET2D::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = true;
    result &= resolve<EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT>( m_modelElementRef, m_modelElement, aRegistry );
    return result;
}

// --------------------------------------------------------------------------
// EDMD_ARC
// --------------------------------------------------------------------------

EDMD_ARC::EDMD_ARC( IDX_XML_NODE& aNode ) :
        EDMD_CURVE( aNode, false )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "d2:StartPoint" ) )
            m_startPointRef = content;
        if( IDX_keyword_compare( name, "d2:EndPoint" ) )
            m_endPointRef = content;
        if( IDX_keyword_compare( name, "d2:IncludeAngle" ) )
            m_includeAngle = idx_property::EDMD_ANGLE_PROPERTY( *child, true );

        child = child->GetNext();
    }
}

bool EDMD_ARC::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = true;
    result &= resolve<EDMD_POINT>( m_startPointRef, &m_startPoint, aRegistry );
    result &= resolve<EDMD_POINT>( m_endPointRef, &m_endPoint, aRegistry );
    return result;
}

std::shared_ptr<EDMD_POINT> EDMD_ARC::GetStart()
{
    return m_startPoint;
}
std::shared_ptr<EDMD_POINT> EDMD_ARC::GetEnd()
{
    return m_endPoint;
}
idx_property::EDMD_ANGLE_PROPERTY& EDMD_ARC::GetIncludeAngle()
{
    return m_includeAngle;
}

// --------------------------------------------------------------------------
// EDMD_ELLIPSE
// --------------------------------------------------------------------------

EDMD_ELLIPSE::EDMD_ELLIPSE( IDX_XML_NODE& aNode ) :
        EDMD_CURVE( aNode, false ),
        m_semiMinor( aNode, true ),
        m_semiMajor( aNode, true )
{
}

// --------------------------------------------------------------------------
// EDMD_POINT
// --------------------------------------------------------------------------

EDMD_POINT::EDMD_POINT() :
        EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT()
{
}

EDMD_POINT::EDMD_POINT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT( aNode, false )
{
}

double EDMD_POINT::GetX() const
{
    return 0;
}
double EDMD_POINT::GetY() const
{
    return 0;
}
EDMD_POINT::~EDMD_POINT() = default;

// --------------------------------------------------------------------------
// EDMD_CIRCLE
// --------------------------------------------------------------------------

EDMD_CIRCLE::EDMD_CIRCLE( IDX_XML_NODE& aNode ) :
        EDMD_CURVE( aNode, false )
{
}

bool EDMD_CIRCLE::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_CURVE::Resolve( aRegistry );
}

std::shared_ptr<EDMD_POINT> EDMD_CIRCLE::GetCenter() const
{
    return std::make_shared<EDMD_POINT>();
}

double EDMD_CIRCLE::GetRadius() const
{
    return 0;
}

// --------------------------------------------------------------------------
// EDMD_CIRCLE_3POINTS
// --------------------------------------------------------------------------

EDMD_CIRCLE_3POINTS::EDMD_CIRCLE_3POINTS( IDX_XML_NODE& aNode ) :
        EDMD_CIRCLE( aNode )
{
}

// --------------------------------------------------------------------------
// EDMD_CIRCLE_CENTER
// --------------------------------------------------------------------------

EDMD_CIRCLE_CENTER::EDMD_CIRCLE_CENTER( IDX_XML_NODE& aNode ) :
        EDMD_CIRCLE( aNode ),
        m_diameter( aNode, false )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "d2:Center" ) )
        {
            m_centerRef = content;
        }
        else if( IDX_keyword_compare( name, "d2:Diameter" ) )
        {
            m_diameter = idx_property::EDMD_LENGTH_PROPERTY( *child, true );
        }

        child = child->GetNext();
    }
}

bool EDMD_CIRCLE_CENTER::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = EDMD_CIRCLE::Resolve( aRegistry );
    result &= resolve<EDMD_POINT>( m_centerRef, &m_center, aRegistry );
    return result;
}

std::shared_ptr<EDMD_POINT> EDMD_CIRCLE_CENTER::GetCenter() const
{
    return m_center;
}
double EDMD_CIRCLE_CENTER::GetRadius() const
{
    return m_diameter.m_value / 2 * 1e6;
}

// --------------------------------------------------------------------------
// EDMD_OFFSET_CURVE
// --------------------------------------------------------------------------

EDMD_OFFSET_CURVE::EDMD_OFFSET_CURVE( IDX_XML_NODE& aNode ) :
        EDMD_CURVE( aNode, false ),
        m_selfIntersects( false )
{
}

// --------------------------------------------------------------------------
// EDMD_PARABOLA
// --------------------------------------------------------------------------

EDMD_PARABOLA::EDMD_PARABOLA( IDX_XML_NODE& aNode ) :
        EDMD_CURVE( aNode, false )
{
}

// --------------------------------------------------------------------------
// EDMD_POINT_ON_CURVE_INTERSECTION
// --------------------------------------------------------------------------

EDMD_POINT_ON_CURVE_INTERSECTION::EDMD_POINT_ON_CURVE_INTERSECTION( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_POINT( aNode, false )
{
}

// --------------------------------------------------------------------------
// EDMD_CARTESIAN_POINT
// --------------------------------------------------------------------------

EDMD_CARTESIAN_POINT::EDMD_CARTESIAN_POINT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_POINT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_CARTESIAN_POINT::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();


    if( IDX_keyword_compare( name, "d2:X" ) )
        m_x = idx_property::EDMD_LENGTH_PROPERTY( aNode, true );
    else if( IDX_keyword_compare( name, "d2:Y" ) )
        m_y = idx_property::EDMD_LENGTH_PROPERTY( aNode, true );
    else if( IDX_keyword_compare( name, "d2:Z" ) )
        m_z = idx_property::EDMD_LENGTH_PROPERTY( aNode, true );
    else
        return false;
    return true;

    return this->EDMD_POINT::Parse( aNode );
}

double EDMD_CARTESIAN_POINT::GetX() const
{
    return m_x.m_value * 1e6;
}
double EDMD_CARTESIAN_POINT::GetY() const
{
    return m_y.m_value * 1e6;
}

// --------------------------------------------------------------------------
// EDMD_POINT_ON_CURVE_DISTANCE
// --------------------------------------------------------------------------

EDMD_POINT_ON_CURVE_DISTANCE::EDMD_POINT_ON_CURVE_DISTANCE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_POINT( aNode, false ),
        m_distance( aNode, true )
{
}

} // namespace idx_geometry
