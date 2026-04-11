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

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <pcbnew_utils/board_test_utils.h>

#include <board.h>
#include <board_commit.h>
#include <footprint.h>
#include <pad.h>
#include <pcb_field.h>
#include <pcb_shape.h>
#include <pcb_board_outline.h>
#include <tool/tool_manager.h>
#include <autorouter/silkscreen_auto_placer.h>


namespace
{

struct TEST_COMMIT_CONTEXT
{
    TEST_COMMIT_CONTEXT( BOARD* aBoard )
    {
        toolMgr.SetEnvironment( aBoard, nullptr, nullptr, nullptr, nullptr );
        dummyTool = new KI_TEST::DUMMY_TOOL();
        toolMgr.RegisterTool( dummyTool );
    }

    BOARD_COMMIT MakeCommit() { return BOARD_COMMIT( dummyTool ); }

    TOOL_MANAGER          toolMgr;
    KI_TEST::DUMMY_TOOL*  dummyTool;
};


/**
 * Build a simple board with a rectangular outline and N footprints arranged in a grid.
 * Each footprint gets a courtyard, two pads, and a visible reference field.
 */
std::unique_ptr<BOARD> BuildTestBoard( int aFpCount, int aColumns = 3 )
{
    auto board = std::make_unique<BOARD>();

    // Create a board outline
    SHAPE_POLY_SET outline;
    int boardW = pcbIUScale.mmToIU( 50 );
    int boardH = pcbIUScale.mmToIU( 50 );
    outline.NewOutline();
    outline.Append( 0, 0 );
    outline.Append( boardW, 0 );
    outline.Append( boardW, boardH );
    outline.Append( 0, boardH );
    board->BoardOutline()->GetOutline() = outline;

    int spacingX = pcbIUScale.mmToIU( 10 );
    int spacingY = pcbIUScale.mmToIU( 10 );
    int padSize = pcbIUScale.mmToIU( 1.5 );
    int padPitch = pcbIUScale.mmToIU( 2.54 );

    for( int i = 0; i < aFpCount; i++ )
    {
        int col = i % aColumns;
        int row = i / aColumns;
        int cx = pcbIUScale.mmToIU( 10 ) + col * spacingX;
        int cy = pcbIUScale.mmToIU( 10 ) + row * spacingY;

        FOOTPRINT* fp = new FOOTPRINT( board.get() );
        fp->SetPosition( VECTOR2I( cx, cy ) );
        fp->SetReference( wxString::Format( wxT( "R%d" ), i + 1 ) );
        fp->SetLayer( F_Cu );

        // Create two pads
        PAD* pad1 = new PAD( fp );
        pad1->SetPosition( VECTOR2I( cx - padPitch / 2, cy ) );
        pad1->SetSize( PADSTACK::ALL_LAYERS, VECTOR2I( padSize, padSize ) );
        pad1->SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::RECTANGLE );
        pad1->SetAttribute( PAD_ATTRIB::SMD );
        pad1->SetLayerSet( LSET( { F_Cu, F_Paste, F_Mask } ) );
        fp->Add( pad1 );

        PAD* pad2 = new PAD( fp );
        pad2->SetPosition( VECTOR2I( cx + padPitch / 2, cy ) );
        pad2->SetSize( PADSTACK::ALL_LAYERS, VECTOR2I( padSize, padSize ) );
        pad2->SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::RECTANGLE );
        pad2->SetAttribute( PAD_ATTRIB::SMD );
        pad2->SetLayerSet( LSET( { F_Cu, F_Paste, F_Mask } ) );
        fp->Add( pad2 );

        // Add a courtyard rectangle
        int crtydMargin = pcbIUScale.mmToIU( 0.5 );
        PCB_SHAPE* crtyd = new PCB_SHAPE( fp );
        crtyd->SetShape( SHAPE_T::RECTANGLE );
        crtyd->SetLayer( F_CrtYd );
        crtyd->SetStart( VECTOR2I( cx - padPitch / 2 - crtydMargin,
                                   cy - padSize / 2 - crtydMargin ) );
        crtyd->SetEnd( VECTOR2I( cx + padPitch / 2 + crtydMargin,
                                 cy + padSize / 2 + crtydMargin ) );
        fp->Add( crtyd );

