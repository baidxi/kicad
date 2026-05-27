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

#include "plm_git_forge.h"

#include <kicad_curl/kicad_curl_easy.h>
#include <curl/curl.h>
#include <wx/log.h>
#include <wx/uri.h>
#include <wx/filename.h>
#include <wx/file.h>
#include <wx/ffile.h>

#include <fstream>


static const char* const traceForge = "KICAD_PLM_FORGE";

static std::string JsonSafe( const nlohmann::json& aJson, const std::string& aKey )
{
    if( aJson.contains( aKey ) && aJson[aKey].is_string() )
        return aJson[aKey].get<std::string>();

    return {};
}


static int64_t JsonInt( const nlohmann::json& aJson, const std::string& aKey )
{
    if( aJson.contains( aKey ) && aJson[aKey].is_number() )
        return aJson[aKey].get<int64_t>();

    return 0;
}


static bool JsonBool( const nlohmann::json& aJson, const std::string& aKey )
{
    if( aJson.contains( aKey ) && aJson[aKey].is_boolean() )
        return aJson[aKey].get<bool>();

    return false;
}


PLM_GIT_FORGE_PROVIDER::PLM_GIT_FORGE_PROVIDER( const wxString& aRepoUrl,
                                                  GIT_AUTH_HELPER* aAuthHelper,
                                                  GIT_AUTH_CONFIG* aAuthConfig ) :
        m_forgeType( PLM_FORGE_TYPE::UNKNOWN ),
        m_repoUrl( aRepoUrl ),
        m_authHelper( aAuthHelper ),
        m_authConfig( aAuthConfig )
{
    ParseRepoUrl();
}


PLM_FORGE_TYPE PLM_GIT_FORGE_PROVIDER::GetForgeType() const
{
    return m_forgeType;
}


wxString PLM_GIT_FORGE_PROVIDER::GetForgeName() const
{
    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:    return wxS( "GitHub" );
    case PLM_FORGE_TYPE::GITLAB:    return wxS( "GitLab" );
    case PLM_FORGE_TYPE::GITEA:     return wxS( "Gitea" );
    case PLM_FORGE_TYPE::BITBUCKET: return wxS( "Bitbucket" );
    default:                        return wxS( "Unknown" );
    }
}


PLM_FORGE_TYPE PLM_GIT_FORGE_PROVIDER::DetectForgeType( const wxString& aRepoUrl )
{
    wxURI uri( aRepoUrl );
    wxString host = uri.GetServer().Lower();

    // Handle git@host:owner/repo SSH URLs
    if( host.IsEmpty() && aRepoUrl.StartsWith( wxS( "git@" ) ) )
    {
        wxString remainder = aRepoUrl.Mid( 4 );
        int colonPos = remainder.Find( ':' );

        if( colonPos != wxNOT_FOUND )
            host = remainder.Left( colonPos ).Lower();
    }

    if( host.Contains( wxS( "github.com" ) ) )
        return PLM_FORGE_TYPE::GITHUB;

    if( host.Contains( wxS( "gitlab.com" ) ) || host.Contains( wxS( "gitlab" ) ) )
        return PLM_FORGE_TYPE::GITLAB;

    if( host.Contains( wxS( "gitea" ) ) || host.Contains( wxS( "codeberg.org" ) ) )
        return PLM_FORGE_TYPE::GITEA;

    if( host.Contains( wxS( "bitbucket.org" ) ) )
        return PLM_FORGE_TYPE::BITBUCKET;

    return PLM_FORGE_TYPE::UNKNOWN;
}


void PLM_GIT_FORGE_PROVIDER::ParseRepoUrl()
{
    m_forgeType = DetectForgeType( m_repoUrl );

    wxString pathPart;
    wxURI uri( m_repoUrl );
    wxString host = uri.GetServer();

    if( host.IsEmpty() && m_repoUrl.StartsWith( wxS( "git@" ) ) )
    {
        // SSH URL: git@github.com:owner/repo.git
        wxString remainder = m_repoUrl.Mid( 4 );
        int colonPos = remainder.Find( ':' );

        if( colonPos != wxNOT_FOUND )
        {
            host = remainder.Left( colonPos );
            pathPart = remainder.Mid( colonPos + 1 );
        }
    }
    else
    {
        pathPart = uri.GetPath();

        // Remove leading slash
        if( pathPart.StartsWith( wxS( "/" ) ) )
            pathPart = pathPart.Mid( 1 );
    }

    // Strip .git suffix
    if( pathPart.EndsWith( wxS( ".git" ) ) )
        pathPart = pathPart.Left( pathPart.length() - 4 );

    // Split owner/repo
    int slashPos = pathPart.Find( '/' );

    if( slashPos != wxNOT_FOUND )
    {
        m_owner = pathPart.Left( slashPos );
        m_repo = pathPart.Mid( slashPos + 1 );

        // GitLab can have nested groups; take only the last component as repo
        int lastSlash = m_repo.Find( '/', true );

        if( lastSlash != wxNOT_FOUND )
        {
            m_owner = pathPart.Left( pathPart.Find( '/', true ) );
            m_repo = pathPart.Mid( pathPart.Find( '/', true ) + 1 );
        }
    }

    // Build API base URL
    wxString scheme = uri.GetScheme();

    if( scheme.IsEmpty() )
        scheme = wxS( "https" );

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
        m_apiBase = wxS( "https://api.github.com" );
        break;

    case PLM_FORGE_TYPE::GITLAB:
        if( host.Contains( wxS( "gitlab.com" ) ) )
            m_apiBase = wxS( "https://gitlab.com/api/v4" );
        else
            m_apiBase = scheme + wxS( "://" ) + host + wxS( "/api/v4" );
        break;

    case PLM_FORGE_TYPE::GITEA:
        m_apiBase = scheme + wxS( "://" ) + host + wxS( "/api/v1" );
        break;

    case PLM_FORGE_TYPE::BITBUCKET:
        m_apiBase = wxS( "https://api.bitbucket.org/2.0" );
        break;

    default:
        // Fall back to GitHub-style API on the host
        m_apiBase = scheme + wxS( "://" ) + host + wxS( "/api/v1" );
        break;
    }

    wxLogTrace( traceForge, wxS( "Forge detected: %s, owner=%s, repo=%s, api=%s" ),
                GetForgeName(), m_owner, m_repo, m_apiBase );
}


