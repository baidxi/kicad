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

#include <fstream>
#include <memory>
#include <filesystem>

#include <board.h>
#include <board_design_settings.h>
#include <footprint.h>
#include <pad.h>
#include <padstack.h>
#include <pcb_barcode.h>
#include <pcb_shape.h>
#include <pcb_track.h>
#include <zone.h>
#include <zone_settings.h>
#include <downgrade/board_downgrade.h>
#include <downgrade_target.h>
#include <pcb_io/kicad_sexpr/pcb_io_kicad_sexpr.h>
#include <settings/settings_manager.h>
#include <pcbnew_utils/board_test_utils.h>


BOOST_AUTO_TEST_SUITE( BoardDowngrade )


// A board with no post-10.0 features exports to 10.0 with nothing lost.
BOOST_AUTO_TEST_CASE( EmptyBoardIsClean )
{
    BOARD                   board;
    const DOWNGRADE_TARGET& kicad10 = GetDowngradeTargets().front();

    COMPATIBILITY_REPORT report = ClassifyBoardForDowngrade( &board, kicad10 );

    BOOST_CHECK( !report.IsLossy() );
    BOOST_CHECK( !report.IsBlocked() );
    BOOST_CHECK( report.Entries().empty() );
}


// The ellipse primitive only exists since 20260508, so 10.0 gets an approximation.
BOOST_AUTO_TEST_CASE( EllipseIsLoweredForKicad10 )
{
    BOARD board;
    board.Add( new PCB_SHAPE( &board, SHAPE_T::ELLIPSE ) );

    const DOWNGRADE_TARGET& kicad10 = GetDowngradeTargets().front();
    COMPATIBILITY_REPORT    report = ClassifyBoardForDowngrade( &board, kicad10 );

    BOOST_CHECK( report.IsLossy() );
    BOOST_CHECK_EQUAL( report.Count( DOWNGRADE_BUCKET::LOWER ), 1 );
}


// A target that already has the ellipse primitive keeps it. This proves the version gate.
BOOST_AUTO_TEST_CASE( EllipseKeptForNewerTarget )
{
    BOARD board;
    board.Add( new PCB_SHAPE( &board, SHAPE_T::ELLIPSE ) );

    DOWNGRADE_TARGET     future = { wxT( "future" ), 20260623, 20260629, 20260629, 3, 5 };
    COMPATIBILITY_REPORT report = ClassifyBoardForDowngrade( &board, future );

    BOOST_CHECK( !report.IsLossy() );
}


// The Lower transform actually rewrites the ellipse as a polygon.
BOOST_AUTO_TEST_CASE( EllipseTransformedToPolygon )
{
    BOARD      board;
    PCB_SHAPE* ellipse = new PCB_SHAPE( &board, SHAPE_T::ELLIPSE );
    ellipse->SetStart( VECTOR2I( 0, 0 ) );
    ellipse->SetEnd( VECTOR2I( 5000000, 0 ) ); // 5 mm radius in IU
    board.Add( ellipse );

    DowngradeBoardInPlace( &board, GetDowngradeTargets().front() );

    BOOST_CHECK( ellipse->GetShape() == SHAPE_T::POLY );
}


// The old (at) header cannot represent a scaled footprint, so the export must block.
BOOST_AUTO_TEST_CASE( ScaledFootprintBlocksExport )
{
    BOARD      board;
    FOOTPRINT* fp = new FOOTPRINT( &board );
    fp->SetTransformScale( 2.0, 1.0 );
    board.Add( fp );

    const DOWNGRADE_TARGET& kicad10 = GetDowngradeTargets().front();
    COMPATIBILITY_REPORT    report = ClassifyBoardForDowngrade( &board, kicad10 );

    BOOST_CHECK( report.IsBlocked() );
    BOOST_CHECK_EQUAL( report.Count( DOWNGRADE_BUCKET::BLOCK ), 1 );
}


// Extruded 3D bodies (20260410) have no equivalent in 10.0, so they are dropped.
BOOST_AUTO_TEST_CASE( ExtrudedBodyDroppedForKicad10 )
{
    BOARD      board;
    FOOTPRINT* fp = new FOOTPRINT( &board );
    fp->EnsureExtrudedBody();
    board.Add( fp );

    const DOWNGRADE_TARGET& kicad10 = GetDowngradeTargets().front();

    BOOST_CHECK_EQUAL( ClassifyBoardForDowngrade( &board, kicad10 ).Count( DOWNGRADE_BUCKET::DROP ), 1 );

    DowngradeBoardInPlace( &board, kicad10 );

    BOOST_CHECK( !fp->HasExtrudedBody() );
}


