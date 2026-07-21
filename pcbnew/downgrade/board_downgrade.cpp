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

#include <downgrade/board_downgrade.h>

#include <cctype>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <wx/ffile.h>
#include <wx/filefn.h>

#include <base_units.h>
#include <board.h>
#include <board_design_settings.h>
#include <footprint.h>
#include <pad.h>
#include <pcb_shape.h>
#include <pcb_track.h>
#include <pcb_point.h>
#include <zone.h>
#include <zone_settings.h>
#include <netinfo.h>
#include <eda_group.h>
#include <eda_shape.h>
#include <layer_ids.h>
#include <lset.h>
#include <geometry/shape_poly_set.h>
#include <pcb_io/kicad_sexpr/pcb_io_kicad_sexpr.h>
#include <pcb_io/kicad_sexpr/writers/pcb_writer_v9.h>
#include <pcb_io/kicad_sexpr/writers/pcb_writer_v10.h>


// Board format date each feature landed. A rule fires only when the target predates this.
static constexpr int VER_USER_LAYERS = 20241229;
static constexpr int VER_SHAPE_HATCH = 20250222;
static constexpr int VER_VIA_PROTECTION = 20250228;
static constexpr int VER_ZONE_DEFAULTS = 20250302;
static constexpr int VER_JUMPER_PADS = 20250324;
static constexpr int VER_VIA_SPLIT = 20250926;
static constexpr int VER_SKIP_VIA = 20250801;
static constexpr int VER_PRESSFIT = 20250811;
static constexpr int VER_POST_MACHINING = 20251101;
static constexpr int VER_PCB_POINTS = 20250901;
static constexpr int VER_FP_UNITS = 20250909;
static constexpr int VER_BARCODE = 20250914;
static constexpr int VER_PAD_DIE_DELAY = 20251027;
static constexpr int VER_BACKDRILL = 20251101;
static constexpr int VER_PCB_VARIANTS = 20260101;
static constexpr int VER_EXTRUDED_BODY = 20260410;
static constexpr int VER_ELLIPSE = 20260508;
static constexpr int VER_THIEVING = 20260513;
static constexpr int VER_NET_CHAINS = 20260512;
static constexpr int VER_PAD_SIM = 20260521;
static constexpr int VER_DIELECTRIC = 20260511;
static constexpr int VER_GROUP_LIB_ID = 20250513;

// The board format this feature was built for. If SEXPR_BOARD_FILE_VERSION moves past this, a new
// feature landed and the rules or denylist may be stale. Review pcb.keywords, then update this.
static constexpr int BOARD_DOWNGRADE_COVERED = 20260623;


static bool isEllipse( const BOARD_ITEM* aItem )
{
    if( aItem->Type() != PCB_SHAPE_T )
        return false;

    SHAPE_T shape = static_cast<const PCB_SHAPE*>( aItem )->GetShape();
    return shape == SHAPE_T::ELLIPSE || shape == SHAPE_T::ELLIPSE_ARC;
}


static int countNetChains( const BOARD* aBoard )
{
    int count = 0;

    // Match what the writer emits: a named chain, or a net with terminal pads but no chain name.
    for( NETINFO_ITEM* net : aBoard->GetNetInfo() )
    {
        if( net && ( !net->GetNetChain().IsEmpty() || net->GetTerminalPad( 0 ) || net->GetTerminalPad( 1 ) ) )
        {
            count++;
        }
    }

    return count;
}


static void dropNetChains( BOARD* aBoard )
{
    for( NETINFO_ITEM* net : aBoard->GetNetInfo() )
    {
        net->SetNetChain( wxEmptyString );
        net->ClearTerminalPad( 0 ); // terminal_pad is part of the net-chain feature
        net->ClearTerminalPad( 1 );
    }

    std::vector<wxString> chains;

    for( const auto& [chain, color] : aBoard->GetNetChainColors() )
        chains.push_back( chain );

    for( const wxString& chain : chains )
        aBoard->SetNetChainColor( chain, KIGFX::COLOR4D::UNSPECIFIED );
}


static int countEllipses( const BOARD* aBoard )
{
    int count = 0;

    for( BOARD_ITEM* item : aBoard->Drawings() )
    {
        if( isEllipse( item ) )
            count++;
    }

    for( FOOTPRINT* fp : aBoard->Footprints() )
    {
        for( BOARD_ITEM* item : fp->GraphicalItems() )
        {
            if( isEllipse( item ) )
                count++;
        }
    }

    return count;
}


