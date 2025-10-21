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
 * @file plm_plugin_manager.cpp
 * @brief Implementation of PLM plugin discovery and management.
 */

#include "plm_plugin_manager.h"

#include <paths.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/stdpaths.h>

#ifdef __WXMSW__
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif


PLM_PLUGIN_MANAGER::PLM_PLUGIN_MANAGER()
{
}


PLM_PLUGIN_MANAGER::~PLM_PLUGIN_MANAGER()
{
    UnloadAllPlugins();
}


void PLM_PLUGIN_MANAGER::AddPluginDirectory( const wxString& aPath )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    // Avoid duplicates
    for( const auto& dir : m_pluginDirs )
    {
        if( dir == aPath )
            return;
    }

    m_pluginDirs.push_back( aPath );
}


void PLM_PLUGIN_MANAGER::AddStandardDirectories()
{
    // System plugin directory — same location as all other KiCad stock plugins
    AddPluginDirectory( PATHS::GetStockPluginsPath() );

    // User plugin directory
    wxString userDir = wxStandardPaths::Get().GetUserDataDir() + wxFileName::GetPathSeparator()
                       + wxS( "plm" ) + wxFileName::GetPathSeparator() + wxS( "plugins" );

    AddPluginDirectory( userDir );
}


size_t PLM_PLUGIN_MANAGER::DiscoverPlugins()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    m_discoveredPlugins.clear();

    size_t total = 0;

    for( const auto& dir : m_pluginDirs )
    {
        total += ScanDirectory( dir );
    }

    return total;
}


size_t PLM_PLUGIN_MANAGER::ScanDirectory( const wxString& aPath )
{
    if( !wxDir::Exists( aPath ) )
        return 0;

    wxDir dir( aPath );

    if( !dir.IsOpened() )
        return 0;

    size_t count = 0;
    wxString filename;
    wxString ext = GetSharedLibraryExtension();

    bool hasMore = dir.GetFirst( &filename, wxS( "*" ) + ext, wxDIR_FILES );

    while( hasMore )
    {
        if( IsPluginFilename( filename ) )
        {
            wxString fullPath = aPath + wxFileName::GetPathSeparator() + filename;
            wxString pluginId = ExtractPluginId( filename );

            PLM_DISCOVERED_PLUGIN discovered( fullPath, filename );
            discovered.plugin_id = pluginId;

            // Check if we already have a plugin with this ID (later dirs override)
            bool replaced = false;

            for( auto& existing : m_discoveredPlugins )
            {
                if( existing.plugin_id == pluginId )
                {
                    existing = discovered;
                    replaced = true;
                    break;
                }
            }

            if( !replaced )
            {
                m_discoveredPlugins.push_back( discovered );
                ++count;
            }
        }

        hasMore = dir.GetNext( &filename );
    }

    return count;
}


const PLM_DISCOVERED_PLUGIN* PLM_PLUGIN_MANAGER::FindDiscoveredPlugin( const wxString& aPluginId ) const
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    for( const auto& plugin : m_discoveredPlugins )
    {
        if( plugin.plugin_id == aPluginId )
            return &plugin;
    }

    return nullptr;
}


wxString PLM_PLUGIN_MANAGER::GetSharedLibraryExtension()
{
#ifdef __WXMSW__
    return wxS( ".dll" );
#elif defined( __WXMAC__ )
    return wxS( ".dylib" );
#else
    return wxS( ".so" );
#endif
}


wxString PLM_PLUGIN_MANAGER::GetPluginPrefix()
{
    return wxS( "plm-" );
}


bool PLM_PLUGIN_MANAGER::IsPluginFilename( const wxString& aFilename )
{
    wxString prefix = GetPluginPrefix();
    wxString ext = GetSharedLibraryExtension();

    return aFilename.StartsWith( prefix ) && aFilename.EndsWith( ext );
}


wxString PLM_PLUGIN_MANAGER::ExtractPluginId( const wxString& aFilename )
{
    wxString prefix = GetPluginPrefix();
    wxString ext = GetSharedLibraryExtension();

    if( !IsPluginFilename( aFilename ) )
        return wxEmptyString;

    // Extract ID between prefix and extension, e.g. "plm-git.so" -> "git"
    wxString id = aFilename.Mid( prefix.length() );
    id = id.Left( id.length() - ext.length() );

    return id;
}


