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


void EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_COLLABORATION_OBJECT::Write( aNode );
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


void EDMD_CURVE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT::Write( aNode );

    WriteChild( aNode, "d2:Thickness", m_thickness );
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


bool EDMD_HYPERBOLA::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "d2:Center" ) )
    {
        m_centerRef = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:SemiMajor" ) )
    {
        m_semiMajor = idx_property::EDMD_LENGTH_PROPERTY( aNode, true );
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:SemiMinor" ) )
    {
        m_semiMinor = idx_property::EDMD_LENGTH_PROPERTY( aNode, true );
        return true;
    }

    return this->EDMD_CURVE::Parse( aNode );
}


void EDMD_HYPERBOLA::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CURVE::Write( aNode );

    WriteValueChild( aNode, "d2:Center", m_centerRef );
    WriteChild( aNode, "d2:SemiMajor", m_semiMajor );
    WriteChild( aNode, "d2:SemiMinor", m_semiMinor );
}

// --------------------------------------------------------------------------
// EDMD_TRIMMED_CURVE
// --------------------------------------------------------------------------

EDMD_TRIMMED_CURVE::EDMD_TRIMMED_CURVE( IDX_XML_NODE& aNode ) :
        EDMD_CURVE( aNode, false )
{
}


bool EDMD_TRIMMED_CURVE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "d2:Curve" ) )
    {
        m_curveRef = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:Start" ) )
    {
        m_startRef = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:End" ) )
    {
        m_endRef = content;
        return true;
    }

    return this->EDMD_CURVE::Parse( aNode );
}


void EDMD_TRIMMED_CURVE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CURVE::Write( aNode );

    WriteValueChild( aNode, "d2:Curve", m_curveRef );
    WriteValueChild( aNode, "d2:Start", m_startRef );
    WriteValueChild( aNode, "d2:End", m_endRef );
}

// --------------------------------------------------------------------------
// EDMD_LINE
// --------------------------------------------------------------------------

EDMD_LINE::EDMD_LINE( IDX_XML_NODE& aNode ) :
        EDMD_CURVE( aNode, false )
{
}


bool EDMD_LINE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "d2:Point" ) )
    {
        m_pointRef = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:Vector" ) )
    {
        m_vectorRef = content;
        return true;
    }

    return this->EDMD_CURVE::Parse( aNode );
}


void EDMD_LINE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CURVE::Write( aNode );

    WriteValueChild( aNode, "d2:Point", m_pointRef );
    WriteValueChild( aNode, "d2:Vector", m_vectorRef );
}

// --------------------------------------------------------------------------
// EDMD_B_SPLINE_CURVE
// --------------------------------------------------------------------------

EDMD_B_SPLINE_CURVE::EDMD_B_SPLINE_CURVE( IDX_XML_NODE& aNode ) :
        EDMD_CURVE( aNode, false )
{
    m_curveForm = EDMD_B_SPLINE_CURVE_FORM::UNSPECIFIED;
    m_closedCurve = false;
    m_selfIntersect = false;
}


