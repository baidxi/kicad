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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <layer_roles.h>

#include <unordered_map>

#include <gerber_file_image.h>
#include <gerber_file_image_list.h>
#include <gerbview_frame.h>
#include <gerbview_painter.h>
#include <gerbview_settings.h>
#include <layer_ids.h>
#include <settings/color_settings.h>
#include <view/view.h>

#include <wx/filename.h>
#include <wx/translation.h>


wxString RoleDisplayName( GERBVIEW_LAYER_ROLE aRole )
{
    switch( aRole )
    {
    case GERBVIEW_LAYER_ROLE::Unassigned: return _( "Unassigned" );

    case GERBVIEW_LAYER_ROLE::TopSolderPaste: return _( "Top Solder Paste" );
    case GERBVIEW_LAYER_ROLE::TopSolderMask: return _( "Top Solder Mask" );
    case GERBVIEW_LAYER_ROLE::TopSilkScreen: return _( "Top Silk Screen" );
    case GERBVIEW_LAYER_ROLE::TopCopper: return _( "Top Copper" );

    case GERBVIEW_LAYER_ROLE::InnerCopper1: return _( "Inner Copper 1" );
    case GERBVIEW_LAYER_ROLE::InnerCopper2: return _( "Inner Copper 2" );
    case GERBVIEW_LAYER_ROLE::InnerCopper3: return _( "Inner Copper 3" );
    case GERBVIEW_LAYER_ROLE::InnerCopper4: return _( "Inner Copper 4" );
    case GERBVIEW_LAYER_ROLE::InnerCopper5: return _( "Inner Copper 5" );
    case GERBVIEW_LAYER_ROLE::InnerCopper6: return _( "Inner Copper 6" );
    case GERBVIEW_LAYER_ROLE::InnerCopper7: return _( "Inner Copper 7" );
    case GERBVIEW_LAYER_ROLE::InnerCopper8: return _( "Inner Copper 8" );

    case GERBVIEW_LAYER_ROLE::BottomCopper: return _( "Bottom Copper" );
    case GERBVIEW_LAYER_ROLE::BottomSilkScreen: return _( "Bottom Silk Screen" );
    case GERBVIEW_LAYER_ROLE::BottomSolderMask: return _( "Bottom Solder Mask" );
    case GERBVIEW_LAYER_ROLE::BottomSolderPaste: return _( "Bottom Solder Paste" );

    case GERBVIEW_LAYER_ROLE::DrillPlated: return _( "Drill (Plated)" );
    case GERBVIEW_LAYER_ROLE::DrillUnplated: return _( "Drill (Unplated)" );
    case GERBVIEW_LAYER_ROLE::ContourPlated: return _( "Contour (Plated)" );
    case GERBVIEW_LAYER_ROLE::ContourUnplated: return _( "Contour (Unplated)" );
    }

    return _( "Unassigned" );
}


wxString RoleShortCode( GERBVIEW_LAYER_ROLE aRole )
{
    // Codes match those displayed next to each layer-manager visibility
    // checkbox.  Three characters keeps the layout compact and predictable.
    switch( aRole )
    {
    case GERBVIEW_LAYER_ROLE::Unassigned: return wxEmptyString;

    case GERBVIEW_LAYER_ROLE::TopSolderPaste: return wxT( "SPT" );
    case GERBVIEW_LAYER_ROLE::TopSolderMask: return wxT( "SMT" );
    case GERBVIEW_LAYER_ROLE::TopSilkScreen: return wxT( "SST" );
    case GERBVIEW_LAYER_ROLE::TopCopper: return wxT( "TOP" );

    case GERBVIEW_LAYER_ROLE::InnerCopper1: return wxT( "IN1" );
    case GERBVIEW_LAYER_ROLE::InnerCopper2: return wxT( "IN2" );
    case GERBVIEW_LAYER_ROLE::InnerCopper3: return wxT( "IN3" );
    case GERBVIEW_LAYER_ROLE::InnerCopper4: return wxT( "IN4" );
    case GERBVIEW_LAYER_ROLE::InnerCopper5: return wxT( "IN5" );
    case GERBVIEW_LAYER_ROLE::InnerCopper6: return wxT( "IN6" );
    case GERBVIEW_LAYER_ROLE::InnerCopper7: return wxT( "IN7" );
    case GERBVIEW_LAYER_ROLE::InnerCopper8: return wxT( "IN8" );

    case GERBVIEW_LAYER_ROLE::BottomCopper: return wxT( "BOT" );
    case GERBVIEW_LAYER_ROLE::BottomSilkScreen: return wxT( "SSB" );
    case GERBVIEW_LAYER_ROLE::BottomSolderMask: return wxT( "SMB" );
    case GERBVIEW_LAYER_ROLE::BottomSolderPaste: return wxT( "SPB" );

    case GERBVIEW_LAYER_ROLE::DrillPlated: return wxT( "DRP" );
    case GERBVIEW_LAYER_ROLE::DrillUnplated: return wxT( "DUP" );
    case GERBVIEW_LAYER_ROLE::ContourPlated: return wxT( "COP" );
    case GERBVIEW_LAYER_ROLE::ContourUnplated: return wxT( "CUP" );
    }

    return wxEmptyString;
}


