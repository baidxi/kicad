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
 * @file plm_token_manager.h
 * @brief Token lifecycle management for PLM sessions.
 *
 * This header defines the PLM_TOKEN_MANAGER class which handles session token
 * storage, expiry tracking, and automatic refresh. Tokens are stored in memory
 * only (not in secure storage) since they are short-lived and server-specific.
 */

#ifndef PLM_TOKEN_MANAGER_H
#define PLM_TOKEN_MANAGER_H

#include "plm_auth.h"
#include "plm_types.h"

#include <wx/string.h>
#include <functional>
#include <map>
#include <memory>
#include <mutex>


/**
 * Callback type for token refresh operations.
 *
 * The callback receives the server URL and current credentials, and should
 * return a new PLM_AUTH_RESULT with a fresh token.
 *
 * @param aServerUrl Server URL to refresh token for.
 * @param aCredentials Current credentials (may be empty if using stored creds).
 * @return Authentication result with new token.
 */
using PLM_TOKEN_REFRESH_CALLBACK = std::function<PLM_AUTH_RESULT( const wxString& aServerUrl,
                                                                   const PLM_CREDENTIALS& aCredentials )>;

/**
 * Callback type for token expiry notifications.
 *
 * Called when a token expires or is about to expire.
 *
 * @param aServerUrl Server URL whose token expired.
 * @param aExpired True if token has fully expired, false if nearing expiry.
 */
using PLM_TOKEN_EXPIRY_CALLBACK = std::function<void( const wxString& aServerUrl, bool aExpired )>;


/**
 * Token entry with metadata.
 */
struct PLM_TOKEN_ENTRY
{
    PLM_SECURE_STRING token;          ///< The session token
    int64_t           expires_at = 0; ///< Expiry timestamp (Unix epoch seconds)
    int64_t           refresh_at = 0; ///< When to trigger proactive refresh
    wxString          server_url;     ///< Associated server URL
    bool              is_refreshing = false; ///< True if refresh is in progress

    PLM_TOKEN_ENTRY() = default;

    PLM_TOKEN_ENTRY( const wxString& aToken, int64_t aExpiresAt, const wxString& aServerUrl ) :
            token( aToken ),
            expires_at( aExpiresAt ),
            server_url( aServerUrl )
    {
        // Set refresh time to 5 minutes before expiry, or halfway if less than 10 min remaining
        int64_t now = std::time( nullptr );
        int64_t remaining = aExpiresAt - now;

        if( remaining > 600 )
            refresh_at = aExpiresAt - 300; // 5 minutes before
        else if( remaining > 0 )
            refresh_at = now + ( remaining / 2 ); // Halfway
        else
            refresh_at = 0; // Already expired
    }

    /**
     * Check if this token has expired.
     * @param aGraceSeconds Grace period before considering expired.
     * @return True if expired.
     */
    bool IsExpired( int64_t aGraceSeconds = 30 ) const
    {
        if( expires_at == 0 )
            return true;

        int64_t now = std::time( nullptr );
        return now >= ( expires_at - aGraceSeconds );
    }

    /**
     * Check if this token should be refreshed proactively.
     * @return True if refresh should be triggered.
     */
    bool NeedsRefresh() const
    {
        if( refresh_at == 0 || is_refreshing )
            return false;

        int64_t now = std::time( nullptr );
        return now >= refresh_at;
    }

    /**
     * Get remaining time until expiry.
     * @return Seconds until expiry, or 0 if expired.
     */
    int64_t GetRemainingTime() const
    {
        if( expires_at == 0 )
            return 0;

        int64_t now = std::time( nullptr );
        int64_t remaining = expires_at - now;

        return ( remaining > 0 ) ? remaining : 0;
    }

    /**
     * Clear this token entry.
     */
    void Clear()
    {
        token.Clear();
        expires_at = 0;
        refresh_at = 0;
        server_url.Clear();
        is_refreshing = false;
    }
};


/**
 * Manages session tokens for PLM servers.
 *
 * Tokens are stored in memory only and are automatically cleared on logout
 * or application exit. Supports proactive refresh before expiry.
 *
 * Thread-safe: All public methods are protected by mutex.
 *
 * Usage:
 * @code
 * PLM_TOKEN_MANAGER tokenMgr;
 *
 * // Store token after successful authentication
 * tokenMgr.StoreToken( serverUrl, authResult.token.Get(), authResult.expires_at );
 *
 * // Get token for API calls
 * wxString token;
 * if( tokenMgr.GetToken( serverUrl, token ) )
 * {
 *     // Use token for API request
 * }
 *
 * // Register refresh callback for automatic refresh
 * tokenMgr.SetRefreshCallback( [&]( const wxString& url, const PLM_CREDENTIALS& creds ) {
 *     return plugin->RefreshToken( url, creds );
 * });
 *
 * // Clear on logout
 * tokenMgr.ClearToken( serverUrl );
 * @endcode
 */
