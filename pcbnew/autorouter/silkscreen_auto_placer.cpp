/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
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

#include "silkscreen_auto_placer.h"

#include <base_units.h>
#include <board.h>
#include <board_commit.h>
#include <drc/drc_rtree.h>
#include <footprint.h>
#include <pad.h>
#include <pcb_field.h>
#include <pcb_shape.h>
#include <pcb_text.h>
#include <pcb_track.h>
#include <pcb_board_outline.h>
#include <progress_reporter.h>

#include <geometry/shape_rect.h>

#include <algorithm>
#include <cmath>
#include <numeric>

static constexpr int COST_OBSTACLE_OVERLAP  = 1000;
static constexpr int COST_OUTSIDE_BOARD     = 500;
static constexpr int COST_SILK_OVERLAP      = 100;
static constexpr int COST_LABEL_OVERLAP     = 200;

static constexpr int SA_MAX_STAGES          = 50;
static constexpr int SA_MOVES_PER_LABEL     = 20;
static constexpr double SA_COOLING_FACTOR   = 0.9;


SILKSCREEN_AUTO_PLACER::SILKSCREEN_AUTO_PLACER( BOARD* aBoard,
                                                const SILK_PLACER_SETTINGS& aSettings ) :
        m_board( aBoard ),
        m_settings( aSettings ),
        m_rng( std::random_device{}() )
{
}


SILKSCREEN_AUTO_PLACER::~SILKSCREEN_AUTO_PLACER()
{
}


SILK_PLACER_RESULT SILKSCREEN_AUTO_PLACER::Run( BOARD_COMMIT& aCommit,
                                                const std::vector<FOOTPRINT*>& aFootprints,
                                                PROGRESS_REPORTER* aProgress )
{
    if( aFootprints.empty() )
        return {};

    std::vector<SILK_PLACEMENT> placements;
    placements.reserve( aFootprints.size() );

    for( FOOTPRINT* fp : aFootprints )
    {
        PCB_FIELD& ref = fp->Reference();

        if( !ref.IsVisible() )
            continue;

        if( ref.GetLayer() != F_SilkS && ref.GetLayer() != B_SilkS )
            continue;

        SILK_PLACEMENT placement;
        placement.footprint = fp;
        placement.refField = &ref;
        placement.silkLayer = ref.GetLayer();
        placement.originalPos = ref.GetPosition();
        placement.originalAngle = ref.GetTextAngle();
        placement.relaxSilkOverlap = m_settings.allowSilkUnder.count( fp->GetReference() ) > 0;
        placement.outlineBBox = resolveOutlineBBox( fp, placement.silkLayer );

        placements.push_back( std::move( placement ) );
    }

    if( placements.empty() )
        return {};

    if( aProgress )
        aProgress->SetMaxProgress( 100 );

    buildObstacleTrees( aFootprints );

    if( aProgress )
    {
        aProgress->SetCurrentProgress( 10 );
        aProgress->KeepRefreshing( false );
    }

    // Score each ref's current position so we can skip moves that don't improve placement
    for( SILK_PLACEMENT& p : placements )
    {
        SILK_CANDIDATE originalCand;
        originalCand.position = p.originalPos;
        originalCand.angle = p.originalAngle;
        originalCand.bbox = computeTextBBox( p, originalCand );

        p.originalCost = scoreCandidateLocal( p, originalCand );

        SHAPE_RECT origShape( originalCand.bbox );

        if( m_obstacleTree->CheckColliding( &origShape, p.silkLayer ) )
            p.originalHardViolation = true;

        if( !isInsideBoard( originalCand.bbox ) )
            p.originalHardViolation = true;
    }

    for( SILK_PLACEMENT& p : placements )
        generateCandidates( p );

    if( aProgress )
    {
        aProgress->SetCurrentProgress( 30 );
        aProgress->KeepRefreshing( false );
    }

    runFALP( placements );

    if( aProgress )
    {
        aProgress->SetCurrentProgress( 50 );
        aProgress->KeepRefreshing( false );
    }

    runSimulatedAnnealing( placements );

    if( aProgress )
    {
        aProgress->SetCurrentProgress( 90 );
        aProgress->KeepRefreshing( false );
    }

    applyPlacements( aCommit, placements );

    if( aProgress )
    {
        aProgress->SetCurrentProgress( 100 );
        aProgress->KeepRefreshing( false );
    }

    SILK_PLACER_RESULT result;
    result.attempted = static_cast<int>( placements.size() );

    for( const SILK_PLACEMENT& p : placements )
    {
        if( p.assignedCandidate >= 0 )
            result.moved++;
    }

    return result;
}


