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

#include <wx/xml/xml.h>
#include <idx/foundation.h>
#include <idx/property.h>
#include <idx/pdm.h>
#include <memory>
#include <vector>

namespace idx_geometry
{

// --------------------------------------------------------------------------
// Class declarations (no function bodies)
// --------------------------------------------------------------------------

class EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT();
    EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT( IDX_XML_NODE& aNode, bool aTopLevel );
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    virtual bool Parse( IDX_XML_NODE& aNode ) override;
};

class EDMD_CURVE : public EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT
{
public:
    EDMD_CURVE( IDX_XML_NODE& aNode, bool aTopLevel );
    virtual ~EDMD_CURVE();
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    virtual bool Parse( IDX_XML_NODE& aNode ) override;

    idx_property::EDMD_LENGTH_PROPERTY m_thickness;
};

// --- Other derived classes ---

class EDMD_HYPERBOLA : public EDMD_CURVE
{
public:
    EDMD_HYPERBOLA( IDX_XML_NODE& aNode );

    std::shared_ptr<EDMD_POINT>        m_center;
    idx_property::EDMD_LENGTH_PROPERTY m_semiMajor;
    idx_property::EDMD_LENGTH_PROPERTY m_semiMinor;
};

class EDMD_TRIMMED_CURVE : public EDMD_CURVE
{
public:
    EDMD_TRIMMED_CURVE( IDX_XML_NODE& aNode );
    std::shared_ptr<EDMD_CURVE> m_curve;
    std::shared_ptr<EDMD_POINT> m_start;
    std::shared_ptr<EDMD_POINT> m_end;
};

class EDMD_LINE : public EDMD_CURVE
{
public:
    EDMD_LINE( IDX_XML_NODE& aNode );
    std::shared_ptr<EDMD_CURVE> m_point;
    std::shared_ptr<EDMD_POINT> m_vector;
};

enum class EDMD_BSPLINE_CURVE_FORM
{
    POLYLINE_FORM = 0,
    CIRCULAR_ARC,
    ELLIPTIC_ARC,
    PARABOLIC_ARC,
    HYPERBOLIC_ARC,
    UNSPECIFIED
};

class EDMD_BSPLINE_CURVE : public EDMD_CURVE
{
public:
    EDMD_BSPLINE_CURVE( IDX_XML_NODE& aNode );

    int                                      m_degree;
    std::vector<std::shared_ptr<EDMD_POINT>> m_controlPointsList;
    EDMD_BSPLINE_CURVE_FORM                  m_curveForm;
    bool                                     m_closedCurve;
    bool                                     m_selfIntersect;
};

class EDMD_COMPOSITE_CURVE : public EDMD_CURVE
{
public:
    EDMD_COMPOSITE_CURVE( IDX_XML_NODE& aNode );
    std::vector<std::shared_ptr<EDMD_CURVE>> m_curve;
};

class EDMD_POLYLINE : public EDMD_CURVE
{
public:
    EDMD_POLYLINE( IDX_XML_NODE& aNode, bool aTopLevel );
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    virtual bool Parse( IDX_XML_NODE& aNode ) override;

    std::vector<std::shared_ptr<EDMD_POINT>> m_point;

private:
    std::vector<std::string> m_pointRef;
};

class EDMD_CURVESET2D : public idx_pdm::EDMD_SHAPE_DESCRIPTION
{
public:
    EDMD_CURVESET2D();
    EDMD_CURVESET2D( IDX_XML_NODE& aNode );
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;

    idx_property::EDMD_LENGTH_PROPERTY                                  m_lowerBound;
    idx_property::EDMD_LENGTH_PROPERTY                                  m_upperBound;
    std::vector<std::shared_ptr<EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT>> m_modelElement;

private:
    std::vector<std::string> m_modelElementRef;
};

class EDMD_ARC : public EDMD_CURVE
{
public:
    EDMD_ARC( IDX_XML_NODE& aNode );
    virtual bool                       Resolve( IDX_REGISTRY& aRegistry ) override;
    std::shared_ptr<EDMD_POINT>        GetStart();
    std::shared_ptr<EDMD_POINT>        GetEnd();
    idx_property::EDMD_ANGLE_PROPERTY& GetIncludeAngle();

private:
    std::shared_ptr<EDMD_POINT>       m_startPoint;
    std::shared_ptr<EDMD_POINT>       m_endPoint;
    idx_property::EDMD_ANGLE_PROPERTY m_includeAngle;
    std::string                       m_startPointRef;
    std::string                       m_endPointRef;
};