// Copper-thieving zones (20260513) become plain filled zones for 10.0.
BOOST_AUTO_TEST_CASE( ThievingZoneLoweredForKicad10 )
{
    BOARD board;
    ZONE* zone = new ZONE( &board );
    zone->SetFillMode( ZONE_FILL_MODE::COPPER_THIEVING );
    board.Add( zone );

    const DOWNGRADE_TARGET& kicad10 = GetDowngradeTargets().front();

    BOOST_CHECK_EQUAL( ClassifyBoardForDowngrade( &board, kicad10 ).Count( DOWNGRADE_BUCKET::LOWER ), 1 );

    DowngradeBoardInPlace( &board, kicad10 );

    BOOST_CHECK( !zone->IsCopperThieving() );
}


// Pad simulation electrical types (20260521) are dropped for 10.0.
BOOST_AUTO_TEST_CASE( PadSimTypeDroppedForKicad10 )
{
    BOARD      board;
    FOOTPRINT* fp = new FOOTPRINT( &board );
    PAD*       pad = new PAD( fp );
    pad->SetSimElectricalType( PAD_SIM_ELECTRICAL_TYPE::SOURCE );
    fp->Add( pad );
    board.Add( fp );

    const DOWNGRADE_TARGET& kicad10 = GetDowngradeTargets().front();

    BOOST_CHECK_EQUAL( ClassifyBoardForDowngrade( &board, kicad10 ).Count( DOWNGRADE_BUCKET::DROP ), 1 );

    DowngradeBoardInPlace( &board, kicad10 );

    BOOST_CHECK( pad->GetSimElectricalType() == PAD_SIM_ELECTRICAL_TYPE::NONE );
}


// Counterbore / countersink is a post-9.0 feature 9.0 cannot parse, so it is dropped for 9.0.
BOOST_AUTO_TEST_CASE( PostMachiningDroppedForKicad9 )
{
    BOARD      board;
    FOOTPRINT* fp = new FOOTPRINT( &board );
    PAD*       pad = new PAD( fp );
    pad->Padstack().FrontPostMachining().mode = PAD_DRILL_POST_MACHINING_MODE::COUNTERBORE;
    fp->Add( pad );
    board.Add( fp );

    const DOWNGRADE_TARGET& kicad9 = GetDowngradeTargets().at( 1 );

    BOOST_CHECK_EQUAL( ClassifyBoardForDowngrade( &board, kicad9 ).Count( DOWNGRADE_BUCKET::DROP ), 1 );

    DowngradeBoardInPlace( &board, kicad9 );

    BOOST_CHECK( !pad->Padstack().FrontPostMachining().mode.has_value() );
}


// Press-fit is a post-9.0 pad property 9.0 cannot parse, so it is dropped for 9.0.
BOOST_AUTO_TEST_CASE( PressFitDroppedForKicad9 )
{
    BOARD      board;
    FOOTPRINT* fp = new FOOTPRINT( &board );
    PAD*       pad = new PAD( fp );
    pad->SetProperty( PAD_PROP::PRESSFIT );
    fp->Add( pad );
    board.Add( fp );

    const DOWNGRADE_TARGET& kicad9 = GetDowngradeTargets().at( 1 );

    BOOST_CHECK_EQUAL( ClassifyBoardForDowngrade( &board, kicad9 ).Count( DOWNGRADE_BUCKET::DROP ), 1 );

    DowngradeBoardInPlace( &board, kicad9 );

    BOOST_CHECK( pad->GetProperty() == PAD_PROP::NONE );
}


// Skip vias are post-9.0, so they are approximated by the nearest mode 9.0 understands.
BOOST_AUTO_TEST_CASE( SkipViaLoweredForKicad9 )
{
    BOARD    board;
    PCB_VIA* via = new PCB_VIA( &board );
    via->Padstack().SetUnconnectedLayerMode( UNCONNECTED_LAYER_MODE::START_END_ONLY );
    board.Add( via );

    const DOWNGRADE_TARGET& kicad9 = GetDowngradeTargets().at( 1 );

    BOOST_CHECK_EQUAL( ClassifyBoardForDowngrade( &board, kicad9 ).Count( DOWNGRADE_BUCKET::LOWER ), 1 );

    DowngradeBoardInPlace( &board, kicad9 );

    BOOST_CHECK( via->Padstack().UnconnectedLayerMode() == UNCONNECTED_LAYER_MODE::REMOVE_EXCEPT_START_AND_END );
}


