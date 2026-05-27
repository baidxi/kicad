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
 * @file plm_git_plugin.h
 * @brief Git-based PLM plugin for KiCad project version control.
 *
 * This plugin provides PLM functionality using Git repositories
 * as the backend storage. It supports local and remote repositories
 * via HTTPS and SSH authentication.
 */

#ifndef PLM_GIT_PLUGIN_H
#define PLM_GIT_PLUGIN_H

#include <plm/plm_activity_tracker.h>
#include <plm/plm_cache_manager.h>
#include <plm/plm_diff_analyzer.h>
#include <plm/plm_plugin.h>
#include <plm/plm_state_mapper.h>
#include <plm/plm_state_persistence.h>
#include <plm/plm_sync_state.h>
#include "plm_git_auth.h"
#include "plm_git_forge.h"

#include <map>
#include <memory>
#include <mutex>


/**
 * Git-based PLM plugin.
 *
 * Implements the PLM_PLUGIN interface using Git repositories as the
 * version control backend. Supports:
 * - Local repositories
 * - Remote repositories (GitHub, GitLab, Bitbucket, etc.)
 * - HTTPS and SSH authentication
 * - Branch-based workflows
 * - Revision history from git log
 *
 * Projects are mapped to directories within the repository, with
 * KiCad project files tracked as regular files. Locking is implemented
 * using Git LFS locking where available, or advisory locks via
 * special marker files.
 */
class PLM_GIT_PLUGIN : public PLM_PLUGIN
{
public:
    PLM_GIT_PLUGIN();
    ~PLM_GIT_PLUGIN() override;

    //-------------------------------------------------------------------------
    // Plugin Interface
    //-------------------------------------------------------------------------

    PLM_PLUGIN_INFO  GetPluginInfo() const override;
    PLM_CAPABILITIES GetCapabilities() const override;

    bool OnLoad() override;
    void OnUnload() override;

    //-------------------------------------------------------------------------
    // Authentication Interface
    //-------------------------------------------------------------------------

    PLM_AUTH_RESULT Authenticate( const PLM_CREDENTIALS& aCredentials ) override;
    bool            Logout() override;
    bool            IsAuthenticated() const override;
    PLM_SESSION     GetCurrentSession() const override;
    PLM_AUTH_RESULT RefreshSession() override;

    //-------------------------------------------------------------------------
    // Health Check Interface
    //-------------------------------------------------------------------------

    bool Ping() override;

    //-------------------------------------------------------------------------
    // Navigation Interface
    //-------------------------------------------------------------------------

    std::vector<PLM_FOLDER>               GetRootFolders() override;
    std::vector<PLM_FOLDER>               GetChildFolders( const wxString& aFolderId ) override;
    PLM_FOLDER_PATH                       GetFolderPath( const wxString& aFolderId ) override;
    PLM_LIST_RESULT<PLM_FILE_INFO>        ListFiles( const wxString&         aFolderId,
                                                     const PLM_LIST_OPTIONS& aOptions ) override;
    PLM_LIST_RESULT<PLM_FILE_INFO>        SearchFiles( const wxString&         aQuery,
                                                       const PLM_LIST_OPTIONS& aOptions ) override;
    PLM_LIST_RESULT<PLM_FILE_INFO> SearchWithFilters(
            const std::map<wxString, wxString>& aFilters,
            const PLM_LIST_OPTIONS&             aOptions ) override;

    std::vector<PLM_SEARCH_FIELD> GetSearchFields() override;

    //-------------------------------------------------------------------------
    // File Operations Interface
    //-------------------------------------------------------------------------

    PLM_CHECKOUT_RESULT Checkout( const wxString&             aProjectId,
                                   const wxString&             aLocalPath,
                                   const PLM_CHECKOUT_OPTIONS& aOptions ) override;

    PLM_CHECKIN_RESULT Checkin( const wxString&            aLocalPath,
                                 const PLM_CHECKIN_OPTIONS& aOptions ) override;

    PLM_FILE_INFO       GetFileInfo( const wxString& aProjectId ) override;

    PLM_CHECKOUT_RESULT CreateProject( const wxString&          aFolderId,
                                        const wxString&          aName,
                                        const PLM_ATTRIBUTE_SET& aMetadata,
                                        const wxString&          aTemplateId = wxEmptyString,
                                        const PLM_CLONE_OPTIONS& aCloneOptions =
                                                PLM_CLONE_OPTIONS() ) override;

