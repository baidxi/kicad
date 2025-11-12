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


#include <footprint.h>
#include <pad.h>
#include <pcb_track.h>

KICAD_IDX::KICAD_IDX( wxString aFilename, BOARD* aBoard, BOARD_COMMIT* aCommit ) :
        m_board( aBoard ),
        m_commit( aCommit )
{
    wxXmlDocument doc;

    if( !doc.Load( aFilename ) )
        return;

    m_holder = std::make_unique<IDX_HOLDER>( doc );

    BOARD_DESIGN_SETTINGS& dsnSettings = m_board->GetDesignSettings();
    BOARD_STACKUP          stackup = dsnSettings.GetStackupDescriptor();
    stackup.SynchronizeWithBoard( &dsnSettings );
    std::vector<BOARD_STACKUP_ITEM*> Llist = stackup.GetList();
    std::reverse( Llist.begin(), Llist.end() );

    int currentThickness = 0;

    for( BOARD_STACKUP_ITEM* bsi : Llist )
    {
        if( bsi )
        {
            if( bsi->GetBrdLayerId() == B_Cu )
            {
                currentThickness = 0;
            }

            if( currentThickness >= 0 && bsi->GetType() == BOARD_STACKUP_ITEM_TYPE::BS_ITEM_TYPE_COPPER )
            {
                m_stackup.push_back( { bsi->GetBrdLayerId(), currentThickness } );
            }

            currentThickness += bsi->GetThickness();
        }
    }


    for( auto inst : m_holder->GetBoardOutlineElements() )
    {
        // Check for board outline
        if( auto prop = inst->GetUserProperty( "THICKNESS" ) )
        {
            try
            {
                double thickness = prop->parseDouble( prop->m_value );

                if( thickness <= 0.0 )
                {
                    std::cout << "Invalid board thickness " << thickness << std::endl;
                }
                else
                {
                    m_boardThickness = thickness;
                }
            }
            catch( const std::invalid_argument& e )
            {
                std::cout << "Invalid numeric value for thickness: " << prop->m_value << std::endl;
            }
            catch( const std::out_of_range& e )
            {
                std::cout << "Thickness value out of range: " << prop->m_value << std::endl;
            }
        }
    }
};

void KICAD_IDX::Report( wxString aMsg, SEVERITY aSev )
{
    std::string prefix;
    if( aSev == RPT_SEVERITY_INFO )
        return;
    switch( aSev )
    {
    case RPT_SEVERITY_UNDEFINED: prefix = "UNDEFINED: "; break;
    case RPT_SEVERITY_INFO: prefix = "INFO: "; break;
    case RPT_SEVERITY_EXCLUSION: prefix = "EXCLUSION: "; break;
    case RPT_SEVERITY_ACTION: prefix = "ACTION: "; break;
    case RPT_SEVERITY_WARNING: prefix = "WARNING: "; break;
    case RPT_SEVERITY_ERROR: prefix = "ERROR: "; break;
    default: prefix = "UNDEFINED: "; break;
    }

    std::cout << prefix << aMsg << std::endl;
}


void KICAD_IDX::AddSelected( std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>>& aVector )
{
    for( auto instance : aVector )
    {
        AddElement( *instance, { Eco1_User } );
    }
}

void KICAD_IDX::SetKeepRules( ZONE& aZone, idx_pdm::EDMD_KEEP_CONSTRAINT& aKeep )
{
    aZone.SetIsRuleArea( true );
    aZone.SetHatchStyle( ZONE_BORDER_DISPLAY_STYLE::DIAGONAL_FULL );

    aZone.SetDoNotAllowZoneFills( false );
    aZone.SetDoNotAllowVias( false );
    aZone.SetDoNotAllowTracks( false );
    aZone.SetDoNotAllowFootprints( false );
    aZone.SetDoNotAllowPads( false );

    if( dynamic_cast<idx_pdm::EDMD_KEEPOUT*>( &aKeep ) )
    {
        for( auto& purpose : aKeep.m_purpose )
        {
            if( purpose == idx_pdm::KEEP_CONSTRAINT_PURPOSE::COMPONENT_PLACEMENT )
            {
                aZone.SetDoNotAllowFootprints( true );
            }
            if( purpose == idx_pdm::KEEP_CONSTRAINT_PURPOSE::ROUTE )
            {
                aZone.SetDoNotAllowPads( true );
                aZone.SetDoNotAllowTracks( true );
                aZone.SetDoNotAllowZoneFills( true );
            }
            if( purpose == idx_pdm::KEEP_CONSTRAINT_PURPOSE::VIA )
            {
                aZone.SetDoNotAllowVias( true );
            }
        }
    }
}


