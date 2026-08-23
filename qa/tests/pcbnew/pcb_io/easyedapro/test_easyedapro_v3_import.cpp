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

/**
 * @file test_easyedapro_v3_import.cpp
 * Test suite for import of EasyEDA Pro v3 PCB files
 */

#include <qa_utils/file_utils.h>
#include <qa_utils/wx_utils/unit_test_utils.h>
#include <pcbnew_utils/board_file_utils.h>
#include <pcbnew_utils/board_test_utils.h>

#include <pcbnew/pcb_io/easyedapro/pcb_io_easyedapro_v3_parser.h>
#include <pcbnew/pcb_io/pcb_io.h>
#include <pcbnew/pcb_io/pcb_io_mgr.h>
#include <pcbnew/pcb_io/easyedapro/pcb_io_easyedapro_parser.h>

#include <board.h>
#include <board_design_settings.h>

#include <footprint.h>
#include <pad.h>

#include <wx/dir.h>
#include <wx/filefn.h>
#include <wx/filename.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <vector>


BOOST_AUTO_TEST_SUITE( EasyedaproV3Import )


static wxString getEasyEdaProV3SymbolLibPath()
{
    return wxString::FromUTF8( KI_TEST::GetTestDataRootDir() + "eeschema/plugins/easyedapro/LS2K0300_Symbol.elibz2" );
}


static wxString getEasyEdaProV3FootprintLibPath()
{
    return wxString::FromUTF8( KI_TEST::GetPcbnewTestDataDir()
                               + "plugins/easyedapro/LS2K0300_Footprint_2025-11-14.elibz2" );
}


static std::vector<wxString> listDirEntries( const wxString& aPath )
{
    std::vector<wxString> entries;
    wxDir                 dir( aPath );
    wxString              name;

    if( !dir.IsOpened() )
        return entries;

    for( bool more = dir.GetFirst( &name, wxEmptyString, wxDIR_DIRS | wxDIR_FILES ); more;
         more = dir.GetNext( &name ) )
    {
        entries.push_back( name );
    }

    std::sort( entries.begin(), entries.end() );

    return entries;
}


BOOST_AUTO_TEST_CASE( FootprintLibraryCanReadOnlyFootprintElibz2 )
{
    IO_RELEASER<PCB_IO> plugin( PCB_IO_MGR::FindPlugin( PCB_IO_MGR::EASYEDAPRO_V3 ) );
    BOOST_REQUIRE( plugin );

    BOOST_CHECK( plugin->CanReadLibrary( getEasyEdaProV3FootprintLibPath() ) );
    BOOST_CHECK( !plugin->CanReadLibrary( getEasyEdaProV3SymbolLibPath() ) );
}


BOOST_AUTO_TEST_CASE( FootprintLibraryEnumeratesAndLoadsElibz2 )
{
    IO_RELEASER<PCB_IO> plugin( PCB_IO_MGR::FindPlugin( PCB_IO_MGR::EASYEDAPRO_V3 ) );
    BOOST_REQUIRE( plugin );

    wxArrayString footprintNames;
    BOOST_REQUIRE_NO_THROW( plugin->FootprintEnumerate( footprintNames, getEasyEdaProV3FootprintLibPath(), false ) );

    BOOST_REQUIRE_EQUAL( footprintNames.GetCount(), 1 );
    BOOST_CHECK_EQUAL( footprintNames[0], wxString( wxS( "BGA-286_17x17_12.0x12.0mm" ) ) );

    std::unique_ptr<FOOTPRINT> footprint =
            plugin->FootprintLoad( getEasyEdaProV3FootprintLibPath(), wxS( "BGA-286_17x17_12.0x12.0mm" ) );

    BOOST_REQUIRE( footprint );
    BOOST_CHECK_EQUAL( footprint->GetFPID().GetLibItemName(), UTF8( "BGA-286_17x17_12.0x12.0mm" ) );
    BOOST_CHECK_EQUAL( footprint->Pads().size(), 286 );
}


BOOST_AUTO_TEST_CASE( FootprintCanvasOriginNormalizesPadPosition )
{
    EASYEDAPRO::V3_DOC_RAW doc;
    doc.rows = { { wxS( "CANVAS" ), wxEmptyString, 0, {}, { { "originX", -145 }, { "originY", -20 } } },
                 { wxS( "PAD" ), wxEmptyString, 0, {}, { { "num", 1 }, { "centerX", -145 }, { "centerY", -20 } } } };

    BOARD                       board;
    PCB_IO_EASYEDAPRO_V3_PARSER parser( &board, nullptr );
    std::unique_ptr<FOOTPRINT>  footprint( parser.ParseFootprint( {}, doc ) );

    BOOST_REQUIRE( footprint );

    PAD* pad = footprint->FindPadByNumber( wxS( "1" ) );
    BOOST_REQUIRE( pad );

    const VECTOR2I localPosition = pad->GetFPRelativePosition();
    BOOST_CHECK_EQUAL( localPosition.x, 0 );
    BOOST_CHECK_EQUAL( localPosition.y, 0 );
}


