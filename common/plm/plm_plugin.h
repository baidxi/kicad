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
 * @file plm_plugin.h
 * @brief Abstract base class for PLM plugins.
 * @ingroup plm_plugin_interface
 *
 * This header defines the PLM_PLUGIN abstract base class that all PLM
 * backend plugins must implement. It provides the plugin lifecycle,
 * capability reporting, and core interface that the PLM manager uses
 * to interact with different PLM systems.
 */

#ifndef PLM_PLUGIN_H
#define PLM_PLUGIN_H

#include <kicommon.h>

#include "plm_audit.h"
#include "plm_auth.h"
#include "plm_bom.h"
#include "plm_capabilities.h"
#include "plm_ci.h"
#include "plm_eco.h"
#include "plm_file.h"
#include "plm_forge.h"
#include "plm_lib_provider.h"
#include "plm_lib_reference.h"
#include "plm_lib_version_tracker.h"
#include "plm_lifecycle.h"
#include "plm_lock.h"
#include "plm_navigation.h"
#include "plm_notifications.h"
#include "plm_parts.h"
#include "plm_permissions.h"
#include "plm_release.h"
#include "plm_review.h"
#include "plm_revision.h"
#include "plm_routing.h"
#include "plm_schema.h"
#include "plm_variant.h"

#include <chrono>
#include <map>
#include <memory>
#include <mutex>

#include <wx/filename.h>


class PLM_PREVIEW_CACHE;
class PLM_ACTIVITY_TRACKER;
class PLM_STATE_MAPPER;
class PLM_CACHE_MANAGER;
class PLM_SYNC_STATE_MANAGER;
class PLM_STATE_PERSISTENCE;
class PLM_LIB_SYNC;


/**
 * Callback to signal that KiCad should reload the current project.
 *
 * @param aProjectPath Path to the project that changed.
 * @param aForceReload True to force full reload, false for incremental.
 */
using PLM_RELOAD_CALLBACK = std::function<void( const wxString& aProjectPath, bool aForceReload )>;


/**
 * PLM Plugin API version.
 *
 * Increment major version for breaking changes.
 * Increment minor version for backwards-compatible additions.
 *
 * Major bumped to 3 in WF-LIBS-002 Slice 1: PLM_PLUGIN gained three
 * more protected std::unique_ptr manager members (m_libProvider,
 * m_libSync, m_libVersionTracker) for shared-library support, and
 * PLM_LIB_DOWNLOAD_CALLBACK gained a PLM_LIB_TYPE parameter plus a
 * result struct that carries the resolved revision back to the
 * provider. Plugins built against major=2 are layout-incompatible.
 */
constexpr int PLM_API_VERSION_MAJOR = 3;
constexpr int PLM_API_VERSION_MINOR = 0;

/**
 * Combined API version for quick compatibility checks.
 * Format: (major * 1000) + minor
 */
constexpr int PLM_API_VERSION = ( PLM_API_VERSION_MAJOR * 1000 ) + PLM_API_VERSION_MINOR;


/**
 * Abstract base class for PLM plugins.
 *
 * All PLM backend implementations must inherit from this class and
 * implement the pure virtual methods. Plugins are loaded as shared
 * libraries and instantiated via the PLM_PLUGIN_CREATE macro.
 *
 * Plugin Lifecycle:
 * 1. Library loaded by PLM manager
 * 2. CreatePlugin() called to instantiate
 * 3. OnLoad() called for initialization
 * 4. Plugin used for operations
 * 5. OnUnload() called for cleanup
 * 6. Plugin destroyed
 * 7. Library unloaded
 */
class KICOMMON_API PLM_PLUGIN
{
public:
    /**
     * Virtual destructor for proper cleanup of derived classes.
     *
     * Defined out-of-line in plm_plugin.cpp because the manager unique_ptrs
     * forward-declared below need complete types at the destruction site.
     */
    virtual ~PLM_PLUGIN();

    /**
     * Get information about this plugin.
     *
     * Returns metadata including plugin ID, name, version, and vendor
     * information. This is used for display in the UI and for
     * compatibility checking.
     *
     * @return Plugin information structure.
     */
    virtual PLM_PLUGIN_INFO GetPluginInfo() const = 0;

    /**
     * Get the capabilities supported by this plugin.
     *
     * Returns a structure describing which features this PLM backend
     * supports. The UI uses this to enable/disable features.
     *
     * @return Capabilities structure.
     */
    virtual PLM_CAPABILITIES GetCapabilities() const = 0;

    /**
     * Called after the plugin is loaded.
     *
     * Use this for initialization that requires the plugin to be
     * fully constructed. Configuration loading, resource allocation,
     * and other startup tasks should happen here.
     *
     * @return True if initialization succeeded.
     */
    virtual bool OnLoad() { return true; }

    /**
     * Called before the plugin is unloaded.
     *
     * Use this for cleanup tasks like saving state, closing connections,
     * and releasing resources. Called before the destructor.
     */
    virtual void OnUnload() {}

    /**
     * Get the API version this plugin was built against.
     *
     * Used by the plugin manager to verify compatibility.
     * Default implementation returns current API version.
     *
     * @return API version number.
     */
    virtual int GetAPIVersion() const { return PLM_API_VERSION; }

    /**
     * Check if this plugin is compatible with the current API.
     *
     * @return True if the plugin can be used with the current KiCad version.
     */
    bool IsCompatible() const
    {
        int pluginMajor = GetAPIVersion() / 1000;
        return pluginMajor == PLM_API_VERSION_MAJOR;
    }

    //-------------------------------------------------------------------------
    // Authentication Interface
    //-------------------------------------------------------------------------

    /**
     * Authenticate with the PLM server.
     *
     * Establishes a session with the PLM backend using the provided
     * credentials. On success, the plugin stores the session internally
     * and subsequent operations use it automatically.
     *
     * @param aCredentials Server URL, username, and password.
     * @return Result containing session token on success, error on failure.
     */
    virtual PLM_AUTH_RESULT Authenticate( const PLM_CREDENTIALS& aCredentials ) = 0;

    /**
     * End the current session.
     *
     * Logs out from the PLM server and clears the stored session.
     * After logout, Authenticate() must be called again before
     * performing any operations.
     *
     * @return True if logout succeeded or no session was active.
     */
    virtual bool Logout() = 0;

    /**
     * Check if there is an active authenticated session.
     *
     * @return True if authenticated and session is valid.
     */
    virtual bool IsAuthenticated() const = 0;

    /**
     * Get the current session information.
     *
     * Returns information about the active session including user
     * details and token expiry time.
     *
     * @return Current session, or empty session if not authenticated.
     */
    virtual PLM_SESSION GetCurrentSession() const = 0;

    /**
     * Refresh the current session token.
     *
     * Attempts to extend the session before it expires. Should be called
     * proactively before the token expires to avoid interrupting work.
     *
     * @return Result containing new session on success.
     */
    virtual PLM_AUTH_RESULT RefreshSession() = 0;

    /**
     * Get information about the currently logged in user.
     *
     * @return User info, or empty if not authenticated.
     */
    virtual PLM_USER_INFO GetCurrentUser() const
    {
        PLM_SESSION session = GetCurrentSession();
        return session.user_info;
    }

    //-------------------------------------------------------------------------
    // Navigation Interface
    //-------------------------------------------------------------------------

    /**
     * Get the root folders in the PLM repository.
     *
     * Returns the top-level folders that the user has access to.
     * These typically represent projects, libraries, or departments.
     *
     * @return List of root folders.
     */
    virtual std::vector<PLM_FOLDER> GetRootFolders() = 0;

    /**
     * Get child folders of a parent folder.
     *
     * Returns immediate child folders within the specified parent.
     *
     * @param aFolderId ID of the parent folder.
     * @return List of child folders.
     */
    virtual std::vector<PLM_FOLDER> GetChildFolders( const wxString& aFolderId ) = 0;

    /**
     * Get the full path from root to a folder.
     *
     * Returns the breadcrumb path for display in the UI navigation.
     * The path includes the folder itself as the last element.
     *
     * @param aFolderId ID of the folder.
     * @return Path from root to folder, or empty if folder not found.
     */
    virtual PLM_FOLDER_PATH GetFolderPath( const wxString& aFolderId ) = 0;

    /**
     * List files in a folder with pagination.
     *
     * Returns files/projects within a folder with optional filtering
     * and pagination for large result sets.
     *
     * @param aFolderId ID of the folder to list.
     * @param aOptions Pagination and sorting options.
     * @return Paginated list of file information.
     */
    virtual PLM_LIST_RESULT<PLM_FILE_INFO> ListFiles( const wxString&          aFolderId,
                                                      const PLM_LIST_OPTIONS&  aOptions ) = 0;

    /**
     * Search for files across the repository.
     *
     * Performs a server-side search if the plugin supports it.
     * Requires supports_search capability to be enabled.
     *
     * @param aQuery Search query string.
     * @param aOptions Pagination options.
     * @return Search results with pagination.
     */
    virtual PLM_LIST_RESULT<PLM_FILE_INFO> SearchFiles( const wxString&         aQuery,
                                                        const PLM_LIST_OPTIONS& aOptions )
    {
        // Default implementation returns empty results
        // Plugins should override if they support search
        return PLM_LIST_RESULT<PLM_FILE_INFO>();
    }

