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
 * @file plm_lock.h
 * @brief Locking structures for the PLM client interface.
 * @ingroup plm_operations
 *
 * This header defines structures for managing file locks in the PLM system.
 * Locks prevent concurrent modifications and can be advisory or mandatory
 * depending on PLM server configuration.
 */

#ifndef PLM_LOCK_H
#define PLM_LOCK_H

#include "plm_types.h"

#include <wx/string.h>
#include <cstdint>
#include <vector>


/**
 * Lock enforcement type.
 *
 * Determines how strictly locks are enforced by the PLM system.
 */
enum class PLM_LOCK_TYPE
{
    ADVISORY = 0,   ///< Lock is informational; users can override
    MANDATORY = 1   ///< Lock is enforced; modifications blocked without lock
};


/**
 * Information about the current lock state of a file.
 *
 * Provides details about who holds the lock and when it was acquired.
 */
struct PLM_LOCK_INFO
{
    bool          is_locked = false;    ///< True if the file is currently locked
    wxString      lock_owner;           ///< Username of the lock holder
    int64_t       lock_timestamp = 0;   ///< When lock was acquired (Unix epoch seconds)
    bool          locked_by_me = false; ///< True if current user holds the lock
    PLM_LOCK_TYPE lock_type = PLM_LOCK_TYPE::ADVISORY;  ///< Type of lock enforcement
    wxString      lock_comment;         ///< Optional comment provided when locking
    wxString      lock_machine;         ///< Machine name where lock was acquired
    std::vector<wxString> locked_files; ///< Individual locked file paths (for file-level locking)

    PLM_LOCK_INFO() = default;

    /**
     * Create lock info for an unlocked file.
     */
    static PLM_LOCK_INFO Unlocked()
    {
        return PLM_LOCK_INFO();
    }

    /**
     * Create lock info for a locked file.
     */
    static PLM_LOCK_INFO Locked( const wxString& aOwner, int64_t aTimestamp, bool aByMe,
                                 PLM_LOCK_TYPE aType = PLM_LOCK_TYPE::ADVISORY,
                                 const wxString& aComment = wxEmptyString )
    {
        PLM_LOCK_INFO info;
        info.is_locked = true;
        info.lock_owner = aOwner;
        info.lock_timestamp = aTimestamp;
        info.locked_by_me = aByMe;
        info.lock_type = aType;
        info.lock_comment = aComment;
        return info;
    }

    /**
     * Check if the lock is mandatory (enforced).
     * @return True if modifications are blocked without holding the lock.
     */
    bool IsMandatory() const { return lock_type == PLM_LOCK_TYPE::MANDATORY; }

    /**
     * Check if the lock is advisory (informational only).
     * @return True if the lock can be overridden.
     */
    bool IsAdvisory() const { return lock_type == PLM_LOCK_TYPE::ADVISORY; }

    /**
     * Check if the file can be safely modified by the current user.
     * @return True if not locked, or locked by the current user.
     */
    bool CanModify() const { return !is_locked || locked_by_me; }
};


/**
 * Request to acquire or modify a lock.
 */
struct PLM_LOCK_REQUEST
{
    wxString              project_id;    ///< ID of the project/file to lock
    bool                  force = false; ///< Force lock even if held by another user
    wxString              comment;       ///< Optional comment explaining the lock
    std::vector<wxString> file_paths;    ///< Specific files to lock (empty = whole project)

    PLM_LOCK_REQUEST() = default;

    explicit PLM_LOCK_REQUEST( const wxString& aProjectId, bool aForce = false,
                               const wxString& aComment = wxEmptyString ) :
            project_id( aProjectId ),
            force( aForce ),
            comment( aComment )
    {
    }
};


/**
 * Request to release a lock.
 */
struct PLM_UNLOCK_REQUEST
{
    wxString project_id;    ///< ID of the project/file to unlock
    bool     force = false; ///< Force unlock even if held by another user

    PLM_UNLOCK_REQUEST() = default;

    explicit PLM_UNLOCK_REQUEST( const wxString& aProjectId, bool aForce = false ) :
            project_id( aProjectId ),
            force( aForce )
    {
    }
};


/**
 * Result of a lock or unlock operation.
 */
struct PLM_LOCK_RESULT
{
    bool           success = false;     ///< True if operation succeeded
    PLM_ERROR_CODE error_code = PLM_ERROR_CODE::SUCCESS;  ///< Error code if failed
    wxString       error_message;       ///< Human-readable error message
    PLM_LOCK_INFO  lock_info;           ///< Updated lock information

    PLM_LOCK_RESULT() = default;

    /**
     * Create a successful lock result.
     */
    static PLM_LOCK_RESULT Success( const PLM_LOCK_INFO& aInfo )
    {
        PLM_LOCK_RESULT result;
        result.success = true;
        result.lock_info = aInfo;
        return result;
    }

    /**
     * Create a failed lock result.
     */
    static PLM_LOCK_RESULT Failure( PLM_ERROR_CODE aCode, const wxString& aMessage,
                                    const PLM_LOCK_INFO& aInfo = PLM_LOCK_INFO() )
    {
        PLM_LOCK_RESULT result;
        result.success = false;
        result.error_code = aCode;
        result.error_message = aMessage;
        result.lock_info = aInfo;
        return result;
    }

    /**
     * Create a lock conflict result (locked by another user).
     */
    static PLM_LOCK_RESULT Conflict( const PLM_LOCK_INFO& aInfo )
    {
        PLM_LOCK_RESULT result;
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_LOCK_CONFLICT;
        result.error_message = wxString::Format( "File is locked by %s", aInfo.lock_owner );
        result.lock_info = aInfo;
        return result;
    }
};


#endif // PLM_LOCK_H