const char* RoleSerializedKey( GERBVIEW_LAYER_ROLE aRole )
{
    switch( aRole )
    {
    case GERBVIEW_LAYER_ROLE::Unassigned: return "unassigned";

    case GERBVIEW_LAYER_ROLE::TopSolderPaste: return "top_solder_paste";
    case GERBVIEW_LAYER_ROLE::TopSolderMask: return "top_solder_mask";
    case GERBVIEW_LAYER_ROLE::TopSilkScreen: return "top_silk_screen";
    case GERBVIEW_LAYER_ROLE::TopCopper: return "top_copper";

    case GERBVIEW_LAYER_ROLE::InnerCopper1: return "inner_copper_1";
    case GERBVIEW_LAYER_ROLE::InnerCopper2: return "inner_copper_2";
    case GERBVIEW_LAYER_ROLE::InnerCopper3: return "inner_copper_3";
    case GERBVIEW_LAYER_ROLE::InnerCopper4: return "inner_copper_4";
    case GERBVIEW_LAYER_ROLE::InnerCopper5: return "inner_copper_5";
    case GERBVIEW_LAYER_ROLE::InnerCopper6: return "inner_copper_6";
    case GERBVIEW_LAYER_ROLE::InnerCopper7: return "inner_copper_7";
    case GERBVIEW_LAYER_ROLE::InnerCopper8: return "inner_copper_8";

    case GERBVIEW_LAYER_ROLE::BottomCopper: return "bottom_copper";
    case GERBVIEW_LAYER_ROLE::BottomSilkScreen: return "bottom_silk_screen";
    case GERBVIEW_LAYER_ROLE::BottomSolderMask: return "bottom_solder_mask";
    case GERBVIEW_LAYER_ROLE::BottomSolderPaste: return "bottom_solder_paste";

    case GERBVIEW_LAYER_ROLE::DrillPlated: return "drill_plated";
    case GERBVIEW_LAYER_ROLE::DrillUnplated: return "drill_unplated";
    case GERBVIEW_LAYER_ROLE::ContourPlated: return "contour_plated";
    case GERBVIEW_LAYER_ROLE::ContourUnplated: return "contour_unplated";
    }

    return "unassigned";
}


GERBVIEW_LAYER_ROLE RoleFromSerializedKey( const std::string& aKey )
{
    static const std::unordered_map<std::string, GERBVIEW_LAYER_ROLE> lookup = {
        { "unassigned", GERBVIEW_LAYER_ROLE::Unassigned },

        { "top_solder_paste", GERBVIEW_LAYER_ROLE::TopSolderPaste },
        { "top_solder_mask", GERBVIEW_LAYER_ROLE::TopSolderMask },
        { "top_silk_screen", GERBVIEW_LAYER_ROLE::TopSilkScreen },
        { "top_copper", GERBVIEW_LAYER_ROLE::TopCopper },

        // Legacy single-inner key: migrate to InnerCopper1.
        { "inner_copper", GERBVIEW_LAYER_ROLE::InnerCopper1 },
        { "inner_copper_1", GERBVIEW_LAYER_ROLE::InnerCopper1 },
        { "inner_copper_2", GERBVIEW_LAYER_ROLE::InnerCopper2 },
        { "inner_copper_3", GERBVIEW_LAYER_ROLE::InnerCopper3 },
        { "inner_copper_4", GERBVIEW_LAYER_ROLE::InnerCopper4 },
        { "inner_copper_5", GERBVIEW_LAYER_ROLE::InnerCopper5 },
        { "inner_copper_6", GERBVIEW_LAYER_ROLE::InnerCopper6 },
        { "inner_copper_7", GERBVIEW_LAYER_ROLE::InnerCopper7 },
        { "inner_copper_8", GERBVIEW_LAYER_ROLE::InnerCopper8 },

        { "bottom_copper", GERBVIEW_LAYER_ROLE::BottomCopper },
        { "bottom_silk_screen", GERBVIEW_LAYER_ROLE::BottomSilkScreen },
        { "bottom_solder_mask", GERBVIEW_LAYER_ROLE::BottomSolderMask },
        { "bottom_solder_paste", GERBVIEW_LAYER_ROLE::BottomSolderPaste },

        { "drill_plated", GERBVIEW_LAYER_ROLE::DrillPlated },
        { "drill_unplated", GERBVIEW_LAYER_ROLE::DrillUnplated },
        { "contour_plated", GERBVIEW_LAYER_ROLE::ContourPlated },
        { "contour_unplated", GERBVIEW_LAYER_ROLE::ContourUnplated },
    };

    auto it = lookup.find( aKey );
    return it == lookup.end() ? GERBVIEW_LAYER_ROLE::Unassigned : it->second;
}