wxString PLM_GIT_FORGE_PROVIDER::BuildApiUrl( const wxString& aPath ) const
{
    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
        return m_apiBase + wxS( "/repos/" ) + m_owner + wxS( "/" ) + m_repo + aPath;

    case PLM_FORGE_TYPE::GITLAB:
    {
        // GitLab uses URL-encoded project path
        wxString projectPath = m_owner + wxS( "%2F" ) + m_repo;
        return m_apiBase + wxS( "/projects/" ) + projectPath + aPath;
    }

    case PLM_FORGE_TYPE::GITEA:
        return m_apiBase + wxS( "/repos/" ) + m_owner + wxS( "/" ) + m_repo + aPath;

    case PLM_FORGE_TYPE::BITBUCKET:
        return m_apiBase + wxS( "/repositories/" ) + m_owner + wxS( "/" ) + m_repo + aPath;

    default:
        return m_apiBase + wxS( "/repos/" ) + m_owner + wxS( "/" ) + m_repo + aPath;
    }
}


int PLM_GIT_FORGE_PROVIDER::HttpRequest( const std::string& aMethod, const std::string& aUrl,
                                          const std::string& aBody, std::string& aResponseBody )
{
    try
    {
        KICAD_CURL_EASY curl;

        curl.SetURL( aUrl );
        curl.SetHeader( "Accept", "application/json" );
        curl.SetHeader( "Content-Type", "application/json" );
        curl.SetFollowRedirects( true );

        // Set auth header based on forge type
        if( m_authConfig )
        {
            std::string token = m_authConfig->password.ToStdString();

            if( !token.empty() )
            {
                switch( m_forgeType )
                {
                case PLM_FORGE_TYPE::GITLAB:
                    curl.SetHeader( "PRIVATE-TOKEN", token );
                    break;

                case PLM_FORGE_TYPE::GITEA:
                    curl.SetHeader( "Authorization", "token " + token );
                    break;

                case PLM_FORGE_TYPE::GITHUB:
                default:
                    curl.SetHeader( "Authorization", "Bearer " + token );
                    break;
                }
            }
        }

        // Set HTTP method
        if( aMethod == "POST" )
        {
            curl.SetPostFields( aBody.empty() ? "{}" : aBody );
        }
        else if( aMethod == "PUT" || aMethod == "PATCH" || aMethod == "DELETE" )
        {
            curl_easy_setopt( curl.GetCurl(), CURLOPT_CUSTOMREQUEST, aMethod.c_str() );

            if( !aBody.empty() )
                curl.SetPostFields( aBody );
        }
        // GET is the default

        wxLogTrace( traceForge, wxS( "%s %s" ), aMethod, aUrl );

        int curlResult = curl.Perform();

        if( curlResult != 0 )
        {
            wxLogTrace( traceForge, wxS( "CURL error %d: %s" ), curlResult,
                        curl.GetErrorText( curlResult ) );
            return 0;
        }

        m_lastHttpStatus = curl.GetResponseStatusCode();
        aResponseBody = curl.GetBuffer();

        wxLogTrace( traceForge, wxS( "HTTP %d, %zu bytes" ), m_lastHttpStatus,
                    aResponseBody.size() );

        return m_lastHttpStatus;
    }
    catch( const std::exception& e )
    {
        wxLogTrace( traceForge, wxS( "HTTP exception: %s" ), e.what() );
        return 0;
    }
}


bool PLM_GIT_FORGE_PROVIDER::HttpGet( const wxString& aUrl, nlohmann::json& aResponse )
{
    std::string responseBody;
    int status = HttpRequest( "GET", aUrl.ToStdString(), {}, responseBody );

    if( status < 200 || status >= 300 )
        return false;

    try
    {
        aResponse = nlohmann::json::parse( responseBody );
        return true;
    }
    catch( const nlohmann::json::parse_error& e )
    {
        wxLogTrace( traceForge, wxS( "JSON parse error: %s" ), e.what() );
        return false;
    }
}


bool PLM_GIT_FORGE_PROVIDER::HttpPost( const wxString& aUrl, const nlohmann::json& aBody,
                                        nlohmann::json& aResponse )
{
    std::string responseBody;
    int status = HttpRequest( "POST", aUrl.ToStdString(), aBody.dump(), responseBody );

    if( status < 200 || status >= 300 )
        return false;

    try
    {
        if( !responseBody.empty() )
            aResponse = nlohmann::json::parse( responseBody );

        return true;
    }
    catch( const nlohmann::json::parse_error& e )
    {
        wxLogTrace( traceForge, wxS( "JSON parse error: %s" ), e.what() );
        return false;
    }
}


bool PLM_GIT_FORGE_PROVIDER::HttpPatch( const wxString& aUrl, const nlohmann::json& aBody,
                                         nlohmann::json& aResponse )
{
    std::string responseBody;
    int status = HttpRequest( "PATCH", aUrl.ToStdString(), aBody.dump(), responseBody );

    if( status < 200 || status >= 300 )
        return false;

    try
    {
        if( !responseBody.empty() )
            aResponse = nlohmann::json::parse( responseBody );

        return true;
    }
    catch( const nlohmann::json::parse_error& e )
    {
        wxLogTrace( traceForge, wxS( "JSON parse error: %s" ), e.what() );
        return false;
    }
}


bool PLM_GIT_FORGE_PROVIDER::HttpPut( const wxString& aUrl, const nlohmann::json& aBody,
                                       nlohmann::json& aResponse )
{
    std::string responseBody;
    int status = HttpRequest( "PUT", aUrl.ToStdString(), aBody.dump(), responseBody );

    if( status < 200 || status >= 300 )
        return false;

    try
    {
        if( !responseBody.empty() )
            aResponse = nlohmann::json::parse( responseBody );

        return true;
    }
    catch( const nlohmann::json::parse_error& e )
    {
        wxLogTrace( traceForge, wxS( "JSON parse error: %s" ), e.what() );
        return false;
    }
}


bool PLM_GIT_FORGE_PROVIDER::HttpDelete( const wxString& aUrl )
{
    std::string responseBody;
    int status = HttpRequest( "DELETE", aUrl.ToStdString(), {}, responseBody );
    return status >= 200 && status < 300;
}


// ---------------------------------------------------------------------------
// JSON-to-struct mapping
// ---------------------------------------------------------------------------

PLM_REVIEW_STATUS PLM_GIT_FORGE_PROVIDER::MapPrState( const std::string& aState,
                                                       bool aMerged ) const
{
    if( aMerged )
        return PLM_REVIEW_STATUS::MERGED;

    if( aState == "open" )
        return PLM_REVIEW_STATUS::OPEN;

    if( aState == "closed" )
        return PLM_REVIEW_STATUS::CLOSED;

    // GitLab-specific states
    if( aState == "merged" )
        return PLM_REVIEW_STATUS::MERGED;

    if( aState == "locked" )
        return PLM_REVIEW_STATUS::CLOSED;

    return PLM_REVIEW_STATUS::DRAFT;
}


