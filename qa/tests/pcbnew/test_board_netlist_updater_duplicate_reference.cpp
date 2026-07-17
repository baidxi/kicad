/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
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

/*
 * Regression test for GitLab MR !2375 ("Remove the user option to select refid or UUID").
 *
 * A stray extra footprint that merely shares a reference designator with a footprint already
 * claimed by a schematic component must remain eligible for deletion by
 * SetDeleteUnusedFootprints(), rather than being silently kept because the reference-designator
 * fallback matches it to the same already-matched component.
 */

#include <board.h>
#include <footprint.h>
#include <netlist_reader/board_netlist_updater.h>
#include <netlist_reader/pcb_netlist.h>
#include <lib_id.h>
#include <project.h>
#include <settings/settings_manager.h>
#include <tool/tool_manager.h>
#include <pcbnew_utils/board_test_utils.h>
#include <qa_utils/wx_utils/unit_test_utils.h>


namespace
{

FOOTPRINT* addFootprint( BOARD* aBoard, const wxString& aRef, const LIB_ID& aFpid )
{
    FOOTPRINT* fp = new FOOTPRINT( aBoard );
    fp->SetReference( aRef );
    fp->SetFPID( aFpid );
    aBoard->Add( fp );
    return fp;
}

} // namespace


BOOST_AUTO_TEST_SUITE( BoardNetlistUpdaterDuplicateReference )


BOOST_AUTO_TEST_CASE( DuplicateReferenceFootprintIsStillDeletable )
{
    // The updater dereferences the board's project, so the board needs one.
    SETTINGS_MANAGER settingsManager;
    settingsManager.LoadProject( "" );

    std::unique_ptr<BOARD> board = std::make_unique<BOARD>();
    board->SetProject( &settingsManager.Prj() );

    LIB_ID fpid;
    BOOST_REQUIRE_EQUAL( fpid.Parse( wxS( "TestLib:R" ) ), -1 );

    // Two identical footprints share the reference "R1" (e.g. from a copy/paste mistake), but
    // the netlist only has one R1 component. Neither footprint's stored path matches the
    // component's KIID, so only one of them can be claimed via the reference-designator
    // fallback; the other is a genuine leftover duplicate and must remain deletable.
    addFootprint( board.get(), wxS( "R1" ), fpid );
    addFootprint( board.get(), wxS( "R1" ), fpid );

    NETLIST netlist;

    KIID symbolUuid;
    netlist.AddComponent( new COMPONENT( fpid, wxS( "R1" ), wxS( "10K" ), KIID_PATH(),
                                          std::vector<KIID>{ symbolUuid } ) );

    TOOL_MANAGER toolMgr;
    toolMgr.SetEnvironment( board.get(), nullptr, nullptr, nullptr, nullptr );
    toolMgr.RegisterTool( new KI_TEST::DUMMY_TOOL() );

    BOARD_NETLIST_UPDATER updater( &toolMgr, board.get() );
    updater.SetReplaceFootprints( false );
    updater.SetDeleteUnusedFootprints( true );

    BOOST_REQUIRE( updater.UpdateNetlist( netlist ) );

    BOOST_CHECK_MESSAGE( board->Footprints().size() == 1,
                         wxString::Format( "expected exactly one R1 footprint to remain on the "
                                            "board after update (the un-matched duplicate should "
                                            "have been deleted), found %zu",
                                            board->Footprints().size() ).ToStdString() );
}


BOOST_AUTO_TEST_SUITE_END()
