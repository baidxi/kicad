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
 * @file plm_plugin_config.h
 * @brief Plugin configuration storage for the PLM system.
 *
 * This header defines structures and classes for persisting PLM plugin
 * configuration to JSON files. Sensitive data like credentials are NOT
 * stored in the config file; only references to secure storage.
 */

#ifndef PLM_PLUGIN_CONFIG_H
#define PLM_PLUGIN_CONFIG_H

#include <wx/string.h>
#include <map>
#include <vector>


/**
 * Configuration for a single PLM plugin.
 *
 * Contains non-sensitive settings like server URL, default options,
 * and user preferences. Credentials are stored separately in the
 * system keychain and only referenced here by key name.
 */
struct PLM_PLUGIN_CONFIG_ENTRY
{
    wxString plugin_id;            ///< Plugin identifier
    wxString server_url;           ///< PLM server URL
    wxString default_project;      ///< Default project to open
    wxString credential_key;       ///< Key name for credential lookup (not the credential itself)
    bool     auto_connect = false; ///< Automatically connect on startup
    bool     enabled = true;       ///< Whether plugin is enabled
    int      cache_timeout = 300;  ///< Cache timeout in seconds
    int      connection_timeout = 30; ///< Connection timeout in seconds

    std::map<wxString, wxString> custom_options; ///< Plugin-specific options

    PLM_PLUGIN_CONFIG_ENTRY() = default;

    PLM_PLUGIN_CONFIG_ENTRY( const wxString& aPluginId ) : plugin_id( aPluginId ) {}

    /**
     * Check if configuration has minimum required fields.
     * @return True if plugin_id is set.
     */
    bool IsValid() const { return !plugin_id.IsEmpty(); }

    /**
     * Get a custom option value.
     * @param aKey Option key.
     * @param aDefault Default value if not found.
     * @return Option value or default.
     */
    wxString GetOption( const wxString& aKey, const wxString& aDefault = wxEmptyString ) const
    {
        auto it = custom_options.find( aKey );

        if( it != custom_options.end() )
            return it->second;

        return aDefault;
    }

    /**
     * Set a custom option value.
     * @param aKey Option key.
     * @param aValue Option value.
     */
    void SetOption( const wxString& aKey, const wxString& aValue )
    {
        custom_options[aKey] = aValue;
    }

    /**
     * Remove a custom option.
     * @param aKey Option key to remove.
     */
    void RemoveOption( const wxString& aKey ) { custom_options.erase( aKey ); }
};


/**
 * PLM configuration file manager.
 *
 * Manages loading and saving of PLM plugin configurations to a JSON file.
 * The configuration file is stored in the KiCad user config directory.
 *
 * Security note: This class never stores actual credentials. Only
 * references to credential storage (key names) are persisted.
 *
 * Usage:
 * @code
 * PLM_PLUGIN_CONFIG config;
 * config.Load();
 *
 * auto entry = config.GetPluginConfig( "git" );
 * if( entry )
 * {
 *     // Use entry->server_url, etc.
 * }
 *
 * config.SetPluginConfig( "git", myEntry );
 * config.Save();
 * @endcode
 */
class PLM_PLUGIN_CONFIG
{
public:
    /**
     * Construct config manager.
     *
     * Does not automatically load configuration.
     */
    PLM_PLUGIN_CONFIG();

    /**
     * Destructor.
     */
    ~PLM_PLUGIN_CONFIG();

    // Non-copyable
    PLM_PLUGIN_CONFIG( const PLM_PLUGIN_CONFIG& ) = delete;
    PLM_PLUGIN_CONFIG& operator=( const PLM_PLUGIN_CONFIG& ) = delete;

    //-------------------------------------------------------------------------
    // File Operations
    //-------------------------------------------------------------------------

    /**
     * Load configuration from file.
     *
     * Loads from the standard config location. Creates default
     * config if file doesn't exist.
     *
     * @return True if loaded successfully.
     */
    bool Load();

    /**
     * Load configuration from a specific file.
     *
     * @param aPath Full path to configuration file.
     * @return True if loaded successfully.
     */
    bool LoadFromFile( const wxString& aPath );

    /**
     * Save configuration to file.
     *
     * Saves to the standard config location.
     *
     * @return True if saved successfully.
     */
    bool Save();

    /**
     * Save configuration to a specific file.
     *
     * @param aPath Full path to configuration file.
     * @return True if saved successfully.
     */
    bool SaveToFile( const wxString& aPath );

    /**
     * Get the default configuration file path.
     *
     * @return Path to the PLM config file.
     */
    static wxString GetDefaultConfigPath();

    //-------------------------------------------------------------------------
    // Plugin Configuration Access
    //-------------------------------------------------------------------------

    /**
     * Get configuration for a specific plugin.
     *
     * @param aPluginId Plugin identifier.
     * @return Pointer to config entry, or nullptr if not found.
     */
    const PLM_PLUGIN_CONFIG_ENTRY* GetPluginConfig( const wxString& aPluginId ) const;

    /**
     * Get mutable configuration for a specific plugin.
     *
     * @param aPluginId Plugin identifier.
     * @return Pointer to config entry, or nullptr if not found.
     */
    PLM_PLUGIN_CONFIG_ENTRY* GetPluginConfigMutable( const wxString& aPluginId );

    /**
     * Set configuration for a plugin.
     *
     * Adds or replaces the configuration entry.
     *
     * @param aPluginId Plugin identifier.
     * @param aConfig Configuration entry to store.
     */
    void SetPluginConfig( const wxString& aPluginId, const PLM_PLUGIN_CONFIG_ENTRY& aConfig );

    /**
     * Remove configuration for a plugin.
     *
     * @param aPluginId Plugin identifier.
     * @return True if entry was removed.
     */
    bool RemovePluginConfig( const wxString& aPluginId );

    /**
     * Get list of all configured plugin IDs.
     *
     * @return Vector of plugin identifiers.
     */
    std::vector<wxString> GetConfiguredPluginIds() const;

    /**
     * Check if a plugin has configuration.
     *
     * @param aPluginId Plugin identifier.
     * @return True if configuration exists.
     */
    bool HasPluginConfig( const wxString& aPluginId ) const;

    //-------------------------------------------------------------------------
    // Global Settings
    //-------------------------------------------------------------------------

    /**
     * Get the active plugin ID.
     *
     * @return Plugin ID of the currently active plugin, or empty if none.
     */
    wxString GetActivePluginId() const { return m_activePluginId; }

    /**
     * Set the active plugin ID.
     *
     * @param aPluginId Plugin ID to make active.
     */
    void SetActivePluginId( const wxString& aPluginId ) { m_activePluginId = aPluginId; }

    /**
     * Get last error message.
     *
     * @return Error message from last failed operation.
     */
    wxString GetLastError() const { return m_lastError; }

private:
    std::map<wxString, PLM_PLUGIN_CONFIG_ENTRY> m_pluginConfigs; ///< Per-plugin configurations
    wxString                                     m_activePluginId; ///< Currently active plugin
    wxString                                     m_lastError;      ///< Last error message
    int                                          m_schemaVersion = 1; ///< Config file version
};


#endif // PLM_PLUGIN_CONFIG_H
