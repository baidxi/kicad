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
 * @file plm_forge.h
 * @brief Abstract interface for Git forge operations (GitHub, GitLab, Bitbucket).
 *
 * This interface abstracts REST API operations on Git hosting platforms.
 * It is implemented by the Git plugin and consumed by review, release,
 * and CI/CD workflows. Non-Git plugins do not implement this interface.
 */

#ifndef PLM_FORGE_H
#define PLM_FORGE_H

#include "plm_review.h"

#include <wx/string.h>
#include <cstdint>
#include <vector>


/**
 * Detected forge platform type.
 */
enum class PLM_FORGE_TYPE
{
    UNKNOWN = 0,
    GITHUB = 1,
    GITLAB = 2,
    GITEA = 3,
    BITBUCKET = 4
};


/**
 * A release on the forge platform.
 */
struct PLM_FORGE_RELEASE
{
    wxString    release_id;     ///< Platform release ID
    wxString    tag_name;       ///< Git tag for this release
    wxString    title;          ///< Release title
    wxString    body;           ///< Release description (markdown)
    wxString    web_url;        ///< URL to release page
    bool        draft = false;
    bool        prerelease = false;
    int64_t     created_at = 0;

    PLM_FORGE_RELEASE() = default;
};


/**
 * An artifact or asset attached to a release or pipeline run.
 */
struct PLM_FORGE_ASSET
{
    wxString    asset_id;       ///< Platform asset ID
    wxString    name;           ///< File name
    wxString    content_type;   ///< MIME type
    size_t      size_bytes = 0; ///< File size
    wxString    download_url;   ///< Direct download URL

    PLM_FORGE_ASSET() = default;
};


/**
 * Status of a CI/CD pipeline run or check at the forge level.
 */
enum class PLM_PIPELINE_STATE
{
    UNKNOWN = 0,
    QUEUED = 1,
    RUNNING = 2,
    PASSED = 3,
    FAILED = 4,
    CANCELLED = 5,
    SKIPPED = 6
};


/**
 * A single CI/CD check or job result.
 */
struct PLM_PIPELINE_CHECK
{
    wxString            name;
    PLM_PIPELINE_STATE  state = PLM_PIPELINE_STATE::UNKNOWN;
    wxString            details_url;
    wxString            summary;
    int64_t             started_at = 0;
    int64_t             completed_at = 0;

    PLM_PIPELINE_CHECK() = default;
};


/**
 * Aggregate status of all CI/CD checks for a commit or PR.
 */
struct PLM_PIPELINE_STATUS
{
    wxString                        commit_sha;
    PLM_PIPELINE_STATE              overall_state = PLM_PIPELINE_STATE::UNKNOWN;
    std::vector<PLM_PIPELINE_CHECK> checks;
    wxString                        web_url;

    PLM_PIPELINE_STATUS() = default;

    bool AllPassed() const { return overall_state == PLM_PIPELINE_STATE::PASSED; }
    bool AnyFailed() const { return overall_state == PLM_PIPELINE_STATE::FAILED; }

    bool IsRunning() const
    {
        return overall_state == PLM_PIPELINE_STATE::RUNNING ||
               overall_state == PLM_PIPELINE_STATE::QUEUED;
    }
};


/**
 * Abstract interface for Git forge operations.
 *
 * Implemented by the Git plugin to provide GitHub/GitLab/Bitbucket
 * API access. Consumed by review, release, and CI/CD workflows.
 * Obtain via PLM_PLUGIN::GetForgeProvider(), which returns nullptr
 * for non-Git plugins.
 */
class PLM_FORGE_PROVIDER
{
public:
    virtual ~PLM_FORGE_PROVIDER() = default;

    virtual PLM_FORGE_TYPE GetForgeType() const = 0;
    virtual wxString GetForgeName() const = 0;

    // Pull Request / Merge Request operations
    virtual PLM_REVIEW_RESULT CreatePullRequest(
            const wxString& aSourceBranch,
            const wxString& aTargetBranch,
            const wxString& aTitle,
            const wxString& aBody,
            const std::vector<wxString>& aReviewerIds,
            bool aDraft = false ) = 0;

    virtual std::vector<PLM_REVIEW> GetPullRequests(
            const wxString& aTargetBranch = wxEmptyString ) = 0;

    virtual PLM_REVIEW GetPullRequest( const wxString& aPrId ) = 0;

    virtual std::vector<PLM_REVIEW_COMMENT> GetPullRequestComments(
            const wxString& aPrId ) = 0;

    virtual bool AddPullRequestComment(
            const wxString& aPrId,
            const PLM_REVIEW_COMMENT& aComment ) = 0;

    virtual PLM_REVIEW_RESULT SubmitPullRequestReview(
            const wxString& aPrId,
            PLM_REVIEW_DECISION aDecision,
            const wxString& aBody = wxEmptyString ) = 0;

    virtual PLM_REVIEW_RESULT MergePullRequest(
            const wxString& aPrId,
            const wxString& aCommitMessage = wxEmptyString ) = 0;

    // Release operations
    virtual PLM_FORGE_RELEASE CreateRelease(
            const wxString& aTagName,
            const wxString& aTitle,
            const wxString& aBody,
            bool aDraft = false,
            bool aPrerelease = false ) = 0;

    virtual std::vector<PLM_FORGE_RELEASE> GetReleases( size_t aLimit = 20 ) = 0;

    virtual bool UploadReleaseAsset(
            const wxString& aReleaseId,
            const wxString& aFilePath,
            const wxString& aContentType = wxEmptyString ) = 0;

    virtual std::vector<PLM_FORGE_ASSET> GetReleaseAssets(
            const wxString& aReleaseId ) = 0;

    // CI/CD pipeline operations
    virtual PLM_PIPELINE_STATUS GetPipelineStatus( const wxString& aCommitSha ) = 0;

    virtual PLM_PIPELINE_STATUS GetPRPipelineStatus( const wxString& aPrId ) = 0;

    virtual std::vector<PLM_FORGE_ASSET> GetPipelineArtifacts(
            const wxString& aCommitSha ) = 0;

    virtual bool DownloadArtifact(
            const wxString& aArtifactId,
            const wxString& aLocalPath ) = 0;

protected:
    PLM_FORGE_PROVIDER() = default;
};


#endif // PLM_FORGE_H
