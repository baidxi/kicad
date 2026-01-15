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
 * @file plm_git_forge.h
 * @brief Concrete forge provider for GitHub, GitLab, and Gitea REST APIs.
 *
 * Implements the PLM_FORGE_PROVIDER interface using KICAD_CURL_EASY
 * for HTTP communication and nlohmann::json for response parsing.
 * Auto-detects the forge platform from the repository URL.
 */

#ifndef PLM_GIT_FORGE_H
#define PLM_GIT_FORGE_H

#include <plm/plm_forge.h>
#include "plm_git_auth.h"

#include <nlohmann/json.hpp>


class PLM_GIT_FORGE_PROVIDER : public PLM_FORGE_PROVIDER
{
public:
    PLM_GIT_FORGE_PROVIDER( const wxString& aRepoUrl, GIT_AUTH_HELPER* aAuthHelper,
                            GIT_AUTH_CONFIG* aAuthConfig );

    PLM_FORGE_TYPE GetForgeType() const override;
    wxString       GetForgeName() const override;

    // Pull Request / Merge Request operations
    PLM_REVIEW_RESULT CreatePullRequest( const wxString& aSourceBranch,
                                         const wxString& aTargetBranch, const wxString& aTitle,
                                         const wxString& aBody,
                                         const std::vector<wxString>& aReviewerIds,
                                         bool aDraft = false ) override;

    std::vector<PLM_REVIEW> GetPullRequests( const wxString& aTargetBranch = wxEmptyString ) override;

    PLM_REVIEW GetPullRequest( const wxString& aPrId ) override;

    std::vector<PLM_REVIEW_COMMENT> GetPullRequestComments( const wxString& aPrId ) override;

    bool AddPullRequestComment( const wxString& aPrId,
                                const PLM_REVIEW_COMMENT& aComment ) override;

    PLM_REVIEW_RESULT SubmitPullRequestReview( const wxString& aPrId,
                                               PLM_REVIEW_DECISION aDecision,
                                               const wxString& aBody = wxEmptyString ) override;

    PLM_REVIEW_RESULT MergePullRequest( const wxString& aPrId,
                                        const wxString& aCommitMessage = wxEmptyString ) override;

    // Release operations
    PLM_FORGE_RELEASE CreateRelease( const wxString& aTagName, const wxString& aTitle,
                                     const wxString& aBody, bool aDraft = false,
                                     bool aPrerelease = false ) override;

    std::vector<PLM_FORGE_RELEASE> GetReleases( size_t aLimit = 20 ) override;

    bool UploadReleaseAsset( const wxString& aReleaseId, const wxString& aFilePath,
                             const wxString& aContentType = wxEmptyString ) override;

    std::vector<PLM_FORGE_ASSET> GetReleaseAssets( const wxString& aReleaseId ) override;

    // CI/CD pipeline operations
    PLM_PIPELINE_STATUS GetPipelineStatus( const wxString& aCommitSha ) override;
    PLM_PIPELINE_STATUS GetPRPipelineStatus( const wxString& aPrId ) override;

    std::vector<PLM_FORGE_ASSET> GetPipelineArtifacts( const wxString& aCommitSha ) override;

    bool DownloadArtifact( const wxString& aArtifactId, const wxString& aLocalPath ) override;

    /**
     * Detect forge type from a repository URL without making any network calls.
     */
    static PLM_FORGE_TYPE DetectForgeType( const wxString& aRepoUrl );

private:
    /**
     * HTTP helpers that set appropriate auth headers and parse JSON responses.
     * All return false on transport or HTTP-level errors.
     */
    bool HttpGet( const wxString& aUrl, nlohmann::json& aResponse );
    bool HttpPost( const wxString& aUrl, const nlohmann::json& aBody, nlohmann::json& aResponse );
    bool HttpPatch( const wxString& aUrl, const nlohmann::json& aBody, nlohmann::json& aResponse );
    bool HttpPut( const wxString& aUrl, const nlohmann::json& aBody, nlohmann::json& aResponse );
    bool HttpDelete( const wxString& aUrl );

    /**
     * Low-level HTTP request with custom method.
     * Returns the HTTP status code, or 0 on transport failure.
     */
    int HttpRequest( const std::string& aMethod, const std::string& aUrl,
                     const std::string& aBody, std::string& aResponseBody );

    wxString BuildApiUrl( const wxString& aPath ) const;

    void ParseRepoUrl();

    // JSON-to-struct mapping helpers
    PLM_REVIEW         ParsePullRequest( const nlohmann::json& aJson ) const;
    PLM_REVIEW_COMMENT ParseComment( const nlohmann::json& aJson ) const;
    PLM_FORGE_RELEASE  ParseRelease( const nlohmann::json& aJson ) const;
    PLM_FORGE_ASSET    ParseAsset( const nlohmann::json& aJson ) const;
    PLM_PIPELINE_CHECK ParseCheck( const nlohmann::json& aJson ) const;

    PLM_REVIEW_STATUS MapPrState( const std::string& aState, bool aMerged ) const;

    PLM_FORGE_TYPE   m_forgeType;
    wxString         m_repoUrl;
    wxString         m_owner;
    wxString         m_repo;
    wxString         m_apiBase;
    GIT_AUTH_HELPER* m_authHelper;
    GIT_AUTH_CONFIG* m_authConfig;
    int              m_lastHttpStatus = 0;
};


#endif // PLM_GIT_FORGE_H
