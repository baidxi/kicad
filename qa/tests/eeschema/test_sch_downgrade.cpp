/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <boost/test/unit_test.hpp>

#include <sch_screen.h>
#include <sch_shape.h>
#include <lib_symbol.h>
#include <sch_file_versions.h>
#include <downgrade/sch_downgrade.h>
#include <downgrade_target.h>


BOOST_AUTO_TEST_SUITE( SchDowngrade )


// A library symbol is downgraded through its own path. Jumper pin groups are dropped for 9.0.
BOOST_AUTO_TEST_CASE( LibSymbolJumperPinsDroppedForKicad9 )
{
    LIB_SYMBOL sym( wxT( "test" ) );
    sym.SetDuplicatePinNumbersAreJumpers( true );

    DowngradeLibSymbolInPlace( &sym, GetDowngradeTargets().at( 1 ) );

    BOOST_CHECK( !sym.GetDuplicatePinNumbersAreJumpers() );
}


// The schematic ellipse primitive only exists since 20260508, so 10.0 gets an approximation.
BOOST_AUTO_TEST_CASE( EllipseIsLoweredForKicad10 )
{
    SCH_SCREEN screen;
    SCH_SHAPE* ellipse = new SCH_SHAPE( SHAPE_T::ELLIPSE );
    ellipse->SetStart( VECTOR2I( 0, 0 ) );
    ellipse->SetEnd( VECTOR2I( 500000, 0 ) );
    screen.Append( ellipse );

    const DOWNGRADE_TARGET& kicad10 = GetDowngradeTargets().front();
    COMPATIBILITY_REPORT    report = ClassifyScreenForDowngrade( &screen, kicad10 );

    BOOST_CHECK( report.IsLossy() );
    BOOST_CHECK_EQUAL( report.Count( DOWNGRADE_BUCKET::LOWER ), 1 );
}


// The Lower transform actually rewrites the ellipse as a polygon.
BOOST_AUTO_TEST_CASE( EllipseTransformedToPolygon )
{
    SCH_SCREEN screen;
    SCH_SHAPE* ellipse = new SCH_SHAPE( SHAPE_T::ELLIPSE );
    ellipse->SetStart( VECTOR2I( 0, 0 ) );
    ellipse->SetEnd( VECTOR2I( 500000, 0 ) );
    screen.Append( ellipse );

    DowngradeScreenInPlace( &screen, GetDowngradeTargets().front() );

    BOOST_CHECK( ellipse->GetShape() == SHAPE_T::POLY );
}


// Hatched fills postdate 9.0, so they become a solid fill for 9.0.
BOOST_AUTO_TEST_CASE( HatchedFillLoweredForKicad9 )
{
    SCH_SCREEN screen;
    SCH_SHAPE* shape = new SCH_SHAPE( SHAPE_T::RECTANGLE );
    shape->SetFillMode( FILL_T::HATCH );
    screen.Append( shape );

    const DOWNGRADE_TARGET& kicad9 = GetDowngradeTargets().at( 1 );

    BOOST_CHECK_EQUAL( ClassifyScreenForDowngrade( &screen, kicad9 ).Count( DOWNGRADE_BUCKET::LOWER ), 1 );

    DowngradeScreenInPlace( &screen, kicad9 );

    BOOST_CHECK( shape->GetFillMode() == FILL_T::FILLED_SHAPE );
}


// Symbol bodies are written from the screen library cache, so an ellipse there must be
// counted and lowered too, not just shapes placed on the screen.
BOOST_AUTO_TEST_CASE( LibSymbolEllipseLoweredForKicad10 )
{
    SCH_SCREEN  screen;
    LIB_SYMBOL* symbol = new LIB_SYMBOL( wxT( "test" ) );
    SCH_SHAPE*  ellipse = new SCH_SHAPE( SHAPE_T::ELLIPSE );
    ellipse->SetStart( VECTOR2I( 0, 0 ) );
    ellipse->SetEnd( VECTOR2I( 500000, 0 ) );
    symbol->AddDrawItem( ellipse );
    screen.AddLibSymbol( symbol );

    const DOWNGRADE_TARGET& kicad10 = GetDowngradeTargets().front();

    BOOST_CHECK_EQUAL( ClassifyScreenForDowngrade( &screen, kicad10 ).Count( DOWNGRADE_BUCKET::LOWER ), 1 );

    DowngradeScreenInPlace( &screen, kicad10 );

    BOOST_CHECK( ellipse->GetShape() == SHAPE_T::POLY );
}


// Item locking (20260326) does not exist in 10.0, so the lock is dropped.
BOOST_AUTO_TEST_CASE( LockedItemUnlockedForKicad10 )
{
    SCH_SCREEN screen;
    SCH_SHAPE* shape = new SCH_SHAPE( SHAPE_T::RECTANGLE );
    shape->SetLocked( true );
    screen.Append( shape );

    const DOWNGRADE_TARGET& kicad10 = GetDowngradeTargets().front();

    BOOST_CHECK_EQUAL( ClassifyScreenForDowngrade( &screen, kicad10 ).Count( DOWNGRADE_BUCKET::LOWER ), 1 );

    DowngradeScreenInPlace( &screen, kicad10 );

    BOOST_CHECK( !shape->IsLocked() );
}


// The schematic fail-closed gate catches post-10.0 tokens and ignores lookalikes.
BOOST_AUTO_TEST_CASE( VerifyGateCatchesUnsupportedTokens )
{
    const DOWNGRADE_TARGET& kicad10 = GetDowngradeTargets().front();

    BOOST_CHECK( FindUnsupportedSchToken( wxT( "(symbol (pin_map_override (mode passthrough)))" ), kicad10 )
                 == wxT( "pin_map_override" ) );
    BOOST_CHECK( FindUnsupportedSchToken( wxT( "(net_chain \"n\")" ), kicad10 ) == wxT( "net_chain" ) );
    BOOST_CHECK( FindUnsupportedSchToken( wxT( "(kicad_sch (symbol (pin \"1\")))" ), kicad10 ).IsEmpty() );

    // Value tokens the node scan cannot see must still be caught for 9.0, but not inside strings.
    const DOWNGRADE_TARGET& kicad9 = GetDowngradeTargets().at( 1 );

    BOOST_CHECK( FindUnsupportedSchToken( wxT( "(fill (type hatch))" ), kicad9 ) == wxT( "hatch" ) );
    BOOST_CHECK( FindUnsupportedSchToken( wxT( "(text \"hatch pattern\")" ), kicad9 ).IsEmpty() );
}


// Coverage guard: a schematic format bump must trigger a denylist review. See the board equivalent.
BOOST_AUTO_TEST_CASE( DenylistCoversCurrentFormat )
{
    BOOST_CHECK_EQUAL( SEXPR_SCHEMATIC_FILE_VERSION, SchDowngradeCoveredVersion() );
}


BOOST_AUTO_TEST_SUITE_END()
