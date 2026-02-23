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
 * @file plm_odoo_plugin.h
 * @brief OdooPLM plugin for KiCad PLM integration.
 *
 * This plugin provides PLM functionality using OdooPLM as the backend.
 * OdooPLM is an open-source PLM module for the Odoo ERP platform.
 * Communication with Odoo uses JSON-RPC over HTTP.
 */

#ifndef PLM_ODOO_PLUGIN_H
#define PLM_ODOO_PLUGIN_H

#include <plm/plm_activity_tracker.h>
#include <plm/plm_cache_manager.h>
#include <plm/plm_plugin.h>
#include <plm/plm_state_mapper.h>
#include <plm/plm_state_persistence.h>
#include <plm/plm_sync_state.h>
#include "odoo_rpc_client.h"

#include <map>
#include <memory>
#include <mutex>
#include <optional>

#include <wx/timer.h>


/**
 * OdooPLM plugin for KiCad.
 *
 * Implements the PLM_PLUGIN interface using OdooPLM as the backend.
 * Supports:
 * - JSON-RPC communication with Odoo server
 * - Session-based and API key authentication
 * - Product category navigation (maps to PLM folders)
 * - Product management (maps to PLM projects)
 * - Attachment handling for KiCad files
 * - OdooPLM document versioning
 *
 * Products in Odoo are mapped to KiCad projects, with attachments
 * serving as the storage for schematic and PCB files.
 */
class PLM_ODOO_PLUGIN : public PLM_PLUGIN
{
public:
    PLM_ODOO_PLUGIN();
    ~PLM_ODOO_PLUGIN() override;

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
    // Notification Interface
    //-------------------------------------------------------------------------

    bool SubscribeNotifications( const wxString&                aProjectId,
                                  PLM_NOTIFICATION_LISTENER*     aListener,
                                  const PLM_NOTIFICATION_CONFIG& aConfig =
                                          PLM_NOTIFICATION_CONFIG() ) override;
    bool UnsubscribeNotifications( const wxString& aProjectId ) override;
    void UnsubscribeAllNotifications() override;

    //-------------------------------------------------------------------------
    // ECO Interface
    //-------------------------------------------------------------------------

    std::vector<PLM_ECO>       GetActiveECOs() override;
    std::vector<PLM_ECO_FIELD> GetECORequiredFields( const wxString& aEcoId ) override;
    PLM_ECO_VALIDATION_RESULT  ValidateECOCheckin( const wxString&            aEcoId,
                                                    const PLM_ECO_FIELD_VALUES& aFields ) override;

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
    PLM_ECO_CREATE_RESULT                  ConvertECRToECO( const wxString& aEcrId ) override;

    //-------------------------------------------------------------------------
    // BOM Management Interface
    //-------------------------------------------------------------------------

    PLM_BOM                    GetBOM( const wxString& aProductId, PLM_BOM_TYPE aType ) override;
    std::vector<PLM_BOM>       GetBOMs( const wxString& aProductId ) override;
    PLM_BOM_RESULT             CreateBOM( const PLM_BOM& aBom ) override;
    PLM_BOM_RESULT             UpdateBOM( const PLM_BOM& aBom ) override;
    bool                       DeleteBOM( const wxString& aBomId ) override;
    PLM_BOM_DIFF_RESULT        CompareBOMs( const PLM_BOM&              aBomA,
                                             const PLM_BOM&              aBomB,
                                             const PLM_BOM_DIFF_OPTIONS& aOptions ) override;
    PLM_BOM_VALIDATION_RESULT  ValidateBOMEntries(
            const std::vector<PLM_BOM_ENTRY>& aEntries ) override;

    //-------------------------------------------------------------------------
    // Routing Interface
    //-------------------------------------------------------------------------

    PLM_ROUTING                 GetRouting( const wxString& aProductId ) override;
    std::vector<PLM_WORKCENTER> GetWorkcenters() override;
    PLM_ROUTING_RESULT          SaveRouting( const PLM_ROUTING& aRouting ) override;

    //-------------------------------------------------------------------------
    // Lifecycle Transition Interface
    //-------------------------------------------------------------------------

    std::vector<PLM_STATE_INFO>       GetStateMappings() override;
    std::vector<PLM_VALID_TRANSITION> GetValidTransitions( const wxString& aProjectId ) override;
    PLM_STATE_TRANSITION_RESULT       ChangeState( const wxString& aProjectId,
                                                    const wxString& aTargetState,
                                                    const wxString& aComment ) override;
    bool                              GetCurrentState( const wxString&  aProjectId,
                                                        PLM_STATE_INFO& aStateInfo ) override;

    //-------------------------------------------------------------------------
    // Review Interface
    //-------------------------------------------------------------------------