static void lowerEllipses( BOARD* aBoard )
{
    int maxError = aBoard->GetDesignSettings().m_MaxError;

    auto convert = [&]( BOARD_ITEM* aItem )
    {
        if( !isEllipse( aItem ) )
            return;

        PCB_SHAPE*     shape = static_cast<PCB_SHAPE*>( aItem );
        SHAPE_POLY_SET poly;
        shape->EDA_SHAPE::TransformShapeToPolygon( poly, 0, maxError, ERROR_INSIDE, true );
        shape->SetShape( SHAPE_T::POLY );
        shape->SetPolyShape( poly );
    };

    for( BOARD_ITEM* item : aBoard->Drawings() )
        convert( item );

    for( FOOTPRINT* fp : aBoard->Footprints() )
    {
        for( BOARD_ITEM* item : fp->GraphicalItems() )
            convert( item );
    }
}


static int countExtrudedBodies( const BOARD* aBoard )
{
    int count = 0;

    for( FOOTPRINT* fp : aBoard->Footprints() )
    {
        if( fp->HasExtrudedBody() )
            count++;
    }

    return count;
}


static void dropExtrudedBodies( BOARD* aBoard )
{
    for( FOOTPRINT* fp : aBoard->Footprints() )
        fp->ClearExtrudedBody();
}


static int countThievingZones( const BOARD* aBoard )
{
    int count = 0;

    for( ZONE* zone : aBoard->Zones() )
    {
        if( zone->IsCopperThieving() )
            count++;
    }

    for( FOOTPRINT* fp : aBoard->Footprints() )
    {
        for( ZONE* zone : fp->Zones() )
        {
            if( zone->IsCopperThieving() )
                count++;
        }
    }

    return count;
}


static void lowerThievingZones( BOARD* aBoard )
{
    auto lower = []( ZONE* aZone )
    {
        if( aZone->IsCopperThieving() )
            aZone->SetFillMode( ZONE_FILL_MODE::POLYGONS );
    };

    for( ZONE* zone : aBoard->Zones() )
        lower( zone );

    for( FOOTPRINT* fp : aBoard->Footprints() )
    {
        for( ZONE* zone : fp->Zones() )
            lower( zone );
    }
}


static int countPadSimTypes( const BOARD* aBoard )
{
    int count = 0;

    for( PAD* pad : aBoard->GetPads() )
    {
        if( pad->GetSimElectricalType() != PAD_SIM_ELECTRICAL_TYPE::NONE )
            count++;
    }

    return count;
}


static void dropPadSimTypes( BOARD* aBoard )
{
    for( PAD* pad : aBoard->GetPads() )
        pad->SetSimElectricalType( PAD_SIM_ELECTRICAL_TYPE::NONE );
}


static int countFpUnitInfo( const BOARD* aBoard )
{
    int count = 0;

    for( FOOTPRINT* fp : aBoard->Footprints() )
    {
        if( !fp->GetUnitInfo().empty() )
            count++;
    }

    return count;
}


static void dropFpUnitInfo( BOARD* aBoard )
{
    for( FOOTPRINT* fp : aBoard->Footprints() )
        fp->SetUnitInfo( {} );
}


static int countBarcodes( const BOARD* aBoard )
{
    int count = 0;

    for( BOARD_ITEM* item : aBoard->Drawings() )
    {
        if( item->Type() == PCB_BARCODE_T )
            count++;
    }

    for( FOOTPRINT* fp : aBoard->Footprints() )
    {
        for( BOARD_ITEM* item : fp->GraphicalItems() )
        {
            if( item->Type() == PCB_BARCODE_T )
                count++;
        }
    }

    return count;
}


// FOOTPRINT::Remove does not detach group membership the way BOARD::Remove does, so items
// deleted from a footprint must leave their group first or the writer walks a dangling pointer.
static void detachFromGroup( BOARD_ITEM* aItem )
{
    if( EDA_GROUP* group = aItem->GetParentGroup() )
        group->RemoveItem( aItem );
}


static void dropBarcodes( BOARD* aBoard )
{
    std::vector<BOARD_ITEM*> barcodes;

    for( BOARD_ITEM* item : aBoard->Drawings() )
    {
        if( item->Type() == PCB_BARCODE_T )
            barcodes.push_back( item );
    }

    for( BOARD_ITEM* item : barcodes )
    {
        aBoard->Remove( item );
        delete item;
    }

    for( FOOTPRINT* fp : aBoard->Footprints() )
    {
        barcodes.clear();

        for( BOARD_ITEM* item : fp->GraphicalItems() )
        {
            if( item->Type() == PCB_BARCODE_T )
                barcodes.push_back( item );
        }

        for( BOARD_ITEM* item : barcodes )
        {
            detachFromGroup( item );
            fp->Remove( item );
            delete item;
        }
    }
}


static int countPcbVariants( const BOARD* aBoard )
{
    int count = aBoard->GetVariantNames().empty() ? 0 : 1;

    for( FOOTPRINT* fp : aBoard->Footprints() )
    {
        if( !fp->GetVariants().empty() )
            count++;
    }

    return count;
}