//-----------------------------------------------------------------------------
// Error Handling Implementation
//-----------------------------------------------------------------------------

void PLM_PLUGIN_MANAGER::LogPluginError( const wxString& aPluginId, const wxString& aOperation,
                                          const wxString& aMessage )
{
    wxLogError( wxS( "PLM Plugin '%s' error in %s: %s" ), aPluginId, aOperation, aMessage );
}


void PLM_PLUGIN_MANAGER::LogPluginWarning( const wxString& aPluginId, const wxString& aOperation,
                                            const wxString& aMessage )
{
    wxLogWarning( wxS( "PLM Plugin '%s' warning in %s: %s" ), aPluginId, aOperation, aMessage );
}


//-----------------------------------------------------------------------------
// Version Checking Implementation
//-----------------------------------------------------------------------------

void PLM_PLUGIN_MANAGER::ParseAPIVersion( int aVersion, int& aMajor, int& aMinor )
{
    // Handle malformed/negative versions
    if( aVersion < 0 )
    {
        aMajor = 0;
        aMinor = 0;
        return;
    }

    aMajor = aVersion / 1000;
    aMinor = aVersion % 1000;
}


bool PLM_PLUGIN_MANAGER::IsVersionCompatible( int aPluginVersion, wxString& aErrorMsg )
{
    aErrorMsg.Clear();

    int pluginMajor, pluginMinor;
    ParseAPIVersion( aPluginVersion, pluginMajor, pluginMinor );

    // Major version must match exactly
    if( pluginMajor != PLM_API_VERSION_MAJOR )
    {
        aErrorMsg = wxString::Format(
                wxS( "Incompatible API version %d.%d (host requires %d.x)" ),
                pluginMajor, pluginMinor, PLM_API_VERSION_MAJOR );
        return false;
    }

    // Plugin with newer minor version is allowed but logged
    if( pluginMinor > PLM_API_VERSION_MINOR )
    {
        wxLogWarning( wxS( "Plugin API %d.%d is newer than host API %d.%d" ),
                      pluginMajor, pluginMinor,
                      PLM_API_VERSION_MAJOR, PLM_API_VERSION_MINOR );
    }

    return true;
}


wxString PLM_PLUGIN_MANAGER::FormatVersionString( int aVersion )
{
    int major, minor;
    ParseAPIVersion( aVersion, major, minor );
    return wxString::Format( wxS( "%d.%d" ), major, minor );
}


//-----------------------------------------------------------------------------
// Plugin Loading Implementation
//-----------------------------------------------------------------------------

PLM_PLUGIN* PLM_PLUGIN_MANAGER::LoadPlugin( const wxString& aPluginId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    m_lastError.Clear();

    // Check if already loaded
    if( IsPluginLoaded( aPluginId ) )
    {
        m_lastError = wxString::Format( wxS( "Plugin '%s' is already loaded" ), aPluginId );
        return GetPlugin( aPluginId );
    }

    // Find the plugin in discovered list
    const PLM_DISCOVERED_PLUGIN* discovered = FindDiscoveredPlugin( aPluginId );

    if( !discovered )
    {
        m_lastError = wxString::Format( wxS( "Plugin '%s' not found in discovered plugins" ),
                                         aPluginId );
        return nullptr;
    }

    return LoadPluginFromPath( discovered->path );
}


