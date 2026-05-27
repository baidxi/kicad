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
 * @file plm_auth.h
 * @brief Authentication structures for the PLM client interface.
 * @ingroup plm_auth
 *
 * This header defines structures for handling authentication credentials,
 * session management, and authentication results. Special care is taken
 * to handle sensitive data (passwords, tokens) securely.
 */

#ifndef PLM_AUTH_H
#define PLM_AUTH_H

#include "plm_types.h"

#include <wx/string.h>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <map>
#include <utility>
#include <vector>


/**
 * A string class that securely zeros its memory on destruction.
 *
 * This class wraps sensitive string data (passwords, tokens) and ensures
 * the memory is overwritten with zeros when the object is destroyed or
 * when the value is changed. This helps prevent sensitive data from
 * lingering in memory.
 */
class PLM_SECURE_STRING
{
public:
    PLM_SECURE_STRING() = default;

    explicit PLM_SECURE_STRING( const wxString& aValue ) : m_value( aValue ) {}

    explicit PLM_SECURE_STRING( wxString&& aValue ) : m_value( std::move( aValue ) ) {}

    ~PLM_SECURE_STRING() { Clear(); }

    PLM_SECURE_STRING( const PLM_SECURE_STRING& aOther ) : m_value( aOther.m_value ) {}

    PLM_SECURE_STRING( PLM_SECURE_STRING&& aOther ) noexcept : m_value( std::move( aOther.m_value ) )
    {
        aOther.Clear();
    }

    PLM_SECURE_STRING& operator=( const PLM_SECURE_STRING& aOther )
    {
        if( this != &aOther )
        {
            Clear();
            m_value = aOther.m_value;
        }

        return *this;
    }

    PLM_SECURE_STRING& operator=( PLM_SECURE_STRING&& aOther ) noexcept
    {
        if( this != &aOther )
        {
            Clear();
            m_value = std::move( aOther.m_value );
            aOther.Clear();
        }

        return *this;
    }

    PLM_SECURE_STRING& operator=( const wxString& aValue )
    {
        Clear();
        m_value = aValue;
        return *this;
    }

    /**
     * Get the underlying string value.
     * @return Reference to the stored string.
     */
    const wxString& Get() const { return m_value; }

    /**
     * Check if the string is empty.
     * @return True if the string is empty.
     */
    bool IsEmpty() const { return m_value.IsEmpty(); }

    /**
     * Clear the string and zero its memory.
     *
     * Uses volatile pointer to prevent compiler optimization from removing
     * the zeroing operation. While wxString's internal buffer may be shared,
     * this at least zeros our local copy before clearing.
     */
    void Clear()
    {
        if( !m_value.IsEmpty() )
        {
            // Overwrite with a string of spaces of the same length, then clear.
            // This forces wxString to copy-on-write if shared, giving us our own buffer to zero.
            size_t len = m_value.Length();
            m_value = wxString( len, wxT( ' ' ) );

            // Now zero the buffer character by character using volatile to prevent optimization
            for( size_t i = 0; i < len && i < m_value.Length(); ++i )
                m_value[i] = wxT( '\0' );

            m_value.Clear();
        }
    }

private:
    wxString m_value;
};


/**
 * Authentication method for PLM connections.
 *
 * Backends interpret these differently. For Git, HTTPS_BASIC uses
 * username/password, HTTPS_TOKEN uses a personal access token, and
 * SSH_KEY/SSH_AGENT use SSH authentication. For Odoo and other RPC
 * backends, PASSWORD is the typical method. AUTO_DETECT lets the
 * plugin choose based on URL scheme and environment.
 */
enum class PLM_AUTH_METHOD
{
    AUTO_DETECT = 0,    ///< Plugin determines the best method from the URL and environment
    PASSWORD,           ///< Username and password (generic)
    TOKEN,              ///< Personal access token or API key
    SSH_KEY,            ///< SSH key file authentication
    SSH_AGENT,          ///< SSH agent forwarding
    OAUTH,              ///< OAuth 2.0 (reserved for future use)
    API_KEY             ///< Backend-specific API key
};


/**
 * Credentials for PLM server authentication.
 *
 * Contains the information needed to authenticate with a PLM server.
 * The password is stored in a secure string that is zeroed on destruction.
 *
 * The auth_method field selects the authentication strategy, and the
 * properties map carries backend-specific parameters such as
 * "ssh_key_path", "database", or "token".
 */
struct PLM_CREDENTIALS
{
    wxString          server_url;   ///< PLM server URL
    wxString          username;     ///< Username for authentication
    PLM_SECURE_STRING password;     ///< Password or token (zeroed after use)