static void dropPcbVariants( BOARD* aBoard )
{
    aBoard->SetVariantNames( {} );
    aBoard->SetCurrentVariant( wxEmptyString );

    for( FOOTPRINT* fp : aBoard->Footprints() )
    {
        std::vector<wxString> names;

        for( const auto& [name, variant] : fp->GetVariants() )
            names.push_back( name );

        for( const wxString& name : names )
            fp->DeleteVariant( name );
    }
}


static int countJumperPads( const BOARD* aBoard )
{
    int count = 0;

    for( FOOTPRINT* fp : aBoard->Footprints() )
    {
        if( !fp->JumperPadGroups().empty() )
            count++;
    }

    return count;
}


static void dropJumperPads( BOARD* aBoard )
{
    for( FOOTPRINT* fp : aBoard->Footprints() )
        fp->JumperPadGroups().clear();
}


static bool hasBackdrill( const PADSTACK& aPadstack )
{
    return aPadstack.SecondaryDrill().size.x > 0 || aPadstack.TertiaryDrill().size.x > 0;
}


static int countBackdrills( const BOARD* aBoard )
{
    int count = 0;

    for( PAD* pad : aBoard->GetPads() )
    {
        if( hasBackdrill( pad->Padstack() ) )
            count++;
    }

    for( PCB_TRACK* track : aBoard->Tracks() )
    {
        if( track->Type() == PCB_VIA_T && hasBackdrill( static_cast<PCB_VIA*>( track )->Padstack() ) )
            count++;
    }

    return count;
}


static void dropBackdrills( BOARD* aBoard )
{
    for( PAD* pad : aBoard->GetPads() )
    {
        pad->ClearSecondaryDrillSize();
        pad->ClearTertiaryDrillSize();
    }

    for( PCB_TRACK* track : aBoard->Tracks() )
    {
        if( track->Type() == PCB_VIA_T )
        {
            PCB_VIA* via = static_cast<PCB_VIA*>( track );
            via->ClearSecondaryDrillSize();
            via->ClearTertiaryDrillSize();
        }
    }
}


static bool hasPostMachining( const PADSTACK& aPadstack )
{
    auto set = []( const PADSTACK::POST_MACHINING_PROPS& aProps )
    {
        return aProps.mode.has_value() && aProps.mode != PAD_DRILL_POST_MACHINING_MODE::NOT_POST_MACHINED;
    };

    return set( aPadstack.FrontPostMachining() ) || set( aPadstack.BackPostMachining() );
}


static int countPostMachining( const BOARD* aBoard )
{
    int count = 0;

    for( PAD* pad : aBoard->GetPads() )
    {
        if( hasPostMachining( pad->Padstack() ) )
            count++;
    }

    for( PCB_TRACK* track : aBoard->Tracks() )
    {
        if( track->Type() == PCB_VIA_T && hasPostMachining( static_cast<PCB_VIA*>( track )->Padstack() ) )
            count++;
    }

    return count;
}


static void dropPostMachining( BOARD* aBoard )
{
    auto clear = []( PADSTACK& aPadstack )
    {
        aPadstack.FrontPostMachining().mode.reset();
        aPadstack.BackPostMachining().mode.reset();
    };

    for( PAD* pad : aBoard->GetPads() )
        clear( pad->Padstack() );

    for( PCB_TRACK* track : aBoard->Tracks() )
    {
        if( track->Type() == PCB_VIA_T )
            clear( static_cast<PCB_VIA*>( track )->Padstack() );
    }
}


static int countPressFitPads( const BOARD* aBoard )
{
    int count = 0;

    for( PAD* pad : aBoard->GetPads() )
    {
        if( pad->GetProperty() == PAD_PROP::PRESSFIT )
            count++;
    }

    return count;
}


static void dropPressFitPads( BOARD* aBoard )
{
    for( PAD* pad : aBoard->GetPads() )
    {
        if( pad->GetProperty() == PAD_PROP::PRESSFIT )
            pad->SetProperty( PAD_PROP::NONE );
    }
}


static int countBuriedVias( const BOARD* aBoard )
{
    int count = 0;

    for( PCB_TRACK* track : aBoard->Tracks() )
    {
        if( track->Type() == PCB_VIA_T && static_cast<PCB_VIA*>( track )->GetViaType() == VIATYPE::BURIED )
            count++;
    }

    return count;
}


// The old format had one blind type covering both, so a buried via becomes blind.
static void lowerBuriedVias( BOARD* aBoard )
{
    for( PCB_TRACK* track : aBoard->Tracks() )
    {
        if( track->Type() == PCB_VIA_T )
        {
            PCB_VIA* via = static_cast<PCB_VIA*>( track );

            if( via->GetViaType() == VIATYPE::BURIED )
                via->SetViaType( VIATYPE::BLIND );
        }
    }
}


