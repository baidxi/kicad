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

#include <pcb_via_stitch.h>

#include <cmath>

#include <gal/graphics_abstraction_layer.h>
#include <gal/painter.h>
#include <tool/action_menu.h>
#include <tools/pcb_actions.h>
#include <view/view.h>
#include <zone.h>
#include <pad.h>

#include <pcb_track.h>
#include <board.h>
#include <board_design_settings.h>
#include <drc/drc_engine.h>
#include <drc/drc_rtree.h>
#include <drc/drc_rule.h>
#include <properties/property.h>
#include <properties/property_mgr.h>
#include <drc/drc_cache_generator.h>

const wxString PCB_VIA_STITCH::DISPLAY_NAME = _HKI( "Via Stitching" );
const wxString PCB_VIA_STITCH::GENERATOR_TYPE = wxS( "via_stitch" );

PCB_VIA_STITCH::PCB_VIA_STITCH( BOARD_ITEM* aParent, PCB_LAYER_ID aLayer ) :
        PCB_GENERATOR_POLY( aParent, aLayer ),
        m_viaTemplate( std::make_unique<PCB_VIA>( this ) )
{
    m_generatorType = GENERATOR_TYPE;
    m_name = DISPLAY_NAME;


    m_viaTemplate->SetIsFree( true );
}


PCB_VIA_STITCH::PCB_VIA_STITCH( const PCB_VIA_STITCH& aOther ) :
        PCB_GENERATOR_POLY( aOther ),
        m_pitch( aOther.m_pitch ),
        m_stagger( aOther.m_stagger ),
        m_viaTemplate( std::make_unique<PCB_VIA>( *aOther.m_viaTemplate ) ),
        m_lastNetName( aOther.m_lastNetName ),
        m_netCode( aOther.m_netCode ),
        m_excludedCells( aOther.m_excludedCells ),
        m_originOffset( aOther.m_originOffset )
{
    // m_childContextMenu is intentionally not copied — each instance lazy-builds its own.
}


PCB_VIA_STITCH::PCB_VIA_STITCH( PCB_VIA_STITCH&& aOther ) noexcept = default;

PCB_VIA_STITCH& PCB_VIA_STITCH::operator=( const PCB_VIA_STITCH& aOther )
{
    if( this != &aOther )
    {
        PCB_GENERATOR_POLY::operator=( aOther );
        m_pitch = aOther.m_pitch;
        m_stagger = aOther.m_stagger;
        m_viaTemplate = std::make_unique<PCB_VIA>( *aOther.m_viaTemplate );
        m_lastNetName = aOther.m_lastNetName;
        m_netCode = aOther.m_netCode;
        m_excludedCells = aOther.m_excludedCells;
        m_originOffset = aOther.m_originOffset;
        // m_childContextMenu is intentionally not copied.
        m_childContextMenu.reset();
    }
    return *this;
}

PCB_VIA_STITCH& PCB_VIA_STITCH::operator=( PCB_VIA_STITCH&& aOther ) noexcept = default;

PCB_VIA_STITCH::~PCB_VIA_STITCH() = default;


int PCB_VIA_STITCH::GetViaSize() const
{
    return m_viaTemplate->GetWidth();
}


void PCB_VIA_STITCH::SetViaSize( int aVal )
{
    m_viaTemplate->SetWidth( aVal );
}


int PCB_VIA_STITCH::GetViaDrill() const
{
    int d = m_viaTemplate->GetDrillValue();
    return d > 0 ? d : 0;
}


void PCB_VIA_STITCH::SetViaDrill( int aVal )
{
    m_viaTemplate->SetDrill( aVal );
}