KIGFX::COLOR4D DefaultColorFor( GERBVIEW_LAYER_ROLE aRole )
{
    // Predefined palette intended for a dark GerbView background.  Values in 0..1 floats.
    //
    // Top section is in the green/teal/blue family (mask + paste sit "above" the copper),
    // bottom is the warm-red/brown mirror of it, and inner layers transition from purple
    // (Inner 1) to pink (Inner 2) with extra shades for additional inner layers.
    switch( aRole )
    {
    // Top - blue / teal / green family.
    case GERBVIEW_LAYER_ROLE::TopSolderPaste: return KIGFX::COLOR4D( 0.18, 0.30, 0.65, 1.0 );
    case GERBVIEW_LAYER_ROLE::TopSolderMask: return KIGFX::COLOR4D( 0.16, 0.62, 0.55, 1.0 );
    case GERBVIEW_LAYER_ROLE::TopSilkScreen: return KIGFX::COLOR4D( 0.96, 0.96, 0.92, 1.0 );
    case GERBVIEW_LAYER_ROLE::TopCopper: return KIGFX::COLOR4D( 0.30, 0.78, 0.40, 1.0 );

    // Inner copper - light blue, then pink, then a purple/violet gradient.
    case GERBVIEW_LAYER_ROLE::InnerCopper1: return KIGFX::COLOR4D( 0.40, 0.72, 0.95, 1.0 );
    case GERBVIEW_LAYER_ROLE::InnerCopper2: return KIGFX::COLOR4D( 0.95, 0.36, 0.71, 1.0 );
    case GERBVIEW_LAYER_ROLE::InnerCopper3: return KIGFX::COLOR4D( 0.50, 0.40, 0.85, 1.0 );
    case GERBVIEW_LAYER_ROLE::InnerCopper4: return KIGFX::COLOR4D( 0.85, 0.30, 0.80, 1.0 );
    case GERBVIEW_LAYER_ROLE::InnerCopper5: return KIGFX::COLOR4D( 0.72, 0.50, 0.95, 1.0 );
    case GERBVIEW_LAYER_ROLE::InnerCopper6: return KIGFX::COLOR4D( 1.00, 0.45, 0.55, 1.0 );
    case GERBVIEW_LAYER_ROLE::InnerCopper7: return KIGFX::COLOR4D( 0.55, 0.55, 0.95, 1.0 );
    case GERBVIEW_LAYER_ROLE::InnerCopper8: return KIGFX::COLOR4D( 0.80, 0.70, 0.95, 1.0 );

    // Bottom - warm red / brown family (mirror of the top palette).
    case GERBVIEW_LAYER_ROLE::BottomCopper: return KIGFX::COLOR4D( 0.86, 0.22, 0.22, 1.0 );
    case GERBVIEW_LAYER_ROLE::BottomSilkScreen: return KIGFX::COLOR4D( 0.82, 0.82, 0.78, 1.0 );
    case GERBVIEW_LAYER_ROLE::BottomSolderMask: return KIGFX::COLOR4D( 0.55, 0.10, 0.15, 1.0 );
    case GERBVIEW_LAYER_ROLE::BottomSolderPaste: return KIGFX::COLOR4D( 0.55, 0.40, 0.20, 1.0 );

    case GERBVIEW_LAYER_ROLE::DrillPlated: return KIGFX::COLOR4D( 0.70, 0.75, 0.95, 1.0 );
    case GERBVIEW_LAYER_ROLE::DrillUnplated: return KIGFX::COLOR4D( 0.38, 0.42, 0.55, 1.0 );
    case GERBVIEW_LAYER_ROLE::ContourPlated: return KIGFX::COLOR4D( 0.90, 0.85, 0.40, 1.0 );
    case GERBVIEW_LAYER_ROLE::ContourUnplated: return KIGFX::COLOR4D( 0.55, 0.55, 0.30, 1.0 );

    case GERBVIEW_LAYER_ROLE::Unassigned:
    default: return KIGFX::COLOR4D( 1.0, 1.0, 1.0, 1.0 );
    }
}