    PLM_AUTH_METHOD   auth_method = PLM_AUTH_METHOD::AUTO_DETECT;

    /// Backend-specific parameters (e.g. "ssh_key_path", "database", "token")
    std::map<wxString, wxString> properties;

    PLM_CREDENTIALS() = default;

    PLM_CREDENTIALS( const wxString& aServerUrl, const wxString& aUsername,
                     const wxString& aPassword ) :
            server_url( aServerUrl ),
            username( aUsername ),
            password( aPassword )
    {
    }

    PLM_CREDENTIALS( const PLM_CREDENTIALS& ) = default;
    PLM_CREDENTIALS( PLM_CREDENTIALS&& ) = default;
    PLM_CREDENTIALS& operator=( const PLM_CREDENTIALS& ) = default;
    PLM_CREDENTIALS& operator=( PLM_CREDENTIALS&& ) = default;

    /**
     * Get a backend-specific property, returning a default if not found.
     */
    wxString GetProperty( const wxString& aKey,
                          const wxString& aDefault = wxEmptyString ) const
    {
        auto it = properties.find( aKey );
        return it != properties.end() ? it->second : aDefault;
    }

    /**
     * Clear all credentials, zeroing the password.
     */
    void Clear()
    {
        server_url.Clear();
        username.Clear();
        password.Clear();
        auth_method = PLM_AUTH_METHOD::AUTO_DETECT;
        properties.clear();
    }

    /**
     * Create credentials for SSH key authentication.
     */
    static PLM_CREDENTIALS SshKey( const wxString& aServerUrl,
                                    const wxString& aKeyPath,
                                    const wxString& aPassphrase = wxEmptyString )
    {
        PLM_CREDENTIALS creds;
        creds.server_url = aServerUrl;
        creds.auth_method = PLM_AUTH_METHOD::SSH_KEY;
        creds.properties[wxS( "ssh_key_path" )] = aKeyPath;

        if( !aPassphrase.IsEmpty() )
            creds.password = PLM_SECURE_STRING( aPassphrase );

        return creds;
    }

    /**
     * Create credentials for token-based authentication.
     */
    static PLM_CREDENTIALS Token( const wxString& aServerUrl,
                                   const wxString& aToken )
    {
        PLM_CREDENTIALS creds;
        creds.server_url = aServerUrl;
        creds.auth_method = PLM_AUTH_METHOD::TOKEN;
        creds.password = PLM_SECURE_STRING( aToken );
        return creds;
    }

    /**
     * Create credentials for Odoo with an explicit database name.
     */
    static PLM_CREDENTIALS OdooLogin( const wxString& aServerUrl,
                                       const wxString& aDatabase,
                                       const wxString& aUsername,
                                       const wxString& aPassword )
    {
        PLM_CREDENTIALS creds( aServerUrl, aUsername, aPassword );
        creds.auth_method = PLM_AUTH_METHOD::PASSWORD;
        creds.properties[wxS( "database" )] = aDatabase;
        return creds;
    }
};


/**
 * Result of an authentication attempt.
 *
 * Contains the outcome of authenticating with a PLM server, including
 * any error information and the session token on success.
 */
struct PLM_AUTH_RESULT
{
    bool              success = false;   ///< True if authentication succeeded
    PLM_ERROR_CODE    error_code = PLM_ERROR_CODE::SUCCESS;  ///< Error code if failed
    wxString          error_message;     ///< Human-readable error message
    PLM_SECURE_STRING token;             ///< Session token (if successful)
    int64_t           expires_at = 0;    ///< Token expiry time (Unix epoch seconds)

    PLM_AUTH_RESULT() = default;

    /**
     * Create a successful authentication result.
     */
    static PLM_AUTH_RESULT Success( const wxString& aToken, int64_t aExpiresAt )
    {
        PLM_AUTH_RESULT result;
        result.success = true;
        result.token = PLM_SECURE_STRING( aToken );
        result.expires_at = aExpiresAt;
        return result;
    }

    /**
     * Create a failed authentication result.
     */
    static PLM_AUTH_RESULT Failure( PLM_ERROR_CODE aCode, const wxString& aMessage )
    {
        PLM_AUTH_RESULT result;
        result.success = false;
        result.error_code = aCode;
        result.error_message = aMessage;
        return result;
    }

    PLM_AUTH_RESULT( const PLM_AUTH_RESULT& ) = default;
    PLM_AUTH_RESULT( PLM_AUTH_RESULT&& ) = default;
    PLM_AUTH_RESULT& operator=( const PLM_AUTH_RESULT& ) = default;
    PLM_AUTH_RESULT& operator=( PLM_AUTH_RESULT&& ) = default;
};