class PLM_TOKEN_MANAGER
{
public:
    PLM_TOKEN_MANAGER() = default;
    ~PLM_TOKEN_MANAGER();

    // Non-copyable
    PLM_TOKEN_MANAGER( const PLM_TOKEN_MANAGER& ) = delete;
    PLM_TOKEN_MANAGER& operator=( const PLM_TOKEN_MANAGER& ) = delete;

    //-------------------------------------------------------------------------
    // Token Storage
    //-------------------------------------------------------------------------

    /**
     * Store a token for a server.
     *
     * Replaces any existing token for this server.
     *
     * @param aServerUrl Server URL.
     * @param aToken Session token.
     * @param aExpiresAt Expiry timestamp (Unix epoch seconds).
     */
    void StoreToken( const wxString& aServerUrl, const wxString& aToken, int64_t aExpiresAt );

    /**
     * Get the token for a server.
     *
     * If the token is near expiry and a refresh callback is registered,
     * this may trigger a proactive refresh in the background.
     *
     * @param aServerUrl Server URL.
     * @param aToken Output token value.
     * @return True if a valid token was found.
     */
    bool GetToken( const wxString& aServerUrl, wxString& aToken );

    /**
     * Check if a valid token exists for a server.
     *
     * @param aServerUrl Server URL.
     * @return True if valid token exists.
     */
    bool HasValidToken( const wxString& aServerUrl ) const;

    /**
     * Clear the token for a server.
     *
     * Call this on logout to ensure tokens are properly cleared.
     *
     * @param aServerUrl Server URL.
     */
    void ClearToken( const wxString& aServerUrl );

    /**
     * Clear all stored tokens.
     *
     * Call this on application exit or global logout.
     */
    void ClearAllTokens();

    //-------------------------------------------------------------------------
    // Token Information
    //-------------------------------------------------------------------------

    /**
     * Get remaining time until token expires.
     *
     * @param aServerUrl Server URL.
     * @return Seconds until expiry, or 0 if no token or expired.
     */
    int64_t GetRemainingTime( const wxString& aServerUrl ) const;

    /**
     * Get list of servers with stored tokens.
     *
     * @return Vector of server URLs.
     */
    std::vector<wxString> GetServersWithTokens() const;

    //-------------------------------------------------------------------------
    // Refresh Management
    //-------------------------------------------------------------------------

    /**
     * Set the callback for token refresh.
     *
     * The callback is called when a token needs to be refreshed.
     *
     * @param aCallback Refresh callback function.
     */
    void SetRefreshCallback( PLM_TOKEN_REFRESH_CALLBACK aCallback );

    /**
     * Set the callback for token expiry notifications.
     *
     * @param aCallback Expiry callback function.
     */
    void SetExpiryCallback( PLM_TOKEN_EXPIRY_CALLBACK aCallback );

    /**
     * Manually trigger a token refresh.
     *
     * @param aServerUrl Server URL.
     * @param aCredentials Credentials to use for refresh.
     * @return True if refresh was successful.
     */
    bool RefreshToken( const wxString& aServerUrl, const PLM_CREDENTIALS& aCredentials );

    /**
     * Check all tokens and refresh those that need it.
     *
     * Call this periodically (e.g., from a timer) to ensure tokens
     * are refreshed before they expire.
     *
     * @param aCredentialsGetter Function to get credentials for a server.
     */
    void CheckAndRefreshTokens( std::function<PLM_CREDENTIALS( const wxString& )> aCredentialsGetter );

private:
    /**
     * Normalize a server URL for consistent keys.
     */
    static wxString NormalizeUrl( const wxString& aUrl );

    mutable std::mutex                   m_mutex;           ///< Thread safety
    std::map<wxString, PLM_TOKEN_ENTRY>  m_tokens;          ///< Token storage
    PLM_TOKEN_REFRESH_CALLBACK           m_refreshCallback; ///< Refresh callback
    PLM_TOKEN_EXPIRY_CALLBACK            m_expiryCallback;  ///< Expiry notification
};


#endif // PLM_TOKEN_MANAGER_H
