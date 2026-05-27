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
 * @file plm_git_auth.h
 * @brief Git authentication helpers for the PLM Git plugin.
 *
 * Provides authentication support for various Git hosting scenarios:
 * - HTTPS with username/password
 * - HTTPS with personal access tokens (GitHub, GitLab)
 * - SSH key authentication
 * - Git credential helper integration
 */

#ifndef PLM_GIT_AUTH_H
#define PLM_GIT_AUTH_H

#include <wx/string.h>

#include <map>
#include <memory>


/**
 * Git authentication method.
 */
enum class GIT_AUTH_METHOD
{
    NONE = 0,           ///< No authentication (public repos)
    HTTPS_BASIC,        ///< HTTPS with username/password
    HTTPS_TOKEN,        ///< HTTPS with personal access token
    SSH_KEY,            ///< SSH with key file
    SSH_AGENT,          ///< SSH with agent
    CREDENTIAL_HELPER   ///< Use Git credential helper
};


/**
 * Result of an authentication attempt.
 */
struct GIT_AUTH_RESULT
{
    bool     success = false;   ///< True if authentication succeeded
    wxString error_message;     ///< Error message if failed
    wxString username;          ///< Resolved username
    wxString auth_url;          ///< URL with credentials embedded (for clone)
};


/**
 * Git authentication configuration.
 */
struct GIT_AUTH_CONFIG
{
    GIT_AUTH_METHOD method = GIT_AUTH_METHOD::NONE;
    wxString        username;           ///< Username for HTTPS auth
    wxString        password;           ///< Password or token for HTTPS
    wxString        ssh_key_path;       ///< Path to SSH private key
    wxString        ssh_passphrase;     ///< SSH key passphrase (if encrypted)
    wxString        credential_helper;  ///< Custom credential helper command
    bool            store_credentials = false;  ///< Remember credentials

    GIT_AUTH_CONFIG() = default;

    static GIT_AUTH_CONFIG HttpsBasic( const wxString& aUsername, const wxString& aPassword )
    {
        GIT_AUTH_CONFIG cfg;
        cfg.method = GIT_AUTH_METHOD::HTTPS_BASIC;
        cfg.username = aUsername;
        cfg.password = aPassword;
        return cfg;
    }

    static GIT_AUTH_CONFIG HttpsToken( const wxString& aToken )
    {
        GIT_AUTH_CONFIG cfg;
        cfg.method = GIT_AUTH_METHOD::HTTPS_TOKEN;
        cfg.password = aToken;
        return cfg;
    }

    static GIT_AUTH_CONFIG SshKey( const wxString& aKeyPath, const wxString& aPassphrase = wxEmptyString )
    {
        GIT_AUTH_CONFIG cfg;
        cfg.method = GIT_AUTH_METHOD::SSH_KEY;
        cfg.ssh_key_path = aKeyPath;
        cfg.ssh_passphrase = aPassphrase;
        return cfg;
    }

    static GIT_AUTH_CONFIG SshAgent()
    {
        GIT_AUTH_CONFIG cfg;
        cfg.method = GIT_AUTH_METHOD::SSH_AGENT;
        return cfg;
    }
};


/**
 * Git authentication helper.
 *
 * Handles authentication for Git operations, supporting multiple
 * authentication methods including HTTPS credentials, tokens, and SSH.
 */
class GIT_AUTH_HELPER
{
public:
    GIT_AUTH_HELPER();
    ~GIT_AUTH_HELPER();

    /**
     * Configure authentication for subsequent operations.
     *
     * @param aConfig Authentication configuration.
     */
    void Configure( const GIT_AUTH_CONFIG& aConfig );

    /**
     * Test authentication against a remote URL.
     *
     * @param aRemoteUrl The Git remote URL to test.
     * @return Authentication result.
     */
    GIT_AUTH_RESULT TestAuth( const wxString& aRemoteUrl );

