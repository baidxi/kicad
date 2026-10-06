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

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <algorithm>

#include <board_stackup_manager/board_stackup.h>
#include <widgets/via_cross_section_geometry.h>

using GEOMETRY = VIA_CROSS_SECTION_GEOMETRY;
using MATERIAL = GEOMETRY::MATERIAL;
using STATE = GEOMETRY::STATE;


namespace
{

/// A through via on a four layer board, tented both sides, nothing else done to it
STATE defaultState()
{
    STATE state;
    state.m_InnerLayers = 2;
    state.m_TentFront = true;
    state.m_TentBack = true;
    state.m_CoverFront = false;
    state.m_CoverBack = false;
    state.m_PlugFront = false;
    state.m_PlugBack = false;
    state.m_Filled = false;
    state.m_Capped = false;
    return state;
}


/// Shapes of a body that span the hole, which only a tent, a cap, a fill or a plug does
std::vector<GEOMETRY::PIECE> bridgingShapes( const GEOMETRY::BODY& aBody )
{
    const double                 centre = GEOMETRY::GRID_W / 2;
    std::vector<GEOMETRY::PIECE> bridging;

    for( const GEOMETRY::PIECE& shape : aBody.m_Pieces )
    {
        if( !shape.m_Rect )
        {
            bridging.push_back( shape );
            continue;
        }

        if( shape.Left() < centre && shape.Right() > centre )
            bridging.push_back( shape );
    }

    return bridging;
}


BOARD_STACKUP_ITEM* stackupItem( BOARD_STACKUP& aStackup, BOARD_STACKUP_ITEM_TYPE aType, int aThickness )
{
    BOARD_STACKUP_ITEM* item = new BOARD_STACKUP_ITEM( aType );
    item->SetThickness( aThickness );
    item->SetEnabled( true );
    aStackup.Add( item );
    return item;
}

} // namespace


BOOST_AUTO_TEST_SUITE( ViaCrossSectionGeometry )


// The grid grows with the layer count, so the widget can ask for a taller minimum size.
BOOST_AUTO_TEST_CASE( GridHeightFollowsLayerCount )
{
    BOOST_CHECK_EQUAL( GEOMETRY::GridHeightFor( 2 ), GEOMETRY::GridHeightFor( 0 ) );
    BOOST_CHECK_GT( GEOMETRY::GridHeightFor( 12 ), GEOMETRY::GridHeightFor( 2 ) );

    STATE state = defaultState();
    state.m_InnerLayers = 12;

    BOOST_CHECK_EQUAL( GEOMETRY( state ).GridHeight(), GEOMETRY::GridHeightFor( 12 ) );
}


// Bodies come out in paint order, each with the right clip, so the painter can be dumb.
BOOST_AUTO_TEST_CASE( BodiesInPaintOrder )
{
    GEOMETRY geometry( defaultState() );

    BOOST_REQUIRE_EQUAL( geometry.Bodies().size(), 5u );
    BOOST_CHECK( geometry.Bodies()[0].m_Material == MATERIAL::SOLDER_MASK );
    BOOST_CHECK( geometry.Bodies()[1].m_Material == MATERIAL::CORE );
    BOOST_CHECK( geometry.Bodies()[2].m_Material == MATERIAL::PLUG );
    BOOST_CHECK( geometry.Bodies()[3].m_Material == MATERIAL::COPPER );
    BOOST_CHECK( geometry.Bodies()[4].m_Material == MATERIAL::COVER );

    BOOST_CHECK( geometry.Body( MATERIAL::SOLDER_MASK ).m_Clip == GEOMETRY::CLIP::OVER_MASK );
    BOOST_CHECK( geometry.Body( MATERIAL::COPPER ).m_Clip == GEOMETRY::CLIP::UNDER_MASK );
    BOOST_CHECK( geometry.Body( MATERIAL::COVER ).m_Clip == GEOMETRY::CLIP::OVER_MASK );
}


