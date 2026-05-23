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
    case GERBVIEW_LAYER_ROLE::Unassigned:        return _( "Unassigned" );
    case GERBVIEW_LAYER_ROLE::TopCopper:         return _( "Top Copper" );
    case GERBVIEW_LAYER_ROLE::BottomCopper:      return _( "Bottom Copper" );
    case GERBVIEW_LAYER_ROLE::InnerCopper:       return _( "Inner Copper" );
    case GERBVIEW_LAYER_ROLE::TopSolderMask:     return _( "Top Solder Mask" );
    case GERBVIEW_LAYER_ROLE::BottomSolderMask:  return _( "Bottom Solder Mask" );
    case GERBVIEW_LAYER_ROLE::TopSolderPaste:    return _( "Top Solder Paste" );
    case GERBVIEW_LAYER_ROLE::BottomSolderPaste: return _( "Bottom Solder Paste" );
    case GERBVIEW_LAYER_ROLE::TopSilkScreen:     return _( "Top Silk Screen" );
    case GERBVIEW_LAYER_ROLE::BottomSilkScreen:  return _( "Bottom Silk Screen" );
    case GERBVIEW_LAYER_ROLE::DrillPlated:       return _( "Drill (Plated)" );
    case GERBVIEW_LAYER_ROLE::DrillUnplated:     return _( "Drill (Unplated)" );
    }

    return _( "Unassigned" );
}


const char* RoleSerializedKey( GERBVIEW_LAYER_ROLE aRole )
{
    switch( aRole )
    {
    case GERBVIEW_LAYER_ROLE::Unassigned:        return "unassigned";
    case GERBVIEW_LAYER_ROLE::TopCopper:         return "top_copper";
    case GERBVIEW_LAYER_ROLE::BottomCopper:      return "bottom_copper";
    case GERBVIEW_LAYER_ROLE::InnerCopper:       return "inner_copper";
    case GERBVIEW_LAYER_ROLE::TopSolderMask:     return "top_solder_mask";
    case GERBVIEW_LAYER_ROLE::BottomSolderMask:  return "bottom_solder_mask";
    case GERBVIEW_LAYER_ROLE::TopSolderPaste:    return "top_solder_paste";
    case GERBVIEW_LAYER_ROLE::BottomSolderPaste: return "bottom_solder_paste";
    case GERBVIEW_LAYER_ROLE::TopSilkScreen:     return "top_silk_screen";
    case GERBVIEW_LAYER_ROLE::BottomSilkScreen:  return "bottom_silk_screen";
    case GERBVIEW_LAYER_ROLE::DrillPlated:       return "drill_plated";
    case GERBVIEW_LAYER_ROLE::DrillUnplated:     return "drill_unplated";
    }

    return "unassigned";
}


GERBVIEW_LAYER_ROLE RoleFromSerializedKey( const std::string& aKey )
{
    static const std::unordered_map<std::string, GERBVIEW_LAYER_ROLE> lookup = {
            { "unassigned",          GERBVIEW_LAYER_ROLE::Unassigned        },
            { "top_copper",          GERBVIEW_LAYER_ROLE::TopCopper         },
            { "bottom_copper",       GERBVIEW_LAYER_ROLE::BottomCopper      },
            { "inner_copper",        GERBVIEW_LAYER_ROLE::InnerCopper       },
            { "top_solder_mask",     GERBVIEW_LAYER_ROLE::TopSolderMask     },
            { "bottom_solder_mask",  GERBVIEW_LAYER_ROLE::BottomSolderMask  },
            { "top_solder_paste",    GERBVIEW_LAYER_ROLE::TopSolderPaste    },
            { "bottom_solder_paste", GERBVIEW_LAYER_ROLE::BottomSolderPaste },
            { "top_silk_screen",     GERBVIEW_LAYER_ROLE::TopSilkScreen     },
            { "bottom_silk_screen",  GERBVIEW_LAYER_ROLE::BottomSilkScreen  },
            { "drill_plated",        GERBVIEW_LAYER_ROLE::DrillPlated       },
            { "drill_unplated",      GERBVIEW_LAYER_ROLE::DrillUnplated     },
    };

    auto it = lookup.find( aKey );
    return it == lookup.end() ? GERBVIEW_LAYER_ROLE::Unassigned : it->second;
}


