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

#include "ar_advanced_placer.h"
#include <board_commit.h>
#include <pad.h>
#include <geometry/seg.h>

AR_ADVANCED_PLACER::AR_ADVANCED_PLACER( BOARD* aBoard ) :
        AR_AUTOPLACER( aBoard ),
        m_crossingWeight( 10.0 )
{
}

AR_RESULT AR_ADVANCED_PLACER::AutoplaceFootprints( std::vector<FOOTPRINT*>& aFootprints,
                                                   BOARD_COMMIT* aCommit,
                                                   bool aPlaceOffboardModules )
{
    return AR_AUTOPLACER::AutoplaceFootprints( aFootprints, aCommit, aPlaceOffboardModules );
}

double AR_ADVANCED_PLACER::computePlacementRatsnestCost( FOOTPRINT* aFootprint,
                                                         const VECTOR2I& aOffset )
{
    double baseCost = AR_AUTOPLACER::computePlacementRatsnestCost( aFootprint, aOffset );
    double crossingPenalty = 0.0;
    int    crossingCount = 0;

    for( PAD* pad : aFootprint->Pads() )
    {
        if( pad->GetNetCode() <= 0 )
            continue;

        const PAD* targetPad = nearestPad( aFootprint, pad, aOffset );

        if( !targetPad )
            continue;

        VECTOR2I proposedStart = pad->GetPosition() - aOffset;
        VECTOR2I proposedEnd = targetPad->GetPosition();

        crossingCount += CountCrossingsWithPlaced( proposedStart, proposedEnd,
                                                   aFootprint, pad->GetNetCode() );
    }

    crossingPenalty = crossingCount * m_crossingWeight * 10000.0;

    return baseCost + crossingPenalty;
}

int AR_ADVANCED_PLACER::CountCrossingsWithPlaced( const VECTOR2I& aStart,
                                                  const VECTOR2I& aEnd,
                                                  FOOTPRINT* aMovingFootprint,
                                                  int aNetCode )
{
    int crossings = 0;

    for( FOOTPRINT* placedFp : m_board->Footprints() )
    {
        if( placedFp == aMovingFootprint || placedFp->NeedsPlaced() )
            continue;

        for( PAD* placedPad : placedFp->Pads() )
        {
            if( placedPad->GetNetCode() <= 0 || placedPad->GetNetCode() == aNetCode )
                continue;

            const PAD* placedTarget = nearestPad( placedFp, placedPad, VECTOR2I( 0, 0 ) );

            if( !placedTarget )
                continue;

            SEG proposedSeg( aStart, aEnd );
            SEG placedSeg( placedPad->GetPosition(), placedTarget->GetPosition() );

            if( proposedSeg.Intersects( placedSeg ) )
                crossings++;
        }
    }

    return crossings;
}
