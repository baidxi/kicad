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
 * @file test_netlist_updater.cpp
 * Tests for the netlist matching logic used in BOARD_NETLIST_UPDATER.
 * These tests verify that the hybrid UUID/reference matching works correctly.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <kiid.h>
#include <netlist_reader/pcb_netlist.h>


BOOST_AUTO_TEST_SUITE( NetlistUpdater )


/**
 * Test that NETLIST::GetComponentByPath finds components correctly
 */
BOOST_AUTO_TEST_CASE( NetlistGetComponentByPath )
{
    NETLIST netlist;

    // Create component with specific path and KIIDs
    KIID sheetUuid;
    KIID symbolUuid;
    KIID_PATH componentPath;
    componentPath.push_back( sheetUuid );

    COMPONENT* component = new COMPONENT( LIB_ID( "TestLib", "TestFP" ),
                                           "R1", "10K", componentPath, { symbolUuid } );
    netlist.AddComponent( component );

    // Build the full path as the updater would
    KIID_PATH fullPath = componentPath;
    fullPath.push_back( symbolUuid );

    // GetComponentByPath should find the component
    COMPONENT* found = netlist.GetComponentByPath( fullPath );
    BOOST_CHECK( found != nullptr );
    BOOST_CHECK_EQUAL( found->GetReference(), "R1" );
}


/**
 * Test that NETLIST::GetComponentByReference finds components correctly
 */
BOOST_AUTO_TEST_CASE( NetlistGetComponentByReference )
{
    NETLIST netlist;

    // Create component with specific reference
    KIID sheetUuid;
    KIID symbolUuid;
    KIID_PATH componentPath;
    componentPath.push_back( sheetUuid );

    COMPONENT* component = new COMPONENT( LIB_ID( "TestLib", "TestFP" ),
                                           "C1", "100nF", componentPath, { symbolUuid } );
    netlist.AddComponent( component );

    // GetComponentByReference should find the component
    COMPONENT* found = netlist.GetComponentByReference( "C1" );
    BOOST_CHECK( found != nullptr );
    BOOST_CHECK_EQUAL( found->GetValue(), "100nF" );

    // Should not find non-existent reference
    COMPONENT* notFound = netlist.GetComponentByReference( "R1" );
    BOOST_CHECK( notFound == nullptr );
}


/**
 * Test that KIID_PATH matching works correctly
 * This tests the same logic used in the hybrid matching approach
 */
BOOST_AUTO_TEST_CASE( KIIDPathMatching )
{
    // Create two different UUIDs
    KIID uuid1;
    KIID uuid2;

    // Create paths
    KIID_PATH path1;
    path1.push_back( uuid1 );
    path1.push_back( uuid2 );

    KIID_PATH path2;
    path2.push_back( uuid1 );
    path2.push_back( uuid2 );

    KIID_PATH path3;
    KIID uuid3;
    path3.push_back( uuid1 );
    path3.push_back( uuid3 );  // Different second UUID

    // Same paths should be equal
    BOOST_CHECK( path1 == path2 );

    // Different paths should not be equal
    BOOST_CHECK( path1 != path3 );
}


/**
 * Test the path building logic used in hybrid matching
 * When matching, the updater builds: base = component.GetPath(), base.push_back(kiid)
 */
BOOST_AUTO_TEST_CASE( PathBuildingLogic )
{
    KIID sheetUuid;
    KIID symbolUuid;

    // Simulate footprint path (already includes symbol UUID)
    KIID_PATH footprintPath;
    footprintPath.push_back( sheetUuid );
    footprintPath.push_back( symbolUuid );

    // Simulate component path (sheet only) and KIIDs (symbol UUID)
    KIID_PATH componentPath;
    componentPath.push_back( sheetUuid );
    std::vector<KIID> kiids = { symbolUuid };

    // Build full path as updater does
    for( const KIID& uuid : kiids )
    {
        KIID_PATH base = componentPath;
        base.push_back( uuid );

        // This is the matching logic from UpdateNetlist
        if( footprintPath == base )
        {
            BOOST_CHECK( true );  // Match found
            return;
        }
    }

    BOOST_FAIL( "Expected path match was not found" );
}


/**
 * Test that reference matching is case-insensitive
 * The fallback matching uses CmpNoCase
 */
BOOST_AUTO_TEST_CASE( ReferenceCaseInsensitiveMatching )
{
    wxString ref1 = "R1";
    wxString ref2 = "r1";
    wxString ref3 = "R2";

    // CmpNoCase returns 0 for equal strings
    BOOST_CHECK_EQUAL( ref1.CmpNoCase( ref2 ), 0 );

    // CmpNoCase returns non-zero for different strings
    BOOST_CHECK( ref1.CmpNoCase( ref3 ) != 0 );
}


BOOST_AUTO_TEST_SUITE_END()