void PCB_VIA_STITCH::ViewDraw( int aLayer, KIGFX::VIEW* aView ) const
{
    if( m_outline.OutlineCount() == 0 )
        return;

    KIGFX::GAL* gal = aView->GetGAL();

    // We are drawing perpendicular ticks along the outline to indicate the stitch zone
    // It kind of looks like a sewing pattern with the ticks, so it fits.

    // Teal color, something unique for now
    const KIGFX::COLOR4D borderColor( 0.3, 0.8, 0.7, 1.0 );

    const SHAPE_LINE_CHAIN& outline = m_outline.COutline( 0 );
    const int               n       = outline.PointCount();

    gal->SetIsFill( false );
    gal->SetIsStroke( true );
    gal->SetStrokeColor( borderColor );
    gal->SetLineWidth( pcbIUScale.mmToIU( 0.1 ) );
    gal->DrawPolyline( outline );

    const double tickSpacing = pcbIUScale.mmToIU( 2.0 );
    const double tickLength  = pcbIUScale.mmToIU( 0.5 );

    // Approximate centroid — used to resolve which perpendicular direction is inward
    // regardless of polygon winding order.
    VECTOR2D centroid( 0.0, 0.0 );
    for( int i = 0; i < n; ++i )
    {
        centroid.x += outline.CPoint( i ).x;
        centroid.y += outline.CPoint( i ).y;
    }
    centroid.x /= n;
    centroid.y /= n;

    gal->SetLineWidth( 1 ); // hairline ticks

    for( int i = 0; i < n; ++i )
    {
        const VECTOR2D p1( outline.CPoint( i ) );
        const VECTOR2D p2( outline.CPoint( ( i + 1 ) % n ) );

        const VECTOR2D edgeVec = p2 - p1;
        const double   edgeLen = edgeVec.EuclideanNorm();

        if( edgeLen < 1.0 )
            continue;

        const VECTOR2D edgeDir = edgeVec / edgeLen;

        // Pick the perpendicular that points toward the centroid
        const VECTOR2D edgeMid  = ( p1 + p2 ) / 2.0;
        const VECTOR2D perp     = VECTOR2D( -edgeDir.y, edgeDir.x );
        const VECTOR2D toCentr  = centroid - edgeMid;
        const bool     perpToward = ( perp.x * toCentr.x + perp.y * toCentr.y >= 0.0 );
        const VECTOR2D inward     = perpToward ? perp : VECTOR2D( -perp.x, -perp.y );

        for( double t = tickSpacing; t < edgeLen; t += tickSpacing )
        {
            const VECTOR2D base = p1 + edgeDir * t;
            gal->DrawLine( base, base + inward * tickLength );
        }
    }
}


VECTOR2I PCB_VIA_STITCH::GetPosition() const
{
    SHAPE_POLY_SET::VERTEX_INDEX index;

    // An empty outline has no vertex 0 (e.g. a freshly constructed generator before the
    // outline is set); fall back to the anchor origin rather than throwing.
    if( !m_outline.GetRelativeIndices( 0, &index ) )
        return m_origin;

    return m_outline.CVertex( index );
}


void PCB_VIA_STITCH::EditStart( GENERATOR_TOOL* aTool, BOARD* aBoard, BOARD_COMMIT* aCommit )
{
    if( aCommit )
    {
        if( IsNew() )
            aCommit->Add( this );
        else
            aCommit->Modify( this );
    }

    // Initialize property defaults on first edit
    if( m_viaTemplate->GetWidth() <= 0 )
        m_viaTemplate->SetWidth( defaultViaSize( aBoard ) );
    if( m_viaTemplate->GetDrillValue() <= 0 )
        m_viaTemplate->SetDrill( defaultViaDrill( aBoard ) );
    if( m_pitch <= 0 )
        m_pitch = defaultPitch( aBoard );

    // Resolve net code from saved name (SetProperties has no board reference)
    if( m_netCode == 0 && !m_lastNetName.empty() && aBoard )
    {
        if( NETINFO_ITEM* net = aBoard->FindNet( m_lastNetName ) )
            m_netCode = net->GetNetCode();
    }

    SetFlags( IN_EDIT );
}


int PCB_VIA_STITCH::defaultPitch( BOARD* aBoard ) const
{
    int mm2 = pcbIUScale.mmToIU( 2.0 );
    int viaSize = defaultViaSize( aBoard );
    int pitch = std::max( mm2, viaSize * 2 );
    return pitch;
}