    //-------------------------------------------------------------------------
    // Search Interface (Optional)
    //-------------------------------------------------------------------------

    /**
     * Get available search filter fields.
     *
     * Returns the list of fields that can be used to filter search results.
     * Only meaningful if supports_search capability is enabled. The UI uses
     * this to build the search filter interface.
     *
     * Each field has an ID (used in queries), display name (shown in UI),
     * and field type indicating what kind of filter control to show.
     *
     * @return Vector of search field definitions.
     */
    virtual std::vector<PLM_SEARCH_FIELD> GetSearchFields()
    {
        // Default implementation returns no fields
        // Plugins should override to provide searchable fields
        return std::vector<PLM_SEARCH_FIELD>();
    }

    /**
     * Perform an advanced search with field filters.
     *
     * Searches using structured field filters instead of a simple query string.
     * Requires supports_search capability to be enabled.
     *
     * @param aFilters Map of field ID to filter value.
     * @param aOptions Pagination options.
     * @return Search results with pagination.
     */
    virtual PLM_LIST_RESULT<PLM_FILE_INFO> SearchWithFilters(
            const std::map<wxString, wxString>& aFilters,
            const PLM_LIST_OPTIONS&             aOptions )
    {
        // Default implementation returns empty results
        return PLM_LIST_RESULT<PLM_FILE_INFO>();
    }

    /**
     * Check if this plugin supports server-side search.
     *
     * Convenience method checking the search capability.
     *
     * @return True if search is supported.
     */
    virtual bool SupportsSearch() const
    {
        return GetCapabilities().supports_search;
    }

    //-------------------------------------------------------------------------
    // File Operations Interface
    //-------------------------------------------------------------------------

    /**
     * Check out a project for editing.
     *
     * Downloads the project files to a local directory and marks the
     * project as checked out in the PLM system. The local copy can then
     * be edited and later checked back in.
     *
     * @param aProjectId ID of the project to check out.
     * @param aLocalPath Local directory to download files to.
     * @param aOptions Checkout options (lock behavior, etc.).
     * @return Result with local path on success.
     */
    virtual PLM_CHECKOUT_RESULT Checkout( const wxString&           aProjectId,
                                          const wxString&           aLocalPath,
                                          const PLM_CHECKOUT_OPTIONS& aOptions ) = 0;

    /**
     * Check in a modified project.
     *
     * Uploads changes from the local directory back to the PLM system
     * and creates a new revision. The checkout is completed and any
     * locks are released.
     *
     * @param aLocalPath Local directory containing the project.
     * @param aOptions Checkin options (comment, attributes, etc.).
     * @return Result with new revision info on success.
     */
    virtual PLM_CHECKIN_RESULT Checkin( const wxString&          aLocalPath,
                                        const PLM_CHECKIN_OPTIONS& aOptions ) = 0;

    /**
     * Get information about a project.
     *
     * Returns metadata about the project including current state,
     * lock status, and available actions.
     *
     * @param aProjectId ID of the project.
     * @return File information, or empty info if not found.
     */
    virtual PLM_FILE_INFO GetFileInfo( const wxString& aProjectId ) = 0;

    /**
     * Create a new project in a folder.
     *
     * Creates a new empty project or one based on a template.
     * The project is created in draft/editable state.
     *
     * @param aFolderId ID of the parent folder.
     * @param aName Name for the new project.
     * @param aMetadata Custom metadata attributes for the project.
     * @param aTemplateId Optional template to use (empty for blank).
     * @param aCloneOptions Options for cloning from an existing project.
     * @return Result with new project ID on success.
     */
    virtual PLM_CHECKOUT_RESULT CreateProject( const wxString&          aFolderId,
                                               const wxString&          aName,
                                               const PLM_ATTRIBUTE_SET& aMetadata,
                                               const wxString&          aTemplateId = wxEmptyString,
                                               const PLM_CLONE_OPTIONS& aCloneOptions =
                                                       PLM_CLONE_OPTIONS() ) = 0;

    /**
     * @deprecated Use the 5-parameter overload instead.
     * Forwarding overload for backwards compatibility.
     */
    PLM_CHECKOUT_RESULT CreateProject( const wxString& aFolderId,
                                       const wxString& aName,
                                       const wxString& aTemplateId )
    {
        return CreateProject( aFolderId, aName, PLM_ATTRIBUTE_SET(), aTemplateId );
    }

    /**
     * Delete a project from the PLM system.
     *
     * Permanently removes the project and all its revisions.
     * Requires appropriate permissions and the project must not be
     * locked by another user.
     *
     * @param aProjectId ID of the project to delete.
     * @return True if deletion succeeded.
     */
    virtual bool DeleteProject( const wxString& aProjectId ) = 0;

    /**
     * Cancel a checkout without saving changes.
     *
     * Discards local changes and releases any locks without
     * creating a new revision.
     *
     * @param aLocalPath Local directory containing the checked-out project.
     * @return Structured result with details about file restoration and lock release.
     */
    virtual PLM_CANCEL_CHECKOUT_RESULT CancelCheckout( const wxString& aLocalPath ) = 0;

    /**
     * Download a read-only copy of a project.
     *
     * Downloads project files without checking out or locking.
     * The files cannot be checked back in.
     *
     * @param aProjectId ID of the project to download.
     * @param aLocalPath Local directory to download to.
     * @return True if download succeeded.
     */
    virtual bool DownloadReadOnly( const wxString& aProjectId, const wxString& aLocalPath )
    {
        PLM_CHECKOUT_OPTIONS opts;
        opts.acquire_lock = false;
        PLM_CHECKOUT_RESULT result = Checkout( aProjectId, aLocalPath, opts );
        return result.success;
    }

    //-------------------------------------------------------------------------
    // Batch Operations Interface
    //-------------------------------------------------------------------------

    /**
     * Check out multiple projects in a single operation.
     *
     * Processes each project sequentially, reporting progress via the callback.
     * The operation can be cancelled by returning false from the callback.
     * Failures on individual projects don't stop the batch, but are recorded.
     *
     * @param aProjectIds List of project IDs to check out.
     * @param aBaseLocalPath Base directory for checkouts (each project in subdirectory).
     * @param aOptions Checkout options applied to all projects.
     * @param aProgressCb Optional callback for progress reporting and cancellation.
     * @return Batch result with individual outcomes for each project.
     */
    virtual PLM_BATCH_RESULT BatchCheckout(
            const std::vector<wxString>&   aProjectIds,
            const wxString&                aBaseLocalPath,
            const PLM_CHECKOUT_OPTIONS&    aOptions,
            PLM_BATCH_PROGRESS_CALLBACK    aProgressCb = nullptr )
    {
        PLM_BATCH_RESULT batchResult;

        for( size_t i = 0; i < aProjectIds.size(); ++i )
        {
            const wxString& projectId = aProjectIds[i];
            wxString localPath = aBaseLocalPath + wxFileName::GetPathSeparator() + projectId;

            PLM_CHECKOUT_RESULT result = Checkout( projectId, localPath, aOptions );

            PLM_BATCH_ITEM_RESULT itemResult( projectId, result.success,
                                              result.error_code, result.error_message );
            itemResult.local_path = result.local_path;
            batchResult.AddResult( itemResult );

            if( aProgressCb && !aProgressCb( i, aProjectIds.size(), projectId, result.success ) )
            {
                batchResult.cancelled = true;
                break;
            }
        }

        return batchResult;
    }

    /**
     * Check in multiple projects in a single operation.
     *
     * Processes each local path sequentially, reporting progress via the callback.
     * The operation can be cancelled by returning false from the callback.
     * Failures on individual projects don't stop the batch, but are recorded.
     *
     * @param aLocalPaths List of local paths to check in.
     * @param aOptions Checkin options applied to all projects.
     * @param aProgressCb Optional callback for progress reporting and cancellation.
     * @return Batch result with individual outcomes for each project.
     */
    virtual PLM_BATCH_RESULT BatchCheckin(
            const std::vector<wxString>&   aLocalPaths,
            const PLM_CHECKIN_OPTIONS&     aOptions,
            PLM_BATCH_PROGRESS_CALLBACK    aProgressCb = nullptr )
    {
        PLM_BATCH_RESULT batchResult;

        for( size_t i = 0; i < aLocalPaths.size(); ++i )
        {
            const wxString& localPath = aLocalPaths[i];

            PLM_CHECKIN_RESULT result = Checkin( localPath, aOptions );

            // Use local path as project_id since we don't have the actual project ID
            wxString projectId = wxFileName( localPath ).GetName();

            PLM_BATCH_ITEM_RESULT itemResult( projectId, result.success,
                                              result.error_code, result.error_message );
            itemResult.new_revision = result.new_revision;
            batchResult.AddResult( itemResult );

            if( aProgressCb && !aProgressCb( i, aLocalPaths.size(), projectId, result.success ) )
            {
                batchResult.cancelled = true;
                break;
            }
        }

        return batchResult;
    }

