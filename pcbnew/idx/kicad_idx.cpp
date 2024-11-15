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

#include <idx/kicad_idx.h>
#include <idx/idx_reader.h>
#include <lset.h>
#include <layer_ids.h>
#include <zone.h>
#include <tools/convert_tool.h>


/*
bool read_IDX( wxString aFilename, BOARD_COMMIT* aCommit )
{
    wxXmlDocument doc;

    if( !doc.Load( aFilename ) )
        return false;

    IDX_HOLDER* idxHolder = new IDX_HOLDER( doc );

    if( aCommit )
    {
        idxHolder->AddElements( *aCommit );
    }


    return true;
}
*/
KICAD_IDX::KICAD_IDX( wxString aFilename, BOARD* aBoard, BOARD_COMMIT* aCommit ) :
        m_board( aBoard ), m_commit( aCommit )
{
    wxXmlDocument doc;

    if( !doc.Load( aFilename ) )
        return;

    m_holder = std::make_unique<IDX_HOLDER>( doc );
};


void KICAD_IDX::AddAll()
{
    for( auto const& x : m_holder->m_stratums )
    {
        std::cout << "------------" << std::endl;
        for( auto const& y : x.second->m_shapeElement )
        {
            std::shared_ptr<idx_geometry::EDMD_CURVESET2D> ptr =
                    std::dynamic_pointer_cast<idx_geometry::EDMD_CURVESET2D>( y->m_definingShape );

            if( ptr )
                AddElement( *ptr, LSET( { Edge_Cuts } ) );
        }
    }


    for( auto const& x : m_holder->m_keeps )
    {
        for( auto const& y : x.second->m_shapeElement )
        {
            std::shared_ptr<idx_geometry::EDMD_CURVESET2D> ptr =
                    std::dynamic_pointer_cast<idx_geometry::EDMD_CURVESET2D>( y->m_definingShape );

            if( ptr )
            {
                ZONE* zone = new ZONE( m_board );
                zone->SetZoneName( x.second->m_id );
                zone->SetLayer( F_Cu );
                zone->SetIsRuleArea( true );
                zone->SetHatchStyle( ZONE_BORDER_DISPLAY_STYLE::DIAGONAL_FULL );

                zone->SetDoNotAllowCopperPour( false );
                zone->SetDoNotAllowVias( false );
                zone->SetDoNotAllowTracks( false );
                zone->SetDoNotAllowFootprints( false );
                zone->SetDoNotAllowPads( false );

                for( auto& purpose : x.second->m_purpose )
                {
                    if( purpose == idx_pdm::KEEP_CONSTRAINT_PURPOSE::COMPONENT_PLACEMENT )
                    {
                        zone->SetDoNotAllowFootprints( true );
                    }
                    if( purpose == idx_pdm::KEEP_CONSTRAINT_PURPOSE::ROUTE )
                    {
                        zone->SetDoNotAllowPads( true );
                        zone->SetDoNotAllowTracks( true );
                        zone->SetDoNotAllowCopperPour( true );
                    }
                    if( purpose == idx_pdm::KEEP_CONSTRAINT_PURPOSE::VIA )
                    {
                        zone->SetDoNotAllowVias( true );
                    }
                }

                for( auto const& z : ptr->m_modelElement )
                {
                    auto polyline = std::dynamic_pointer_cast<idx_geometry::EDMD_POLYLINE>( z );

                    if( polyline )
                    {
                        zone->SetIsRuleArea( true );
                        for( std::shared_ptr<idx_geometry::EDMD_POINT> p : polyline->m_point )
                        {
                            if( p )
                            {
                                zone->AppendCorner( VECTOR2D( p->GetX(), p->GetY() ), -1 );
                            }
                        }
                    }


                    auto arc = std::dynamic_pointer_cast<idx_geometry::EDMD_ARC>( z );

                    if( arc )
                    {
                        VECTOR2I start =
                                VECTOR2I( arc->GetStart()->GetX(), arc->GetStart()->GetY() );
                        VECTOR2I end = VECTOR2I( arc->GetEnd()->GetX(), arc->GetEnd()->GetY() );
                        VECTOR2I mid = ( start + end ) / 2;


                        double theta = arc->GetIncludeAngle().m_value * 3.1415 / 180;

                        double L = start.Distance( end ) / 2;
                        double R = L / sin( theta / 2 );
                        double D = L / tan( theta / 2 );

                        mid -= ( end - mid ).Perpendicular().Resize( R - D );
                        SHAPE_ARC* s = new SHAPE_ARC( start, mid, end, 0 );

                        SHAPE_LINE_CHAIN* buffer = new SHAPE_LINE_CHAIN( *s );

                        for( const VECTOR2I p : buffer->CPoints() )
                        {
                            zone->AppendCorner( VECTOR2D( p.x, p.y ), -1 );
                        }
                    }


                    auto circle = std::dynamic_pointer_cast<idx_geometry::EDMD_CIRCLE>( z );

                    if( circle )
                    {
                        VECTOR2I center = VECTOR2I( circle->GetCenter()->GetX(),
                                                    circle->GetCenter()->GetY() );
                        int      radius = circle->GetRadius();

                        VECTOR2I start = center + VECTOR2I( 0, radius );
                        VECTOR2I mid = center - VECTOR2I( 0, radius );

                        SHAPE_ARC* s = new SHAPE_ARC( start, mid, start, 0 );

                        SHAPE_LINE_CHAIN* buffer = new SHAPE_LINE_CHAIN( *s );

                        for( const VECTOR2I p : buffer->CPoints() )
                        {
                            zone->AppendCorner( VECTOR2D( p.x, p.y ), -1 );
                        }
                    }
                    m_commit->Add( zone );
                }
            }
        }
    }
    m_commit->Push( "Import IDX" );
}


