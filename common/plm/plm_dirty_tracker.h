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
 * @file plm_dirty_tracker.h
 * @brief Local modification tracking for PLM-managed files.
 *
 * This header defines the PLM_DIRTY_TRACKER class which tracks local
 * modifications to files since checkout from the PLM system.
 */

#ifndef PLM_DIRTY_TRACKER_H
#define PLM_DIRTY_TRACKER_H

#include <kicommon.h>
#include "plm_types.h"

#include <wx/string.h>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <vector>


/**
 * Modification state for a tracked file.
 */
enum class PLM_DIRTY_STATE
{
    CLEAN,          ///< File matches checkout state
    MODIFIED,       ///< File has local modifications
    NEW,            ///< File is new (not in PLM)
    DELETED,        ///< File was deleted locally
    CONFLICT,       ///< File has conflicting changes
    UNKNOWN         ///< State cannot be determined
};


/**
 * Information about a tracked file's state.
 */
struct PLM_FILE_STATE
{
    wxString        file_path;          ///< Path to the tracked file
    PLM_DIRTY_STATE state = PLM_DIRTY_STATE::UNKNOWN;
    wxString        checkout_hash;      ///< Hash at checkout time
    wxString        current_hash;       ///< Current file hash
    int64_t         checkout_time = 0;  ///< When file was checked out
    int64_t         last_modified = 0;  ///< Last modification time
    int64_t         last_checked = 0;   ///< Last time state was verified

    PLM_FILE_STATE() = default;

    explicit PLM_FILE_STATE( const wxString& aPath ) : file_path( aPath ) {}

    /**
     * Check if file has local modifications.
     * @return True if modified, new, or deleted.
     */
    bool IsDirty() const
    {
        return state == PLM_DIRTY_STATE::MODIFIED ||
               state == PLM_DIRTY_STATE::NEW ||
               state == PLM_DIRTY_STATE::DELETED;
    }

    /**
     * Check if file is in a conflict state.
     * @return True if there is a conflict.
     */
    bool HasConflict() const { return state == PLM_DIRTY_STATE::CONFLICT; }
};


/**
 * Callback type for dirty state change notifications.
 *
 * @param aFilePath Path to the file whose state changed.
 * @param aNewState New dirty state.
 */
using PLM_DIRTY_STATE_CALLBACK = std::function<void( const wxString& aFilePath,
                                                      PLM_DIRTY_STATE aNewState )>;


/**
 * Tracks local modifications for PLM-managed files.
 *
 * Maintains file hashes from checkout time and detects modifications
 * by comparing current hashes. Updates on file save notifications.
 *
 * Thread-safe: All public methods are protected by mutex.
 *
 * Usage:
 * @code
 * PLM_DIRTY_TRACKER tracker;
 *
 * // Set state change callback
 * tracker.SetStateCallback( []( const wxString& path, PLM_DIRTY_STATE state ) {
 *     // Update UI icon
 * });
 *
 * // Record checkout state
 * tracker.RecordCheckout( filePath, checkoutHash );
 *
 * // Notify of file save
 * tracker.OnFileSaved( filePath );
 *
 * // Check modification state
 * if( tracker.IsModified( filePath ) )
 * {
 *     // Show modified indicator
 * }
 *
 * // Get all modified files
 * std::vector<wxString> modified = tracker.GetModifiedFiles();
 * @endcode
 */
class KICOMMON_API PLM_DIRTY_TRACKER
{
public:
    /**
     * Get the singleton instance.
     *
     * @return Reference to the global dirty tracker.
     */
    static PLM_DIRTY_TRACKER& GetInstance();

    ~PLM_DIRTY_TRACKER() = default;

    // Non-copyable
    PLM_DIRTY_TRACKER( const PLM_DIRTY_TRACKER& ) = delete;
    PLM_DIRTY_TRACKER& operator=( const PLM_DIRTY_TRACKER& ) = delete;

private:
    PLM_DIRTY_TRACKER();

public:

    //-------------------------------------------------------------------------
    // Tracking Management
    //-------------------------------------------------------------------------

    /**
     * Record checkout state for a file.
     *
     * Call this when a file is checked out from PLM to record the baseline state.
     *
     * @param aFilePath Path to the file.
     * @param aCheckoutHash Hash of the file at checkout.
     */
    void RecordCheckout( const wxString& aFilePath, const wxString& aCheckoutHash );

    /**
     * Start tracking a new file.
     *
     * Call this for files created locally that don't exist in PLM yet.
     *
     * @param aFilePath Path to the file.
     */
    void TrackNewFile( const wxString& aFilePath );