// Without a stackup the inner layers are spaced evenly and sit inside the core.
BOOST_AUTO_TEST_CASE( EvenLayerSpacing )
{
    STATE state = defaultState();
    state.m_InnerLayers = 3;

    GEOMETRY geometry( state );

    BOOST_CHECK_EQUAL( geometry.InnerLayers(), 3 );

    const double gap1 = geometry.LayerCentre( 2 ) - geometry.LayerCentre( 1 );
    const double gap2 = geometry.LayerCentre( 3 ) - geometry.LayerCentre( 2 );

    BOOST_CHECK_CLOSE( gap1, gap2, 1e-9 );
    BOOST_CHECK_LT( geometry.LayerCentre( 0 ), geometry.LayerCentre( 1 ) );
    BOOST_CHECK_LT( geometry.LayerCentre( 3 ), geometry.LayerCentre( 4 ) );
}


// With a stackup the inner layers sit at their real depth, so an asymmetric board draws
// asymmetric.
BOOST_AUTO_TEST_CASE( LayersFollowStackup )
{
    BOARD_STACKUP stackup;
    stackupItem( stackup, BS_ITEM_TYPE_COPPER, 35 );
    stackupItem( stackup, BS_ITEM_TYPE_DIELECTRIC, 100 );
    stackupItem( stackup, BS_ITEM_TYPE_COPPER, 35 );
    stackupItem( stackup, BS_ITEM_TYPE_DIELECTRIC, 1000 );
    stackupItem( stackup, BS_ITEM_TYPE_COPPER, 35 );
    stackupItem( stackup, BS_ITEM_TYPE_DIELECTRIC, 100 );
    stackupItem( stackup, BS_ITEM_TYPE_COPPER, 35 );

    STATE state = defaultState();
    int   thickness = GEOMETRY::SetLayersFromStackup( state, stackup, 4 );

    BOOST_CHECK_EQUAL( thickness, 1340 );
    BOOST_REQUIRE_EQUAL( state.m_LayerDepths.size(), 4u );
    BOOST_CHECK_LT( state.m_LayerDepths[1], 0.2 );
    BOOST_CHECK_GT( state.m_LayerDepths[2], 0.8 );

    GEOMETRY geometry( state );

    // Inner layer 1 is close to the front, inner layer 2 close to the back
    const double span = geometry.LayerCentre( 3 ) - geometry.LayerCentre( 0 );
    BOOST_CHECK_LT( geometry.LayerCentre( 1 ) - geometry.LayerCentre( 0 ), span / 4 );
    BOOST_CHECK_LT( geometry.LayerCentre( 3 ) - geometry.LayerCentre( 2 ), span / 4 );

    // A stackup that does not match the copper count is ignored rather than misapplied
    STATE mismatched = defaultState();
    GEOMETRY::SetLayersFromStackup( mismatched, stackup, 6 );
    BOOST_CHECK( mismatched.m_LayerDepths.empty() );
}


// A tent is the one mask shape that bridges the hole, on each side it is asked for.
BOOST_AUTO_TEST_CASE( TentBridgesTheOpening )
{
    STATE state = defaultState();
    state.m_TentBack = false;

    GEOMETRY geometry( state );
    auto     bridging = bridgingShapes( geometry.Body( MATERIAL::SOLDER_MASK ) );

    BOOST_REQUIRE_EQUAL( bridging.size(), 1u );
    BOOST_CHECK_LT( bridging[0].Top(), geometry.LayerCentre( 0 ) );
    BOOST_CHECK( !bridging[0].m_Mixed );
}


// A value left unset is mixed across a selection: still drawn, but marked for hatching.
BOOST_AUTO_TEST_CASE( MixedValuesAreHatched )
{
    STATE state = defaultState();
    state.m_TentFront.reset();
    state.m_Filled.reset();

    GEOMETRY geometry( state );

    auto tents = bridgingShapes( geometry.Body( MATERIAL::SOLDER_MASK ) );
    BOOST_REQUIRE_EQUAL( tents.size(), 2u );
    BOOST_CHECK( tents[0].m_Mixed );
    BOOST_CHECK( !tents[1].m_Mixed );

    auto fills = bridgingShapes( geometry.Body( MATERIAL::PLUG ) );
    BOOST_REQUIRE_EQUAL( fills.size(), 1u );
    BOOST_CHECK( fills[0].m_Mixed );
}