static int countZoneDefaults( const BOARD* aBoard )
{
    return aBoard->GetDesignSettings().m_ZoneLayerProperties.empty() ? 0 : 1;
}


static void dropZoneDefaults( BOARD* aBoard )
{
    aBoard->GetDesignSettings().m_ZoneLayerProperties.clear();
}


static int countSkipVias( const BOARD* aBoard )
{
    int count = 0;

    for( PCB_TRACK* track : aBoard->Tracks() )
    {
        if( track->Type() == PCB_VIA_T
            && static_cast<PCB_VIA*>( track )->Padstack().UnconnectedLayerMode()
                       == UNCONNECTED_LAYER_MODE::START_END_ONLY )
        {
            count++;
        }
    }

    return count;
}


// Skip vias keep copper only on the start and end layers. The nearest older mode keeps the outer
// rings and drops the inner ones, so use that.
static void lowerSkipVias( BOARD* aBoard )
{
    for( PCB_TRACK* track : aBoard->Tracks() )
    {
        if( track->Type() != PCB_VIA_T )
            continue;

        PADSTACK& padstack = static_cast<PCB_VIA*>( track )->Padstack();

        if( padstack.UnconnectedLayerMode() == UNCONNECTED_LAYER_MODE::START_END_ONLY )
            padstack.SetUnconnectedLayerMode( UNCONNECTED_LAYER_MODE::REMOVE_EXCEPT_START_AND_END );
    }
}


static int countDieDelays( const BOARD* aBoard )
{
    int count = 0;

    for( PAD* pad : aBoard->GetPads() )
    {
        if( pad->GetPadToDieDelay() != 0 )
            count++;
    }

    return count;
}


static void dropDieDelays( BOARD* aBoard )
{
    for( PAD* pad : aBoard->GetPads() )
        pad->SetPadToDieDelay( 0 );
}


static bool isHatchedShape( const BOARD_ITEM* aItem )
{
    return aItem->Type() == PCB_SHAPE_T && static_cast<const PCB_SHAPE*>( aItem )->IsHatchedFill();
}


static int countHatchedShapes( const BOARD* aBoard )
{
    int count = 0;

    for( BOARD_ITEM* item : aBoard->Drawings() )
    {
        if( isHatchedShape( item ) )
            count++;
    }

    for( FOOTPRINT* fp : aBoard->Footprints() )
    {
        for( BOARD_ITEM* item : fp->GraphicalItems() )
        {
            if( isHatchedShape( item ) )
                count++;
        }
    }

    return count;
}


static void lowerHatchedShapes( BOARD* aBoard )
{
    auto convert = [&]( BOARD_ITEM* aItem )
    {
        if( isHatchedShape( aItem ) )
            static_cast<PCB_SHAPE*>( aItem )->SetFillMode( FILL_T::FILLED_SHAPE );
    };

    for( BOARD_ITEM* item : aBoard->Drawings() )
        convert( item );

    for( FOOTPRINT* fp : aBoard->Footprints() )
    {
        for( BOARD_ITEM* item : fp->GraphicalItems() )
            convert( item );
    }
}


static int countPcbPoints( const BOARD* aBoard )
{
    int count = static_cast<int>( aBoard->Points().size() );

    for( FOOTPRINT* fp : aBoard->Footprints() )
        count += static_cast<int>( fp->Points().size() );

    return count;
}


static void dropPcbPoints( BOARD* aBoard )
{
    std::vector<BOARD_ITEM*> points( aBoard->Points().begin(), aBoard->Points().end() );

    for( BOARD_ITEM* item : points )
    {
        aBoard->Remove( item );
        delete item;
    }

    for( FOOTPRINT* fp : aBoard->Footprints() )
    {
        for( PCB_POINT* point : fp->Points() )
        {
            detachFromGroup( point );
            delete point;
        }

        fp->Points().clear();
    }
}


static int countHighUserLayers( const BOARD* aBoard )
{
    const LSET& enabled = aBoard->GetEnabledLayers();
    int         count = 0;

    // User_10 and up only exist since 20241229. Odd ids in this range are the user layers.
    for( int layer = User_10; layer <= User_45; layer += 2 )
    {
        if( enabled.Contains( PCB_LAYER_ID( layer ) ) )
            count++;
    }

    return count;
}


static int countScaledFootprints( const BOARD* aBoard )
{
    int count = 0;

    for( FOOTPRINT* fp : aBoard->Footprints() )
    {
        if( fp->GetTransform().GetScaleX() != 1.0 || fp->GetTransform().GetScaleY() != 1.0 )
            count++;
    }

    return count;
}