KIGFX::COLOR4D DefaultColorFor( GERBVIEW_LAYER_ROLE aRole )
{
    // Saturated, visible on the typical dark GerbView background.  Values in 0..1 floats.
    switch( aRole )
    {
    case GERBVIEW_LAYER_ROLE::TopCopper:         return KIGFX::COLOR4D( 0.86, 0.62, 0.20, 1.0 );
    case GERBVIEW_LAYER_ROLE::BottomCopper:      return KIGFX::COLOR4D( 0.65, 0.42, 0.10, 1.0 );
    case GERBVIEW_LAYER_ROLE::InnerCopper:       return KIGFX::COLOR4D( 0.75, 0.55, 0.30, 1.0 );

    case GERBVIEW_LAYER_ROLE::TopSolderMask:     return KIGFX::COLOR4D( 0.16, 0.52, 0.27, 1.0 );
    case GERBVIEW_LAYER_ROLE::BottomSolderMask:  return KIGFX::COLOR4D( 0.10, 0.38, 0.20, 1.0 );

    case GERBVIEW_LAYER_ROLE::TopSolderPaste:    return KIGFX::COLOR4D( 0.80, 0.80, 0.82, 1.0 );
    case GERBVIEW_LAYER_ROLE::BottomSolderPaste: return KIGFX::COLOR4D( 0.60, 0.60, 0.62, 1.0 );

    case GERBVIEW_LAYER_ROLE::TopSilkScreen:     return KIGFX::COLOR4D( 0.96, 0.96, 0.92, 1.0 );
    case GERBVIEW_LAYER_ROLE::BottomSilkScreen:  return KIGFX::COLOR4D( 0.82, 0.82, 0.78, 1.0 );

    case GERBVIEW_LAYER_ROLE::DrillPlated:       return KIGFX::COLOR4D( 0.70, 0.75, 0.95, 1.0 );
    case GERBVIEW_LAYER_ROLE::DrillUnplated:     return KIGFX::COLOR4D( 0.38, 0.42, 0.55, 1.0 );

    case GERBVIEW_LAYER_ROLE::Unassigned:
    default:
        return KIGFX::COLOR4D( 1.0, 1.0, 1.0, 1.0 );
    }
}


const std::vector<GERBVIEW_LAYER_ROLE>& AllRoles()
{
    static const std::vector<GERBVIEW_LAYER_ROLE> all = {
            GERBVIEW_LAYER_ROLE::Unassigned,

            GERBVIEW_LAYER_ROLE::TopCopper,
            GERBVIEW_LAYER_ROLE::BottomCopper,
            GERBVIEW_LAYER_ROLE::InnerCopper,

            GERBVIEW_LAYER_ROLE::TopSolderMask,
            GERBVIEW_LAYER_ROLE::BottomSolderMask,

            GERBVIEW_LAYER_ROLE::TopSolderPaste,
            GERBVIEW_LAYER_ROLE::BottomSolderPaste,

            GERBVIEW_LAYER_ROLE::TopSilkScreen,
            GERBVIEW_LAYER_ROLE::BottomSilkScreen,

            GERBVIEW_LAYER_ROLE::DrillPlated,
            GERBVIEW_LAYER_ROLE::DrillUnplated,
    };

    return all;
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
static KIGFX::COLOR4D resolveColorForRole( const GERBVIEW_SETTINGS* aCfg,
                                           GERBVIEW_LAYER_ROLE aRole )
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
        aFrame->ReFillLayerWidget();

        if( aFrame->GetCanvas() )
            aFrame->GetCanvas()->Refresh();
    }
}