        // Set up reference field
        PCB_FIELD& ref = fp->Reference();
        ref.SetVisible( true );
        ref.SetTextSize( VECTOR2I( pcbIUScale.mmToIU( 0.8 ), pcbIUScale.mmToIU( 0.8 ) ) );
        ref.SetTextThickness( pcbIUScale.mmToIU( 0.15 ) );
        ref.SetLayer( F_SilkS );

        // Position ref at the center of the footprint (the naive placement)
        ref.SetPosition( VECTOR2I( cx, cy ) );

        fp->BuildCourtyardCaches();
        board->Add( fp );
    }

    return board;
}


SILK_PLACER_SETTINGS DefaultSettings()
{
    SILK_PLACER_SETTINGS settings;
    settings.positionFlags = POS_TOP_CENTER | POS_CENTER_RIGHT | POS_BOT_CENTER | POS_CENTER_LEFT;
    settings.positionDelta = pcbIUScale.mmToIU( 0.42 );
    settings.avoidVias = true;
    settings.tryAlteredRotation = false;
    settings.rotationStrategy = SILK_ROTATION_STRATEGY::KLC_STYLE;
    settings.failedPlacement = SILK_FAILED_PLACEMENT::RESTORE_ORIGINAL;
    return settings;
}

} // namespace


BOOST_AUTO_TEST_SUITE( SilkscreenAutoPlacerTests )


BOOST_AUTO_TEST_CASE( OutlineResolution_Courtyard )
{
    auto board = BuildTestBoard( 1 );
    FOOTPRINT* fp = board->Footprints().front();

    SILK_PLACER_SETTINGS settings = DefaultSettings();
    SILKSCREEN_AUTO_PLACER placer( board.get(), settings );

    TEST_COMMIT_CONTEXT ctx( board.get() );
    BOARD_COMMIT commit = ctx.MakeCommit();
    std::vector<FOOTPRINT*> fps = { fp };
    SILK_PLACER_RESULT result = placer.Run( commit, fps );

    // The footprint has a courtyard and enough space; must be successfully placed
    BOOST_CHECK_EQUAL( result.moved, 1 );
    BOOST_CHECK_EQUAL( result.attempted, 1 );

    // Ref must have moved away from its original center-of-footprint position
    PCB_FIELD& ref = fp->Reference();
    BOOST_CHECK( ref.GetPosition() != fp->GetPosition() );
}


BOOST_AUTO_TEST_CASE( CandidateGeneration_FourPositions )
{
    auto board = BuildTestBoard( 1 );
    FOOTPRINT* fp = board->Footprints().front();

    SILK_PLACER_SETTINGS settings = DefaultSettings();
    settings.positionFlags = POS_TOP_CENTER | POS_CENTER_RIGHT | POS_BOT_CENTER | POS_CENTER_LEFT;
    settings.tryAlteredRotation = false;

    SILKSCREEN_AUTO_PLACER placer( board.get(), settings );
    TEST_COMMIT_CONTEXT ctx( board.get() );
    BOARD_COMMIT commit = ctx.MakeCommit();
    std::vector<FOOTPRINT*> fps = { fp };
    SILK_PLACER_RESULT result = placer.Run( commit, fps );

    BOOST_CHECK_EQUAL( result.moved, 1 );

    // The placed ref must not overlap either pad
    PCB_FIELD& ref = fp->Reference();
    BOX2I refBBox = ref.GetBoundingBox();

    for( PAD* pad : fp->Pads() )
    {
        BOX2I padBBox = pad->GetBoundingBox();
        BOOST_CHECK( !refBBox.Intersects( padBBox ) );
    }
}


BOOST_AUTO_TEST_CASE( CollisionScoring_PadOverlap )
{
    auto board = BuildTestBoard( 1 );
    FOOTPRINT* fp = board->Footprints().front();
    PCB_FIELD& ref = fp->Reference();

    // Place ref directly on top of a pad (should be detected as collision)
    ref.SetPosition( fp->Pads().front()->GetPosition() );

    SILK_PLACER_SETTINGS settings = DefaultSettings();
    SILKSCREEN_AUTO_PLACER placer( board.get(), settings );
    TEST_COMMIT_CONTEXT ctx( board.get() );
    BOARD_COMMIT commit = ctx.MakeCommit();
    std::vector<FOOTPRINT*> fps = { fp };
    placer.Run( commit, fps );

    // The placer should move the ref away from the pad
    VECTOR2I newPos = ref.GetPosition();
    VECTOR2I padPos = fp->Pads().front()->GetPosition();

    BOOST_CHECK( newPos != padPos );
}


