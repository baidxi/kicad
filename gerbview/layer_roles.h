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


/**
 * Logical PCB-layer roles that a loaded Gerber/Excellon file may represent.
 *
 * Assignments are user-driven (see DIALOG_LAYER_ROLES).  When a slot is assigned
 * a role, the corresponding predefined color from the palette is applied to that
 * slot through the regular GerbView color path.
 *
 * Persisted JSON keys are stable (see RoleSerializedKey) so older settings remain
 * loadable after enum reordering.
 */
enum class GERBVIEW_LAYER_ROLE
{
    Unassigned = 0,

    TopCopper,
    BottomCopper,
    InnerCopper,

    TopSolderMask,
    BottomSolderMask,

    TopSolderPaste,
    BottomSolderPaste,

    TopSilkScreen,
    BottomSilkScreen,

    DrillPlated,
    DrillUnplated
};


/// Localized display name for the role (e.g. "Top Copper").
wxString RoleDisplayName( GERBVIEW_LAYER_ROLE aRole );

/// Stable, case-sensitive JSON key (e.g. "top_copper").  Do not localize.
const char* RoleSerializedKey( GERBVIEW_LAYER_ROLE aRole );

/// Parse a stable JSON key into a role; falls back to Unassigned for unknown values.
GERBVIEW_LAYER_ROLE RoleFromSerializedKey( const std::string& aKey );

/// Built-in predefined color for the given role.
KIGFX::COLOR4D DefaultColorFor( GERBVIEW_LAYER_ROLE aRole );

/// Every role, ordered for UI presentation (Unassigned first).
const std::vector<GERBVIEW_LAYER_ROLE>& AllRoles();


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


#endif // GERBVIEW_LAYER_ROLES_H