PLM_PLUGIN* PLM_PLUGIN_MANAGER::LoadPluginFromPath( const wxString& aPath )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    m_lastError.Clear();

    // Verify file exists
    if( !wxFileName::FileExists( aPath ) )
    {
        m_lastError = wxString::Format( wxS( "Plugin file not found: %s" ), aPath );
        return nullptr;
    }

    // Extract plugin ID from path
    wxFileName fn( aPath );
    wxString pluginId = ExtractPluginId( fn.GetFullName() );

    if( pluginId.IsEmpty() )
    {
        m_lastError = wxString::Format( wxS( "Invalid plugin filename: %s" ), aPath );
        return nullptr;
    }

    // Check if already loaded
    if( IsPluginLoaded( pluginId ) )
    {
        m_lastError = wxString::Format( wxS( "Plugin '%s' is already loaded" ), pluginId );
        return GetPlugin( pluginId );
    }

    // Load the shared library
    void* handle = LoadLibrary( aPath );

    if( !handle )
    {
        m_lastError = wxString::Format( wxS( "Failed to load plugin library '%s': %s" ),
                                         aPath, GetLibraryError() );
        UpdateDiscoveryInfo( pluginId, false, m_lastError );
        return nullptr;
    }

    // Get the API version function
    PLM_PLUGIN_VERSION_FUNC versionFunc =
            reinterpret_cast<PLM_PLUGIN_VERSION_FUNC>( GetSymbol( handle, PLM_FUNC_VERSION ) );

    if( !versionFunc )
    {
        m_lastError = wxString::Format( wxS( "Plugin '%s' missing %s symbol" ),
                                         pluginId, PLM_FUNC_VERSION );
        UnloadLibrary( handle );
        UpdateDiscoveryInfo( pluginId, false, m_lastError );
        return nullptr;
    }

    // Check API version compatibility
    int pluginVersion = versionFunc();
    wxString versionError;

    if( !IsVersionCompatible( pluginVersion, versionError ) )
    {
        m_lastError = wxString::Format( wxS( "Plugin '%s': %s" ), pluginId, versionError );
        UnloadLibrary( handle );
        UpdateDiscoveryInfo( pluginId, false, m_lastError );
        return nullptr;
    }

    // Get the factory function
    PLM_PLUGIN_CREATE_FUNC createFunc =
            reinterpret_cast<PLM_PLUGIN_CREATE_FUNC>( GetSymbol( handle, PLM_FUNC_CREATE ) );

    if( !createFunc )
    {
        m_lastError = wxString::Format( wxS( "Plugin '%s' missing %s symbol" ),
                                         pluginId, PLM_FUNC_CREATE );
        UnloadLibrary( handle );
        UpdateDiscoveryInfo( pluginId, false, m_lastError );
        return nullptr;
    }

    // Get the destroy function
    PLM_PLUGIN_DESTROY_FUNC destroyFunc =
            reinterpret_cast<PLM_PLUGIN_DESTROY_FUNC>( GetSymbol( handle, PLM_FUNC_DESTROY ) );

    if( !destroyFunc )
    {
        m_lastError = wxString::Format( wxS( "Plugin '%s' missing %s symbol" ),
                                         pluginId, PLM_FUNC_DESTROY );
        UnloadLibrary( handle );
        UpdateDiscoveryInfo( pluginId, false, m_lastError );
        return nullptr;
    }

    // Create the plugin instance
    PLM_PLUGIN* plugin = createFunc();

    if( !plugin )
    {
        m_lastError = wxString::Format( wxS( "Plugin '%s' factory returned null" ), pluginId );
        UnloadLibrary( handle );
        UpdateDiscoveryInfo( pluginId, false, m_lastError );
        return nullptr;
    }

    // Verify compatibility check
    if( !plugin->IsCompatible() )
    {
        m_lastError = wxString::Format( wxS( "Plugin '%s' failed compatibility check" ), pluginId );
        destroyFunc( plugin );
        UnloadLibrary( handle );
        UpdateDiscoveryInfo( pluginId, false, m_lastError );
        return nullptr;
    }

    // Call OnLoad with exception handling
    bool loadSuccess = false;

    try
    {
        loadSuccess = plugin->OnLoad();
    }
    catch( const std::exception& e )
    {
        m_lastError = wxString::Format( wxS( "Plugin '%s' OnLoad() threw exception: %s" ),
                                         pluginId, wxString::FromUTF8( e.what() ) );
        LogPluginError( pluginId, wxS( "OnLoad" ), wxString::FromUTF8( e.what() ) );
        destroyFunc( plugin );
        UnloadLibrary( handle );
        UpdateDiscoveryInfo( pluginId, false, m_lastError );
        return nullptr;
    }
    catch( ... )
    {
        m_lastError = wxString::Format( wxS( "Plugin '%s' OnLoad() threw unknown exception" ),
                                         pluginId );
        LogPluginError( pluginId, wxS( "OnLoad" ), wxS( "Unknown exception" ) );
        destroyFunc( plugin );
        UnloadLibrary( handle );
        UpdateDiscoveryInfo( pluginId, false, m_lastError );
        return nullptr;
    }

    if( !loadSuccess )
    {
        m_lastError = wxString::Format( wxS( "Plugin '%s' OnLoad() failed" ), pluginId );
        LogPluginError( pluginId, wxS( "OnLoad" ), wxS( "Returned false" ) );
        destroyFunc( plugin );
        UnloadLibrary( handle );
        UpdateDiscoveryInfo( pluginId, false, m_lastError );
        return nullptr;
    }

    // Create loaded plugin entry
    PLM_LOADED_PLUGIN loaded;
    loaded.plugin = plugin;
    loaded.library_handle = handle;
    loaded.destroy_func = destroyFunc;
    loaded.plugin_info = plugin->GetPluginInfo();

    // Set discovery info
    const PLM_DISCOVERED_PLUGIN* discovered = FindDiscoveredPlugin( pluginId );

    if( discovered )
    {
        loaded.discovery_info = *discovered;
    }
    else
    {
        // Plugin loaded from path not in discovery list
        wxFileName pathFn( aPath );
        loaded.discovery_info.path = aPath;
        loaded.discovery_info.filename = pathFn.GetFullName();
        loaded.discovery_info.plugin_id = pluginId;
    }

    loaded.discovery_info.loaded = true;
    loaded.discovery_info.compatible = true;
    loaded.discovery_info.error_message.Clear();

    m_loadedPlugins.push_back( loaded );
    UpdateDiscoveryInfo( pluginId, true );

    wxLogMessage( wxS( "PLM: Loaded plugin '%s' version %s" ),
                  loaded.plugin_info.display_name, loaded.plugin_info.version.ToString() );

    return plugin;
}