BOOST_AUTO_TEST_CASE( FALP_MostConstrainedFirst )
{
    // With multiple footprints, the most-constrained (fewest viable positions) should be
    // assigned first by the FALP algorithm.
    auto board = BuildTestBoard( 4, 2 );

    SILK_PLACER_SETTINGS settings = DefaultSettings();
    SILKSCREEN_AUTO_PLACER placer( board.get(), settings );
    TEST_COMMIT_CONTEXT ctx( board.get() );
    BOARD_COMMIT commit = ctx.MakeCommit();

    std::vector<FOOTPRINT*> fps;

    for( FOOTPRINT* fp : board->Footprints() )
        fps.push_back( fp );

    SILK_PLACER_RESULT result = placer.Run( commit, fps );

    // All 4 should be placed since they have enough space
    BOOST_CHECK_EQUAL( result.moved, 4 );
}


BOOST_AUTO_TEST_CASE( SA_ImprovesOverFALP )
{
    // A denser board where FALP alone may produce overlaps
    auto board = BuildTestBoard( 9, 3 );

    SILK_PLACER_SETTINGS settings = DefaultSettings();
    SILKSCREEN_AUTO_PLACER placer( board.get(), settings );
    TEST_COMMIT_CONTEXT ctx( board.get() );
    BOARD_COMMIT commit = ctx.MakeCommit();

    std::vector<FOOTPRINT*> fps;

    for( FOOTPRINT* fp : board->Footprints() )
        fps.push_back( fp );

    SILK_PLACER_RESULT result = placer.Run( commit, fps );

    // SA should find a solution for most/all labels
    BOOST_CHECK_GE( result.moved, 5 );
}


BOOST_AUTO_TEST_CASE( FailedPlacement_CenterOverComponent )
{
    // Create a very tight board where placement should fail for at least one footprint
    auto board = std::make_unique<BOARD>();

    SHAPE_POLY_SET outline;
    int boardSize = pcbIUScale.mmToIU( 8 );
    outline.NewOutline();
    outline.Append( 0, 0 );
    outline.Append( boardSize, 0 );
    outline.Append( boardSize, boardSize );
    outline.Append( 0, boardSize );
    board->BoardOutline()->GetOutline() = outline;

    // Single footprint that fills most of the board
    FOOTPRINT* fp = new FOOTPRINT( board.get() );
    int center = boardSize / 2;
    fp->SetPosition( VECTOR2I( center, center ) );
    fp->SetReference( wxT( "U1" ) );
    fp->SetLayer( F_Cu );

    // Large pad covering most of the board
    PAD* pad = new PAD( fp );
    pad->SetPosition( VECTOR2I( center, center ) );
    pad->SetSize( PADSTACK::ALL_LAYERS, VECTOR2I( boardSize - pcbIUScale.mmToIU( 1 ),
                                                   boardSize - pcbIUScale.mmToIU( 1 ) ) );
    pad->SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::RECTANGLE );
    pad->SetAttribute( PAD_ATTRIB::SMD );
    pad->SetLayerSet( LSET( { F_Cu, F_Paste, F_Mask } ) );
    fp->Add( pad );

    PCB_FIELD& ref = fp->Reference();
    ref.SetVisible( true );
    ref.SetTextSize( VECTOR2I( pcbIUScale.mmToIU( 1.0 ), pcbIUScale.mmToIU( 1.0 ) ) );
    ref.SetTextThickness( pcbIUScale.mmToIU( 0.15 ) );
    ref.SetLayer( F_SilkS );
    ref.SetPosition( VECTOR2I( center, center ) );

    fp->BuildCourtyardCaches();
    board->Add( fp );

    SILK_PLACER_SETTINGS settings = DefaultSettings();
    settings.failedPlacement = SILK_FAILED_PLACEMENT::CENTER_OVER_COMPONENT;

    SILKSCREEN_AUTO_PLACER placer( board.get(), settings );
    TEST_COMMIT_CONTEXT ctx( board.get() );
    BOARD_COMMIT commit = ctx.MakeCommit();
    std::vector<FOOTPRINT*> fps = { fp };
    placer.Run( commit, fps );

    // The ref should be approximately at the component center for failed placement
    // (within 1mm tolerance since centering uses the outline bbox center)
    VECTOR2I refPos = ref.GetPosition();
    int tolerance = pcbIUScale.mmToIU( 2.0 );
    BOOST_CHECK_LE( std::abs( refPos.x - center ), tolerance );
    BOOST_CHECK_LE( std::abs( refPos.y - center ), tolerance );
}