    bool                       DeleteProject( const wxString& aProjectId ) override;
    PLM_CANCEL_CHECKOUT_RESULT CancelCheckout( const wxString& aLocalPath ) override;

    //-------------------------------------------------------------------------
    // Revision Interface
    //-------------------------------------------------------------------------

    PLM_REVISION_HISTORY GetRevisionHistory( const wxString& aProjectId ) override;

    PLM_REVISION GetRevision( const wxString& aProjectId, const wxString& aRevisionId ) override;

    bool DownloadRevision( const wxString& aProjectId, const wxString& aRevisionId,
                           const wxString& aLocalPath ) override;

    PLM_REVISION_COMPARE CompareRevisions( const wxString& aProjectId, const wxString& aRevision1,
                                            const wxString& aRevision2 ) override;

    bool RestoreRevision( const wxString& aProjectId, const wxString& aRevisionId,
                          const wxString& aComment ) override;

    //-------------------------------------------------------------------------
    // Lock Interface
    //-------------------------------------------------------------------------

    PLM_LOCK_RESULT Lock( const PLM_LOCK_REQUEST& aRequest ) override;
    PLM_LOCK_RESULT Unlock( const PLM_UNLOCK_REQUEST& aRequest ) override;
    PLM_LOCK_INFO   GetLockInfo( const wxString& aProjectId ) override;

    //-------------------------------------------------------------------------
    // Schema Interface
    //-------------------------------------------------------------------------

    PLM_SCHEMA        GetSchema() override;
    PLM_ATTRIBUTE_SET GetAttributes( const wxString& aProjectId ) override;
    bool              SetAttributes( const wxString&          aProjectId,
                                     const PLM_ATTRIBUTE_SET& aAttributes ) override;

    //-------------------------------------------------------------------------
    // Permissions Interface
    //-------------------------------------------------------------------------

    wxString GetUserDisplayName( const wxString& aUserId ) override;

protected:
    PLM_PERMISSIONS QueryPermissions( const wxString& aProjectId ) override;

public:

    //-------------------------------------------------------------------------
    // Part Database Interface
    //-------------------------------------------------------------------------

    PLM_LIST_RESULT<PLM_PART>         SearchParts( const PLM_PART_SEARCH_OPTIONS& aOptions ) override;
    PLM_PART                          GetPart( const wxString& aPartId ) override;
    bool                              DownloadPartLibrary( const wxString& aPartId,
                                                           const wxString& aLocalPath ) override;
    PLM_BOM_VALIDATION_RESULT         ValidateBOM( const std::vector<wxString>& aPartNumbers ) override;
    std::vector<PLM_PART_REPLACEMENT> GetReplacements( const wxString& aPartId ) override;

    //-------------------------------------------------------------------------
    // Shared Library Interface
    //-------------------------------------------------------------------------

    std::vector<PLM_LIB_REFERENCE>
    GetAvailableLibraries( const wxString& aProjectId = wxEmptyString ) override;

    std::vector<PLM_LIB_VERSION_INFO> GetLibraryVersions( const wxString& aLibId ) override;
    wxString                          GetLatestLibraryRevision( const wxString& aLibId ) override;
    PLM_LIB_DOWNLOAD_RESULT           DownloadLibrary( const wxString& aLibId,
                                                       PLM_LIB_TYPE aLibType,
                                                       const wxString& aRequestedRevision,
                                                       const wxString& aLocalPath ) override;

    //-------------------------------------------------------------------------
    // ECO Extended Interface
    //-------------------------------------------------------------------------

    PLM_ECO_CREATE_RESULT CreateECO( const PLM_ECO_CREATE_OPTIONS& aOptions ) override;
    PLM_ECO_UPDATE_RESULT GetECO( const wxString& aEcoId ) override;
    PLM_ECO_UPDATE_RESULT UpdateECOStatus( const PLM_ECO_STATUS_TRANSITION& aTransition ) override;
    PLM_ECO_UPDATE_RESULT UpdateECOItems( const wxString&                  aEcoId,
                                           const std::vector<PLM_ECO_ITEM>& aItems ) override;
    PLM_ECO_UPDATE_RESULT UpdateECOImpact( const wxString&       aEcoId,
                                            const PLM_ECO_IMPACT& aImpact ) override;
    std::vector<PLM_ECO_STATUS_TRANSITION> GetECOHistory( const wxString& aEcoId ) override;
    PLM_ECO_CREATE_RESULT ConvertECRToECO( const wxString& aEcrId ) override;

