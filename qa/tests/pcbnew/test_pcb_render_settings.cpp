#include <pcb_generator.h>
/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.TXT for contributors.
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
 * @file test_pcb_render_settings.cpp
 * Test suite for PCB_RENDER_SETTINGS
 */

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <pcbnew_utils/board_test_utils.h>
#include <settings/settings_manager.h>
#include <zone.h>
#include <pcb_painter.h>
#include <pcb_point.h>
#include <layer_ids.h>
#include <settings/color_settings.h>
#include <board.h>
#include <base_units.h>
#include <pcb_track.h>
#include <pcb_view.h>
#include <footprint.h>
#include <pad.h>
#include <gal/gal_display_options.h>
#include <gal/graphics_abstraction_layer.h>

class TEST_PCB_RENDER_SETTINGS_FIXTURE
{
public:
    KIGFX::PCB_RENDER_SETTINGS m_settings;
    COLOR_SETTINGS             m_colorSettings;

    TEST_PCB_RENDER_SETTINGS_FIXTURE()
    {
        m_colorSettings.SetColor( LAYER_POINTS, KIGFX::COLOR4D( 1.0, 0.0, 1.0, 1.0 ) );

        for( int i = 0; i < PCB_LAYER_ID_COUNT; i++ )
            m_colorSettings.SetColor( i, KIGFX::COLOR4D( 0.5, 0.5, 0.5, 1.0 ) );

        m_settings.LoadColors( &m_colorSettings );
    }
};


BOOST_FIXTURE_TEST_SUITE( PcbRenderSettings, TEST_PCB_RENDER_SETTINGS_FIXTURE )


/**
 * Test that PCB_RENDER_SETTINGS::GetColor returns a visible color for POINT layers.
 *
 * This is a regression test for issue #22634 where pressing ESC caused footprint
 * points to vanish because UpdateAllLayersColor() was requesting the color for
 * the virtual POINT layer (LAYER_POINT_START + boardLayer), which had no color
 * defined and returned transparent.
 */
BOOST_AUTO_TEST_CASE( PointLayerColorIsVisible )
{
    PCB_POINT point( nullptr );
    point.SetLayer( F_SilkS );

    int pointLayer = POINT_LAYER_FOR( F_SilkS );

    KIGFX::COLOR4D color = m_settings.GetColor( &point, pointLayer );

    BOOST_CHECK_MESSAGE( color.a > 0.0, "Color for POINT layer should not be transparent (alpha was "
                                                + std::to_string( color.a ) + ")" );

    BOOST_CHECK_MESSAGE( color == m_colorSettings.GetColor( LAYER_POINTS ),
                         "POINT layer color should match LAYER_POINTS color" );
}


/**
 * Test that multiple board layers all map to LAYER_POINTS color.
 */
BOOST_AUTO_TEST_CASE( AllPointLayersMapToLayerPoints )
{
    KIGFX::COLOR4D expectedColor = m_colorSettings.GetColor( LAYER_POINTS );

    std::vector<PCB_LAYER_ID> testLayers = { F_Cu, B_Cu, F_SilkS, B_SilkS, Edge_Cuts, User_1 };

    for( PCB_LAYER_ID boardLayer : testLayers )
    {
        PCB_POINT point( nullptr );
        point.SetLayer( boardLayer );

        int            pointLayer = POINT_LAYER_FOR( boardLayer );
        KIGFX::COLOR4D color = m_settings.GetColor( &point, pointLayer );

        BOOST_CHECK_MESSAGE( color == expectedColor,
                             "POINT layer for " + std::to_string( boardLayer ) + " should have LAYER_POINTS color" );
    }
}


/**
 * Test Net Color Styles (Solid, Outline/Halo, Spine) and GetItemNetColor resolution.
 */