BOARD_ITEM* KICAD_IDX::AddElement( idx_geometry::EDMD_CURVESET2D& aSet, LSET aLset )
{
    PCB_GROUP* group = nullptr;

    int i = 0;

    PCB_SHAPE* s = new PCB_SHAPE();
    group = new PCB_GROUP( s );
    group->SetName( aSet.m_id );
    m_commit->Add( group );
    for( std::shared_ptr<idx_geometry::EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT>& model :
         aSet.m_modelElement )
    {
        BOARD_ITEM* item = nullptr;
        auto        polyline = std::dynamic_pointer_cast<idx_geometry::EDMD_POLYLINE>( model );
        auto        circle = std::dynamic_pointer_cast<idx_geometry::EDMD_CIRCLE>( model );
        auto        arc = std::dynamic_pointer_cast<idx_geometry::EDMD_ARC>( model );

        if( polyline )
        {
            item = AddElement( *polyline, aLset );
        }
        if( arc )
        {
            item = AddElement( *arc, aLset );
        }
        if( circle )
        {
            item = AddElement( *circle, aLset );
        }
        m_commit->Push( "aaa" );

        if( item )
        {
            group->AddItem( item );
            m_commit->Stage( item, CHT_GROUP );
            m_commit->Add( item );
        }
    }

    return group;
}

PCB_SHAPE KICAD_IDX::ConvertToKiCadShape( idx_geometry::EDMD_ARC& aArc )
{
    PCB_SHAPE s( nullptr, SHAPE_T::ARC );
    VECTOR2I  start = VECTOR2I( aArc.GetStart()->GetX(), aArc.GetStart()->GetY() );
    VECTOR2I  end = VECTOR2I( aArc.GetEnd()->GetX(), aArc.GetEnd()->GetY() );
    VECTOR2I  mid = ( start + end );

    double theta = aArc.GetIncludeAngle().m_value * 3.1415 / 180;

    double L = start.Distance( end ) / 2;
    double R = L / sin( theta / 2 );
    double D = L / tan( theta / 2 );

    mid -= ( end - mid ).Perpendicular().Resize( R - D );
    s.SetArcGeometry( start, mid, end );

    return s;
}


SHAPE_ARC KICAD_IDX::ConvertToKiCadArc( idx_geometry::EDMD_ARC& aArc )
{
    VECTOR2I start = VECTOR2I( aArc.GetStart()->GetX(), aArc.GetStart()->GetY() );
    VECTOR2I end = VECTOR2I( aArc.GetEnd()->GetX(), aArc.GetEnd()->GetY() );
    VECTOR2I mid = ( start + end );

    double theta = aArc.GetIncludeAngle().m_value * 3.1415 / 180;

    double L = start.Distance( end ) / 2;
    double R = L / sin( theta / 2 );
    double D = L / tan( theta / 2 );

    mid -= ( end - mid ).Perpendicular().Resize( R - D );

    return SHAPE_ARC( start, mid, end, 0 );
}