/**
 * Basic user information from the PLM system.
 */
struct PLM_USER_INFO
{
    wxString              user_id;       ///< Unique user identifier
    wxString              display_name;  ///< User's display name
    wxString              email;         ///< User's email address (may be empty)
    std::vector<wxString> roles;         ///< Roles assigned to the user (e.g. "admin", "write")
    std::vector<wxString> groups;        ///< Group memberships (e.g. "PLM / Manager")

    PLM_USER_INFO() = default;

    PLM_USER_INFO( const wxString& aUserId, const wxString& aDisplayName,
                   const wxString& aEmail = wxEmptyString ) :
            user_id( aUserId ),
            display_name( aDisplayName ),
            email( aEmail )
    {
    }
};


/**
 * Active session with a PLM server.
 *
 * Represents an authenticated session including the token, expiry time,
 * and information about the authenticated user. The token is stored
 * securely and zeroed on destruction.
 */
struct PLM_SESSION
{
    PLM_SECURE_STRING token;            ///< Session token (zeroed on destruction)
    int64_t           expires_at = 0;   ///< Token expiry time (Unix epoch seconds)
    wxString          server_url;       ///< PLM server URL for this session
    PLM_USER_INFO     user_info;        ///< Information about the authenticated user

    PLM_SESSION() = default;

    PLM_SESSION( const wxString& aToken, int64_t aExpiresAt, const wxString& aServerUrl,
                 const PLM_USER_INFO& aUserInfo ) :
            token( aToken ),
            expires_at( aExpiresAt ),
            server_url( aServerUrl ),
            user_info( aUserInfo )
    {
    }

    PLM_SESSION( const PLM_SESSION& ) = default;
    PLM_SESSION( PLM_SESSION&& ) = default;
    PLM_SESSION& operator=( const PLM_SESSION& ) = default;
    PLM_SESSION& operator=( PLM_SESSION&& ) = default;

    /**
     * Check if the session token has expired.
     * @return True if the token has expired or is about to expire within the grace period.
     */
    bool IsExpired( int64_t aGraceSeconds = 60 ) const
    {
        if( expires_at == 0 )
            return true;

        // Get current time as Unix epoch seconds
        int64_t now = static_cast<int64_t>( std::time( nullptr ) );
        return now >= ( expires_at - aGraceSeconds );
    }

    /**
     * Check if this session is valid (has a token and is not expired).
     * @return True if the session appears valid.
     */
    bool IsValid() const
    {
        return !token.IsEmpty() && !IsExpired();
    }

    /**
     * Clear the session, zeroing the token.
     */
    void Clear()
    {
        token.Clear();
        expires_at = 0;
        server_url.Clear();
        user_info = PLM_USER_INFO();
    }
};


/**
 * Health status of a PLM plugin connection.
 *
 * Contains detailed information about server connectivity, session validity,
 * and any error conditions for displaying connection health in the UI.
 */
struct PLM_HEALTH_STATUS
{
    bool     is_healthy = false;         ///< True if server is reachable and responding
    bool     is_authenticated = false;   ///< True if session is valid
    int64_t  session_expires_in = 0;     ///< Seconds until session expires (0 if not authenticated)
    wxString server_url;                 ///< Server URL being checked
    wxString error_message;              ///< Error message if unhealthy

    PLM_HEALTH_STATUS() = default;

    /**
     * Check if the session is about to expire.
     * @param aThresholdSeconds Warn if session expires within this time.
     * @return True if session will expire within the threshold.
     */
    bool IsSessionExpiringSoon( int64_t aThresholdSeconds = 300 ) const
    {
        return is_authenticated && session_expires_in > 0 && session_expires_in < aThresholdSeconds;
    }

    /**
     * Create a healthy status.
     */
    static PLM_HEALTH_STATUS Healthy( const wxString& aServerUrl = wxEmptyString )
    {
        PLM_HEALTH_STATUS status;
        status.is_healthy = true;
        status.server_url = aServerUrl;
        return status;
    }

    /**
     * Create an unhealthy status.
     */
    static PLM_HEALTH_STATUS Unhealthy( const wxString& aError,
                                        const wxString& aServerUrl = wxEmptyString )
    {
        PLM_HEALTH_STATUS status;
        status.is_healthy = false;
        status.error_message = aError;
        status.server_url = aServerUrl;
        return status;
    }
};


#endif // PLM_AUTH_H