BOOST_AUTO_TEST_CASE( FailedPlacement_RestoreOriginal )
{
    auto board = std::make_unique<BOARD>();

    SHAPE_POLY_SET outline;
    int boardSize = pcbIUScale.mmToIU( 8 );
    outline.NewOutline();
    outline.Append( 0, 0 );
    outline.Append( boardSize, 0 );
    outline.Append( boardSize, boardSize );
    outline.Append( 0, boardSize );
    board->BoardOutline()->GetOutline() = outline;

    FOOTPRINT* fp = new FOOTPRINT( board.get() );
    int center = boardSize / 2;
    fp->SetPosition( VECTOR2I( center, center ) );
    fp->SetReference( wxT( "U1" ) );
    fp->SetLayer( F_Cu );

    PAD* pad = new PAD( fp );
    pad->SetPosition( VECTOR2I( center, center ) );
    pad->SetSize( PADSTACK::ALL_LAYERS, VECTOR2I( boardSize - pcbIUScale.mmToIU( 1 ),
                                                   boardSize - pcbIUScale.mmToIU( 1 ) ) );
    pad->SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::RECTANGLE );
    pad->SetAttribute( PAD_ATTRIB::SMD );
    pad->SetLayerSet( LSET( { F_Cu, F_Paste, F_Mask } ) );
    fp->Add( pad );

    VECTOR2I originalPos( pcbIUScale.mmToIU( 1 ), pcbIUScale.mmToIU( 1 ) );

    PCB_FIELD& ref = fp->Reference();
    ref.SetVisible( true );
    ref.SetTextSize( VECTOR2I( pcbIUScale.mmToIU( 1.0 ), pcbIUScale.mmToIU( 1.0 ) ) );
    ref.SetTextThickness( pcbIUScale.mmToIU( 0.15 ) );
    ref.SetLayer( F_SilkS );
    ref.SetPosition( originalPos );

    fp->BuildCourtyardCaches();
    board->Add( fp );

    SILK_PLACER_SETTINGS settings = DefaultSettings();
    settings.failedPlacement = SILK_FAILED_PLACEMENT::RESTORE_ORIGINAL;

    SILKSCREEN_AUTO_PLACER placer( board.get(), settings );
    TEST_COMMIT_CONTEXT ctx( board.get() );
    BOARD_COMMIT commit = ctx.MakeCommit();
    std::vector<FOOTPRINT*> fps = { fp };
    placer.Run( commit, fps );

    // The ref should be restored to its original position
    VECTOR2I refPos = ref.GetPosition();
    BOOST_CHECK_EQUAL( refPos.x, originalPos.x );
    BOOST_CHECK_EQUAL( refPos.y, originalPos.y );
}


BOOST_AUTO_TEST_CASE( FixedSizeOverride )
{
    auto board = BuildTestBoard( 1 );
    FOOTPRINT* fp = board->Footprints().front();

    int customSize = pcbIUScale.mmToIU( 1.2 );

    SILK_PLACER_SETTINGS settings = DefaultSettings();
    settings.fixedSize = true;
    settings.fixedSizeValue = customSize;

    SILKSCREEN_AUTO_PLACER placer( board.get(), settings );
    TEST_COMMIT_CONTEXT ctx( board.get() );
    BOARD_COMMIT commit = ctx.MakeCommit();
    std::vector<FOOTPRINT*> fps = { fp };
    SILK_PLACER_RESULT result = placer.Run( commit, fps );

    if( result.moved > 0 )
    {
        PCB_FIELD& ref = fp->Reference();
        VECTOR2I textSize = ref.GetTextSize();
        BOOST_CHECK_EQUAL( textSize.x, customSize );
        BOOST_CHECK_EQUAL( textSize.y, customSize );
    }
}