bool PLM_PLUGIN_MANAGER::UnloadPlugin( const wxString& aPluginId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    m_lastError.Clear();

    auto it = FindLoadedPlugin( aPluginId );

    if( it == m_loadedPlugins.end() )
    {
        m_lastError = wxString::Format( wxS( "Plugin '%s' is not loaded" ), aPluginId );
        return false;
    }

    PLM_LOADED_PLUGIN& loaded = *it;

    // Call OnUnload with exception handling
    if( loaded.plugin )
    {
        try
        {
            loaded.plugin->OnUnload();
        }
        catch( const std::exception& e )
        {
            LogPluginError( aPluginId, wxS( "OnUnload" ), wxString::FromUTF8( e.what() ) );
        }
        catch( ... )
        {
            LogPluginError( aPluginId, wxS( "OnUnload" ), wxS( "Unknown exception" ) );
        }
    }

    // Destroy the plugin instance
    if( loaded.destroy_func && loaded.plugin )
    {
        loaded.destroy_func( loaded.plugin );
        loaded.plugin = nullptr;
    }

    // Unload the library
    if( loaded.library_handle )
    {
        UnloadLibrary( loaded.library_handle );
        loaded.library_handle = nullptr;
    }

    wxString pluginName = loaded.plugin_info.display_name;

    // Remove from loaded list
    m_loadedPlugins.erase( it );

    UpdateDiscoveryInfo( aPluginId, false );

    // Clear active plugin if this was the active one
    if( m_activePluginId == aPluginId )
    {
        m_activePluginId.Clear();
    }

    wxLogMessage( wxS( "PLM: Unloaded plugin '%s'" ), pluginName );

    return true;
}


void PLM_PLUGIN_MANAGER::UnloadAllPlugins()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    // Clear active plugin first
    m_activePluginId.Clear();

    // Unload in reverse order to handle dependencies
    while( !m_loadedPlugins.empty() )
    {
        wxString pluginId = m_loadedPlugins.back().discovery_info.plugin_id;
        UnloadPlugin( pluginId );
    }
}


PLM_PLUGIN* PLM_PLUGIN_MANAGER::GetPlugin( const wxString& aPluginId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    auto it = FindLoadedPlugin( aPluginId );

    if( it != m_loadedPlugins.end() )
        return it->plugin;

    return nullptr;
}


bool PLM_PLUGIN_MANAGER::IsPluginLoaded( const wxString& aPluginId ) const
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    for( const auto& loaded : m_loadedPlugins )
    {
        if( loaded.discovery_info.plugin_id == aPluginId )
            return true;
    }

    return false;
}


//-----------------------------------------------------------------------------
// Active Plugin Management Implementation
//-----------------------------------------------------------------------------