    //-------------------------------------------------------------------------
    // Revision Interface
    //-------------------------------------------------------------------------

    /**
     * Get the revision history for a project.
     *
     * Returns all revisions of the project in chronological order,
     * with the most recent revision first.
     *
     * @param aProjectId ID of the project.
     * @return Revision history with all revisions.
     */
    virtual PLM_REVISION_HISTORY GetRevisionHistory( const wxString& aProjectId ) = 0;

    /**
     * Get details of a specific revision.
     *
     * Returns full metadata for a single revision including
     * author, timestamp, comment, and state.
     *
     * @param aProjectId ID of the project.
     * @param aRevisionId ID of the revision.
     * @return Revision details, or empty revision if not found.
     */
    virtual PLM_REVISION GetRevision( const wxString& aProjectId,
                                      const wxString& aRevisionId ) = 0;

    /**
     * Download a specific revision to a local directory.
     *
     * Downloads a historical version of the project for viewing
     * or comparison. The downloaded files are read-only.
     *
     * @param aProjectId ID of the project.
     * @param aRevisionId ID of the revision to download.
     * @param aLocalPath Local directory to download to.
     * @return True if download succeeded.
     */
    virtual bool DownloadRevision( const wxString& aProjectId,
                                   const wxString& aRevisionId,
                                   const wxString& aLocalPath ) = 0;

    /**
     * Compare two revisions of a project.
     *
     * Returns a structured diff showing what changed between
     * two revisions. Requires supports_diff capability.
     *
     * @param aProjectId ID of the project.
     * @param aRevision1 ID of the first (older) revision.
     * @param aRevision2 ID of the second (newer) revision.
     * @return Comparison result with list of changes.
     */
    virtual PLM_REVISION_COMPARE CompareRevisions( const wxString& aProjectId,
                                                   const wxString& aRevision1,
                                                   const wxString& aRevision2 ) = 0;

    /**
     * Restore a previous revision as the current version.
     *
     * Creates a new revision based on a historical version,
     * effectively reverting changes. Requires supports_revisions capability.
     *
     * @param aProjectId ID of the project.
     * @param aRevisionId ID of the revision to restore.
     * @param aComment Comment explaining the restore.
     * @return True if restore succeeded.
     */
    virtual bool RestoreRevision( const wxString& aProjectId,
                                  const wxString& aRevisionId,
                                  const wxString& aComment )
    {
        // Default implementation not supported
        // Plugins should override if they support restore
        return false;
    }

    //-------------------------------------------------------------------------
    // Lock Interface
    //-------------------------------------------------------------------------

    /**
     * Acquire a lock on a project.
     *
     * Prevents other users from modifying the project while locked.
     * The lock type (advisory or mandatory) depends on server configuration.
     *
     * @param aRequest Lock request with project ID and optional comment.
     * @return Result with lock info on success, error details on failure.
     */
    virtual PLM_LOCK_RESULT Lock( const PLM_LOCK_REQUEST& aRequest ) = 0;

    /**
     * Release a lock on a project.
     *
     * Releases a lock previously acquired by the current user.
     * If force is set, may release locks held by other users
     * (requires elevated permissions).
     *
     * @param aRequest Unlock request with project ID and force flag.
     * @return Result with updated lock info.
     */
    virtual PLM_LOCK_RESULT Unlock( const PLM_UNLOCK_REQUEST& aRequest ) = 0;

    /**
     * Get current lock information for a project.
     *
     * Returns details about who holds the lock, when it was acquired,
     * and whether the current user can modify the file.
     *
     * @param aProjectId ID of the project.
     * @return Lock information.
     */
    virtual PLM_LOCK_INFO GetLockInfo( const wxString& aProjectId ) = 0;

    /**
     * Force-acquire a lock breaking another user's lock.
     *
     * This is a privileged operation that requires appropriate
     * permissions. Use with caution as it may cause data loss
     * for the previous lock holder.
     *
     * @param aProjectId ID of the project.
     * @param aReason Reason for forcing the lock (required for audit).
     * @return Result with lock info on success.
     */
    virtual PLM_LOCK_RESULT ForceLock( const wxString& aProjectId, const wxString& aReason )
    {
        PLM_LOCK_REQUEST request( aProjectId, true, aReason );
        return Lock( request );
    }

    /**
     * Check if a project is currently locked.
     *
     * Convenience method that checks lock status without full details.
     *
     * @param aProjectId ID of the project.
     * @return True if locked by any user.
     */
    virtual bool IsLocked( const wxString& aProjectId )
    {
        PLM_LOCK_INFO info = GetLockInfo( aProjectId );
        return info.is_locked;
    }

    /**
     * Check if the current user holds the lock.
     *
     * @param aProjectId ID of the project.
     * @return True if locked by the current user.
     */
    virtual bool IsLockedByMe( const wxString& aProjectId )
    {
        PLM_LOCK_INFO info = GetLockInfo( aProjectId );
        return info.is_locked && info.locked_by_me;
    }

    //-------------------------------------------------------------------------
    // Schema Interface
    //-------------------------------------------------------------------------

    /**
     * Get the attribute schema for this PLM system.
     *
     * Returns the schema defining available attributes, their types,
     * and validation rules. The schema should be cached after first fetch.
     *
     * @return Schema with attribute definitions.
     */
    virtual PLM_SCHEMA GetSchema() = 0;

    /**
     * Get attribute values for a project.
     *
     * Returns the current attribute values stored with a project.
     * Only returns attributes that have been set.
     *
     * @param aProjectId ID of the project.
     * @return Set of attribute values.
     */
    virtual PLM_ATTRIBUTE_SET GetAttributes( const wxString& aProjectId ) = 0;

    /**
     * Set attribute values for a project.
     *
     * Updates attribute values on a project. Values are validated
     * against the schema before being committed.
     *
     * @param aProjectId ID of the project.
     * @param aAttributes Attribute values to set.
     * @return True if all attributes were set successfully.
     */
    virtual bool SetAttributes( const wxString& aProjectId,
                                const PLM_ATTRIBUTE_SET& aAttributes ) = 0;

    /**
     * Validate attribute values against the schema.
     *
     * Checks that all values conform to their type definitions,
     * required fields are present, and enum values are valid.
     *
     * @param aAttributes Attribute values to validate.
     * @return Vector of error messages (empty if valid).
     */
    virtual std::vector<wxString> ValidateAttributes( const PLM_ATTRIBUTE_SET& aAttributes )
    {
        std::vector<wxString> errors;
        PLM_SCHEMA schema = GetSchema();

        for( const auto& attr : schema.GetRequiredAttributes() )
        {
            const PLM_ATTRIBUTE_VALUE* val = aAttributes.FindValue( attr->attr_id );

            if( !val || val->value.IsEmpty() )
            {
                errors.push_back( wxString::Format( wxS( "Required attribute '%s' is missing" ),
                                                    attr->display_name ) );
            }
        }

        for( const auto& val : aAttributes.values )
        {
            const PLM_ATTRIBUTE_SCHEMA* attrSchema = schema.FindAttribute( val.attr_id );

            if( attrSchema && !attrSchema->IsValidValue( val.value ) )
            {
                errors.push_back( wxString::Format( wxS( "Invalid value for attribute '%s'" ),
                                                    attrSchema->display_name ) );
            }
        }

        return errors;
    }

    /**
     * Get a localized string for a key.
     *
     * Returns a translated string for UI display. If no translation
     * exists for the specified locale, returns the original key.
     *
     * @param aKey String key to localize.
     * @param aLocale Locale code (e.g., "en_US", "de_DE").
     * @return Localized string, or key if no translation.
     */
    virtual wxString GetLocalizedString( const wxString& aKey, const wxString& aLocale )
    {
        // Default implementation returns the key unchanged
        // Plugins should override to provide translations
        return aKey;
    }

    //-------------------------------------------------------------------------
    // Permissions Interface
    //-------------------------------------------------------------------------

    /**
     * Get permissions for a project, using a cache with configurable TTL.
     *
     * Returns cached permissions if they are still valid. Otherwise calls
     * QueryPermissions() to fetch fresh permissions from the backend.
     *
     * @param aProjectId ID of the project to check permissions for.
     * @return Permissions structure indicating allowed operations.
     */
    virtual PLM_PERMISSIONS GetPermissions( const wxString& aProjectId )
    {
        std::lock_guard<std::mutex> guard( m_permCacheMutex );

        auto it = m_permCache.find( aProjectId );
        auto now = std::chrono::steady_clock::now();

        if( it != m_permCache.end() &&
            ( now - it->second.timestamp ) < m_permCacheTTL )
        {
            return it->second.permissions;
        }

        PLM_PERMISSIONS perms = QueryPermissions( aProjectId );

        m_permCache[aProjectId] = { perms, now };
        return perms;
    }