const std::vector<GERBVIEW_LAYER_ROLE>& AllRoles()
{
    static const std::vector<GERBVIEW_LAYER_ROLE> all = {
        GERBVIEW_LAYER_ROLE::Unassigned,

        GERBVIEW_LAYER_ROLE::TopSolderPaste,   GERBVIEW_LAYER_ROLE::TopSolderMask,
        GERBVIEW_LAYER_ROLE::TopSilkScreen,    GERBVIEW_LAYER_ROLE::TopCopper,

        GERBVIEW_LAYER_ROLE::InnerCopper1,     GERBVIEW_LAYER_ROLE::InnerCopper2,
        GERBVIEW_LAYER_ROLE::InnerCopper3,     GERBVIEW_LAYER_ROLE::InnerCopper4,
        GERBVIEW_LAYER_ROLE::InnerCopper5,     GERBVIEW_LAYER_ROLE::InnerCopper6,
        GERBVIEW_LAYER_ROLE::InnerCopper7,     GERBVIEW_LAYER_ROLE::InnerCopper8,

        GERBVIEW_LAYER_ROLE::BottomCopper,     GERBVIEW_LAYER_ROLE::BottomSilkScreen,
        GERBVIEW_LAYER_ROLE::BottomSolderMask, GERBVIEW_LAYER_ROLE::BottomSolderPaste,

        GERBVIEW_LAYER_ROLE::DrillPlated,      GERBVIEW_LAYER_ROLE::DrillUnplated,
        GERBVIEW_LAYER_ROLE::ContourPlated,    GERBVIEW_LAYER_ROLE::ContourUnplated,
    };

    return all;
}


bool IsInnerCopperRole( GERBVIEW_LAYER_ROLE aRole )
{
    return InnerCopperIndex( aRole ) != 0;
}


int InnerCopperIndex( GERBVIEW_LAYER_ROLE aRole )
{
    switch( aRole )
    {
    case GERBVIEW_LAYER_ROLE::InnerCopper1: return 1;
    case GERBVIEW_LAYER_ROLE::InnerCopper2: return 2;
    case GERBVIEW_LAYER_ROLE::InnerCopper3: return 3;
    case GERBVIEW_LAYER_ROLE::InnerCopper4: return 4;
    case GERBVIEW_LAYER_ROLE::InnerCopper5: return 5;
    case GERBVIEW_LAYER_ROLE::InnerCopper6: return 6;
    case GERBVIEW_LAYER_ROLE::InnerCopper7: return 7;
    case GERBVIEW_LAYER_ROLE::InnerCopper8: return 8;
    default: return 0;
    }
}


GERBVIEW_LAYER_ROLE InnerCopperRoleAt( int aIndex )
{
    switch( aIndex )
    {
    case 1: return GERBVIEW_LAYER_ROLE::InnerCopper1;
    case 2: return GERBVIEW_LAYER_ROLE::InnerCopper2;
    case 3: return GERBVIEW_LAYER_ROLE::InnerCopper3;
    case 4: return GERBVIEW_LAYER_ROLE::InnerCopper4;
    case 5: return GERBVIEW_LAYER_ROLE::InnerCopper5;
    case 6: return GERBVIEW_LAYER_ROLE::InnerCopper6;
    case 7: return GERBVIEW_LAYER_ROLE::InnerCopper7;
    case 8: return GERBVIEW_LAYER_ROLE::InnerCopper8;
    default: return GERBVIEW_LAYER_ROLE::InnerCopper1;
    }
}


/**
 * Build a normalized lookup key from a Gerber/Excellon file path: basename only,
 * lower-cased on platforms with case-insensitive file systems.
 */
static wxString normalizeAssignmentKey( const wxString& aFullPath )
{
    wxString base = wxFileName( aFullPath ).GetFullName();

#ifdef __WINDOWS__
    base.MakeLower();
#endif

    return base;
}