// A blind via reaches one surface only. The other side has no opening, no tent and no pad,
// its mask and copper simply run across.
BOOST_AUTO_TEST_CASE( BlindViaKeepsTheFarSideClosed )
{
    STATE state = defaultState();
    state.m_TopLayer = 0;
    state.m_BottomLayer = 1;

    GEOMETRY geometry( state );

    const GEOMETRY::BODY& mask = geometry.Body( MATERIAL::SOLDER_MASK );
    const double          backSurface = geometry.LayerCentre( 3 );

    // The back mask is one full-width band
    int backBands = 0;

    for( const GEOMETRY::PIECE& shape : mask.m_Pieces )
    {
        if( shape.Top() >= backSurface )
        {
            backBands++;
            BOOST_CHECK_CLOSE( shape.Right() - shape.Left(), GEOMETRY::GRID_W - 20, 1e-9 );
        }
    }

    BOOST_CHECK_EQUAL( backBands, 1 );

    // The barrel stops at inner layer 1 instead of the back
    double barrelBottom = 0;

    for( const GEOMETRY::PIECE& shape : geometry.Body( MATERIAL::COPPER ).m_Pieces )
    {
        if( shape.Right() - shape.Left() < 10 ) // the two barrel walls
            barrelBottom = std::max( barrelBottom, shape.Bottom() );
    }

    BOOST_CHECK_GT( barrelBottom, geometry.LayerCentre( 1 ) );
    BOOST_CHECK_LT( barrelBottom, geometry.LayerCentre( 2 ) );
}


// A buried hole fills with resin at lamination, so the core bridges it.
BOOST_AUTO_TEST_CASE( BuriedViaIsFilledWithCore )
{
    STATE state = defaultState();
    state.m_TopLayer = 1;
    state.m_BottomLayer = 2;

    GEOMETRY geometry( state );

    // Neither surface has an opening: the mask is one full band on each side
    const GEOMETRY::BODY& mask = geometry.Body( MATERIAL::SOLDER_MASK );
    BOOST_REQUIRE_EQUAL( mask.m_Pieces.size(), 2u );

    for( const GEOMETRY::PIECE& piece : mask.m_Pieces )
        BOOST_CHECK_CLOSE( piece.Right() - piece.Left(), GEOMETRY::GRID_W - 20, 1e-9 );

    BOOST_CHECK_EQUAL( bridgingShapes( geometry.Body( MATERIAL::CORE ) ).size(), 3u );
    BOOST_CHECK( geometry.Bores().empty() );
}


// A backdrill is a bore from the surface to half way past the last removed layer, and that
// side loses its tent because there is no pad left to tent.
BOOST_AUTO_TEST_CASE( BackdrillDepthFollowsLayers )
{
    STATE state = defaultState();
    state.m_InnerLayers = 4;
    state.m_BackdrillBackLayers = 2;

    GEOMETRY geometry( state );

    BOOST_REQUIRE_EQUAL( geometry.Bores().size(), 1u );

    const GEOMETRY::PIECE& bore = geometry.Bores()[0];

    // The count includes the surface layer: two from the back takes the back layer and
    // inner 4, so the bore ends half way between inner 3 and inner 4
    BOOST_CHECK_GT( bore.Top(), geometry.LayerCentre( 3 ) );
    BOOST_CHECK_LT( bore.Top(), geometry.LayerCentre( 4 ) );
    BOOST_CHECK_GT( bore.Bottom(), geometry.LayerCentre( 5 ) );

    auto tents = bridgingShapes( geometry.Body( MATERIAL::SOLDER_MASK ) );
    BOOST_REQUIRE_EQUAL( tents.size(), 1u );
    BOOST_CHECK_LT( tents[0].Top(), geometry.LayerCentre( 0 ) );

    // A backdrill is a through via operation
    state.m_BottomLayer = 3;
    BOOST_CHECK( GEOMETRY( state ).Bores().empty() );
}