void SILKSCREEN_AUTO_PLACER::buildObstacleTrees( const std::vector<FOOTPRINT*>& aFootprints )
{
    m_obstacleTree = std::make_unique<DRC_RTREE>();
    m_silkTree = std::make_unique<DRC_RTREE>();
    m_solutionTree = std::make_unique<DRC_RTREE>();

    // Build a set of ref fields being placed so we can exclude them from the silk tree
    std::set<BOARD_ITEM*> refsBeingPlaced;

    for( const FOOTPRINT* fp : aFootprints )
    {
        if( fp->Reference().IsVisible() )
            refsBeingPlaced.insert( const_cast<PCB_FIELD*>( &fp->Reference() ) );
    }

    auto insertSilkItem = [&]( BOARD_ITEM* item )
    {
        if( refsBeingPlaced.count( item ) )
            return;

        for( PCB_LAYER_ID layer : { F_SilkS, B_SilkS } )
        {
            if( item->IsOnLayer( layer ) )
                m_silkTree->Insert( item, layer );
        }
    };

    for( FOOTPRINT* fp : m_board->Footprints() )
    {
        for( PAD* pad : fp->Pads() )
        {
            if( pad->FlashLayer( F_Cu ) )
                m_obstacleTree->Insert( pad, pad->GetPrincipalLayer(), F_SilkS, 0 );

            if( pad->FlashLayer( B_Cu ) )
                m_obstacleTree->Insert( pad, pad->GetPrincipalLayer(), B_SilkS, 0 );
        }

        for( BOARD_ITEM* item : fp->GraphicalItems() )
            insertSilkItem( item );

        for( PCB_FIELD* field : fp->GetFields() )
        {
            if( field->IsVisible() )
                insertSilkItem( field );
        }
    }

    if( m_settings.avoidVias )
    {
        for( PCB_TRACK* track : m_board->Tracks() )
        {
            if( track->Type() == PCB_VIA_T )
            {
                m_obstacleTree->Insert( track, F_SilkS );
                m_obstacleTree->Insert( track, B_SilkS );
            }
        }
    }

    for( BOARD_ITEM* item : m_board->Drawings() )
        insertSilkItem( item );
}


BOX2I SILKSCREEN_AUTO_PLACER::resolveOutlineBBox( FOOTPRINT* aFp,
                                                   PCB_LAYER_ID aSilkLayer ) const
{
    PCB_LAYER_ID crtydLayer = ( aSilkLayer == F_SilkS ) ? F_CrtYd : B_CrtYd;
    const SHAPE_POLY_SET& courtyard = aFp->GetCourtyard( crtydLayer );

    if( courtyard.OutlineCount() > 0 )
        return courtyard.BBox();

    PCB_LAYER_ID fabLayer = ( aSilkLayer == F_SilkS ) ? F_Fab : B_Fab;
    BOX2I fabBBox = aFp->GetLayerBoundingBox( LSET( { fabLayer } ) );

    if( fabBBox.GetWidth() > 0 && fabBBox.GetHeight() > 0 )
        return fabBBox;

    return aFp->GetBoundingBox( false );
}