    /**
     * Force a refresh of cached permissions for a project.
     *
     * Invalidates the cache entry and re-queries the backend.
     *
     * @param aProjectId ID of the project. If empty, all cached entries are cleared.
     * @return Fresh permissions for the specified project, or default if aProjectId is empty.
     */
    PLM_PERMISSIONS RefreshPermissions( const wxString& aProjectId = wxEmptyString )
    {
        std::lock_guard<std::mutex> guard( m_permCacheMutex );

        if( aProjectId.IsEmpty() )
        {
            m_permCache.clear();
            return PLM_PERMISSIONS();
        }

        m_permCache.erase( aProjectId );

        PLM_PERMISSIONS perms = QueryPermissions( aProjectId );
        m_permCache[aProjectId] = { perms, std::chrono::steady_clock::now() };
        return perms;
    }

    /**
     * Set the TTL for the permission cache.
     *
     * @param aSeconds Cache lifetime in seconds. Default is 300 (5 minutes).
     */
    void SetPermissionCacheTTL( int aSeconds )
    {
        m_permCacheTTL = std::chrono::seconds( aSeconds );
    }

    /**
     * Get the display name for a user.
     *
     * Resolves a user ID to a human-readable display name for showing
     * in the UI (e.g., lock owner, revision author). Results should be
     * cached for the session duration.
     *
     * @param aUserId User identifier to look up.
     * @return Display name (e.g., "John Smith"), or user ID if not found.
     */
    virtual wxString GetUserDisplayName( const wxString& aUserId ) = 0;

    /**
     * Get the organizational policy.
     *
     * Returns the policy rules that apply to the current organization.
     * Policies define constraints like requiring ECO for changes or
     * mandatory lock-before-edit.
     *
     * @return Policy structure with organizational rules.
     */
    virtual PLM_POLICY GetPolicy() const
    {
        // Default implementation returns permissive policy
        return PLM_POLICY::Permissive();
    }

    /**
     * Get the combined permission context for a project.
     *
     * Returns both user permissions and organizational policy combined
     * into a single context for convenience when checking multiple
     * constraints.
     *
     * @param aProjectId ID of the project.
     * @return Permission context combining permissions and policy.
     */
    virtual PLM_PERMISSION_CONTEXT GetPermissionContext( const wxString& aProjectId )
    {
        return PLM_PERMISSION_CONTEXT( GetPermissions( aProjectId ), GetPolicy() );
    }

    //-------------------------------------------------------------------------
    // Part Database Interface (Optional)
    //-------------------------------------------------------------------------

    /**
     * Search the part database.
     *
     * Searches for parts/components in the PLM part database using the
     * specified search options. Requires supports_part_database capability.
     *
     * @param aOptions Search options including query, category, manufacturer.
     * @return Paginated list of matching parts.
     */
    virtual PLM_LIST_RESULT<PLM_PART> SearchParts( const PLM_PART_SEARCH_OPTIONS& aOptions )
    {
        // Default implementation returns empty results
        return PLM_LIST_RESULT<PLM_PART>();
    }

    /**
     * Get detailed information about a specific part.
     *
     * Retrieves full part information including attributes, datasheet URL,
     * symbol/footprint references, and lifecycle state.
     *
     * @param aPartId ID of the part to retrieve.
     * @return Part information, or empty part if not found.
     */
    virtual PLM_PART GetPart( const wxString& aPartId )
    {
        // Default implementation returns empty part
        return PLM_PART();
    }

    /**
     * Download a part's symbol and footprint library.
     *
     * Downloads the KiCad-compatible library files for a part to the
     * specified local directory. Creates symbol (.kicad_sym) and
     * footprint (.kicad_mod) files.
     *
     * @param aPartId ID of the part to download.
     * @param aLocalPath Local directory to download files to.
     * @return True if download succeeded.
     */
    virtual bool DownloadPartLibrary( const wxString& aPartId, const wxString& aLocalPath )
    {
        // Default implementation does nothing
        return false;
    }

    /**
     * List shared libraries the user has subscribed to via this plugin.
     *
     * @param aProjectId Optional workspace/project scope; empty for global.
     *                   Plugins that don't scope per-project (e.g. Git, where
     *                   subscriptions are plugin-global) ignore the parameter.
     * @return Library references with id, type, and metadata. Empty if the
     *         plugin doesn't support shared libraries or none are configured.
     */
    virtual std::vector<PLM_LIB_REFERENCE>
    GetAvailableLibraries( const wxString& aProjectId = wxEmptyString )
    {
        return {};
    }

    /**
     * Enumerate available revisions/versions of a shared library.
     *
     * @param aLibId Library identifier (matches PLM_LIB_REFERENCE::lib_id).
     * @return Version records ordered however the backend yields them. Empty
     *         when the plugin doesn't support shared libraries or the lib id
     *         is unknown.
     */
    virtual std::vector<PLM_LIB_VERSION_INFO> GetLibraryVersions( const wxString& aLibId )
    {
        return {};
    }

    /**
     * Resolve the latest revision for a shared library.
     *
     * Bound to PLM_LIB_VERSION_TRACKER::SetServerVersionCallback so version
     * pinning detection has a server-side source.
     *
     * @param aLibId Library identifier.
     * @return Revision string (commit SHA, tag, or backend-defined token).
     *         Empty if the plugin doesn't support shared libraries.
     */
    virtual wxString GetLatestLibraryRevision( const wxString& aLibId )
    {
        return wxEmptyString;
    }

    /**
     * Download a shared library at the requested revision into aLocalPath.
     *
     * Bound to PLM_LIB_PROVIDER::SetDownloadCallback. The provider passes the
     * revision to fetch (empty for "latest") and the destination path. The
     * result struct carries back the resolved revision so the version tracker
     * can pin against the actual SHA, not whatever the caller requested.
     *
     * @param aLibId Library identifier.
     * @param aLibType Library type (symbol, footprint, 3D model). Plugins use
     *                 this to extract the right artifact from a fetched repo
     *                 or attachment set.
     * @param aRequestedRevision Revision to fetch, or empty for latest.
     * @param aLocalPath Path where the artifact must land.
     * @return Result with success state, revision actually written, and an
     *         error message on failure.
     */
    virtual PLM_LIB_DOWNLOAD_RESULT DownloadLibrary( const wxString& aLibId,
                                                      PLM_LIB_TYPE aLibType,
                                                      const wxString& aRequestedRevision,
                                                      const wxString& aLocalPath )
    {
        PLM_LIB_DOWNLOAD_RESULT result;
        result.error_message = wxS( "Plugin does not support shared libraries" );
        return result;
    }

    /**
     * Validate a BOM against the part database.
     *
     * Checks that all parts in the BOM exist in the database, are not
     * obsolete, and suggests replacements for problematic parts.
     *
     * @param aPartNumbers List of part numbers to validate.
     * @return Validation result with issues and suggestions.
     */
    virtual PLM_BOM_VALIDATION_RESULT ValidateBOM( const std::vector<wxString>& aPartNumbers )
    {
        // Default implementation returns success with no validation
        return PLM_BOM_VALIDATION_RESULT::Success( aPartNumbers.size() );
    }

    /**
     * Get suggested replacement for an obsolete part.
     *
     * Finds replacement parts for an obsolete or discontinued part.
     * Returns parts that are form-fit-function compatible when possible.
     *
     * @param aPartId ID of the obsolete part.
     * @return Replacement suggestions, or empty vector if none found.
     */
    virtual std::vector<PLM_PART_REPLACEMENT> GetReplacements( const wxString& aPartId )
    {
        // Default implementation returns no replacements
        return std::vector<PLM_PART_REPLACEMENT>();
    }

    /**
     * Check if this plugin supports part database operations.
     *
     * Convenience method checking the part_database capability.
     *
     * @return True if part database is supported.
     */
    virtual bool SupportsPartDatabase() const
    {
        return GetCapabilities().supports_part_database;
    }

    //-------------------------------------------------------------------------
    // Notification Interface (Optional)
    //-------------------------------------------------------------------------

    /**
     * Subscribe to notifications for a project.
     *
     * Registers a listener to receive real-time notifications about changes
     * to the specified project. Requires supports_push_notifications capability.
     *
     * The listener's OnNotification method will be called on a background
     * thread when events occur. Implementations should post to the main
     * thread for UI updates.
     *
     * @param aProjectId ID of the project to watch.
     * @param aListener Listener to receive notifications (caller owns).
     * @param aConfig Notification configuration options.
     * @return True if subscription succeeded.
     */
    virtual bool SubscribeNotifications( const wxString&                 aProjectId,
                                          PLM_NOTIFICATION_LISTENER*      aListener,
                                          const PLM_NOTIFICATION_CONFIG&  aConfig =
                                                  PLM_NOTIFICATION_CONFIG() )
    {
        // Default implementation does nothing
        return false;
    }

    /**
     * Unsubscribe from notifications for a project.
     *
     * Removes the notification subscription for the specified project.
     * The listener will no longer receive events after this call returns.
     *
     * @param aProjectId ID of the project to stop watching.
     * @return True if unsubscription succeeded.
     */
    virtual bool UnsubscribeNotifications( const wxString& aProjectId )
    {
        // Default implementation does nothing
        return false;
    }