BOOST_AUTO_TEST_CASE( TestSketchModeNetColors )
{
    BOARD board;
    board.SetCopperLayerCount( 2 );

    const KIGFX::COLOR4D fcuLayerColor( 0.8, 0.2, 0.2, 1.0 );
    const KIGFX::COLOR4D netCustomColor( 0.2, 0.9, 0.3, 1.0 );

    m_settings.SetLayerColor( F_Cu, fcuLayerColor );

    NETINFO_ITEM* netinfo = new NETINFO_ITEM( &board, "TEST_NET", 1 );
    board.Add( netinfo );
    int testNetCode = netinfo->GetNetCode();
    m_settings.GetNetColorMap()[testNetCode] = netCustomColor;
    m_settings.SetNetColorMode( NET_COLOR_MODE::ALL );

    // Pad with sketch mode forced
    FOOTPRINT* footprint = new FOOTPRINT( &board );
    board.Add( footprint );
    PAD* pad = new PAD( footprint );
    pad->SetLayerSet( LSET( { F_Cu } ) );
    pad->SetNet( netinfo );
    footprint->Add( pad );

    m_settings.m_ForcePadSketchModeOn = true;

    for( NET_COLOR_STYLE style : { NET_COLOR_STYLE::SOLID, NET_COLOR_STYLE::OUTLINE_HALO, NET_COLOR_STYLE::SPINE } )
    {
        m_settings.SetNetColorStyle( style );
        KIGFX::COLOR4D padColor = m_settings.GetColor( pad, F_Cu );
        BOOST_CHECK_MESSAGE( padColor == netCustomColor,
                             "Pad in sketch mode should have net color for all net color styles" );
    }

    m_settings.m_ForcePadSketchModeOn = false;
}


BOOST_AUTO_TEST_CASE( NetColorAreaRatioControl )
{
    BOOST_CHECK_CLOSE( m_settings.GetNetColorAreaRatio(), KIGFX::PCB_RENDER_SETTINGS::DEFAULT_NET_COLOR_AREA_RATIO,
                       1e-5 );
    BOOST_CHECK_CLOSE( m_settings.GetNetColorAreaRatio(), 0.50, 1e-5 );

    m_settings.SetNetColorAreaRatio( 0.40 );
    BOOST_CHECK_CLOSE( m_settings.GetNetColorAreaRatio(), 0.40, 1e-5 );

    m_settings.SetNetColorAreaRatio( KIGFX::PCB_RENDER_SETTINGS::DEFAULT_NET_COLOR_AREA_RATIO );

    // Verify annular ring 50% area division formula:
    // Outer radius R = 1.0, hole radius r_h = 0.4
    // Total area = pi * (1.0^2 - 0.4^2) = pi * 0.84
    // Half area = pi * 0.42
    // Mid radius r_mid = sqrt((1.0^2 + 0.4^2) / 2) = sqrt(1.16 / 2) = sqrt(0.58)
    // Inner band area = pi * (0.58 - 0.16) = pi * 0.42 (50.0%)
    // Outer band area = pi * (1.00 - 0.58) = pi * 0.42 (50.0%)
    double R = 1.0;
    double r_h = 0.4;
    double totalArea = M_PI * ( R * R - r_h * r_h );
    double r_mid = std::sqrt( ( R * R + r_h * r_h ) / 2.0 );
    double innerArea = M_PI * ( r_mid * r_mid - r_h * r_h );
    double outerArea = M_PI * ( R * R - r_mid * r_mid );

    BOOST_CHECK_CLOSE( innerArea / totalArea, 0.50, 1e-5 );
    BOOST_CHECK_CLOSE( outerArea / totalArea, 0.50, 1e-5 );
}