// The v10 writer must not move anything. Every footprint and pad position in the export must match
// the source exactly. This guards the extracted writer against silent model drift.
BOOST_AUTO_TEST_CASE( ExportToKicad10PreservesGeometry )
{
    SETTINGS_MANAGER settingsManager;

    const std::string src = KI_TEST::GetPcbnewTestDataDir() + "via_off_center.kicad_pcb";

    KI_TEST::TEMPORARY_DIRECTORY tmp( "kicad_qa_downgrade_geom", "" );
    const std::string            dest = ( tmp.GetPath() / "out.kicad_pcb" ).string();

    COMPATIBILITY_REPORT report;
    BOOST_REQUIRE( ExportBoardToOlderVersion( src, dest, GetDowngradeTargets().front(), report ) );

    PCB_IO_KICAD_SEXPR     io;
    std::unique_ptr<BOARD> source( io.LoadBoard( src, nullptr ) );
    std::unique_ptr<BOARD> exported( io.LoadBoard( dest, nullptr ) );

    BOOST_REQUIRE( source && exported );
    BOOST_REQUIRE_EQUAL( source->Footprints().size(), exported->Footprints().size() );

    for( size_t i = 0; i < source->Footprints().size(); i++ )
    {
        FOOTPRINT* a = source->Footprints()[i];
        FOOTPRINT* b = exported->Footprints()[i];

        BOOST_CHECK_EQUAL( a->GetReference(), b->GetReference() );
        BOOST_CHECK( a->GetPosition() == b->GetPosition() );
        BOOST_CHECK( a->GetOrientation() == b->GetOrientation() );

        BOOST_REQUIRE_EQUAL( a->Pads().size(), b->Pads().size() );

        for( size_t p = 0; p < a->Pads().size(); p++ )
            BOOST_CHECK( a->Pads()[p]->GetPosition() == b->Pads()[p]->GetPosition() );
    }
}


// Same geometry guard for the v9 writer.
BOOST_AUTO_TEST_CASE( ExportToKicad9PreservesGeometry )
{
    SETTINGS_MANAGER settingsManager;

    const std::string src = KI_TEST::GetPcbnewTestDataDir() + "via_off_center.kicad_pcb";

    KI_TEST::TEMPORARY_DIRECTORY tmp( "kicad_qa_downgrade_geom9", "" );
    const std::string            dest = ( tmp.GetPath() / "out.kicad_pcb" ).string();

    COMPATIBILITY_REPORT report;
    BOOST_REQUIRE( ExportBoardToOlderVersion( src, dest, GetDowngradeTargets().at( 1 ), report ) );

    PCB_IO_KICAD_SEXPR     io;
    std::unique_ptr<BOARD> source( io.LoadBoard( src, nullptr ) );
    std::unique_ptr<BOARD> exported( io.LoadBoard( dest, nullptr ) );

    BOOST_REQUIRE( source && exported );
    BOOST_REQUIRE_EQUAL( source->Footprints().size(), exported->Footprints().size() );

    for( size_t i = 0; i < source->Footprints().size(); i++ )
    {
        FOOTPRINT* a = source->Footprints()[i];
        FOOTPRINT* b = exported->Footprints()[i];

        BOOST_CHECK( a->GetPosition() == b->GetPosition() );

        BOOST_REQUIRE_EQUAL( a->Pads().size(), b->Pads().size() );

        for( size_t pd = 0; pd < a->Pads().size(); pd++ )
            BOOST_CHECK( a->Pads()[pd]->GetPosition() == b->Pads()[pd]->GetPosition() );
    }
}


// Buried vias postdate 9.0's blind/buried split, so they are written with the old combined type.
BOOST_AUTO_TEST_CASE( BuriedViaLoweredForKicad9 )
{
    BOARD    board;
    PCB_VIA* via = new PCB_VIA( &board );
    via->SetViaType( VIATYPE::BURIED );
    board.Add( via );

    const DOWNGRADE_TARGET& kicad9 = GetDowngradeTargets().at( 1 );

    BOOST_CHECK_EQUAL( ClassifyBoardForDowngrade( &board, kicad9 ).Count( DOWNGRADE_BUCKET::LOWER ), 1 );

    DowngradeBoardInPlace( &board, kicad9 );

    BOOST_CHECK( via->GetViaType() == VIATYPE::BLIND );
}


