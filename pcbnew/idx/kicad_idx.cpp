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
#include <lset.h>
#include <layer_ids.h>
#include <zone.h>
#include <tools/convert_tool.h>
#include <board_design_settings.h>
#include <layer_range.h>


#include <footprint.h>
#include <pad.h>
#include <pcb_track.h>

KICAD_IDX::KICAD_IDX( std::shared_ptr<idx_foundation::EDMD_DATASET> aDataset, std::shared_ptr<KIDX_REGISTRY_KICAD> aReg,
                      BOARD* aBoard, BOARD_COMMIT* aCommit ) :
        m_commit( aCommit ),
        m_board( aBoard ),
        m_dataset( aDataset ),
        m_reg( aReg )
{
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

    m_boardThickness = aDataset->GetBoardThickness();
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
        AddElement( *instance, Eco1_User );
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

    if( dynamic_cast<idx_pdm::EDMD_KEEP_OUT*>( &aKeep ) )
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
        std::shared_ptr<idx_geometry::EDMD_CURVE_SET_2D> curveset =
                std::dynamic_pointer_cast<idx_geometry::EDMD_CURVE_SET_2D>( z->m_definingShape );

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

                pad->SetDrillSize( VECTOR2I( diameter, diameter ) );
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
        std::shared_ptr<idx_geometry::EDMD_CURVE_SET_2D> curveset =
                std::dynamic_pointer_cast<idx_geometry::EDMD_CURVE_SET_2D>( z->m_definingShape );

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

std::vector<BOARD_ITEM*> KICAD_IDX::AddZone( idx_pdm::EDMD_ITEM_INSTANCE& aInst )
{
    std::vector<BOARD_ITEM*>                       m_result;
    std::shared_ptr<idx_pdm::EDMD_KEEP_CONSTRAINT> keep =
            std::dynamic_pointer_cast<idx_pdm::EDMD_KEEP_CONSTRAINT>( aInst.m_item->m_shape );

    if( !keep )
        return {};

    for( auto const& y : keep->m_shapeElement )
    {
        std::shared_ptr<idx_geometry::EDMD_CURVE_SET_2D> ptr =
                std::dynamic_pointer_cast<idx_geometry::EDMD_CURVE_SET_2D>( y->m_definingShape );

        if( ptr )
        {
            LSET layers = GetLayers( *ptr );
            if( layers.Seq().size() < 1 )
            {
                continue;
            }

            ZONE* zone = new ZONE( m_board );
            zone->SetZoneName( keep->m_id );
            zone->SetLayerSet( layers );
            this->SetKeepRules( *zone, *keep );

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

                    for( const VECTOR2I& p : buffer->CPoints() )
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

                    for( const VECTOR2I& p : buffer->CPoints() )
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

KICAD_IDX_CHANGE::ELEMENT_TYPE KICAD_IDX_CHANGE::GetType( idx_pdm::EDMD_ITEM_INSTANCE& aInst )
{
    switch( aInst.GetType() )
    {
    case idx_pdm::EDMD_ITEM_INSTANCE::TYPE::BOARD_OUTLINE: return KICAD_IDX_CHANGE::ELEMENT_TYPE::BOARD_OUTLINE;

    case idx_pdm::EDMD_ITEM_INSTANCE::TYPE::KEEP_IN:
    case idx_pdm::EDMD_ITEM_INSTANCE::TYPE::KEEP_OUT: return KICAD_IDX_CHANGE::ELEMENT_TYPE::KEEP;

    case idx_pdm::EDMD_ITEM_INSTANCE::TYPE::COMPONENT: return KICAD_IDX_CHANGE::ELEMENT_TYPE::COMPONENT;

    case idx_pdm::EDMD_ITEM_INSTANCE::TYPE::NPTH: return KICAD_IDX_CHANGE::ELEMENT_TYPE::NPTH;

    case idx_pdm::EDMD_ITEM_INSTANCE::TYPE::PTH: return KICAD_IDX_CHANGE::ELEMENT_TYPE::PTH;

    default: return KICAD_IDX_CHANGE::ELEMENT_TYPE::OTHER; break;
    }
}

std::vector<BOARD_ITEM*> KICAD_IDX::ConvertInstance( idx_pdm::EDMD_ITEM_INSTANCE& aInst, bool aPredecessor )
{
    switch( aInst.GetType() )
    {
    case idx_pdm::EDMD_ITEM_INSTANCE::TYPE::BOARD_OUTLINE: return AddElement( aInst, Edge_Cuts );

    case idx_pdm::EDMD_ITEM_INSTANCE::TYPE::KEEP_IN:
    case idx_pdm::EDMD_ITEM_INSTANCE::TYPE::KEEP_OUT: return AddZone( aInst );

    case idx_pdm::EDMD_ITEM_INSTANCE::TYPE::COMPONENT: return AddElement( aInst, User_1 );

    case idx_pdm::EDMD_ITEM_INSTANCE::TYPE::NPTH: return AddNPTH( aInst );

    case idx_pdm::EDMD_ITEM_INSTANCE::TYPE::PTH: return AddNPTH( aInst );

    case idx_pdm::EDMD_ITEM_INSTANCE::TYPE::OTHER: return AddElement( aInst, User_2 );

    default: return {};
    }
}


std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> KICAD_IDX::SendInformation()
{
    std::cout << "processing SendInformation" << std::endl;
    std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> result;

    if( !m_reg || !m_dataset )
        return {};

    // SendInformation processing according to Developer Recommendations 6.4.2
    // Items in the message are processed one after the other

    for( const std::shared_ptr<idx_pdm::EDMD_ITEM>& item : m_dataset->m_body.m_items )
    {
        if( !item )
            continue;

        std::cout << "Processing SendInformation item: " << item->m_id << std::endl;

        // Create a copy of the item so we can modify it during processing
        idx_pdm::EDMD_ITEM itemCopy = *item;

        // Step 1: Checking identifiers (list) - handled by SI_Process
        // If the item does not contain any identifiers, the message is errored and the processing transaction is aborted
        if( !m_reg->SI_1CheckIdentifiers( *item ) )
        {
            std::cerr << "SendInformation: Item " << item->m_id << " has no identifiers - aborting transaction"
                      << std::endl;
            return {}; // Abort the entire transaction
        }

        // Step 2-8: Process through SI_Process which handles all adjustment steps
        if( !m_reg->SI_Process( itemCopy ) )
        {
            std::cerr << "SendInformation: SI_Process failed for item " << item->m_id << " - aborting transaction"
                      << std::endl;
            return {}; // Abort the entire transaction
        }

        // If we reach here, the item has been successfully processed through all 8 steps
        // Now we can convert the item instances to KiCad board items

        for( const std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>& instance : item->m_itemInstance )
        {
            if( !instance )
                continue;

            std::shared_ptr<KICAD_IDX_CHANGE> kChange = std::make_shared<KICAD_IDX_CHANGE>( *instance );

            kChange->m_name = instance->m_id;
            kChange->m_type = kChange->GetType( *instance );
            kChange->m_newItems = ConvertInstance( *instance );

            result.push_back( kChange );
        }

        // Add the processed item to the repository
        m_reg->GetItemRepository().push_back( *item );
    }

    return result;
}

std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> KICAD_IDX::Process()
{
    if( !m_dataset )
        return {};

    if( m_dataset->m_processInstruction.size() < 1 )
        return {};

    idx_foundation::EDMD_PROCESS_INSTRUCTION::PROCESS_TYPE type;
    type = m_dataset->m_processInstruction[0]->m_processType;

    if( type == idx_foundation::EDMD_PROCESS_INSTRUCTION::PROCESS_TYPE::SEND_INFORMATION )
    {
        return SendInformation();
    }
    else if( type == idx_foundation::EDMD_PROCESS_INSTRUCTION::PROCESS_TYPE::SEND_CHANGES )
    {
        return GetChanges();
    }
    return {};
}

std::shared_ptr<KICAD_IDX_CHANGE> KICAD_IDX::ConvertChange( idx_computational::EDMD_CHANGE& aChange )
{
    std::shared_ptr<KICAD_IDX_CHANGE> kChange = std::make_shared<KICAD_IDX_CHANGE>( aChange );

    // NewItem is only one that is mandatory
    if( !aChange.m_newItem )
        return {};

    // If NewItem is already known, reject the transaction
    // TODO, actually drop the entire IDX file

    if( m_reg->isIdentifierKnown( *( aChange.m_newItem ) ) )
        return {};

    //

    if( aChange.m_predecessorItem && !m_reg->isIdentifierKnown( *( aChange.m_predecessorItem ) ) )
    {
        // TODO: implement the RequestInformation
    }


    bool found = false;

    if( m_dataset->GetItem( *( aChange.m_newItem ) ) )
    {
        auto item = m_dataset->GetItem( *( aChange.m_newItem ) );

        if( item )
        {
            for( auto instance : item->m_itemInstance )
            {
                if( !instance )
                    continue;

                kChange->m_name = instance->m_id;
                kChange->m_type = kChange->GetType( *instance );
                kChange->m_newItems = ConvertInstance( *instance );
            }
        }
    }

    if( aChange.m_predecessorItem && m_dataset->GetItem( *( aChange.m_predecessorItem ) ) )
    {
        auto elmt = m_dataset->GetItem( *( aChange.m_predecessorItem ) );

        if( elmt )
        {
            for( auto instance : elmt->m_itemInstance )
            {
                if( !instance )
                    continue;
                kChange->m_predecessorItems = ConvertInstance( *instance );
            }
        }
    }

    return kChange;
}

std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> KICAD_IDX::GetChanges()
{
    std::cout << "processing changes" << std::endl;

    std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> result;


    for( auto instruction : m_dataset->m_processInstruction )
    {
        std::shared_ptr<idx_computational::EDMD_PROCESS_INSTRUCTION_SEND_CHANGES> instr =
                std::dynamic_pointer_cast<idx_computational::EDMD_PROCESS_INSTRUCTION_SEND_CHANGES>( instruction );

        if( !instr )
        {
            std::cout << "Process instruction dropped" << std::endl;
            continue;
        }

        // Any item not referenced by a change should be processed like in a SenfInformation message
        // TODO

        for( auto change : instr->m_transaction->m_changes )
        {
            if( !std::dynamic_pointer_cast<idx_computational::EDMD_CHANGE>( change ) )
                continue;


            result.push_back( ConvertChange( *std::dynamic_pointer_cast<idx_computational::EDMD_CHANGE>( change ) ) );
        }
    }

    return result;
}


void KICAD_IDX::Apply( std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> aVector )
{
    PCB_GROUP* group = new PCB_GROUP( nullptr );

    for( auto change : aVector )
    {
        group->SetName( "" );
        m_commit->Add( group );

        for( BOARD_ITEM* item : change->m_newItems )
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
    switch( aShape.m_type )
    {
    case idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE::TYPE::RIGID_AREA:
    case idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE::TYPE::FLEXIBLE_AREA:
    case idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE::TYPE::STIFFENER:
    case idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE::TYPE::USER_AREA:
    case idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE::TYPE::MECHANICAL:
    case idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE::TYPE::PLACEMENT_GROUP: break;
    }


    PCB_GROUP* group = new PCB_GROUP( nullptr );
    group->SetName( aShape.m_id );
    m_commit->Add( group );
    for( auto const& elt : aShape.m_shapeElement )
    {
        std::shared_ptr<idx_geometry::EDMD_CURVE_SET_2D> ptr =
                std::dynamic_pointer_cast<idx_geometry::EDMD_CURVE_SET_2D>( elt->m_definingShape );

        if( ptr )
        {
            // TODO
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
        std::shared_ptr<idx_geometry::EDMD_CURVE_SET_2D> curveset =
                std::dynamic_pointer_cast<idx_geometry::EDMD_CURVE_SET_2D>( z->m_definingShape );

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


std::vector<BOARD_ITEM*> KICAD_IDX::ConvertElement( idx_geometry::EDMD_CURVE_SET_2D& aSet )
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


LSET KICAD_IDX::GetLayers( idx_geometry::EDMD_CURVE_SET_2D& aCurveSet )
{
    double upper = aCurveSet.m_upperBound.m_value * 1e3;
    double lower = aCurveSet.m_lowerBound.m_value * 1e3;

    std::vector<PCB_LAYER_ID> pli;

    LSET result = LSET();

    for( auto& layer : m_stackup )
    {
        if( layer.second >= lower && layer.second <= upper )
        {
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