BOOST_AUTO_TEST_CASE( NetColorStylesResolution )
{
    BOARD board;
    board.SetCopperLayerCount( 2 );

    const KIGFX::COLOR4D fcuLayerColor( 0.8, 0.2, 0.2, 1.0 );
    const KIGFX::COLOR4D netCustomColor( 0.2, 0.9, 0.3, 1.0 );

    m_settings.SetLayerColor( F_Cu, fcuLayerColor );

    // Configure a net with a custom color override
    NETINFO_ITEM* netinfo = new NETINFO_ITEM( &board, "TEST_NET", 1 );
    board.Add( netinfo );
    int testNetCode = netinfo->GetNetCode();
    m_settings.GetNetColorMap()[testNetCode] = netCustomColor;

    PCB_TRACK track( &board );
    track.SetLayer( F_Cu );
    track.SetNet( netinfo );

    KIGFX::COLOR4D retrievedNetColor;
    bool           hasNet = m_settings.GetItemNetColor( &track, F_Cu, retrievedNetColor );

    BOOST_CHECK_MESSAGE( hasNet, "GetItemNetColor should find configured net color" );
    BOOST_CHECK_MESSAGE( retrievedNetColor == netCustomColor, "Retrieved net color should match configured net color" );

    // Test SOLID style: GetColor returns the net color override
    m_settings.SetNetColorMode( NET_COLOR_MODE::ALL );
    m_settings.SetNetColorStyle( NET_COLOR_STYLE::SOLID );
    KIGFX::COLOR4D solidColor = m_settings.GetColor( &track, F_Cu );
    BOOST_CHECK_MESSAGE( solidColor == netCustomColor,
                         "In SOLID style with Net Color Mode ALL, GetColor should return net color" );

    // Test OUTLINE_HALO style: base copper pass returns net color (outer halo), overlay pass returns layer color
    m_settings.SetNetColorStyle( NET_COLOR_STYLE::OUTLINE_HALO );
    KIGFX::COLOR4D haloColor = m_settings.GetColor( &track, F_Cu );
    BOOST_CHECK_MESSAGE( haloColor == netCustomColor,
                         "In OUTLINE_HALO style, GetColor on base copper layer should return net color" );
    KIGFX::COLOR4D haloCoreColor = m_settings.GetColor( &track, NETCOLOR_LAYER_FOR( F_Cu ) );
    BOOST_CHECK_MESSAGE( haloCoreColor == fcuLayerColor,
                         "In OUTLINE_HALO style, GetColor on netcolor overlay pass should return layer color" );

    // Test SPINE style: GetColor returns layer color
    m_settings.SetNetColorStyle( NET_COLOR_STYLE::SPINE );
    KIGFX::COLOR4D spineColor = m_settings.GetColor( &track, F_Cu );
    BOOST_CHECK_MESSAGE( spineColor == fcuLayerColor, "In SPINE style, GetColor should return base layer color" );
}


BOOST_AUTO_TEST_SUITE_END()


namespace
{

class RECORDING_GAL : public KIGFX::GAL
{
public:
    explicit RECORDING_GAL( KIGFX::GAL_DISPLAY_OPTIONS& aOptions ) :
            GAL( aOptions )
    {
    }

    int BeginGroup() override
    {
        m_openGroup = m_nextGroup++;
        m_groups[m_openGroup].clear();
        return m_openGroup;
    }

    void EndGroup() override { m_openGroup = -1; }

    void DeleteGroup( int aGroupNumber ) override { m_groups.erase( aGroupNumber ); }

    void ClearCache() override { m_groups.clear(); }

    void DrawCircle( const VECTOR2D&, double ) override { record(); }

    void DrawArc( const VECTOR2D&, double, const EDA_ANGLE&, const EDA_ANGLE& ) override { record(); }

    void DrawSegment( const VECTOR2D&, const VECTOR2D&, double ) override { record(); }

    void DrawArcSegment( const VECTOR2D&, double, const EDA_ANGLE&, const EDA_ANGLE&, double, double ) override
    {
        record();
    }

    void DrawPolyline( const SHAPE_LINE_CHAIN& ) override { record(); }
    void DrawPolyline( const std::deque<VECTOR2D>& ) override { record(); }
    void DrawPolyline( const std::vector<VECTOR2D>& ) override { record(); }
    void DrawPolyline( const VECTOR2D[], int ) override { record(); }
    void DrawPolygon( const SHAPE_POLY_SET&, bool = false ) override { record(); }
    void DrawPolygon( const SHAPE_LINE_CHAIN& ) override { record(); }
    void DrawPolygon( const std::deque<VECTOR2D>& ) override { record(); }
    void DrawPolygon( const VECTOR2D[], int ) override { record(); }

