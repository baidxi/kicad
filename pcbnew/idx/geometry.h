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

#ifndef IDX_GEOMETRY_HEADER_H
#define IDX_GEOMETRY_HEADER_H

#include <iostream>

namespace idx_geometry
{
class EDMD_ARC;
class EDMD_BSPLINE_CURVE;
class EDMD_CARTESIAN_POINT;
class EDMD_CIRCLE;
class EDMD_CIRCLE_3POINTS;
class EDMD_CIRCLE_CENTER;
class EDMD_COMPOSITE_CURVE;
class EDMD_CURVE;
class EDMD_CURVESET2D;
class EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT;
class EDMD_ELLIPSE;
class EDMD_HYPERBOLA;
class EDMD_LINE;
class EDMD_OFFSET_CURVE;
class EDMD_PARABOLA;
class EDMD_POINT;
class EDMD_POINT_ON_CURVE_DISTANCE;
class EDMD_POINT_ON_CURVE_INTERSECTION;
class EDMD_POLYLINE;
class EDMD_TRIMMED_CURVED;
} // namespace idx_geometry

#include <wx/xml/xml.h>
#include <idx/foundation.h>
#include <idx/property.h>
#include <idx/pdm.h>
#include <memory>


namespace idx_geometry
{
class EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT() : idx_foundation::EDMD_COLLABORATION_OBJECT() {};

    EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT( wxXmlNode& aNode ) :
            idx_foundation::EDMD_COLLABORATION_OBJECT( aNode ) {

            };

    bool Resolve( IDX_REGISTRY& aRegistry )
    {
        return idx_foundation::EDMD_COLLABORATION_OBJECT::Resolve( aRegistry );
    };
};

class EDMD_CURVE : public EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT
{
public:
    EDMD_CURVE( wxXmlNode& aNode ) : EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT( aNode ), m_thickness()
    {
        m_thickness.m_value = 0;

        wxXmlNode* child = aNode.GetChildren();

        while( child )
        {
            const wxString content = child->GetNodeContent();
            const wxString name = child->GetName();

            if( name == "d2:Thickness" )
            {
                m_thickness = idx_property::EDMD_LENGTH_PROPERTY( *child );
            }

            child = child->GetNext();
        }
    };

    virtual ~EDMD_CURVE() = default;

    virtual bool Resolve( IDX_REGISTRY& aRegistry )
    {
        return EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT::Resolve( aRegistry );
    };