std::vector<BOARD_ITEM*> KICAD_IDX::AddNPTH( idx_pdm::EDMD_ITEM_INSTANCE& aInstance )
{
    std::vector<BOARD_ITEM*> m_result;

    if( !CheckInstance( aInstance ) )
        return m_result;

    for( auto const& z : aInstance.m_item->m_shape->m_shapeElement )
    {
        std::shared_ptr<idx_geometry::EDMD_CURVESET2D> curveset =
                std::dynamic_pointer_cast<idx_geometry::EDMD_CURVESET2D>( z->m_definingShape );

        if( !curveset )
        {
            this->Report( wxString::Format( wxT( "%s has no valid curveset" ), aInstance.m_name ), RPT_SEVERITY_ERROR );
            continue;
        }

        for( const auto& model : curveset->m_modelElement )
        {
            if( auto circle = std::dynamic_pointer_cast<idx_geometry::EDMD_CIRCLE>( model ) )
            {
                double   diameter = circle->GetRadius() * 2;
                VECTOR2I center = VECTOR2I( circle->GetCenter()->GetX(), -circle->GetCenter()->GetY() );
                auto     npth = new FOOTPRINT( m_board );
                auto     pad = new PAD( npth );

                pad->SetDrillSize( { diameter, diameter } );
                pad->SetPosition( { 0, 0 } );
                pad->SetAttribute( PAD_ATTRIB::NPTH );

                npth->SetPosition( center );
                npth->Add( pad );

                this->ApplyTransformation( npth, aInstance.m_transformation );

                m_result.push_back( npth );
            }
        }
    }

    return m_result;
}


std::vector<BOARD_ITEM*> KICAD_IDX::AddPTH( idx_pdm::EDMD_ITEM_INSTANCE& aInstance )
{
    std::vector<BOARD_ITEM*> m_result;

    if( !CheckInstance( aInstance ) )
        return m_result;

    for( auto const& z : aInstance.m_item->m_shape->m_shapeElement )
    {
        std::shared_ptr<idx_geometry::EDMD_CURVESET2D> curveset =
                std::dynamic_pointer_cast<idx_geometry::EDMD_CURVESET2D>( z->m_definingShape );

        if( !curveset )
        {
            this->Report( wxString::Format( wxT( "%s has no valid curveset" ), aInstance.m_name ), RPT_SEVERITY_ERROR );
            continue;
        }

        for( const auto& model : curveset->m_modelElement )
        {
            if( auto circle = std::dynamic_pointer_cast<idx_geometry::EDMD_CIRCLE>( model ) )
            {
                // Assuming GetRadius() returns in internal KiCad units (nanometers)
                double   diameter = circle->GetRadius() * 2;
                VECTOR2I center = VECTOR2I( circle->GetCenter()->GetX() * 1e6, -circle->GetCenter()->GetY() * 1e6 );
                auto     pth = new PCB_VIA( m_board );
                pth->SetFrontTentingMode( TENTING_MODE::NOT_TENTED );
                pth->SetBackTentingMode( TENTING_MODE::NOT_TENTED );
                pth->SetDrill( diameter );
                pth->SetPosition( center );

                this->ApplyTransformation( pth, aInstance.m_transformation );
                m_result.push_back( pth );
            }
        }
    }

    return m_result;
}

std::vector<BOARD_ITEM*> KICAD_IDX::AddZone( idx_pdm::EDMD_KEEP_CONSTRAINT& aKeep )
{
    std::vector<BOARD_ITEM*> m_result;

    for( auto const& y : aKeep.m_shapeElement )
    {
        std::shared_ptr<idx_geometry::EDMD_CURVESET2D> ptr =
                std::dynamic_pointer_cast<idx_geometry::EDMD_CURVESET2D>( y->m_definingShape );

        if( ptr )
        {
            LSET layers = GetLayers( *ptr );
            if( layers.Seq().size() < 1 )
            {
                continue;
            }

            ZONE* zone = new ZONE( m_board );
            zone->SetZoneName( aKeep.m_id );
            zone->SetLayerSet( layers );
            this->SetKeepRules( *zone, aKeep );

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
                m_result.push_back( zone );
            }
        }
    }
    return m_result;
}

