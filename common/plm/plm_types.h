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
 * @file plm_types.h
 * @brief Core type definitions and enumerations for the PLM (Product Lifecycle Management)
 *        client interface.
 * @ingroup plm_types
 *
 * This header defines the fundamental types used throughout the PLM client system.
 * All enumerations use explicit integer values to maintain ABI stability across
 * plugin versions.
 */

#ifndef PLM_TYPES_H
#define PLM_TYPES_H

#include <wx/string.h>
#include <functional>

/**
 * PLM lifecycle state categories.
 *
 * These categories abstract away PLM-specific state names into behavioral categories
 * that KiCad can use to determine what operations are permitted on a project.
 */
enum class PLM_STATE_CATEGORY
{
    EDITABLE = 0,   ///< Project can be modified and checked in
    LOCKED = 1,     ///< Project is locked by another user, read-only access
    RELEASED = 2,       ///< Project is in a released state, requires formal change process
    PENDING_REVIEW = 3  ///< Project is submitted for review, modifications restricted
};


/**
 * PLM attribute data types.
 *
 * Defines the data types supported for PLM metadata attributes.
 * Used when rendering attribute editors and validating user input.
 */
enum class PLM_ATTR_TYPE
{
    STRING = 0,         ///< Free-form text value
    INTEGER = 1,        ///< Whole number value
    FLOAT = 2,          ///< Floating-point number value
    DATE = 3,           ///< Date without time component (YYYY-MM-DD)
    DATETIME = 4,       ///< Date and time (ISO 8601)
    BOOLEAN = 5,        ///< True/false value
    ENUM = 6,           ///< Selection from predefined list of values
    USER_REFERENCE = 7  ///< Reference to a user in the PLM system
};


/**
 * Types of changes detected between revisions.
 *
 * Used by the diff/merge system to categorize modifications.
 */
enum class PLM_CHANGE_TYPE
{
    ADDED = 0,      ///< Element exists in new revision but not in base
    REMOVED = 1,    ///< Element exists in base revision but not in new
    MOVED = 2,      ///< Element position changed between revisions
    MODIFIED = 3    ///< Element properties changed between revisions
};


/**
 * Standard PLM error codes.
 *
 * These codes provide a standardized way for plugins to report errors back to KiCad.
 * Plugins may extend this with custom codes starting at PLM_ERR_PLUGIN_SPECIFIC.
 */
enum class PLM_ERROR_CODE
{
    SUCCESS = 0,                    ///< Operation completed successfully

    // Authentication errors (100-199)
    ERR_AUTH_FAILED = 100,          ///< Authentication credentials invalid
    ERR_AUTH_EXPIRED = 101,         ///< Session or token has expired
    ERR_AUTH_REQUIRED = 102,        ///< Operation requires authentication
    ERR_AUTH_INSUFFICIENT = 103,    ///< User lacks permission for operation

    // Network errors (200-299)
    ERR_NETWORK_UNREACHABLE = 200,  ///< Cannot reach the PLM server
    ERR_NETWORK_TIMEOUT = 201,      ///< Operation timed out
    ERR_NETWORK_SSL = 202,          ///< SSL/TLS certificate validation failed

    // Lock errors (300-399)
    ERR_LOCK_CONFLICT = 300,        ///< Resource is locked by another user
    ERR_LOCK_NOT_HELD = 301,        ///< Attempted unlock without holding lock
    ERR_LOCK_EXPIRED = 302,         ///< Lock has expired

    // File/project errors (400-499)
    ERR_FILE_NOT_FOUND = 400,       ///< Requested file or project not found
    ERR_FILE_ACCESS_DENIED = 401,   ///< Access denied to file or project
    ERR_FILE_CORRUPT = 402,         ///< File is corrupt or unreadable
    ERR_FILE_TOO_LARGE = 403,       ///< File exceeds size limits
    ERR_FILE_EXISTS = 404,          ///< File already exists (for create operations)

    // Version/revision errors (500-599)
    ERR_VERSION_CONFLICT = 500,     ///< Local and server versions conflict
    ERR_VERSION_OUTDATED = 501,     ///< Local version is behind server version
    ERR_REVISION_NOT_FOUND = 502,   ///< Requested revision does not exist

    // State errors (600-699)
    ERR_STATE_INVALID = 600,        ///< Invalid state transition requested
    ERR_STATE_LOCKED = 601,         ///< Operation not allowed in current state
    ERR_ECO_REQUIRED = 602,         ///< Engineering Change Order required

    // Plugin errors (700-799)
    ERR_PLUGIN_NOT_FOUND = 700,     ///< Plugin could not be found
    ERR_PLUGIN_LOAD_FAILED = 701,   ///< Plugin failed to load
    ERR_PLUGIN_VERSION = 702,       ///< Plugin API version incompatible
    PLUGIN_ERROR = 703,             ///< General plugin operation error
    PLUGIN_EXCEPTION = 704,         ///< Plugin threw an unhandled exception