    //-------------------------------------------------------------------------
    // BOM Management Interface
    //-------------------------------------------------------------------------

    PLM_BOM                  GetBOM( const wxString& aProductId, PLM_BOM_TYPE aType ) override;
    std::vector<PLM_BOM>     GetBOMs( const wxString& aProductId ) override;
    PLM_BOM_RESULT           CreateBOM( const PLM_BOM& aBom ) override;
    PLM_BOM_RESULT           UpdateBOM( const PLM_BOM& aBom ) override;
    bool                     DeleteBOM( const wxString& aBomId ) override;
    PLM_BOM_DIFF_RESULT      CompareBOMs( const PLM_BOM&              aBomA,
                                           const PLM_BOM&              aBomB,
                                           const PLM_BOM_DIFF_OPTIONS& aOptions ) override;
    PLM_BOM_VALIDATION_RESULT ValidateBOMEntries(
            const std::vector<PLM_BOM_ENTRY>& aEntries ) override;

    //-------------------------------------------------------------------------
    // Manufacturing/Routing Interface
    //-------------------------------------------------------------------------

    PLM_ROUTING                 GetRouting( const wxString& aProductId ) override;
    std::vector<PLM_WORKCENTER> GetWorkcenters() override;
    PLM_ROUTING_RESULT          SaveRouting( const PLM_ROUTING& aRouting ) override;

    //-------------------------------------------------------------------------
    // Lifecycle State Transition Interface
    //-------------------------------------------------------------------------

    std::vector<PLM_STATE_INFO>        GetStateMappings() override;
    std::vector<PLM_VALID_TRANSITION>  GetValidTransitions( const wxString& aProjectId ) override;
    PLM_STATE_TRANSITION_RESULT        ChangeState( const wxString& aProjectId,
                                                     const wxString& aTargetState,
                                                     const wxString& aComment ) override;
    bool GetCurrentState( const wxString& aProjectId, PLM_STATE_INFO& aStateInfo ) override;

    //-------------------------------------------------------------------------
    // Review and Approval Interface
    //-------------------------------------------------------------------------

    PLM_REVIEW_RESULT SubmitForReview( const wxString&                  aProjectId,
                                        const PLM_REVIEW_SUBMIT_OPTIONS& aOptions ) override;
    std::vector<PLM_REVIEW>         GetReviews( const wxString& aProjectId ) override;
    PLM_REVIEW                      GetReview( const wxString& aReviewId ) override;
    std::vector<PLM_REVIEW_COMMENT> GetReviewComments( const wxString& aReviewId ) override;
    bool                            AddReviewComment( const PLM_REVIEW_COMMENT& aComment ) override;
    bool ResolveReviewComment( const wxString& aCommentId, bool aResolved ) override;
    PLM_REVIEW_RESULT SubmitReviewDecision( const wxString&     aReviewId,
                                             PLM_REVIEW_DECISION aDecision,
                                             const wxString&     aComment ) override;
    PLM_REVIEW_RESULT MergeReview( const wxString& aReviewId,
                                    const wxString& aStrategy ) override;

    //-------------------------------------------------------------------------
    // Forge Provider Interface
    //-------------------------------------------------------------------------

    PLM_FORGE_PROVIDER* GetForgeProvider() override;

    //-------------------------------------------------------------------------
    // Part Management Extended Interface
    //-------------------------------------------------------------------------

    PLM_PART                          CreatePart( const PLM_PART_CREATE_OPTIONS& aOptions ) override;
    PLM_PART                          UpdatePart( const PLM_PART& aPart ) override;
    PLM_PART                          UpdatePartState( const wxString& aPartId,
                                                        PLM_PART_STATE  aNewState,
                                                        const wxString& aComment ) override;
    std::vector<PLM_VENDOR_SOURCE>    GetVendorSources( const wxString& aPartId ) override;
    std::vector<PLM_WHERE_USED_ENTRY> GetWhereUsed( const wxString& aPartId ) override;

    //-------------------------------------------------------------------------
    // Release Management Interface
    //-------------------------------------------------------------------------