std::vector<KICAD_IDX_CHANGE> KICAD_IDX::Baseline()
{
    std::vector<KICAD_IDX_CHANGE> m_result;

    for( auto edge : m_holder->GetBoardOutlineElements() )
    {
        KICAD_IDX_CHANGE change = KICAD_IDX_CHANGE( *edge );
        change.m_type = KICAD_IDX_CHANGE::ELEMENT_TYPE::BOARD_OUTLINE;
        change.m_items = AddElement( *edge, { Edge_Cuts } );
        change.m_name = edge->m_name;
        m_result.push_back( change );
    }

    for( auto keep : m_holder->GetKeepZones() )
    {
        KICAD_IDX_CHANGE change = KICAD_IDX_CHANGE( *keep );
        change.m_type = KICAD_IDX_CHANGE::ELEMENT_TYPE::KEEP;
        change.m_items = AddZone( *keep );
        change.m_name = keep->m_name;
        m_result.push_back( change );
    }

    for( auto cmp : m_holder->GetComponents() )
    {
        KICAD_IDX_CHANGE change = KICAD_IDX_CHANGE( *cmp );
        change.m_type = KICAD_IDX_CHANGE::ELEMENT_TYPE::COMPONENT;
        change.m_items = AddElement( *cmp, { User_1 } );
        change.m_name = cmp->m_name;
        m_result.push_back( change );
    }

    for( auto npth : m_holder->GetNPTH() )
    {
        KICAD_IDX_CHANGE change = KICAD_IDX_CHANGE( *npth );
        change.m_type = KICAD_IDX_CHANGE::ELEMENT_TYPE::NPTH;
        change.m_items = AddNPTH( *npth );
        change.m_name = npth->m_name;
        m_result.push_back( change );
    }

    for( auto pth : m_holder->GetPTH() )
    {
        KICAD_IDX_CHANGE change = KICAD_IDX_CHANGE( *pth );
        change.m_type = KICAD_IDX_CHANGE::ELEMENT_TYPE::PTH;
        change.m_items = AddPTH( *pth );
        change.m_name = pth->m_name;
        m_result.push_back( change );
    }

    for( auto other : m_holder->GetOthers() )
    {
        KICAD_IDX_CHANGE change = KICAD_IDX_CHANGE( *other );
        change.m_type = KICAD_IDX_CHANGE::ELEMENT_TYPE::OTHER;
        change.m_items = AddElement( *other, { User_1 } );
        change.m_name = other->m_name;
        m_result.push_back( change );
    }

    return m_result;
}

std::vector<KICAD_IDX_CHANGE> KICAD_IDX::Process()
{
    if( m_holder->m_dataset->m_processInstruction.size() < 1 )
        return {};

    idx_foundation::EDMD_PROCESS_INSTRUCTION::PROCESS_TYPE type;
    type = m_holder->m_dataset->m_processInstruction[0]->m_processType;

    if( type == idx_foundation::EDMD_PROCESS_INSTRUCTION::PROCESS_TYPE::BASELINE )
    {
        return Baseline();
    }
    if( type == idx_foundation::EDMD_PROCESS_INSTRUCTION::PROCESS_TYPE::SEND_CHANGES )
    {
        return GetChanges();
    }
    return {};
}


std::vector<KICAD_IDX_CHANGE> KICAD_IDX::GetChanges()
{
    return {};
}


void KICAD_IDX::AddAll()
{
    PCB_GROUP* group = new PCB_GROUP( nullptr );

    for( KICAD_IDX_CHANGE edge : GetChanges() )
    {
        group->SetName( "" );
        m_commit->Add( group );

        for( BOARD_ITEM* item : edge.m_items )
        {
            group->AddItem( item );
            m_commit->Stage( item, CHT_ADD );
            m_commit->Add( item );
        }
    }


    for( auto const& x : m_holder->GetKeepZones() )
    {
        AddZone( *x );
    }
    m_commit->Push( "Import IDX" );
}