BOOST_AUTO_TEST_CASE( DuplicatePadNumbersReceiveNets )
{
    // A hot-swap socket style footprint where the SMD contact and the through-hole
    // pin share the same pad number and must end up on the same net.
    EASYEDAPRO::V3_DOC_RAW fpDoc;
    fpDoc.rows = { { wxS( "PAD" ),
                     wxEmptyString,
                     0,
                     {},
                     { { "num", "1" }, { "layerId", 2 }, { "centerX", -100 }, { "centerY", 0 } } },
                   { wxS( "PAD" ),
                     wxEmptyString,
                     0,
                     {},
                     { { "num", "1" },
                       { "layerId", 12 },
                       { "centerX", 100 },
                       { "centerY", 0 },
                       { "hole", { { "holeType", "ROUND" }, { "width", 1 }, { "height", 1 } } } } },
                   { wxS( "PAD" ),
                     wxEmptyString,
                     0,
                     {},
                     { { "num", "2" }, { "layerId", 2 }, { "centerX", -100 }, { "centerY", 200 } } },
                   { wxS( "PAD" ),
                     wxEmptyString,
                     0,
                     {},
                     { { "num", "2" },
                       { "layerId", 12 },
                       { "centerX", 100 },
                       { "centerY", 200 },
                       { "hole", { { "holeType", "ROUND" }, { "width", 1 }, { "height", 1 } } } } } };

    BOARD                       board;
    PCB_IO_EASYEDAPRO_V3_PARSER parser( &board, nullptr );
    std::unique_ptr<FOOTPRINT>  footprint( parser.ParseFootprint( {}, fpDoc ) );

    BOOST_REQUIRE( footprint );
    BOOST_REQUIRE_EQUAL( footprint->Pads().size(), 4 );

    std::map<wxString, std::unique_ptr<FOOTPRINT>> footprintMap;
    footprintMap[wxS( "fp1" )] = std::move( footprint );

    EASYEDAPRO::V3_DOC_RAW boardDoc;
    boardDoc.rows = {
        { wxS( "NET" ), wxS( "[\"NET\",\"GND\"]" ), 0, {}, {} },
        { wxS( "NET" ), wxS( "[\"NET\",\"PA8\"]" ), 0, {}, {} },
        { wxS( "COMPONENT" ), wxS( "c1" ), 0, {}, { { "layerId", 1 }, { "x", 0 }, { "y", 0 } } },
        { wxS( "ATTR" ), wxEmptyString, 0, {}, { { "parentId", "c1" }, { "key", "Footprint" }, { "value", "fp1" } } },
        { wxS( "ATTR" ), wxEmptyString, 0, {}, { { "parentId", "c1" }, { "key", "Designator" }, { "value", "KEY1" } } },
        { wxS( "PAD_NET" ), wxS( "[\"PAD_NET\",\"c1\",\"1\",\"e1\"]" ), 0, {}, { { "padNet", "GND" } } },
        { wxS( "PAD_NET" ), wxS( "[\"PAD_NET\",\"c1\",\"1\",\"e47\"]" ), 0, {}, { { "padNet", "GND" } } },
        { wxS( "PAD_NET" ), wxS( "[\"PAD_NET\",\"c1\",\"2\",\"e2\"]" ), 0, {}, { { "padNet", "PA8" } } },
        { wxS( "PAD_NET" ), wxS( "[\"PAD_NET\",\"c1\",\"2\",\"e48\"]" ), 0, {}, { { "padNet", "PA8" } } }
    };

    parser.ParseBoard( &board, nlohmann::json::object(), footprintMap, {}, {}, boardDoc, wxS( "lib" ) );

    BOOST_REQUIRE_EQUAL( board.Footprints().size(), 1 );

    FOOTPRINT* placed = board.Footprints().front();

    int pad1WithNet = 0;
    int pad2WithNet = 0;

    for( PAD* pad : placed->Pads() )
    {
        if( pad->GetNumber() == wxS( "1" ) && pad->GetNetname() == wxS( "GND" ) )
            pad1WithNet++;

        if( pad->GetNumber() == wxS( "2" ) && pad->GetNetname() == wxS( "PA8" ) )
            pad2WithNet++;
    }

    BOOST_CHECK_EQUAL( pad1WithNet, 2 );
    BOOST_CHECK_EQUAL( pad2WithNet, 2 );
}