    std::vector<PLM_RELEASE_CHECK> ValidateForRelease( const wxString& aProjectId ) override;
    PLM_RELEASE_RESULT CreateRelease( const wxString&            aProjectId,
                                       const PLM_RELEASE_OPTIONS& aOptions ) override;
    PLM_RELEASE_RESULT         FinalizeRelease( const wxString& aReleaseId ) override;
    std::vector<PLM_RELEASE>   ListReleases( const wxString& aProjectId ) override;
    PLM_RELEASE                GetRelease( const wxString& aReleaseId ) override;
    wxString                   SuggestNextVersion( const wxString&    aProjectId,
                                                    PLM_VERSION_SCHEME aScheme ) override;

    //-------------------------------------------------------------------------
    // Deviation Branch Interface
    //-------------------------------------------------------------------------

    PLM_DEVIATION_BRANCH CreateDeviationBranch( const wxString& aProjectId,
                                                 const wxString& aBaseRevision,
                                                 const wxString& aReason ) override;
    std::vector<PLM_DEVIATION_BRANCH> ListDeviationBranches( const wxString& aProjectId ) override;

    //-------------------------------------------------------------------------
    // Variant Configuration Interface
    //-------------------------------------------------------------------------

    PLM_VARIANT        GetVariants( const wxString& aProjectId ) override;
    PLM_VARIANT_RESULT CreateVariant( const wxString&           aProjectId,
                                       const PLM_VARIANT_CONFIG& aConfig ) override;
    PLM_VARIANT_RESULT UpdateVariant( const PLM_VARIANT_CONFIG& aConfig ) override;
    bool               DeleteVariant( const wxString& aConfigId ) override;

    //-------------------------------------------------------------------------
    // CI/CD Interface
    //-------------------------------------------------------------------------

    std::vector<PLM_PIPELINE_RUN> GetPipelineRuns( const wxString&            aProjectId,
                                                    const PLM_PIPELINE_FILTER& aFilter ) override;
    PLM_PIPELINE_RUN GetPipelineRun( const wxString& aProjectId,
                                      const wxString& aRunId ) override;
    bool DownloadCIArtifact( const wxString& aProjectId, const wxString& aRunId,
                              const wxString& aArtifactId,
                              const wxString& aLocalPath ) override;
    PLM_CI_RUN_STATUS GetCommitPipelineStatus( const wxString& aProjectId,
                                                const wxString& aCommitSha ) override;

    //-------------------------------------------------------------------------
    // Audit Extended Interface
    //-------------------------------------------------------------------------

    PLM_AUDIT_QUERY_RESULT GetAuditLog( const PLM_AUDIT_FILTER& aFilter ) override;
    PLM_AUDIT_QUERY_RESULT GetProjectAuditLog( const wxString& aProjectId,
                                                size_t          aLimit ) override;

    //-------------------------------------------------------------------------
    // File Operations Extended Interface
    //-------------------------------------------------------------------------

    PLM_CHECKOUT_RESULT      CreateFolder( const wxString& aParentFolderId,
                                            const wxString& aName ) override;
    std::vector<wxString>    GetModifiedFiles( const wxString& aLocalPath ) override;

    //-------------------------------------------------------------------------
    // Preview Cache Interface
    //-------------------------------------------------------------------------

    PLM_PREVIEW_CACHE* GetPreviewCache() override;

private:
    /**
     * Read PLM_CREDENTIALS::properties["shared_libraries"] (JSON array) and
     * populate m_sharedLibrariesCache plus m_sharedLibArtifactPaths. Empty
     * or malformed input leaves both empty; never throws.
     *
     * Each JSON entry must be an object with: lib_id (string), url (string),
     * type ("symbol" | "footprint" | "3d_model"). Optional: artifact_path
     * (string, relative path inside the repo to the artifact for SYMBOL).
     */
    void ParseSharedLibrariesConfig( const PLM_CREDENTIALS& aCredentials );

    /**
     * Resolve a configured shared lib by id; returns nullptr if absent.
     */
    const PLM_LIB_REFERENCE* FindSharedLibrary( const wxString& aLibId ) const;

    /**
     * Build an authenticated remote URL for a shared library, threading
     * m_authHelper as if the lib URL were the main repo. Returns the input
     * unchanged when auth doesn't apply (SSH-managed clone, local path).
     */
    wxString AuthenticatedLibraryUrl( const wxString& aRawUrl );

    /**
     * Run a git command and capture output.
     *
     * @param aArgs Command arguments (git is prepended).
     * @param aOutput Output string to receive stdout.
     * @param aWorkDir Working directory (empty for current).
     * @return Exit code from git command.
     */
    int RunGitCommand( const std::vector<wxString>& aArgs, wxString& aOutput,
                        const wxString& aWorkDir = wxEmptyString );