// Per-layer zone defaults are a post-9.0 setup block, so they are dropped for 9.0.
BOOST_AUTO_TEST_CASE( ZoneDefaultsDroppedForKicad9 )
{
    BOARD board;
    board.GetDesignSettings().m_ZoneLayerProperties[F_Cu] = {};

    const DOWNGRADE_TARGET& kicad9 = GetDowngradeTargets().at( 1 );

    BOOST_CHECK_EQUAL( ClassifyBoardForDowngrade( &board, kicad9 ).Count( DOWNGRADE_BUCKET::DROP ), 1 );

    DowngradeBoardInPlace( &board, kicad9 );

    BOOST_CHECK( board.GetDesignSettings().m_ZoneLayerProperties.empty() );
}


// A barcode inside a footprint must be classified and dropped like one on the board.
BOOST_AUTO_TEST_CASE( FootprintBarcodeDroppedForKicad9 )
{
    BOARD      board;
    FOOTPRINT* fp = new FOOTPRINT( &board );
    fp->Add( new PCB_BARCODE( fp ) );
    board.Add( fp );

    const DOWNGRADE_TARGET& kicad9 = GetDowngradeTargets().at( 1 );

    BOOST_CHECK_EQUAL( ClassifyBoardForDowngrade( &board, kicad9 ).Count( DOWNGRADE_BUCKET::DROP ), 1 );

    DowngradeBoardInPlace( &board, kicad9 );

    BOOST_CHECK_EQUAL( ClassifyBoardForDowngrade( &board, kicad9 ).Count( DOWNGRADE_BUCKET::DROP ), 0 );
}


// Exercise the real .kicad_mod write path used to downgrade a footprint library, and confirm the
// result carries the target stamp and reloads cleanly.
BOOST_AUTO_TEST_CASE( FootprintFileDowngradeRoundTrips )
{
    const DOWNGRADE_TARGET& kicad9 = GetDowngradeTargets().at( 1 );

    const std::string src = KI_TEST::GetPcbnewTestDataDir() + "issue24045/QFN-24_L4.0-W4.0-P0.50-BL-EP2.6.kicad_mod";

    PCB_IO_KICAD_SEXPR         loader;
    wxString                   nameOut;
    std::unique_ptr<FOOTPRINT> fp( loader.ImportFootprint( src, nameOut ) );
    BOOST_REQUIRE( fp != nullptr );

    DowngradeFootprintInPlace( fp.get(), kicad9 );

    KI_TEST::TEMPORARY_DIRECTORY tmp( "kicad_qa_fp_downgrade", "" );
    const wxString               dest = ( tmp.GetPath() / "out.kicad_mod" ).wstring();

    SaveFootprintForTarget( fp.get(), dest, kicad9 );

    // The stamp proves the dispatch picked the right writer, not just a parseable one.
    std::ifstream out( dest.ToStdString() );
    std::string   content( ( std::istreambuf_iterator<char>( out ) ), std::istreambuf_iterator<char>() );
    BOOST_CHECK( content.find( "(version " + std::to_string( kicad9.m_boardVersion ) + ")" ) != std::string::npos );

    PCB_IO_KICAD_SEXPR         reader;
    wxString                   reloadName;
    std::unique_ptr<FOOTPRINT> reloaded( reader.ImportFootprint( dest, reloadName ) );
    BOOST_CHECK( reloaded != nullptr );
}


