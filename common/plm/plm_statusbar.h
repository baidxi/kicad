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
 * @file plm_statusbar.h
 * @brief PLM status bar integration helper.
 */

#ifndef PLM_STATUSBAR_H
#define PLM_STATUSBAR_H

#include <kicommon.h>
#include <plm/plm_icon_provider.h>

#include <wx/statusbr.h>

#include <functional>


/**
 * PLM status bar field indices.
 *
 * These are offsets from a base field index provided by the application.
 */
enum PLM_STATUSBAR_FIELD
{
    PLM_SB_CONNECTION = 0,  ///< Connection status field
    PLM_SB_LOCK,            ///< Lock status field
    PLM_SB_SYNC,            ///< Sync status field

    PLM_SB_FIELD_COUNT      ///< Number of PLM fields
};


/**
 * PLM status bar click callback.
 */
using PLM_STATUSBAR_CLICK_CALLBACK = std::function<void( PLM_STATUSBAR_FIELD aField )>;


/**
 * PLM status bar state.
 */
struct PLM_STATUSBAR_STATE
{
    bool connected = false;                             ///< Connected to PLM server
    wxString server_name;                               ///< Server name/URL
    PLM_ICON_STATE lock_state = PLM_ICON_STATE::UNMANAGED;  ///< Lock state
    wxString lock_owner;                                ///< Lock owner if locked by other
    PLM_ICON_STATE sync_state = PLM_ICON_STATE::UNMANAGED;  ///< Sync state
};


/**
 * Helper class for managing PLM status bar fields.
 *
 * Provides utilities to update and manage PLM-related status bar fields
 * in the KiCad Project Manager status bar.
 */
class KICOMMON_API PLM_STATUSBAR
{
public:
    PLM_STATUSBAR();
    ~PLM_STATUSBAR();

    /**
     * Update PLM status bar fields.
     *
     * @param aStatusBar Status bar to update.
     * @param aBaseField Base field index for PLM fields.
     * @param aState Current PLM state.
     */
    void UpdateFields( wxStatusBar* aStatusBar, int aBaseField,
                       const PLM_STATUSBAR_STATE& aState );

    /**
     * Get text for a status bar field.
     *
     * @param aField PLM field.
     * @param aState Current PLM state.
     * @return Text to display in field.
     */
    wxString GetFieldText( PLM_STATUSBAR_FIELD aField, const PLM_STATUSBAR_STATE& aState ) const;

    /**
     * Get tooltip for a status bar field.
     *
     * @param aField PLM field.
     * @param aState Current PLM state.
     * @return Tooltip text.
     */
    wxString GetFieldTooltip( PLM_STATUSBAR_FIELD aField,
                               const PLM_STATUSBAR_STATE& aState ) const;

    /**
     * Get suggested field widths for PLM fields.
     *
     * @return Array of field widths.
     */
    static std::vector<int> GetFieldWidths();

    /**
     * Set callback for field clicks.
     */
    void SetClickCallback( PLM_STATUSBAR_CLICK_CALLBACK aCallback ) { m_clickCb = aCallback; }

    /**
     * Handle a click on a status bar field.
     *
     * @param aBaseField Base field index for PLM fields.
     * @param aClickedField Field that was clicked.
     */
    void HandleClick( int aBaseField, int aClickedField );

    /**
     * Check if PLM status bar should be shown.
     *
     * @return True if PLM integration is enabled.
     */
    static bool ShouldShow();

private:
    PLM_STATUSBAR_CLICK_CALLBACK m_clickCb;
};


#endif // PLM_STATUSBAR_H
