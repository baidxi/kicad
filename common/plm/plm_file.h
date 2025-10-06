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
 * @file plm_file.h
 * @brief File and project information structures for the PLM client interface.
 *
 * This header defines structures for representing files/projects in the PLM
 * system, along with options for checkout and checkin operations.
 */

#ifndef PLM_FILE_H
#define PLM_FILE_H

#include "plm_types.h"

#include <wx/string.h>
#include <cstdint>
#include <functional>
#include <vector>


/**
 * Information about a file or project in the PLM system.
 *
 * Represents the metadata for a managed file including its current state,
 * revision, and lock status.
 */
struct PLM_FILE_INFO
{
    wxString           id;                  ///< Unique file/project identifier
    wxString           name;                ///< Display name of the file
    wxString           revision;            ///< Current revision identifier
    PLM_STATE_CATEGORY state_category;      ///< Abstracted lifecycle state
    wxString           native_state;        ///< PLM-specific state name
    wxString           lock_owner;          ///< Username of lock holder (empty if unlocked)
    int64_t            modified_timestamp = 0;  ///< Last modification time (Unix epoch seconds)
    wxString           folder_id;           ///< Parent folder identifier
    wxString           description;         ///< Optional description

    // Lineage fields for cloned projects
    wxString           source_project_id;   ///< Source project if cloned (empty if original)
    wxString           source_revision_id;  ///< Source revision that was cloned
    wxString           clone_type;          ///< How this project was created (e.g., "clone", "template")

    PLM_FILE_INFO() : state_category( PLM_STATE_CATEGORY::EDITABLE ) {}

    PLM_FILE_INFO( const wxString& aId, const wxString& aName, const wxString& aRevision,
                   PLM_STATE_CATEGORY aStateCategory, const wxString& aNativeState = wxEmptyString,
                   const wxString& aLockOwner = wxEmptyString, int64_t aModifiedTimestamp = 0 ) :
            id( aId ),
            name( aName ),
            revision( aRevision ),
            state_category( aStateCategory ),
            native_state( aNativeState ),
            lock_owner( aLockOwner ),
            modified_timestamp( aModifiedTimestamp )
    {
    }

    /**
     * Check if the file is currently locked.
     * @return True if the file is locked by any user.
     */
    bool IsLocked() const { return !lock_owner.IsEmpty(); }

    /**
     * Check if the file is locked by a specific user.
     * @param aUsername Username to check.
     * @return True if locked by the specified user.
     */
    bool IsLockedBy( const wxString& aUsername ) const
    {
        return !lock_owner.IsEmpty() && lock_owner == aUsername;
    }

    /**
     * Check if the file can be edited based on its state.
     * @return True if the state category allows editing.
     */
    bool IsEditable() const { return state_category == PLM_STATE_CATEGORY::EDITABLE; }

    /**
     * Check if the file is in a released state.
     * @return True if the file has been released.
     */
    bool IsReleased() const { return state_category == PLM_STATE_CATEGORY::RELEASED; }
};


/**
 * Options for checking out a file from the PLM system.
 */
struct PLM_CHECKOUT_OPTIONS
{
    bool     acquire_lock = true;   ///< Request lock when checking out
    wxString revision;              ///< Specific revision to checkout (empty for latest)
    wxString local_path;            ///< Local directory path for checkout

    PLM_CHECKOUT_OPTIONS() = default;

    explicit PLM_CHECKOUT_OPTIONS( const wxString& aLocalPath, bool aAcquireLock = true,
                                   const wxString& aRevision = wxEmptyString ) :
            acquire_lock( aAcquireLock ),
            revision( aRevision ),
            local_path( aLocalPath )
    {
    }
};


/**
 * Options for checking in a file to the PLM system.
 */
struct PLM_CHECKIN_OPTIONS
{
    wxString revision_comment;      ///< Comment describing the changes
    wxString eco_id;                ///< Engineering Change Order ID (if required)
    bool     release_lock = true;   ///< Release lock after checkin
    bool     keep_local = true;     ///< Keep local copy after checkin

    PLM_CHECKIN_OPTIONS() = default;