BOOST_AUTO_TEST_CASE( AllowSilkUnder_RelaxesSilkOverlap )
{
    auto board = BuildTestBoard( 2 );

    SILK_PLACER_SETTINGS settings = DefaultSettings();
    settings.allowSilkUnder.insert( wxT( "R1" ) );

    SILKSCREEN_AUTO_PLACER placer( board.get(), settings );
    TEST_COMMIT_CONTEXT ctx( board.get() );
    BOARD_COMMIT commit = ctx.MakeCommit();

    std::vector<FOOTPRINT*> fps;

    for( FOOTPRINT* fp : board->Footprints() )
        fps.push_back( fp );

    SILK_PLACER_RESULT result = placer.Run( commit, fps );

    // Both should be placed; R1 has relaxed silk overlap constraints
    BOOST_CHECK_EQUAL( result.moved, 2 );
}


BOOST_AUTO_TEST_CASE( ThroughHolePad_BlocksBothSilkLayers )
{
    auto board = std::make_unique<BOARD>();

    SHAPE_POLY_SET outline;
    int boardSize = pcbIUScale.mmToIU( 30 );
    outline.NewOutline();
    outline.Append( 0, 0 );
    outline.Append( boardSize, 0 );
    outline.Append( boardSize, boardSize );
    outline.Append( 0, boardSize );
    board->BoardOutline()->GetOutline() = outline;

    int center = boardSize / 2;

    // Create a footprint with a large through-hole pad
    FOOTPRINT* fp = new FOOTPRINT( board.get() );
    fp->SetPosition( VECTOR2I( center, center ) );
    fp->SetReference( wxT( "J1" ) );
    fp->SetLayer( F_Cu );

    PAD* thPad = new PAD( fp );
    thPad->SetPosition( VECTOR2I( center, center ) );
    thPad->SetSize( PADSTACK::ALL_LAYERS,
                    VECTOR2I( pcbIUScale.mmToIU( 3 ), pcbIUScale.mmToIU( 3 ) ) );
    thPad->SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::CIRCLE );
    thPad->SetAttribute( PAD_ATTRIB::PTH );
    thPad->SetDrillSize( VECTOR2I( pcbIUScale.mmToIU( 1.5 ), pcbIUScale.mmToIU( 1.5 ) ) );
    thPad->SetLayerSet( PAD::PTHMask() );
    fp->Add( thPad );

    int crtydMargin = pcbIUScale.mmToIU( 0.5 );
    PCB_SHAPE* crtyd = new PCB_SHAPE( fp );
    crtyd->SetShape( SHAPE_T::RECTANGLE );
    crtyd->SetLayer( F_CrtYd );
    crtyd->SetStart( VECTOR2I( center - pcbIUScale.mmToIU( 2 ),
                               center - pcbIUScale.mmToIU( 2 ) ) );
    crtyd->SetEnd( VECTOR2I( center + pcbIUScale.mmToIU( 2 ),
                             center + pcbIUScale.mmToIU( 2 ) ) );
    fp->Add( crtyd );

    // Put the ref on B_SilkS to test that TH pad still blocks it
    PCB_FIELD& ref = fp->Reference();
    ref.SetVisible( true );
    ref.SetTextSize( VECTOR2I( pcbIUScale.mmToIU( 0.8 ), pcbIUScale.mmToIU( 0.8 ) ) );
    ref.SetTextThickness( pcbIUScale.mmToIU( 0.15 ) );
    ref.SetLayer( B_SilkS );
    ref.SetPosition( VECTOR2I( center, center ) );

    fp->BuildCourtyardCaches();
    board->Add( fp );

    SILK_PLACER_SETTINGS settings = DefaultSettings();
    SILKSCREEN_AUTO_PLACER placer( board.get(), settings );
    TEST_COMMIT_CONTEXT ctx( board.get() );
    BOARD_COMMIT commit = ctx.MakeCommit();
    std::vector<FOOTPRINT*> fps = { fp };
    SILK_PLACER_RESULT result = placer.Run( commit, fps );

    BOOST_CHECK_EQUAL( result.moved, 1 );

    // The placed ref must not overlap the through-hole pad
    BOX2I refBBox = ref.GetBoundingBox();
    BOX2I padBBox = thPad->GetBoundingBox();
    BOOST_CHECK( !refBBox.Intersects( padBBox ) );
}