bool EDMD_B_SPLINE_CURVE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "d2:Degree" ) )
    {
        m_degree = std::stoi( content );
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:ControlPointsList" ) )
    {
        m_controlPointsRef.push_back( content );
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:CurveForm" ) )
    {
        if( content == "PolylineForm" )
            m_curveForm = EDMD_B_SPLINE_CURVE_FORM::POLYLINE_FORM;
        else if( content == "CircularArc" )
            m_curveForm = EDMD_B_SPLINE_CURVE_FORM::CIRCULAR_ARC;
        else if( content == "EllipticArc" )
            m_curveForm = EDMD_B_SPLINE_CURVE_FORM::ELLIPTIC_ARC;
        else if( content == "ParabolicArc" )
            m_curveForm = EDMD_B_SPLINE_CURVE_FORM::PARABOLIC_ARC;
        else if( content == "HyperbolicArc" )
            m_curveForm = EDMD_B_SPLINE_CURVE_FORM::HYPERBOLIC_ARC;
        else
            m_curveForm = EDMD_B_SPLINE_CURVE_FORM::UNSPECIFIED;
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:ClosedCurve" ) )
    {
        m_closedCurve = ( content == "true" || content == "1" );
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:SelfIntersect" ) )
    {
        m_selfIntersect = ( content == "true" || content == "1" );
        return true;
    }

    return this->EDMD_CURVE::Parse( aNode );
}


void EDMD_B_SPLINE_CURVE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CURVE::Write( aNode );

    WriteValueChild( aNode, "d2:Degree", std::to_string( m_degree ) );

    for( const auto& pointRef : m_controlPointsRef )
    {
        WriteValueChild( aNode, "d2:ControlPointsList", pointRef );
    }

    std::string curveFormStr;
    switch( m_curveForm )
    {
    case EDMD_B_SPLINE_CURVE_FORM::POLYLINE_FORM: curveFormStr = "PolylineForm"; break;
    case EDMD_B_SPLINE_CURVE_FORM::CIRCULAR_ARC: curveFormStr = "CircularArc"; break;
    case EDMD_B_SPLINE_CURVE_FORM::ELLIPTIC_ARC: curveFormStr = "EllipticArc"; break;
    case EDMD_B_SPLINE_CURVE_FORM::PARABOLIC_ARC: curveFormStr = "ParabolicArc"; break;
    case EDMD_B_SPLINE_CURVE_FORM::HYPERBOLIC_ARC: curveFormStr = "HyperbolicArc"; break;
    default: curveFormStr = "Unspecified"; break;
    }
    WriteValueChild( aNode, "d2:CurveForm", curveFormStr );

    WriteValueChild( aNode, "d2:ClosedCurve", std::string( m_closedCurve ? "true" : "false" ) );
    WriteValueChild( aNode, "d2:SelfIntersect", std::string( m_selfIntersect ? "true" : "false" ) );
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


bool EDMD_COMPOSITE_CURVE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "d2:Curve" ) )
    {
        m_curveRef.push_back( content );
        return true;
    }

    return this->EDMD_CURVE::Parse( aNode );
}


void EDMD_COMPOSITE_CURVE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CURVE::Write( aNode );

    for( const auto& curveRef : m_curveRef )
    {
        WriteValueChild( aNode, "d2:Curve", curveRef );
    }
}

// --------------------------------------------------------------------------
// EDMD_POLY_LINE
// --------------------------------------------------------------------------

EDMD_POLY_LINE::EDMD_POLY_LINE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_CURVE( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_POLY_LINE::Parse( IDX_XML_NODE& aNode )
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


void EDMD_POLY_LINE::Write( IDX_XML_NODE& aNode ) const
{
    // Write points
    for( const auto& point : m_point )
    {
        if( point )
        {
            WriteValueChild( aNode, "d2:Point", point->m_id );
        }
    }
    this->EDMD_CURVE::Write( aNode );
}

bool EDMD_POLY_LINE::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = EDMD_CURVE::Resolve( aRegistry );
    result &= resolve<EDMD_POINT>( m_pointRef, m_point, aRegistry );
    return result;
}

// --------------------------------------------------------------------------
// EDMD_CURVE_SET2D
// --------------------------------------------------------------------------

EDMD_CURVE_SET2D::EDMD_CURVE_SET2D() :
        idx_pdm::EDMD_SHAPE_DESCRIPTION()
{
    m_xsiType = "d2:EDMDCurveSet2d";
}