PLM_REVIEW PLM_GIT_FORGE_PROVIDER::ParsePullRequest( const nlohmann::json& aJson ) const
{
    PLM_REVIEW review;

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
    {
        review.review_id = wxString::FromUTF8( std::to_string( JsonInt( aJson, "number" ) ) );
        review.title = wxString::FromUTF8( JsonSafe( aJson, "title" ) );
        review.description = wxString::FromUTF8( JsonSafe( aJson, "body" ) );
        review.web_url = wxString::FromUTF8( JsonSafe( aJson, "html_url" ) );

        bool merged = JsonBool( aJson, "merged" );
        bool draft = JsonBool( aJson, "draft" );

        review.status = MapPrState( JsonSafe( aJson, "state" ), merged );

        if( draft && review.status == PLM_REVIEW_STATUS::OPEN )
            review.status = PLM_REVIEW_STATUS::DRAFT;

        if( aJson.contains( "user" ) && aJson["user"].is_object() )
        {
            review.submitter_id = wxString::FromUTF8( JsonSafe( aJson["user"], "login" ) );
            review.submitter_name = review.submitter_id;
        }

        if( aJson.contains( "head" ) && aJson["head"].is_object() )
            review.source_branch = wxString::FromUTF8( JsonSafe( aJson["head"], "ref" ) );

        if( aJson.contains( "base" ) && aJson["base"].is_object() )
            review.target_branch = wxString::FromUTF8( JsonSafe( aJson["base"], "ref" ) );

        if( aJson.contains( "requested_reviewers" ) && aJson["requested_reviewers"].is_array() )
        {
            for( const auto& r : aJson["requested_reviewers"] )
            {
                wxString login = wxString::FromUTF8( JsonSafe( r, "login" ) );
                review.reviewers.emplace_back( login, login, PLM_REVIEW_ROLE::REVIEWER );
            }
        }

        break;
    }

    case PLM_FORGE_TYPE::GITLAB:
    {
        review.review_id = wxString::FromUTF8( std::to_string( JsonInt( aJson, "iid" ) ) );
        review.title = wxString::FromUTF8( JsonSafe( aJson, "title" ) );
        review.description = wxString::FromUTF8( JsonSafe( aJson, "description" ) );
        review.web_url = wxString::FromUTF8( JsonSafe( aJson, "web_url" ) );
        review.source_branch = wxString::FromUTF8( JsonSafe( aJson, "source_branch" ) );
        review.target_branch = wxString::FromUTF8( JsonSafe( aJson, "target_branch" ) );

        review.status = MapPrState( JsonSafe( aJson, "state" ), false );

        if( aJson.contains( "author" ) && aJson["author"].is_object() )
        {
            review.submitter_id = wxString::FromUTF8( JsonSafe( aJson["author"], "username" ) );
            review.submitter_name = wxString::FromUTF8( JsonSafe( aJson["author"], "name" ) );
        }

        if( aJson.contains( "reviewers" ) && aJson["reviewers"].is_array() )
        {
            for( const auto& r : aJson["reviewers"] )
            {
                wxString username = wxString::FromUTF8( JsonSafe( r, "username" ) );
                wxString name = wxString::FromUTF8( JsonSafe( r, "name" ) );
                review.reviewers.emplace_back( username, name, PLM_REVIEW_ROLE::REVIEWER );
            }
        }

        break;
    }

    case PLM_FORGE_TYPE::GITEA:
    {
        review.review_id = wxString::FromUTF8( std::to_string( JsonInt( aJson, "number" ) ) );
        review.title = wxString::FromUTF8( JsonSafe( aJson, "title" ) );
        review.description = wxString::FromUTF8( JsonSafe( aJson, "body" ) );
        review.web_url = wxString::FromUTF8( JsonSafe( aJson, "html_url" ) );

        if( aJson.contains( "head" ) && aJson["head"].is_object() )
            review.source_branch = wxString::FromUTF8( JsonSafe( aJson["head"], "ref" ) );

        if( aJson.contains( "base" ) && aJson["base"].is_object() )
            review.target_branch = wxString::FromUTF8( JsonSafe( aJson["base"], "ref" ) );

        bool merged = JsonBool( aJson, "merged" );
        review.status = MapPrState( JsonSafe( aJson, "state" ), merged );

        if( aJson.contains( "user" ) && aJson["user"].is_object() )
        {
            review.submitter_id = wxString::FromUTF8( JsonSafe( aJson["user"], "login" ) );
            review.submitter_name = review.submitter_id;
        }

        break;
    }

    default:
        break;
    }

    review.project_id = m_owner + wxS( "/" ) + m_repo;
    return review;
}


PLM_REVIEW_COMMENT PLM_GIT_FORGE_PROVIDER::ParseComment( const nlohmann::json& aJson ) const
{
    PLM_REVIEW_COMMENT comment;

    comment.comment_id = wxString::FromUTF8( std::to_string( JsonInt( aJson, "id" ) ) );
    comment.body = wxString::FromUTF8( JsonSafe( aJson, "body" ) );

    if( aJson.contains( "user" ) && aJson["user"].is_object() )
    {
        comment.author_id = wxString::FromUTF8( JsonSafe( aJson["user"], "login" ) );
        comment.author_name = comment.author_id;
    }
    else if( aJson.contains( "author" ) && aJson["author"].is_object() )
    {
        comment.author_id = wxString::FromUTF8( JsonSafe( aJson["author"], "username" ) );
        comment.author_name = wxString::FromUTF8( JsonSafe( aJson["author"], "name" ) );
    }

    return comment;
}


PLM_FORGE_RELEASE PLM_GIT_FORGE_PROVIDER::ParseRelease( const nlohmann::json& aJson ) const
{
    PLM_FORGE_RELEASE release;

    release.release_id = wxString::FromUTF8( std::to_string( JsonInt( aJson, "id" ) ) );
    release.tag_name = wxString::FromUTF8( JsonSafe( aJson, "tag_name" ) );
    release.draft = JsonBool( aJson, "draft" );
    release.prerelease = JsonBool( aJson, "prerelease" );

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
    case PLM_FORGE_TYPE::GITEA:
        release.title = wxString::FromUTF8( JsonSafe( aJson, "name" ) );
        release.body = wxString::FromUTF8( JsonSafe( aJson, "body" ) );
        release.web_url = wxString::FromUTF8( JsonSafe( aJson, "html_url" ) );
        break;

    case PLM_FORGE_TYPE::GITLAB:
        release.title = wxString::FromUTF8( JsonSafe( aJson, "name" ) );
        release.body = wxString::FromUTF8( JsonSafe( aJson, "description" ) );

        if( aJson.contains( "_links" ) && aJson["_links"].is_object() )
            release.web_url = wxString::FromUTF8( JsonSafe( aJson["_links"], "self" ) );

        break;

    default:
        break;
    }

    return release;
}