int PCB_VIA_STITCH::defaultViaSize( BOARD* aBoard ) const
{
    if( !aBoard )
        return pcbIUScale.mmToIU( 0.6 );

    BOARD_DESIGN_SETTINGS& ds = aBoard->GetDesignSettings();
    int val = ds.GetCurrentViaSize();
    if( val <= 0 )
        val = pcbIUScale.mmToIU( 0.6 );
    return val;
}

int PCB_VIA_STITCH::defaultViaDrill( BOARD* aBoard ) const
{
    if( !aBoard )
        return pcbIUScale.mmToIU( 0.3 );

    BOARD_DESIGN_SETTINGS& ds = aBoard->GetDesignSettings();
    int val = ds.GetCurrentViaDrill();
    if( val <= 0 )
        val = pcbIUScale.mmToIU( 0.3 );
    return val;
}


/**
 * Returns true if @a aVia violates the required copper clearance against any existing board
 * item (ignoring same-net connected items).  The via must NOT yet have been added to the
 * board so that the R-tree query only sees pre-existing items.
 *
 * @param aBoard Board to test against
 * @param aDRCEngine Used to retrieve DRC clearance constraints for the via vs. other items.
 * @param aVia The via to test for clearance violations
 */
static bool collidesWithBoard( BOARD* aBoard, DRC_ENGINE* aDRCEngine, PCB_VIA* aVia )
{
    LSET boardCopperLayers = LSET::AllCuMask( aBoard->GetCopperLayerCount() );

    for( PCB_LAYER_ID layer : LSET( aVia->GetLayerSet() & boardCopperLayers ) )
    {
        std::shared_ptr<SHAPE> viaShape = aVia->GetEffectiveShape( layer );
        bool                   violationFound = false;

        aBoard->m_CopperItemRTreeCache->QueryColliding(
                aVia, layer, layer,
                // Filter out same net collisions, generally ok except for collisions with vias
                // TODO: Probably some other types we dont want to collide with
                [&]( BOARD_ITEM* other ) -> bool
                {
                    if( other->Type() == PCB_VIA_T && other->GetParent() != aVia->GetParent() )
                        return true;

                    if( BOARD_CONNECTED_ITEM* otherCItem =
                                dynamic_cast<BOARD_CONNECTED_ITEM*>( other ) )
                    {
                        if( otherCItem->GetNetCode() == aVia->GetNetCode() )
                            return false;
                    }
                    return true;
                },
                // Visitor: evaluate the actual required clearance for this pair, then
                // check whether the shapes are within that distance of each other.
                // (QueryColliding already pre-filtered to items within m_DRCMaxClearance.)
                [&]( BOARD_ITEM* other ) -> bool
                {
                    int requiredClearance = 0;

                    if( aDRCEngine )
                    {
                        DRC_CONSTRAINT constraint = aDRCEngine->EvalRules(
                                CLEARANCE_CONSTRAINT, aVia, other, layer );
                        requiredClearance = constraint.GetValue().Min();
                    }

                    std::shared_ptr<SHAPE> otherShape = other->GetEffectiveShape( layer );

                    if( viaShape->Collide( otherShape.get(), requiredClearance ) )
                    {
                        violationFound = true;
                        return false; // stop on first violation
                    }

                    return true;
                },
                aBoard->m_DRCMaxClearance );

        if( violationFound )
            return true;

        // Zones are stored in a separate per-zone R-tree (m_CopperItemRTreeCache does not
        // contain them).  Check the via against every different-net zone's filled polygon.
        BOX2I viaBBox = aVia->GetBoundingBox();

        for( auto& [zone, zoneTree] : aBoard->m_CopperZoneRTreeCache )
        {
            if( zone->GetNetCode() == aVia->GetNetCode() )
                continue; // same net — no clearance required

            if( !zone->IsOnLayer( layer ) )
                continue;

            int requiredClearance = 0;

            if( aDRCEngine )
            {
                DRC_CONSTRAINT constraint = aDRCEngine->EvalRules(
                        CLEARANCE_CONSTRAINT, aVia, zone, layer );
                requiredClearance = constraint.GetValue().Min();
            }

            int      actual;
            VECTOR2I pos;

            if( zoneTree->QueryColliding( viaBBox, viaShape.get(), layer,
                                          requiredClearance, &actual, &pos ) )
            {
                return true;
            }
        }
    }

    return false;
}


