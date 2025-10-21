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
 * @file plm_session_manager.h
 * @brief Session management for PLM connections.
 *
 * This header defines the PLM_SESSION_MANAGER class which tracks session state
 * for connected PLM servers, handles session expiry, and provides events for
 * UI updates. It works with PLM_TOKEN_MANAGER for token lifecycle and
 * PLM_CREDENTIAL_STORE for credential management.
 */

#ifndef PLM_SESSION_MANAGER_H
#define PLM_SESSION_MANAGER_H

#include "plm_auth.h"
#include "plm_credential_store.h"
#include "plm_token_manager.h"
#include "plm_types.h"

#include <wx/string.h>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>


/**
 * Session state for a PLM server connection.
 */
enum class PLM_SESSION_STATE
{
    DISCONNECTED,     ///< Not connected to server
    CONNECTING,       ///< Authentication in progress
    CONNECTED,        ///< Successfully authenticated
    EXPIRED,          ///< Session has expired
    ERROR             ///< Connection error occurred
};


/**
 * Event types for session state changes.
 */
enum class PLM_SESSION_EVENT
{
    CONNECTED,           ///< Successfully connected to server
    DISCONNECTED,        ///< Disconnected from server (logout or error)
    SESSION_EXPIRING,    ///< Session about to expire, refresh needed
    SESSION_EXPIRED,     ///< Session has expired, re-auth needed
    USER_INFO_UPDATED,   ///< User information was updated
    CONNECTION_ERROR     ///< Connection error occurred
};


/**
 * Callback type for session events.
 *
 * @param aServerUrl Server URL the event is for.
 * @param aEvent Event type.
 * @param aMessage Optional message (e.g., error details).
 */
using PLM_SESSION_EVENT_CALLBACK = std::function<void( const wxString& aServerUrl,
                                                        PLM_SESSION_EVENT aEvent,
                                                        const wxString& aMessage )>;

/**
 * Callback type for re-authentication prompts.
 *
 * Called when session expires and credentials are needed for re-authentication.
 * Return true if user provided credentials, false to cancel.
 *
 * @param aServerUrl Server URL needing re-authentication.
 * @param aCredentials Output credentials structure to populate.
 * @return True if credentials were provided.
 */
using PLM_REAUTH_PROMPT_CALLBACK = std::function<bool( const wxString& aServerUrl,
                                                        PLM_CREDENTIALS& aCredentials )>;

/**
 * Callback type for authentication requests.
 *
 * Called to perform authentication with the PLM server.
 *
 * @param aServerUrl Server URL to authenticate with.
 * @param aCredentials Credentials to use.
 * @return Authentication result.
 */
using PLM_AUTH_CALLBACK = std::function<PLM_AUTH_RESULT( const wxString& aServerUrl,
                                                          const PLM_CREDENTIALS& aCredentials )>;


/**
 * Managed session entry with state tracking.
 */
struct PLM_MANAGED_SESSION
{
    PLM_SESSION       session;              ///< Session data
    PLM_SESSION_STATE state = PLM_SESSION_STATE::DISCONNECTED;
    wxString          last_error;           ///< Last error message if state is ERROR
    int64_t           connected_at = 0;     ///< When session was established
    int64_t           last_activity = 0;    ///< Last API activity timestamp

    PLM_MANAGED_SESSION() = default;

    explicit PLM_MANAGED_SESSION( const PLM_SESSION& aSession ) :
            session( aSession ),
            state( PLM_SESSION_STATE::CONNECTED ),
            connected_at( std::time( nullptr ) ),
            last_activity( std::time( nullptr ) )
    {
    }

    /**
     * Update last activity timestamp.
     */
    void RecordActivity()
    {
        last_activity = std::time( nullptr );
    }

    /**
     * Get session duration in seconds.
     */
    int64_t GetSessionDuration() const
    {
        if( connected_at == 0 )
            return 0;

        return std::time( nullptr ) - connected_at;
    }
};


/**
 * Manages PLM server sessions.
 *
 * Provides centralized session management including:
 * - Session state tracking per server
 * - Session expiry handling
 * - Re-authentication prompts
 * - Event notifications for UI updates
 *
 * Thread-safe: All public methods are protected by mutex.
 *
 * Usage:
 * @code
 * PLM_SESSION_MANAGER sessionMgr;
 *
 * // Set up callbacks
 * sessionMgr.SetEventCallback( []( const wxString& url, PLM_SESSION_EVENT event, const wxString& msg ) {
 *     // Handle session events (update UI, show notifications, etc.)
 * });
 *
 * sessionMgr.SetReauthCallback( []( const wxString& url, PLM_CREDENTIALS& creds ) {
 *     // Prompt user for credentials
 *     return ShowLoginDialog( url, creds );
 * });
 *
 * // Connect to server
 * PLM_AUTH_RESULT result = sessionMgr.Connect( serverUrl, credentials, authCallback );
 *
 * if( result.success )
 * {
 *     // Use session
 *     PLM_SESSION session;
 *     if( sessionMgr.GetSession( serverUrl, session ) )
 *     {
 *         // Make API calls using session.token
 *     }
 * }
 *
 * // Disconnect when done
 * sessionMgr.Disconnect( serverUrl );
 * @endcode
 */