PLM_FORGE_ASSET PLM_GIT_FORGE_PROVIDER::ParseAsset( const nlohmann::json& aJson ) const
{
    PLM_FORGE_ASSET asset;

    asset.asset_id = wxString::FromUTF8( std::to_string( JsonInt( aJson, "id" ) ) );
    asset.name = wxString::FromUTF8( JsonSafe( aJson, "name" ) );
    asset.content_type = wxString::FromUTF8( JsonSafe( aJson, "content_type" ) );

    if( aJson.contains( "size" ) && aJson["size"].is_number() )
        asset.size_bytes = static_cast<size_t>( aJson["size"].get<int64_t>() );

    asset.download_url = wxString::FromUTF8(
            JsonSafe( aJson, "browser_download_url" ).empty()
                    ? JsonSafe( aJson, "url" )
                    : JsonSafe( aJson, "browser_download_url" ) );

    return asset;
}


PLM_PIPELINE_CHECK PLM_GIT_FORGE_PROVIDER::ParseCheck( const nlohmann::json& aJson ) const
{
    PLM_PIPELINE_CHECK check;

    check.name = wxString::FromUTF8( JsonSafe( aJson, "name" ) );

    if( check.name.IsEmpty() )
        check.name = wxString::FromUTF8( JsonSafe( aJson, "context" ) );

    check.details_url = wxString::FromUTF8( JsonSafe( aJson, "target_url" ) );

    if( check.details_url.IsEmpty() )
        check.details_url = wxString::FromUTF8( JsonSafe( aJson, "html_url" ) );

    std::string state = JsonSafe( aJson, "state" );

    if( state.empty() )
        state = JsonSafe( aJson, "status" );

    std::string conclusion = JsonSafe( aJson, "conclusion" );

    if( state == "success" || conclusion == "success" )
        check.state = PLM_PIPELINE_STATE::PASSED;
    else if( state == "failure" || state == "error" || conclusion == "failure" )
        check.state = PLM_PIPELINE_STATE::FAILED;
    else if( state == "pending" || state == "queued" )
        check.state = PLM_PIPELINE_STATE::QUEUED;
    else if( state == "in_progress" )
        check.state = PLM_PIPELINE_STATE::RUNNING;
    else if( conclusion == "cancelled" || conclusion == "timed_out" )
        check.state = PLM_PIPELINE_STATE::CANCELLED;
    else if( conclusion == "skipped" )
        check.state = PLM_PIPELINE_STATE::SKIPPED;

    return check;
}


// ---------------------------------------------------------------------------
// Pull Request operations
// ---------------------------------------------------------------------------

PLM_REVIEW_RESULT PLM_GIT_FORGE_PROVIDER::CreatePullRequest(
        const wxString& aSourceBranch, const wxString& aTargetBranch, const wxString& aTitle,
        const wxString& aBody, const std::vector<wxString>& aReviewerIds, bool aDraft )
{
    nlohmann::json body;
    nlohmann::json response;

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
    {
        body["title"] = aTitle.ToStdString();
        body["body"] = aBody.ToStdString();
        body["head"] = aSourceBranch.ToStdString();
        body["base"] = aTargetBranch.ToStdString();
        body["draft"] = aDraft;

        wxString url = BuildApiUrl( wxS( "/pulls" ) );

        if( !HttpPost( url, body, response ) )
        {
            return PLM_REVIEW_RESULT::Failure(
                    PLM_ERROR_CODE::ERR_FORGE_API_FAILED,
                    wxString::Format( wxS( "Failed to create pull request (HTTP %d)" ),
                                      m_lastHttpStatus ) );
        }

        PLM_REVIEW review = ParsePullRequest( response );

        // Request reviewers in a separate call
        if( !aReviewerIds.empty() )
        {
            nlohmann::json reviewerBody;
            nlohmann::json reviewers = nlohmann::json::array();

            for( const auto& id : aReviewerIds )
                reviewers.push_back( id.ToStdString() );

            reviewerBody["reviewers"] = reviewers;

            nlohmann::json reviewerResponse;
            wxString reviewerUrl = BuildApiUrl(
                    wxS( "/pulls/" ) + review.review_id + wxS( "/requested_reviewers" ) );
            HttpPost( reviewerUrl, reviewerBody, reviewerResponse );
        }

        return PLM_REVIEW_RESULT::Success( review );
    }

    case PLM_FORGE_TYPE::GITLAB:
    {
        body["title"] = aTitle.ToStdString();
        body["description"] = aBody.ToStdString();
        body["source_branch"] = aSourceBranch.ToStdString();
        body["target_branch"] = aTargetBranch.ToStdString();

        if( aDraft )
            body["draft"] = true;

        if( !aReviewerIds.empty() )
        {
            nlohmann::json reviewerIds = nlohmann::json::array();

            for( const auto& id : aReviewerIds )
                reviewerIds.push_back( id.ToStdString() );

            body["reviewer_ids"] = reviewerIds;
        }

        wxString url = BuildApiUrl( wxS( "/merge_requests" ) );

        if( !HttpPost( url, body, response ) )
        {
            return PLM_REVIEW_RESULT::Failure(
                    PLM_ERROR_CODE::ERR_FORGE_API_FAILED,
                    wxString::Format( wxS( "Failed to create merge request (HTTP %d)" ),
                                      m_lastHttpStatus ) );
        }

        return PLM_REVIEW_RESULT::Success( ParsePullRequest( response ) );
    }

    case PLM_FORGE_TYPE::GITEA:
    {
        body["title"] = aTitle.ToStdString();
        body["body"] = aBody.ToStdString();
        body["head"] = aSourceBranch.ToStdString();
        body["base"] = aTargetBranch.ToStdString();

        if( !aReviewerIds.empty() )
        {
            nlohmann::json reviewers = nlohmann::json::array();

            for( const auto& id : aReviewerIds )
                reviewers.push_back( id.ToStdString() );

            body["reviewers"] = reviewers;
        }

        wxString url = BuildApiUrl( wxS( "/pulls" ) );

        if( !HttpPost( url, body, response ) )
        {
            return PLM_REVIEW_RESULT::Failure(
                    PLM_ERROR_CODE::ERR_FORGE_API_FAILED,
                    wxString::Format( wxS( "Failed to create pull request (HTTP %d)" ),
                                      m_lastHttpStatus ) );
        }

        return PLM_REVIEW_RESULT::Success( ParsePullRequest( response ) );
    }

    default:
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_FORGE_API_FAILED,
                                           wxS( "Unsupported forge type" ) );
    }
}


