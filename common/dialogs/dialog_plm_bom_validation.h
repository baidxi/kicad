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

/**
 * @file dialog_plm_bom_validation.h
 * @brief Dialog for validating BOM items against PLM part database.
 */

#ifndef DIALOG_PLM_BOM_VALIDATION_H
#define DIALOG_PLM_BOM_VALIDATION_H

#include "dialog_plm_bom_validation_base.h"

#include <vector>


/**
 * Status of a BOM item after PLM validation.
 */
enum class PLM_BOM_STATUS
{
    VALID,          // Part exists in PLM and matches
    MISSING,        // Part not found in PLM database
    OBSOLETE,       // Part is marked as obsolete
    MISMATCH        // Value or footprint doesn't match PLM data
};


/**
 * Represents a BOM item with its validation status.
 */
struct PLM_BOM_ITEM
{
    wxString       reference;               // Component reference (e.g., "R1", "U3")
    wxString       value;                   // Component value (e.g., "10K", "LM7805")
    wxString       footprint;               // Component footprint
    PLM_BOM_STATUS status;                  // Validation status
    wxString       plm_part_number;         // PLM part number if found
    wxString       suggested_replacement;   // Suggested replacement for obsolete parts
    bool           accept_suggestion = false; // Whether suggestion has been accepted
};


/**
 * Dialog for displaying BOM validation results against PLM database.
 *
 * Shows validation status for each component, allows accepting replacement suggestions,
 * and provides summary statistics.
 */
class DIALOG_PLM_BOM_VALIDATION : public DIALOG_PLM_BOM_VALIDATION_BASE
{
public:
    DIALOG_PLM_BOM_VALIDATION( wxWindow* aParent );
    ~DIALOG_PLM_BOM_VALIDATION();

    /**
     * Add a BOM item to the validation list.
     *
     * @param aItem The BOM item with validation results to add.
     */
    void AddBomItem( const PLM_BOM_ITEM& aItem );

    /**
     * Get all BOM items where suggestions were accepted.
     *
     * @return Vector of BOM items with accept_suggestion = true.
     */
    std::vector<PLM_BOM_ITEM> GetAcceptedSuggestions() const;

    /**
     * Update the summary count labels based on current items.
     */
    void UpdateCounts();

protected:
    void onBomItemSelected( wxDataViewEvent& event ) override;
    void onAcceptAll( wxCommandEvent& event ) override;
    void onAcceptSelected( wxCommandEvent& event ) override;

private:
    /**
     * Get display text for a status enum value.
     *
     * @param aStatus The status to convert to text.
     * @return Translated display string for the status.
     */
    wxString GetStatusText( PLM_BOM_STATUS aStatus ) const;

    /**
     * Update button states based on current selection and item states.
     */
    void updateButtonStates();

    std::vector<PLM_BOM_ITEM> m_items;
};


#endif // DIALOG_PLM_BOM_VALIDATION_H