EDA_ANGLE SILKSCREEN_AUTO_PLACER::computeAngle( FOOTPRINT* aFp, bool aPerpendicular ) const
{
    EDA_ANGLE fpAngle = aFp->GetOrientation();

    if( aPerpendicular )
        fpAngle += EDA_ANGLE( 90, DEGREES_T );

    if( m_settings.rotationStrategy == SILK_ROTATION_STRATEGY::KLC_STYLE )
    {
        // KLC requires text to be readable from bottom or right
        EDA_ANGLE normalized = fpAngle.Normalize180();

        if( normalized > EDA_ANGLE( 90, DEGREES_T ) || normalized <= EDA_ANGLE( -90, DEGREES_T ) )
            fpAngle += EDA_ANGLE( 180, DEGREES_T );
    }

    return fpAngle.Normalize();
}


void SILKSCREEN_AUTO_PLACER::setupMeasurementText( PCB_TEXT& aText,
                                                    const SILK_PLACEMENT& aPlacement ) const
{
    aText.SetText( aPlacement.refField->GetText() );

    if( m_settings.fixedSize )
        aText.SetTextSize( VECTOR2I( m_settings.fixedSizeValue, m_settings.fixedSizeValue ) );
    else
        aText.SetTextSize( aPlacement.refField->GetTextSize() );

    if( m_settings.fixedWidth )
        aText.SetTextThickness( m_settings.fixedWidthValue );
    else
        aText.SetTextThickness( aPlacement.refField->GetTextThickness() );
}


BOX2I SILKSCREEN_AUTO_PLACER::computeTextBBox( const SILK_PLACEMENT& aPlacement,
                                                const SILK_CANDIDATE& aCand ) const
{
    PCB_TEXT tempText( *aPlacement.refField );
    setupMeasurementText( tempText, aPlacement );
    tempText.SetPosition( aCand.position );
    tempText.SetTextAngle( aCand.angle );
    tempText.SetLayer( aPlacement.silkLayer );

    return tempText.GetBoundingBox();
}


bool SILKSCREEN_AUTO_PLACER::isInsideBoard( const BOX2I& aBBox ) const
{
    const PCB_BOARD_OUTLINE* outline = m_board->BoardOutline();

    if( !outline || !outline->HasOutline() )
        return true;

    const SHAPE_POLY_SET& boardPoly = outline->GetOutline();

    return boardPoly.Contains( aBBox.GetOrigin() )
           && boardPoly.Contains( VECTOR2I( aBBox.GetRight(), aBBox.GetTop() ) )
           && boardPoly.Contains( VECTOR2I( aBBox.GetRight(), aBBox.GetBottom() ) )
           && boardPoly.Contains( VECTOR2I( aBBox.GetLeft(), aBBox.GetBottom() ) );
}