// One row per feature. The report and the transform both walk this list, so they cannot drift.
// To add a feature, add a row. A null apply means the feature blocks the export.
struct BOARD_RULE
{
    int                                m_introducedIn;
    DOWNGRADE_BUCKET                   m_bucket;
    wxString                           m_feature;
    wxString                           m_detail;
    std::function<int( const BOARD* )> m_count;
    std::function<void( BOARD* )>      m_apply;
};


static std::vector<BOARD_RULE> boardRules()
{
    return {
        { VER_NET_CHAINS, DOWNGRADE_BUCKET::DROP, _( "Net chains" ),
          _( "No equivalent in the target. Connectivity will change." ), countNetChains, dropNetChains },
        { VER_ELLIPSE, DOWNGRADE_BUCKET::LOWER, _( "Ellipse graphics" ), _( "Approximated by a polygon." ),
          countEllipses, lowerEllipses },
        { VER_EXTRUDED_BODY, DOWNGRADE_BUCKET::DROP, _( "Extruded 3D bodies" ),
          _( "No equivalent in the target. The 3D body is left out." ), countExtrudedBodies, dropExtrudedBodies },
        { VER_THIEVING, DOWNGRADE_BUCKET::LOWER, _( "Copper-thieving zones" ), _( "Converted to a plain filled zone." ),
          countThievingZones, lowerThievingZones },
        { VER_PAD_SIM, DOWNGRADE_BUCKET::DROP, _( "Pad simulation types" ),
          _( "The simulation electrical type is left out." ), countPadSimTypes, dropPadSimTypes },
        { VER_FP_UNITS, DOWNGRADE_BUCKET::DROP, _( "Footprint unit metadata" ),
          _( "Gate-swap unit metadata is removed." ), countFpUnitInfo, dropFpUnitInfo },
        { VER_BARCODE, DOWNGRADE_BUCKET::DROP, _( "Barcodes" ),
          _( "No equivalent in the target. Barcodes are removed." ), countBarcodes, dropBarcodes },
        { VER_PCB_VARIANTS, DOWNGRADE_BUCKET::DROP, _( "PCB variants" ),
          _( "Variant registry and per-footprint overrides are removed." ), countPcbVariants, dropPcbVariants },
        { VER_JUMPER_PADS, DOWNGRADE_BUCKET::DROP, _( "Jumper pad groups" ), _( "Jumper pad groupings are removed." ),
          countJumperPads, dropJumperPads },
        { VER_BACKDRILL, DOWNGRADE_BUCKET::DROP, _( "Backdrill / tertiary drill" ),
          _( "Secondary and tertiary drills are removed." ), countBackdrills, dropBackdrills },
        { VER_POST_MACHINING, DOWNGRADE_BUCKET::DROP, _( "Counterbore / countersink" ),
          _( "Drill post-machining is removed." ), countPostMachining, dropPostMachining },
        { VER_PRESSFIT, DOWNGRADE_BUCKET::DROP, _( "Press-fit pads" ),
          _( "The press-fit fabrication property is removed." ), countPressFitPads, dropPressFitPads },
        { VER_SKIP_VIA, DOWNGRADE_BUCKET::LOWER, _( "Skip vias" ),
          _( "Approximated by keeping only the start and end layers." ), countSkipVias, lowerSkipVias },
        { VER_VIA_SPLIT, DOWNGRADE_BUCKET::LOWER, _( "Buried vias" ),
          _( "Written with the older combined blind type." ), countBuriedVias, lowerBuriedVias },
        { VER_ZONE_DEFAULTS, DOWNGRADE_BUCKET::DROP, _( "Zone layer defaults" ),
          _( "Per-layer zone defaults are removed." ), countZoneDefaults, dropZoneDefaults },
        { VER_PAD_DIE_DELAY, DOWNGRADE_BUCKET::DROP, _( "Pad-to-die delays" ), _( "Pad-to-die delay is removed." ),
          countDieDelays, dropDieDelays },
        { VER_SHAPE_HATCH, DOWNGRADE_BUCKET::LOWER, _( "Hatched shape fills" ), _( "Converted to a solid fill." ),
          countHatchedShapes, lowerHatchedShapes },
        { VER_PCB_POINTS, DOWNGRADE_BUCKET::DROP, _( "PCB points" ),
          _( "No equivalent in the target. Points are removed." ), countPcbPoints, dropPcbPoints },
        { VER_USER_LAYERS, DOWNGRADE_BUCKET::BLOCK, _( "User layers past User_9" ),
          _( "The target supports only 9 user layers." ), countHighUserLayers, nullptr },
        { FIRST_FP_AFFINE_TRANSFORM, DOWNGRADE_BUCKET::BLOCK, _( "Scaled footprints" ),
          _( "The target cannot represent a scaled footprint." ), countScaledFootprints, nullptr },
    };
}


