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
 * @file plm_plugin_manager.h
 * @brief PLM plugin discovery and management.
 *
 * This header defines the PLM_PLUGIN_MANAGER class which handles
 * discovering, loading, and managing PLM plugin shared libraries.
 */

#ifndef PLM_PLUGIN_MANAGER_H
#define PLM_PLUGIN_MANAGER_H

#include "plm_plugin.h"

#include <wx/string.h>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>


/**
 * Information about a discovered plugin file.
 *
 * Contains metadata about a plugin shared library that was found
 * during directory scanning. Does not include loaded plugin info
 * until the plugin is actually loaded.
 */
struct PLM_DISCOVERED_PLUGIN
{
    wxString path;           ///< Full path to the plugin shared library
    wxString filename;       ///< Filename without path
    wxString plugin_id;      ///< Extracted plugin ID from filename
    bool     loaded = false; ///< True if plugin is currently loaded
    bool     compatible = true; ///< True if plugin passed version check
    wxString error_message;  ///< Error message if load failed

    PLM_DISCOVERED_PLUGIN() = default;

    PLM_DISCOVERED_PLUGIN( const wxString& aPath, const wxString& aFilename ) :
            path( aPath ),
            filename( aFilename )
    {
    }
};


/**
 * Handle to a loaded plugin.
 *
 * Wraps a loaded plugin instance along with metadata about the
 * plugin and the library handle for unloading.
 */
struct PLM_LOADED_PLUGIN
{
    PLM_DISCOVERED_PLUGIN   discovery_info; ///< Original discovery information
    PLM_PLUGIN_INFO         plugin_info;    ///< Plugin metadata from GetPluginInfo()
    PLM_PLUGIN*             plugin = nullptr; ///< Plugin instance
    void*                   library_handle = nullptr; ///< Platform library handle
    PLM_PLUGIN_DESTROY_FUNC destroy_func = nullptr; ///< Cleanup function

    PLM_LOADED_PLUGIN() = default;

    /**
     * Check if plugin is valid and loaded.
     * @return True if plugin instance is available.
     */
    bool IsValid() const { return plugin != nullptr; }
};


/**
 * Manages PLM plugin discovery, loading, and lifecycle.
 *
 * The plugin manager scans directories for plugin shared libraries,
 * loads them on demand, and handles proper cleanup. It maintains
 * a registry of discovered and loaded plugins.
 *
 * Usage:
 * @code
 * PLM_PLUGIN_MANAGER mgr;
 * mgr.AddPluginDirectory( "/usr/lib/kicad/plm" );
 * mgr.DiscoverPlugins();
 *
 * auto plugins = mgr.GetDiscoveredPlugins();
 * for( const auto& p : plugins )
 * {
 *     PLM_PLUGIN* plugin = mgr.LoadPlugin( p.plugin_id );
 *     if( plugin )
 *     {
 *         // Use plugin...
 *     }
 * }
 * @endcode
 */
class PLM_PLUGIN_MANAGER
{
public:
    /**
     * Construct a plugin manager.
     *
     * Does not automatically scan for plugins. Call AddPluginDirectory()
     * and DiscoverPlugins() to find plugins.
     */
    PLM_PLUGIN_MANAGER();

    /**
     * Destructor unloads all plugins.
     */
    ~PLM_PLUGIN_MANAGER();

    // Non-copyable
    PLM_PLUGIN_MANAGER( const PLM_PLUGIN_MANAGER& ) = delete;
    PLM_PLUGIN_MANAGER& operator=( const PLM_PLUGIN_MANAGER& ) = delete;

    //-------------------------------------------------------------------------
    // Plugin Directory Management
    //-------------------------------------------------------------------------

    /**
     * Add a directory to scan for plugins.
     *
     * Directories are scanned in order added. Later directories
     * can override earlier ones if plugins have the same ID.
     *
     * @param aPath Directory path to add.
     */
    void AddPluginDirectory( const wxString& aPath );

