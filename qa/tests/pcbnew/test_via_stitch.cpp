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
 */

#include <boost/test/unit_test.hpp>

#include <filesystem>
#include <set>

#include <base_units.h>
#include <board.h>
#include <board_commit.h>
#include <board_design_settings.h>
#include <drc/drc_engine.h>
#include <generators/pcb_via_stitch.h>
#include <netinfo.h>
#include <pcb_track.h>
#include <tool/tool_manager.h>
#include <zone.h>

#include <pcbnew_utils/board_file_utils.h>
#include <pcbnew_utils/board_test_utils.h>
#include <qa_utils/wx_utils/unit_test_utils.h>

BOOST_AUTO_TEST_SUITE( ViaStitch )

static SHAPE_POLY_SET makeRectPoly( int aX1, int aY1, int aX2, int aY2 )
{
    SHAPE_POLY_SET poly;
    poly.NewOutline();
    poly.Append( aX1, aY1 );
    poly.Append( aX2, aY1 );
    poly.Append( aX2, aY2 );
    poly.Append( aX1, aY2 );
    return poly;
}


static void configureStitch( PCB_VIA_STITCH* aStitch )
{
    aStitch->SetOutline( makeRectPoly( pcbIUScale.mmToIU( 2 ), pcbIUScale.mmToIU( 2 ),
                                       pcbIUScale.mmToIU( 18 ), pcbIUScale.mmToIU( 18 ) ) );
    aStitch->SetPitch( pcbIUScale.mmToIU( 2 ) );
    aStitch->SetLayout( PCB_VIA_STITCH_LAYOUT::STAGGERED );
    aStitch->SetMode( PCB_VIA_STITCH_MODE::STITCH );
    aStitch->SetSeed( 1234 );
    aStitch->ViaTemplate()->SetWidth( pcbIUScale.mmToIU( 0.6 ) );
    aStitch->ViaTemplate()->SetDrill( pcbIUScale.mmToIU( 0.3 ) );
    aStitch->SetNetCode( 1 );
}


static void checkOutlinesEqual( const SHAPE_POLY_SET& aExpected, const SHAPE_POLY_SET& aActual )
{
    BOOST_REQUIRE_EQUAL( aActual.OutlineCount(), aExpected.OutlineCount() );

    const SHAPE_LINE_CHAIN& expected = aExpected.COutline( 0 );
    const SHAPE_LINE_CHAIN& actual = aActual.COutline( 0 );

    BOOST_REQUIRE_EQUAL( actual.PointCount(), expected.PointCount() );

    for( int i = 0; i < expected.PointCount(); ++i )
        BOOST_CHECK_EQUAL( actual.CPoint( i ), expected.CPoint( i ) );
}


BOOST_AUTO_TEST_CASE( BakedPatternConstantsLocked )
{
    // Extra idiot checking that nobody touches the constants
    // Or else existing boards will change stitch patterns
    BOOST_CHECK_EQUAL( PCB_VIA_STITCH::POISSON_TILE_SEED, 0x542c21fcu );
    BOOST_CHECK_EQUAL( PCB_VIA_STITCH::POISSON_TILE_PITCHES, 8 );
    BOOST_CHECK( !PCB_VIA_STITCH::bakedPoissonTile().empty() );
}