    explicit PLM_CHECKIN_OPTIONS( const wxString& aComment, bool aReleaseLock = true,
                                  const wxString& aEcoId = wxEmptyString ) :
            revision_comment( aComment ),
            eco_id( aEcoId ),
            release_lock( aReleaseLock ),
            keep_local( true )
    {
    }
};


/**
 * Result of a checkout operation.
 */
struct PLM_CHECKOUT_RESULT
{
    bool           success = false;     ///< True if checkout succeeded
    PLM_ERROR_CODE error_code = PLM_ERROR_CODE::SUCCESS;  ///< Error code if failed
    wxString       error_message;       ///< Human-readable error message
    wxString       local_path;          ///< Path where files were checked out
    PLM_FILE_INFO  file_info;           ///< Updated file info after checkout

    PLM_CHECKOUT_RESULT() = default;

    static PLM_CHECKOUT_RESULT Success( const wxString& aLocalPath, const PLM_FILE_INFO& aInfo )
    {
        PLM_CHECKOUT_RESULT result;
        result.success = true;
        result.local_path = aLocalPath;
        result.file_info = aInfo;
        return result;
    }

    static PLM_CHECKOUT_RESULT Failure( PLM_ERROR_CODE aCode, const wxString& aMessage )
    {
        PLM_CHECKOUT_RESULT result;
        result.success = false;
        result.error_code = aCode;
        result.error_message = aMessage;
        return result;
    }
};


/**
 * Result of a checkin operation.
 */
struct PLM_CHECKIN_RESULT
{
    bool           success = false;     ///< True if checkin succeeded
    PLM_ERROR_CODE error_code = PLM_ERROR_CODE::SUCCESS;  ///< Error code if failed
    wxString       error_message;       ///< Human-readable error message
    wxString       new_revision;        ///< New revision identifier after checkin
    PLM_FILE_INFO  file_info;           ///< Updated file info after checkin

    PLM_CHECKIN_RESULT() = default;

    static PLM_CHECKIN_RESULT Success( const wxString& aNewRevision, const PLM_FILE_INFO& aInfo )
    {
        PLM_CHECKIN_RESULT result;
        result.success = true;
        result.new_revision = aNewRevision;
        result.file_info = aInfo;
        return result;
    }

    static PLM_CHECKIN_RESULT Failure( PLM_ERROR_CODE aCode, const wxString& aMessage )
    {
        PLM_CHECKIN_RESULT result;
        result.success = false;
        result.error_code = aCode;
        result.error_message = aMessage;
        return result;
    }
};


/**
 * Progress callback for batch operations.
 *
 * Called by batch operations to report progress. The callback receives:
 * - Current item index (0-based)
 * - Total item count
 * - Current item project ID
 * - Whether the current item succeeded
 *
 * Return false from the callback to cancel the batch operation.
 */
using PLM_BATCH_PROGRESS_CALLBACK = std::function<bool( size_t aCurrent, size_t aTotal,
                                                         const wxString& aProjectId,
                                                         bool aSuccess )>;


/**
 * Result for a single item in a batch operation.
 */
struct PLM_BATCH_ITEM_RESULT
{
    wxString       project_id;       ///< Project ID that was processed
    bool           success = false;  ///< True if operation succeeded
    PLM_ERROR_CODE error_code = PLM_ERROR_CODE::SUCCESS;  ///< Error code if failed
    wxString       error_message;    ///< Human-readable error message
    wxString       local_path;       ///< Local path for checkout, empty for checkin
    wxString       new_revision;     ///< New revision for checkin, empty for checkout

    PLM_BATCH_ITEM_RESULT() = default;

    PLM_BATCH_ITEM_RESULT( const wxString& aProjectId, bool aSuccess,
                           PLM_ERROR_CODE aCode = PLM_ERROR_CODE::SUCCESS,
                           const wxString& aMessage = wxEmptyString ) :
            project_id( aProjectId ),
            success( aSuccess ),
            error_code( aCode ),
            error_message( aMessage )
    {
    }
};


/**
 * Result of a batch checkout or checkin operation.
 */
