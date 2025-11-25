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
 * @file plm_status_panel.h
 * @brief Dockable PLM status panel widget.
 *
 * Provides a compact panel showing current PLM project status including
 * connection state, lock status, last sync time, and quick action buttons.
 */

#ifndef PLM_STATUS_PANEL_H
#define PLM_STATUS_PANEL_H

#include <kicommon.h>
#include <plm/plm_event_bus.h>
#include <plm/plm_icon_provider.h>

#include <wx/panel.h>


class wxStaticText;
class wxStaticBitmap;
class wxButton;
class wxBitmapButton;
class wxBoxSizer;


/**
 * PLM action callback type.
 */
using PLM_ACTION_CALLBACK = std::function<void()>;


/**
 * Dockable panel showing PLM status for the current project.
 *
 * Displays connection status, current server, lock status, last sync time,
 * and provides quick action buttons for common PLM operations.
 *
 * The panel automatically subscribes to PLM events and updates in real-time.
 *
 * Usage:
 * @code
 * PLM_STATUS_PANEL* panel = new PLM_STATUS_PANEL( parentWindow );
 *
 * // Set action callbacks
 * panel->SetLockCallback( []() { ... } );
 * panel->SetUnlockCallback( []() { ... } );
 * panel->SetRefreshCallback( []() { ... } );
 *
 * // Update for current project
 * panel->SetProject( projectPath );
 * @endcode
 */
class KICOMMON_API PLM_STATUS_PANEL : public wxPanel
{
public:
    PLM_STATUS_PANEL( wxWindow* aParent );
    ~PLM_STATUS_PANEL();

    //-------------------------------------------------------------------------
    // Project Configuration
    //-------------------------------------------------------------------------

    /**
     * Set the current project for status display.
     *
     * @param aProjectPath Path to the project file.
     */
    void SetProject( const wxString& aProjectPath );

    /**
     * Clear project and show unmanaged state.
     */
    void ClearProject();

    /**
     * Get the current project path.
     *
     * @return Project path, or empty if no project set.
     */
    wxString GetProject() const { return m_projectPath; }

    //-------------------------------------------------------------------------
    // Status Updates
    //-------------------------------------------------------------------------

    /**
     * Update connection status display.
     *
     * @param aConnected True if connected to PLM server.
     * @param aServerName Server name to display.
     */
    void SetConnectionStatus( bool aConnected, const wxString& aServerName = wxEmptyString );

    /**
     * Update lock status display.
     *
     * @param aState Lock state.
     * @param aOwner Lock owner (empty if unlocked or owned by current user).
     */
    void SetLockStatus( PLM_ICON_STATE aState, const wxString& aOwner = wxEmptyString );

    /**
     * Update sync status display.
     *
     * @param aState Sync state.
     * @param aLastSyncTime Last sync timestamp (Unix epoch).
     */
    void SetSyncStatus( PLM_ICON_STATE aState, int64_t aLastSyncTime = 0 );

    /**
     * Refresh all status displays from PLM system.
     */
    void RefreshStatus();

    //-------------------------------------------------------------------------
    // Action Callbacks
    //-------------------------------------------------------------------------

    /**
     * Set callback for lock action button.
     */
    void SetLockCallback( PLM_ACTION_CALLBACK aCallback ) { m_lockCallback = aCallback; }

    /**
     * Set callback for unlock action button.
     */
    void SetUnlockCallback( PLM_ACTION_CALLBACK aCallback ) { m_unlockCallback = aCallback; }

    /**
     * Set callback for refresh action button.
     */
    void SetRefreshCallback( PLM_ACTION_CALLBACK aCallback ) { m_refreshCallback = aCallback; }

    /**
     * Set callback for browse action button.
     */
    void SetBrowseCallback( PLM_ACTION_CALLBACK aCallback ) { m_browseCallback = aCallback; }

    //-------------------------------------------------------------------------
    // UI State
    //-------------------------------------------------------------------------

    /**
     * Enable or disable action buttons.
     *
     * @param aEnable True to enable buttons.
     */
    void EnableActions( bool aEnable );

    /**
     * Show or hide the panel.
     *
     * @param aShow True to show.
     * @return True if visibility changed.
     */
    bool Show( bool aShow ) override;

private:
    void createControls();
    void layoutControls();
    void subscribeToEvents();
    void unsubscribeFromEvents();
    void onPlmEvent( const PLM_EVENT& aEvent );
    void updateStateIcon( wxStaticBitmap* aBitmap, PLM_ICON_STATE aState );

    // Event handlers
    void onLockButton( wxCommandEvent& aEvent );
    void onUnlockButton( wxCommandEvent& aEvent );
    void onRefreshButton( wxCommandEvent& aEvent );
    void onBrowseButton( wxCommandEvent& aEvent );

    // Format helpers
    wxString formatTimestamp( int64_t aTimestamp );
    wxString formatLockOwner( const wxString& aOwner );

    wxString              m_projectPath;
    PLM_SUBSCRIPTION_ID   m_subscriptionId;

    // Status display controls
    wxStaticBitmap*       m_connectionIcon;
    wxStaticText*         m_connectionLabel;
    wxStaticText*         m_serverLabel;
    wxStaticBitmap*       m_lockIcon;
    wxStaticText*         m_lockLabel;
    wxStaticBitmap*       m_syncIcon;
    wxStaticText*         m_syncLabel;
    wxStaticText*         m_lastSyncLabel;

    // Action buttons
    wxButton*             m_lockButton;
    wxButton*             m_unlockButton;
    wxBitmapButton*       m_refreshButton;
    wxBitmapButton*       m_browseButton;

    // Callbacks
    PLM_ACTION_CALLBACK   m_lockCallback;
    PLM_ACTION_CALLBACK   m_unlockCallback;
    PLM_ACTION_CALLBACK   m_refreshCallback;
    PLM_ACTION_CALLBACK   m_browseCallback;

    // State
    bool                  m_connected;
    PLM_ICON_STATE        m_lockState;
    PLM_ICON_STATE        m_syncState;
};


#endif // PLM_STATUS_PANEL_H