void SILKSCREEN_AUTO_PLACER::generateCandidates( SILK_PLACEMENT& aPlacement )
{
    const BOX2I& outline = aPlacement.outlineBBox;
    int delta = m_settings.positionDelta;

    EDA_ANGLE primaryAngle = computeAngle( aPlacement.footprint );
    std::vector<EDA_ANGLE> angles = { primaryAngle };

    if( m_settings.tryAlteredRotation )
        angles.push_back( computeAngle( aPlacement.footprint, true ) );

    PCB_TEXT tempText( *aPlacement.refField );
    setupMeasurementText( tempText, aPlacement );

    int centerX = outline.GetCenter().x;
    int centerY = outline.GetCenter().y;
    int top = outline.GetTop();
    int bottom = outline.GetBottom();
    int left = outline.GetLeft();
    int right = outline.GetRight();

    for( const EDA_ANGLE& angle : angles )
    {
        tempText.SetTextAngle( angle );
        BOX2I textBox = tempText.GetBoundingBox();
        int textW = textBox.GetWidth();
        int textH = textBox.GetHeight();

        struct PosEntry
        {
            int         flag;
            VECTOR2I    pos;
        };

        std::vector<PosEntry> positions = {
            { POS_TOP_CENTER,   VECTOR2I( centerX, top - delta - textH / 2 ) },
            { POS_CENTER_RIGHT, VECTOR2I( right + delta + textW / 2, centerY ) },
            { POS_BOT_CENTER,   VECTOR2I( centerX, bottom + delta + textH / 2 ) },
            { POS_CENTER_LEFT,  VECTOR2I( left - delta - textW / 2, centerY ) },
            { POS_TOP_LEFT,     VECTOR2I( left, top - delta - textH / 2 ) },
            { POS_TOP_RIGHT,    VECTOR2I( right, top - delta - textH / 2 ) },
            { POS_BOT_LEFT,     VECTOR2I( left, bottom + delta + textH / 2 ) },
            { POS_BOT_RIGHT,    VECTOR2I( right, bottom + delta + textH / 2 ) },
        };

        for( const PosEntry& entry : positions )
        {
            if( !( m_settings.positionFlags & entry.flag ) )
                continue;

            SILK_CANDIDATE cand;
            cand.position = entry.pos;
            cand.angle = angle;
            cand.bbox = computeTextBBox( aPlacement, cand );
            cand.localCost = scoreCandidateLocal( aPlacement, cand );
            cand.hardViolation = false;

            // Check for pad overlap (hard violation)
            SHAPE_RECT candShape( cand.bbox );

            if( m_obstacleTree->CheckColliding( &candShape, aPlacement.silkLayer ) )
                cand.hardViolation = true;

            // Check board edge containment
            if( !isInsideBoard( cand.bbox ) )
                cand.hardViolation = true;

            aPlacement.candidates.push_back( std::move( cand ) );
        }
    }
}


int SILKSCREEN_AUTO_PLACER::scoreCandidateLocal( const SILK_PLACEMENT& aPlacement,
                                                  const SILK_CANDIDATE& aCandidate ) const
{
    int cost = 0;
    SHAPE_RECT candShape( aCandidate.bbox );

    if( m_obstacleTree->CheckColliding( &candShape, aPlacement.silkLayer ) )
        cost += COST_OBSTACLE_OVERLAP;

    if( !aPlacement.relaxSilkOverlap
            && m_silkTree->CheckColliding( &candShape, aPlacement.silkLayer ) )
    {
        cost += COST_SILK_OVERLAP;
    }

    if( !isInsideBoard( aCandidate.bbox ) )
        cost += COST_OUTSIDE_BOARD;

    return cost;
}


int SILKSCREEN_AUTO_PLACER::countLabelOverlaps( const std::vector<SILK_PLACEMENT>& aPlacements,
                                                 int aIndex ) const
{
    const SILK_PLACEMENT& placement = aPlacements[aIndex];

    if( placement.assignedCandidate < 0 )
        return 0;

    const SILK_CANDIDATE& cand = placement.candidates[placement.assignedCandidate];
    SHAPE_RECT candShape( cand.bbox );

    int overlaps = 0;

    for( int i = 0; i < static_cast<int>( aPlacements.size() ); i++ )
    {
        if( i == aIndex )
            continue;

        if( aPlacements[i].assignedCandidate < 0 )
            continue;

        if( aPlacements[i].silkLayer != placement.silkLayer )
            continue;

        const SILK_CANDIDATE& other = aPlacements[i].candidates[aPlacements[i].assignedCandidate];
        SHAPE_RECT otherShape( other.bbox );

        if( candShape.Collide( &otherShape, 0 ) )
            overlaps++;
    }

    return overlaps;
}