    PLM_REVIEW_RESULT              SubmitForReview( const wxString&                  aProjectId,
                                                     const PLM_REVIEW_SUBMIT_OPTIONS& aOptions ) override;
    std::vector<PLM_REVIEW>        GetReviews( const wxString& aProjectId ) override;
    PLM_REVIEW                     GetReview( const wxString& aReviewId ) override;
    std::vector<PLM_REVIEW_COMMENT> GetReviewComments( const wxString& aReviewId ) override;
    bool                           AddReviewComment( const PLM_REVIEW_COMMENT& aComment ) override;
    bool                           ResolveReviewComment( const wxString& aCommentId,
                                                          bool            aResolved ) override;
    PLM_REVIEW_RESULT              SubmitReviewDecision( const wxString&     aReviewId,
                                                          PLM_REVIEW_DECISION aDecision,
                                                          const wxString&     aComment ) override;
    PLM_REVIEW_RESULT              MergeReview( const wxString& aReviewId,
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
    // Release Interface
    //-------------------------------------------------------------------------

    std::vector<PLM_RELEASE_CHECK> ValidateForRelease( const wxString& aProjectId ) override;
    PLM_RELEASE_RESULT             CreateRelease( const wxString&            aProjectId,
                                                   const PLM_RELEASE_OPTIONS& aOptions ) override;
    PLM_RELEASE_RESULT             FinalizeRelease( const wxString& aReleaseId ) override;
    std::vector<PLM_RELEASE>       ListReleases( const wxString& aProjectId ) override;
    PLM_RELEASE                    GetRelease( const wxString& aReleaseId ) override;
    wxString                       SuggestNextVersion( const wxString&    aProjectId,
                                                        PLM_VERSION_SCHEME aScheme ) override;

    //-------------------------------------------------------------------------
    // Deviation Branch Interface
    //-------------------------------------------------------------------------

    PLM_DEVIATION_BRANCH              CreateDeviationBranch( const wxString& aProjectId,
                                                              const wxString& aBaseRevision,
                                                              const wxString& aReason ) override;
    std::vector<PLM_DEVIATION_BRANCH> ListDeviationBranches( const wxString& aProjectId ) override;

    //-------------------------------------------------------------------------
    // Variant Interface
    //-------------------------------------------------------------------------

    PLM_VARIANT        GetVariants( const wxString& aProjectId ) override;
    PLM_VARIANT_RESULT CreateVariant( const wxString&           aProjectId,
                                       const PLM_VARIANT_CONFIG& aConfig ) override;
    PLM_VARIANT_RESULT UpdateVariant( const PLM_VARIANT_CONFIG& aConfig ) override;
    bool               DeleteVariant( const wxString& aConfigId ) override;

    //-------------------------------------------------------------------------
    // CI/CD Pipeline Interface
    //-------------------------------------------------------------------------

    std::vector<PLM_PIPELINE_RUN> GetPipelineRuns( const wxString&            aProjectId,
                                                    const PLM_PIPELINE_FILTER& aFilter ) override;
    PLM_PIPELINE_RUN              GetPipelineRun( const wxString& aProjectId,
                                                   const wxString& aRunId ) override;
    bool                          DownloadCIArtifact( const wxString& aProjectId,
                                                       const wxString& aRunId,
                                                       const wxString& aArtifactId,
                                                       const wxString& aLocalPath ) override;
    PLM_CI_RUN_STATUS             GetCommitPipelineStatus( const wxString& aProjectId,
                                                            const wxString& aCommitSha ) override;

    //-------------------------------------------------------------------------
    // Audit Trail Interface
    //-------------------------------------------------------------------------

    PLM_AUDIT_QUERY_RESULT GetAuditLog( const PLM_AUDIT_FILTER& aFilter ) override;
    PLM_AUDIT_QUERY_RESULT GetProjectAuditLog( const wxString& aProjectId,
                                                size_t          aLimit ) override;

    //-------------------------------------------------------------------------
    // File Operations Extended Interface
    //-------------------------------------------------------------------------

    PLM_CHECKOUT_RESULT  CreateFolder( const wxString& aParentFolderId,
                                        const wxString& aName ) override;
    std::vector<wxString> GetModifiedFiles( const wxString& aLocalPath ) override;

    //-------------------------------------------------------------------------
    // Preview Cache Interface
    //-------------------------------------------------------------------------

    PLM_PREVIEW_CACHE* GetPreviewCache() override;

private:
    /**
     * Query the authenticated user's Odoo group memberships and store
     * the group names in m_session.user_info.groups.
     */
    void QueryUserGroups();

    /**
     * Parse server URL and extract database name if present.
     * Odoo URLs may include database in path: https://server.com/database
     */
    void ParseServerUrl( const wxString& aServerUrl );

    /**
     * Populate a PLM_FILE_INFO from an Odoo product JSON object.
     */
    void PopulateFileInfoFromProduct( PLM_FILE_INFO& aFile, const nlohmann::json& aProduct );

    /**
     * Download all attachments for a product to a local directory.
     * @param aProductId Odoo product ID.
     * @param aLocalPath Local directory path to download files to.
     * @param aWarnings  Optional output for per-file failure warnings.
     * @return True if at least one attachment downloaded. False if ALL failed.
     */
    bool DownloadProductAttachments( int aProductId, const wxString& aLocalPath,
                                     wxString* aWarnings = nullptr );

    /**
     * Upload all KiCad project files from a directory as attachments.
     * @param aProductId Odoo product ID to attach files to.
     * @param aLocalPath Local directory path to upload files from.
     * @param aWarnings  Optional output for per-file failure warnings.
     * @return True if at least one file uploaded. False if ALL failed.
     */
    bool UploadProjectFiles( int aProductId, const wxString& aLocalPath,
                              wxString* aWarnings = nullptr );

    /**
     * Decode base64 string to binary data.
     */
    static wxMemoryBuffer DecodeBase64( const wxString& aBase64 );

    /**
     * Encode binary data to base64 string.
     */
    static wxString EncodeBase64( const wxMemoryBuffer& aData );

    /**
     * Write checkout metadata file to track checkout state.
     * @param aLocalPath Local checkout path.
     * @param aProjectId Odoo product ID.
     * @param aRevision Checkout revision.
     * @return True if metadata written successfully.
     */
    bool WriteCheckoutMetadata( const wxString& aLocalPath, const wxString& aProjectId,
                                 const wxString& aRevision );

    /**
     * Read checkout metadata from a local path.
     * @param aLocalPath Local checkout path.
     * @param aProjectId Output: Product ID.
     * @param aRevision Output: Checkout revision.
     * @return True if metadata read successfully.
     */
    bool ReadCheckoutMetadata( const wxString& aLocalPath, wxString& aProjectId,
                                wxString& aRevision );

    /**
     * Build an Odoo web interface URL for a given model record.
     * @param aModel  Odoo model name (e.g., "product.product", "mrp.bom").
     * @param aRecordId  Numeric record ID.
     * @return URL string like "https://server/web#id=123&model=product.product&view_type=form"
     */
    wxString GetOdooWebUrl( const wxString& aModel, int aRecordId ) const;

    std::unique_ptr<OdooRpcClient> m_rpcClient;        ///< JSON-RPC client
    wxString                       m_serverUrl;        ///< Odoo server URL
    wxString                       m_database;         ///< Odoo database name
    wxString                       m_sessionId;        ///< Current session ID
    int                            m_userId;           ///< Authenticated user ID
    wxString                       m_username;         ///< Authenticated username
    bool                           m_isAuthenticated;  ///< Authentication state
    PLM_SESSION                    m_session;          ///< Current session info

    std::map<wxString, wxString> m_userDisplayNames;   ///< Cached user names
    std::map<int, wxString>      m_categoryCache;      ///< Cached category names

    std::optional<bool> m_auditTrailProbe; ///< mail.message readability probe; empty until tested

    /// mrp.eco.type model existence probe; true on Odoo Enterprise (mrp_plm)
    /// where the live ECO type catalogue can be queried, false on community
    /// OdooPLM where the static fallback list is used. Empty until tested.
    std::optional<bool> m_ecoTypeProbe;

    /// OdooPLM (engineering_state on product.product) availability probe.
    /// True when the OmniaSolutions OdooPLM module is installed and the
    /// session can read the engineering_* revision fields. False on stock
    /// Odoo or insufficient ACL. Drives supports_revisions and the revision
    /// methods that walk engineering_branch_parent_id. Empty until tested.
    std::optional<bool> m_odooPlmProbe;

    // Notification polling state
    struct NotificationSubscription
    {
        PLM_NOTIFICATION_LISTENER* listener = nullptr;
        PLM_NOTIFICATION_CONFIG    config;
        wxString                   last_write_date;
        bool                       last_lock_state = false;
    };

    std::map<wxString, NotificationSubscription> m_subscriptions; ///< Active subscriptions
    std::unique_ptr<wxTimer>                     m_pollTimer;     ///< Polling timer
    static constexpr int                         POLL_INTERVAL_MS = 30000; ///< 30 second poll interval

    void OnPollTimer( wxTimerEvent& aEvent );
    void PollForChanges();

    mutable std::recursive_mutex m_mutex;              ///< Protects access to all mutable state
};


#endif // PLM_ODOO_PLUGIN_H