bool PCB_VIA_STITCH::Update( GENERATOR_TOOL* aTool, BOARD* aBoard, BOARD_COMMIT* aCommit )
{
    if( !( GetFlags() & IN_EDIT ) )
        return false;

    if( !aBoard || !aCommit )
        return false;

    // Resolve net code from the saved name in case EditStart() was not called
    // (e.g. programmatic regeneration after load)
    if( m_netCode == 0 && !m_lastNetName.empty() )
    {
        if( NETINFO_ITEM* net = aBoard->FindNet( m_lastNetName ) )
            m_netCode = net->GetNetCode();
    }

    int detectPitch = m_pitch > 0 ? m_pitch : defaultPitch( aBoard );

    // Try to detect origin offset change due to moved via
    for( BOARD_ITEM* it : GetBoardItems() )
    {
        if( it->Type() != PCB_VIA_T )
            continue;

        VECTOR2I p = static_cast<PCB_VIA*>( it )->GetPosition();
        VECTOR2I expectedPos = positionForCell( cellForPosition( p ), detectPitch );

        if( expectedPos != p )
        {
            // Recalculate the origin offset but account for stagger possibly being enabled
            m_originOffset = VECTOR2I( ( ( p.x % detectPitch ) + detectPitch ) % detectPitch,
                                       ( ( p.y % detectPitch ) + detectPitch ) % detectPitch );
            break;
        }
    }

    for( BOARD_ITEM* item : GetBoardItems() )
    {
        if( item->Type() == PCB_VIA_T )
            aCommit->Remove( item );
    }

    // Remove from group container
    std::vector<BOARD_ITEM*> toRemove;
    for( BOARD_ITEM* item : GetBoardItems() )
        if( item->Type() == PCB_VIA_T )
            toRemove.push_back( item );

    for( BOARD_ITEM* item : toRemove )
        RemoveItem( item );

    if( m_outline.IsEmpty() )
        return false;

    int pitch = m_pitch > 0 ? m_pitch : defaultPitch( aBoard );

    if( m_viaTemplate->GetWidth() <= 0 )
        m_viaTemplate->SetWidth( defaultViaSize( aBoard ) );
    if( m_viaTemplate->GetDrillValue() <= 0 )
        m_viaTemplate->SetDrill( defaultViaDrill( aBoard ) );

    // Compute the set of grid cells where a via fits cleanly (DRC + same-net zone overlap).
    // TODO, we will use a cache grid
    std::set<VECTOR2I> placementCells = buildPlacementCells( aBoard );

    for( const VECTOR2I& cell : placementCells )
    {
        if( m_excludedCells.count( cell ) )
            continue;

        VECTOR2I pt = positionForCell( cell, pitch );

        PCB_VIA* via = static_cast<PCB_VIA*>( m_viaTemplate->Clone() );
        via->SetParent( aBoard );
        via->SetPosition( pt );

        if( m_netCode )
            via->SetNetCode( m_netCode );

        // Vias must be marked free to avoid connectivity stealing them away
        via->SetIsFree( true );

        AddItem( via );
        aCommit->Add( via );
    }

    return true;
}


