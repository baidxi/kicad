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

#ifndef DIALOG_LAYER_ROLES_H
#define DIALOG_LAYER_ROLES_H

#include <map>
#include <string>
#include <vector>

#include <dialog_shim.h>
#include <gal/color4d.h>
#include <layer_roles.h>

class GERBVIEW_FRAME;
class wxButton;
class wxColourPickerCtrl;
class wxGrid;
class wxScrolledWindow;
class wxSizer;
class wxStaticText;


/**
 * Modeless dialog that lets the user manually map every loaded Gerber/Excellon file to
 * a PCB layer role, and tweak the per-role color palette.
 *
 * Layout:
 *   - Left column:  file -> role assignment grid.
 *   - Right column: grouped role colors (Top / Inner / Bottom / Other),
 *                   "loaded scheme" label, and Import / Export / Reset buttons.
 *   - Below both columns: Apply / OK / Cancel.
 *
 * State lives entirely in the dialog while it is open.  On OK / Apply the contents are
 * written back to GERBVIEW_SETTINGS::m_LayerRoles and pushed to the painter through
 * ReapplyAllLayerRoles().  Import / Export use a small JSON file with stable keys so
 * configurations can be shared between users.
 */
class DIALOG_LAYER_ROLES : public DIALOG_SHIM
{
public:
    DIALOG_LAYER_ROLES( GERBVIEW_FRAME* aParent );

    /// Bypass DIALOG_SHIM::Show(false), which runs SaveControlState() +
    /// focusParentCanvas(); the recursive child traversal in SaveControlState
    /// has been observed to deadlock for the wxColourPickerCtrl + wxGrid +
    /// wxStaticBox mix used by this dialog on Windows.  We do not rely on
    /// DIALOG_SHIM's generic geometry persistence because we keep our own
    /// state in GERBVIEW_SETTINGS::m_LayerRoles.
    bool Show( bool show ) override;

private:
    void buildAssignmentsGrid();

    /// Build (or rebuild) the right-hand color column based on m_visibleInnerCount.
    void buildColorPanel();

    /// Add one labeled color picker row to the given sizer for @p aRole.
    /// @p aDisplayLabel allows overriding the role's default name (e.g. dropping
    /// the "Top"/"Bottom" prefix in the section headers).
    void addColorRow( wxSizer* aSizer, GERBVIEW_LAYER_ROLE aRole, const wxString& aDisplayLabel );

    /// Push the current loaded-scheme name into m_schemeLabel.
    void updateSchemeLabel();

    /// Tear down and rebuild the color section (used after Add / Remove inner layer).
    void rebuildColorSection();

    void onApply( wxCommandEvent& aEvent );
    void onImport( wxCommandEvent& aEvent );
    void onExport( wxCommandEvent& aEvent );
    void onResetColors( wxCommandEvent& aEvent );
    void onOkButton( wxCommandEvent& aEvent );
    void onCancelButton( wxCommandEvent& aEvent );
    void onAddInnerLayer( wxCommandEvent& aEvent );
    void onRemoveInnerLayer( wxCommandEvent& aEvent );

    /// Convert the role display-name shown in the dropdown back to the enum.
    GERBVIEW_LAYER_ROLE roleFromDisplayName( const wxString& aName ) const;

    /// Pull the dialog state into the application's GERBVIEW_SETTINGS.
    void commitToSettings();

    /// Queue ReapplyAllLayerRoles() on the parent frame so the heavy AUI /
    /// toolbar work happens AFTER the current event handler has returned.
    void scheduleReapply();

    /// Refill all color pickers from m_workingColors.
    void refreshColorPickers();

    GERBVIEW_FRAME*   m_frame;
    wxGrid*           m_assignmentsGrid;
    wxScrolledWindow* m_colorPanel;
    wxStaticText*     m_schemeLabel;
    wxButton*         m_addInnerBtn;
    wxButton*         m_removeInnerBtn;

    std::vector<int>                      m_slotIndices;  // grid row -> slot
    std::vector<wxColourPickerCtrl*>      m_colorPickers; // index matches m_coloredRoles
    std::vector<GERBVIEW_LAYER_ROLE>      m_coloredRoles; // roles displayed in color panel
    std::map<std::string, KIGFX::COLOR4D> m_workingColors;

    /// 2..MAX_INNER_COPPER_LAYERS, controls how many Inner Copper rows are visible.
    int m_visibleInnerCount;

    /// Empty if the palette has not been imported (or was modified after import).
    wxString m_loadedSchemeName;
};


#endif // DIALOG_LAYER_ROLES_H