EDMD_CURVE_SET2D::EDMD_CURVE_SET2D( IDX_XML_NODE& aNode ) :
        idx_pdm::EDMD_SHAPE_DESCRIPTION( aNode, false )
{
    m_xsiType = "d2:EDMDCurveSet2d";
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

bool EDMD_CURVE_SET2D::Resolve( IDX_REGISTRY& aRegistry )
{
    bool result = true;
    result &= resolve<EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT>( m_modelElementRef, m_modelElement, aRegistry );
    return result;
}


bool EDMD_CURVE_SET2D::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "d2:LowerBound" ) )
    {
        m_lowerBound = idx_property::EDMD_LENGTH_PROPERTY( aNode, true );
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:UpperBound" ) )
    {
        m_upperBound = idx_property::EDMD_LENGTH_PROPERTY( aNode, true );
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:DetailedGeometricModelElement" ) )
    {
        m_modelElementRef.push_back( content );
        return true;
    }

    return this->EDMD_SHAPE_DESCRIPTION::Parse( aNode );
}


void EDMD_CURVE_SET2D::Write( IDX_XML_NODE& aNode ) const
{
    aNode.SetAttribute( "xsi:type", "d2:EDMDCurveSet2d" );

    this->EDMD_SHAPE_DESCRIPTION::Write( aNode );

    WriteChild( aNode, "d2:LowerBound", m_lowerBound );
    WriteChild( aNode, "d2:UpperBound", m_upperBound );

    // Write model elements
    for( const auto& element : m_modelElement )
    {
        if( element )
        {
            WriteValueChild( aNode, "d2:DetailedGeometricModelElement", element->m_id );
        }
    }
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


bool EDMD_ARC::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "d2:StartPoint" ) )
    {
        m_startPointRef = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:EndPoint" ) )
    {
        m_endPointRef = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:IncludeAngle" ) )
    {
        m_includeAngle = idx_property::EDMD_ANGLE_PROPERTY( aNode, true );
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:Thickness" ) )
    {
        m_thickness = idx_property::EDMD_LENGTH_PROPERTY( aNode, true );
        return true;
    }

    return this->EDMD_CURVE::Parse( aNode );
}


void EDMD_ARC::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CURVE::Write( aNode );

    // Write StartPoint reference or actual point
    if( m_startPoint )
    {
        std::shared_ptr<IDX_XML_NODE> startNode = aNode.AppendChild( "d2:StartPoint" );
        m_startPoint->Write( *startNode );
    }
    else if( !m_startPointRef.empty() )
    {
        aNode.AppendChild( "d2:StartPoint" )->SetText( m_startPointRef );
    }

    // Write EndPoint reference or actual point
    if( m_endPoint )
    {
        std::shared_ptr<IDX_XML_NODE> endNode = aNode.AppendChild( "d2:EndPoint" );
        m_endPoint->Write( *endNode );
    }
    else if( !m_endPointRef.empty() )
    {
        aNode.AppendChild( "d2:EndPoint" )->SetText( m_endPointRef );
    }

    WriteChild( aNode, "d2:IncludeAngle", m_includeAngle );
    WriteChild( aNode, "d2:Thickness", m_thickness );
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


bool EDMD_ELLIPSE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "d2:Center" ) )
    {
        m_centerRef = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:SemiMajor" ) )
    {
        m_semiMajor = idx_property::EDMD_LENGTH_PROPERTY( aNode, true );
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:SemiMinor" ) )
    {
        m_semiMinor = idx_property::EDMD_LENGTH_PROPERTY( aNode, true );
        return true;
    }

    return this->EDMD_CURVE::Parse( aNode );
}


void EDMD_ELLIPSE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CURVE::Write( aNode );

    WriteValueChild( aNode, "d2:Center", m_centerRef );
    WriteChild( aNode, "d2:SemiMajor", m_semiMajor );
    WriteChild( aNode, "d2:SemiMinor", m_semiMinor );
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


bool EDMD_POINT::Parse( IDX_XML_NODE& aNode )
{
    return this->EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT::Parse( aNode );
}


// --------------------------------------------------------------------------
// EDMD_CIRCLE
// --------------------------------------------------------------------------

EDMD_CIRCLE::EDMD_CIRCLE( IDX_XML_NODE& aNode ) :
        EDMD_CURVE( aNode, false )
{
}


bool EDMD_CIRCLE::Parse( IDX_XML_NODE& aNode )
{
    // EDMD_CIRCLE doesn't have additional members beyond EDMD_CURVE
    // The actual circle-specific members are in subclasses
    return this->EDMD_CURVE::Parse( aNode );
}


void EDMD_CIRCLE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CURVE::Write( aNode );
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
// EDMD_CIRCLE3_POINT
// --------------------------------------------------------------------------

EDMD_CIRCLE3_POINT::EDMD_CIRCLE3_POINT( IDX_XML_NODE& aNode ) :
        EDMD_CIRCLE( aNode )
{
}


bool EDMD_CIRCLE3_POINT::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "d2:P1" ) )
    {
        m_p1Ref = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:P2" ) )
    {
        m_p2Ref = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:P3" ) )
    {
        m_p3Ref = content;
        return true;
    }

    return this->EDMD_CIRCLE::Parse( aNode );
}


void EDMD_CIRCLE3_POINT::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CIRCLE::Write( aNode );

    WriteValueChild( aNode, "d2:P1", m_p1Ref );
    WriteValueChild( aNode, "d2:P2", m_p2Ref );
    WriteValueChild( aNode, "d2:P3", m_p3Ref );
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


bool EDMD_CIRCLE_CENTER::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "d2:Center" ) )
    {
        m_centerRef = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:Diameter" ) )
    {
        m_diameter = idx_property::EDMD_LENGTH_PROPERTY( aNode, true );
        return true;
    }

    return this->EDMD_CIRCLE::Parse( aNode );
}