const STRING_ANY_MAP PCB_VIA_STITCH::GetProperties() const
{
    STRING_ANY_MAP props = PCB_GENERATOR::GetProperties();
    props.set_iu( "via_size", m_viaTemplate->GetWidth() );
    props.set_iu( "via_drill", m_viaTemplate->GetDrillValue() );
    props.set_iu( "pitch", m_pitch );
    props.set( "stagger", m_stagger );
    props.set_iu( "origin_offset_x", m_originOffset.x );
    props.set_iu( "origin_offset_y", m_originOffset.y );

    // Save the net by name so it survives across sessions (net codes are session-local)
    if( !m_lastNetName.empty() )
        props.set( "net_name", m_lastNetName );

    if( !m_excludedCells.empty() )
    {
        SHAPE_LINE_CHAIN chain;
        for( const VECTOR2I& cell : m_excludedCells )
            chain.Append( cell );
        props.set( "excluded_cells", wxAny( chain ) );
    }

    // Serialize the single zone outline as a SHAPE_LINE_CHAIN.
    // The file format natively supports SHAPE_LINE_CHAIN but not SHAPE_POLY_SET directly.
    if( m_outline.OutlineCount() > 0 )
        props.set( "outline", wxAny( m_outline.COutline( 0 ) ) );

    return props;
}


void PCB_VIA_STITCH::SetProperties( const STRING_ANY_MAP& aProps )
{
    PCB_GENERATOR::SetProperties( aProps );

    int viaSize = 0;
    int viaDrill = 0;
    aProps.get_to_iu( "via_size", viaSize );
    aProps.get_to_iu( "via_drill", viaDrill );

    if( viaSize > 0 )
        m_viaTemplate->SetWidth( viaSize );
    if( viaDrill > 0 )
        m_viaTemplate->SetDrill( viaDrill );

    aProps.get_to_iu( "pitch", m_pitch );
    aProps.get_to( "stagger", m_stagger );
    aProps.get_to_iu( "origin_offset_x", m_originOffset.x );
    aProps.get_to_iu( "origin_offset_y", m_originOffset.y );

    // Restore net name; net code is resolved from the board in EditStart()/Update()
    aProps.get_to( "net_name", m_lastNetName );

    m_excludedCells.clear();
    if( auto chain = aProps.get_opt<SHAPE_LINE_CHAIN>( "excluded_cells" ) )
        for( int i = 0; i < chain->PointCount(); ++i )
            m_excludedCells.insert( chain->CPoint( i ) );

    // Restore the single zone outline
    m_outline.RemoveAllContours();

    if( auto outline = aProps.get_opt<SHAPE_LINE_CHAIN>( "outline" ) )
        m_outline.AddOutline( *outline );
}


void PCB_VIA_STITCH::EditFinish( GENERATOR_TOOL* aTool, BOARD* aBoard, BOARD_COMMIT* aCommit )
{
    if( !( GetFlags() & IN_EDIT ) )
        return;

    ClearFlags( IN_EDIT );
}


void PCB_VIA_STITCH::EditCancel( GENERATOR_TOOL* aTool, BOARD* aBoard, BOARD_COMMIT* aCommit )
{
    if( !( GetFlags() & IN_EDIT ) )
        return;

    ClearFlags( IN_EDIT );
}


ACTION_MENU* PCB_VIA_STITCH::GetChildContextMenu( TOOL_INTERACTIVE* aTool ) const
{
    if( !m_childContextMenu )
    {
        m_childContextMenu = std::make_unique<ACTION_MENU>( true, aTool );
        m_childContextMenu->Add( PCB_ACTIONS::excludeStitchVia );
    }

    return m_childContextMenu.get();
}


void PCB_VIA_STITCH::Remove( GENERATOR_TOOL* aTool, BOARD* aBoard, BOARD_COMMIT* aCommit )
{
    if( !aCommit )
        return;

    for( BOARD_ITEM* item : GetBoardItems() )
        aCommit->Remove( item );

    aCommit->Remove( this );
}


VECTOR2I PCB_VIA_STITCH::cellForPosition( const VECTOR2I& aPos ) const
{
    int pitch = m_pitch > 0 ? m_pitch : 1;

    auto roundDiv = []( int a, int b ) -> int
    {
        return ( a >= 0 ) ? ( a + b / 2 ) / b : -( ( -a + b / 2 ) / b );
    };

    int row = roundDiv( aPos.y - m_originOffset.y, pitch );
    int xShift = ( m_stagger && ( ( row % 2 + 2 ) % 2 != 0 ) ) ? pitch / 2 : 0;
    int col = roundDiv( aPos.x - m_originOffset.x - xShift, pitch );

    return VECTOR2I( col, row );
}