std::vector<PLM_REVIEW> PLM_GIT_FORGE_PROVIDER::GetPullRequests( const wxString& aTargetBranch )
{
    std::vector<PLM_REVIEW> results;
    nlohmann::json response;
    wxString url;

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
        url = BuildApiUrl( wxS( "/pulls?state=open&per_page=100" ) );

        if( !aTargetBranch.IsEmpty() )
            url += wxS( "&base=" ) + aTargetBranch;

        break;

    case PLM_FORGE_TYPE::GITLAB:
        url = BuildApiUrl( wxS( "/merge_requests?state=opened&per_page=100" ) );

        if( !aTargetBranch.IsEmpty() )
            url += wxS( "&target_branch=" ) + aTargetBranch;

        break;

    case PLM_FORGE_TYPE::GITEA:
        url = BuildApiUrl( wxS( "/pulls?state=open&limit=50" ) );
        break;

    default:
        return results;
    }

    if( !HttpGet( url, response ) || !response.is_array() )
        return results;

    for( const auto& pr : response )
        results.push_back( ParsePullRequest( pr ) );

    return results;
}


PLM_REVIEW PLM_GIT_FORGE_PROVIDER::GetPullRequest( const wxString& aPrId )
{
    nlohmann::json response;
    wxString url;

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
    case PLM_FORGE_TYPE::GITEA:
        url = BuildApiUrl( wxS( "/pulls/" ) + aPrId );
        break;

    case PLM_FORGE_TYPE::GITLAB:
        url = BuildApiUrl( wxS( "/merge_requests/" ) + aPrId );
        break;

    default:
        return PLM_REVIEW();
    }

    if( !HttpGet( url, response ) )
        return PLM_REVIEW();

    return ParsePullRequest( response );
}


std::vector<PLM_REVIEW_COMMENT> PLM_GIT_FORGE_PROVIDER::GetPullRequestComments(
        const wxString& aPrId )
{
    std::vector<PLM_REVIEW_COMMENT> results;
    nlohmann::json response;
    wxString url;

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
        url = BuildApiUrl( wxS( "/issues/" ) + aPrId + wxS( "/comments?per_page=100" ) );
        break;

    case PLM_FORGE_TYPE::GITLAB:
        url = BuildApiUrl( wxS( "/merge_requests/" ) + aPrId + wxS( "/notes?per_page=100" ) );
        break;

    case PLM_FORGE_TYPE::GITEA:
        url = BuildApiUrl( wxS( "/issues/" ) + aPrId + wxS( "/comments" ) );
        break;

    default:
        return results;
    }

    if( !HttpGet( url, response ) || !response.is_array() )
        return results;

    for( const auto& c : response )
    {
        PLM_REVIEW_COMMENT comment = ParseComment( c );
        comment.review_id = aPrId;
        results.push_back( comment );
    }

    return results;
}


bool PLM_GIT_FORGE_PROVIDER::AddPullRequestComment( const wxString& aPrId,
                                                     const PLM_REVIEW_COMMENT& aComment )
{
    nlohmann::json body;
    nlohmann::json response;
    wxString url;

    // Encode spatial annotation metadata into the comment body
    wxString commentBody = aComment.body;

    if( aComment.has_position )
    {
        wxString annotation = wxString::Format(
                wxS( "<!-- kicad-annotation:%s:%s:%f:%f -->\n" ),
                aComment.file_path, aComment.sheet_path, aComment.pos_x, aComment.pos_y );
        commentBody = annotation + commentBody;
    }

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
        body["body"] = commentBody.ToStdString();
        url = BuildApiUrl( wxS( "/issues/" ) + aPrId + wxS( "/comments" ) );
        break;

    case PLM_FORGE_TYPE::GITLAB:
        body["body"] = commentBody.ToStdString();
        url = BuildApiUrl( wxS( "/merge_requests/" ) + aPrId + wxS( "/notes" ) );
        break;

    case PLM_FORGE_TYPE::GITEA:
        body["body"] = commentBody.ToStdString();
        url = BuildApiUrl( wxS( "/issues/" ) + aPrId + wxS( "/comments" ) );
        break;

    default:
        return false;
    }

    return HttpPost( url, body, response );
}


PLM_REVIEW_RESULT PLM_GIT_FORGE_PROVIDER::SubmitPullRequestReview(
        const wxString& aPrId, PLM_REVIEW_DECISION aDecision, const wxString& aBody )
{
    nlohmann::json body;
    nlohmann::json response;

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
    {
        std::string event;

        switch( aDecision )
        {
        case PLM_REVIEW_DECISION::APPROVE:
        case PLM_REVIEW_DECISION::APPROVE_WITH_CONDITIONS:
            event = "APPROVE";
            break;
        case PLM_REVIEW_DECISION::REQUEST_CHANGES:
        case PLM_REVIEW_DECISION::REJECT:
            event = "REQUEST_CHANGES";
            break;
        case PLM_REVIEW_DECISION::COMMENT_ONLY:
        default:
            event = "COMMENT";
            break;
        }

        body["event"] = event;

        if( !aBody.IsEmpty() )
            body["body"] = aBody.ToStdString();

        wxString url = BuildApiUrl( wxS( "/pulls/" ) + aPrId + wxS( "/reviews" ) );

        if( !HttpPost( url, body, response ) )
        {
            return PLM_REVIEW_RESULT::Failure(
                    PLM_ERROR_CODE::ERR_FORGE_API_FAILED,
                    wxString::Format( wxS( "Failed to submit review (HTTP %d)" ),
                                      m_lastHttpStatus ) );
        }

        return PLM_REVIEW_RESULT::Success( GetPullRequest( aPrId ) );
    }

    case PLM_FORGE_TYPE::GITLAB:
    {
        // GitLab uses separate approve/unapprove endpoints
        if( aDecision == PLM_REVIEW_DECISION::APPROVE ||
            aDecision == PLM_REVIEW_DECISION::APPROVE_WITH_CONDITIONS )
        {
            wxString url = BuildApiUrl(
                    wxS( "/merge_requests/" ) + aPrId + wxS( "/approve" ) );

            if( !HttpPost( url, nlohmann::json::object(), response ) )
            {
                return PLM_REVIEW_RESULT::Failure(
                        PLM_ERROR_CODE::ERR_FORGE_API_FAILED,
                        wxString::Format( wxS( "Failed to approve MR (HTTP %d)" ),
                                          m_lastHttpStatus ) );
            }
        }
        else if( aDecision == PLM_REVIEW_DECISION::REQUEST_CHANGES ||
                 aDecision == PLM_REVIEW_DECISION::REJECT )
        {
            wxString url = BuildApiUrl(
                    wxS( "/merge_requests/" ) + aPrId + wxS( "/unapprove" ) );
            HttpPost( url, nlohmann::json::object(), response );
        }

        // Add the review body as a note
        if( !aBody.IsEmpty() )
        {
            nlohmann::json noteBody;
            noteBody["body"] = aBody.ToStdString();

            nlohmann::json noteResponse;
            wxString noteUrl = BuildApiUrl(
                    wxS( "/merge_requests/" ) + aPrId + wxS( "/notes" ) );
            HttpPost( noteUrl, noteBody, noteResponse );
        }

        return PLM_REVIEW_RESULT::Success( GetPullRequest( aPrId ) );
    }

    case PLM_FORGE_TYPE::GITEA:
    {
        std::string event;

        switch( aDecision )
        {
        case PLM_REVIEW_DECISION::APPROVE:
        case PLM_REVIEW_DECISION::APPROVE_WITH_CONDITIONS:
            event = "APPROVED";
            break;
        case PLM_REVIEW_DECISION::REQUEST_CHANGES:
        case PLM_REVIEW_DECISION::REJECT:
            event = "REQUEST_CHANGES";
            break;
        case PLM_REVIEW_DECISION::COMMENT_ONLY:
        default:
            event = "COMMENT";
            break;
        }

        body["event"] = event;

        if( !aBody.IsEmpty() )
            body["body"] = aBody.ToStdString();

        wxString url = BuildApiUrl( wxS( "/pulls/" ) + aPrId + wxS( "/reviews" ) );

        if( !HttpPost( url, body, response ) )
        {
            return PLM_REVIEW_RESULT::Failure(
                    PLM_ERROR_CODE::ERR_FORGE_API_FAILED,
                    wxString::Format( wxS( "Failed to submit review (HTTP %d)" ),
                                      m_lastHttpStatus ) );
        }

        return PLM_REVIEW_RESULT::Success( GetPullRequest( aPrId ) );
    }

    default:
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_FORGE_API_FAILED,
                                           wxS( "Unsupported forge type" ) );
    }
}