void EDMD_CIRCLE_CENTER::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CIRCLE::Write( aNode );

    WriteValueChild( aNode, "d2:Center", m_centerRef );
    WriteChild( aNode, "d2:Diameter", m_diameter );
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


bool EDMD_OFFSET_CURVE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "d2:BasisCurve" ) )
    {
        m_basisCurveRef = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:Distance" ) )
    {
        m_distance = idx_property::EDMD_LENGTH_PROPERTY( aNode, true );
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:SelfIntersect" ) )
    {
        m_selfIntersects = ( content == "true" || content == "1" );
        return true;
    }

    return this->EDMD_CURVE::Parse( aNode );
}


void EDMD_OFFSET_CURVE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CURVE::Write( aNode );

    WriteValueChild( aNode, "d2:BasisCurve", m_basisCurveRef );
    WriteChild( aNode, "d2:Distance", m_distance );
    WriteValueChild( aNode, "d2:SelfIntersect", std::string( m_selfIntersects ? "true" : "false" ) );
}

// --------------------------------------------------------------------------
// EDMD_PARABOLA
// --------------------------------------------------------------------------

EDMD_PARABOLA::EDMD_PARABOLA( IDX_XML_NODE& aNode ) :
        EDMD_CURVE( aNode, false )
{
}


bool EDMD_PARABOLA::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "d2:Apex" ) )
    {
        m_apexRef = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "d2:Focus" ) )
    {
        m_focusRef = content;
        return true;
    }

    return this->EDMD_CURVE::Parse( aNode );
}


void EDMD_PARABOLA::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CURVE::Write( aNode );

    WriteValueChild( aNode, "d2:Apex", m_apexRef );
    WriteValueChild( aNode, "d2:Focus", m_focusRef );
}

// --------------------------------------------------------------------------
// EDMD_POINT_ON_CURVE_INTERSECTION
// --------------------------------------------------------------------------

EDMD_POINT_ON_CURVE_INTERSECTION::EDMD_POINT_ON_CURVE_INTERSECTION( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_POINT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}


bool EDMD_POINT_ON_CURVE_INTERSECTION::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();

    if( IDX_keyword_compare( name, "geometry.d2:Curve" ) )
    {
        std::string ref = aNode.GetNodeContent();
        if( !ref.empty() )
            m_curveRef = ref;
        return true;
    }
    else if( IDX_keyword_compare( name, "geometry.d2:Intersector" ) )
    {
        std::string ref = aNode.GetNodeContent();
        if( !ref.empty() )
            m_intersectorRef = ref;
        return true;
    }

    return this->EDMD_POINT::Parse( aNode );
}