    /**
     * Unsubscribe from all notifications.
     *
     * Removes all notification subscriptions. Called automatically
     * during plugin unload.
     */
    virtual void UnsubscribeAllNotifications()
    {
        // Default implementation does nothing
    }

    /**
     * Check if notifications are active for a project.
     *
     * @param aProjectId ID of the project to check.
     * @return True if subscribed to notifications.
     */
    virtual bool IsSubscribedToNotifications( const wxString& aProjectId ) const
    {
        // Default implementation returns false
        return false;
    }

    /**
     * Check if this plugin supports push notifications.
     *
     * Convenience method checking the push_notifications capability.
     *
     * @return True if push notifications are supported.
     */
    virtual bool SupportsPushNotifications() const
    {
        return GetCapabilities().supports_push_notifications;
    }

    //-------------------------------------------------------------------------
    // Health Check Interface
    //-------------------------------------------------------------------------

    /**
     * Check server connectivity and session validity.
     *
     * Performs a lightweight check to verify that the server is reachable
     * and the current session (if any) is still valid. Use this for
     * connection status indicators in the UI.
     *
     * @return True if server is reachable and session is valid.
     */
    virtual bool Ping()
    {
        // Default implementation checks authentication status
        // Plugins should override to actually verify server connectivity
        return IsAuthenticated();
    }

    /**
     * Get detailed health status.
     *
     * Returns detailed information about the connection status including
     * server reachability, session validity, and any error messages.
     *
     * @return Health status with details.
     */
    virtual PLM_HEALTH_STATUS GetHealthStatus()
    {
        PLM_HEALTH_STATUS status;
        status.is_healthy = Ping();
        status.is_authenticated = IsAuthenticated();

        if( status.is_authenticated )
        {
            PLM_SESSION session = GetCurrentSession();
            int64_t now = static_cast<int64_t>( std::time( nullptr ) );
            status.session_expires_in = session.expires_at - now;
        }

        return status;
    }

    //-------------------------------------------------------------------------
    // ECO Interface (Optional)
    //-------------------------------------------------------------------------

    /**
     * Get active ECOs that can accept changes.
     *
     * Returns ECOs in approved or in-progress status that the current
     * user can associate with check-ins. Requires supports_eco capability.
     *
     * @return Vector of active ECOs.
     */
    virtual std::vector<PLM_ECO> GetActiveECOs()
    {
        // Default implementation returns no ECOs
        return std::vector<PLM_ECO>();
    }

    /**
     * Get ECOs that affect a specific project.
     *
     * Returns active ECOs that include the specified project in their
     * affected projects list.
     *
     * @param aProjectId ID of the project.
     * @return Vector of ECOs affecting this project.
     */
    virtual std::vector<PLM_ECO> GetProjectECOs( const wxString& aProjectId )
    {
        std::vector<PLM_ECO> result;
        std::vector<PLM_ECO> allEcos = GetActiveECOs();

        for( const auto& eco : allEcos )
        {
            if( eco.AffectsProject( aProjectId ) )
                result.push_back( eco );
        }

        return result;
    }

    /**
     * Get required fields for an ECO.
     *
     * Returns the fields that must be filled in when checking in
     * changes under the specified ECO.
     *
     * @param aEcoId ID of the ECO.
     * @return Vector of required field definitions.
     */
    virtual std::vector<PLM_ECO_FIELD> GetECORequiredFields( const wxString& aEcoId )
    {
        // Default implementation returns no required fields
        return std::vector<PLM_ECO_FIELD>();
    }

    /**
     * Validate ECO fields before checkin.
     *
     * Checks that all required fields are present and valid for the
     * specified ECO. Called before allowing a checkin to proceed.
     *
     * @param aEcoId ID of the ECO.
     * @param aFields Field values provided by user.
     * @return Validation result with errors if invalid.
     */
    virtual PLM_ECO_VALIDATION_RESULT ValidateECOCheckin( const wxString&            aEcoId,
                                                           const PLM_ECO_FIELD_VALUES& aFields )
    {
        // Default implementation allows all checkins
        return PLM_ECO_VALIDATION_RESULT::Success( aEcoId );
    }

    /**
     * Check if ECO is required for checkin.
     *
     * Determines whether the current policy requires an ECO to be
     * selected when checking in changes.
     *
     * @return True if ECO is mandatory.
     */
    virtual bool RequiresECO() const
    {
        return GetCapabilities().supports_eco && GetPolicy().require_eco;
    }

    /**
     * Check if this plugin supports ECO workflows.
     *
     * Convenience method checking the ECO capability.
     *
     * @return True if ECO is supported.
     */
    virtual bool SupportsECO() const
    {
        return GetCapabilities().supports_eco;
    }