COMPATIBILITY_REPORT ClassifyBoardForDowngrade( const BOARD* aBoard, const DOWNGRADE_TARGET& aTarget )
{
    // The rules were written against a specific format. Warn developers when the format has
    // moved on, since a frozen writer silently omits features that have no rule yet.
    wxASSERT_MSG( SEXPR_BOARD_FILE_VERSION == BOARD_DOWNGRADE_COVERED,
                  wxT( "Board format changed. Review the downgrade rules and denylist, then update "
                       "BOARD_DOWNGRADE_COVERED." ) );

    COMPATIBILITY_REPORT report;

    for( const BOARD_RULE& rule : boardRules() )
    {
        if( aTarget.m_boardVersion >= rule.m_introducedIn )
            continue;

        if( int count = rule.m_count( aBoard ); count > 0 )
            report.Add( rule.m_bucket, rule.m_feature, rule.m_detail, count );
    }

    return report;
}


void DowngradeBoardInPlace( BOARD* aBoard, const DOWNGRADE_TARGET& aTarget )
{
    for( const BOARD_RULE& rule : boardRules() )
    {
        if( aTarget.m_boardVersion >= rule.m_introducedIn )
            continue;

        if( rule.m_apply )
            rule.m_apply( aBoard );
    }
}


// A library footprint carries the same post-target features as one on a board, but it lives in its
// own .kicad_mod, so the board rules never see it. This applies the footprint-scoped subset.
void DowngradeFootprintInPlace( FOOTPRINT* aFootprint, const DOWNGRADE_TARGET& aTarget )
{
    const int ver = aTarget.m_boardVersion;

    for( PAD* pad : aFootprint->Pads() )
    {
        if( ver < VER_POST_MACHINING )
        {
            pad->Padstack().FrontPostMachining().mode.reset();
            pad->Padstack().BackPostMachining().mode.reset();
        }

        if( ver < VER_PRESSFIT && pad->GetProperty() == PAD_PROP::PRESSFIT )
            pad->SetProperty( PAD_PROP::NONE );

        if( ver < VER_BACKDRILL )
        {
            pad->ClearSecondaryDrillSize();
            pad->ClearTertiaryDrillSize();
        }

        if( ver < VER_PAD_DIE_DELAY )
            pad->SetPadToDieDelay( 0 );

        if( ver < VER_PAD_SIM )
            pad->SetSimElectricalType( PAD_SIM_ELECTRICAL_TYPE::NONE );
    }

    int maxError = aFootprint->GetBoard() ? aFootprint->GetBoard()->GetDesignSettings().m_MaxError
                                          : pcbIUScale.mmToIU( 0.005 );

    std::vector<BOARD_ITEM*> barcodes;

    for( BOARD_ITEM* item : aFootprint->GraphicalItems() )
    {
        if( ver < VER_ELLIPSE && isEllipse( item ) )
        {
            PCB_SHAPE*     shape = static_cast<PCB_SHAPE*>( item );
            SHAPE_POLY_SET poly;
            shape->EDA_SHAPE::TransformShapeToPolygon( poly, 0, maxError, ERROR_INSIDE, true );
            shape->SetShape( SHAPE_T::POLY );
            shape->SetPolyShape( poly );
        }

        if( ver < VER_SHAPE_HATCH && isHatchedShape( item ) )
            static_cast<PCB_SHAPE*>( item )->SetFillMode( FILL_T::FILLED_SHAPE );

        if( ver < VER_BARCODE && item->Type() == PCB_BARCODE_T )
            barcodes.push_back( item );
    }

    for( BOARD_ITEM* item : barcodes )
    {
        detachFromGroup( item );
        aFootprint->Remove( item );
        delete item;
    }

    if( ver < VER_EXTRUDED_BODY )
        aFootprint->ClearExtrudedBody();

    if( ver < VER_FP_UNITS )
        aFootprint->SetUnitInfo( {} );

    if( ver < VER_JUMPER_PADS )
        aFootprint->JumperPadGroups().clear();

    if( ver < VER_THIEVING )
    {
        for( ZONE* zone : aFootprint->Zones() )
        {
            if( zone->IsCopperThieving() )
                zone->SetFillMode( ZONE_FILL_MODE::POLYGONS );
        }
    }

    if( ver < VER_PCB_POINTS )
    {
        for( PCB_POINT* point : aFootprint->Points() )
        {
            detachFromGroup( point );
            delete point;
        }

        aFootprint->Points().clear();
    }

    if( ver < VER_PCB_VARIANTS )
    {
        std::vector<wxString> names;

        for( const auto& [name, variant] : aFootprint->GetVariants() )
            names.push_back( name );

        for( const wxString& name : names )
            aFootprint->DeleteVariant( name );
    }
}


