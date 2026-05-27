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
 * @file plm_state_persistence.h
 * @brief State persistence for PLM data across sessions.
 *
 * This header defines the PLM_STATE_PERSISTENCE class which saves and
 * restores PLM state (bindings, dirty tracking, sync state) to disk
 * so it survives application restarts.
 */

#ifndef PLM_STATE_PERSISTENCE_H
#define PLM_STATE_PERSISTENCE_H

#include "plm_dirty_tracker.h"
#include "plm_project_binding.h"
#include "plm_sync_state.h"

#include <wx/string.h>
#include <cstdint>
#include <map>
#include <mutex>
#include <vector>


/**
 * Result of a state persistence operation.
 */
enum class PLM_PERSIST_RESULT
{
    SUCCESS,            ///< Operation completed successfully
    FILE_NOT_FOUND,     ///< State file does not exist
    FILE_CORRUPT,       ///< State file is invalid or corrupt
    WRITE_ERROR,        ///< Failed to write state file
    READ_ERROR,         ///< Failed to read state file
    VERSION_MISMATCH    ///< State file format version not supported
};


/**
 * Complete PLM state snapshot for a project.
 *
 * Contains all persisted state for a single project that needs to
 * survive application restarts.
 */
struct PLM_PROJECT_STATE
{
    int                               version = 1;        ///< State format version
    PLM_PROJECT_BINDING               binding;            ///< Project-to-PLM binding
    PLM_SYNC_INFO                     sync_info;          ///< Sync state info
    std::map<wxString, PLM_FILE_STATE> file_states;       ///< Per-file dirty state
    int64_t                           saved_at = 0;       ///< When state was saved

    PLM_PROJECT_STATE() = default;

    /**
     * Check if state contains valid data.
     * @return True if binding is valid.
     */
    bool IsValid() const { return binding.IsValid(); }

    /**
     * Check if state is stale.
     * @param aMaxAgeSeconds Maximum age in seconds.
     * @return True if state is older than max age.
     */
    bool IsStale( int64_t aMaxAgeSeconds = 86400 ) const
    {
        if( saved_at == 0 )
            return true;

        int64_t now = std::time( nullptr );
        return ( now - saved_at ) > aMaxAgeSeconds;
    }

    /**
     * Clear all state data.
     */
    void Clear()
    {
        version = 1;
        binding.Clear();
        sync_info = PLM_SYNC_INFO();
        file_states.clear();
        saved_at = 0;
    }
};


/**
 * Configuration for state persistence.
 */
struct PLM_PERSISTENCE_CONFIG
{
    wxString cache_directory;       ///< Base directory for state files
    bool     auto_save = true;      ///< Automatically save on changes
    bool     validate_on_load = true; ///< Validate state after loading
    int      format_version = 1;    ///< Current format version

    PLM_PERSISTENCE_CONFIG() = default;

    /**
     * Get the state file path for a project.
     * @param aProjectPath Local project path.
     * @return Path to state file.
     */
    wxString GetStateFilePath( const wxString& aProjectPath ) const;
};


/**
 * Manages persistence of PLM state to disk.
 *
 * Saves and restores PLM state for projects so it survives application
 * restarts. State is stored in the KiCad cache directory as JSON files.
 *
 * State files are named based on a hash of the project path to avoid
 * conflicts and handle special characters in paths.
 *
 * Thread-safe: All public methods are protected by mutex.
 *
 * Usage:
 * @code
 * PLM_STATE_PERSISTENCE persistence;
 *
 * // Configure cache directory
 * PLM_PERSISTENCE_CONFIG config;
 * config.cache_directory = "/path/to/kicad/cache/plm";
 * persistence.SetConfig( config );
 *
 * // Save state on project close
 * PLM_PROJECT_STATE state;
 * state.binding = currentBinding;
 * state.sync_info = currentSyncInfo;
 * state.file_states = dirtyTracker.GetAllStates();
 * persistence.SaveState( projectPath, state );
 *
 * // Restore state on project open
 * PLM_PROJECT_STATE restored;
 * if( persistence.LoadState( projectPath, restored ) == PLM_PERSIST_RESULT::SUCCESS )
 * {
 *     // Apply restored state
 * }
 * @endcode
 */