    // Mirrors the real backends, which rewrite the colour of every vertex in the group.
    void ChangeGroupColor( int aGroupNumber, const KIGFX::COLOR4D& aNewColor ) override
    {
        auto it = m_groups.find( aGroupNumber );

        if( it != m_groups.end() )
            it->second.assign( it->second.size(), aNewColor );
    }

    bool HasGroup( const KIGFX::COLOR4D& aColor ) const
    {
        for( const auto& [groupNumber, colors] : m_groups )
        {
            if( std::find( colors.begin(), colors.end(), aColor ) != colors.end() )
                return true;
        }

        return false;
    }

    bool HasGroupWith( const KIGFX::COLOR4D& aFirst, const KIGFX::COLOR4D& aSecond ) const
    {
        for( const auto& [groupNumber, colors] : m_groups )
        {
            if( std::find( colors.begin(), colors.end(), aFirst ) != colors.end()
                && std::find( colors.begin(), colors.end(), aSecond ) != colors.end() )
            {
                return true;
            }
        }

        return false;
    }

private:
    void record()
    {
        if( m_openGroup >= 0 )
            m_groups[m_openGroup].push_back( GetIsFill() ? GetFillColor() : GetStrokeColor() );
    }

    std::map<int, std::vector<KIGFX::COLOR4D>> m_groups;
    int                                        m_nextGroup = 1;
    int                                        m_openGroup = -1;
};

} // namespace


BOOST_AUTO_TEST_SUITE( PcbViaHoleRecolour )


BOOST_AUTO_TEST_CASE( BlindViaHoleKeepsLayerPairColours )
{
    const KIGFX::COLOR4D topColor( 1.0, 0.0, 0.0, 1.0 );
    const KIGFX::COLOR4D bottomColor( 0.0, 1.0, 0.0, 1.0 );

    KIGFX::GAL_DISPLAY_OPTIONS options;
    RECORDING_GAL              gal( options );

    // view must outlive board so board items unregister from a live view at teardown.
    KIGFX::PCB_VIEW    view;
    KIGFX::PCB_PAINTER painter( &gal, FRAME_PCB_EDITOR );
    BOARD              board;

    KIGFX::PCB_RENDER_SETTINGS* settings = painter.GetSettings();
    settings->SetLayerColor( F_Cu, topColor );
    settings->SetLayerColor( In1_Cu, bottomColor );
    settings->SetLayerColor( LAYER_VIA_HOLES, KIGFX::COLOR4D( 1.0, 0.8, 0.0, 1.0 ) );

    view.SetGAL( &gal );
    view.SetPainter( &painter );

    board.SetCopperLayerCount( 4 );

    PCB_VIA* via = new PCB_VIA( &board );
    via->SetPadstackMode( PADSTACK::MODE::NORMAL );
    via->SetViaType( VIATYPE::BLIND );
    via->SetLayerPair( F_Cu, In1_Cu );
    via->SetStart( { 0, 0 } );
    via->SetEnd( { 0, 0 } );
    via->SetWidth( PADSTACK::ALL_LAYERS, pcbIUScale.mmToIU( 0.8 ) );
    via->SetDrill( pcbIUScale.mmToIU( 0.4 ) );
    board.Add( via );

    view.Add( via );
    view.UpdateItems();

    BOOST_REQUIRE_MESSAGE( gal.HasGroupWith( topColor, bottomColor ),
                           "painter did not emit a two colour via hole to begin with" );

    view.UpdateAllLayersColor();

    BOOST_CHECK_MESSAGE( gal.HasGroupWith( topColor, bottomColor ),
                         "recolouring flattened the blind via hole to a single colour" );
}