bool EDMD_POINT_ON_CURVE_INTERSECTION::Resolve( IDX_REGISTRY& aRegistry )
{
    if( !this->EDMD_POINT::Resolve( aRegistry ) )
        return false;

    if( !m_curveRef.empty() )
    {
        m_curve = std::dynamic_pointer_cast<EDMD_CURVE>( aRegistry.resolve<EDMD_CURVE>( m_curveRef, true ) );
        if( !m_curve )
            return false;
    }

    if( !m_intersectorRef.empty() )
    {
        m_intersector =
                std::dynamic_pointer_cast<EDMD_CURVE>( aRegistry.resolve<EDMD_CURVE>( m_intersectorRef, true ) );
        if( !m_intersector )
            return false;
    }

    return true;
}


void EDMD_POINT_ON_CURVE_INTERSECTION::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_POINT::Write( aNode );

    WriteValueChild( aNode, "geometry.d2:Curve", m_curveRef );
    WriteValueChild( aNode, "geometry.d2:Intersector", m_intersectorRef );
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


void EDMD_POINT::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT::Write( aNode );
}


void EDMD_CARTESIAN_POINT::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_POINT::Write( aNode );

    // Write X, Y, Z coordinates using their Write methods to ensure proper XML structure
    // Only write coordinates that have values (m_value_str is not empty)
    if( !m_x.m_value_str.empty() )
    {
        std::shared_ptr<IDX_XML_NODE> xNode = aNode.AppendChild( "d2:X" );
        m_x.Write( *xNode );
    }
    if( !m_y.m_value_str.empty() )
    {
        std::shared_ptr<IDX_XML_NODE> yNode = aNode.AppendChild( "d2:Y" );
        m_y.Write( *yNode );
    }
    if( !m_z.m_value_str.empty() )
    {
        std::shared_ptr<IDX_XML_NODE> zNode = aNode.AppendChild( "d2:Z" );
        m_z.Write( *zNode );
    }
}


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
        return this->EDMD_POINT::Parse( aNode );

    return true;
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
    if( aTopLevel )
        ParseChildren( aNode );
}


bool EDMD_POINT_ON_CURVE_DISTANCE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();

    if( IDX_keyword_compare( name, "geometry.d2:Curve" ) )
    {
        std::string ref = aNode.GetNodeContent();
        if( !ref.empty() )
            m_curveRef = ref;
        return true;
    }
    else if( IDX_keyword_compare( name, "geometry.d2:Start" ) )
    {
        std::string ref = aNode.GetNodeContent();
        if( !ref.empty() )
            m_startRef = ref;
        return true;
    }
    else if( IDX_keyword_compare( name, "geometry.d2:Distance" ) )
    {
        return m_distance.Parse( aNode );
    }

    return this->EDMD_POINT::Parse( aNode );
}


bool EDMD_POINT_ON_CURVE_DISTANCE::Resolve( IDX_REGISTRY& aRegistry )
{
    if( !this->EDMD_POINT::Resolve( aRegistry ) )
        return false;

    if( !m_curveRef.empty() )
    {
        m_curve = std::dynamic_pointer_cast<EDMD_CURVE>( aRegistry.resolve<EDMD_CURVE>( m_curveRef, true ) );
        if( !m_curve )
            return false;
    }

    if( !m_startRef.empty() )
    {
        m_start = std::dynamic_pointer_cast<EDMD_POINT>( aRegistry.resolve<EDMD_POINT>( m_startRef, true ) );
        if( !m_start )
            return false;
    }

    if( !m_distance.Resolve( aRegistry ) )
        return false;

    return true;
}


void EDMD_POINT_ON_CURVE_DISTANCE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_POINT::Write( aNode );

    WriteValueChild( aNode, "geometry.d2:Curve", m_curveRef );
    WriteValueChild( aNode, "geometry.d2:Start", m_startRef );
    m_distance.Write( *( aNode.AppendChild( "geometry.d2:Distance" ) ) );
}

} // namespace idx_geometry
