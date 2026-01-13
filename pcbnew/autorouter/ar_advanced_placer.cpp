/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Enhanced Placement with Crossing Minimization
 * Author: Ayomide Adekoya
 * 
 * This is a minimal enhancement to the existing autoplacer that adds
 * crossing detection to the cost function. The key change is that
 * the original autoplacer ONLY considers distance, while this version
 * penalizes placements that would create trace crossings.
 */

#include "ar_advanced_placer.h"
#include <board_commit.h>
#include <pad.h>
#include <geometry/seg.h>

AR_ADVANCED_PLACER::AR_ADVANCED_PLACER( BOARD* aBoard ) 
    : AR_AUTOPLACER( aBoard ),
      m_crossingWeight( 10.0 )  // This controls how much we penalize crossings
{
}

AR_RESULT AR_ADVANCED_PLACER::AutoplaceFootprints( std::vector<FOOTPRINT*>& aFootprints,
                                                   BOARD_COMMIT* aCommit,
                                                   bool aPlaceOffboardModules )
{
    // Simply use the parent implementation - it will call our enhanced cost function
    return AR_AUTOPLACER::AutoplaceFootprints( aFootprints, aCommit, aPlaceOffboardModules );
}

double AR_ADVANCED_PLACER::computePlacementRatsnestCost( FOOTPRINT* aFootprint, 
                                                         const VECTOR2I& aOffset )
{
    // Start with the original cost (based on connection length)
    double baseCost = AR_AUTOPLACER::computePlacementRatsnestCost( aFootprint, aOffset );
    
    // Now add our KEY change: crossing penalty
    double crossingPenalty = 0.0;
    int crossingCount = 0;
    
    // For each connection this footprint would create
    for( PAD* pad : aFootprint->Pads() )
    {
        if( pad->GetNetCode() <= 0 )
            continue;
            
        // Find where this pad wants to connect
        const PAD* targetPad = nearestPad( aFootprint, pad, aOffset );
        if( !targetPad )
            continue;
            
        // This is the proposed connection line
        VECTOR2I proposedStart = pad->GetPosition() - aOffset;
        VECTOR2I proposedEnd = targetPad->GetPosition();
        
        // Check if it crosses existing connections
        crossingCount += CountCrossingsWithPlaced( proposedStart, proposedEnd, 
                                                   aFootprint, pad->GetNetCode() );
    }
    
    // Heavy penalty for each crossing - this is what makes placement avoid crossings!
    crossingPenalty = crossingCount * m_crossingWeight * 10000.0;
    
    return baseCost + crossingPenalty;
}

int AR_ADVANCED_PLACER::CountCrossingsWithPlaced( const VECTOR2I& aStart, 
                                                  const VECTOR2I& aEnd,
                                                  FOOTPRINT* aMovingFootprint,
                                                  int aNetCode )
{
    int crossings = 0;
    
    // Check against all placed footprints
    for( FOOTPRINT* placedFp : m_board->Footprints() )
    {
        // Skip the footprint we're placing and those not yet placed
        if( placedFp == aMovingFootprint || placedFp->NeedsPlaced() )
            continue;
            
        // Check each connection from the placed footprint
        for( PAD* placedPad : placedFp->Pads() )
        {
            // Skip if same net (connections on same net don't "cross")
            if( placedPad->GetNetCode() <= 0 || placedPad->GetNetCode() == aNetCode )
                continue;
                
            // Find this pad's target
            const PAD* placedTarget = nearestPad( placedFp, placedPad, VECTOR2I(0,0) );
            if( !placedTarget )
                continue;
                
            SEG proposedSeg( aStart, aEnd );
            SEG placedSeg( placedPad->GetPosition(), placedTarget->GetPosition() );

            if( proposedSeg.Intersects( placedSeg ) )
            {
                crossings++;
            }
        }
    }
    
    return crossings;
}