    /**
     * Check if a path is a valid git repository.
     *
     * @param aPath Path to check.
     * @return True if path is a git repository.
     */
    bool IsGitRepository( const wxString& aPath );

    /**
     * Clone or fetch a remote repository.
     *
     * @param aUrl Remote repository URL.
     * @param aLocalPath Local clone path.
     * @return True if operation succeeded.
     */
    bool CloneOrFetch( const wxString& aUrl, const wxString& aLocalPath );

    /**
     * Get the repository root for a path.
     *
     * @param aPath Path within repository.
     * @return Repository root path.
     */
    wxString GetRepositoryRoot( const wxString& aPath );

    /**
     * Parse git log output into revision list.
     *
     * @param aLogOutput Output from git log command.
     * @return Vector of parsed revisions.
     */
    std::vector<PLM_REVISION> ParseGitLog( const wxString& aLogOutput );

    /**
     * Convert git status to PLM state.
     *
     * @param aStatus Git status string.
     * @return PLM state category.
     */
    PLM_STATE_CATEGORY GitStatusToPLMState( const wxString& aStatus );

    /**
     * Build folder ID from path.
     *
     * @param aPath Path relative to repository root.
     * @return Encoded folder ID.
     */
    wxString PathToFolderId( const wxString& aPath );

    /**
     * Convert folder ID back to path.
     *
     * @param aFolderId Folder ID.
     * @return Path relative to repository root.
     */
    wxString FolderIdToPath( const wxString& aFolderId );

    /**
     * Recursively remove a directory.
     *
     * @param aPath Directory to remove.
     * @return True if removal succeeded.
     */
    bool RemoveDirectoryRecursive( const wxString& aPath );

    /**
     * Clean up temporary clone directory if it exists.
     */
    void CleanupTempClone();

    /**
     * Map PLM_CREDENTIALS to a GIT_AUTH_CONFIG using the credential's auth_method
     * and properties, falling back to DetectAuthMethod() when set to AUTO_DETECT.
     */
    GIT_AUTH_CONFIG BuildAuthConfig( const PLM_CREDENTIALS& aCredentials );

    /**
     * Extract a file at a specific revision into a temporary file. Returns the
     * full temp-file path (caller deletes) or wxEmptyString on failure.
     *
     * @param aRevision Git revision (commit/branch/tag).
     * @param aRepoRelativePath Path relative to the repo root.
     */
    wxString ExtractRevisionToTemp( const wxString& aRevision,
                                    const wxString& aRepoRelativePath );

    /**
     * Project PLM_DIFF_ANALYZER component changes into PLM_REVISION_CHANGE rows
     * appended to aOut.
     *
     * @param aResult Analyzer output to project.
     * @param aRepoRelativePath The file the analysis ran against (for element_id).
     * @param aOut Vector of revision changes to append to.
     */
    void AppendComponentChanges( const PLM_DIFF_ANALYSIS_RESULT&   aResult,
                                 const wxString&                   aRepoRelativePath,
                                 std::vector<PLM_REVISION_CHANGE>& aOut );

    /**
     * Resolve the directory `<git-dir>/plm` for plugin-private state.
     *
     * Uses `git rev-parse --git-dir` so the path works for plain clones,
     * git worktrees (where `.git` is a file pointing at the real git dir),
     * and bare repos. Creates the directory if it does not exist. Returns
     * wxEmptyString if the path cannot be resolved or created.
     */
    wxString GetGitPlmDir();

    /**
     * Persist a comment_id -> review_id mapping in <git-dir>/plm/reviews.json
     * so later ResolveReviewComment calls can recover the review context that
     * forge APIs require but PLM_PLUGIN does not pass through.
     *
     * @param aCommentId Forge comment identifier.
     * @param aReviewId Forge review (PR) identifier owning the comment.
     */
    void RecordReviewComment( const wxString& aCommentId, const wxString& aReviewId );

    /**
     * Look up the review_id previously recorded for aCommentId.
     *
     * @return Review id on success, or wxEmptyString if no mapping exists.
     */
    wxString LookupReviewForComment( const wxString& aCommentId );

    /**
     * Add aBranch to the pending-push queue at <git-dir>/plm/pending_pushes.json
     * so a later DrainPendingPushes() call can re-publish commits that were
     * left local because the remote was unreachable. Idempotent: a branch
     * already in the queue is not duplicated.
     */
    void RecordPendingPush( const wxString& aBranch );