    /**
     * Get the list of plugin directories.
     *
     * @return Vector of directory paths.
     */
    std::vector<wxString> GetPluginDirectories() const { return m_pluginDirs; }

    /**
     * Clear all plugin directories.
     */
    void ClearPluginDirectories() { m_pluginDirs.clear(); }

    /**
     * Add the standard KiCad plugin directories.
     *
     * Adds system and user plugin directories based on platform.
     */
    void AddStandardDirectories();

    //-------------------------------------------------------------------------
    // Plugin Loading
    //-------------------------------------------------------------------------

    /**
     * Load a plugin by ID.
     *
     * Loads the shared library, calls the factory function, and verifies
     * API compatibility. The plugin's OnLoad() method is called before
     * returning.
     *
     * @param aPluginId Plugin identifier (from discovered plugins).
     * @return Pointer to loaded plugin, or nullptr on failure.
     */
    PLM_PLUGIN* LoadPlugin( const wxString& aPluginId );

    /**
     * Load a plugin from a specific path.
     *
     * Loads a plugin from an explicit path rather than from discovered
     * plugins. Useful for development and testing.
     *
     * @param aPath Full path to the plugin shared library.
     * @return Pointer to loaded plugin, or nullptr on failure.
     */
    PLM_PLUGIN* LoadPluginFromPath( const wxString& aPath );

    /**
     * Unload a specific plugin.
     *
     * Calls the plugin's OnUnload() method, destroys the instance,
     * and unloads the shared library.
     *
     * @param aPluginId Plugin identifier to unload.
     * @return True if plugin was unloaded.
     */
    bool UnloadPlugin( const wxString& aPluginId );

    /**
     * Unload all loaded plugins.
     *
     * Called automatically by the destructor.
     */
    void UnloadAllPlugins();

    /**
     * Get a loaded plugin by ID.
     *
     * @param aPluginId Plugin identifier.
     * @return Pointer to plugin, or nullptr if not loaded.
     */
    PLM_PLUGIN* GetPlugin( const wxString& aPluginId );

    /**
     * Get all loaded plugins.
     *
     * @return Vector of loaded plugin information.
     */
    std::vector<PLM_LOADED_PLUGIN> GetLoadedPlugins() const { return m_loadedPlugins; }

    /**
     * Check if a plugin is loaded.
     *
     * @param aPluginId Plugin identifier.
     * @return True if plugin is currently loaded.
     */
    bool IsPluginLoaded( const wxString& aPluginId ) const;

    /**
     * Get last error message from loading operations.
     *
     * @return Error message from last failed operation.
     */
    wxString GetLastError() const { return m_lastError; }

    //-------------------------------------------------------------------------
    // Active Plugin Management
    //-------------------------------------------------------------------------

    /**
     * Set the active plugin.
     *
     * Switches to a different plugin. The previous active plugin is
     * disconnected but remains loaded. Only one plugin can be active
     * at a time.
     *
     * @param aPluginId Plugin ID to make active. Empty string clears active.
     * @return True if switch was successful.
     */
    bool SetActivePlugin( const wxString& aPluginId );

    /**
     * Get the currently active plugin.
     *
     * @return Pointer to active plugin, or nullptr if none active.
     */
    PLM_PLUGIN* GetActivePlugin();

    /**
     * Get the ID of the currently active plugin.
     *
     * @return Active plugin ID, or empty string if none active.
     */
    wxString GetActivePluginId() const { return m_activePluginId; }

    /**
     * Check if there is an active plugin.
     *
     * @return True if a plugin is currently active.
     */
    bool HasActivePlugin() const { return !m_activePluginId.IsEmpty(); }

    /**
     * Get information about the active plugin.
     *
     * @return Pointer to active plugin info, or nullptr if none active.
     */
    const PLM_LOADED_PLUGIN* GetActivePluginInfo() const;

    //-------------------------------------------------------------------------
    // Plugin Discovery
    //-------------------------------------------------------------------------

