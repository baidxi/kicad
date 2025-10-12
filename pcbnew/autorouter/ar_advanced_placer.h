/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Enhanced Placement with Crossing Minimization
 * Author: Ayomide Adekoya
 */

#ifndef __AR_ADVANCED_PLACER_H
#define __AR_ADVANCED_PLACER_H

#include "ar_autoplacer.h"

/**
 * Enhanced placement optimizer that adds crossing minimization to the standard autoplacer.
 * 
 * The key innovation: The original KiCad autoplacer only considers distance when
 * placing components. This enhanced version also counts how many trace crossings
 * would be created and penalizes placements that create crossings.
 * 
 * Result: Fewer vias needed, cleaner routing, better signal integrity.
 */
class AR_ADVANCED_PLACER : public AR_AUTOPLACER
{
public:
    AR_ADVANCED_PLACER( BOARD* aBoard );
    
    /**
     * Override to use our cost function
     */
    AR_RESULT AutoplaceFootprints( std::vector<FOOTPRINT*>& aFootprints, 
                                   BOARD_COMMIT* aCommit,
                                   bool aPlaceOffboardModules = false ) override;
    
    /**
     * Enhanced cost function that includes crossing penalty
     * This is the CORE - we add penalty for crossings
     */
    double computePlacementRatsnestCost( FOOTPRINT* aFootprint, 
                                         const VECTOR2I& aOffset ) override;

private:
    /**
     * Count how many existing traces a proposed connection would cross
     */
    int CountCrossingsWithPlaced( const VECTOR2I& aStart, 
                                  const VECTOR2I& aEnd,
                                  FOOTPRINT* aMovingFootprint,
                                  int aNetCode );
    
    /**
     * Check if two line segments intersect (2D)
     */
    bool DoSegmentsIntersect( const VECTOR2I& p1, const VECTOR2I& q1,
                             const VECTOR2I& p2, const VECTOR2I& q2 );
    
    // The weight factor for crossing penalty (higher = avoid crossings more)
    double m_crossingWeight;
};

#endif // __AR_ADVANCED_PLACER_H