class PLM_SESSION_MANAGER
{
public:
    PLM_SESSION_MANAGER();
    ~PLM_SESSION_MANAGER();

    // Non-copyable
    PLM_SESSION_MANAGER( const PLM_SESSION_MANAGER& ) = delete;
    PLM_SESSION_MANAGER& operator=( const PLM_SESSION_MANAGER& ) = delete;

    //-------------------------------------------------------------------------
    // Connection Management
    //-------------------------------------------------------------------------

    /**
     * Connect to a PLM server.
     *
     * Authenticates with the server and establishes a session.
     *
     * @param aServerUrl Server URL.
     * @param aCredentials Credentials to use.
     * @param aAuthCallback Callback to perform authentication.
     * @return Authentication result.
     */
    PLM_AUTH_RESULT Connect( const wxString& aServerUrl, const PLM_CREDENTIALS& aCredentials,
                             PLM_AUTH_CALLBACK aAuthCallback );

    /**
     * Disconnect from a PLM server.
     *
     * Clears the session and notifies listeners.
     *
     * @param aServerUrl Server URL.
     */
    void Disconnect( const wxString& aServerUrl );

    /**
     * Disconnect from all servers.
     */
    void DisconnectAll();

    /**
     * Check if connected to a server.
     *
     * @param aServerUrl Server URL.
     * @return True if connected with valid session.
     */
    bool IsConnected( const wxString& aServerUrl ) const;

    //-------------------------------------------------------------------------
    // Session Access
    //-------------------------------------------------------------------------

    /**
     * Get session for a server.
     *
     * @param aServerUrl Server URL.
     * @param aSession Output session structure.
     * @return True if session was found and is valid.
     */
    bool GetSession( const wxString& aServerUrl, PLM_SESSION& aSession );

    /**
     * Get session state for a server.
     *
     * @param aServerUrl Server URL.
     * @return Session state, or DISCONNECTED if not found.
     */
    PLM_SESSION_STATE GetSessionState( const wxString& aServerUrl ) const;

    /**
     * Get list of connected servers.
     *
     * @return Vector of server URLs with active sessions.
     */
    std::vector<wxString> GetConnectedServers() const;

    /**
     * Record API activity for a session.
     *
     * Updates the last activity timestamp for the session.
     *
     * @param aServerUrl Server URL.
     */
    void RecordActivity( const wxString& aServerUrl );

    //-------------------------------------------------------------------------
    // Session Monitoring
    //-------------------------------------------------------------------------

    /**
     * Check sessions and handle expiry.
     *
     * Call this periodically (e.g., from a timer) to check for expiring
     * sessions and trigger re-authentication if needed.
     */
    void CheckSessions();

    /**
     * Attempt to re-authenticate an expired session.
     *
     * Uses the re-auth callback to prompt for credentials.
     *
     * @param aServerUrl Server URL.
     * @param aAuthCallback Callback to perform authentication.
     * @return True if re-authentication succeeded.
     */
    bool TryReauthenticate( const wxString& aServerUrl, PLM_AUTH_CALLBACK aAuthCallback );

    //-------------------------------------------------------------------------
    // Callbacks
    //-------------------------------------------------------------------------

    /**
     * Set the callback for session events.
     *
     * @param aCallback Event callback function.
     */
    void SetEventCallback( PLM_SESSION_EVENT_CALLBACK aCallback );

    /**
     * Set the callback for re-authentication prompts.
     *
     * @param aCallback Re-auth prompt callback function.
     */
    void SetReauthCallback( PLM_REAUTH_PROMPT_CALLBACK aCallback );

    //-------------------------------------------------------------------------
    // Token Manager Integration
    //-------------------------------------------------------------------------

    /**
     * Get the token manager.
     *
     * @return Reference to the token manager.
     */
    PLM_TOKEN_MANAGER& GetTokenManager() { return m_tokenManager; }

private:
    /**
     * Normalize a server URL for consistent keys.
     */
    static wxString NormalizeUrl( const wxString& aUrl );

    /**
     * Fire a session event.
     */
    void FireEvent( const wxString& aServerUrl, PLM_SESSION_EVENT aEvent,
                    const wxString& aMessage = wxEmptyString );

    mutable std::mutex                         m_mutex;
    std::map<wxString, PLM_MANAGED_SESSION>    m_sessions;
    PLM_TOKEN_MANAGER                          m_tokenManager;
    PLM_SESSION_EVENT_CALLBACK                 m_eventCallback;
    PLM_REAUTH_PROMPT_CALLBACK                 m_reauthCallback;
};


#endif // PLM_SESSION_MANAGER_H