/**
 * Resolve the effective color for a role: user override if set, else built-in default.
 */
static KIGFX::COLOR4D resolveColorForRole( const GERBVIEW_SETTINGS* aCfg, GERBVIEW_LAYER_ROLE aRole )
{
    if( aCfg )
    {
        auto it = aCfg->m_LayerRoles.colors.find( RoleSerializedKey( aRole ) );

        if( it != aCfg->m_LayerRoles.colors.end() )
            return it->second;
    }

    return DefaultColorFor( aRole );
}


void ApplyLayerRoleToSlot( GERBVIEW_FRAME* aFrame, int aSlot, GERBVIEW_LAYER_ROLE aRole )
{
    if( !aFrame || aRole == GERBVIEW_LAYER_ROLE::Unassigned )
        return;

    KIGFX::COLOR4D color = resolveColorForRole( aFrame->gvconfig(), aRole );

    aFrame->SetLayerColor( GERBER_DRAW_LAYER( aSlot ), color );

    if( COLOR_SETTINGS* cs = aFrame->GetColorSettings() )
        cs->SetColor( GERBER_DRAW_LAYER( aSlot ), color );

    if( KIGFX::VIEW* view = aFrame->GetCanvas() ? aFrame->GetCanvas()->GetView() : nullptr )
    {
        if( auto* settings = view->GetPainter() ? view->GetPainter()->GetSettings() : nullptr )
            settings->LoadColors( aFrame->GetColorSettings() );

        view->UpdateLayerColor( GERBER_DRAW_LAYER( aSlot ) );
    }
}


GERBVIEW_LAYER_ROLE LookupRoleForSlot( GERBVIEW_FRAME* aFrame, int aSlot )
{
    if( !aFrame )
        return GERBVIEW_LAYER_ROLE::Unassigned;

    GERBVIEW_SETTINGS* cfg = aFrame->gvconfig();

    if( !cfg )
        return GERBVIEW_LAYER_ROLE::Unassigned;

    GERBER_FILE_IMAGE* img = aFrame->GetGbrImage( aSlot );

    if( !img )
        return GERBVIEW_LAYER_ROLE::Unassigned;

    wxString key = normalizeAssignmentKey( img->m_FileName );

    auto it = cfg->m_LayerRoles.assignments.find( key );

    if( it == cfg->m_LayerRoles.assignments.end() )
        return GERBVIEW_LAYER_ROLE::Unassigned;

    return it->second;
}


void SetRoleForSlot( GERBVIEW_FRAME* aFrame, int aSlot, GERBVIEW_LAYER_ROLE aRole )
{
    if( !aFrame )
        return;

    GERBVIEW_SETTINGS* cfg = aFrame->gvconfig();

    if( !cfg )
        return;

    GERBER_FILE_IMAGE* img = aFrame->GetGbrImage( aSlot );

    if( !img )
        return;

    wxString key = normalizeAssignmentKey( img->m_FileName );

    if( aRole == GERBVIEW_LAYER_ROLE::Unassigned )
        cfg->m_LayerRoles.assignments.erase( key );
    else
        cfg->m_LayerRoles.assignments[key] = aRole;

    ApplyLayerRoleToSlot( aFrame, aSlot, aRole );
}


void ReapplyAllLayerRoles( GERBVIEW_FRAME* aFrame )
{
    if( !aFrame )
        return;

    GERBER_FILE_IMAGE_LIST* list = aFrame->GetImagesList();

    if( !list )
        return;

    bool anyApplied = false;

    for( unsigned slot = 0; slot < list->ImagesMaxCount(); ++slot )
    {
        GERBER_FILE_IMAGE* img = list->GetGbrImage( slot );

        if( !img )
            continue;

        GERBVIEW_LAYER_ROLE role = LookupRoleForSlot( aFrame, slot );

        if( role == GERBVIEW_LAYER_ROLE::Unassigned )
            continue;

        ApplyLayerRoleToSlot( aFrame, slot, role );
        anyApplied = true;
    }

    if( anyApplied )
    {
        // Reorder the slots to match the role grouping (drill on top, then
        // top side, inner copper, bottom side, unmapped at the tail) so the
        // canvas Z-stack mirrors the Layer Manager.  SortLayersByLayerRoles
        // already does a ReFillLayerWidget() internally via RemapLayers().
        aFrame->SortLayersByLayerRoles();

        if( aFrame->GetCanvas() )
            aFrame->GetCanvas()->Refresh();
    }
}