VECTOR2I KICAD_IDX::ArcMidPoint( idx_geometry::EDMD_ARC& aArc )
{
    VECTOR2I start = VECTOR2I( aArc.GetStart()->GetX(), aArc.GetStart()->GetY() );
    VECTOR2I end = VECTOR2I( aArc.GetEnd()->GetX(), aArc.GetEnd()->GetY() );
    VECTOR2I mid = ( start + end );

    double theta = aArc.GetIncludeAngle().m_value * 3.1415 / 180;

    double L = start.Distance( end ) / 2;
    double R = L / sin( theta / 2 );
    double D = L / tan( theta / 2 );

    mid -= ( end - mid ).Perpendicular().Resize( R - D );


    return mid;
}

BOARD_ITEM* KICAD_IDX::AddElement( idx_geometry::EDMD_ARC& aArc, LSET aLset )
{
    PCB_SHAPE* s = new PCB_SHAPE( nullptr, SHAPE_T::ARC );

    VECTOR2I start = VECTOR2I( aArc.GetStart()->GetX(), aArc.GetStart()->GetY() );
    VECTOR2I end = VECTOR2I( aArc.GetEnd()->GetX(), aArc.GetEnd()->GetY() );
    VECTOR2I mid = ( start + end ) / 2;


    double theta = aArc.GetIncludeAngle().m_value * 3.1415 / 180;

    double L = start.Distance( end ) / 2;
    double R = L / sin( theta / 2 );
    double D = L / tan( theta / 2 );

    mid -= ( end - mid ).Perpendicular().Resize( R - D );
    s->SetArcGeometry( start, mid, end );


    AddItem( s, aLset );

    PCB_GROUP* group = new PCB_GROUP( s );
    group->SetName( aArc.m_id );
    m_commit->Add( group );
    group->AddItem( s );
    m_commit->Stage( s, CHT_GROUP );
    m_commit->Add( s );

    return group;
}


BOARD_ITEM* KICAD_IDX::AddElement( idx_geometry::EDMD_CIRCLE& aCircle, LSET aLset )
{
    if( aCircle.GetCenter() )
    {
        PCB_SHAPE* s = new PCB_SHAPE( nullptr, SHAPE_T::CIRCLE );
        s->SetCenter( VECTOR2I( aCircle.GetCenter()->GetX(), aCircle.GetCenter()->GetY() ) );
        s->SetRadius( aCircle.GetRadius() );
        AddItem( s, aLset );
        PCB_GROUP* group = new PCB_GROUP( s );
        group->SetName( aCircle.m_id );
        m_commit->Add( group );
        group->AddItem( s );
        m_commit->Stage( s, CHT_GROUP );
        m_commit->Add( s );
        return group;
    }
    return nullptr;
}


BOARD_ITEM* KICAD_IDX::AddElement( idx_geometry::EDMD_POLYLINE& aPolyline, LSET aLset )
{
    std::vector<VECTOR2I> polypoints;

    for( std::shared_ptr<idx_geometry::EDMD_POINT> p : aPolyline.m_point )
    {
        if( p )
        {
            polypoints.emplace_back( p->GetX(), p->GetY() );
        }
    }

    PCB_SHAPE* s = new PCB_SHAPE();

    PCB_GROUP* group = new PCB_GROUP( s );
    group->SetName( aPolyline.m_id );
    m_commit->Add( group );

    for( size_t i = 0; i < polypoints.size() - 1; i++ )
    {
        PCB_SHAPE* s = new PCB_SHAPE();

        AddItem( s, aLset );
        s->SetWidth( aPolyline.m_thickness.m_value * 1e6 );
        s->SetStart( polypoints[i] );
        s->SetEnd( polypoints[i + 1] );
        group->AddItem( s );
        m_commit->Stage( s, CHT_GROUP );
        m_commit->Add( s );
    }

    return group;
}


void KICAD_IDX::AddItem( BOARD_ITEM* aItem, LSET aLset )
{
    for( PCB_LAYER_ID id : aLset.Seq() )
    {
        aItem->SetLayer( id );
    }
}
