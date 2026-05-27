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
 * @file plm_lib_sync.h
 * @brief PLM library synchronization mechanism.
 */

#ifndef PLM_LIB_SYNC_H
#define PLM_LIB_SYNC_H

#include <kicommon.h>
#include "plm_lib_reference.h"
#include "plm_lib_provider.h"
#include "plm_lib_version_tracker.h"

#include <wx/string.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <vector>


/**
 * Status of a library sync operation.
 */
enum class PLM_SYNC_STATUS
{
    PENDING,     ///< Not yet started
    IN_PROGRESS, ///< Currently syncing
    SUCCEEDED,   ///< Sync completed successfully
    FAILED,      ///< Sync failed
    SKIPPED,     ///< Skipped (e.g., pinned or up-to-date)
    CANCELLED    ///< Cancelled by user
};


/**
 * Result of synchronizing a single library.
 */
struct PLM_LIB_SYNC_ITEM
{
    wxString        lib_id;          ///< Library identifier
    PLM_LIB_TYPE    lib_type;        ///< Library type
    PLM_SYNC_STATUS status;          ///< Sync status
    wxString        old_revision;    ///< Revision before sync
    wxString        new_revision;    ///< Revision after sync
    wxString        error_message;   ///< Error message if failed
    int64_t         download_bytes;  ///< Bytes downloaded
    int64_t         sync_time_ms;    ///< Time taken in milliseconds

    PLM_LIB_SYNC_ITEM() :
            lib_type( PLM_LIB_TYPE::SYMBOL ),
            status( PLM_SYNC_STATUS::PENDING ),
            download_bytes( 0 ),
            sync_time_ms( 0 )
    {
    }
};


/**
 * Overall result of a sync operation.
 */
struct PLM_LIB_SYNC_SUMMARY
{
    size_t  total_libraries;    ///< Total libraries in sync
    size_t  succeeded;          ///< Successfully synced
    size_t  failed;             ///< Failed to sync
    size_t  skipped;            ///< Skipped (up-to-date or pinned)
    size_t  cancelled;          ///< Cancelled
    int64_t total_bytes;        ///< Total bytes downloaded
    int64_t total_time_ms;      ///< Total time taken

    std::vector<PLM_LIB_SYNC_ITEM> items; ///< Individual results

    PLM_LIB_SYNC_SUMMARY() :
            total_libraries( 0 ),
            succeeded( 0 ),
            failed( 0 ),
            skipped( 0 ),
            cancelled( 0 ),
            total_bytes( 0 ),
            total_time_ms( 0 )
    {
    }

    bool IsComplete() const
    {
        return succeeded + failed + skipped + cancelled == total_libraries;
    }

    bool AllSucceeded() const { return failed == 0 && cancelled == 0; }

    bool HasFailures() const { return failed > 0; }
};


/**
 * Progress callback for sync operations.
 *
 * @param aItem Current library being synced.
 * @param aProgress Overall progress 0.0 to 1.0.
 * @return False to cancel sync.
 */
using PLM_SYNC_PROGRESS_CALLBACK = std::function<bool( const PLM_LIB_SYNC_ITEM& aItem,
                                                        float aProgress )>;


/**
 * Completion callback for sync operations.
 *
 * @param aSummary Final sync summary.
 */
using PLM_SYNC_COMPLETE_CALLBACK = std::function<void( const PLM_LIB_SYNC_SUMMARY& aSummary )>;


/**
 * PLM library synchronization manager.
 *
 * Coordinates synchronization of PLM-managed libraries with the server.
 * Downloads updated libraries, updates references, and handles conflicts.
 *
 * Usage:
 * @code
 * PLM_LIB_SYNC sync;
 * sync.SetProvider( &provider );
 * sync.SetReferenceManager( &refMgr );
 * sync.SetVersionTracker( &versionTracker );
 *
 * // Sync all referenced libraries
 * auto refs = refMgr.GetAllReferences();
 * sync.StartSync( refs );
 *
 * // Or sync with progress callback
 * sync.StartSyncAsync( refs,
 *     []( const PLM_LIB_SYNC_ITEM& item, float progress ) {
 *         UpdateProgressUI( progress );
 *         return !userCancelled;
 *     },
 *     []( const PLM_LIB_SYNC_SUMMARY& summary ) {
 *         ShowSyncComplete( summary );
 *     }
 * );
 * @endcode
 */