PLM_REVIEW_RESULT PLM_GIT_FORGE_PROVIDER::MergePullRequest( const wxString& aPrId,
                                                             const wxString& aCommitMessage )
{
    nlohmann::json body;
    nlohmann::json response;

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
    {
        if( !aCommitMessage.IsEmpty() )
            body["commit_message"] = aCommitMessage.ToStdString();

        wxString url = BuildApiUrl( wxS( "/pulls/" ) + aPrId + wxS( "/merge" ) );

        if( !HttpPut( url, body, response ) )
        {
            return PLM_REVIEW_RESULT::Failure(
                    PLM_ERROR_CODE::ERR_FORGE_API_FAILED,
                    wxString::Format( wxS( "Failed to merge PR (HTTP %d)" ), m_lastHttpStatus ) );
        }

        PLM_REVIEW review = GetPullRequest( aPrId );
        review.status = PLM_REVIEW_STATUS::MERGED;
        return PLM_REVIEW_RESULT::Success( review );
    }

    case PLM_FORGE_TYPE::GITLAB:
    {
        if( !aCommitMessage.IsEmpty() )
            body["merge_commit_message"] = aCommitMessage.ToStdString();

        wxString url = BuildApiUrl( wxS( "/merge_requests/" ) + aPrId + wxS( "/merge" ) );

        if( !HttpPut( url, body, response ) )
        {
            return PLM_REVIEW_RESULT::Failure(
                    PLM_ERROR_CODE::ERR_FORGE_API_FAILED,
                    wxString::Format( wxS( "Failed to merge MR (HTTP %d)" ), m_lastHttpStatus ) );
        }

        return PLM_REVIEW_RESULT::Success( ParsePullRequest( response ) );
    }

    case PLM_FORGE_TYPE::GITEA:
    {
        if( !aCommitMessage.IsEmpty() )
            body["merge_message_field"] = aCommitMessage.ToStdString();

        body["Do"] = "merge";

        wxString url = BuildApiUrl( wxS( "/pulls/" ) + aPrId + wxS( "/merge" ) );

        if( !HttpPost( url, body, response ) )
        {
            return PLM_REVIEW_RESULT::Failure(
                    PLM_ERROR_CODE::ERR_FORGE_API_FAILED,
                    wxString::Format( wxS( "Failed to merge PR (HTTP %d)" ), m_lastHttpStatus ) );
        }

        PLM_REVIEW review = GetPullRequest( aPrId );
        review.status = PLM_REVIEW_STATUS::MERGED;
        return PLM_REVIEW_RESULT::Success( review );
    }

    default:
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_FORGE_API_FAILED,
                                           wxS( "Unsupported forge type" ) );
    }
}


// ---------------------------------------------------------------------------
// Release operations
// ---------------------------------------------------------------------------

PLM_FORGE_RELEASE PLM_GIT_FORGE_PROVIDER::CreateRelease(
        const wxString& aTagName, const wxString& aTitle, const wxString& aBody,
        bool aDraft, bool aPrerelease )
{
    nlohmann::json body;
    nlohmann::json response;
    wxString url;

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
        body["tag_name"] = aTagName.ToStdString();
        body["name"] = aTitle.ToStdString();
        body["body"] = aBody.ToStdString();
        body["draft"] = aDraft;
        body["prerelease"] = aPrerelease;
        url = BuildApiUrl( wxS( "/releases" ) );
        break;

    case PLM_FORGE_TYPE::GITLAB:
        body["tag_name"] = aTagName.ToStdString();
        body["name"] = aTitle.ToStdString();
        body["description"] = aBody.ToStdString();
        url = BuildApiUrl( wxS( "/releases" ) );
        break;

    case PLM_FORGE_TYPE::GITEA:
        body["tag_name"] = aTagName.ToStdString();
        body["name"] = aTitle.ToStdString();
        body["body"] = aBody.ToStdString();
        body["draft"] = aDraft;
        body["prerelease"] = aPrerelease;
        url = BuildApiUrl( wxS( "/releases" ) );
        break;

    default:
        return PLM_FORGE_RELEASE();
    }

    if( !HttpPost( url, body, response ) )
        return PLM_FORGE_RELEASE();

    return ParseRelease( response );
}


std::vector<PLM_FORGE_RELEASE> PLM_GIT_FORGE_PROVIDER::GetReleases( size_t aLimit )
{
    std::vector<PLM_FORGE_RELEASE> results;
    nlohmann::json response;
    wxString url;

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
        url = BuildApiUrl( wxString::Format( wxS( "/releases?per_page=%zu" ), aLimit ) );
        break;

    case PLM_FORGE_TYPE::GITLAB:
        url = BuildApiUrl( wxString::Format( wxS( "/releases?per_page=%zu" ), aLimit ) );
        break;

    case PLM_FORGE_TYPE::GITEA:
        url = BuildApiUrl( wxString::Format( wxS( "/releases?limit=%zu" ), aLimit ) );
        break;

    default:
        return results;
    }

    if( !HttpGet( url, response ) || !response.is_array() )
        return results;

    for( const auto& r : response )
        results.push_back( ParseRelease( r ) );

    return results;
}


