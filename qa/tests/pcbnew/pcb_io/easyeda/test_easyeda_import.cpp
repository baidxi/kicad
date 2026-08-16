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

/**
 * @file test_easyeda_import.cpp
 * Test suite for import of EasyEDA standard-format boards.
 */

#include <pcbnew_utils/board_test_utils.h>
#include <qa_utils/wx_utils/unit_test_utils.h>

#include <pcbnew/pcb_io/easyeda/pcb_io_easyeda_parser.h>

#include <board.h>
#include <footprint.h>
#include <zone.h>


struct EASYEDA_IMPORT_FIXTURE
{
    EASYEDA_IMPORT_FIXTURE() {}
};


BOOST_FIXTURE_TEST_SUITE( EasyedaImport, EASYEDA_IMPORT_FIXTURE )


/**
 * EasyEDA's COPPERAREA fill data describes each filled polygon as a path string made of
 * multiple closed subpaths: the first is the outer contour, the rest are holes (clearance
 * around pads/vias/tracks of other nets). The importer must keep those holes, otherwise
 * the pre-computed fill covers non-GND copper and different nets are shorted together.
 */
BOOST_AUTO_TEST_CASE( CopperAreaFillHolesPreserved )
{
    BOARD                  board;
    PCB_IO_EASYEDA_PARSER  parser( nullptr );

    // Outer square 0..1000 with an inner square hole 400..600.
    wxString outline = wxS( "M 0 0 L 1000 0 L 1000 1000 L 0 1000 Z" );
    wxString fillData =
            wxS( R"([["M 0 0 L 1000 0 L 1000 1000 L 0 1000 Z M 400 400 L 400 600 L 600 600 L 600 400 Z"]])" );

    wxArrayString parts;
    parts.Add( wxS( "COPPERAREA" ) );
    parts.Add( wxS( "1" ) );       // stroke width
    parts.Add( wxS( "1" ) );       // layer -> F.Cu
    parts.Add( wxS( "GND" ) );     // net
    parts.Add( outline );          // pour outline
    parts.Add( wxS( "1" ) );       // clearance
    parts.Add( wxS( "solid" ) );   // fill style
    parts.Add( wxS( "gge1" ) );    // gge id
    parts.Add( wxS( "spoke" ) );   // thermal relief
    parts.Add( wxS( "none" ) );    // keep island
    parts.Add( fillData );         // fill data
    parts.Add( wxS( "0" ) );
    parts.Add( wxEmptyString );    // other copper area gge id
    parts.Add( wxS( "1" ) );       // fill order
    parts.Add( wxS( "1" ) );
    parts.Add( wxS( "1" ) );
    parts.Add( wxS( "1" ) );
    parts.Add( wxS( "none" ) );    // improve fabrication
    parts.Add( wxS( "0" ) );       // thermal spoke width

    wxString shape;
    for( size_t i = 0; i < parts.size(); ++i )
    {
        if( i )
            shape += wxS( "~" );

        shape += parts[i];
    }

    wxArrayString shapes;
    shapes.Add( shape );

    std::map<wxString, std::unique_ptr<FOOTPRINT>> footprintMap;
    std::map<wxString, wxString>                   paramMap;

    parser.ParseToBoardItemContainer( &board, &board, paramMap, footprintMap, shapes );

    BOOST_REQUIRE_EQUAL( board.Zones().size(), 1 );

    ZONE* zone = board.Zones().front();

    BOOST_REQUIRE( zone );
    BOOST_CHECK_EQUAL( zone->GetNetname(), wxS( "GND" ) );

    std::shared_ptr<SHAPE_POLY_SET> filled = zone->GetFilledPolysList( F_Cu );

    BOOST_REQUIRE( filled );

    // A point in the copper ring must be covered.
    VECTOR2I copperPoint( int( parser.ScaleSize( 200.0 ) ), int( parser.ScaleSize( 200.0 ) ) );
    // A point inside the hole must NOT be covered by the pre-computed fill.
    VECTOR2I holePoint( int( parser.ScaleSize( 500.0 ) ), int( parser.ScaleSize( 500.0 ) ) );

    BOOST_CHECK( filled->Contains( copperPoint ) );
    BOOST_CHECK( !filled->Contains( holePoint ) );
}


BOOST_AUTO_TEST_SUITE_END()