class KICOMMON_API PLM_LIB_SYNC
{
public:
    PLM_LIB_SYNC();
    ~PLM_LIB_SYNC();

    /**
     * Set library provider.
     *
     * @param aProvider Library provider for downloads.
     */
    void SetProvider( PLM_LIB_PROVIDER* aProvider );

    /**
     * Set reference manager.
     *
     * @param aRefMgr Reference manager to update.
     */
    void SetReferenceManager( PLM_LIB_REFERENCE_MANAGER* aRefMgr );

    /**
     * Set version tracker.
     *
     * @param aTracker Version tracker to update.
     */
    void SetVersionTracker( PLM_LIB_VERSION_TRACKER* aTracker );

    /**
     * Synchronize a single library.
     *
     * @param aRef Library reference to sync.
     * @param aForce Force download even if up-to-date.
     * @return Sync result.
     */
    PLM_LIB_SYNC_ITEM SyncLibrary( const PLM_LIB_REFERENCE& aRef, bool aForce = false );

    /**
     * Synchronize multiple libraries.
     *
     * Blocks until all libraries are synced.
     *
     * @param aRefs Library references to sync.
     * @param aForce Force download even if up-to-date.
     * @return Sync summary.
     */
    PLM_LIB_SYNC_SUMMARY SyncLibraries( const std::vector<PLM_LIB_REFERENCE>& aRefs,
                                         bool aForce = false );

    /**
     * Start asynchronous sync.
     *
     * Returns immediately. Progress reported via callbacks.
     *
     * @param aRefs Library references to sync.
     * @param aProgressCb Progress callback (return false to cancel).
     * @param aCompleteCb Completion callback.
     */
    void StartSyncAsync( const std::vector<PLM_LIB_REFERENCE>& aRefs,
                          PLM_SYNC_PROGRESS_CALLBACK aProgressCb,
                          PLM_SYNC_COMPLETE_CALLBACK aCompleteCb );

    /**
     * Cancel in-progress async sync.
     */
    void CancelSync();

    /**
     * Check if sync is in progress.
     *
     * @return True if syncing.
     */
    bool IsSyncing() const { return m_syncing; }

    /**
     * Wait for async sync to complete.
     */
    void WaitForSync();

    /**
     * Get libraries that need syncing.
     *
     * Checks version tracker for outdated libraries.
     *
     * @param aRefs References to check.
     * @return Library IDs that have updates available.
     */
    std::vector<wxString> GetLibrariesNeedingSync( const std::vector<PLM_LIB_REFERENCE>& aRefs );

    /**
     * Check if any libraries need syncing.
     *
     * @param aRefs References to check.
     * @return True if any have updates.
     */
    bool HasPendingUpdates( const std::vector<PLM_LIB_REFERENCE>& aRefs );

private:
    void runAsyncSync( std::vector<PLM_LIB_REFERENCE> aRefs,
                        PLM_SYNC_PROGRESS_CALLBACK aProgressCb,
                        PLM_SYNC_COMPLETE_CALLBACK aCompleteCb );

    PLM_LIB_PROVIDER*          m_provider;
    PLM_LIB_REFERENCE_MANAGER* m_refMgr;
    PLM_LIB_VERSION_TRACKER*   m_versionTracker;

    std::atomic<bool>              m_syncing;
    std::atomic<bool>              m_cancelRequested;
    std::unique_ptr<std::thread>   m_syncThread;
};


#endif // PLM_LIB_SYNC_H