BOOST_AUTO_TEST_CASE( PropertiesRoundTrip )
{
    BOARD board;
    board.Add( new NETINFO_ITEM( &board, wxT( "GND" ), 1 ) );

    PCB_VIA_STITCH a;
    a.SetParent( &board );
    configureStitch( &a );
    a.ExcludePosition( VECTOR2I( pcbIUScale.mmToIU( 6 ), pcbIUScale.mmToIU( 6 ) ) );

    PCB_VIA_STITCH b;
    b.SetParent( &board );
    b.SetProperties( a.GetProperties() );

    BOOST_CHECK_EQUAL( b.GetPitch(), a.GetPitch() );
    BOOST_CHECK_EQUAL( (int) b.GetLayout(), (int) a.GetLayout() );
    BOOST_CHECK_EQUAL( (int) b.GetMode(), (int) a.GetMode() );
    BOOST_CHECK_EQUAL( b.GetSeed(), a.GetSeed() );
    BOOST_CHECK_EQUAL( b.GetNetCode(), 1 );

    checkOutlinesEqual( a.Outline(), b.Outline() );

    std::optional<SHAPE_LINE_CHAIN> aCells =
            a.GetProperties().get_opt<SHAPE_LINE_CHAIN>( "excluded_cells" );
    std::optional<SHAPE_LINE_CHAIN> bCells =
            b.GetProperties().get_opt<SHAPE_LINE_CHAIN>( "excluded_cells" );

    BOOST_REQUIRE( aCells.has_value() );
    BOOST_REQUIRE( bCells.has_value() );
    BOOST_REQUIRE_EQUAL( bCells->PointCount(), aCells->PointCount() );

    for( int i = 0; i < aCells->PointCount(); ++i )
        BOOST_CHECK_EQUAL( bCells->CPoint( i ), aCells->CPoint( i ) );
}


BOOST_AUTO_TEST_CASE( SexprSaveLoad )
{
    auto board = std::make_unique<BOARD>();
    board->Add( new NETINFO_ITEM( board.get(), wxT( "GND" ), 1 ) );

    ZONE* zone = new ZONE( board.get() );
    zone->SetLayer( F_Cu );
    zone->SetNetCode( 1 );
    zone->Outline()->NewOutline();
    zone->Outline()->Append( 0, 0 );
    zone->Outline()->Append( pcbIUScale.mmToIU( 1 ), 0 );
    zone->Outline()->Append( pcbIUScale.mmToIU( 1 ), pcbIUScale.mmToIU( 1 ) );
    zone->Outline()->Append( 0, pcbIUScale.mmToIU( 1 ) );
    board->Add( zone );

    PCB_VIA_STITCH* stitch = new PCB_VIA_STITCH();
    board->Add( stitch );
    configureStitch( stitch );
    stitch->ExcludePosition( VECTOR2I( pcbIUScale.mmToIU( 6 ), pcbIUScale.mmToIU( 6 ) ) );

    auto path = std::filesystem::temp_directory_path() / "qa_via_stitch_roundtrip.kicad_pcb";

    ::KI_TEST::DumpBoardToFile( *board, path.string() );

    std::unique_ptr<BOARD> board2 = ::KI_TEST::ReadBoardFromFileOrStream( path.string() );

    BOOST_REQUIRE_EQUAL( board2->Generators().size(), 1 );

    PCB_VIA_STITCH* loaded = dynamic_cast<PCB_VIA_STITCH*>( board2->Generators().front() );
    BOOST_REQUIRE( loaded );

    BOOST_CHECK_EQUAL( loaded->GetPitch(), stitch->GetPitch() );
    BOOST_CHECK_EQUAL( (int) loaded->GetLayout(), (int) stitch->GetLayout() );
    BOOST_CHECK_EQUAL( (int) loaded->GetMode(), (int) stitch->GetMode() );
    BOOST_CHECK_EQUAL( loaded->GetSeed(), stitch->GetSeed() );

    // The template via travels through the (templates ...) section
    BOOST_CHECK_EQUAL( loaded->GetViaSize(), stitch->GetViaSize() );
    BOOST_CHECK_EQUAL( loaded->GetViaDrill(), stitch->GetViaDrill() );

    // Net is stored by name and re-resolved against the loaded board
    NETINFO_ITEM* gnd = board2->FindNet( wxT( "GND" ) );
    BOOST_REQUIRE( gnd );
    BOOST_CHECK_EQUAL( loaded->GetNetCode(), gnd->GetNetCode() );

    checkOutlinesEqual( stitch->Outline(), loaded->Outline() );

    std::optional<SHAPE_LINE_CHAIN> savedCells =
            stitch->GetProperties().get_opt<SHAPE_LINE_CHAIN>( "excluded_cells" );
    std::optional<SHAPE_LINE_CHAIN> loadedCells =
            loaded->GetProperties().get_opt<SHAPE_LINE_CHAIN>( "excluded_cells" );

    BOOST_REQUIRE( savedCells.has_value() );
    BOOST_REQUIRE( loadedCells.has_value() );
    BOOST_REQUIRE_EQUAL( loadedCells->PointCount(), savedCells->PointCount() );

    for( int i = 0; i < savedCells->PointCount(); ++i )
        BOOST_CHECK_EQUAL( loadedCells->CPoint( i ), savedCells->CPoint( i ) );
}