    // General errors (900-999)
    ERR_CANCELLED = 900,            ///< Operation was cancelled by user
    ERR_INTERNAL = 901,             ///< Internal plugin error
    ERR_NOT_SUPPORTED = 902,        ///< Operation not supported by plugin
    ERR_INVALID_PARAMETER = 903,    ///< Invalid parameter passed to operation

    // ECO/ECR lifecycle errors (1000-1099)
    ERR_ECO_NOT_FOUND = 1000,           ///< ECO with given ID does not exist
    ERR_ECO_INVALID_TRANSITION = 1001,  ///< Requested state transition is not allowed
    ERR_ECO_ALREADY_EXISTS = 1002,      ///< Duplicate ECO detected
    ERR_ECO_NOT_EDITABLE = 1003,        ///< ECO is in a state that does not allow edits
    ERR_ECO_ITEMS_INCOMPLETE = 1004,    ///< Cannot advance; items not all complete
    ERR_ECO_APPROVAL_REQUIRED = 1005,   ///< Operation requires prior approval
    ERR_ECO_TYPE_MISMATCH = 1006,       ///< Operation invalid for this ECO type
    ERR_ECO_LINKED_ECR_NOT_FOUND = 1007, ///< Linked ECR does not exist or is not approved
    ERR_ECO_PERMISSION_DENIED = 1008,   ///< User lacks permission for this ECO operation

    // BOM errors (1100-1199)
    ERR_BOM_NOT_FOUND = 1100,           ///< BOM does not exist for the given product
    ERR_BOM_DUPLICATE = 1101,           ///< A BOM of this type already exists
    ERR_BOM_LINE_INVALID = 1102,        ///< BOM line references an invalid or missing product
    ERR_BOM_CIRCULAR = 1103,            ///< Circular reference detected in hierarchical BOM
    ERR_BOM_LOCKED = 1104,              ///< BOM is locked by another user or process
    ERR_BOM_TYPE_MISMATCH = 1105,       ///< Operation not valid for this BOM type
    ERR_BOM_EXTRACTION_FAILED = 1106,   ///< Failed to extract BOM from schematic
    ERR_ROUTING_NOT_FOUND = 1107,       ///< Referenced routing does not exist
    ERR_WORKCENTER_NOT_FOUND = 1108,    ///< Referenced work center does not exist

    // Lifecycle state transition errors (1200-1299)
    ERR_TRANSITION_INVALID = 1200,          ///< Transition not valid from current state
    ERR_TRANSITION_PRECONDITION = 1201,     ///< Blocking preconditions not satisfied
    ERR_TRANSITION_PERMISSION = 1202,       ///< User lacks permission for this transition
    ERR_TRANSITION_CONCURRENT = 1203,       ///< State changed by another user since query
    ERR_TRANSITION_APPROVAL_REQUIRED = 1204, ///< Transition requires approval
    ERR_TRANSITION_APPROVAL_TIMEOUT = 1205, ///< Approval request timed out
    ERR_TRANSITION_APPROVAL_REJECTED = 1206, ///< Transition rejected by an approver
    ERR_TRANSITION_COMMENT_REQUIRED = 1207, ///< Transition requires a comment

    // Review errors (1300-1349)
    ERR_REVIEW_NOT_FOUND = 1300,        ///< Review does not exist
    ERR_REVIEW_ALREADY_EXISTS = 1301,   ///< Review already exists for this revision
    ERR_REVIEW_CLOSED = 1302,           ///< Cannot modify a closed/merged review
    ERR_REVIEW_NOT_APPROVED = 1303,     ///< Cannot merge without required approvals
    ERR_REVIEW_CONFLICT = 1304,         ///< Merge conflict prevents completion
    ERR_REVIEWER_NOT_FOUND = 1305,      ///< Specified reviewer user does not exist
    ERR_REVIEW_PERMISSION = 1306,       ///< User lacks permission for this review action
    ERR_REVIEW_STALE = 1307,            ///< Review is stale (new commits since last review)

    // Shared forge errors (1350-1399)
    ERR_FORGE_API_FAILED = 1350,        ///< Forge API call failed
    ERR_FORGE_AUTH_FAILED = 1351,       ///< Forge API authentication failed
    ERR_FORGE_RATE_LIMITED = 1352,      ///< Forge API rate limit exceeded

