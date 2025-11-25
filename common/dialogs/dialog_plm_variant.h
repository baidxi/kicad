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
 * @file dialog_plm_variant.h
 * @brief Dialog for managing PLM product variant configurations.
 */

#ifndef DIALOG_PLM_VARIANT_H
#define DIALOG_PLM_VARIANT_H

#include "dialog_plm_variant_base.h"

#include <plm/plm_variant.h>


class PLM_PLUGIN;


/**
 * Dialog for managing product variant configurations.
 *
 * Provides a split-panel interface for selecting variant configurations
 * and editing per-component BOM overrides.
 */
class DIALOG_PLM_VARIANT : public DIALOG_PLM_VARIANT_BASE
{
public:
    DIALOG_PLM_VARIANT( wxWindow* aParent, PLM_PLUGIN* aPlugin, const wxString& aProjectId );
    ~DIALOG_PLM_VARIANT();

    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;

protected:
    void onVariantSelected( wxCommandEvent& event ) override;
    void onAddVariant( wxCommandEvent& event ) override;
    void onDeleteVariant( wxCommandEvent& event ) override;
    void onAddOverride( wxCommandEvent& event ) override;
    void onRemoveOverride( wxCommandEvent& event ) override;

private:
    void loadVariants();
    void populateVariantList();
    void showVariantDetail( int aIndex );
    void clearDetail();
    void saveCurrentVariant();
    void populateOverridesList( const PLM_VARIANT_CONFIG& aConfig );
    void updateButtonStates();

    wxString getOverrideTypeName( PLM_VARIANT_OVERRIDE_TYPE aType ) const;

    PLM_PLUGIN*     m_plugin;
    wxString        m_projectId;
    PLM_VARIANT     m_variantData;
    int             m_selectedIndex;
};


#endif // DIALOG_PLM_VARIANT_H