class PLM_STATE_PERSISTENCE
{
public:
    PLM_STATE_PERSISTENCE();
    ~PLM_STATE_PERSISTENCE() = default;

    // Non-copyable
    PLM_STATE_PERSISTENCE( const PLM_STATE_PERSISTENCE& ) = delete;
    PLM_STATE_PERSISTENCE& operator=( const PLM_STATE_PERSISTENCE& ) = delete;

    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------

    /**
     * Set persistence configuration.
     *
     * @param aConfig Configuration options.
     */
    void SetConfig( const PLM_PERSISTENCE_CONFIG& aConfig );

    /**
     * Get current configuration.
     *
     * @return Current configuration.
     */
    PLM_PERSISTENCE_CONFIG GetConfig() const;

    /**
     * Set the cache directory.
     *
     * Convenience method to set just the cache directory.
     *
     * @param aCacheDir Path to cache directory.
     */
    void SetCacheDirectory( const wxString& aCacheDir );

    //-------------------------------------------------------------------------
    // State Operations
    //-------------------------------------------------------------------------

    /**
     * Save state for a project.
     *
     * @param aProjectPath Local project path.
     * @param aState State to save.
     * @return Result of save operation.
     */
    PLM_PERSIST_RESULT SaveState( const wxString& aProjectPath,
                                   const PLM_PROJECT_STATE& aState );

    /**
     * Load state for a project.
     *
     * @param aProjectPath Local project path.
     * @param aState Output state structure.
     * @return Result of load operation.
     */
    PLM_PERSIST_RESULT LoadState( const wxString& aProjectPath,
                                   PLM_PROJECT_STATE& aState ) const;

    /**
     * Delete state for a project.
     *
     * @param aProjectPath Local project path.
     * @return True if state was deleted or didn't exist.
     */
    bool DeleteState( const wxString& aProjectPath );

    /**
     * Check if state exists for a project.
     *
     * @param aProjectPath Local project path.
     * @return True if state file exists.
     */
    bool HasState( const wxString& aProjectPath ) const;

    //-------------------------------------------------------------------------
    // Bulk Operations
    //-------------------------------------------------------------------------

    /**
     * Get paths of all projects with saved state.
     *
     * @return Vector of project paths.
     */
    std::vector<wxString> GetPersistedProjects() const;

    /**
     * Delete all persisted state.
     *
     * @return Number of state files deleted.
     */
    size_t DeleteAllState();

    /**
     * Clean up stale state files.
     *
     * Deletes state files older than the specified age.
     *
     * @param aMaxAgeSeconds Maximum age in seconds.
     * @return Number of files deleted.
     */
    size_t CleanupStaleState( int64_t aMaxAgeSeconds = 2592000 );

    //-------------------------------------------------------------------------
    // Utilities
    //-------------------------------------------------------------------------

    /**
     * Get the state file path for a project.
     *
     * @param aProjectPath Local project path.
     * @return Path to state file.
     */
    wxString GetStateFilePath( const wxString& aProjectPath ) const;

    /**
     * Compute hash of a project path for filename.
     *
     * @param aProjectPath Local project path.
     * @return Hash string suitable for filename.
     */
    static wxString ComputePathHash( const wxString& aProjectPath );

private:
    /**
     * Ensure cache directory exists.
     * @return True if directory exists or was created.
     */
    bool EnsureCacheDirectory() const;

    /**
     * Write state to JSON string.
     */
    wxString StateToJson( const PLM_PROJECT_STATE& aState ) const;

    /**
     * Parse state from JSON string.
     */
    PLM_PERSIST_RESULT JsonToState( const wxString& aJson, PLM_PROJECT_STATE& aState ) const;

    mutable std::mutex      m_mutex;
    PLM_PERSISTENCE_CONFIG  m_config;
};


#endif // PLM_STATE_PERSISTENCE_H
