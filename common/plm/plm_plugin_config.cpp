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
 * @file plm_plugin_config.cpp
 * @brief Implementation of PLM plugin configuration storage.
 */

#include "plm_plugin_config.h"

#include <nlohmann/json.hpp>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/stdpaths.h>
#include <wx/wfstream.h>

#include <fstream>


// JSON keys
static const char* KEY_SCHEMA_VERSION = "schema_version";
static const char* KEY_ACTIVE_PLUGIN = "active_plugin";
static const char* KEY_PLUGINS = "plugins";
static const char* KEY_PLUGIN_ID = "plugin_id";
static const char* KEY_SERVER_URL = "server_url";
static const char* KEY_DEFAULT_PROJECT = "default_project";
static const char* KEY_CREDENTIAL_KEY = "credential_key";
static const char* KEY_AUTO_CONNECT = "auto_connect";
static const char* KEY_ENABLED = "enabled";
static const char* KEY_CACHE_TIMEOUT = "cache_timeout";
static const char* KEY_CONNECTION_TIMEOUT = "connection_timeout";
static const char* KEY_CUSTOM_OPTIONS = "custom_options";


PLM_PLUGIN_CONFIG::PLM_PLUGIN_CONFIG()
{
}


PLM_PLUGIN_CONFIG::~PLM_PLUGIN_CONFIG()
{
}


wxString PLM_PLUGIN_CONFIG::GetDefaultConfigPath()
{
    wxString configDir = wxStandardPaths::Get().GetUserDataDir();
    return configDir + wxFileName::GetPathSeparator() + wxS( "plm_config.json" );
}


bool PLM_PLUGIN_CONFIG::Load()
{
    return LoadFromFile( GetDefaultConfigPath() );
}


bool PLM_PLUGIN_CONFIG::LoadFromFile( const wxString& aPath )
{
    m_lastError.Clear();

    if( !wxFileName::FileExists( aPath ) )
    {
        // No config file yet, start with empty config
        m_pluginConfigs.clear();
        m_activePluginId.Clear();
        return true;
    }

    try
    {
        std::ifstream file( aPath.ToStdString() );

        if( !file.is_open() )
        {
            m_lastError = wxString::Format( wxS( "Cannot open config file: %s" ), aPath );
            return false;
        }

        nlohmann::json root;
        file >> root;

        // Check schema version
        if( root.contains( KEY_SCHEMA_VERSION ) )
        {
            m_schemaVersion = root[KEY_SCHEMA_VERSION].get<int>();
        }

        // Load active plugin
        if( root.contains( KEY_ACTIVE_PLUGIN ) && !root[KEY_ACTIVE_PLUGIN].is_null() )
        {
            m_activePluginId = wxString::FromUTF8( root[KEY_ACTIVE_PLUGIN].get<std::string>() );
        }

        // Load plugin configurations
        m_pluginConfigs.clear();

        if( root.contains( KEY_PLUGINS ) && root[KEY_PLUGINS].is_array() )
        {
            for( const auto& pluginJson : root[KEY_PLUGINS] )
            {
                PLM_PLUGIN_CONFIG_ENTRY entry;

                if( pluginJson.contains( KEY_PLUGIN_ID ) )
                {
                    entry.plugin_id =
                            wxString::FromUTF8( pluginJson[KEY_PLUGIN_ID].get<std::string>() );
                }

                if( pluginJson.contains( KEY_SERVER_URL ) && !pluginJson[KEY_SERVER_URL].is_null() )
                {
                    entry.server_url =
                            wxString::FromUTF8( pluginJson[KEY_SERVER_URL].get<std::string>() );
                }

                if( pluginJson.contains( KEY_DEFAULT_PROJECT )
                    && !pluginJson[KEY_DEFAULT_PROJECT].is_null() )
                {
                    entry.default_project =
                            wxString::FromUTF8( pluginJson[KEY_DEFAULT_PROJECT].get<std::string>() );
                }

                if( pluginJson.contains( KEY_CREDENTIAL_KEY )
                    && !pluginJson[KEY_CREDENTIAL_KEY].is_null() )
                {
                    entry.credential_key =
                            wxString::FromUTF8( pluginJson[KEY_CREDENTIAL_KEY].get<std::string>() );
                }

                if( pluginJson.contains( KEY_AUTO_CONNECT ) )
                {
                    entry.auto_connect = pluginJson[KEY_AUTO_CONNECT].get<bool>();
                }

                if( pluginJson.contains( KEY_ENABLED ) )
                {
                    entry.enabled = pluginJson[KEY_ENABLED].get<bool>();
                }

                if( pluginJson.contains( KEY_CACHE_TIMEOUT ) )
                {
                    entry.cache_timeout = pluginJson[KEY_CACHE_TIMEOUT].get<int>();
                }

                if( pluginJson.contains( KEY_CONNECTION_TIMEOUT ) )
                {
                    entry.connection_timeout = pluginJson[KEY_CONNECTION_TIMEOUT].get<int>();
                }

                if( pluginJson.contains( KEY_CUSTOM_OPTIONS )
                    && pluginJson[KEY_CUSTOM_OPTIONS].is_object() )
                {
                    for( auto& [key, value] : pluginJson[KEY_CUSTOM_OPTIONS].items() )
                    {
                        if( value.is_string() )
                        {
                            entry.custom_options[wxString::FromUTF8( key )] =
                                    wxString::FromUTF8( value.get<std::string>() );
                        }
                    }
                }

                if( entry.IsValid() )
                {
                    m_pluginConfigs[entry.plugin_id] = entry;
                }
            }
        }

        return true;
    }
    catch( const nlohmann::json::exception& e )
    {
        m_lastError =
                wxString::Format( wxS( "JSON parse error: %s" ), wxString::FromUTF8( e.what() ) );
        return false;
    }
    catch( const std::exception& e )
    {
        m_lastError = wxString::Format( wxS( "Error loading config: %s" ),
                                         wxString::FromUTF8( e.what() ) );
        return false;
    }
}