// Build a board with a GND pour on both outer layers, filled, plus a stitch generator
// ready to Update()
struct STITCH_UPDATE_FIXTURE
{
    STITCH_UPDATE_FIXTURE()
    {
        m_board = std::make_unique<BOARD>();
        m_board->Add( new NETINFO_ITEM( m_board.get(), wxT( "GND" ), 1 ) );

        auto drcEngine =
                std::make_shared<DRC_ENGINE>( m_board.get(), &m_board->GetDesignSettings() );
        drcEngine->InitEngine( wxFileName() );
        m_board->GetDesignSettings().m_DRCEngine = drcEngine;

        ZONE* zone = new ZONE( m_board.get() );
        zone->SetLayerSet( LSET( { F_Cu, B_Cu } ) );
        zone->SetNetCode( 1 );
        zone->Outline()->NewOutline();
        zone->Outline()->Append( 0, 0 );
        zone->Outline()->Append( pcbIUScale.mmToIU( 20 ), 0 );
        zone->Outline()->Append( pcbIUScale.mmToIU( 20 ), pcbIUScale.mmToIU( 20 ) );
        zone->Outline()->Append( 0, pcbIUScale.mmToIU( 20 ) );
        m_board->Add( zone );

        KI_TEST::FillZones( m_board.get() );

        m_stitch = new PCB_VIA_STITCH();
        m_board->Add( m_stitch );
        configureStitch( m_stitch );
        m_stitch->SetLayout( PCB_VIA_STITCH_LAYOUT::PLAIN );

        m_toolMgr.SetEnvironment( m_board.get(), nullptr, nullptr, nullptr, nullptr );
        m_dummyTool = new KI_TEST::DUMMY_TOOL();
        m_toolMgr.RegisterTool( m_dummyTool );
    }

    void regenerate()
    {
        BOARD_COMMIT commit( m_dummyTool );

        m_stitch->EditStart( nullptr, m_board.get(), &commit );
        m_stitch->Update( nullptr, m_board.get(), &commit );
        m_stitch->EditFinish( nullptr, m_board.get(), &commit );

        commit.Push( wxT( "regen" ), SKIP_UNDO | SKIP_SET_DIRTY | SKIP_CONNECTIVITY );
    }

    std::set<KIID> childViaIds() const
    {
        std::set<KIID> ids;

        for( BOARD_ITEM* item : m_stitch->GetBoardItems() )
        {
            if( item->Type() == PCB_VIA_T )
                ids.insert( item->m_Uuid );
        }

        return ids;
    }

    std::unique_ptr<BOARD>  m_board;
    PCB_VIA_STITCH*         m_stitch = nullptr;
    TOOL_MANAGER            m_toolMgr;
    KI_TEST::DUMMY_TOOL*    m_dummyTool = nullptr;
};