// The v10 footprint file path gets the same round-trip check as v9.
BOOST_AUTO_TEST_CASE( FootprintFileV10RoundTrips )
{
    SETTINGS_MANAGER settingsManager;

    const DOWNGRADE_TARGET& kicad10 = GetDowngradeTargets().front();

    const std::string src = KI_TEST::GetPcbnewTestDataDir() + "issue24045/QFN-24_L4.0-W4.0-P0.50-BL-EP2.6.kicad_mod";

    PCB_IO_KICAD_SEXPR         loader;
    wxString                   nameOut;
    std::unique_ptr<FOOTPRINT> fp( loader.ImportFootprint( src, nameOut ) );
    BOOST_REQUIRE( fp != nullptr );

    DowngradeFootprintInPlace( fp.get(), kicad10 );

    KI_TEST::TEMPORARY_DIRECTORY tmp( "kicad_qa_fp_downgrade10", "" );
    const wxString               dest = ( tmp.GetPath() / "out.kicad_mod" ).wstring();

    SaveFootprintForTarget( fp.get(), dest, kicad10 );

    // The stamp proves the dispatch picked the right writer, not just a parseable one.
    std::ifstream out( dest.ToStdString() );
    std::string   content( ( std::istreambuf_iterator<char>( out ) ), std::istreambuf_iterator<char>() );
    BOOST_CHECK( content.find( "(version " + std::to_string( kicad10.m_boardVersion ) + ")" ) != std::string::npos );

    PCB_IO_KICAD_SEXPR         reader;
    wxString                   reloadName;
    std::unique_ptr<FOOTPRINT> reloaded( reader.ImportFootprint( dest, reloadName ) );
    BOOST_CHECK( reloaded != nullptr );
}


// A library footprint carries the same features but is downgraded through its own path.
BOOST_AUTO_TEST_CASE( FootprintPressFitDroppedForKicad9 )
{
    BOARD      board;
    FOOTPRINT* fp = new FOOTPRINT( &board );
    PAD*       pad = new PAD( fp );
    pad->SetProperty( PAD_PROP::PRESSFIT );
    fp->Add( pad );

    DowngradeFootprintInPlace( fp, GetDowngradeTargets().at( 1 ) );

    BOOST_CHECK( pad->GetProperty() == PAD_PROP::NONE );

    delete fp;
}


// Saving with a target version writes the older stamp, and reloading reads it back.
BOOST_AUTO_TEST_CASE( SaveWithTargetVersionStampsOlder )
{
    SETTINGS_MANAGER settingsManager;

    BOARD                   board;
    const DOWNGRADE_TARGET& kicad10 = GetDowngradeTargets().front();

    KI_TEST::TEMPORARY_DIRECTORY tmp( "kicad_qa_downgrade", "" );
    std::string                  dest = ( tmp.GetPath() / "out.kicad_pcb" ).string();

    SaveBoardForTarget( &board, dest, kicad10 );

    PCB_IO_KICAD_SEXPR     reader;
    std::unique_ptr<BOARD> reloaded( reader.LoadBoard( dest, nullptr ) );

    BOOST_REQUIRE( reloaded != nullptr );
    BOOST_CHECK_EQUAL( reloaded->GetFileFormatVersionAtLoad(), kicad10.m_boardVersion );
}


// The fail-closed gate: a post-10.0 token surviving in the output is caught, and lookalikes are not.
BOOST_AUTO_TEST_CASE( VerifyGateCatchesUnsupportedTokens )
{
    const DOWNGRADE_TARGET& kicad10 = GetDowngradeTargets().front();

    // An unstripped feature (e.g. an affine transform) must be caught.
    BOOST_CHECK( FindUnsupportedBoardToken( wxT( "(footprint (transform (translate 1 2)))" ), kicad10 )
                 == wxT( "transform" ) );
    BOOST_CHECK( FindUnsupportedBoardToken( wxT( "(net_chain \"n\")" ), kicad10 ) == wxT( "net_chain" ) );

    // A clean board, and lookalike prefixes, must pass.
    BOOST_CHECK( FindUnsupportedBoardToken( wxT( "(kicad_pcb (net 0 \"n\") (gr_line ...))" ), kicad10 ).IsEmpty() );

    // Value tokens the node scan cannot see must still be caught for 9.0, but not inside strings.
    const DOWNGRADE_TARGET& kicad9 = GetDowngradeTargets().at( 1 );

    BOOST_CHECK( FindUnsupportedBoardToken( wxT( "(via buried (at 1 1))" ), kicad9 ) == wxT( "buried" ) );
    BOOST_CHECK( FindUnsupportedBoardToken( wxT( "(gr_text \"buried treasure\")" ), kicad9 ).IsEmpty() );
}


// When this fails a format bump happened. Review the new keywords, update the denylist, then bump
// BoardDowngradeCoveredVersion.
BOOST_AUTO_TEST_CASE( DenylistCoversCurrentFormat )
{
    BOOST_CHECK_EQUAL( SEXPR_BOARD_FILE_VERSION, BoardDowngradeCoveredVersion() );
}


BOOST_AUTO_TEST_SUITE_END()