// Node heads dated by the format that introduced them. The versioned writers cannot emit these,
// so this gate is a safety net against a wrong or miswired writer. To add a token, add a row
// with its format version.
struct DATED_TOKEN
{
    const char* m_token;
    int         m_introducedIn;
};

static constexpr DATED_TOKEN BOARD_TOKENS[] = {
    { "hatch_position", VER_SHAPE_HATCH },
    { "covering", VER_VIA_PROTECTION },
    { "plugging", VER_VIA_PROTECTION },
    { "capping", VER_VIA_PROTECTION },
    { "filling", VER_VIA_PROTECTION },
    { "depth", VER_VIA_PROTECTION },
    { "zone_defaults", VER_ZONE_DEFAULTS },
    { "jumper_pad_groups", VER_JUMPER_PADS },
    { "lib_id", VER_GROUP_LIB_ID },
    { "start_end_only", VER_SKIP_VIA },
    { "point", VER_PCB_POINTS },
    { "unit", VER_FP_UNITS },
    { "pins", VER_FP_UNITS },
    { "barcode", VER_BARCODE },
    { "ecc_level", VER_BARCODE },
    { "text", VER_BARCODE },
    { "text_height", VER_BARCODE },
    { "die_delay", VER_PAD_DIE_DELAY },
    { "backdrill", VER_BACKDRILL },
    { "tertiary_drill", VER_BACKDRILL },
    { "front_post_machining", VER_POST_MACHINING },
    { "back_post_machining", VER_POST_MACHINING },
    { "variant", VER_PCB_VARIANTS },
    { "variants", VER_PCB_VARIANTS },
    { "field", VER_PCB_VARIANTS },
    { "description", VER_PCB_VARIANTS },
    { "body_pcb_gap", VER_EXTRUDED_BODY },
    { "overall_height", VER_EXTRUDED_BODY },
    { "gr_ellipse", VER_ELLIPSE },
    { "gr_ellipse_arc", VER_ELLIPSE },
    { "fp_ellipse", VER_ELLIPSE },
    { "fp_ellipse_arc", VER_ELLIPSE },
    { "dielectric_model", VER_DIELECTRIC },
    { "spec_frequency", VER_DIELECTRIC },
    { "constant", VER_DIELECTRIC },
    { "net_chain", VER_NET_CHAINS },
    { "net_chains", VER_NET_CHAINS },
    { "terminal_pad", VER_NET_CHAINS },
    { "thieving", VER_THIEVING },
    { "rotation_angle", VER_THIEVING },
    { "stagger", VER_THIEVING },
    { "sim_electrical_type", VER_PAD_SIM },
    { "transform", FIRST_FP_AFFINE_TRANSFORM },
};

// Bareword values the target cannot parse. The node scan cannot see these.
static constexpr DATED_TOKEN BOARD_VALUES[] = {
    { "pad_prop_pressfit", VER_PRESSFIT },
    { "buried", VER_VIA_SPLIT },
};


static bool containsSexprNode( const std::string& aText, const std::string& aToken )
{
    const std::string needle = "(" + aToken;

    for( size_t pos = aText.find( needle ); pos != std::string::npos; pos = aText.find( needle, pos + 1 ) )
    {
        // Match the whole token, so "(net" is never mistaken for "(net_chain".
        size_t after = pos + needle.size();
        char   next = ( after < aText.size() ) ? aText[after] : ' ';

        if( !std::isalnum( static_cast<unsigned char>( next ) ) && next != '_' )
            return true;
    }

    return false;
}


// Some features are written as a bareword value, not a node head, so the node scan cannot see
// them. Match the token as a whole word instead.
static bool containsSexprValue( const std::string& aText, const std::string& aToken )
{
    for( size_t pos = aText.find( aToken ); pos != std::string::npos; pos = aText.find( aToken, pos + 1 ) )
    {
        char   prev = pos > 0 ? aText[pos - 1] : ' ';
        size_t after = pos + aToken.size();
        char   next = after < aText.size() ? aText[after] : ' ';

        bool prevOk = !std::isalnum( static_cast<unsigned char>( prev ) ) && prev != '_';
        bool nextOk = !std::isalnum( static_cast<unsigned char>( next ) ) && next != '_';

        if( prevOk && nextOk )
            return true;
    }

    return false;
}


// Drop quoted strings so a token inside a text value cannot cause a false refusal.
static std::string stripSexprStrings( const std::string& aText )
{
    std::string result;
    bool        inString = false;

    for( size_t i = 0; i < aText.size(); i++ )
    {
        char c = aText[i];

        // A quote is real only if an even number of backslashes precede it. This stops a value that
        // ends in a backslash from swallowing the rest of the file.
        if( c == '"' )
        {
            size_t backslashes = 0;

            for( size_t k = i; k > 0 && aText[k - 1] == '\\'; k-- )
                backslashes++;

            if( backslashes % 2 == 0 )
            {
                inString = !inString;
                continue;
            }
        }

        if( !inString )
            result += c;
    }

    return result;
}