    /**
     * Create a new ECO or ECR.
     *
     * @param aOptions Creation options including type, title, and affected projects.
     * @return Result with the created ECO on success.
     */
    virtual PLM_ECO_CREATE_RESULT CreateECO( const PLM_ECO_CREATE_OPTIONS& aOptions )
    {
        return PLM_ECO_CREATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                               wxS( "CreateECO not supported" ) );
    }

    /**
     * Get a single ECO by ID.
     *
     * @param aEcoId ID of the ECO to retrieve.
     * @return Result containing the ECO, or error if not found.
     */
    virtual PLM_ECO_UPDATE_RESULT GetECO( const wxString& aEcoId )
    {
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                               wxS( "GetECO not supported" ) );
    }

    /**
     * Transition an ECO to a new status.
     *
     * @param aTransition Requested state transition.
     * @return Result with updated ECO on success.
     */
    virtual PLM_ECO_UPDATE_RESULT UpdateECOStatus( const PLM_ECO_STATUS_TRANSITION& aTransition )
    {
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                               wxS( "UpdateECOStatus not supported" ) );
    }

    /**
     * Update the items tracked within an ECO.
     *
     * @param aEcoId ID of the ECO.
     * @param aItems Updated item list.
     * @return Result with updated ECO on success.
     */
    virtual PLM_ECO_UPDATE_RESULT UpdateECOItems( const wxString&                 aEcoId,
                                                   const std::vector<PLM_ECO_ITEM>& aItems )
    {
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                               wxS( "UpdateECOItems not supported" ) );
    }

    /**
     * Update the impact assessment on an ECO.
     *
     * @param aEcoId ID of the ECO.
     * @param aImpact Impact assessment data.
     * @return Result with updated ECO on success.
     */
    virtual PLM_ECO_UPDATE_RESULT UpdateECOImpact( const wxString&     aEcoId,
                                                    const PLM_ECO_IMPACT& aImpact )
    {
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                               wxS( "UpdateECOImpact not supported" ) );
    }

    /**
     * Get the status transition history for an ECO.
     *
     * @param aEcoId ID of the ECO.
     * @return Ordered list of status transitions, oldest first.
     */
    virtual std::vector<PLM_ECO_STATUS_TRANSITION> GetECOHistory( const wxString& aEcoId )
    {
        return {};
    }

    /**
     * Convert an approved ECR into a full ECO.
     *
     * @param aEcrId ID of the ECR to convert.
     * @return Result with the created ECO on success.
     */
    virtual PLM_ECO_CREATE_RESULT ConvertECRToECO( const wxString& aEcrId )
    {
        return PLM_ECO_CREATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                               wxS( "ConvertECRToECO not supported" ) );
    }

    //-------------------------------------------------------------------------
    // BOM Management Interface (Optional)
    //-------------------------------------------------------------------------

    /**
     * Get a BOM for a product by type.
     *
     * @param aProductId Product/project ID.
     * @param aType BOM type to retrieve.
     * @return The BOM, or empty BOM if not found.
     */
    virtual PLM_BOM GetBOM( const wxString& aProductId, PLM_BOM_TYPE aType )
    {
        return PLM_BOM();
    }

    /**
     * Get all BOMs for a product.
     *
     * @param aProductId Product/project ID.
     * @return All BOMs associated with the product.
     */
    virtual std::vector<PLM_BOM> GetBOMs( const wxString& aProductId )
    {
        return {};
    }

    /**
     * Create a new BOM.
     *
     * @param aBom BOM data including lines and type.
     * @return Result with the created BOM on success.
     */
    virtual PLM_BOM_RESULT CreateBOM( const PLM_BOM& aBom )
    {
        return PLM_BOM_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                        wxS( "CreateBOM not supported" ) );
    }

    /**
     * Update an existing BOM.
     *
     * @param aBom Updated BOM data.
     * @return Result with the updated BOM on success.
     */
    virtual PLM_BOM_RESULT UpdateBOM( const PLM_BOM& aBom )
    {
        return PLM_BOM_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                        wxS( "UpdateBOM not supported" ) );
    }

    /**
     * Delete a BOM.
     *
     * @param aBomId ID of the BOM to delete.
     * @return True if deletion succeeded.
     */
    virtual bool DeleteBOM( const wxString& aBomId )
    {
        return false;
    }

    /**
     * Compare two BOMs and produce a diff.
     *
     * @param aBomA First BOM (baseline).
     * @param aBomB Second BOM (comparison).
     * @param aOptions Comparison options.
     * @return Diff result with individual change entries.
     */
    virtual PLM_BOM_DIFF_RESULT CompareBOMs( const PLM_BOM&              aBomA,
                                              const PLM_BOM&              aBomB,
                                              const PLM_BOM_DIFF_OPTIONS& aOptions )
    {
        return PLM_BOM_DIFF_RESULT();
    }

    /**
     * Validate enriched BOM entries against the part database.
     *
     * Uses the richer PLM_BOM_ENTRY struct for validation with severity levels.
     * Falls back to ValidateBOM() if not overridden.
     *
     * @param aEntries Enriched BOM entries to validate.
     * @return Validation result with categorized issues.
     */
    virtual PLM_BOM_VALIDATION_RESULT ValidateBOMEntries(
            const std::vector<PLM_BOM_ENTRY>& aEntries )
    {
        std::vector<wxString> partNumbers;
        partNumbers.reserve( aEntries.size() );

        for( const auto& entry : aEntries )
            partNumbers.push_back( entry.part_number );

        return ValidateBOM( partNumbers );
    }

    /**
     * Check if this plugin supports BOM management.
     *
     * @return True if BOM create/update/delete operations are supported.
     */
    virtual bool SupportsBOMManagement() const
    {
        return GetCapabilities().supports_bom_management;
    }

    //-------------------------------------------------------------------------
    // Routing Interface (Optional)
    //-------------------------------------------------------------------------

    /**
     * Get the manufacturing routing for a product.
     *
     * @param aProductId Product/project ID.
     * @return The routing, or empty routing if not found.
     */
    virtual PLM_ROUTING GetRouting( const wxString& aProductId )
    {
        return PLM_ROUTING();
    }

    /**
     * Get all available work centers.
     *
     * @return List of work centers defined in the PLM system.
     */
    virtual std::vector<PLM_WORKCENTER> GetWorkcenters()
    {
        return {};
    }

    /**
     * Create or update a manufacturing routing.
     *
     * @param aRouting Routing data including operations.
     * @return Result with the saved routing on success.
     */
    virtual PLM_ROUTING_RESULT SaveRouting( const PLM_ROUTING& aRouting )
    {
        return PLM_ROUTING_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                            wxS( "SaveRouting not supported" ) );
    }

    /**
     * Check if this plugin supports manufacturing routing.
     *
     * @return True if routing operations are supported.
     */
    virtual bool SupportsRouting() const
    {
        return GetCapabilities().supports_routing;
    }

    //-------------------------------------------------------------------------
    // Lifecycle Transition Interface (Optional)
    //-------------------------------------------------------------------------

    /**
     * Get the state mappings for this PLM system.
     *
     * Returns all native states and their mapped categories.
     *
     * @return List of state info entries.
     */
    virtual std::vector<PLM_STATE_INFO> GetStateMappings()
    {
        return {};
    }

    /**
     * Get valid transitions from the current state of a project.
     *
     * @param aProjectId ID of the project.
     * @return List of valid transitions with precondition info.
     */
    virtual std::vector<PLM_VALID_TRANSITION> GetValidTransitions( const wxString& aProjectId )
    {
        return {};
    }

    /**
     * Transition a project to a new lifecycle state.
     *
     * @param aProjectId ID of the project.
     * @param aTargetState Native state name to transition to.
     * @param aComment Optional comment explaining the transition.
     * @return Result with the new state on success, or pending approval info.
     */
    virtual PLM_STATE_TRANSITION_RESULT ChangeState( const wxString& aProjectId,
                                                      const wxString& aTargetState,
                                                      const wxString& aComment )
    {
        return PLM_STATE_TRANSITION_RESULT::Error( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                                    wxS( "ChangeState not supported" ) );
    }

    /**
     * Get the current lifecycle state of a project.
     *
     * @param aProjectId ID of the project.
     * @param[out] aStateInfo Populated with the current state info.
     * @return True if the state was successfully retrieved.
     */
    virtual bool GetCurrentState( const wxString& aProjectId, PLM_STATE_INFO& aStateInfo )
    {
        return false;
    }

    //-------------------------------------------------------------------------
    // Review Interface (Optional)
    //-------------------------------------------------------------------------

    /**
     * Submit a project revision for review.
     *
     * @param aProjectId ID of the project.
     * @param aOptions Review submission options (title, reviewers, etc.).
     * @return Result with the created review on success.
     */
    virtual PLM_REVIEW_RESULT SubmitForReview( const wxString&                aProjectId,
                                                const PLM_REVIEW_SUBMIT_OPTIONS& aOptions )
    {
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                           wxS( "SubmitForReview not supported" ) );
    }

    /**
     * Get all reviews for a project.
     *
     * @param aProjectId ID of the project.
     * @return List of reviews, most recent first.
     */
    virtual std::vector<PLM_REVIEW> GetReviews( const wxString& aProjectId )
    {
        return {};
    }

    /**
     * Get a single review by ID.
     *
     * @param aReviewId ID of the review.
     * @return The review, or empty review if not found.
     */
    virtual PLM_REVIEW GetReview( const wxString& aReviewId )
    {
        return PLM_REVIEW();
    }

    /**
     * Get comments on a review.
     *
     * @param aReviewId ID of the review.
     * @return List of comments with spatial annotations.
     */
    virtual std::vector<PLM_REVIEW_COMMENT> GetReviewComments( const wxString& aReviewId )
    {
        return {};
    }

    /**
     * Add a comment to a review.
     *
     * @param aComment Comment data including optional spatial annotation.
     * @return True if the comment was added successfully.
     */
    virtual bool AddReviewComment( const PLM_REVIEW_COMMENT& aComment )
    {
        return false;
    }

    /**
     * Resolve or unresolve a review comment.
     *
     * @param aCommentId ID of the comment.
     * @param aResolved True to mark resolved, false to unresolve.
     * @return True if the resolution state was updated.
     */
    virtual bool ResolveReviewComment( const wxString& aCommentId, bool aResolved )
    {
        return false;
    }

    /**
     * Submit a formal review decision.
     *
     * @param aReviewId ID of the review.
     * @param aDecision Approval, rejection, or other decision.
     * @param aComment Comment explaining the decision.
     * @return Result with updated review on success.
     */
    virtual PLM_REVIEW_RESULT SubmitReviewDecision( const wxString&     aReviewId,
                                                     PLM_REVIEW_DECISION aDecision,
                                                     const wxString&     aComment )
    {
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                           wxS( "SubmitReviewDecision not supported" ) );
    }

    /**
     * Merge a review into the target branch.
     *
     * @param aReviewId ID of the review.
     * @param aStrategy Merge strategy (e.g., "merge", "squash", "rebase").
     * @return Result with updated review on success.
     */
    virtual PLM_REVIEW_RESULT MergeReview( const wxString& aReviewId,
                                            const wxString& aStrategy )
    {
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                           wxS( "MergeReview not supported" ) );
    }

    /**
     * Check if this plugin supports design review workflows.
     *
     * @return True if review operations are supported.
     */
    virtual bool SupportsReview() const
    {
        return GetCapabilities().supports_review;
    }

    //-------------------------------------------------------------------------
    // Forge Provider Interface (Optional)
    //-------------------------------------------------------------------------

    /**
     * Get the forge provider for Git hosting platform operations.
     *
     * Returns a pointer to the forge provider abstraction for operations
     * specific to Git hosting platforms (GitHub, GitLab, etc.).
     *
     * @return Forge provider, or nullptr if not supported.
     */
    virtual PLM_FORGE_PROVIDER* GetForgeProvider()
    {
        return nullptr;
    }

    //-------------------------------------------------------------------------
    // Part Management Interface (Optional)
    //-------------------------------------------------------------------------

    /**
     * Create a new part in the PLM part database.
     *
     * @param aOptions Part creation parameters.
     * @return The created part, or empty part on failure.
     */
    virtual PLM_PART CreatePart( const PLM_PART_CREATE_OPTIONS& aOptions )
    {
        return PLM_PART();
    }

    /**
     * Update an existing part.
     *
     * @param aPart Updated part data.
     * @return The updated part, or empty part on failure.
     */
    virtual PLM_PART UpdatePart( const PLM_PART& aPart )
    {
        return PLM_PART();
    }

    /**
     * Transition a part to a new lifecycle state.
     *
     * @param aPartId ID of the part.
     * @param aNewState Target lifecycle state.
     * @param aComment Comment explaining the state change.
     * @return The updated part, or empty part on failure.
     */
    virtual PLM_PART UpdatePartState( const wxString& aPartId,
                                       PLM_PART_STATE  aNewState,
                                       const wxString& aComment )
    {
        return PLM_PART();
    }

    /**
     * Get approved vendor sources for a part.
     *
     * @param aPartId ID of the part.
     * @return List of vendor source records.
     */
    virtual std::vector<PLM_VENDOR_SOURCE> GetVendorSources( const wxString& aPartId )
    {
        return {};
    }

    /**
     * Get where-used references for a part.
     *
     * @param aPartId ID of the part.
     * @return List of BOMs and assemblies that use this part.
     */
    virtual std::vector<PLM_WHERE_USED_ENTRY> GetWhereUsed( const wxString& aPartId )
    {
        return {};
    }

    //-------------------------------------------------------------------------
    // Release Interface (Optional)
    //-------------------------------------------------------------------------

    /**
     * Run pre-release validation checks.
     *
     * @param aProjectId ID of the project to validate.
     * @return List of validation check results.
     */
    virtual std::vector<PLM_RELEASE_CHECK> ValidateForRelease( const wxString& aProjectId )
    {
        return {};
    }

    /**
     * Create a new release package.
     *
     * @param aProjectId ID of the project to release.
     * @param aOptions Release creation options (version, artifacts, etc.).
     * @return Result with the created release on success.
     */
    virtual PLM_RELEASE_RESULT CreateRelease( const wxString&          aProjectId,
                                               const PLM_RELEASE_OPTIONS& aOptions )
    {
        return PLM_RELEASE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                            wxS( "CreateRelease not supported" ) );
    }

    /**
     * Finalize and publish a release.
     *
     * @param aReleaseId ID of the release to finalize.
     * @return Result with the finalized release on success.
     */
    virtual PLM_RELEASE_RESULT FinalizeRelease( const wxString& aReleaseId )
    {
        return PLM_RELEASE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                            wxS( "FinalizeRelease not supported" ) );
    }

    /**
     * List all releases for a project.
     *
     * @param aProjectId ID of the project.
     * @return List of releases, most recent first.
     */
    virtual std::vector<PLM_RELEASE> ListReleases( const wxString& aProjectId )
    {
        return {};
    }

    /**
     * Get a single release by ID.
     *
     * @param aReleaseId ID of the release.
     * @return The release, or empty release if not found.
     */
    virtual PLM_RELEASE GetRelease( const wxString& aReleaseId )
    {
        return PLM_RELEASE();
    }

    /**
     * Suggest the next version string for a project.
     *
     * @param aProjectId ID of the project.
     * @param aScheme Versioning scheme to use.
     * @return Suggested version string, or empty if unable to determine.
     */
    virtual wxString SuggestNextVersion( const wxString&    aProjectId,
                                          PLM_VERSION_SCHEME aScheme )
    {
        return wxEmptyString;
    }

    /**
     * Check if this plugin supports release packaging.
     *
     * @return True if release operations are supported.
     */
    virtual bool SupportsReleasePackaging() const
    {
        return GetCapabilities().supports_release_packaging;
    }

    //-------------------------------------------------------------------------
    // Deviation Branch Interface (Optional)
    //-------------------------------------------------------------------------

    /**
     * Create a deviation branch from a released revision.
     *
     * @param aProjectId ID of the project.
     * @param aBaseRevision Revision to branch from.
     * @param aReason Reason for the deviation.
     * @return The created branch metadata, or empty on failure.
     */
    virtual PLM_DEVIATION_BRANCH CreateDeviationBranch( const wxString& aProjectId,
                                                         const wxString& aBaseRevision,
                                                         const wxString& aReason )
    {
        return PLM_DEVIATION_BRANCH();
    }

    /**
     * List deviation branches for a project.
     *
     * @param aProjectId ID of the project.
     * @return List of deviation branches.
     */
    virtual std::vector<PLM_DEVIATION_BRANCH> ListDeviationBranches(
            const wxString& aProjectId )
    {
        return {};
    }

    //-------------------------------------------------------------------------
    // Variant Interface (Optional)
    //-------------------------------------------------------------------------

    /**
     * Get the variant configuration for a project.
     *
     * @param aProjectId ID of the project.
     * @return Variant definition with all configurations.
     */
    virtual PLM_VARIANT GetVariants( const wxString& aProjectId )
    {
        return PLM_VARIANT( aProjectId );
    }

    /**
     * Create a new variant configuration.
     *
     * @param aProjectId ID of the project.
     * @param aConfig Variant configuration to create.
     * @return Result with the updated variant on success.
     */
    virtual PLM_VARIANT_RESULT CreateVariant( const wxString&          aProjectId,
                                               const PLM_VARIANT_CONFIG& aConfig )
    {
        return PLM_VARIANT_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                            wxS( "CreateVariant not supported" ) );
    }

    /**
     * Update an existing variant configuration.
     *
     * @param aConfig Updated variant configuration.
     * @return Result with the updated variant on success.
     */
    virtual PLM_VARIANT_RESULT UpdateVariant( const PLM_VARIANT_CONFIG& aConfig )
    {
        return PLM_VARIANT_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                            wxS( "UpdateVariant not supported" ) );
    }

    /**
     * Delete a variant configuration.
     *
     * @param aConfigId ID of the variant configuration to delete.
     * @return True if deletion succeeded.
     */
    virtual bool DeleteVariant( const wxString& aConfigId )
    {
        return false;
    }

    /**
     * Check if this plugin supports product variants.
     *
     * @return True if variant operations are supported.
     */
    virtual bool SupportsVariants() const
    {
        return GetCapabilities().supports_variants;
    }

    //-------------------------------------------------------------------------
    // CI/CD Pipeline Interface (Optional)
    //-------------------------------------------------------------------------

    /**
     * Get pipeline runs for a project, optionally filtered.
     *
     * @param aProjectId ID of the project.
     * @param aFilter Filter criteria for pipeline runs.
     * @return List of pipeline runs matching the filter.
     */
    virtual std::vector<PLM_PIPELINE_RUN> GetPipelineRuns(
            const wxString&          aProjectId,
            const PLM_PIPELINE_FILTER& aFilter )
    {
        return {};
    }

    /**
     * Get a specific pipeline run.
     *
     * @param aProjectId ID of the project.
     * @param aRunId ID of the pipeline run.
     * @return The pipeline run, or empty run if not found.
     */
    virtual PLM_PIPELINE_RUN GetPipelineRun( const wxString& aProjectId,
                                              const wxString& aRunId )
    {
        return PLM_PIPELINE_RUN();
    }

    /**
     * Download a CI artifact to a local path.
     *
     * @param aProjectId ID of the project.
     * @param aRunId ID of the pipeline run.
     * @param aArtifactId ID of the artifact.
     * @param aLocalPath Local path to download to.
     * @return True if download succeeded.
     */
    virtual bool DownloadCIArtifact( const wxString& aProjectId,
                                      const wxString& aRunId,
                                      const wxString& aArtifactId,
                                      const wxString& aLocalPath )
    {
        return false;
    }

    /**
     * Get the aggregate pipeline status for a commit.
     *
     * @param aProjectId ID of the project.
     * @param aCommitSha Commit SHA to check.
     * @return Aggregate pipeline status.
     */
    virtual PLM_CI_RUN_STATUS GetCommitPipelineStatus( const wxString& aProjectId,
                                                        const wxString& aCommitSha )
    {
        return PLM_CI_RUN_STATUS::UNKNOWN;
    }

    //-------------------------------------------------------------------------
    // Audit Trail Interface (Optional)
    //-------------------------------------------------------------------------

    /**
     * Query the audit log with filters.
     *
     * @param aFilter Audit log filter criteria.
     * @return Paginated audit query results.
     */
    virtual PLM_AUDIT_QUERY_RESULT GetAuditLog( const PLM_AUDIT_FILTER& aFilter )
    {
        PLM_AUDIT_QUERY_RESULT result;
        result.error = PLM_ERROR_CODE::ERR_NOT_SUPPORTED;
        result.error_message = wxS( "GetAuditLog not supported" );
        return result;
    }

    /**
     * Get the audit log for a specific project.
     *
     * Convenience wrapper around GetAuditLog() that filters by project.
     *
     * @param aProjectId ID of the project.
     * @param aLimit Maximum number of entries to return.
     * @return Paginated audit query results.
     */
    virtual PLM_AUDIT_QUERY_RESULT GetProjectAuditLog( const wxString& aProjectId,
                                                        size_t          aLimit )
    {
        PLM_AUDIT_FILTER filter;
        filter.project_id = aProjectId;
        filter.limit = aLimit;
        return GetAuditLog( filter );
    }

    //-------------------------------------------------------------------------
    // File Operations Extensions (Optional)
    //-------------------------------------------------------------------------

    /**
     * Create a folder in the PLM repository.
     *
     * @param aParentFolderId ID of the parent folder.
     * @param aName Name for the new folder.
     * @return Result with the created folder info on success.
     */
    virtual PLM_CHECKOUT_RESULT CreateFolder( const wxString& aParentFolderId,
                                               const wxString& aName )
    {
        return PLM_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                             wxS( "CreateFolder not supported" ) );
    }

    /**
     * Get the list of locally modified files in a checkout.
     *
     * @param aLocalPath Local checkout directory.
     * @return List of modified file paths relative to aLocalPath.
     */
    virtual std::vector<wxString> GetModifiedFiles( const wxString& aLocalPath )
    {
        return {};
    }

    //-------------------------------------------------------------------------
    // Reload Callback Interface
    //-------------------------------------------------------------------------

    /**
     * Set the callback for signaling KiCad to reload the current project.
     *
     * Called by the PLM manager after plugin construction to wire up
     * project reload notifications.
     *
     * @param aCallback Callback to invoke when reload is needed.
     */
    void SetReloadCallback( PLM_RELOAD_CALLBACK aCallback )
    {
        m_reloadCallback = std::move( aCallback );
    }

    //-------------------------------------------------------------------------
    // Preview Cache Interface (Optional)
    //-------------------------------------------------------------------------

    /**
     * Get the preview cache for this plugin.
     *
     * Returns a pointer to the preview cache used for caching file preview
     * images. May return nullptr if previews are not supported.
     *
     * @return Preview cache pointer, or nullptr if not available.
     */
    virtual PLM_PREVIEW_CACHE* GetPreviewCache()
    {
        return nullptr;
    }