BOOST_FIXTURE_TEST_CASE( UpdateDeltaStability, STITCH_UPDATE_FIXTURE )
{
    regenerate();

    std::set<KIID> first = childViaIds();

    // The 16mm x 16mm outline at 2mm pitch should have 10 vias
    BOOST_REQUIRE_GT( first.size(), 10 );

    // Vias must respect the pitch
    std::vector<VECTOR2I> positions;

    for( BOARD_ITEM* item : m_stitch->GetBoardItems() )
    {
        if( item->Type() == PCB_VIA_T )
            positions.push_back( item->GetPosition() );
    }

    const int64_t minDistSq =
            (int64_t) pcbIUScale.mmToIU( 2 ) * pcbIUScale.mmToIU( 2 ) - 1;

    for( size_t i = 0; i < positions.size(); ++i )
    {
        for( size_t j = i + 1; j < positions.size(); ++j )
        {
            VECTOR2I d = positions[i] - positions[j];
            int64_t  distSq = (int64_t) d.x * d.x + (int64_t) d.y * d.y;

            BOOST_CHECK_MESSAGE( distSq >= minDistSq,
                                 "vias " << i << " and " << j << " closer than the pitch" );
        }
    }

    // Regenerating an unchanged board, none of the vias should change
    regenerate();

    std::set<KIID> second = childViaIds();

    BOOST_CHECK( first == second );
}


BOOST_FIXTURE_TEST_CASE( UpdateRespectsExclusions, STITCH_UPDATE_FIXTURE )
{
    regenerate();

    std::set<KIID> before = childViaIds();
    BOOST_REQUIRE_GT( before.size(), 10 );

    // Exclude one existing via's position: regeneration must remove exactly that via and
    // leave every other via untouched
    PCB_VIA* victim = nullptr;

    for( BOARD_ITEM* item : m_stitch->GetBoardItems() )
    {
        if( item->Type() == PCB_VIA_T )
        {
            victim = static_cast<PCB_VIA*>( item );
            break;
        }
    }

    BOOST_REQUIRE( victim );

    KIID     victimId = victim->m_Uuid;
    VECTOR2I victimPos = victim->GetPosition();

    m_stitch->ExcludePosition( victimPos );
    regenerate();

    std::set<KIID> after = childViaIds();

    BOOST_CHECK_EQUAL( after.size(), before.size() - 1 );
    BOOST_CHECK_EQUAL( after.count( victimId ), 0 );

    for( const KIID& id : after )
        BOOST_CHECK_EQUAL( before.count( id ), 1 );

    // Clearing the exclusion brings a via back at that position (with a new identity)
    m_stitch->ClearExclusion( victimPos );
    regenerate();

    std::set<KIID> restored = childViaIds();

    BOOST_CHECK_EQUAL( restored.size(), before.size() );

    bool found = false;

    for( BOARD_ITEM* item : m_stitch->GetBoardItems() )
    {
        if( item->Type() == PCB_VIA_T && item->GetPosition() == victimPos )
            found = true;
    }

    BOOST_CHECK( found );
}


BOOST_FIXTURE_TEST_CASE( ClearAllExclusionsRestoresEveryVia, STITCH_UPDATE_FIXTURE )
{
    regenerate();

    std::set<KIID> before = childViaIds();
    BOOST_REQUIRE_GT( before.size(), 10 );

    std::vector<VECTOR2I> victimPositions;

    for( BOARD_ITEM* item : m_stitch->GetBoardItems() )
    {
        if( item->Type() == PCB_VIA_T && victimPositions.size() < 3 )
            victimPositions.push_back( item->GetPosition() );
    }

    BOOST_REQUIRE_EQUAL( victimPositions.size(), 3 );
    BOOST_CHECK( !m_stitch->HasExclusions() );

    for( const VECTOR2I& pos : victimPositions )
        m_stitch->ExcludePosition( pos );

    BOOST_CHECK( m_stitch->HasExclusions() );

    regenerate();
    BOOST_CHECK_EQUAL( childViaIds().size(), before.size() - 3 );

    m_stitch->ClearAllExclusions();
    BOOST_CHECK( !m_stitch->HasExclusions() );

    regenerate();
    BOOST_CHECK_EQUAL( childViaIds().size(), before.size() );

    // Every excluded position must be occupied again
    for( const VECTOR2I& pos : victimPositions )
    {
        bool found = false;

        for( BOARD_ITEM* item : m_stitch->GetBoardItems() )
        {
            if( item->Type() == PCB_VIA_T && item->GetPosition() == pos )
                found = true;
        }

        BOOST_CHECK( found );
    }
}

BOOST_AUTO_TEST_SUITE_END()