bool PLM_GIT_FORGE_PROVIDER::UploadReleaseAsset( const wxString& aReleaseId,
                                                  const wxString& aFilePath,
                                                  const wxString& aContentType )
{
    if( !wxFileExists( aFilePath ) )
        return false;

    wxFileName fn( aFilePath );
    wxString fileName = fn.GetFullName();
    wxString contentType = aContentType;

    if( contentType.IsEmpty() )
        contentType = wxS( "application/octet-stream" );

    // Read file contents
    wxFFile file( aFilePath, wxS( "rb" ) );

    if( !file.IsOpened() )
        return false;

    wxString fileData;
    file.ReadAll( &fileData );
    file.Close();

    std::string fileBytes( fileData.mb_str(), fileData.length() );

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
    {
        // GitHub uses a different upload URL
        std::string url = "https://uploads.github.com/repos/"
                          + m_owner.ToStdString() + "/" + m_repo.ToStdString()
                          + "/releases/" + aReleaseId.ToStdString()
                          + "/assets?name=" + fileName.ToStdString();

        try
        {
            KICAD_CURL_EASY curl;
            curl.SetURL( url );
            curl.SetHeader( "Content-Type", contentType.ToStdString() );
            curl.SetFollowRedirects( true );

            if( m_authConfig && !m_authConfig->password.IsEmpty() )
            {
                curl.SetHeader( "Authorization",
                                "Bearer " + m_authConfig->password.ToStdString() );
            }

            curl.SetPostFields( fileBytes );
            int curlResult = curl.Perform();

            return curlResult == 0 && curl.GetResponseStatusCode() >= 200
                   && curl.GetResponseStatusCode() < 300;
        }
        catch( const std::exception& )
        {
            return false;
        }
    }

    case PLM_FORGE_TYPE::GITLAB:
    {
        // GitLab release asset upload requires multipart form data, not supported yet
        return false;
    }

    case PLM_FORGE_TYPE::GITEA:
    {
        std::string url = m_apiBase.ToStdString() + "/repos/"
                          + m_owner.ToStdString() + "/" + m_repo.ToStdString()
                          + "/releases/" + aReleaseId.ToStdString()
                          + "/assets?name=" + fileName.ToStdString();

        try
        {
            KICAD_CURL_EASY curl;
            curl.SetURL( url );
            curl.SetHeader( "Content-Type", contentType.ToStdString() );
            curl.SetFollowRedirects( true );

            if( m_authConfig && !m_authConfig->password.IsEmpty() )
                curl.SetHeader( "Authorization", "token " + m_authConfig->password.ToStdString() );

            curl.SetPostFields( fileBytes );
            int curlResult = curl.Perform();

            return curlResult == 0 && curl.GetResponseStatusCode() >= 200
                   && curl.GetResponseStatusCode() < 300;
        }
        catch( const std::exception& )
        {
            return false;
        }
    }

    default:
        return false;
    }
}


std::vector<PLM_FORGE_ASSET> PLM_GIT_FORGE_PROVIDER::GetReleaseAssets(
        const wxString& aReleaseId )
{
    std::vector<PLM_FORGE_ASSET> results;
    nlohmann::json response;
    wxString url;

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
        url = BuildApiUrl( wxS( "/releases/" ) + aReleaseId + wxS( "/assets" ) );
        break;

    case PLM_FORGE_TYPE::GITLAB:
    {
        // GitLab groups assets under the release JSON itself
        url = BuildApiUrl( wxS( "/releases" ) );
        nlohmann::json releasesJson;

        if( HttpGet( url, releasesJson ) && releasesJson.is_array() )
        {
            for( const auto& r : releasesJson )
            {
                if( std::to_string( JsonInt( r, "id" ) ) == aReleaseId.ToStdString() &&
                    r.contains( "assets" ) && r["assets"].is_object() &&
                    r["assets"].contains( "links" ) && r["assets"]["links"].is_array() )
                {
                    for( const auto& link : r["assets"]["links"] )
                    {
                        PLM_FORGE_ASSET asset;
                        asset.asset_id = wxString::FromUTF8(
                                std::to_string( JsonInt( link, "id" ) ) );
                        asset.name = wxString::FromUTF8( JsonSafe( link, "name" ) );
                        asset.download_url = wxString::FromUTF8( JsonSafe( link, "url" ) );
                        results.push_back( asset );
                    }
                }
            }
        }

        return results;
    }

    case PLM_FORGE_TYPE::GITEA:
        url = BuildApiUrl( wxS( "/releases/" ) + aReleaseId + wxS( "/assets" ) );
        break;

    default:
        return results;
    }

    if( !HttpGet( url, response ) || !response.is_array() )
        return results;

    for( const auto& a : response )
        results.push_back( ParseAsset( a ) );

    return results;
}


// ---------------------------------------------------------------------------
// CI/CD Pipeline operations
// ---------------------------------------------------------------------------

PLM_PIPELINE_STATUS PLM_GIT_FORGE_PROVIDER::GetPipelineStatus( const wxString& aCommitSha )
{
    PLM_PIPELINE_STATUS status;
    status.commit_sha = aCommitSha;

    nlohmann::json response;

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
    {
        // Fetch both commit status and check runs
        wxString statusUrl = BuildApiUrl(
                wxS( "/commits/" ) + aCommitSha + wxS( "/status" ) );

        if( HttpGet( statusUrl, response ) && response.is_object() )
        {
            if( response.contains( "statuses" ) && response["statuses"].is_array() )
            {
                for( const auto& s : response["statuses"] )
                    status.checks.push_back( ParseCheck( s ) );
            }
        }

        // Also get check runs (GitHub Actions)
        nlohmann::json checksResponse;
        wxString checksUrl = BuildApiUrl(
                wxS( "/commits/" ) + aCommitSha + wxS( "/check-runs" ) );

        if( HttpGet( checksUrl, checksResponse ) && checksResponse.is_object()
            && checksResponse.contains( "check_runs" ) )
        {
            for( const auto& cr : checksResponse["check_runs"] )
                status.checks.push_back( ParseCheck( cr ) );
        }

        break;
    }

    case PLM_FORGE_TYPE::GITLAB:
    {
        wxString url = BuildApiUrl(
                wxS( "/repository/commits/" ) + aCommitSha + wxS( "/statuses" ) );

        if( HttpGet( url, response ) && response.is_array() )
        {
            for( const auto& s : response )
                status.checks.push_back( ParseCheck( s ) );
        }

        break;
    }

    case PLM_FORGE_TYPE::GITEA:
    {
        wxString url = BuildApiUrl(
                wxS( "/commits/" ) + aCommitSha + wxS( "/status" ) );

        if( HttpGet( url, response ) && response.is_object()
            && response.contains( "statuses" ) && response["statuses"].is_array() )
        {
            for( const auto& s : response["statuses"] )
                status.checks.push_back( ParseCheck( s ) );
        }

        break;
    }

    default:
        break;
    }

    // Compute overall state from individual checks
    if( status.checks.empty() )
    {
        status.overall_state = PLM_PIPELINE_STATE::UNKNOWN;
    }
    else
    {
        bool allPassed = true;
        bool anyFailed = false;
        bool anyRunning = false;

        for( const auto& check : status.checks )
        {
            if( check.state == PLM_PIPELINE_STATE::FAILED )
                anyFailed = true;

            if( check.state == PLM_PIPELINE_STATE::RUNNING ||
                check.state == PLM_PIPELINE_STATE::QUEUED )
            {
                anyRunning = true;
            }

            if( check.state != PLM_PIPELINE_STATE::PASSED &&
                check.state != PLM_PIPELINE_STATE::SKIPPED )
            {
                allPassed = false;
            }
        }

        if( anyFailed )
            status.overall_state = PLM_PIPELINE_STATE::FAILED;
        else if( anyRunning )
            status.overall_state = PLM_PIPELINE_STATE::RUNNING;
        else if( allPassed )
            status.overall_state = PLM_PIPELINE_STATE::PASSED;
        else
            status.overall_state = PLM_PIPELINE_STATE::UNKNOWN;
    }

    return status;
}