protected:
    /**
     * Protected constructor to prevent direct instantiation.
     * Derived classes should have public constructors.
     *
     * Defined out-of-line in plm_plugin.cpp because the implicit
     * destructor instantiation needs the manager unique_ptr types to be
     * complete (see ~PLM_PLUGIN above).
     */
    PLM_PLUGIN();

    // Non-copyable. Move operations are also disabled because plugins are
    // owned by unique_ptr in the plugin manager and the manager unique_ptrs
    // below would require out-of-line definitions otherwise.
    PLM_PLUGIN( const PLM_PLUGIN& ) = delete;
    PLM_PLUGIN& operator=( const PLM_PLUGIN& ) = delete;
    PLM_PLUGIN( PLM_PLUGIN&& ) = delete;
    PLM_PLUGIN& operator=( PLM_PLUGIN&& ) = delete;

    //-------------------------------------------------------------------------
    // Shared infrastructure for derived plugins
    //-------------------------------------------------------------------------

    /**
     * Construct the plugin-side manager objects (activity tracker, state
     * mapper, cache manager, sync state, state persistence). Subclasses
     * should call this from OnLoad() before authenticating. Safe to call
     * more than once; subsequent calls are no-ops.
     */
    void InitManagers();

    /**
     * Tear down all manager objects, releasing on-disk and in-memory
     * resources. Subclasses should call this from OnUnload() at the
     * point where their other manager-style state is reset.
     */
    void TeardownManagers();

    /**
     * Bind the activity tracker's fetch function to GetAuditLog() and
     * configure it for the current session. Idempotent; safe to call
     * from every Authenticate() success path.
     */
    void SetupActivityTracker();

    /**
     * Bind the state mapper's callback to GetStateMappings() and load
     * the initial mapping list. Idempotent.
     */
    void SetupStateMapper();

    /**
     * Push an audit event into the activity tracker. No-op if the
     * tracker is not initialized.
     *
     * @param aType Audit event category.
     * @param aProjectId Affected project (may be empty for pre-auth events).
     * @param aSuccess True for normal completion, false for an error path.
     * @param aDetails Optional human-readable detail string.
     */
    void RecordAuditEvent( PLM_AUDIT_EVENT_TYPE aType, const wxString& aProjectId, bool aSuccess,
                           const wxString& aDetails = wxEmptyString );

    /**
     * Persist the current project's binding and sync state via
     * PLM_SYNC_STATE_MANAGER and PLM_STATE_PERSISTENCE. Reads the
     * server URL from GetCurrentSession().server_url; no-op if either
     * the server URL or aLocalPath is empty (PLM_STATE_PERSISTENCE
     * does not validate inputs itself).
     *
     * @param aLocalPath Local checkout path.
     * @param aProjectId PLM project identifier.
     * @param aRevision Current revision string.
     */
    void PersistProjectState( const wxString& aLocalPath, const wxString& aProjectId,
                              const wxString& aRevision );

    std::unique_ptr<PLM_ACTIVITY_TRACKER>     m_activityTracker;   ///< Recent PLM activity for UI
    std::unique_ptr<PLM_STATE_MAPPER>         m_stateMapper;       ///< Native-state to category map
    std::unique_ptr<PLM_CACHE_MANAGER>        m_cacheManager;      ///< Per-server cache directory
    std::unique_ptr<PLM_SYNC_STATE_MANAGER>   m_syncStateManager;  ///< Per-project sync tracking
    std::unique_ptr<PLM_STATE_PERSISTENCE>    m_statePersistence;  ///< Project state on disk

    // Shared-library managers. Session-scoped — InitManagers wires the
    // download and server-version callbacks to this plugin's virtuals.
    // PLM_LIB_REFERENCE_MANAGER is intentionally NOT here: it is project-
    // scoped (per its GetDefaultPath(projectPath)) and will be owned by the
    // project-binding path when that wave lands.
    //
    // Member order matters: unique_ptr destruction runs in reverse, so
    // m_libSync (declared last) is destroyed first. Its dtor calls
    // CancelSync()/WaitForSync() which may join a worker thread holding raw
    // pointers to provider/tracker; both must outlive sync. Provider also
    // holds a raw tracker pointer, so tracker must outlive provider.
    std::unique_ptr<PLM_LIB_VERSION_TRACKER>  m_libVersionTracker; ///< Per-lib pinning + update detection
    std::unique_ptr<PLM_LIB_PROVIDER>         m_libProvider;       ///< Cache + revision-aware download
    std::unique_ptr<PLM_LIB_SYNC>             m_libSync;           ///< Async lib sync orchestrator

    /**
     * Query permissions from the backend for a project.
     *
     * Plugins override this to provide actual permission lookups.
     * Called by GetPermissions() when the cache is empty or expired.
     *
     * @param aProjectId ID of the project to check permissions for.
     * @return Permissions structure indicating allowed operations.
     */
    virtual PLM_PERMISSIONS QueryPermissions( const wxString& aProjectId )
    {
        return PLM_PERMISSIONS();
    }

    /**
     * Notify KiCad that the project should be reloaded.
     *
     * Call this from plugin implementations when external changes
     * are detected that require reloading the project in the editor.
     *
     * @param aProjectPath Path to the project that changed.
     * @param aForceReload True to force full reload, false for incremental.
     */
    void NotifyReload( const wxString& aProjectPath, bool aForceReload )
    {
        if( m_reloadCallback )
            m_reloadCallback( aProjectPath, aForceReload );
    }

