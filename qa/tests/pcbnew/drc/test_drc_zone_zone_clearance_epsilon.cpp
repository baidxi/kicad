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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <memory>

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <board.h>
#include <board_design_settings.h>
#include <netinfo.h>
#include <zone.h>
#include <geometry/shape_poly_set.h>
#include <geometry/shape_line_chain.h>
#include <drc/drc_engine.h>
#include <drc/drc_item.h>


struct DRC_ZONE_ZONE_EPSILON_FIXTURE
{
    static SHAPE_LINE_CHAIN rect( int aX0, int aY0, int aX1, int aY1 )
    {
        SHAPE_LINE_CHAIN chain;
        chain.Append( aX0, aY0 );
        chain.Append( aX1, aY0 );
        chain.Append( aX1, aY1 );
        chain.Append( aX0, aY1 );
        chain.SetClosed( true );
        return chain;
    }

    // Add a copper zone on F.Cu with a directly-assigned (not re-poured) fill.  This models
    // an imported board whose fills are stored verbatim, exactly like the Altium importer.
    static ZONE* addFilledZone( BOARD* aBoard, int aNetCode, int aClearance, const SHAPE_POLY_SET& aFill )
    {
        ZONE* zone = new ZONE( aBoard );
        zone->SetLayer( F_Cu );
        zone->SetNetCode( aNetCode );
        zone->SetAssignedPriority( 0 );
        zone->SetLocalClearance( aClearance );

        *zone->Outline() = aFill;
        zone->SetFilledPolysList( F_Cu, aFill );
        zone->SetIsFilled( true );
        zone->SetNeedRefill( false );

        aBoard->Add( zone, ADD_MODE::APPEND );
        return zone;
    }

    // Count zone-to-zone copper clearance violations for two interlocking fills whose nearest
    // approach is (clearance - aDeficit).  Zone B is nested inside a square hole cut into zone
    // A, so the two fills' bounding boxes overlap (satisfying the broad-phase filter) while the
    // copper stays a uniform gap apart on all four sides.
    static int countClearanceViolations( int aDeficit )
    {
        auto board = std::make_unique<BOARD>();

        BOARD_DESIGN_SETTINGS& bds = board->GetDesignSettings();

        const int clearance = pcbIUScale.mmToIU( 0.2 );
        const int gap = clearance - aDeficit;

        // Zone A: 3mm square with a 1mm square hole centred on (1.5mm, 1.5mm).
        const int outer = pcbIUScale.mmToIU( 3.0 );
        const int holeLo = pcbIUScale.mmToIU( 1.0 );
        const int holeHi = pcbIUScale.mmToIU( 2.0 );

        SHAPE_POLY_SET fillA;
        fillA.AddOutline( rect( 0, 0, outer, outer ) );
        fillA.AddHole( rect( holeLo, holeLo, holeHi, holeHi ) );

        // Zone B: square nested inside the hole, uniformly 'gap' away from every hole edge.
        SHAPE_POLY_SET fillB;
        fillB.AddOutline( rect( holeLo + gap, holeLo + gap, holeHi - gap, holeHi - gap ) );

        bds.m_MinClearance = clearance;

        NETINFO_ITEM* netA = new NETINFO_ITEM( board.get(), wxT( "A" ), 1 );
        NETINFO_ITEM* netB = new NETINFO_ITEM( board.get(), wxT( "B" ), 2 );
        board->Add( netA );
        board->Add( netB );

        addFilledZone( board.get(), netA->GetNetCode(), clearance, fillA );
        addFilledZone( board.get(), netB->GetNetCode(), clearance, fillB );

        board->BuildConnectivity();

        bds.m_DRCEngine = std::make_shared<DRC_ENGINE>( board.get(), &bds );
        bds.m_DRCEngine->InitEngine( wxFileName() );

        int clearanceViolations = 0;

        bds.m_DRCEngine->SetViolationHandler(
                [&]( const std::shared_ptr<DRC_ITEM>& aItem, const VECTOR2I&, int,
                     const std::function<void( PCB_MARKER* )>& )
                {
                    if( aItem->GetErrorCode() == DRCE_CLEARANCE )
                        clearanceViolations++;
                } );

        bds.m_DRCEngine->RunTests( EDA_UNITS::MM, true, false );

        return clearanceViolations;
    }
};


BOOST_FIXTURE_TEST_SUITE( DRCZoneZoneClearanceEpsilon, DRC_ZONE_ZONE_EPSILON_FIXTURE )


/**
 * Imported fills are stored verbatim and never re-poured, so rounding can leave two zones a
 * couple of nanometres short of the clearance rule.  The zone-to-zone clearance check must
 * apply the shared DRC epsilon, just like every other copper clearance test, and not report a
 * violation for a gap that is within epsilon of the rule.
 */
BOOST_AUTO_TEST_CASE( GapWithinEpsilonIsNotReported )
{
    // 100 nm short of a 0.2 mm clearance is well inside the default 500 nm DRC epsilon.
    BOOST_CHECK_EQUAL( countClearanceViolations( 100 ), 0 );
}


/**
 * A gap that is short by far more than the DRC epsilon must still be reported; the epsilon
 * tolerance must not silently swallow genuine clearance violations.
 */
BOOST_AUTO_TEST_CASE( GapBeyondEpsilonIsReported )
{
    // 0.1 mm short of a 0.2 mm clearance is two orders of magnitude beyond the epsilon.
    BOOST_CHECK_GE( countClearanceViolations( pcbIUScale.mmToIU( 0.1 ) ), 1 );
}


BOOST_AUTO_TEST_SUITE_END()