VECTOR2I PCB_VIA_STITCH::positionForCell( const VECTOR2I& aCell, int aPitch ) const
{
    int xShift = ( m_stagger && ( ( aCell.y % 2 + 2 ) % 2 != 0 ) ) ? aPitch / 2 : 0;
    return VECTOR2I( aCell.x * aPitch + m_originOffset.x + xShift,
                     aCell.y * aPitch + m_originOffset.y );
}


std::set<VECTOR2I> PCB_VIA_STITCH::buildPlacementCells( BOARD* aBoard ) const
{
    std::set<VECTOR2I> cells;

    if( !aBoard || m_outline.IsEmpty() )
        return cells;

    int pitch = m_pitch > 0 ? m_pitch : defaultPitch( aBoard );
    int viaSize = m_viaTemplate->GetWidth() > 0 ? m_viaTemplate->GetWidth() : defaultViaSize( aBoard );

    DRC_ENGINE* drcEngine = aBoard->GetDesignSettings().m_DRCEngine.get();

    DRC_CACHE_GENERATOR cacheGenerator;
    cacheGenerator.SetDRCEngine( drcEngine );
    cacheGenerator.Run();

    // Build allowed region: outline ∩ union of same-net zones' *filled* polygons across all
    // copper layers.  Using filled polys (not the user-drawn outline) means the result respects
    // Edge.Cuts, keepouts, clearances to other items, and unfilled (un-poured) regions — a
    // via only lands where there's actually same-net copper to connect to.
    //
    // If no net is set, or no same-net zone has any fill, the cell set is empty (matches
    // the existing "do not place vias" early-outs).
    SHAPE_POLY_SET allowedRegion = m_outline;

    if( GetNetCode() == 0 )
        return cells;

    SHAPE_POLY_SET zonesCombined;
    bool           haveFills = false;

    for( ZONE* zone : aBoard->Zones() )
    {
        if( zone->GetIsRuleArea() )
            continue;

        if( zone->GetNetCode() != GetNetCode() )
            continue;

        for( PCB_LAYER_ID layer : zone->GetLayerSet() )
        {
            SHAPE_POLY_SET* fill = zone->GetFill( layer );

            if( fill && !fill->IsEmpty() )
            {
                zonesCombined.BooleanAdd( *fill );
                haveFills = true;
            }
        }
    }

    if( !haveFills )
        return cells;

    SHAPE_POLY_SET tmp = allowedRegion;
    tmp.BooleanIntersection( zonesCombined );

    if( tmp.OutlineCount() == 0 )
        return cells;

    allowedRegion = std::move( tmp );

    BOX2I bbox = allowedRegion.BBox();

    // Anchor the grid to global (0, 0) + m_originOffset.
    int      startRow = (int) std::floor( double( bbox.GetY() - m_originOffset.y ) / pitch );
    int      startCol = (int) std::floor( double( bbox.GetX() - m_originOffset.x ) / pitch );
    VECTOR2I origin( startCol * pitch + m_originOffset.x,
                     startRow * pitch + m_originOffset.y );

    for( int row = startRow, y = origin.y; y <= bbox.GetBottom(); y += pitch, ++row )
    {
        // Odd rows are shifted right by half the pitch when stagger is enabled.
        int xOffset = ( m_stagger && ( ( row % 2 + 2 ) % 2 != 0 ) ) ? pitch / 2 : 0;

        for( int col = startCol, x = origin.x + xOffset; x <= bbox.GetRight();
             x += pitch, ++col )
        {
            VECTOR2I pt( x, y );

            if( !allowedRegion.PointInside( pt, 0 ) )
                continue;

            if( allowedRegion.PointOnEdge( pt, viaSize / 2 ) )
                continue;

            // Now create a probe via to test for placement
            std::unique_ptr<PCB_VIA> probe( static_cast<PCB_VIA*>( m_viaTemplate->Clone() ) );
            probe->SetParent( const_cast<BOARD*>( aBoard ) );
            probe->SetPosition( pt );

            if( m_netCode )
                probe->SetNetCode( m_netCode );

            if( collidesWithBoard( aBoard, drcEngine, probe.get() ) )
                continue;

            cells.insert( VECTOR2I( col, row ) );
        }
    }

    return cells;
}


