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

#ifndef DIALOG_PLM_BOM_H
#define DIALOG_PLM_BOM_H

#include "dialog_plm_bom_base.h"
#include <plm/plm_bom.h>
#include <plm/plm_parts.h>
#include <vector>

class PLM_PLUGIN;


/**
 * BOM management dialog for PLM integration.
 *
 * Provides a full-featured BOM viewer with support for creating, editing,
 * deleting, comparing, and validating Bills of Materials. Supports both
 * Engineering BOM (EBOM) and Manufacturing BOM (MBOM) types.
 */
class DIALOG_PLM_BOM : public DIALOG_PLM_BOM_BASE
{
public:
    DIALOG_PLM_BOM( wxWindow* aParent, PLM_PLUGIN* aPlugin, const wxString& aProductId );
    ~DIALOG_PLM_BOM();

    /**
     * Get the currently displayed BOM.
     */
    const PLM_BOM& GetCurrentBOM() const { return m_currentBom; }

protected:
    void onBomTypeChanged( wxCommandEvent& event ) override;
    void onCreateBom( wxCommandEvent& event ) override;
    void onDeleteBom( wxCommandEvent& event ) override;
    void onDiffBom( wxCommandEvent& event ) override;
    void onValidateBom( wxCommandEvent& event ) override;
    void onBomItemSelected( wxDataViewEvent& event ) override;

private:
    /**
     * Set up the BOM list and validation list columns.
     */
    void setupColumns();

    /**
     * Load the BOM for the current product and type selection.
     */
    void loadBom();

    /**
     * Populate the BOM list control with line items.
     */
    void populateBomList( const PLM_BOM& aBom );

    /**
     * Display validation results in the validation panel.
     */
    void populateValidationResults( const PLM_BOM_VALIDATION_RESULT& aResult );

    /**
     * Clear validation results.
     */
    void clearValidation();

    /**
     * Get the currently selected BOM type from the choice control.
     */
    PLM_BOM_TYPE getSelectedBomType() const;

    /**
     * Format a BOM type for display.
     */
    wxString formatBomType( PLM_BOM_TYPE aType ) const;

    PLM_PLUGIN* m_plugin;
    wxString    m_productId;
    PLM_BOM     m_currentBom;

    std::vector<PLM_BOM> m_availableBoms;

    enum BomColumns
    {
        COL_LINE_NUM = 0,
        COL_REFERENCE,
        COL_PART_NUMBER,
        COL_DESCRIPTION,
        COL_QUANTITY,
        COL_UOM
    };

    enum ValidationColumns
    {
        VCOL_SEVERITY = 0,
        VCOL_REFERENCE,
        VCOL_PART_NUMBER,
        VCOL_MESSAGE,
        VCOL_SUGGESTION
    };
};


#endif // DIALOG_PLM_BOM_H
