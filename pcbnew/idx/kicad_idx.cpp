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
#include <board_design_settings.h>
#include <layer_range.h>    


KICAD_IDX::KICAD_IDX( wxString aFilename, BOARD* aBoard, BOARD_COMMIT* aCommit ) :
        m_board( aBoard ), m_commit( aCommit )
{
    wxXmlDocument doc;

    if( !doc.Load( aFilename ) )
        return;

    m_holder = std::make_unique<IDX_HOLDER>( doc );

    BOARD_DESIGN_SETTINGS& dsnSettings = m_board->GetDesignSettings();
    BOARD_STACKUP stackup = dsnSettings.GetStackupDescriptor();
    stackup.SynchronizeWithBoard( &dsnSettings );
    std::vector<BOARD_STACKUP_ITEM*> Llist = stackup.GetList();
    std::reverse( Llist.begin(), Llist.end( ) );

    int currentThickness = 0;

    for ( BOARD_STACKUP_ITEM* bsi : Llist )
    {       
        if ( bsi )
        {
            if ( bsi->GetBrdLayerId() == B_Cu )
            {
                currentThickness = 0;
            }

            if ( currentThickness >= 0 && bsi->GetType() == BOARD_STACKUP_ITEM_TYPE::BS_ITEM_TYPE_COPPER )
            {
                m_stackup.push_back( { bsi->GetBrdLayerId(), currentThickness } );
            }

            currentThickness += bsi->GetThickness();
        }
    }
    
};

void KICAD_IDX::Report( wxString aMsg, SEVERITY aSev )
{
    std::string prefix;
    if ( aSev == RPT_SEVERITY_INFO )
        return;
    switch ( aSev )
    {
        case RPT_SEVERITY_UNDEFINED : prefix = "UNDEFINED: "; break;
        case RPT_SEVERITY_INFO :      prefix = "INFO: "; break;
        case RPT_SEVERITY_EXCLUSION : prefix = "EXCLUSION: "; break;
        case RPT_SEVERITY_ACTION :    prefix = "ACTION: "; break;
        case RPT_SEVERITY_WARNING :   prefix = "WARNING: "; break;
        case RPT_SEVERITY_ERROR :     prefix = "ERROR: "; break;
        default :                     prefix = "UNDEFINED: "; break;
    }

    std::cout << prefix << aMsg << std::endl;
}


std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> KICAD_IDX::GetAllItemInstances()
{
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> result;


    for( auto& item : m_holder->m_items )
    {
        std::cout << item->m_id << " - ";
        for( auto& instance : item->m_itemInstance )
        {
            std::cout << instance->m_name << " - ";
            result.push_back( instance );
        }
        std::cout << std::endl;
    }

    return result;
}

void KICAD_IDX::AddSelected( std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>>& aVector )
{

    for( auto instance : aVector )
    {
        AddElement( *instance, {Eco1_User} );
    }
}



