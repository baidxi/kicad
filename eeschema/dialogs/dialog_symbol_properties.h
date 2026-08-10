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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#ifndef DIALOG_SYMBOL_PROPERTIES_H
#define DIALOG_SYMBOL_PROPERTIES_H

#include <dialog_symbol_properties_base.h>
#include <fields_grid_table.h>
#include <sch_pin.h>


class LIB_SYMBOL;
class SCH_PIN_TABLE_DATA_MODEL;
class PIN_FUNCTION_GROUP_DATA_MODEL;
class SCH_EDIT_FRAME;
class PANEL_EMBEDDED_FILES;
class PANEL_SYMBOL_PIN_MAP;


// The dialog can be closed for several reasons.
enum SYMBOL_PROPS_RETVALUE
{
    SYMBOL_PROPS_WANT_UPDATE_SYMBOL,
    SYMBOL_PROPS_WANT_EXCHANGE_SYMBOL,
    SYMBOL_PROPS_WANT_SET_VARIANT_SYMBOL,
    SYMBOL_PROPS_WANT_CLEAR_VARIANT_SYMBOL,
    SYMBOL_PROPS_EDIT_OK,
    SYMBOL_PROPS_EDIT_SCHEMATIC_SYMBOL,
    SYMBOL_PROPS_EDIT_LIBRARY_SYMBOL
};


/**
 * Dialog used to edit #SCH_SYMBOL objects in a schematic.
 *
 * This is derived from DIALOG_SYMBOL_PROPERTIES_BASE which is maintained by
 * wxFormBuilder.
 */
class DIALOG_SYMBOL_PROPERTIES : public DIALOG_SYMBOL_PROPERTIES_BASE
{
public:
    DIALOG_SYMBOL_PROPERTIES( SCH_EDIT_FRAME* aParent, SCH_SYMBOL* aSymbol );
    ~DIALOG_SYMBOL_PROPERTIES() override;

    SCH_EDIT_FRAME* GetParent();

    /**
     * Select the Pin Map page so the dialog opens directly on the pin-to-pad mapping editor
     * (issue #2282).  Call before showing the dialog.
     */
    void SelectPinMapPage();

private:
    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;

    bool Validate() override;

    // event handlers
    void OnAddField( wxCommandEvent& event ) override;
    void OnDeleteField( wxCommandEvent& event ) override;
    void OnMoveUp( wxCommandEvent& event ) override;
    void OnMoveDown( wxCommandEvent& event ) override;
    void OnEditSpiceModel( wxCommandEvent& event ) override;
    void OnPinTableColSort( wxGridEvent& aEvent );
    void OnPinTableCellEdited( wxGridEvent& event ) override;
    void OnSizePinsGrid( wxSizeEvent& event ) override;
    void OnGridCellChanging( wxGridEvent& event );
    void OnUpdateUI( wxUpdateUIEvent& event ) override;
    void OnCancelButtonClick( wxCommandEvent& event ) override;
    void OnGridEditorShown( wxGridEvent& event ) override;
    void OnGridEditorHidden( wxGridEvent& event ) override;
    void OnUnitChoice( wxCommandEvent& event ) override;
    void OnCheckBox( wxCommandEvent& event ) override;
    void OnPageChanging( wxNotebookEvent& event ) override;

    void OnEditSymbol( wxCommandEvent&  ) override;
    void OnEditLibrarySymbol( wxCommandEvent&  ) override;
    void OnUpdateSymbol( wxCommandEvent&  ) override;
    void OnExchangeSymbol( wxCommandEvent&  ) override;
    void OnClearVariantSymbol( wxCommandEvent& ) override;

    void AdjustPinsGridColumns();
    void HandleDelayedFocus( wxCommandEvent& event );
    void HandleDelayedSelection( wxCommandEvent& event );

    void OnGroupGridCellClicked( wxGridEvent& aEvent );
    void RebuildGroupGrid( int aUnit );
    /// Apply the group grid's active flags to the pin assignments.  MUTATING: assigns required
    /// members, tops each subgroup up to its minimum, and clears alternates of deactivated
    /// groups.  Call only in response to a user toggle, never from a refresh.
    void ApplyGroupActivation();

    /// Assign additional subgroup members until each active group's subgroups meet their
    /// minimum, without disturbing choices the user already made.
    void TopUpSubgroups( const std::set<wxString>& aActiveGroups );

    /// Recompute which pins active groups hold, and rebuild the pin grid attributes.
    /// NON-MUTATING: safe to call on every refresh.
    void RefreshGroupStatus();

    /// Assign or free a single group member from its child row in the group grid.
    void ToggleGroupMember( const struct PIN_FUNCTION_GROUP_ROW& aGroup,
                            const struct PIN_FUNCTION_GROUP_MEMBER& aMember );

    /// Re-fit the group grid's fixed height after its visible row count changes.
    void sizeGroupGrid();

    virtual void onUpdateEditSymbol( wxUpdateUIEvent& event ) override;
    virtual void onUpdateEditLibrarySymbol( wxUpdateUIEvent& event ) override;

private:
    SCH_SYMBOL*                        m_symbol;
    LIB_SYMBOL*                        m_part;

    wxSize                             m_pinsSize;
    wxSize                             m_lastRequestedPinsSize;
    bool                               m_editorShown;
    std::bitset<64>                    m_shownColumns;

    FIELDS_GRID_TABLE*             m_fields;
    SCH_PIN_TABLE_DATA_MODEL*      m_dataModel;
    WX_GRID*                       m_groupGrid;
    PIN_FUNCTION_GROUP_DATA_MODEL* m_groupDataModel;
    PANEL_EMBEDDED_FILES*          m_embeddedFiles;
    PANEL_SYMBOL_PIN_MAP*          m_pinMapPanel;
    bool                           m_forcePinMapPage = false;
};

#endif // DIALOG_SYMBOL_PROPERTIES_H