void KICAD_IDX::Apply( std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> aVector )
{
    PCB_GROUP* group = new PCB_GROUP( nullptr );

    for( auto change : aVector )
    {
        group->SetName( "" );
        m_commit->Add( group );

        for( BOARD_ITEM* item : change->m_items )
        {
            group->AddItem( item );
            m_commit->Stage( item, CHT_ADD );
            m_commit->Add( item );
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
            BOARD_ITEM* result; /*
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


VECTOR2I GetTransfoTranslation( const idx_pdm::EDMD_TRANSFORMATION& aTransfo )
{
    VECTOR2I moveVector( aTransfo.m_tx.m_value * 1e6, -aTransfo.m_ty.m_value * 1e6 );
    return moveVector;
}


EDA_ANGLE GetTransfoRotation( const idx_pdm::EDMD_TRANSFORMATION& aTransfo )
{
    double xx = aTransfo.m_xx;
    double xy = aTransfo.m_xy;
    double yx = aTransfo.m_yx;
    double yy = aTransfo.m_yy;

    if( xx == -yy )
    {
        xx = yy;
        yx = -yx;
    }

    if( ( xx == yy ) && ( xy == -yx ) )
    {
        if( xx != 1 || xy != 0 )
        {
            return EDA_ANGLE( atan2( yx, xx ), RADIANS_T );
        }
    }
    // Inconsistent rotation matrix
    return EDA_ANGLE( 0 );
}


bool GetTransfoFlipped( const idx_pdm::EDMD_TRANSFORMATION& aTransfo )
{
    double xx = aTransfo.m_xx;
    double yy = aTransfo.m_yy;

    return ( xx == -yy );
}

bool KICAD_IDX::ApplyTransformation( BOARD_ITEM* aItem, const idx_pdm::EDMD_TRANSFORMATION& aTransfo )
{
    if( GetTransfoFlipped( aTransfo ) )
        aItem->Mirror( VECTOR2I( 0, 0 ), FLIP_DIRECTION::LEFT_RIGHT );
    aItem->Rotate( { 0, 0 }, GetTransfoRotation( aTransfo ) );
    aItem->Move( GetTransfoTranslation( aTransfo ) );

    return true;
}

bool KICAD_IDX::CheckInstance( idx_pdm::EDMD_ITEM_INSTANCE& aInstance )
{
    if( !( aInstance.m_item ) )
    {
        this->Report( wxString::Format( wxT( "%s has no item" ), aInstance.m_name ), RPT_SEVERITY_ERROR );
        return false;
    }

    if( !( aInstance.m_item->m_shape ) )
    {
        this->Report( wxString::Format( wxT( "%s - %s has no shape" ), aInstance.m_name, aInstance.m_item->m_id ),
                      RPT_SEVERITY_ERROR );
        return false;
    }

    if( aInstance.m_item->m_shape->m_shapeElement.size() < 1 )
    {
        this->Report( wxString::Format( wxT( "%s - %s shape has no shape element" ), aInstance.m_name,
                                        aInstance.m_item->m_id ),
                      RPT_SEVERITY_ERROR );
        return false;
    }
    return true;
}


std::vector<BOARD_ITEM*> KICAD_IDX::AddElement( idx_pdm::EDMD_ITEM_INSTANCE& aInstance, PCB_LAYER_ID aLayer )
{
    std::vector<BOARD_ITEM*> m_result;

    if( !CheckInstance( aInstance ) )
        return m_result;


    for( auto const& z : aInstance.m_item->m_shape->m_shapeElement )
    {
        std::shared_ptr<idx_geometry::EDMD_CURVESET2D> curveset =
                std::dynamic_pointer_cast<idx_geometry::EDMD_CURVESET2D>( z->m_definingShape );

        if( curveset )
        {
            for( BOARD_ITEM* item : ConvertElement( *curveset ) )
            {
                item->SetLayer( aLayer );
                this->ApplyTransformation( item, aInstance.m_transformation );
                m_result.push_back( item );
            }
        }
        else
        {
            this->Report( wxString::Format( wxT( "%s has no valid curveset" ), aInstance.m_name ), RPT_SEVERITY_ERROR );
        }
    }

    return m_result;
}


std::vector<BOARD_ITEM*> KICAD_IDX::ConvertElement( idx_geometry::EDMD_CURVESET2D& aSet )
{
    std::vector<BOARD_ITEM*> result;

    for( const auto& model : aSet.m_modelElement )
    {
        if( !model )
            continue;

        if( auto polyline = std::dynamic_pointer_cast<idx_geometry::EDMD_POLYLINE>( model ) )
        {
            auto items = ConvertElement( *polyline );
            result.insert( result.end(), std::make_move_iterator( items.begin() ),
                           std::make_move_iterator( items.end() ) );
        }
        else if( auto arc = std::dynamic_pointer_cast<idx_geometry::EDMD_ARC>( model ) )
        {
            auto items = ConvertElement( *arc );
            result.insert( result.end(), std::make_move_iterator( items.begin() ),
                           std::make_move_iterator( items.end() ) );
        }
        else if( auto circle = std::dynamic_pointer_cast<idx_geometry::EDMD_CIRCLE>( model ) )
        {
            auto items = ConvertElement( *circle );
            result.insert( result.end(), std::make_move_iterator( items.begin() ),
                           std::make_move_iterator( items.end() ) );
        }
    }

    return result;
}


std::vector<BOARD_ITEM*> KICAD_IDX::ConvertElement( idx_geometry::EDMD_POLYLINE& aPolyline )
{
    const auto& points = aPolyline.m_point;
    size_t      num_points = points.size();
    if( num_points < 2 )
        return {};

    std::vector<BOARD_ITEM*> result;
    result.reserve( num_points - 1 );

    double width = aPolyline.m_thickness.m_value * 1e6;

    for( size_t i = 0; i < num_points - 1; ++i )
    {
        auto& p1 = points[i];
        auto& p2 = points[i + 1];
        if( !p1 || !p2 )
            continue;

        PCB_SHAPE* s = new PCB_SHAPE();
        s->SetWidth( width );
        s->SetStart( VECTOR2I( p1->GetX(), -p1->GetY() ) );
        s->SetEnd( VECTOR2I( p2->GetX(), -p2->GetY() ) );

        result.push_back( s );
    }

    return result;
}


std::vector<BOARD_ITEM*> KICAD_IDX::ConvertElement( idx_geometry::EDMD_CIRCLE& aCircle )
{
    std::vector<BOARD_ITEM*> result;

    if( auto center = aCircle.GetCenter() )
    {
        PCB_SHAPE* s = new PCB_SHAPE( nullptr, SHAPE_T::CIRCLE );
        s->SetCenter( VECTOR2I( center->GetX(), -center->GetY() ) );
        s->SetRadius( aCircle.GetRadius() );
        result.push_back( s );
    }

    return result;
}


std::vector<BOARD_ITEM*> KICAD_IDX::ConvertElement( idx_geometry::EDMD_ARC& aArc )
{
    std::vector<BOARD_ITEM*> result;

    PCB_SHAPE* s = new PCB_SHAPE( nullptr, SHAPE_T::ARC );

    VECTOR2I end = VECTOR2I( aArc.GetStart()->GetX(), -aArc.GetStart()->GetY() );
    VECTOR2I start = VECTOR2I( aArc.GetEnd()->GetX(), -aArc.GetEnd()->GetY() );
    VECTOR2I mid = ArcMidPoint( aArc );
    s->SetArcGeometry( start, mid, end );

    result.push_back( s );
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
    double upper = aCurveSet.m_upperBound.m_value * 1e3;
    double lower = aCurveSet.m_lowerBound.m_value * 1e3;

    std::vector<PCB_LAYER_ID> pli;

    int currentThickness = 0;
    int layerCount = 0;

    LSET result = LSET();

    for( auto& layer : m_stackup )
    {
        if( layer.second >= lower && layer.second <= upper )
        {
            layerCount++;
            pli.push_back( layer.first );
            result.set( layer.first );
        }
    }

    if( upper == m_boardThickness * 1e3 || lower == m_boardThickness * 1e3 )
    {
        result.set( F_Cu );
    }

    return result;
}