    // Part management errors (1400-1499)
    ERR_PART_NOT_FOUND = 1400,          ///< Part does not exist in PLM
    ERR_PART_DUPLICATE = 1401,          ///< Part number already exists
    ERR_PART_INVALID_STATE = 1402,      ///< Invalid state transition for part
    ERR_PART_IN_USE = 1403,             ///< Part cannot be deleted, referenced by BOMs
    ERR_PART_VALIDATION_FAILED = 1404,  ///< Part data failed validation rules
    ERR_VENDOR_NOT_FOUND = 1450,        ///< Vendor source record not found
    ERR_VENDOR_DUPLICATE = 1451,        ///< Duplicate vendor source for this part
    ERR_VENDOR_INVALID = 1452,          ///< Vendor data failed validation
    ERR_WHERE_USED_DEPTH_EXCEEDED = 1475, ///< Multi-level traversal hit max depth
    ERR_WHERE_USED_CYCLE_DETECTED = 1476, ///< Circular reference in BOM hierarchy

    // Release errors (1500-1599)
    ERR_RELEASE_VALIDATION_FAILED = 1500, ///< Pre-release validation checks failed
    ERR_RELEASE_TAG_EXISTS = 1501,      ///< Release tag/version already exists
    ERR_RELEASE_ARTIFACT_FAILED = 1502, ///< Artifact generation or upload failed
    ERR_RELEASE_NOT_FOUND = 1503,       ///< Specified release does not exist
    ERR_RELEASE_IMMUTABLE = 1504,       ///< Cannot modify a finalized release
    ERR_VARIANT_NOT_FOUND = 1510,       ///< Variant definition not found
    ERR_VARIANT_CONFLICT = 1511,        ///< Variant component conflict
    ERR_VARIANT_BOM_INVALID = 1512,     ///< Variant BOM validation failed
    ERR_BRANCH_EXISTS = 1520,           ///< Branch name already in use
    ERR_BRANCH_NOT_FOUND = 1521,        ///< Branch does not exist
    ERR_BRANCH_PROTECTED = 1522,        ///< Cannot modify a protected branch

    // CI/CD and audit errors (1600-1699)
    ERR_CI_NOT_CONFIGURED = 1600,       ///< No CI/CD pipeline configured
    ERR_CI_PIPELINE_NOT_FOUND = 1601,   ///< Requested pipeline run does not exist
    ERR_CI_ARTIFACT_NOT_FOUND = 1602,   ///< Requested artifact does not exist
    ERR_CI_ARTIFACT_EXPIRED = 1603,     ///< Artifact has been garbage-collected
    ERR_CI_PROVIDER_ERROR = 1604,       ///< CI platform returned an API error
    ERR_CI_RATE_LIMITED = 1605,         ///< CI platform rate limit exceeded
    ERR_AUDIT_NOT_AVAILABLE = 1650,     ///< Audit log not available on this backend
    ERR_AUDIT_QUERY_FAILED = 1651,      ///< Audit log query failed
    ERR_AUDIT_EXPORT_FAILED = 1652,     ///< Audit log export failed

    // File operations errors (1700-1799)
    ERR_FOLDER_EXISTS = 1700,           ///< Folder already exists
    ERR_FOLDER_CREATE_FAILED = 1701,    ///< Failed to create folder
    ERR_NO_CHANGES = 1702,              ///< Checkin attempted with no local modifications
    ERR_CANCEL_FAILED = 1703,           ///< Cancel checkout failed (partial cleanup)
    ERR_CANCEL_UNLOCK_FAILED = 1704,    ///< Cancel checkout lock release failed
    ERR_CLONE_FAILED = 1705,            ///< Project cloning failed
    ERR_CLONE_SOURCE_NOT_FOUND = 1706,  ///< Clone source project not found
    ERR_PROGRESS_CANCELLED = 1707,      ///< User cancelled via progress callback
    ERR_RELOAD_FAILED = 1708,           ///< KiCad project reload signaling failed

    // Plugin-specific error codes start here
    PLM_ERR_PLUGIN_SPECIFIC = 10000 ///< Base value for plugin-specific error codes
};


/**
 * Phases of a long-running operation for progress reporting.
 */
enum class PLM_PROGRESS_PHASE
{
    PREPARING,
    DOWNLOADING,
    UPLOADING,
    PROCESSING,
    FINALIZING
};


/**
 * Progress state for long-running operations.
 */
struct PLM_PROGRESS_INFO
{
    PLM_PROGRESS_PHASE phase = PLM_PROGRESS_PHASE::PREPARING;
    int      current = 0;
    int      total = 0;
    wxString current_item;
    int      percent = -1;

    PLM_PROGRESS_INFO() = default;

    PLM_PROGRESS_INFO( PLM_PROGRESS_PHASE aPhase, int aCurrent, int aTotal,
                       const wxString& aItem = wxEmptyString ) :
            phase( aPhase ),
            current( aCurrent ),
            total( aTotal ),
            current_item( aItem )
    {
        if( total > 0 )
            percent = ( current * 100 ) / total;
    }
};


/**
 * Callback for progress reporting during long-running operations.
 * Return false to cancel the operation.
 */
using PLM_PROGRESS_CALLBACK = std::function<bool( const PLM_PROGRESS_INFO& aInfo )>;


#endif // PLM_TYPES_H
