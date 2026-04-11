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

#ifndef SILKSCREEN_AUTO_PLACER_H
#define SILKSCREEN_AUTO_PLACER_H

#include <math/vector2d.h>
#include <math/box2.h>
#include <layer_ids.h>
#include <geometry/eda_angle.h>

#include <wx/string.h>

#include <memory>
#include <random>
#include <set>
#include <vector>

class BOARD;
class BOARD_COMMIT;
class BOARD_ITEM;
class DRC_RTREE;
class FOOTPRINT;
class PCB_FIELD;
class PROGRESS_REPORTER;

enum class SILK_ROTATION_STRATEGY
{
    KLC_STYLE,
    MATCH_COMPONENT
};

enum class SILK_FAILED_PLACEMENT
{
    CENTER_OVER_COMPONENT,
    PLACE_OFF_BOARD,
    RESTORE_ORIGINAL
};

enum SILK_POSITION_FLAG
{
    POS_TOP_CENTER  = 1 << 0,
    POS_CENTER_RIGHT = 1 << 1,
    POS_BOT_CENTER  = 1 << 2,
    POS_CENTER_LEFT = 1 << 3,
    POS_TOP_LEFT    = 1 << 4,
    POS_TOP_RIGHT   = 1 << 5,
    POS_BOT_LEFT    = 1 << 6,
    POS_BOT_RIGHT   = 1 << 7
};

struct SILK_PLACER_SETTINGS
{
    bool                    placeSelectedOnly = false;
    std::set<wxString>      allowSilkUnder;
    SILK_FAILED_PLACEMENT   failedPlacement = SILK_FAILED_PLACEMENT::RESTORE_ORIGINAL;
    bool                    avoidVias = true;
    SILK_ROTATION_STRATEGY  rotationStrategy = SILK_ROTATION_STRATEGY::KLC_STYLE;
    bool                    tryAlteredRotation = true;
    bool                    fixedSize = false;
    int                     fixedSizeValue = 0;
    bool                    fixedWidth = false;
    int                     fixedWidthValue = 0;
    int                     positionFlags = POS_TOP_CENTER | POS_CENTER_RIGHT
                                          | POS_BOT_CENTER | POS_CENTER_LEFT;
    int                     positionDelta = 0;
};


struct SILK_CANDIDATE
{
    VECTOR2I    position;
    EDA_ANGLE   angle;
    BOX2I       bbox;
    int         localCost = 0;
    bool        hardViolation = false;
};


struct SILK_PLACEMENT
{
    FOOTPRINT*                  footprint = nullptr;
    PCB_FIELD*                  refField = nullptr;
    PCB_LAYER_ID                silkLayer = F_SilkS;
    VECTOR2I                    originalPos;
    EDA_ANGLE                   originalAngle;
    bool                        relaxSilkOverlap = false;
    int                         originalCost = 0;
    bool                        originalHardViolation = false;
    BOX2I                       outlineBBox;
    std::vector<SILK_CANDIDATE> candidates;
    int                         assignedCandidate = -1;
};


struct SILK_PLACER_RESULT
{
    int attempted = 0;
    int moved = 0;
};


class SILKSCREEN_AUTO_PLACER
{
public:
    SILKSCREEN_AUTO_PLACER( BOARD* aBoard, const SILK_PLACER_SETTINGS& aSettings );
    ~SILKSCREEN_AUTO_PLACER();

    /**
     * Run the auto-placer on the given footprints.
     * @return attempted and moved counts.
     */
    SILK_PLACER_RESULT Run( BOARD_COMMIT& aCommit, const std::vector<FOOTPRINT*>& aFootprints,
                            PROGRESS_REPORTER* aProgress = nullptr );

private:
    void buildObstacleTrees( const std::vector<FOOTPRINT*>& aFootprints );

    BOX2I resolveOutlineBBox( FOOTPRINT* aFp, PCB_LAYER_ID aSilkLayer ) const;

    void generateCandidates( SILK_PLACEMENT& aPlacement );

    int scoreCandidateLocal( const SILK_PLACEMENT& aPlacement,
                             const SILK_CANDIDATE& aCandidate ) const;

    int countLabelOverlaps( const std::vector<SILK_PLACEMENT>& aPlacements,
                            int aIndex ) const;

    void runFALP( std::vector<SILK_PLACEMENT>& aPlacements );

    void runSimulatedAnnealing( std::vector<SILK_PLACEMENT>& aPlacements );

    void applyPlacements( BOARD_COMMIT& aCommit,
                          std::vector<SILK_PLACEMENT>& aPlacements );

    EDA_ANGLE computeAngle( FOOTPRINT* aFp, bool aPerpendicular = false ) const;

    void setupMeasurementText( PCB_TEXT& aText, const SILK_PLACEMENT& aPlacement ) const;

    BOX2I computeTextBBox( const SILK_PLACEMENT& aPlacement,
                           const SILK_CANDIDATE& aCand ) const;

    bool isInsideBoard( const BOX2I& aBBox ) const;

    BOARD*                                      m_board;
    SILK_PLACER_SETTINGS                        m_settings;
    std::unique_ptr<DRC_RTREE>                  m_obstacleTree;
    std::unique_ptr<DRC_RTREE>                  m_silkTree;
    std::unique_ptr<DRC_RTREE>                  m_solutionTree;
    std::vector<std::unique_ptr<BOARD_ITEM>>    m_ownedCarriers;
    std::mt19937                                m_rng;
};

#endif // SILKSCREEN_AUTO_PLACER_H