    /**
     * Build an authenticated URL for HTTPS operations.
     *
     * Embeds credentials into the URL for clone/fetch operations.
     * Returns original URL for SSH or if no auth configured.
     *
     * @param aOriginalUrl Original Git URL.
     * @return URL with embedded credentials, or original for SSH.
     */
    wxString BuildAuthenticatedUrl( const wxString& aOriginalUrl );

    /**
     * Get environment variables needed for authentication.
     *
     * For SSH operations, returns variables like GIT_SSH_COMMAND.
     * For credential helpers, returns GIT_ASKPASS settings.
     *
     * @return Map of environment variable name to value.
     */
    std::map<wxString, wxString> GetAuthEnvironment();

    /**
     * Store credentials in Git credential helper.
     *
     * @param aUrl URL the credentials are for.
     * @param aUsername Username to store.
     * @param aPassword Password or token to store.
     * @return True if stored successfully.
     */
    bool StoreCredentials( const wxString& aUrl, const wxString& aUsername,
                           const wxString& aPassword );

    /**
     * Retrieve credentials from Git credential helper.
     *
     * @param aUrl URL to retrieve credentials for.
     * @param aUsername Output parameter for username.
     * @param aPassword Output parameter for password.
     * @return True if credentials found.
     */
    bool RetrieveCredentials( const wxString& aUrl, wxString& aUsername, wxString& aPassword );

    /**
     * Remove stored credentials for a URL.
     *
     * @param aUrl URL to remove credentials for.
     * @return True if removal succeeded.
     */
    bool RemoveCredentials( const wxString& aUrl );

    /**
     * Detect the appropriate authentication method for a URL.
     *
     * Analyzes the URL scheme and host to suggest the best auth method.
     *
     * @param aUrl Git URL to analyze.
     * @return Suggested authentication method.
     */
    static GIT_AUTH_METHOD DetectAuthMethod( const wxString& aUrl );

    /**
     * Check if a URL uses SSH protocol.
     *
     * @param aUrl URL to check.
     * @return True if URL uses SSH (git@ or ssh://).
     */
    static bool IsSshUrl( const wxString& aUrl );

    /**
     * Check if a URL uses HTTPS protocol.
     *
     * @param aUrl URL to check.
     * @return True if URL uses HTTPS.
     */
    static bool IsHttpsUrl( const wxString& aUrl );

    /**
     * Get the default SSH key path.
     *
     * @return Path to ~/.ssh/id_rsa or equivalent.
     */
    static wxString GetDefaultSshKeyPath();

    /**
     * Check if SSH agent is available.
     *
     * @return True if SSH agent is running and has keys.
     */
    static bool IsSshAgentAvailable();

private:
    /**
     * Run git credential fill command.
     *
     * @param aUrl URL to get credentials for.
     * @param aUsername Output username.
     * @param aPassword Output password.
     * @return True if credentials obtained.
     */
    bool RunCredentialFill( const wxString& aUrl, wxString& aUsername, wxString& aPassword );

    /**
     * Run git credential store command.
     *
     * @param aUrl URL.
     * @param aUsername Username.
     * @param aPassword Password.
     * @return True if stored.
     */
    bool RunCredentialStore( const wxString& aUrl, const wxString& aUsername,
                             const wxString& aPassword );

    /**
     * Run git credential reject command.
     *
     * @param aUrl URL.
     * @return True if rejected.
     */
    bool RunCredentialReject( const wxString& aUrl );

    /**
     * Parse URL components.
     *
     * @param aUrl URL to parse.
     * @param aProtocol Output protocol (https, ssh, git).
     * @param aHost Output hostname.
     * @param aPath Output path.
     * @return True if parsed successfully.
     */
    bool ParseUrl( const wxString& aUrl, wxString& aProtocol, wxString& aHost, wxString& aPath );

    GIT_AUTH_CONFIG m_config;
};


#endif // PLM_GIT_AUTH_H