    /**
     * Try to push every branch currently in the pending-push queue. Each
     * successful push removes that branch from the queue; failures leave it
     * for the next attempt. Returns the number of branches that were
     * successfully pushed.
     */
    int DrainPendingPushes();

    /**
     * Ensure the remote repository has been cloned locally. Called on first
     * access after Authenticate() which only validates connectivity.
     *
     * @return True if the local clone is available.
     */
    bool EnsureClone();

    /**
     * Reload the parts cache from parts.csv, respecting the cache timestamp.
     *
     * @return True if parts are available.
     */
    bool EnsurePartsCache();

    /**
     * Write all cached parts back to parts.csv.
     *
     * @return True if write succeeded.
     */
    bool WritePartsCSV();

    /**
     * Parse a simple CSV line respecting quoted fields.
     */
    static wxArrayString ParseCSVLine( const wxString& aLine );

    /**
     * Build the filesystem path to a BOM file for a given product and type.
     */
    wxString BuildBOMPath( const wxString& aProductId, PLM_BOM_TYPE aType ) const;

    /**
     * Parse a BOM CSV file into a PLM_BOM structure.
     */
    PLM_BOM ParseBOMFile( const wxString& aFilePath, const wxString& aProductId ) const;

    /**
     * Write a PLM_BOM to a CSV file.
     */
    bool WriteBOMFile( const wxString& aFilePath, const PLM_BOM& aBom ) const;

    /**
     * Read a JSON file and return parsed content.
     */
    static bool ReadJsonFile( const wxString& aPath, nlohmann::json& aOut );

    /**
     * Write JSON content to a file.
     */
    static bool WriteJsonFile( const wxString& aPath, const nlohmann::json& aData );

    /**
     * Get the absolute path to a project directory from its product ID.
     */
    wxString GetProjectPath( const wxString& aProductId ) const;

    /**
     * Map a PLM_PART_STATE enum to a lowercase string for CSV storage.
     */
    static wxString PartStateToString( PLM_PART_STATE aState );

    /**
     * Map a PLM_BOM_TYPE enum to a filename suffix.
     */
    static wxString BOMTypeToSuffix( PLM_BOM_TYPE aType );

    /**
     * Map a filename suffix back to a PLM_BOM_TYPE.
     */
    static PLM_BOM_TYPE SuffixToBOMType( const wxString& aSuffix );

    wxString    m_repositoryUrl;       ///< Remote repository URL
    wxString    m_localClonePath;      ///< Local clone directory
    wxString    m_currentBranch;       ///< Current working branch
    wxString    m_username;            ///< Authenticated username
    wxString    m_detectedRole;        ///< Detected repository role (e.g. "admin", "write", "read")
    bool        m_isAuthenticated;     ///< Authentication state
    bool        m_isTempClone;         ///< True if m_localClonePath is a temp clone
    PLM_SESSION m_session;             ///< Current session info

    GIT_AUTH_HELPER m_authHelper;                    ///< Authentication helper for git operations
    GIT_AUTH_CONFIG m_authConfig;                    ///< Active authentication configuration

    std::unique_ptr<PLM_GIT_FORGE_PROVIDER> m_forgeProvider; ///< Forge API provider (GitHub/GitLab/Gitea)

    std::map<wxString, wxString> m_userDisplayNames; ///< Cached user names

    time_t                       m_partsCacheModTime = 0;  ///< Modification time of parts.csv at last parse
    std::vector<PLM_PART>        m_partsCache;             ///< Cached parts from last CSV parse

    /// Shared libraries declared by the user via PLM_CREDENTIALS::properties
    /// "shared_libraries" key (JSON array). Populated on Authenticate; flushed
    /// on Logout. PLM_LIB_REFERENCE::server_url carries the remote URL.
    std::vector<PLM_LIB_REFERENCE> m_sharedLibrariesCache;

    /// Repo-relative artifact path per declared lib, keyed by lib_id. Used by
    /// DownloadLibrary to copy the right file out of a fetched clone (SYMBOL
    /// is the only type Slice 1 honors). Empty for libs that don't specify.
    std::map<wxString, wxString>   m_sharedLibArtifactPaths;

    mutable std::recursive_mutex m_mutex;            ///< Protects access to all mutable state
};


#endif // PLM_GIT_PLUGIN_H