BOOST_AUTO_TEST_CASE( BackdrilledViaKeepsIndicatorColours )
{
    const KIGFX::COLOR4D startColor( 1.0, 0.0, 0.0, 1.0 );
    const KIGFX::COLOR4D endColor( 0.0, 1.0, 0.0, 1.0 );

    KIGFX::GAL_DISPLAY_OPTIONS options;
    RECORDING_GAL              gal( options );

    // view must outlive board so board items unregister from a live view at teardown.
    KIGFX::PCB_VIEW    view;
    KIGFX::PCB_PAINTER painter( &gal, FRAME_PCB_EDITOR );
    BOARD              board;

    KIGFX::PCB_RENDER_SETTINGS* settings = painter.GetSettings();
    settings->SetLayerColor( F_Cu, startColor );
    settings->SetLayerColor( In1_Cu, endColor );
    settings->SetLayerColor( LAYER_VIA_HOLEWALLS, KIGFX::COLOR4D( 1.0, 0.8, 0.0, 1.0 ) );

    view.SetGAL( &gal );
    view.SetPainter( &painter );

    board.SetCopperLayerCount( 4 );

    PCB_VIA* via = new PCB_VIA( &board );
    via->SetPadstackMode( PADSTACK::MODE::NORMAL );
    via->SetLayerPair( F_Cu, B_Cu );
    via->SetStart( { 0, 0 } );
    via->SetEnd( { 0, 0 } );
    via->SetWidth( PADSTACK::ALL_LAYERS, pcbIUScale.mmToIU( 0.8 ) );
    via->SetDrill( pcbIUScale.mmToIU( 0.4 ) );
    via->SetSecondaryDrillSize( std::optional<int>( pcbIUScale.mmToIU( 0.6 ) ) );
    via->SetSecondaryDrillStartLayer( F_Cu );
    via->SetSecondaryDrillEndLayer( In1_Cu );
    board.Add( via );

    view.Add( via );
    view.UpdateItems();

    BOOST_REQUIRE_MESSAGE( gal.HasGroupWith( startColor, endColor ),
                           "painter did not emit a two colour backdrill indicator to begin with" );

    view.UpdateAllLayersColor();

    BOOST_CHECK_MESSAGE( gal.HasGroupWith( startColor, endColor ),
                         "recolouring flattened the backdrill indicator to a single colour" );
}


BOOST_AUTO_TEST_CASE( BackdrilledPadKeepsIndicatorColours )
{
    const KIGFX::COLOR4D startColor( 1.0, 0.0, 0.0, 1.0 );
    const KIGFX::COLOR4D endColor( 0.0, 1.0, 0.0, 1.0 );

    KIGFX::GAL_DISPLAY_OPTIONS options;
    RECORDING_GAL              gal( options );

    // view must outlive board so board items unregister from a live view at teardown.
    KIGFX::PCB_VIEW    view;
    KIGFX::PCB_PAINTER painter( &gal, FRAME_PCB_EDITOR );
    BOARD              board;

    KIGFX::PCB_RENDER_SETTINGS* settings = painter.GetSettings();
    settings->SetLayerColor( F_Cu, startColor );
    settings->SetLayerColor( In1_Cu, endColor );
    settings->SetLayerColor( LAYER_PAD_HOLEWALLS, KIGFX::COLOR4D( 1.0, 0.8, 0.0, 1.0 ) );

    view.SetGAL( &gal );
    view.SetPainter( &painter );

    board.SetCopperLayerCount( 4 );

    FOOTPRINT* footprint = new FOOTPRINT( &board );
    board.Add( footprint );

    PAD* pad = new PAD( footprint );
    pad->SetAttribute( PAD_ATTRIB::PTH );
    pad->SetPosition( { 0, 0 } );
    pad->SetSizeX( pcbIUScale.mmToIU( 1.6 ) );
    pad->SetSizeY( pcbIUScale.mmToIU( 1.6 ) );
    pad->SetDrillSizeX( pcbIUScale.mmToIU( 0.8 ) );
    pad->SetDrillSizeY( pcbIUScale.mmToIU( 0.8 ) );
    pad->SetSecondaryDrillSizeX( pcbIUScale.mmToIU( 1.2 ) );
    pad->SetSecondaryDrillSizeY( pcbIUScale.mmToIU( 1.2 ) );
    pad->SetSecondaryDrillStartLayer( F_Cu );
    pad->SetSecondaryDrillEndLayer( In1_Cu );
    footprint->Add( pad );

    view.Add( pad );
    view.UpdateItems();

    BOOST_REQUIRE_MESSAGE( gal.HasGroupWith( startColor, endColor ),
                           "painter did not emit a two colour pad backdrill to begin with" );

    view.UpdateAllLayersColor();

    BOOST_CHECK_MESSAGE( gal.HasGroupWith( startColor, endColor ),
                         "recolouring flattened the pad backdrill indicator to a single colour" );
}


