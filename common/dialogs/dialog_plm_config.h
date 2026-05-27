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

#ifndef DIALOG_PLM_CONFIG_H
#define DIALOG_PLM_CONFIG_H

#include <vector>
#include <utility>
#include <wx/string.h>
#include <dialog_plm_config_base.h>

class wxCheckBox;
class wxChoice;
class wxSpinCtrl;
class wxStaticText;
class wxTextCtrl;
class PLM_PLUGIN;


struct PLM_SERVER_CONFIG
{
    wxString plugin_id;
    wxString server_url;
    wxString default_folder;
    bool     use_stored_credentials = true;
    wxString username;
    wxString password;

    // Extended settings
    bool     auto_sync_enabled = false;
    int      auto_sync_interval_minutes = 15;
    int      conflict_resolution = 0;   ///< 0 = ask, 1 = local wins, 2 = remote wins
    bool     offline_mode = false;
    int      cache_size_mb = 256;
};


class DIALOG_PLM_CONFIG : public DIALOG_PLM_CONFIG_BASE
{
public:
    DIALOG_PLM_CONFIG( wxWindow* aParent );
    ~DIALOG_PLM_CONFIG() override;

    void SetAvailablePlugins( const std::vector<std::pair<wxString, wxString>>& aPlugins );

    void SetPlugin( PLM_PLUGIN* aPlugin );

    void SetConfig( const PLM_SERVER_CONFIG& aConfig );

    PLM_SERVER_CONFIG GetConfig() const;

    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;

protected:
    void onPluginSelected( wxCommandEvent& event ) override;
    void onTestConnection( wxCommandEvent& event ) override;
    void onUseDefaultCredsChanged( wxCommandEvent& event ) override;

private:
    void createExtendedControls();
    void testConnection();
    void updateCredentialsEnabled();
    void updateCapabilitiesDisplay();
    void onClearCache( wxCommandEvent& event );

    std::vector<std::pair<wxString, wxString>> m_plugins;
    PLM_SERVER_CONFIG                          m_config;
    PLM_PLUGIN*                                m_plugin;

    // Extended controls (added dynamically)
    wxTextCtrl*     m_capabilitiesText;
    wxCheckBox*     m_autoSyncCheckbox;
    wxSpinCtrl*     m_syncIntervalSpin;
    wxChoice*       m_conflictResolutionChoice;
    wxCheckBox*     m_offlineModeCheckbox;
    wxSpinCtrl*     m_cacheSizeSpin;
    wxStaticText*   m_cacheUsageLabel;
};

#endif // DIALOG_PLM_CONFIG_H
