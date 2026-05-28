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

#ifndef GERBVIEW_LAYER_ROLES_H
#define GERBVIEW_LAYER_ROLES_H

#include <vector>

#include <gal/color4d.h>
#include <wx/string.h>

class GERBVIEW_FRAME;


/// Maximum number of inner copper layers exposed by the layer-roles UI.
constexpr int MAX_INNER_COPPER_LAYERS = 8;


/**
 * Logical PCB-layer roles that a loaded Gerber/Excellon file may represent.
 *
 * Assignments are user-driven (see DIALOG_LAYER_ROLES).  When a slot is assigned
 * a role, the corresponding predefined color from the palette is applied to that
 * slot through the regular GerbView color path.
 *
 * Persisted JSON keys are stable (see RoleSerializedKey) so older settings remain
 * loadable after enum reordering.  The deprecated key "inner_copper" is migrated
 * to InnerCopper1 on load.
 */
enum class GERBVIEW_LAYER_ROLE
{
    Unassigned = 0,

    TopSolderPaste,
    TopSolderMask,
    TopSilkScreen,
    TopCopper,

    InnerCopper1,
    InnerCopper2,
    InnerCopper3,
    InnerCopper4,
    InnerCopper5,
    InnerCopper6,
    InnerCopper7,
    InnerCopper8,

    BottomCopper,
    BottomSilkScreen,
    BottomSolderMask,
    BottomSolderPaste,

    DrillPlated,
    DrillUnplated,
    ContourPlated,
    ContourUnplated
};


/// Localized display name for the role (e.g. "Top Copper", "Inner Copper 3").
wxString RoleDisplayName( GERBVIEW_LAYER_ROLE aRole );

/// Compact three-letter code shown in the layer manager next to the visibility
/// checkbox (e.g. "SPT" for Top Solder Paste, "IN1" for Inner Copper 1).
/// Returns an empty string for Unassigned.
wxString RoleShortCode( GERBVIEW_LAYER_ROLE aRole );

/// Stable, case-sensitive JSON key (e.g. "top_copper", "inner_copper_3").  Do not localize.
const char* RoleSerializedKey( GERBVIEW_LAYER_ROLE aRole );

/// Parse a stable JSON key into a role; falls back to Unassigned for unknown values.
/// Recognises the legacy key "inner_copper" as InnerCopper1.
GERBVIEW_LAYER_ROLE RoleFromSerializedKey( const std::string& aKey );

/// Built-in predefined color for the given role.
KIGFX::COLOR4D DefaultColorFor( GERBVIEW_LAYER_ROLE aRole );

/// Every role, ordered for UI presentation (Unassigned first).
const std::vector<GERBVIEW_LAYER_ROLE>& AllRoles();

/// True if @p aRole is one of InnerCopper1..MAX_INNER_COPPER_LAYERS.
bool IsInnerCopperRole( GERBVIEW_LAYER_ROLE aRole );

/// 1-based inner-copper layer index for @p aRole, or 0 if it is not an inner copper role.
int InnerCopperIndex( GERBVIEW_LAYER_ROLE aRole );

/// Inner copper role for @p aIndex (1..MAX_INNER_COPPER_LAYERS); InnerCopper1 if out of range.
GERBVIEW_LAYER_ROLE InnerCopperRoleAt( int aIndex );


/**
 * Apply the configured role color to a single Gerber/drill slot.
 *
 * Looks the color up in GERBVIEW_SETTINGS::m_LayerRoles.colors first; falls back to
 * the built-in default if no override has been set.  Also pushes the color into the
 * shared COLOR_SETTINGS (under the correct GERBER_DRAW_LAYER(slot) key) so the change
 * persists, reloads the painter colors and refreshes the relevant view layer.
 *
 * If @p aRole is Unassigned, the slot color is left untouched.
 */
void ApplyLayerRoleToSlot( GERBVIEW_FRAME* aFrame, int aSlot, GERBVIEW_LAYER_ROLE aRole );


/**
 * Walk every loaded slot, look up its current assignment by file basename in
 * GERBVIEW_SETTINGS::m_LayerRoles.assignments, and re-apply the role color.
 *
 * Safe to call after any number of file loads / re-orders.
 */
void ReapplyAllLayerRoles( GERBVIEW_FRAME* aFrame );


/**
 * Resolve the role currently assigned to the file occupying @p aSlot, or Unassigned.
 *
 * Helper for the layer-roles dialog; uses GERBVIEW_SETTINGS::m_LayerRoles.assignments
 * keyed by the file's basename (case-insensitive on Windows).
 */
GERBVIEW_LAYER_ROLE LookupRoleForSlot( GERBVIEW_FRAME* aFrame, int aSlot );


/**
 * Persist a new role assignment for the file currently loaded at @p aSlot and
 * apply the corresponding color.  Setting @p aRole to Unassigned removes the
 * mapping (the slot keeps its current color until the next ReFill).
 */
void SetRoleForSlot( GERBVIEW_FRAME* aFrame, int aSlot, GERBVIEW_LAYER_ROLE aRole );


#endif // GERBVIEW_LAYER_ROLES_H