BOOST_AUTO_TEST_CASE( TestVmeWrenBoardRendering )
{
    KIGFX::GAL_DISPLAY_OPTIONS options;
    RECORDING_GAL              gal( options );
    KIGFX::PCB_VIEW            view;
    KIGFX::PCB_PAINTER         painter( &gal, FRAME_PCB_EDITOR );

    SETTINGS_MANAGER       settingsManager;
    std::unique_ptr<BOARD> board = std::make_unique<BOARD>();
    KI_TEST::LoadBoard( settingsManager, "vme-wren", board );
    BOOST_REQUIRE( board );

    view.SetGAL( &gal );
    view.SetPainter( &painter );

    painter.GetSettings()->SetNetColorMode( NET_COLOR_MODE::ALL );
    painter.GetSettings()->SetNetColorStyle( NET_COLOR_STYLE::OUTLINE_HALO );

    for( BOARD_ITEM* item : board->AllConnectedItems() )
        view.Add( static_cast<KIGFX::VIEW_ITEM*>( item ) );

    for( ZONE* zone : board->Zones() )
        view.Add( static_cast<KIGFX::VIEW_ITEM*>( zone ) );

    view.UpdateItems();
    view.UpdateAllLayersColor();
}


BOOST_AUTO_TEST_CASE( TestArcPaintingColors )
{
    KIGFX::GAL_DISPLAY_OPTIONS options;
    RECORDING_GAL              gal( options );
    KIGFX::PCB_VIEW            view;
    KIGFX::PCB_PAINTER         painter( &gal, FRAME_PCB_EDITOR );

    view.SetGAL( &gal );
    view.SetPainter( &painter );

    const KIGFX::COLOR4D fcuLayerColor( 0.8, 0.2, 0.2, 1.0 );
    const KIGFX::COLOR4D netCustomColor( 0.2, 0.9, 0.3, 1.0 );

    painter.GetSettings()->SetLayerColor( F_Cu, fcuLayerColor );
    painter.GetSettings()->GetNetColorMap()[1] = netCustomColor;
    painter.GetSettings()->SetNetColorMode( NET_COLOR_MODE::ALL );
    painter.GetSettings()->SetNetColorStyle( NET_COLOR_STYLE::SPINE );

    BOARD         board;
    NETINFO_ITEM* netinfo = new NETINFO_ITEM( &board, "TEST_NET", 1 );
    board.Add( netinfo );

    PCB_ARC* arc = new PCB_ARC( &board );
    arc->SetLayer( F_Cu );
    arc->SetNet( netinfo );
    arc->SetStart( { 0, 0 } );
    arc->SetMid( { 1000000, 1000000 } );
    arc->SetEnd( { 2000000, 0 } );
    arc->SetWidth( 100000 );
    board.Add( arc );

    view.Add( arc );
    view.UpdateItems();

    BOOST_REQUIRE_MESSAGE( gal.HasGroup( fcuLayerColor ) && gal.HasGroup( netCustomColor ),
                           "Painter should emit both layer color and net color passes for arc spine" );

    view.UpdateAllLayersColor();

    BOOST_CHECK_MESSAGE( gal.HasGroup( fcuLayerColor ) && gal.HasGroup( netCustomColor ),
                         "Recolouring must preserve two-tone colors on arc spine passes without flattening" );
}


