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
class wxColourPickerEvent;
class wxGrid;
class wxGridEvent;
class wxScrolledWindow;


/**
 * Modeless dialog that lets the user manually map every loaded Gerber/Excellon file to
 * a PCB layer role, and tweak the per-role color palette.
 *
 * State lives entirely in the dialog while it is open.  On OK / Apply the contents are
 * written back to GERBVIEW_SETTINGS::m_LayerRoles and pushed to the painter through
 * ReapplyAllLayerRoles().  Import / Export use a small JSON file with two top-level
 * keys ("colors" and "assignments") so configurations can be shared between users.
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
    void buildColorPanel();

    void onApply( wxCommandEvent& aEvent );
    void onImport( wxCommandEvent& aEvent );
    void onExport( wxCommandEvent& aEvent );
    void onResetColors( wxCommandEvent& aEvent );
    void onColorPicked( wxColourPickerEvent& aEvent );
    void onOkButton( wxCommandEvent& aEvent );
    void onCancelButton( wxCommandEvent& aEvent );

    /// Convert the role display-name shown in the dropdown back to the enum.
    GERBVIEW_LAYER_ROLE roleFromDisplayName( const wxString& aName ) const;

    /// Pull the dialog state into the application's GERBVIEW_SETTINGS.
    void commitToSettings();

    /// Queue ReapplyAllLayerRoles() on the parent frame so the heavy AUI /
    /// toolbar work happens AFTER the current event handler has returned.
    void scheduleReapply();

    /// Refill all color pickers from m_workingColors.
    void refreshColorPickers();

    GERBVIEW_FRAME*                            m_frame;
    wxGrid*                                    m_assignmentsGrid;
    wxScrolledWindow*                          m_colorPanel;
    std::vector<int>                           m_slotIndices;       // grid row -> slot
    std::vector<wxColourPickerCtrl*>           m_colorPickers;      // index matches m_coloredRoles
    std::vector<GERBVIEW_LAYER_ROLE>           m_coloredRoles;      // roles displayed in color panel
    std::map<std::string, KIGFX::COLOR4D>      m_workingColors;
};


#endif // DIALOG_LAYER_ROLES_H