bool PLM_PLUGIN_CONFIG::Save()
{
    return SaveToFile( GetDefaultConfigPath() );
}


bool PLM_PLUGIN_CONFIG::SaveToFile( const wxString& aPath )
{
    m_lastError.Clear();

    try
    {
        nlohmann::json root;

        root[KEY_SCHEMA_VERSION] = m_schemaVersion;

        if( !m_activePluginId.IsEmpty() )
        {
            root[KEY_ACTIVE_PLUGIN] = m_activePluginId.ToUTF8().data();
        }
        else
        {
            root[KEY_ACTIVE_PLUGIN] = nullptr;
        }

        nlohmann::json pluginsArray = nlohmann::json::array();

        for( const auto& [id, entry] : m_pluginConfigs )
        {
            nlohmann::json pluginJson;

            pluginJson[KEY_PLUGIN_ID] = entry.plugin_id.ToUTF8().data();

            if( !entry.server_url.IsEmpty() )
            {
                pluginJson[KEY_SERVER_URL] = entry.server_url.ToUTF8().data();
            }

            if( !entry.default_project.IsEmpty() )
            {
                pluginJson[KEY_DEFAULT_PROJECT] = entry.default_project.ToUTF8().data();
            }

            if( !entry.credential_key.IsEmpty() )
            {
                pluginJson[KEY_CREDENTIAL_KEY] = entry.credential_key.ToUTF8().data();
            }

            pluginJson[KEY_AUTO_CONNECT] = entry.auto_connect;
            pluginJson[KEY_ENABLED] = entry.enabled;
            pluginJson[KEY_CACHE_TIMEOUT] = entry.cache_timeout;
            pluginJson[KEY_CONNECTION_TIMEOUT] = entry.connection_timeout;

            if( !entry.custom_options.empty() )
            {
                nlohmann::json optionsJson;

                for( const auto& [key, value] : entry.custom_options )
                {
                    optionsJson[key.ToUTF8().data()] = value.ToUTF8().data();
                }

                pluginJson[KEY_CUSTOM_OPTIONS] = optionsJson;
            }

            pluginsArray.push_back( pluginJson );
        }

        root[KEY_PLUGINS] = pluginsArray;

        // Ensure directory exists
        wxFileName fn( aPath );
        wxString dir = fn.GetPath();

        if( !dir.IsEmpty() && !wxFileName::DirExists( dir ) )
        {
            wxFileName::Mkdir( dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );
        }

        // Write to file with pretty printing
        std::ofstream file( aPath.ToStdString() );

        if( !file.is_open() )
        {
            m_lastError = wxString::Format( wxS( "Cannot write config file: %s" ), aPath );
            return false;
        }

        file << std::setw( 2 ) << root << std::endl;

        return true;
    }
    catch( const std::exception& e )
    {
        m_lastError = wxString::Format( wxS( "Error saving config: %s" ),
                                         wxString::FromUTF8( e.what() ) );
        return false;
    }
}


const PLM_PLUGIN_CONFIG_ENTRY* PLM_PLUGIN_CONFIG::GetPluginConfig( const wxString& aPluginId ) const
{
    auto it = m_pluginConfigs.find( aPluginId );

    if( it != m_pluginConfigs.end() )
        return &it->second;

    return nullptr;
}


PLM_PLUGIN_CONFIG_ENTRY* PLM_PLUGIN_CONFIG::GetPluginConfigMutable( const wxString& aPluginId )
{
    auto it = m_pluginConfigs.find( aPluginId );

    if( it != m_pluginConfigs.end() )
        return &it->second;

    return nullptr;
}


void PLM_PLUGIN_CONFIG::SetPluginConfig( const wxString& aPluginId,
                                          const PLM_PLUGIN_CONFIG_ENTRY& aConfig )
{
    PLM_PLUGIN_CONFIG_ENTRY entry = aConfig;
    entry.plugin_id = aPluginId;
    m_pluginConfigs[aPluginId] = entry;
}


bool PLM_PLUGIN_CONFIG::RemovePluginConfig( const wxString& aPluginId )
{
    auto it = m_pluginConfigs.find( aPluginId );

    if( it != m_pluginConfigs.end() )
    {
        m_pluginConfigs.erase( it );

        if( m_activePluginId == aPluginId )
        {
            m_activePluginId.Clear();
        }

        return true;
    }

    return false;
}


std::vector<wxString> PLM_PLUGIN_CONFIG::GetConfiguredPluginIds() const
{
    std::vector<wxString> ids;
    ids.reserve( m_pluginConfigs.size() );

    for( const auto& [id, entry] : m_pluginConfigs )
    {
        ids.push_back( id );
    }

    return ids;
}


bool PLM_PLUGIN_CONFIG::HasPluginConfig( const wxString& aPluginId ) const
{
    return m_pluginConfigs.find( aPluginId ) != m_pluginConfigs.end();
}