PLM_PIPELINE_STATUS PLM_GIT_FORGE_PROVIDER::GetPRPipelineStatus( const wxString& aPrId )
{
    // Get the PR to find the head commit SHA, then delegate
    PLM_REVIEW pr = GetPullRequest( aPrId );

    if( pr.review_id.IsEmpty() )
        return PLM_PIPELINE_STATUS();

    // For the head commit, we need to fetch the PR JSON directly
    nlohmann::json response;
    wxString url;
    wxString commitSha;

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
    case PLM_FORGE_TYPE::GITEA:
        url = BuildApiUrl( wxS( "/pulls/" ) + aPrId );
        break;

    case PLM_FORGE_TYPE::GITLAB:
        url = BuildApiUrl( wxS( "/merge_requests/" ) + aPrId );
        break;

    default:
        return PLM_PIPELINE_STATUS();
    }

    if( !HttpGet( url, response ) )
        return PLM_PIPELINE_STATUS();

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
        if( response.contains( "head" ) && response["head"].is_object() )
            commitSha = wxString::FromUTF8( JsonSafe( response["head"], "sha" ) );
        break;

    case PLM_FORGE_TYPE::GITLAB:
        commitSha = wxString::FromUTF8( JsonSafe( response, "sha" ) );
        break;

    case PLM_FORGE_TYPE::GITEA:
        if( response.contains( "head" ) && response["head"].is_object() )
            commitSha = wxString::FromUTF8( JsonSafe( response["head"], "sha" ) );
        break;

    default:
        break;
    }

    if( commitSha.IsEmpty() )
        return PLM_PIPELINE_STATUS();

    return GetPipelineStatus( commitSha );
}


std::vector<PLM_FORGE_ASSET> PLM_GIT_FORGE_PROVIDER::GetPipelineArtifacts(
        const wxString& aCommitSha )
{
    std::vector<PLM_FORGE_ASSET> results;
    nlohmann::json response;

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
    {
        // List artifacts for the repository, filtered by workflow runs for this commit
        wxString url = BuildApiUrl( wxS( "/actions/artifacts?per_page=100" ) );

        if( HttpGet( url, response ) && response.is_object()
            && response.contains( "artifacts" ) && response["artifacts"].is_array() )
        {
            for( const auto& a : response["artifacts"] )
            {
                PLM_FORGE_ASSET asset;
                asset.asset_id = wxString::FromUTF8( std::to_string( JsonInt( a, "id" ) ) );
                asset.name = wxString::FromUTF8( JsonSafe( a, "name" ) );

                if( a.contains( "size_in_bytes" ) && a["size_in_bytes"].is_number() )
                    asset.size_bytes = static_cast<size_t>( a["size_in_bytes"].get<int64_t>() );

                asset.download_url = wxString::FromUTF8(
                        JsonSafe( a, "archive_download_url" ) );
                results.push_back( asset );
            }
        }

        break;
    }

    case PLM_FORGE_TYPE::GITLAB:
    {
        wxString url = BuildApiUrl( wxS( "/jobs?per_page=100" ) );

        if( HttpGet( url, response ) && response.is_array() )
        {
            for( const auto& job : response )
            {
                if( job.contains( "artifacts" ) && job["artifacts"].is_array() )
                {
                    for( const auto& a : job["artifacts"] )
                    {
                        PLM_FORGE_ASSET asset;
                        asset.name = wxString::FromUTF8( JsonSafe( a, "filename" ) );

                        if( a.contains( "size" ) && a["size"].is_number() )
                            asset.size_bytes = static_cast<size_t>( a["size"].get<int64_t>() );

                        results.push_back( asset );
                    }
                }
            }
        }

        break;
    }

    default:
        break;
    }

    return results;
}


bool PLM_GIT_FORGE_PROVIDER::DownloadArtifact( const wxString& aArtifactId,
                                                const wxString& aLocalPath )
{
    wxString url;

    switch( m_forgeType )
    {
    case PLM_FORGE_TYPE::GITHUB:
        url = BuildApiUrl( wxS( "/actions/artifacts/" ) + aArtifactId + wxS( "/zip" ) );
        break;

    case PLM_FORGE_TYPE::GITLAB:
        url = BuildApiUrl( wxS( "/jobs/" ) + aArtifactId + wxS( "/artifacts" ) );
        break;

    default:
        return false;
    }

    try
    {
        KICAD_CURL_EASY curl;
        curl.SetURL( url.ToStdString() );
        curl.SetFollowRedirects( true );

        if( m_authConfig && !m_authConfig->password.IsEmpty() )
        {
            switch( m_forgeType )
            {
            case PLM_FORGE_TYPE::GITLAB:
                curl.SetHeader( "PRIVATE-TOKEN", m_authConfig->password.ToStdString() );
                break;

            default:
                curl.SetHeader( "Authorization",
                                "Bearer " + m_authConfig->password.ToStdString() );
                break;
            }
        }

        std::ofstream outFile( aLocalPath.ToStdString(), std::ios::binary );

        if( !outFile.is_open() )
            return false;

        curl.SetOutputStream( &outFile );

        int curlResult = curl.Perform();
        outFile.close();

        return curlResult == 0 && curl.GetResponseStatusCode() >= 200
               && curl.GetResponseStatusCode() < 300;
    }
    catch( const std::exception& )
    {
        return false;
    }
}