    /**
     * Stop tracking a file.
     *
     * @param aFilePath Path to the file.
     */
    void StopTracking( const wxString& aFilePath );

    /**
     * Stop tracking all files.
     */
    void StopTrackingAll();

    /**
     * Check if a file is being tracked.
     *
     * @param aFilePath Path to the file.
     * @return True if file is tracked.
     */
    bool IsTracking( const wxString& aFilePath ) const;

    //-------------------------------------------------------------------------
    // State Queries
    //-------------------------------------------------------------------------

    /**
     * Get the dirty state for a file.
     *
     * @param aFilePath Path to the file.
     * @return Dirty state, or UNKNOWN if not tracked.
     */
    PLM_DIRTY_STATE GetState( const wxString& aFilePath ) const;

    /**
     * Get full state information for a file.
     *
     * @param aFilePath Path to the file.
     * @param aState Output state information.
     * @return True if file is tracked.
     */
    bool GetFileState( const wxString& aFilePath, PLM_FILE_STATE& aState ) const;

    /**
     * Check if a file has local modifications.
     *
     * @param aFilePath Path to the file.
     * @return True if modified, new, or deleted.
     */
    bool IsModified( const wxString& aFilePath ) const;

    /**
     * Check if any tracked files are modified.
     *
     * @return True if any files are dirty.
     */
    bool HasModifications() const;

    /**
     * Get all tracked file paths.
     *
     * @return Vector of file paths.
     */
    std::vector<wxString> GetTrackedFiles() const;

    /**
     * Get all modified file paths.
     *
     * @return Vector of modified file paths.
     */
    std::vector<wxString> GetModifiedFiles() const;

    /**
     * Get file states by dirty state.
     *
     * @param aState State to filter by.
     * @return Vector of file paths with the specified state.
     */
    std::vector<wxString> GetFilesByState( PLM_DIRTY_STATE aState ) const;

    //-------------------------------------------------------------------------
    // State Updates
    //-------------------------------------------------------------------------

    /**
     * Notify that a file was saved.
     *
     * Recalculates hash and updates dirty state.
     *
     * @param aFilePath Path to the file.
     */
    void OnFileSaved( const wxString& aFilePath );

    /**
     * Notify that a file was deleted.
     *
     * @param aFilePath Path to the file.
     */
    void OnFileDeleted( const wxString& aFilePath );

    /**
     * Refresh state for a file.
     *
     * Recalculates hash and updates dirty state.
     *
     * @param aFilePath Path to the file.
     */
    void RefreshState( const wxString& aFilePath );

    /**
     * Refresh state for all tracked files.
     */
    void RefreshAllStates();

    /**
     * Mark a file as clean after successful checkin.
     *
     * Updates the checkout hash to the current hash.
     *
     * @param aFilePath Path to the file.
     * @param aNewHash New checkout hash (current hash if empty).
     */
    void MarkClean( const wxString& aFilePath, const wxString& aNewHash = wxEmptyString );

    /**
     * Mark a file as having a conflict.
     *
     * @param aFilePath Path to the file.
     */
    void MarkConflict( const wxString& aFilePath );

    /**
     * Clear conflict state for a file.
     *
     * @param aFilePath Path to the file.
     */
    void ClearConflict( const wxString& aFilePath );

    //-------------------------------------------------------------------------
    // Callbacks
    //-------------------------------------------------------------------------

    /**
     * Set callback for state changes.
     *
     * @param aCallback State change callback function.
     */
    void SetStateCallback( PLM_DIRTY_STATE_CALLBACK aCallback );

    //-------------------------------------------------------------------------
    // Utilities
    //-------------------------------------------------------------------------

    /**
     * Calculate hash for a file.
     *
     * @param aFilePath Path to the file.
     * @return Hash string, or empty if file doesn't exist.
     */
    static wxString CalculateFileHash( const wxString& aFilePath );

    /**
     * Get display name for a dirty state.
     *
     * @param aState The state.
     * @return Human-readable state name.
     */
    static wxString GetStateDisplayName( PLM_DIRTY_STATE aState );

private:
    /**
     * Normalize a path for consistent keys.
     */
    static wxString NormalizePath( const wxString& aPath );

    /**
     * Fire a state change event.
     */
    void FireStateChange( const wxString& aFilePath, PLM_DIRTY_STATE aNewState );

    /**
     * Update state based on current hash comparison.
     */
    void UpdateState( PLM_FILE_STATE& aState );

    mutable std::mutex                   m_mutex;
    std::map<wxString, PLM_FILE_STATE>   m_trackedFiles;
    PLM_DIRTY_STATE_CALLBACK             m_stateCallback;
};


#endif // PLM_DIRTY_TRACKER_H
