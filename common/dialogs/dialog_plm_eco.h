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

#ifndef DIALOG_PLM_ECO_H
#define DIALOG_PLM_ECO_H

#include "dialog_plm_eco_base.h"
#include <plm/plm_eco.h>
#include <plm/plm_plugin.h>
#include <vector>
#include <map>

class wxStaticText;
class wxWindow;
class wxTextCtrl;
class wxChoice;
class wxCheckBox;
class wxButton;
class wxPanel;
class wxStaticBoxSizer;


/**
 * Dialog for ECO selection during check-in and ECO creation/management.
 *
 * Operates in two modes:
 * - Selection mode: choose an existing ECO and fill required fields
 * - Creation mode: create a new ECR/ECO with full metadata
 *
 * Also supports status transitions (approve, reject) and impact editing
 * on selected ECOs when a PLM_PLUGIN is provided.
 */
class DIALOG_PLM_ECO : public DIALOG_PLM_ECO_BASE
{
public:
    /**
     * Construct the ECO dialog in selection mode.
     *
     * @param aParent Parent window.
     */
    DIALOG_PLM_ECO( wxWindow* aParent );

    /**
     * Construct the ECO dialog with plugin access for creation and status transitions.
     *
     * @param aParent Parent window.
     * @param aPlugin PLM plugin for ECO operations.
     */
    DIALOG_PLM_ECO( wxWindow* aParent, PLM_PLUGIN* aPlugin );

    ~DIALOG_PLM_ECO() override;

    /**
     * Set the list of available ECOs.
     *
     * @param aEcos Vector of ECOs to display.
     */
    void SetEcoList( const std::vector<PLM_ECO>& aEcos );

    /**
     * Get the selected ECO.
     *
     * @return Reference to the selected ECO.
     */
    const PLM_ECO& GetSelectedEco() const;

    /**
     * Get the ECO created in creation mode, if any.
     *
     * @return The newly created ECO, or empty ECO if none was created.
     */
    const PLM_ECO& GetCreatedEco() const { return m_createdEco; }

    /**
     * Check whether an ECO was created during this dialog session.
     */
    bool WasEcoCreated() const { return m_ecoCreated; }

    /**
     * Get field values entered by the user.
     *
     * @return Map of field IDs to values.
     */
    PLM_ECO_FIELD_VALUES GetFieldValues() const;

    /**
     * Validate that all required fields are filled.
     *
     * @return True if validation passes.
     */
    bool ValidateFields();

protected:
    bool TransferDataFromWindow() override;

    void onEcoSelected( wxCommandEvent& event ) override;

private:
    void initCommon();

    /**
     * Build the creation mode panel and status/impact controls.
     */
    void buildExtendedUI();

    /**
     * Switch between selection and creation modes.
     */
    void switchToCreateMode();
    void switchToSelectMode();

    void updateDetails( int aIndex );
    void createFieldControls( const std::vector<PLM_ECO_FIELD>& aFields );
    void clearFieldControls();
    wxString formatDate( int64_t aTimestamp ) const;

    // Event handlers for extended functionality
    void onCreateNewEco( wxCommandEvent& aEvent );
    void onSubmitCreate( wxCommandEvent& aEvent );
    void onCancelCreate( wxCommandEvent& aEvent );
    void onApproveEco( wxCommandEvent& aEvent );
    void onRejectEco( wxCommandEvent& aEvent );
    void onSaveImpact( wxCommandEvent& aEvent );

    /**
     * Update status button visibility based on selected ECO state.
     */
    void updateStatusButtons();

    PLM_PLUGIN*          m_plugin;
    std::vector<PLM_ECO> m_ecos;
    int                  m_selectedIndex;
    bool                 m_createMode;
    bool                 m_ecoCreated;
    PLM_ECO              m_createdEco;

    struct FieldControl
    {
        wxStaticText* label;
        wxWindow*     control;
        PLM_ECO_FIELD field;
    };

    std::vector<FieldControl> m_fieldControls;

    // Creation mode controls
    wxPanel*            m_createPanel;
    wxTextCtrl*         m_createTitleCtrl;
    wxChoice*           m_createTypeChoice;
    wxChoice*           m_createPriorityChoice;
    wxChoice*           m_createReasonChoice;
    wxTextCtrl*         m_createReasonDetailCtrl;
    wxTextCtrl*         m_createDescriptionCtrl;
    wxButton*           m_createNewBtn;
    wxButton*           m_submitCreateBtn;
    wxButton*           m_cancelCreateBtn;

    // Impact assessment controls
    wxPanel*            m_impactPanel;
    wxTextCtrl*         m_costImpactCtrl;
    wxTextCtrl*         m_scheduleImpactCtrl;
    wxTextCtrl*         m_mfgImpactCtrl;
    wxButton*           m_saveImpactBtn;

    // Status transition controls
    wxPanel*            m_statusPanel;
    wxButton*           m_approveBtn;
    wxButton*           m_rejectBtn;
};


#endif // DIALOG_PLM_ECO_H