struct PLM_BATCH_RESULT
{
    bool success = false;                          ///< True if all operations succeeded
    bool cancelled = false;                        ///< True if operation was cancelled
    size_t total_count = 0;                        ///< Total number of items attempted
    size_t success_count = 0;                      ///< Number of successful operations
    size_t failure_count = 0;                      ///< Number of failed operations
    std::vector<PLM_BATCH_ITEM_RESULT> results;    ///< Individual results for each item

    PLM_BATCH_RESULT() = default;

    /**
     * Add a result for an individual item.
     */
    void AddResult( const PLM_BATCH_ITEM_RESULT& aResult )
    {
        results.push_back( aResult );
        total_count = results.size();

        if( aResult.success )
            success_count++;
        else
            failure_count++;

        success = ( failure_count == 0 );
    }

    /**
     * Get the result for a specific project.
     * @return Pointer to result, or nullptr if not found.
     */
    const PLM_BATCH_ITEM_RESULT* FindResult( const wxString& aProjectId ) const
    {
        for( const auto& r : results )
        {
            if( r.project_id == aProjectId )
                return &r;
        }

        return nullptr;
    }
};


/**
 * Scope flags for project cloning.
 */
enum class PLM_CLONE_SCOPE
{
    FULL = 0,           ///< Clone all project files and history
    FILES_ONLY = 1,     ///< Clone files without revision history
    STRUCTURE_ONLY = 2  ///< Clone folder structure and metadata only
};


/**
 * Options for cloning a project.
 */
struct PLM_CLONE_OPTIONS
{
    PLM_CLONE_SCOPE scope = PLM_CLONE_SCOPE::FULL;
    wxString        source_project_id;  ///< Project to clone from (empty = new project)
    wxString        source_revision_id; ///< Specific revision to clone (empty = latest)
    bool            include_attributes = true; ///< Copy custom attributes from source

    PLM_CLONE_OPTIONS() = default;

    PLM_CLONE_OPTIONS( const wxString& aSourceId, PLM_CLONE_SCOPE aScope = PLM_CLONE_SCOPE::FULL ) :
            scope( aScope ),
            source_project_id( aSourceId )
    {
    }
};


/**
 * Result of cancelling a checkout.
 */
struct PLM_CANCEL_CHECKOUT_RESULT
{
    bool           success = false;
    PLM_ERROR_CODE error_code = PLM_ERROR_CODE::SUCCESS;
    wxString       error_message;
    bool           files_restored = false; ///< True if local files were reverted
    bool           lock_released = false;  ///< True if lock was released

    PLM_CANCEL_CHECKOUT_RESULT() = default;

    static PLM_CANCEL_CHECKOUT_RESULT Success()
    {
        PLM_CANCEL_CHECKOUT_RESULT r;
        r.success = true;
        r.files_restored = true;
        r.lock_released = true;
        return r;
    }

    static PLM_CANCEL_CHECKOUT_RESULT Failure( PLM_ERROR_CODE aCode, const wxString& aMessage )
    {
        PLM_CANCEL_CHECKOUT_RESULT r;
        r.error_code = aCode;
        r.error_message = aMessage;
        return r;
    }
};


/**
 * Write a common `.kicad_plm` checkout metadata file.
 *
 * All PLM plugins should call this during Checkout() so that the PLM manager
 * can identify which plugin owns a given checkout directory.
 *
 * @param aLocalPath  Directory where the checkout lives.
 * @param aPluginId   Plugin identifier (e.g. "plm-git", "odoo-plm").
 * @param aProjectId  PLM project / product identifier.
 * @param aRevision   Revision string (commit hash, version number, etc.).
 * @param aUser       Authenticated username.
 * @param aServerUrl  Server URL the plugin connected to.
 * @return True if the file was written successfully.
 */
bool WritePLMCheckoutMetadata( const wxString& aLocalPath,
                               const wxString& aPluginId,
                               const wxString& aProjectId,
                               const wxString& aRevision,
                               const wxString& aUser,
                               const wxString& aServerUrl );


#endif // PLM_FILE_H