void PCB_VIA_STITCH::ExcludePosition( const VECTOR2I& aPos )
{
    m_excludedCells.insert( cellForPosition( aPos ) );
}


void PCB_VIA_STITCH::ClearExclusion( const VECTOR2I& aPos )
{
    m_excludedCells.erase( cellForPosition( aPos ) );
}


void PCB_VIA_STITCH::ClearAllExclusions()
{
    m_excludedCells.clear();
}


int PCB_VIA_STITCH::GetNetCode() const
{
    return m_netCode;
}

void PCB_VIA_STITCH::SetNetCode( int aNetCode )
{
    m_netCode = aNetCode;
    if( BOARD* board = GetBoard() )
    {
        if( NETINFO_ITEM* net = board->FindNet( aNetCode ) )
            m_lastNetName = net->GetNetname();
        else
            m_lastNetName.clear();
    }

    for( BOARD_ITEM* item : GetBoardItems() )
        if( BOARD_CONNECTED_ITEM* bci = dynamic_cast<BOARD_CONNECTED_ITEM*>( item ) )
            bci->SetNetCode( aNetCode );
}

static struct PCB_VIA_STITCH_DESC
{
    PCB_VIA_STITCH_DESC()
    {
        PROPERTY_MANAGER& propMgr = PROPERTY_MANAGER::Instance();
        REGISTER_TYPE( PCB_VIA_STITCH );
        propMgr.AddTypeCast( new TYPE_CAST<PCB_VIA_STITCH, PCB_GENERATOR> );
        propMgr.AddTypeCast( new TYPE_CAST<PCB_VIA_STITCH, BOARD_ITEM> );
        propMgr.InheritsAfter( TYPE_HASH( PCB_VIA_STITCH ), TYPE_HASH( PCB_GENERATOR ) );
        propMgr.InheritsAfter( TYPE_HASH( PCB_VIA_STITCH ), TYPE_HASH( BOARD_ITEM ) );


        propMgr.AddProperty( new PROPERTY<PCB_VIA_STITCH, int>( _HKI( "Size" ),
                                     &PCB_VIA_STITCH::SetViaSize, &PCB_VIA_STITCH::GetViaSize,
                                     PROPERTY_DISPLAY::PT_SIZE ) );

        propMgr.AddProperty( new PROPERTY<PCB_VIA_STITCH, int>( _HKI( "Drill" ),
                                     &PCB_VIA_STITCH::SetViaDrill, &PCB_VIA_STITCH::GetViaDrill,
                                     PROPERTY_DISPLAY::PT_SIZE ) );

        propMgr.AddProperty( new PROPERTY<PCB_VIA_STITCH, int>( _HKI( "Pitch" ),
                                     &PCB_VIA_STITCH::SetPitch, &PCB_VIA_STITCH::GetPitch,
                                     PROPERTY_DISPLAY::PT_SIZE ) );

        propMgr.AddProperty( new PROPERTY<PCB_VIA_STITCH, bool>( _HKI( "Stagger" ),
                                     &PCB_VIA_STITCH::SetStagger, &PCB_VIA_STITCH::GetStagger ) );

        propMgr.AddProperty( new PROPERTY_ENUM<PCB_VIA_STITCH, int>( _HKI( "Net" ),
                                     &PCB_VIA_STITCH::SetNetCode, &PCB_VIA_STITCH::GetNetCode, PT_NET ) );

    }
} _PCB_VIA_STITCH_DESC;


static GENERATORS_MGR::REGISTER<PCB_VIA_STITCH> registerMe;