BOOST_AUTO_TEST_CASE( TestTrackPaintingColors )
{
    KIGFX::GAL_DISPLAY_OPTIONS options;
    RECORDING_GAL              gal( options );
    KIGFX::PCB_VIEW            view;
    KIGFX::PCB_PAINTER         painter( &gal, FRAME_PCB_EDITOR );

    view.SetGAL( &gal );
    view.SetPainter( &painter );

    const KIGFX::COLOR4D fcuLayerColor( 0.8, 0.2, 0.2, 1.0 );
    const KIGFX::COLOR4D netCustomColor( 0.2, 0.9, 0.3, 1.0 );

    painter.GetSettings()->SetLayerColor( F_Cu, fcuLayerColor );
    painter.GetSettings()->GetNetColorMap()[1] = netCustomColor;
    painter.GetSettings()->SetNetColorMode( NET_COLOR_MODE::ALL );
    painter.GetSettings()->SetNetColorStyle( NET_COLOR_STYLE::OUTLINE_HALO );

    BOARD         board;
    NETINFO_ITEM* netinfo = new NETINFO_ITEM( &board, "TEST_NET", 1 );
    board.Add( netinfo );

    PCB_TRACK* track = new PCB_TRACK( &board );
    track->SetLayer( F_Cu );
    track->SetNet( netinfo );
    track->SetStart( { 0, 0 } );
    track->SetEnd( { 2000000, 0 } );
    track->SetWidth( 100000 );
    board.Add( track );

    view.Add( track );
    view.UpdateItems();

    BOOST_REQUIRE_MESSAGE( gal.HasGroup( fcuLayerColor ) && gal.HasGroup( netCustomColor ),
                           "Painter should emit both layer color and net color passes for track outline halo" );

    view.UpdateAllLayersColor();

    BOOST_CHECK_MESSAGE( gal.HasGroup( fcuLayerColor ) && gal.HasGroup( netCustomColor ),
                         "Recolouring must preserve two-tone colors on track halo passes without flattening" );
}


BOOST_AUTO_TEST_CASE( TestPadPaintingColors )
{
    KIGFX::GAL_DISPLAY_OPTIONS options;
    RECORDING_GAL              gal( options );
    KIGFX::PCB_VIEW            view;
    KIGFX::PCB_PAINTER         painter( &gal, FRAME_PCB_EDITOR );

    view.SetGAL( &gal );
    view.SetPainter( &painter );

    const KIGFX::COLOR4D fcuLayerColor( 0.8, 0.2, 0.2, 1.0 );
    const KIGFX::COLOR4D netCustomColor( 0.2, 0.9, 0.3, 1.0 );

    painter.GetSettings()->SetLayerColor( F_Cu, fcuLayerColor );
    painter.GetSettings()->GetNetColorMap()[1] = netCustomColor;
    painter.GetSettings()->SetNetColorMode( NET_COLOR_MODE::ALL );
    painter.GetSettings()->SetNetColorStyle( NET_COLOR_STYLE::OUTLINE_HALO );

    BOARD         board;
    NETINFO_ITEM* netinfo = new NETINFO_ITEM( &board, "TEST_NET", 1 );
    board.Add( netinfo );

    FOOTPRINT* footprint = new FOOTPRINT( &board );
    board.Add( footprint );

    PAD* pad = new PAD( footprint );
    pad->SetLayerSet( LSET( { F_Cu } ) );
    pad->SetNet( netinfo );
    pad->SetPosition( { 0, 0 } );
    pad->SetSizeX( pcbIUScale.mmToIU( 1.6 ) );
    pad->SetSizeY( pcbIUScale.mmToIU( 1.6 ) );
    footprint->Add( pad );

    view.Add( pad );
    view.UpdateItems();

    BOOST_REQUIRE_MESSAGE( gal.HasGroup( fcuLayerColor ) && gal.HasGroup( netCustomColor ),
                           "Painter should emit both layer color and net color passes for pad outline halo" );

    view.UpdateAllLayersColor();

    BOOST_CHECK_MESSAGE( gal.HasGroup( fcuLayerColor ) && gal.HasGroup( netCustomColor ),
                         "Recolouring must preserve two-tone colors on pad halo passes without flattening" );
}