private:
    struct PermCacheEntry
    {
        PLM_PERMISSIONS                          permissions;
        std::chrono::steady_clock::time_point    timestamp;
    };

    std::mutex                             m_permCacheMutex;
    std::map<wxString, PermCacheEntry>     m_permCache;
    std::chrono::seconds                   m_permCacheTTL{ 300 };

    PLM_RELOAD_CALLBACK                    m_reloadCallback;
};


/**
 * Function pointer type for plugin factory function.
 */
using PLM_PLUGIN_CREATE_FUNC = PLM_PLUGIN* (*)();

/**
 * Function pointer type for plugin destruction function.
 */
using PLM_PLUGIN_DESTROY_FUNC = void (*)( PLM_PLUGIN* );

/**
 * Function pointer type for API version query.
 */
using PLM_PLUGIN_VERSION_FUNC = int (*)();


/**
 * Macro to declare plugin entry points.
 *
 * Use this macro in the plugin implementation file to create the
 * required exported functions for plugin loading.
 *
 * Example:
 * @code
 * PLM_PLUGIN_ENTRY_POINTS( MyPlmPlugin )
 * @endcode
 *
 * This creates:
 * - PLM_CreatePlugin(): Factory function returning new plugin instance
 * - PLM_DestroyPlugin(): Cleanup function to delete plugin
 * - PLM_GetAPIVersion(): Returns the API version the plugin was built with
 */
#ifdef _WIN32
    #define PLM_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define PLM_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#define PLM_PLUGIN_ENTRY_POINTS( PluginClass )                                  \
    extern "C" PLM_PLUGIN_EXPORT PLM_PLUGIN* PLM_CreatePlugin()                 \
    {                                                                           \
        return new PluginClass();                                               \
    }                                                                           \
                                                                                \
    extern "C" PLM_PLUGIN_EXPORT void PLM_DestroyPlugin( PLM_PLUGIN* aPlugin )  \
    {                                                                           \
        delete aPlugin;                                                         \
    }                                                                           \
                                                                                \
    extern "C" PLM_PLUGIN_EXPORT int PLM_GetAPIVersion()                        \
    {                                                                           \
        return PLM_API_VERSION;                                                 \
    }


/**
 * Names of the exported plugin functions.
 *
 * Used by the plugin manager to look up symbols in loaded libraries.
 */
constexpr const char* PLM_FUNC_CREATE = "PLM_CreatePlugin";
constexpr const char* PLM_FUNC_DESTROY = "PLM_DestroyPlugin";
constexpr const char* PLM_FUNC_VERSION = "PLM_GetAPIVersion";


#endif // PLM_PLUGIN_H