void SILKSCREEN_AUTO_PLACER::runFALP( std::vector<SILK_PLACEMENT>& aPlacements )
{
    // Sort by ascending count of viable (low-cost, no hard violation) candidates.
    // This is the "most constrained first" heuristic.
    std::vector<int> order( aPlacements.size() );
    std::iota( order.begin(), order.end(), 0 );

    std::sort( order.begin(), order.end(),
        [&aPlacements]( int a, int b )
        {
            int viableA = 0, viableB = 0;

            for( const SILK_CANDIDATE& c : aPlacements[a].candidates )
            {
                if( !c.hardViolation && c.localCost == 0 )
                    viableA++;
            }

            for( const SILK_CANDIDATE& c : aPlacements[b].candidates )
            {
                if( !c.hardViolation && c.localCost == 0 )
                    viableB++;
            }

            return viableA < viableB;
        } );

    for( int idx : order )
    {
        SILK_PLACEMENT& placement = aPlacements[idx];

        if( placement.candidates.empty() )
            continue;

        int bestIdx = -1;
        int bestCost = INT_MAX;

        for( int i = 0; i < static_cast<int>( placement.candidates.size() ); i++ )
        {
            const SILK_CANDIDATE& cand = placement.candidates[i];

            if( cand.hardViolation )
                continue;

            SHAPE_RECT candShape( cand.bbox );

            // Check overlap with already-assigned labels in the solution tree
            bool overlapsExisting = m_solutionTree->CheckColliding(
                    &candShape, placement.silkLayer );

            int totalCost = cand.localCost + ( overlapsExisting ? COST_LABEL_OVERLAP : 0 );

            if( totalCost < bestCost )
            {
                bestCost = totalCost;
                bestIdx = i;
            }
        }

        if( bestIdx >= 0 )
        {
            placement.assignedCandidate = bestIdx;

            // Insert the assigned label bbox into the solution tree for future overlap checks.
            // PCB_SHAPE rectangles serve as carriers since DRC_RTREE requires BOARD_ITEM*.
            const SILK_CANDIDATE& chosen = placement.candidates[bestIdx];
            auto carrier = std::make_unique<PCB_SHAPE>();
            carrier->SetShape( SHAPE_T::RECTANGLE );
            carrier->SetStart( chosen.bbox.GetOrigin() );
            carrier->SetEnd( VECTOR2I( chosen.bbox.GetRight(), chosen.bbox.GetBottom() ) );
            carrier->SetLayer( placement.silkLayer );
            m_solutionTree->Insert( carrier.get(), placement.silkLayer );
            m_ownedCarriers.push_back( std::move( carrier ) );
        }
    }
}