class EDMD_ELLIPSE : public EDMD_CURVE
{
public:
    EDMD_ELLIPSE( IDX_XML_NODE& aNode );
    std::shared_ptr<EDMD_POINT>        m_center;
    idx_property::EDMD_LENGTH_PROPERTY m_semiMinor;
    idx_property::EDMD_LENGTH_PROPERTY m_semiMajor;
};

class EDMD_POINT : public EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT
{
public:
    EDMD_POINT();
    EDMD_POINT( IDX_XML_NODE& aNode, bool aTopLevel );
    virtual double GetX() const;
    virtual double GetY() const;
    virtual ~EDMD_POINT();
};

class EDMD_CIRCLE : public EDMD_CURVE
{
public:
    EDMD_CIRCLE( IDX_XML_NODE& aNode );
    virtual bool                        Resolve( IDX_REGISTRY& aRegistry ) override;
    virtual std::shared_ptr<EDMD_POINT> GetCenter() const;
    virtual double                      GetRadius() const;
};

class EDMD_CIRCLE_3POINTS : public EDMD_CIRCLE
{
public:
    EDMD_CIRCLE_3POINTS( IDX_XML_NODE& aNode );
    std::shared_ptr<EDMD_POINT> m_p1;
    std::shared_ptr<EDMD_POINT> m_p2;
    std::shared_ptr<EDMD_POINT> m_p3;
};

class EDMD_CIRCLE_CENTER : public EDMD_CIRCLE
{
public:
    EDMD_CIRCLE_CENTER( IDX_XML_NODE& aNode );
    virtual bool                Resolve( IDX_REGISTRY& aRegistry ) override;
    std::shared_ptr<EDMD_POINT> GetCenter() const override;
    double                      GetRadius() const override;

private:
    std::shared_ptr<EDMD_POINT>        m_center;
    idx_property::EDMD_LENGTH_PROPERTY m_diameter;
    std::string                        m_centerRef;
};

class EDMD_OFFSET_CURVE : public EDMD_CURVE
{
public:
    EDMD_OFFSET_CURVE( IDX_XML_NODE& aNode );
    std::shared_ptr<EDMD_CURVE>        m_basisCurve;
    idx_property::EDMD_LENGTH_PROPERTY m_distance;
    bool                               m_selfIntersects;
};

class EDMD_PARABOLA : public EDMD_CURVE
{
public:
    EDMD_PARABOLA( IDX_XML_NODE& aNode );
    std::shared_ptr<EDMD_POINT> m_focus;
    std::shared_ptr<EDMD_POINT> m_parabola;
};

class EDMD_POINT_ON_CURVE_INTERSECTION : public EDMD_POINT
{
public:
    EDMD_POINT_ON_CURVE_INTERSECTION( IDX_XML_NODE& aNode, bool aTopLevel );
    std::shared_ptr<EDMD_CURVE> m_curve;
    std::shared_ptr<EDMD_CURVE> m_intersector;
    bool                        Parse( IDX_XML_NODE& aNode ) override { return false; };
};

class EDMD_CARTESIAN_POINT : public EDMD_POINT
{
public:
    EDMD_CARTESIAN_POINT( IDX_XML_NODE& aNode, bool aTOpLevel );
    bool   Parse( IDX_XML_NODE& aNode ) override;
    double GetX() const override;
    double GetY() const override;

private:
    idx_property::EDMD_LENGTH_PROPERTY m_x;
    idx_property::EDMD_LENGTH_PROPERTY m_y;
    idx_property::EDMD_LENGTH_PROPERTY m_z;
};

class EDMD_POINT_ON_CURVE_DISTANCE : public EDMD_POINT
{
public:
    EDMD_POINT_ON_CURVE_DISTANCE( IDX_XML_NODE& aNode, bool aTopLevel );
    bool                               Parse( IDX_XML_NODE& aNode ) override { return false; };
    std::shared_ptr<EDMD_CURVE>        m_curve;
    std::shared_ptr<EDMD_POINT>        m_start;
    idx_property::EDMD_LENGTH_PROPERTY m_distance;
};

} // namespace idx_geometry

#endif // IDX_GEOMETRY_HEADER_H