BOOST_AUTO_TEST_CASE( SafeClearanceUsesTrackToTrackEntry )
{
    BOARD                       board;
    PCB_IO_EASYEDAPRO_V3_PARSER parser( &board, nullptr );

    nlohmann::json content = { { 4.0157 }, { 5.9843, 5.9843 } };

    while( content.size() < 13 )
        content.push_back( nlohmann::json::array() );

    content[12] = { 6.9 };
    content[11] = { 11.811, 10.0 };


    EASYEDAPRO::V3_DOC_RAW boardDoc;
    boardDoc.rows = { { wxS( "RULE" ),
                        wxS( "[\"RULE\",\"SAFE\",\"safeClearance\"]" ),
                        0,
                        {},
                        { { "ruleState", "DEFAULT" },
                          { "ruleContext", { { "safeSpacing", { { { "content", content } } } } } } } } };

    std::map<wxString, std::unique_ptr<FOOTPRINT>> footprints;
    parser.ParseBoard( &board, nlohmann::json::object(), footprints, {}, {}, boardDoc, wxEmptyString );

    BOOST_CHECK_EQUAL( board.GetDesignSettings().m_MinClearance, PCB_IO_EASYEDAPRO_PARSER::ScaleSize( 4.0157 ) );
    BOOST_CHECK_EQUAL( board.GetDesignSettings().m_CopperEdgeClearance, PCB_IO_EASYEDAPRO_PARSER::ScaleSize( 10.0 ) );

    BOOST_CHECK( board.m_LegacyDesignSettingsLoaded );

    wxString rules = parser.GenerateSafeSpacingRules();

    BOOST_CHECK( rules.Contains( wxS( "(A.Type == 'Pad' && A.Pad_Type == 'SMD') && (B.Type == 'Track')" ) ) );

    BOOST_CHECK( rules.Contains( wxS( "SMD Pad ↔ Track: 5.9843 mil (0.152 mm)" ) ) );
    BOOST_CHECK( rules.Contains( wxS( "Hole ↔ Track: 6.9 mil (0.1755 mm)" ) ) );
    BOOST_CHECK( rules.Contains( wxS( "EasyEDA safe spacing: SMD Pad ↔ Track" ) ) );
}


BOOST_AUTO_TEST_CASE( BoardLoadImportsInnerLayers )
{
    // Import from a private copy so a stray write lands here rather than in the shared test data
    KI_TEST::SCOPED_TEMP_DIR tempDir( "easyedapro_v3_board_load" );

    const wxString archiveName = wxS( "ProProject_LS2K0300Core_2025-11-14.epro2" );
    wxString       sourceDir = wxString::FromUTF8( KI_TEST::GetPcbnewTestDataDir() + "plugins/easyedapro/" );
    const wxString tempDirPath = tempDir.PathStr();

    wxFileName dataFile( tempDirPath, archiveName );
    BOOST_REQUIRE( wxCopyFile( sourceDir + archiveName, dataFile.GetFullPath() ) );

    std::map<std::string, UTF8> properties;
    properties["pcb_id"] = "eb9fbfba682940f7a002816e66fbb3d7";

    IO_RELEASER<PCB_IO> plugin( PCB_IO_MGR::FindPlugin( PCB_IO_MGR::EASYEDAPRO_V3 ) );
    BOOST_REQUIRE( plugin );

    std::unique_ptr<BOARD> board = plugin->LoadBoard( dataFile.GetFullPath(), &properties );
    BOOST_REQUIRE( board );

    // Loading a board is a read, so nothing may appear beside the archive
    std::vector<wxString> entries = listDirEntries( tempDirPath );
    BOOST_REQUIRE_EQUAL( entries.size(), 1 );
    BOOST_CHECK_EQUAL( entries[0], archiveName );

    std::vector<FOOTPRINT*> definitions = plugin->GetImportedCachedLibraryFootprints();
    BOOST_CHECK( definitions.size() > 0 );

    for( FOOTPRINT* definition : definitions )
    {
        BOOST_CHECK( !definition->GetFPID().GetLibItemName().empty() );
        delete definition;
    }

    BOOST_CHECK( board->Footprints().size() > 0 );
    BOOST_CHECK( board->GetNetCount() > 0 );

    // A 4-layer board whose inner and back copper carry their EasyEDA names.
    BOOST_CHECK_EQUAL( board->GetCopperLayerCount(), 4 );
    BOOST_CHECK_EQUAL( board->GetLayerName( In1_Cu ), wxString( wxS( "Inner1" ) ) );
    BOOST_CHECK_EQUAL( board->GetLayerName( In2_Cu ), wxString( wxS( "Inner2" ) ) );
    BOOST_CHECK_EQUAL( board->GetLayerName( B_Cu ), wxString( wxS( "Bottom Layer" ) ) );
}


BOOST_AUTO_TEST_CASE( ParseContourClampsRoundedRectangleRadius )
{
    // Given the BATIN FILL rounded rectangle with an oversized radius.
    BOARD                    board;
    PCB_IO_EASYEDAPRO_PARSER parser( &board, nullptr );
    nlohmann::json contourData = nlohmann::json::parse( R"(["R", -736.1963, -1056.456, 34.1853, 84.0904, -90, 1111])" );

    // When parsing the contour, the radius must be clamped to the short side.
    SHAPE_LINE_CHAIN contour = parser.ParseContour( contourData, true );
    contour.SetClosed( true );

    // Then the result must have the area of a rounded rectangle/capsule, not a thin cross.
    const double boundingBoxArea = static_cast<double>( contour.BBox().GetArea() );
    BOOST_REQUIRE_GT( boundingBoxArea, 0.0 );
    BOOST_CHECK_GT( std::abs( contour.Area() ) / boundingBoxArea, 0.75 );
}


BOOST_AUTO_TEST_SUITE_END()