int BoardDowngradeCoveredVersion()
{
    return BOARD_DOWNGRADE_COVERED;
}


// Each target has its own writer, extracted from that release. A target without a writer writes
// nothing, and the fail-closed check downstream refuses the export.
void SaveBoardForTarget( BOARD* aBoard, const wxString& aPath, const DOWNGRADE_TARGET& aTarget )
{
    if( aTarget.m_boardVersion == PCB_WRITER_V10::FORMAT_VERSION )
        PCB_WRITER_V10().SaveBoard( aPath, aBoard );
    else if( aTarget.m_boardVersion == PCB_WRITER_V9::FORMAT_VERSION )
        PCB_WRITER_V9().SaveBoard( aPath, aBoard );
    else
        wxFAIL_MSG( wxT( "No writer for this downgrade target." ) );
}


void SaveFootprintForTarget( FOOTPRINT* aFootprint, const wxString& aPath, const DOWNGRADE_TARGET& aTarget )
{
    if( aTarget.m_boardVersion == PCB_WRITER_V10::FORMAT_VERSION )
        PCB_WRITER_V10().SaveFootprintFile( aPath, aFootprint );
    else if( aTarget.m_boardVersion == PCB_WRITER_V9::FORMAT_VERSION )
        PCB_WRITER_V9().SaveFootprintFile( aPath, aFootprint );
    else
        wxFAIL_MSG( wxT( "No writer for this downgrade target." ) );
}


wxString FindUnsupportedBoardToken( const wxString& aSerialized, const DOWNGRADE_TARGET& aTarget )
{
    if( aTarget.m_boardVersion >= SEXPR_BOARD_FILE_VERSION )
        return wxEmptyString;

    // Explicit UTF-8: the default locale conversion returns an empty string on unrepresentable
    // characters, which would silently pass the scan.
    std::string text = stripSexprStrings( aSerialized.ToStdString( wxConvUTF8 ) );

    for( const DATED_TOKEN& token : BOARD_TOKENS )
    {
        if( aTarget.m_boardVersion < token.m_introducedIn && containsSexprNode( text, token.m_token ) )
            return wxString::FromUTF8( token.m_token );
    }

    for( const DATED_TOKEN& value : BOARD_VALUES )
    {
        if( aTarget.m_boardVersion < value.m_introducedIn && containsSexprValue( text, value.m_token ) )
            return wxString::FromUTF8( value.m_token );
    }

    return wxEmptyString;
}


bool ExportBoardToOlderVersion( const wxString& aSrcFile, const wxString& aDestFile, const DOWNGRADE_TARGET& aTarget,
                                COMPATIBILITY_REPORT& aReport, wxString* aUnsupportedToken )
{
    if( aUnsupportedToken )
        aUnsupportedToken->clear();

    PCB_IO_KICAD_SEXPR     pi;
    std::unique_ptr<BOARD> board( pi.LoadBoard( aSrcFile, nullptr ) );

    if( !board )
        return false;

    aReport = ClassifyBoardForDowngrade( board.get(), aTarget );

    if( aReport.IsBlocked() )
        return false;

    DowngradeBoardInPlace( board.get(), aTarget );

    // Write to a sibling temp and verify that before it replaces the destination, so a failed
    // export never destroys a pre-existing output file.
    const wxString tmpFile = aDestFile + wxT( ".downgrade_tmp" );

    SaveBoardForTarget( board.get(), tmpFile, aTarget );

    // Fail closed. Discard the temp if it still uses a token the target cannot parse, or if it
    // cannot be read back to verify.
    wxString content;
    bool     readOk;

    {
        wxFFile file( tmpFile, wxT( "rb" ) );
        readOk = file.IsOpened() && file.ReadAll( &content );
    }

    if( !readOk )
    {
        wxRemoveFile( tmpFile );
        return false;
    }

    // The output must carry the target's stamp. Catches a dispatch miss that left another file.
    if( !content.Contains( wxString::Format( wxT( "(version %d)" ), aTarget.m_boardVersion ) ) )
    {
        wxRemoveFile( tmpFile );
        return false;
    }

    wxString bad = FindUnsupportedBoardToken( content, aTarget );

    if( !bad.IsEmpty() )
    {
        wxRemoveFile( tmpFile );

        if( aUnsupportedToken )
            *aUnsupportedToken = bad;

        return false;
    }

    if( !wxRenameFile( tmpFile, aDestFile, true ) )
    {
        wxRemoveFile( tmpFile );
        return false;
    }

    return true;
}