BOOST_AUTO_TEST_CASE( TestPadPaintingColorsSpine )
{
    KIGFX::GAL_DISPLAY_OPTIONS options;
    RECORDING_GAL              gal( options );
    KIGFX::PCB_VIEW            view;
    KIGFX::PCB_PAINTER         painter( &gal, FRAME_PCB_EDITOR );

    view.SetGAL( &gal );
    view.SetPainter( &painter );

    const KIGFX::COLOR4D fcuLayerColor( 0.8, 0.2, 0.2, 1.0 );
    const KIGFX::COLOR4D netCustomColor( 0.2, 0.9, 0.3, 1.0 );

    painter.GetSettings()->SetLayerColor( F_Cu, fcuLayerColor );
    painter.GetSettings()->GetNetColorMap()[1] = netCustomColor;
    painter.GetSettings()->SetNetColorMode( NET_COLOR_MODE::ALL );
    painter.GetSettings()->SetNetColorStyle( NET_COLOR_STYLE::SPINE );

    BOARD         board;
    NETINFO_ITEM* netinfo = new NETINFO_ITEM( &board, "TEST_NET", 1 );
    board.Add( netinfo );

    FOOTPRINT* footprint = new FOOTPRINT( &board );
    board.Add( footprint );

    PAD* pad = new PAD( footprint );
    pad->SetLayerSet( LSET( { F_Cu } ) );
    pad->SetNet( netinfo );
    pad->SetPosition( { 0, 0 } );
    pad->SetSizeX( pcbIUScale.mmToIU( 1.6 ) );
    pad->SetSizeY( pcbIUScale.mmToIU( 1.6 ) );
    footprint->Add( pad );

    view.Add( pad );
    view.UpdateItems();

    BOOST_REQUIRE_MESSAGE( gal.HasGroup( fcuLayerColor ) && gal.HasGroup( netCustomColor ),
                           "Painter should emit both layer color and net color passes for pad spine" );

    view.UpdateAllLayersColor();

    BOOST_CHECK_MESSAGE( gal.HasGroup( fcuLayerColor ) && gal.HasGroup( netCustomColor ),
                         "Recolouring must preserve two-tone colors on pad spine passes without flattening" );
}


BOOST_AUTO_TEST_CASE( TestZonePaintingColors )
{
    KIGFX::GAL_DISPLAY_OPTIONS options;
    RECORDING_GAL              gal( options );
    KIGFX::PCB_VIEW            view;
    KIGFX::PCB_PAINTER         painter( &gal, FRAME_PCB_EDITOR );

    view.SetGAL( &gal );
    view.SetPainter( &painter );

    const KIGFX::COLOR4D fcuLayerColor( 0.8, 0.2, 0.2, 1.0 );
    const KIGFX::COLOR4D netCustomColor( 0.2, 0.9, 0.3, 1.0 );

    painter.GetSettings()->SetLayerColor( F_Cu, fcuLayerColor );
    painter.GetSettings()->GetNetColorMap()[1] = netCustomColor;
    painter.GetSettings()->SetNetColorMode( NET_COLOR_MODE::ALL );
    painter.GetSettings()->SetNetColorStyle( NET_COLOR_STYLE::OUTLINE_HALO );

    BOARD         board;
    NETINFO_ITEM* netinfo = new NETINFO_ITEM( &board, "TEST_NET", 1 );
    board.Add( netinfo );

    ZONE* zone = new ZONE( &board );
    zone->SetLayerSet( LSET( { F_Cu } ) );
    zone->SetNet( netinfo );

    SHAPE_POLY_SET poly;
    poly.NewOutline();
    poly.Append( 0, 0 );
    poly.Append( 2000000, 0 );
    poly.Append( 2000000, 2000000 );
    poly.Append( 0, 2000000 );
    zone->SetFilledPolysList( F_Cu, poly );
    board.Add( zone );

    view.Add( zone );
    view.UpdateItems();

    BOOST_REQUIRE_MESSAGE( gal.HasGroup( fcuLayerColor ) && gal.HasGroup( netCustomColor ),
                           "Painter should emit both layer color and net color passes for zone halo" );

    view.UpdateAllLayersColor();

    BOOST_CHECK_MESSAGE( gal.HasGroup( fcuLayerColor ) && gal.HasGroup( netCustomColor ),
                         "Recolouring must preserve two-tone colors on zone halo passes without flattening" );
}


BOOST_AUTO_TEST_SUITE_END()