bool PLM_PLUGIN_MANAGER::SetActivePlugin( const wxString& aPluginId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    m_lastError.Clear();

    // Clearing active plugin
    if( aPluginId.IsEmpty() )
    {
        if( !m_activePluginId.IsEmpty() )
        {
            wxLogMessage( wxS( "PLM: Deactivated plugin '%s'" ), m_activePluginId );
        }

        m_activePluginId.Clear();
        return true;
    }

    // Same plugin already active
    if( m_activePluginId == aPluginId )
    {
        return true;
    }

    // Check if the plugin is loaded
    if( !IsPluginLoaded( aPluginId ) )
    {
        // Try to load it
        PLM_PLUGIN* plugin = LoadPlugin( aPluginId );

        if( !plugin )
        {
            // m_lastError already set by LoadPlugin
            return false;
        }
    }

    // Deactivate previous plugin if any
    if( !m_activePluginId.IsEmpty() )
    {
        wxLogMessage( wxS( "PLM: Switching from plugin '%s' to '%s'" ),
                      m_activePluginId, aPluginId );
    }

    m_activePluginId = aPluginId;

    wxLogMessage( wxS( "PLM: Activated plugin '%s'" ), aPluginId );

    return true;
}


PLM_PLUGIN* PLM_PLUGIN_MANAGER::GetActivePlugin()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( m_activePluginId.IsEmpty() )
        return nullptr;

    return GetPlugin( m_activePluginId );
}


const PLM_LOADED_PLUGIN* PLM_PLUGIN_MANAGER::GetActivePluginInfo() const
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( m_activePluginId.IsEmpty() )
        return nullptr;

    for( const auto& loaded : m_loadedPlugins )
    {
        if( loaded.discovery_info.plugin_id == m_activePluginId )
            return &loaded;
    }

    return nullptr;
}


std::vector<PLM_LOADED_PLUGIN>::iterator
PLM_PLUGIN_MANAGER::FindLoadedPlugin( const wxString& aPluginId )
{
    for( auto it = m_loadedPlugins.begin(); it != m_loadedPlugins.end(); ++it )
    {
        if( it->discovery_info.plugin_id == aPluginId )
            return it;
    }

    return m_loadedPlugins.end();
}


void PLM_PLUGIN_MANAGER::UpdateDiscoveryInfo( const wxString& aPluginId, bool aLoaded,
                                               const wxString& aError )
{
    for( auto& plugin : m_discoveredPlugins )
    {
        if( plugin.plugin_id == aPluginId )
        {
            plugin.loaded = aLoaded;

            if( !aError.IsEmpty() )
            {
                plugin.error_message = aError;
                plugin.compatible = false;
            }
            else if( aLoaded )
            {
                plugin.error_message.Clear();
                plugin.compatible = true;
            }

            break;
        }
    }
}


//-----------------------------------------------------------------------------
// Platform-Specific Dynamic Loading
//-----------------------------------------------------------------------------

void* PLM_PLUGIN_MANAGER::LoadLibrary( const wxString& aPath )
{
#ifdef __WXMSW__
    return static_cast<void*>( ::LoadLibraryW( aPath.wc_str() ) );
#else
    return dlopen( aPath.ToUTF8(), RTLD_NOW | RTLD_LOCAL );
#endif
}


void PLM_PLUGIN_MANAGER::UnloadLibrary( void* aHandle )
{
    if( !aHandle )
        return;

#ifdef __WXMSW__
    ::FreeLibrary( static_cast<HMODULE>( aHandle ) );
#else
    dlclose( aHandle );
#endif
}


void* PLM_PLUGIN_MANAGER::GetSymbol( void* aHandle, const char* aSymbol )
{
    if( !aHandle || !aSymbol )
        return nullptr;

#ifdef __WXMSW__
    return reinterpret_cast<void*>( ::GetProcAddress( static_cast<HMODULE>( aHandle ), aSymbol ) );
#else
    return dlsym( aHandle, aSymbol );
#endif
}


wxString PLM_PLUGIN_MANAGER::GetLibraryError()
{
#ifdef __WXMSW__
    DWORD error = ::GetLastError();

    if( error == 0 )
        return wxEmptyString;

    LPWSTR messageBuffer = nullptr;
    size_t size = FormatMessageW( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                                          | FORMAT_MESSAGE_IGNORE_INSERTS,
                                  nullptr, error, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
                                  reinterpret_cast<LPWSTR>( &messageBuffer ), 0, nullptr );

    wxString message( messageBuffer, size );
    LocalFree( messageBuffer );

    return message.Trim();
#else
    const char* error = dlerror();

    if( error )
        return wxString::FromUTF8( error );

    return wxEmptyString;
#endif
}