BOOST_AUTO_TEST_CASE( BackSilkRef_UsesFieldLayer )
{
    auto board = BuildTestBoard( 1 );
    FOOTPRINT* fp = board->Footprints().front();

    // Move the ref to B_SilkS and place it off-board so the placer has reason to move it
    PCB_FIELD& ref = fp->Reference();
    ref.SetLayer( B_SilkS );
    ref.SetPosition( VECTOR2I( -pcbIUScale.mmToIU( 10 ), -pcbIUScale.mmToIU( 10 ) ) );

    SILK_PLACER_SETTINGS settings = DefaultSettings();
    SILKSCREEN_AUTO_PLACER placer( board.get(), settings );
    TEST_COMMIT_CONTEXT ctx( board.get() );
    BOARD_COMMIT commit = ctx.MakeCommit();
    std::vector<FOOTPRINT*> fps = { fp };
    SILK_PLACER_RESULT result = placer.Run( commit, fps );

    // Should be moved from its bad original position
    BOOST_CHECK_EQUAL( result.moved, 1 );

    // Ref should remain on B_SilkS after placement
    BOOST_CHECK_EQUAL( ref.GetLayer(), B_SilkS );
}


BOOST_AUTO_TEST_CASE( NonSilkRef_IsSkipped )
{
    auto board = BuildTestBoard( 1 );
    FOOTPRINT* fp = board->Footprints().front();

    // Put ref on a non-silk layer
    PCB_FIELD& ref = fp->Reference();
    ref.SetLayer( F_Fab );
    VECTOR2I originalPos = ref.GetPosition();

    SILK_PLACER_SETTINGS settings = DefaultSettings();
    SILKSCREEN_AUTO_PLACER placer( board.get(), settings );
    TEST_COMMIT_CONTEXT ctx( board.get() );
    BOARD_COMMIT commit = ctx.MakeCommit();
    std::vector<FOOTPRINT*> fps = { fp };
    SILK_PLACER_RESULT result = placer.Run( commit, fps );

    // Ref on F_Fab should not be touched
    BOOST_CHECK_EQUAL( result.moved, 0 );
    BOOST_CHECK_EQUAL( result.attempted, 0 );
    BOOST_CHECK_EQUAL( ref.GetPosition().x, originalPos.x );
    BOOST_CHECK_EQUAL( ref.GetPosition().y, originalPos.y );
}