void SILKSCREEN_AUTO_PLACER::runSimulatedAnnealing( std::vector<SILK_PLACEMENT>& aPlacements )
{
    // Solution tree was only needed for FALP greedy initialization
    m_solutionTree.reset();
    m_ownedCarriers.clear();

    int n = static_cast<int>( aPlacements.size() );

    if( n == 0 )
        return;

    // Check if we already have a perfect solution
    auto computeTotalCost = [&]() -> int
    {
        int total = 0;

        for( int i = 0; i < n; i++ )
        {
            if( aPlacements[i].assignedCandidate < 0 )
                continue;

            const SILK_CANDIDATE& c = aPlacements[i].candidates[aPlacements[i].assignedCandidate];
            total += c.localCost;
            total += countLabelOverlaps( aPlacements, i ) * COST_LABEL_OVERLAP;
        }

        return total;
    };

    if( computeTotalCost() == 0 )
        return;

    // T0 = -1 / ln(1/3) gives P(accept) = 2/3 when deltaE = 1 (Christensen et al. 1995)
    double T = -1.0 / std::log( 1.0 / 3.0 );
    std::uniform_int_distribution<int> placementDist( 0, n - 1 );
    std::uniform_real_distribution<double> acceptDist( 0.0, 1.0 );

    for( int stage = 0; stage < SA_MAX_STAGES; stage++ )
    {
        int accepted = 0;

        for( int move = 0; move < SA_MOVES_PER_LABEL * n; move++ )
        {
            int idx = placementDist( m_rng );
            SILK_PLACEMENT& placement = aPlacements[idx];

            if( placement.assignedCandidate < 0 )
                continue;

            // Build list of legal candidates excluding the current assignment
            std::vector<int> legalAlts;

            for( int i = 0; i < static_cast<int>( placement.candidates.size() ); i++ )
            {
                if( i != placement.assignedCandidate && !placement.candidates[i].hardViolation )
                    legalAlts.push_back( i );
            }

            if( legalAlts.empty() )
                continue;

            int oldIdx = placement.assignedCandidate;
            std::uniform_int_distribution<int> candDist( 0,
                    static_cast<int>( legalAlts.size() ) - 1 );
            int newIdx = legalAlts[candDist( m_rng )];

            int oldCost = placement.candidates[oldIdx].localCost
                          + countLabelOverlaps( aPlacements, idx ) * COST_LABEL_OVERLAP;

            placement.assignedCandidate = newIdx;

            int newCost = placement.candidates[newIdx].localCost
                          + countLabelOverlaps( aPlacements, idx ) * COST_LABEL_OVERLAP;

            int deltaE = newCost - oldCost;

            if( deltaE < 0 || acceptDist( m_rng ) < std::exp( -static_cast<double>( deltaE ) / T ) )
            {
                accepted++;
            }
            else
            {
                placement.assignedCandidate = oldIdx;
            }
        }

        T *= SA_COOLING_FACTOR;

        if( accepted == 0 )
            break;

        if( computeTotalCost() == 0 )
            break;
    }
}


void SILKSCREEN_AUTO_PLACER::applyPlacements( BOARD_COMMIT& aCommit,
                                               std::vector<SILK_PLACEMENT>& aPlacements )
{
    for( SILK_PLACEMENT& placement : aPlacements )
    {
        if( placement.assignedCandidate >= 0 )
        {
            const SILK_CANDIDATE& cand = placement.candidates[placement.assignedCandidate];

            if( !cand.hardViolation )
            {
                // Only move the ref if the candidate is actually better than the original.
                // A hard-violated original is always worth replacing; otherwise compare local cost.
                bool isBetter = placement.originalHardViolation
                                || cand.localCost < placement.originalCost;

                if( !isBetter )
                {
                    placement.assignedCandidate = -1;
                    continue;
                }

                aCommit.Modify( placement.footprint );
                placement.refField->SetPosition( cand.position );
                placement.refField->SetTextAngle( cand.angle );

                if( m_settings.fixedSize )
                {
                    placement.refField->SetTextSize(
                            VECTOR2I( m_settings.fixedSizeValue, m_settings.fixedSizeValue ) );
                }

                if( m_settings.fixedWidth )
                    placement.refField->SetTextThickness( m_settings.fixedWidthValue );

                continue;
            }

            placement.assignedCandidate = -1;
        }

        switch( m_settings.failedPlacement )
        {
        case SILK_FAILED_PLACEMENT::CENTER_OVER_COMPONENT:
            aCommit.Modify( placement.footprint );
            placement.refField->SetPosition( placement.outlineBBox.GetCenter() );
            placement.refField->SetTextAngle( computeAngle( placement.footprint ) );
            break;

        case SILK_FAILED_PLACEMENT::PLACE_OFF_BOARD:
        {
            aCommit.Modify( placement.footprint );
            BOX2I boardBBox = m_board->GetBoardEdgesBoundingBox();
            VECTOR2I offBoardPos( boardBBox.GetLeft() - pcbIUScale.mmToIU( 5 ),
                                  boardBBox.GetBottom() + pcbIUScale.mmToIU( 5 ) );
            placement.refField->SetPosition( offBoardPos );
            placement.refField->SetTextAngle( ANGLE_0 );
            break;
        }

        case SILK_FAILED_PLACEMENT::RESTORE_ORIGINAL:
            break;
        }
    }
}