    /**
     * Scan directories for plugin shared libraries.
     *
     * Scans all registered directories for files matching the plugin
     * naming convention. Updates the discovered plugins list.
     *
     * @return Number of plugins discovered.
     */
    size_t DiscoverPlugins();

    /**
     * Get list of discovered plugins.
     *
     * @return Vector of discovered plugin information.
     */
    std::vector<PLM_DISCOVERED_PLUGIN> GetDiscoveredPlugins() const { return m_discoveredPlugins; }

    /**
     * Find a discovered plugin by ID.
     *
     * @param aPluginId Plugin identifier to find.
     * @return Pointer to discovered plugin info, or nullptr if not found.
     */
    const PLM_DISCOVERED_PLUGIN* FindDiscoveredPlugin( const wxString& aPluginId ) const;

    /**
     * Check if any plugins were discovered.
     *
     * @return True if at least one plugin was found.
     */
    bool HasPlugins() const { return !m_discoveredPlugins.empty(); }

    //-------------------------------------------------------------------------
    // Platform-Specific Helpers
    //-------------------------------------------------------------------------

    /**
     * Get the shared library file extension for this platform.
     *
     * @return Extension including dot (e.g., ".so", ".dll", ".dylib").
     */
    static wxString GetSharedLibraryExtension();

    /**
     * Get the plugin filename prefix.
     *
     * @return Prefix (e.g., "libplm-" on Unix, "plm-" on Windows).
     */
    static wxString GetPluginPrefix();

    /**
     * Check if a filename matches the plugin naming convention.
     *
     * @param aFilename Filename to check.
     * @return True if filename looks like a PLM plugin.
     */
    static bool IsPluginFilename( const wxString& aFilename );

    /**
     * Extract plugin ID from filename.
     *
     * @param aFilename Plugin filename.
     * @return Plugin ID, or empty string if not a valid plugin name.
     */
    static wxString ExtractPluginId( const wxString& aFilename );

    //-------------------------------------------------------------------------
    // Version Checking
    //-------------------------------------------------------------------------

    /**
     * Parse a combined API version number into components.
     *
     * The combined version format is (major * 1000) + minor.
     *
     * @param aVersion Combined version number.
     * @param aMajor Output major version.
     * @param aMinor Output minor version.
     */
    static void ParseAPIVersion( int aVersion, int& aMajor, int& aMinor );

    /**
     * Check if a plugin version is compatible with the host.
     *
     * Compatibility rules:
     * - Major version must match exactly
     * - Plugin minor version may be <= host minor version
     * - Plugin minor > host minor generates a warning but is allowed
     *
     * @param aPluginVersion Combined plugin version number.
     * @param aErrorMsg Output error message if incompatible.
     * @return True if compatible (including with warnings).
     */
    static bool IsVersionCompatible( int aPluginVersion, wxString& aErrorMsg );

    /**
     * Get a human-readable version string.
     *
     * @param aVersion Combined version number.
     * @return Version string like "1.2".
     */
    static wxString FormatVersionString( int aVersion );

    /**
     * Get the host API version.
     *
     * @return Combined host API version number.
     */
    static int GetHostAPIVersion() { return PLM_API_VERSION; }

    /**
     * Get the host API major version.
     *
     * @return Host API major version.
     */
    static int GetHostAPIMajorVersion() { return PLM_API_VERSION_MAJOR; }

    /**
     * Get the host API minor version.
     *
     * @return Host API minor version.
     */
    static int GetHostAPIMinorVersion() { return PLM_API_VERSION_MINOR; }

    //-------------------------------------------------------------------------
    // Error Handling
    //-------------------------------------------------------------------------

    /**
     * Result of a plugin operation that may fail.
     *
     * Used by SafePluginCall() to return both success/failure status
     * and any error information.
     */
    struct PLM_PLUGIN_CALL_RESULT
    {
        bool     success = false;      ///< True if call completed without exception
        bool     threw_exception = false; ///< True if an exception was caught
        wxString error_message;        ///< Error message if call failed
        PLM_ERROR_CODE error_code = PLM_ERROR_CODE::SUCCESS; ///< Error code