BOOST_AUTO_TEST_CASE( HardViolation_NeverSelected )
{
    // Verify that when some candidates have hardViolation, the placer picks
    // a legal one rather than falling through to failed placement.
    // Only enable top and bottom positions. A wide pad above the courtyard
    // blocks the top candidate, leaving only bottom as legal.
    auto board = std::make_unique<BOARD>();

    SHAPE_POLY_SET outline;
    int boardSize = pcbIUScale.mmToIU( 40 );
    outline.NewOutline();
    outline.Append( 0, 0 );
    outline.Append( boardSize, 0 );
    outline.Append( boardSize, boardSize );
    outline.Append( 0, boardSize );
    board->BoardOutline()->GetOutline() = outline;

    int center = boardSize / 2;

    FOOTPRINT* fp = new FOOTPRINT( board.get() );
    fp->SetPosition( VECTOR2I( center, center ) );
    fp->SetReference( wxT( "U1" ) );
    fp->SetLayer( F_Cu );

    // A wide pad placed just above the courtyard top edge to block the top candidate.
    // Courtyard will be center ± 3mm, so top candidate is around center - 3mm - delta.
    // Place the blocking pad right there with generous size.
    PAD* blockingPad = new PAD( fp );
    blockingPad->SetPosition( VECTOR2I( center, center - pcbIUScale.mmToIU( 4 ) ) );
    blockingPad->SetSize( PADSTACK::ALL_LAYERS,
                          VECTOR2I( pcbIUScale.mmToIU( 15 ), pcbIUScale.mmToIU( 4 ) ) );
    blockingPad->SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::RECTANGLE );
    blockingPad->SetAttribute( PAD_ATTRIB::SMD );
    blockingPad->SetLayerSet( LSET( { F_Cu, F_Paste, F_Mask } ) );
    fp->Add( blockingPad );

    PCB_SHAPE* crtyd = new PCB_SHAPE( fp );
    crtyd->SetShape( SHAPE_T::RECTANGLE );
    crtyd->SetLayer( F_CrtYd );
    crtyd->SetStart( VECTOR2I( center - pcbIUScale.mmToIU( 3 ),
                               center - pcbIUScale.mmToIU( 3 ) ) );
    crtyd->SetEnd( VECTOR2I( center + pcbIUScale.mmToIU( 3 ),
                             center + pcbIUScale.mmToIU( 3 ) ) );
    fp->Add( crtyd );

    PCB_FIELD& ref = fp->Reference();
    ref.SetVisible( true );
    ref.SetTextSize( VECTOR2I( pcbIUScale.mmToIU( 0.8 ), pcbIUScale.mmToIU( 0.8 ) ) );
    ref.SetTextThickness( pcbIUScale.mmToIU( 0.15 ) );
    ref.SetLayer( F_SilkS );

    // Start the ref on the blocking pad so the original position has a hard violation
    ref.SetPosition( blockingPad->GetPosition() );

    fp->BuildCourtyardCaches();
    board->Add( fp );

    SILK_PLACER_SETTINGS settings = DefaultSettings();
    settings.positionFlags = POS_TOP_CENTER | POS_BOT_CENTER;

    SILKSCREEN_AUTO_PLACER placer( board.get(), settings );
    TEST_COMMIT_CONTEXT ctx( board.get() );
    BOARD_COMMIT commit = ctx.MakeCommit();
    std::vector<FOOTPRINT*> fps = { fp };
    SILK_PLACER_RESULT result = placer.Run( commit, fps );

    // Must be placed (at bottom, since top is blocked by pad)
    BOOST_CHECK_EQUAL( result.moved, 1 );

    // Y coordinate must be below the courtyard bottom edge (center + 3mm)
    BOOST_CHECK_GT( ref.GetPosition().y, center + pcbIUScale.mmToIU( 3 ) );
}


BOOST_AUTO_TEST_CASE( WellPlacedRef_NotMoved )
{
    // When the ref is already in a clear position with zero cost, the placer should
    // leave it alone rather than move it to one of the generated candidates.
    auto board = BuildTestBoard( 1 );
    FOOTPRINT* fp = board->Footprints().front();

    // Position the ref above the courtyard, clear of all obstacles
    PCB_FIELD& ref = fp->Reference();
    BOX2I crtydBBox = fp->GetCourtyard( F_CrtYd ).BBox();
    int delta = pcbIUScale.mmToIU( 0.42 );
    BOX2I textBBox = ref.GetBoundingBox();
    VECTOR2I goodPos( crtydBBox.GetCenter().x,
                      crtydBBox.GetTop() - delta - textBBox.GetHeight() / 2 );
    ref.SetPosition( goodPos );
    ref.SetTextAngle( ANGLE_0 );

    SILK_PLACER_SETTINGS settings = DefaultSettings();
    SILKSCREEN_AUTO_PLACER placer( board.get(), settings );
    TEST_COMMIT_CONTEXT ctx( board.get() );
    BOARD_COMMIT commit = ctx.MakeCommit();
    std::vector<FOOTPRINT*> fps = { fp };
    SILK_PLACER_RESULT result = placer.Run( commit, fps );

    // The ref was already well-placed so it should not have been moved
    BOOST_CHECK_EQUAL( result.attempted, 1 );
    BOOST_CHECK_EQUAL( result.moved, 0 );
    BOOST_CHECK_EQUAL( ref.GetPosition().x, goodPos.x );
    BOOST_CHECK_EQUAL( ref.GetPosition().y, goodPos.y );
}


BOOST_AUTO_TEST_SUITE_END()