// An inner layer the via spans keeps its ring unless told otherwise, and then stands clear.
BOOST_AUTO_TEST_CASE( RingsCanBeRemoved )
{
    STATE state = defaultState();
    state.m_RingLayers = { true, true, false, true };

    GEOMETRY geometry( state );

    const double layer1 = geometry.LayerCentre( 1 );
    const double layer2 = geometry.LayerCentre( 2 );
    double       ring1Right = 0;
    double       ring2Right = 0;

    for( const GEOMETRY::PIECE& shape : geometry.Body( MATERIAL::COPPER ).m_Pieces )
    {
        if( shape.Left() > 10.5 ) // the pieces starting at the board edge are the left halves
            continue;

        if( shape.Top() < layer1 && shape.Bottom() > layer1 )
            ring1Right = shape.Right();

        if( shape.Top() < layer2 && shape.Bottom() > layer2 )
            ring2Right = shape.Right();
    }

    BOOST_CHECK_GT( ring1Right, ring2Right );
}


// Post machining is a cut out of everything: a counterbore is a box, a countersink a cone,
// and neither is made on a surface the via does not reach.
BOOST_AUTO_TEST_CASE( PostMachiningShapes )
{
    STATE state = defaultState();
    state.m_PostMachiningFront.m_Mode = PAD_DRILL_POST_MACHINING_MODE::COUNTERSINK;
    state.m_PostMachiningBack.m_Mode = PAD_DRILL_POST_MACHINING_MODE::COUNTERBORE;
    state.m_PostMachiningBack.m_DepthFraction = 0.25;

    GEOMETRY geometry( state );

    BOOST_REQUIRE_EQUAL( geometry.Cuts().size(), 2u );
    BOOST_CHECK( !geometry.Cuts()[0].m_Rect );
    BOOST_CHECK_EQUAL( geometry.Cuts()[0].m_Points.size(), 6u );
    BOOST_CHECK( geometry.Cuts()[1].m_Rect );

    // A quarter of the board from the back ends between the two inner layers
    BOOST_CHECK_GT( geometry.Cuts()[1].Top(), geometry.LayerCentre( 2 ) );
    BOOST_CHECK_LT( geometry.Cuts()[1].Top(), geometry.LayerCentre( 3 ) );

    state.m_BottomLayer = 1;
    BOOST_CHECK_EQUAL( GEOMETRY( state ).Cuts().size(), 1u );
}


// The features a fab cannot build together are not drawn together, the same rules Board
// Setup greys out with.
BOOST_AUTO_TEST_CASE( ProtectionRulesApply )
{
    // A cap needs a fill
    STATE capOnly = defaultState();
    capOnly.m_Capped = true;
    BOOST_CHECK( bridgingShapes( GEOMETRY( capOnly ).Body( MATERIAL::COPPER ) ).empty() );

    capOnly.m_Filled = true;
    BOOST_CHECK_EQUAL( bridgingShapes( GEOMETRY( capOnly ).Body( MATERIAL::COPPER ) ).size(), 2u );

    // A fill supersedes a plug
    STATE plugged = defaultState();
    plugged.m_PlugFront = true;
    BOOST_CHECK_EQUAL( bridgingShapes( GEOMETRY( plugged ).Body( MATERIAL::PLUG ) ).size(), 1u );

    plugged.m_Filled = true;
    auto fills = bridgingShapes( GEOMETRY( plugged ).Body( MATERIAL::PLUG ) );
    BOOST_REQUIRE_EQUAL( fills.size(), 1u );
    BOOST_CHECK( fills[0].m_Rect );

    // A cover needs something under it
    STATE coverOnly = defaultState();
    coverOnly.m_TentFront = false;
    coverOnly.m_CoverFront = true;
    BOOST_CHECK( GEOMETRY( coverOnly ).Body( MATERIAL::COVER ).m_Pieces.empty() );

    coverOnly.m_PlugFront = true;
    BOOST_CHECK_EQUAL( GEOMETRY( coverOnly ).Body( MATERIAL::COVER ).m_Pieces.size(), 2u );

    STATE covered = defaultState();
    covered.m_CoverFront = true;
    BOOST_CHECK_EQUAL( GEOMETRY( covered ).Body( MATERIAL::COVER ).m_Pieces.size(), 1u );
}


BOOST_AUTO_TEST_SUITE_END()