    idx_property::EDMD_LENGTH_PROPERTY m_thickness;
    };


    class EDMD_HYPERBOLA : public EDMD_CURVE
    {
    public:
        EDMD_HYPERBOLA( wxXmlNode& aNode ) :
                m_semiMajor( aNode ), m_semiMinor( aNode ), EDMD_CURVE( aNode ) {

                };

        std::shared_ptr<EDMD_POINT>        m_center;
        idx_property::EDMD_LENGTH_PROPERTY m_semiMajor;
        idx_property::EDMD_LENGTH_PROPERTY m_semiMinor;
    };

    class EDMD_TRIMMED_CURVE : public EDMD_CURVE
    {
    public:
        EDMD_TRIMMED_CURVE( wxXmlNode& aNode ) :
                EDMD_CURVE( aNode ) {

                };

        std::shared_ptr<EDMD_CURVE> m_curve;
        std::shared_ptr<EDMD_POINT> m_start;
        std::shared_ptr<EDMD_POINT> m_end;
    };


    class EDMD_LINE : public EDMD_CURVE
    {
    public:
        EDMD_LINE( wxXmlNode& aNode ) :
                EDMD_CURVE( aNode ) {

                };

        std::shared_ptr<EDMD_CURVE> m_point;
        std::shared_ptr<EDMD_POINT> m_vector;
    };


    enum class EDMD_BSPLINE_CURVE_FORM
    {
        POLYLINE_FORM = 0,
        CIRCULAR_ARC = 1,
        ELLIPTIC_ARC = 2,
        PARABOLIC_ARC = 3,
        HYPERBOLIC_ARC = 4,
        UNSPECIFIED
    };


    class EDMD_BSPLINE_CURVE : public EDMD_CURVE
    {
    public:
        EDMD_BSPLINE_CURVE( wxXmlNode& aNode ) :
                EDMD_CURVE( aNode ) {

                };

        int                                      m_degree;
        std::vector<std::shared_ptr<EDMD_POINT>> m_controlPointsList;
        EDMD_BSPLINE_CURVE_FORM                  m_curveForm = EDMD_BSPLINE_CURVE_FORM::UNSPECIFIED;
        bool                                     m_closedCurve;
        bool                                     m_selfIntersect;
    };

    class EDMD_COMPOSITE_CURVE : public EDMD_CURVE
    {
    public:
        EDMD_COMPOSITE_CURVE( wxXmlNode& aNode ) : EDMD_CURVE( aNode )
        {
            wxXmlNode* child = aNode.GetChildren();

            std::vector<wxString> refnames;
            while( child )
            {
                const wxString content = child->GetNodeContent();
                const wxString name = child->GetName();

                if( name == "d2:Curve" )
                {
                    m_curve.emplace_back();
                    refnames.push_back( content );
                }

                child = child->GetNext();
            }

            int i = 0;
            for( wxString& curveName : refnames )
            {
                m_pointsToLink[curveName] = (std::shared_ptr<EDMD_OBJECT>*) ( &( m_curve[i++] ) );
            }
        };

        std::vector<std::shared_ptr<EDMD_CURVE>> m_curve;
    };


    class EDMD_POLYLINE : public EDMD_CURVE
    {
    public:
        EDMD_POLYLINE( wxXmlNode& aNode ) : EDMD_CURVE( aNode )
        {
            wxXmlNode* child = aNode.GetChildren();

            std::vector<wxString> refnames;
            while( child )
            {
                const wxString content = child->GetNodeContent();
                const wxString name = child->GetName();

                if( name == "d2:Point" )
                {
                    //m_point.emplace_back();
                    //refnames.push_back( content );
                    m_pointRef.push_back( content );
                }

                child = child->GetNext();
            }

            int i = 0;
            /*
                for ( wxString& pointName : refnames )
                {
                    m_pointsToLink[ pointName ] = ( std::shared_ptr<EDMD_OBJECT>* ) ( &( m_point[i++] ) );
                    std::cout << m_point.size() << std::endl;
                    std::cout << "Saving " << pointName << " : " << m_pointsToLink[ pointName ] << " (" << m_pointsToLink.size() << ")" << std::endl;
                }*/
        };

        std::vector<std::shared_ptr<EDMD_POINT>> m_point;


        bool Resolve( IDX_REGISTRY& aRegistry )
        {
            bool result = EDMD_CURVE::Resolve( aRegistry );
            result &= resolve<EDMD_POINT>( m_pointRef, m_point, aRegistry );
            return result;
        };

    private:
        std::vector<wxString> m_pointRef;
    };


    class EDMD_CURVESET2D : public idx_pdm::EDMD_SHAPE_DESCRIPTION
    {
    public:
        EDMD_CURVESET2D() : idx_pdm::EDMD_SHAPE_DESCRIPTION() {};
        EDMD_CURVESET2D( wxXmlNode& aNode ) : idx_pdm::EDMD_SHAPE_DESCRIPTION( aNode )
        {
            wxXmlNode* child = aNode.GetChildren();

            while( child )
            {
                const wxString content = child->GetNodeContent();
                const wxString name = child->GetName();

                if( name == "d2:LowerBound" )
                {
                    m_lowerBound = idx_property::EDMD_LENGTH_PROPERTY( *child );
                }
                else if( name == "d2:UpperBound" )
                {
                    m_upperBound = idx_property::EDMD_LENGTH_PROPERTY( *child );
                }
                else if( name == "d2:DetailedGeometricModelElement" )
                {
                    m_modelElementRef.push_back( content );
                }
                child = child->GetNext();
            }
        };

        bool Resolve( IDX_REGISTRY& aRegistry )
        {
            bool result = true;
            result &= resolve<EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT>( m_modelElementRef,
                                                                      m_modelElement, aRegistry );
            return result;
        };


        idx_property::EDMD_LENGTH_PROPERTY                                  m_lowerBound;
        idx_property::EDMD_LENGTH_PROPERTY                                  m_upperBound;
        std::vector<std::shared_ptr<EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT>> m_modelElement;

    private:
        std::vector<wxString> m_modelElementRef;
    };


    class EDMD_ARC : public EDMD_CURVE
    {
    public:
        EDMD_ARC( wxXmlNode& aNode ) : EDMD_CURVE( aNode )
        {
            wxXmlNode* child = aNode.GetChildren();

            while( child )
            {
                const wxString content = child->GetNodeContent();
                const wxString name = child->GetName();

                if( name == "d2:StartPoint" )
                    m_startPointRef = content;
                if( name == "d2:EndPoint" )
                    m_endPointRef = content;
                if( name == "d2:IncludeAngle" )
                    m_includeAngle = idx_property::EDMD_ANGLE_PROPERTY( *child );

                child = child->GetNext();
            }
        };


        bool Resolve( IDX_REGISTRY& aRegistry )
        {
            bool result = true;

            result &= resolve<EDMD_POINT>( m_startPointRef, &m_startPoint, aRegistry );
            result &= resolve<EDMD_POINT>( m_endPointRef, &m_endPoint, aRegistry );
            return result;
        }

        std::shared_ptr<EDMD_POINT>        GetStart() { return m_startPoint; }
        std::shared_ptr<EDMD_POINT>        GetEnd() { return m_endPoint; }
        idx_property::EDMD_ANGLE_PROPERTY& GetIncludeAngle() { return m_includeAngle; }

        std::shared_ptr<EDMD_POINT>       m_startPoint;
        std::shared_ptr<EDMD_POINT>       m_endPoint;
        idx_property::EDMD_ANGLE_PROPERTY m_includeAngle;

    private:
        wxString m_startPointRef;
        wxString m_endPointRef;
    };


    class EDMD_ELLIPSE : public EDMD_CURVE
    {
    public:
        EDMD_ELLIPSE( wxXmlNode& aNode ) :
                m_semiMinor( aNode ), m_semiMajor( aNode ), EDMD_CURVE( aNode ) {

                };

        std::shared_ptr<EDMD_POINT>        m_center;
        idx_property::EDMD_LENGTH_PROPERTY m_semiMinor;
        idx_property::EDMD_LENGTH_PROPERTY m_semiMajor;
    };


    class EDMD_POINT : public EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT
    {
    public:
        EDMD_POINT() : EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT() {};

        EDMD_POINT( wxXmlNode& aNode ) : EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT( aNode ) {};

        virtual double GetX() const { return 0; };
        virtual double GetY() const { return 0; };

        virtual ~EDMD_POINT() = default; // Ensures polymorphic behavior
    };

    class EDMD_CIRCLE : public EDMD_CURVE
    {
    public:
        EDMD_CIRCLE( wxXmlNode& aNode ) : EDMD_CURVE( aNode ) {};

        virtual bool Resolve( IDX_REGISTRY& aRegistry )
        {
            return EDMD_CURVE::Resolve( aRegistry );
        };

        virtual std::shared_ptr<EDMD_POINT> GetCenter() const
        {
            return std::make_shared<EDMD_POINT>();
        };
        virtual double GetRadius() const { return 0; };
    };

    class EDMD_CIRCLE_3POINTS : public EDMD_CIRCLE
    {
    public:
        EDMD_CIRCLE_3POINTS( wxXmlNode& aNode ) :
                EDMD_CIRCLE( aNode ) {

                };

        std::shared_ptr<EDMD_POINT> m_p1;
        std::shared_ptr<EDMD_POINT> m_p2;
        std::shared_ptr<EDMD_POINT> m_p3;
    };


    class EDMD_CIRCLE_CENTER : public EDMD_CIRCLE
    {
    public:
        EDMD_CIRCLE_CENTER( wxXmlNode& aNode ) : m_diameter( aNode ), EDMD_CIRCLE( aNode )
        {
            wxXmlNode* child = aNode.GetChildren();

            wxString centerName;

            while( child )
            {
                const wxString content = child->GetNodeContent();
                const wxString name = child->GetName();

                if( name == "d2:Center" )
                {
                    m_centerRef = content;
                }
                else if( name == "d2:Diameter" )
                {
                    m_diameter = idx_property::EDMD_LENGTH_PROPERTY( *child );
                }

                child = child->GetNext();
            }
        };

        bool Resolve( IDX_REGISTRY& aRegistry )
        {
            bool result = EDMD_CIRCLE::Resolve( aRegistry );
            result &= resolve<EDMD_POINT>( m_centerRef, &m_center, aRegistry );
            return result;
        }


        std::shared_ptr<EDMD_POINT> GetCenter() const override { return m_center; };
        double GetRadius() const override { return m_diameter.m_value / 2 * 1e6; };

        std::shared_ptr<EDMD_POINT>        m_center;
        idx_property::EDMD_LENGTH_PROPERTY m_diameter;

    private:
        wxString m_centerRef;
    };


    class EDMD_OFFSET_CURVE : public EDMD_CURVE
    {
    public:
        EDMD_OFFSET_CURVE( wxXmlNode& aNode ) :
                EDMD_CURVE( aNode ) {

                };


        std::shared_ptr<EDMD_CURVE>        m_basisCurve;
        idx_property::EDMD_LENGTH_PROPERTY m_distance;
        bool                               m_selfIntersects = false;
    };

    class EDMD_PARABOLA : public EDMD_CURVE
    {
    public:
        EDMD_PARABOLA( wxXmlNode& aNode ) :
                EDMD_CURVE( aNode ) {

                };


        std::shared_ptr<EDMD_POINT> m_focus;
        std::shared_ptr<EDMD_POINT> m_parabola;
    };


    class EDMD_POINT_ON_CURVE_INTERSECTION : public EDMD_POINT
    {
    public:
        EDMD_POINT_ON_CURVE_INTERSECTION( wxXmlNode& aNode ) :
                EDMD_POINT( aNode ) {

                };

        std::shared_ptr<EDMD_CURVE> m_curve;
        std::shared_ptr<EDMD_CURVE> m_intersector;
    };


    class EDMD_CARTESIAN_POINT : public EDMD_POINT
    {
    public:
        EDMD_CARTESIAN_POINT( wxXmlNode& aNode ) : EDMD_POINT( aNode )
        {
            wxXmlNode* child = aNode.GetChildren();

            while( child )
            {
                const wxString content = child->GetNodeContent();
                const wxString name = child->GetName();

                if( name == "d2:X" )
                    m_x = idx_property::EDMD_LENGTH_PROPERTY( *child );
                if( name == "d2:Y" )
                    m_y = idx_property::EDMD_LENGTH_PROPERTY( *child );
                if( name == "d2:Z" )
                    m_z = idx_property::EDMD_LENGTH_PROPERTY( *child );
                else
                {
                }
                child = child->GetNext();
            }
        };

        double GetX() const override { return m_x.m_value * 1e6; };
        double GetY() const override { return m_y.m_value * 1e6; };


        idx_property::EDMD_LENGTH_PROPERTY m_x;
        idx_property::EDMD_LENGTH_PROPERTY m_y;
        idx_property::EDMD_LENGTH_PROPERTY m_z;
    };


    class EDMD_POINT_ON_CURVE_DISTANCE : public EDMD_POINT
    {
    public:
        EDMD_POINT_ON_CURVE_DISTANCE( wxXmlNode& aNode ) :
                m_distance( aNode ), EDMD_POINT( aNode ) {

                };

        std::shared_ptr<EDMD_CURVE>        m_curve;
        std::shared_ptr<EDMD_POINT>        m_start;
        idx_property::EDMD_LENGTH_PROPERTY m_distance;
    };


    } // namespace idx_geometry
    // namespace idx_geometry

#endif