void KICAD_IDX::AddAll()
{

    std::cout << "ADDING BOARD EDGE ELEMENTS " << m_holder->GetBoardOutlineElements().size() << std::endl;
    for ( auto edge : m_holder->GetBoardOutlineElements() )
    {
        AddElement( *edge, Edge_Cuts );
    }

    std::cout << "ADDING INSTANCES" << std::endl;
    for( auto instance : GetAllItemInstances() )
        AddElement( *instance, {Eco2_User} );


    for( auto const& x : m_holder->m_keeps )
    {
        for( auto const& y : x->m_shapeElement )
        {
            std::shared_ptr<idx_geometry::EDMD_CURVESET2D> ptr =
                    std::dynamic_pointer_cast<idx_geometry::EDMD_CURVESET2D>( y->m_definingShape );

            if( ptr )
            {                
                LSET layers = GetLayers( *ptr );
                if (layers.Seq().size() < 1 )
                {
                    continue;
                }

                ZONE* zone = new ZONE( m_board );
                zone->SetZoneName( x->m_id );
                zone->SetLayerSet( layers );
                zone->SetIsRuleArea( true );
                zone->SetHatchStyle( ZONE_BORDER_DISPLAY_STYLE::DIAGONAL_FULL );

                zone->SetDoNotAllowZoneFills( false );
                zone->SetDoNotAllowVias( false );
                zone->SetDoNotAllowTracks( false );
                zone->SetDoNotAllowFootprints( false );
                zone->SetDoNotAllowPads( false );

                for( auto& purpose : x->m_purpose )
                {
                    if( purpose == idx_pdm::KEEP_CONSTRAINT_PURPOSE::COMPONENT_PLACEMENT )
                    {
                        zone->SetDoNotAllowFootprints( true );
                    }
                    if( purpose == idx_pdm::KEEP_CONSTRAINT_PURPOSE::ROUTE )
                    {
                        zone->SetDoNotAllowPads( true );
                        zone->SetDoNotAllowTracks( true );
                        zone->SetDoNotAllowZoneFills( true );
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
                                zone->AppendCorner( VECTOR2D( p->GetX(), -p->GetY() ), -1 );
                            }
                        }
                    }


                    auto arc = std::dynamic_pointer_cast<idx_geometry::EDMD_ARC>( z );

                    if( arc )
                    {
                        VECTOR2I start = VECTOR2I( arc->GetStart()->GetX(), -arc->GetStart()->GetY() );
                        VECTOR2I end = VECTOR2I( arc->GetEnd()->GetX(), -arc->GetEnd()->GetY() );
                        VECTOR2I mid = ArcMidPoint( *arc );

                        SHAPE_ARC* s = new SHAPE_ARC( start, mid, end, 0);

                        SHAPE_LINE_CHAIN* buffer = new SHAPE_LINE_CHAIN( *s );

                        for( const VECTOR2I p : buffer->CPoints() )
                        {
                            zone->AppendCorner( VECTOR2D( p.x, p.y ), -1 );
                        }
                    }


                    auto circle = std::dynamic_pointer_cast<idx_geometry::EDMD_CIRCLE>( z );

                    if( circle )
                    {
                        VECTOR2I center = VECTOR2I( circle->GetCenter()->GetX(), -circle->GetCenter()->GetY() );
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



BOARD_ITEM* KICAD_IDX::AddElement( idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE aShape )
{
    int i = 0;

    PCB_LAYER_ID layer = Eco2_User;

    switch( aShape.m_type )
    {
    case idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE::TYPE::RIGID_AREA:
    case idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE::TYPE::FLEXIBLE_AREA:
    case idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE::TYPE::STIFFENER:
    case idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE::TYPE::USER_AREA:
    case idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE::TYPE::MECHANICAL:
    case idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE::TYPE::PLACEMENT_GROUP: layer = PCB_LAYER_ID::Eco2_User;
    }


    PCB_GROUP* group = new PCB_GROUP( nullptr );
    group->SetName( aShape.m_id );
    m_commit->Add( group );
    for( auto const elt : aShape.m_shapeElement )
    {
        std::shared_ptr<idx_geometry::EDMD_CURVESET2D> ptr =
                std::dynamic_pointer_cast<idx_geometry::EDMD_CURVESET2D>( elt->m_definingShape );

        if( ptr )
        {
            BOARD_ITEM* result;/*
            result = AddElement( *ptr, LSET( { layer } ) );
            if( result )
            {
                group->AddItem( result );
                m_commit->Stage( result, CHANGE_TYPE::CHT_ADD );
            }*/
        }
    }
    return nullptr;
}

PCB_SHAPE KICAD_IDX::ConvertToKiCadShape( idx_geometry::EDMD_ARC& aArc )
{

    PCB_SHAPE s( nullptr, SHAPE_T::ARC );
    VECTOR2I  end = VECTOR2I( aArc.GetStart()->GetX(), -aArc.GetStart()->GetY() );
    VECTOR2I  start = VECTOR2I( aArc.GetEnd()->GetX(), -aArc.GetEnd()->GetY() );
    VECTOR2I  mid = ArcMidPoint( aArc );
    s.SetArcGeometry( start, mid, end );

    return s;
}


VECTOR2I KICAD_IDX::ArcMidPoint( idx_geometry::EDMD_ARC& aArc )
{
    VECTOR2I start = VECTOR2I( aArc.GetStart()->GetX(), -aArc.GetStart()->GetY() );
    VECTOR2I end = VECTOR2I( aArc.GetEnd()->GetX(), -aArc.GetEnd()->GetY() );
    VECTOR2I mid = ( start + end ) / 2;

    double theta = aArc.GetIncludeAngle().m_value * 3.1415 / 180;

    double L = start.Distance( end ) / 2;
    double R = L / sin( theta / 2 );
    double D = L / tan( theta / 2 );

    mid += ( end - mid ).Perpendicular().Resize( R - D );

    return mid;
}



bool KICAD_IDX::ApplyTransformation(BOARD_ITEM* aItem, const idx_pdm::EDMD_TRANSFORMATION& aTransfo ) 
{

        VECTOR2I moveVector( aTransfo.m_tx.m_value * 1e6, -aTransfo.m_ty.m_value * 1e6 );
        
        double xx = aTransfo.m_xx;
        double xy = aTransfo.m_xy;
        double yx = aTransfo.m_yx;
        double yy = aTransfo.m_yy;

        if ( xx == -yy )
        {
            aItem->Mirror( VECTOR2I(0,0), FLIP_DIRECTION::LEFT_RIGHT );
            xx = yy;
            yx = -yx; 
        }

        if ( (xx == yy) && (xy == -yx) )
        {
            if ( xx != 1 || xy != 0 )
            {
                aItem->Rotate( VECTOR2I(0,0), EDA_ANGLE( atan2(yx, xx), RADIANS_T ) );
            }
        }
        else
        {
            return false;
        }

        aItem->Move( moveVector );
        return true;
}

BOARD_ITEM* KICAD_IDX::AddElement( idx_pdm::EDMD_ITEM_INSTANCE& aInstance, PCB_LAYER_ID aLayer )
{
    if ( !( aInstance.m_item ) )
    {
        this->Report( wxString::Format( wxT( "%s has no item" ), aInstance.m_name ) , RPT_SEVERITY_ERROR);
        return nullptr;
    }

    if ( !( aInstance.m_item->m_shape ) )
    {
        this->Report( wxString::Format( wxT( "%s - %s has no shape" ), aInstance.m_name, aInstance.m_item->m_id ) , RPT_SEVERITY_ERROR);
        return nullptr;
    }
        
    LSET layers;
    
    if ( aInstance.m_item->m_shape->m_shapeElement.size() < 1 )
    {
        this->Report( wxString::Format( wxT( "%s - %s shape has no shape element" ), aInstance.m_name, aInstance.m_item->m_id ) , RPT_SEVERITY_ERROR);
        for ( wxString ref: aInstance.m_item->m_shape->m_shapeElementRef )
        {
            std::cout << ref << std::endl;
        }
        return nullptr;
    }

    PCB_GROUP* group = new PCB_GROUP( nullptr );
    group->SetName( aInstance.m_name );
    m_commit->Add( group );
    std::cout << " Instance has elemnts: " << aInstance.m_item->m_shape->m_shapeElement.size()  << std::endl;

    for( auto const& z : aInstance.m_item->m_shape->m_shapeElement )
    {

        std::shared_ptr<idx_geometry::EDMD_CURVESET2D> curveset =
                std::dynamic_pointer_cast<idx_geometry::EDMD_CURVESET2D>( z->m_definingShape );

        if( curveset )
        {

            for ( BOARD_ITEM* item : ConvertElement(*curveset) )
            {
                item->SetLayer( aLayer );

                if ( this->ApplyTransformation( item, aInstance.m_transformation ) )
                {
                    group->AddItem( item );
                    m_commit->Stage( item, CHT_ADD );
                    m_commit->Add( item );
                    this->Report( wxString::Format( wxT( "%s - %s added" ), aInstance.m_name, aInstance.m_item->m_id ) , RPT_SEVERITY_INFO);
                }
                else
                {
                    this->Report( wxString::Format( wxT( "%s - %s Error: Rotation is incorrect" ), aInstance.m_name, aInstance.m_item->m_id ) , RPT_SEVERITY_ERROR);

                }

            }
        }
        else
        {
            this->Report( wxString::Format( wxT( "%s has no valid curveset" ), aInstance.m_name ) , RPT_SEVERITY_ERROR );
        }
    }

    return nullptr;
}



std::vector<BOARD_ITEM*> KICAD_IDX::ConvertElement(idx_geometry::EDMD_CURVESET2D& aSet)
{
    std::vector<BOARD_ITEM*> result;

    for (const auto& model : aSet.m_modelElement)
    {
        if (!model) continue;

        if (auto polyline = std::dynamic_pointer_cast<idx_geometry::EDMD_POLYLINE>(model))
        {
            auto items = ConvertElement(*polyline);
            result.insert(result.end(), std::make_move_iterator(items.begin()), std::make_move_iterator(items.end()));
        }
        else if (auto arc = std::dynamic_pointer_cast<idx_geometry::EDMD_ARC>(model))
        {
            auto items = ConvertElement(*arc);
            result.insert(result.end(), std::make_move_iterator(items.begin()), std::make_move_iterator(items.end()));
        }
        else if (auto circle = std::dynamic_pointer_cast<idx_geometry::EDMD_CIRCLE>(model))
        {
            auto items = ConvertElement(*circle);
            result.insert(result.end(), std::make_move_iterator(items.begin()), std::make_move_iterator(items.end()));
        }
    }

    return result;
}


std::vector<BOARD_ITEM*> KICAD_IDX::ConvertElement(idx_geometry::EDMD_POLYLINE& aPolyline)
{
    const auto& points = aPolyline.m_point;
    size_t num_points = points.size();
    if (num_points < 2) return {};

    std::vector<BOARD_ITEM*> result;
    result.reserve(num_points - 1);

    double width = aPolyline.m_thickness.m_value * 1e6;

    for (size_t i = 0; i < num_points - 1; ++i)
    {
        auto& p1 = points[i];
        auto& p2 = points[i + 1];
        if (!p1 || !p2) continue;

        PCB_SHAPE* s = new PCB_SHAPE();
        s->SetWidth(width);
        s->SetStart(VECTOR2I(p1->GetX(), -p1->GetY()));
        s->SetEnd(VECTOR2I(p2->GetX(), -p2->GetY()));

        result.push_back(s);
    }

    return result;
}


std::vector<BOARD_ITEM*> KICAD_IDX::ConvertElement(idx_geometry::EDMD_CIRCLE& aCircle)
{
    std::vector<BOARD_ITEM*> result;

    if (auto center = aCircle.GetCenter())
    {
        PCB_SHAPE* s = new PCB_SHAPE(nullptr, SHAPE_T::CIRCLE);
        s->SetCenter(VECTOR2I(center->GetX(), -center->GetY()));
        s->SetRadius(aCircle.GetRadius());
        result.push_back(s);
    }

    return result;
}



std::vector<BOARD_ITEM*> KICAD_IDX::ConvertElement( idx_geometry::EDMD_ARC& aArc )
{
    std::vector<BOARD_ITEM*> result;

    PCB_SHAPE* s = new PCB_SHAPE( nullptr, SHAPE_T::ARC );

    VECTOR2I  end = VECTOR2I( aArc.GetStart()->GetX(), -aArc.GetStart()->GetY() );
    VECTOR2I  start = VECTOR2I( aArc.GetEnd()->GetX(), -aArc.GetEnd()->GetY() );
    VECTOR2I  mid = ArcMidPoint( aArc );
    s->SetArcGeometry( start, mid, end );

    result.push_back(s);
    return result;
}


void KICAD_IDX::AddItem( BOARD_ITEM* aItem, LSET aLset )
{
    for( PCB_LAYER_ID id : aLset.Seq() )
    {
        aItem->SetLayer( id );
    }
}

    
LSET KICAD_IDX::GetLayers( idx_geometry::EDMD_CURVESET2D& aCurveSet )
{

    double upper = aCurveSet.m_upperBound.m_value * 1e3 ;
    double lower = aCurveSet.m_lowerBound.m_value * 1e3   ;

    std::vector<PCB_LAYER_ID> pli;

    int currentThickness = 0;
    int layerCount = 0  ;
    
    LSET result = LSET();

    for ( auto& layer : m_stackup )
    {
        if ( layer.second >= lower && layer.second <= upper  )
        {
            layerCount++;
            pli.push_back( layer.first );
            result.set( layer.first);
        }       
    }

    return result;

}