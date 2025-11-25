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
 * @file plm_toolbar.h
 * @brief PLM toolbar integration helper.
 */

#ifndef PLM_TOOLBAR_H
#define PLM_TOOLBAR_H

#include <kicommon.h>
#include <plm/plm_icon_provider.h>

#include <wx/toolbar.h>

#include <functional>


/**
 * PLM toolbar button IDs.
 */
enum PLM_TOOLBAR_ID
{
    ID_PLM_TB_BROWSER = 15200,      ///< Open PLM browser
    ID_PLM_TB_CONNECT,              ///< Connect/disconnect toggle
    ID_PLM_TB_SYNC,                 ///< Sync with server
    ID_PLM_TB_STATUS,               ///< Status indicator (not a button)

    ID_PLM_TB_END                   ///< End marker
};


/**
 * PLM toolbar callback type.
 */
using PLM_TOOLBAR_CALLBACK = std::function<void( int aId )>;


/**
 * PLM toolbar state.
 */
struct PLM_TOOLBAR_STATE
{
    bool connected = false;             ///< Connected to PLM server
    bool project_managed = false;       ///< Current project is PLM-managed
    PLM_ICON_STATE sync_state = PLM_ICON_STATE::UNMANAGED; ///< Sync status
};


/**
 * Helper class for creating PLM toolbar buttons.
 *
 * Creates toolbar buttons for PLM operations that can be added to the
 * KiCad Project Manager toolbar.
 */
class KICOMMON_API PLM_TOOLBAR
{
public:
    PLM_TOOLBAR();
    ~PLM_TOOLBAR();

    /**
     * Add PLM buttons to a toolbar.
     *
     * Adds PLM browser, connect toggle, and sync buttons to the toolbar.
     *
     * @param aToolBar Toolbar to add buttons to.
     */
    void AddButtons( wxToolBar* aToolBar );

    /**
     * Update toolbar button states.
     *
     * @param aToolBar Toolbar to update.
     * @param aState Current PLM state.
     */
    void UpdateState( wxToolBar* aToolBar, const PLM_TOOLBAR_STATE& aState );

    /**
     * Get tooltip for a toolbar button.
     *
     * @param aId Button ID.
     * @param aState Current PLM state.
     * @return Tooltip string.
     */
    wxString GetTooltip( int aId, const PLM_TOOLBAR_STATE& aState ) const;

    /**
     * Set callback for toolbar actions.
     */
    void SetCallback( PLM_TOOLBAR_CALLBACK aCallback ) { m_callback = aCallback; }

    /**
     * Execute a toolbar action.
     *
     * @param aId Toolbar button ID.
     */
    void ExecuteAction( int aId );

    /**
     * Check if PLM toolbar buttons should be shown.
     *
     * @return True if PLM integration is enabled.
     */
    static bool ShouldShowButtons();

private:
    PLM_TOOLBAR_CALLBACK m_callback;
};


#endif // PLM_TOOLBAR_H