        PLM_PLUGIN_CALL_RESULT() = default;

        static PLM_PLUGIN_CALL_RESULT Success()
        {
            PLM_PLUGIN_CALL_RESULT result;
            result.success = true;
            return result;
        }

        static PLM_PLUGIN_CALL_RESULT Failure( const wxString& aMessage,
                                                PLM_ERROR_CODE aCode = PLM_ERROR_CODE::PLUGIN_ERROR )
        {
            PLM_PLUGIN_CALL_RESULT result;
            result.success = false;
            result.error_message = aMessage;
            result.error_code = aCode;
            return result;
        }

        static PLM_PLUGIN_CALL_RESULT Exception( const wxString& aMessage )
        {
            PLM_PLUGIN_CALL_RESULT result;
            result.success = false;
            result.threw_exception = true;
            result.error_message = aMessage;
            result.error_code = PLM_ERROR_CODE::PLUGIN_EXCEPTION;
            return result;
        }
    };

    /**
     * Log a plugin error with context.
     *
     * Logs the error with plugin identification for debugging.
     *
     * @param aPluginId Plugin that caused the error.
     * @param aOperation Operation that failed.
     * @param aMessage Error message.
     */
    void LogPluginError( const wxString& aPluginId, const wxString& aOperation,
                         const wxString& aMessage );

    /**
     * Log a plugin warning with context.
     *
     * @param aPluginId Plugin that caused the warning.
     * @param aOperation Operation that generated the warning.
     * @param aMessage Warning message.
     */
    void LogPluginWarning( const wxString& aPluginId, const wxString& aOperation,
                           const wxString& aMessage );

private:
    /**
     * Scan a single directory for plugins.
     *
     * @param aPath Directory to scan.
     * @return Number of plugins found.
     */
    size_t ScanDirectory( const wxString& aPath );

    /**
     * Load a shared library.
     *
     * Platform-specific implementation.
     *
     * @param aPath Path to the library.
     * @return Library handle, or nullptr on failure.
     */
    void* LoadLibrary( const wxString& aPath );

    /**
     * Unload a shared library.
     *
     * @param aHandle Library handle to unload.
     */
    void UnloadLibrary( void* aHandle );

    /**
     * Get a symbol from a loaded library.
     *
     * @param aHandle Library handle.
     * @param aSymbol Symbol name.
     * @return Symbol address, or nullptr if not found.
     */
    void* GetSymbol( void* aHandle, const char* aSymbol );

    /**
     * Get platform-specific library load error.
     *
     * @return Error message from dynamic loader.
     */
    wxString GetLibraryError();

    /**
     * Find a loaded plugin by ID.
     *
     * @param aPluginId Plugin identifier.
     * @return Iterator to loaded plugin, or end() if not found.
     */
    std::vector<PLM_LOADED_PLUGIN>::iterator FindLoadedPlugin( const wxString& aPluginId );

    /**
     * Update discovery info for a loaded plugin.
     *
     * @param aPluginId Plugin identifier.
     * @param aLoaded Whether plugin is now loaded.
     * @param aError Error message if load failed.
     */
    void UpdateDiscoveryInfo( const wxString& aPluginId, bool aLoaded,
                              const wxString& aError = wxEmptyString );

    mutable std::recursive_mutex       m_mutex;             ///< Protects all member data
    std::vector<wxString>              m_pluginDirs;        ///< Directories to scan
    std::vector<PLM_DISCOVERED_PLUGIN> m_discoveredPlugins; ///< Found plugins
    std::vector<PLM_LOADED_PLUGIN>     m_loadedPlugins;     ///< Currently loaded plugins
    wxString                           m_activePluginId;    ///< Currently active plugin
    wxString                           m_lastError;         ///< Last error message
};


#endif // PLM_PLUGIN_MANAGER_H
