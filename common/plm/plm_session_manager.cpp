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

#include "plm_session_manager.h"

#include <wx/log.h>


PLM_SESSION_MANAGER::PLM_SESSION_MANAGER()
{
}


PLM_SESSION_MANAGER::~PLM_SESSION_MANAGER()
{
    DisconnectAll();
}


wxString PLM_SESSION_MANAGER::NormalizeUrl( const wxString& aUrl )
{
    wxString normalized = aUrl.Lower();

    while( normalized.EndsWith( wxS( "/" ) ) )
        normalized.RemoveLast();

    return normalized;
}


void PLM_SESSION_MANAGER::FireEvent( const wxString& aServerUrl, PLM_SESSION_EVENT aEvent,
                                      const wxString& aMessage )
{
    if( m_eventCallback )
        m_eventCallback( aServerUrl, aEvent, aMessage );
}


PLM_AUTH_RESULT PLM_SESSION_MANAGER::Connect( const wxString& aServerUrl,
                                               const PLM_CREDENTIALS& aCredentials,
                                               PLM_AUTH_CALLBACK aAuthCallback )
{
    wxString key = NormalizeUrl( aServerUrl );

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        // Check if already connecting
        auto it = m_sessions.find( key );

        if( it != m_sessions.end() && it->second.state == PLM_SESSION_STATE::CONNECTING )
        {
            PLM_AUTH_RESULT result;
            result.success = false;
            result.error_message = wxS( "Connection already in progress" );
            result.error_code = PLM_ERROR_CODE::ERR_INVALID_PARAMETER;
            return result;
        }

        // Set state to connecting
        m_sessions[key].state = PLM_SESSION_STATE::CONNECTING;
        m_sessions[key].last_error.Clear();
    }

    wxLogMessage( wxS( "PLM: Connecting to %s" ), aServerUrl );

    // Perform authentication outside of lock
    PLM_AUTH_RESULT result = aAuthCallback( aServerUrl, aCredentials );

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        if( result.success )
        {
            // Store session
            PLM_SESSION session;
            session.server_url = aServerUrl;
            session.token = result.token;
            session.expires_at = result.expires_at;
            // User info is populated separately via UpdateUserInfo if needed

            m_sessions[key] = PLM_MANAGED_SESSION( session );

            // Store token in token manager
            m_tokenManager.StoreToken( aServerUrl, result.token.Get(), result.expires_at );

            wxLogMessage( wxS( "PLM: Connected to %s" ), aServerUrl );
        }
        else
        {
            // Set error state
            m_sessions[key].state = PLM_SESSION_STATE::ERROR;
            m_sessions[key].last_error = result.error_message;

            wxLogWarning( wxS( "PLM: Connection failed for %s: %s" ),
                          aServerUrl, result.error_message );
        }
    }

    // Fire event outside of lock
    if( result.success )
        FireEvent( aServerUrl, PLM_SESSION_EVENT::CONNECTED, wxEmptyString );
    else
        FireEvent( aServerUrl, PLM_SESSION_EVENT::CONNECTION_ERROR, result.error_message );

    return result;
}


void PLM_SESSION_MANAGER::Disconnect( const wxString& aServerUrl )
{
    wxString key = NormalizeUrl( aServerUrl );
    bool wasConnected = false;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_sessions.find( key );

        if( it != m_sessions.end() )
        {
            wasConnected = ( it->second.state == PLM_SESSION_STATE::CONNECTED );
            it->second.session.token.Clear();
            m_sessions.erase( it );
        }

        // Clear token
        m_tokenManager.ClearToken( aServerUrl );
    }

    if( wasConnected )
    {
        wxLogMessage( wxS( "PLM: Disconnected from %s" ), aServerUrl );
        FireEvent( aServerUrl, PLM_SESSION_EVENT::DISCONNECTED, wxEmptyString );
    }
}


void PLM_SESSION_MANAGER::DisconnectAll()
{
    std::vector<wxString> servers;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        for( const auto& [key, managed] : m_sessions )
        {
            if( managed.state == PLM_SESSION_STATE::CONNECTED )
                servers.push_back( managed.session.server_url );
        }
    }

    for( const wxString& server : servers )
        Disconnect( server );

    {
        std::lock_guard<std::mutex> lock( m_mutex );
        m_sessions.clear();
        m_tokenManager.ClearAllTokens();
    }

    wxLogMessage( wxS( "PLM: All sessions disconnected" ) );
}


bool PLM_SESSION_MANAGER::IsConnected( const wxString& aServerUrl ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizeUrl( aServerUrl );
    auto it = m_sessions.find( key );

    if( it == m_sessions.end() )
        return false;

    return it->second.state == PLM_SESSION_STATE::CONNECTED;
}


bool PLM_SESSION_MANAGER::GetSession( const wxString& aServerUrl, PLM_SESSION& aSession )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizeUrl( aServerUrl );
    auto it = m_sessions.find( key );

    if( it == m_sessions.end() )
        return false;

    if( it->second.state != PLM_SESSION_STATE::CONNECTED )
        return false;

    // Update activity
    it->second.RecordActivity();

    aSession = it->second.session;
    return true;
}


PLM_SESSION_STATE PLM_SESSION_MANAGER::GetSessionState( const wxString& aServerUrl ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizeUrl( aServerUrl );
    auto it = m_sessions.find( key );

    if( it == m_sessions.end() )
        return PLM_SESSION_STATE::DISCONNECTED;

    return it->second.state;
}


std::vector<wxString> PLM_SESSION_MANAGER::GetConnectedServers() const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    std::vector<wxString> servers;
    servers.reserve( m_sessions.size() );

    for( const auto& [key, managed] : m_sessions )
    {
        if( managed.state == PLM_SESSION_STATE::CONNECTED )
            servers.push_back( managed.session.server_url );
    }

    return servers;
}


void PLM_SESSION_MANAGER::RecordActivity( const wxString& aServerUrl )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizeUrl( aServerUrl );
    auto it = m_sessions.find( key );

    if( it != m_sessions.end() )
        it->second.RecordActivity();
}


void PLM_SESSION_MANAGER::CheckSessions()
{
    std::vector<std::pair<wxString, bool>> expiryEvents; // server URL, fully expired

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        int64_t now = std::time( nullptr );

        for( auto& [key, managed] : m_sessions )
        {
            if( managed.state != PLM_SESSION_STATE::CONNECTED )
                continue;

            int64_t remaining = managed.session.expires_at - now;

            if( remaining <= 0 )
            {
                // Session has expired
                managed.state = PLM_SESSION_STATE::EXPIRED;
                expiryEvents.emplace_back( managed.session.server_url, true );
            }
            else if( remaining <= 300 ) // 5 minutes
            {
                // Session is about to expire
                expiryEvents.emplace_back( managed.session.server_url, false );
            }
        }
    }

    // Fire events outside of lock
    for( const auto& [serverUrl, expired] : expiryEvents )
    {
        if( expired )
        {
            wxLogMessage( wxS( "PLM: Session expired for %s" ), serverUrl );
            FireEvent( serverUrl, PLM_SESSION_EVENT::SESSION_EXPIRED, wxEmptyString );
        }
        else
        {
            wxLogMessage( wxS( "PLM: Session expiring soon for %s" ), serverUrl );
            FireEvent( serverUrl, PLM_SESSION_EVENT::SESSION_EXPIRING, wxEmptyString );
        }
    }
}


bool PLM_SESSION_MANAGER::TryReauthenticate( const wxString& aServerUrl,
                                              PLM_AUTH_CALLBACK aAuthCallback )
{
    if( !m_reauthCallback )
    {
        wxLogWarning( wxS( "PLM: No re-auth callback registered" ) );
        return false;
    }

    PLM_CREDENTIALS credentials;

    if( !m_reauthCallback( aServerUrl, credentials ) )
    {
        wxLogMessage( wxS( "PLM: Re-authentication cancelled by user for %s" ), aServerUrl );
        return false;
    }

    PLM_AUTH_RESULT result = Connect( aServerUrl, credentials, aAuthCallback );
    return result.success;
}


void PLM_SESSION_MANAGER::SetEventCallback( PLM_SESSION_EVENT_CALLBACK aCallback )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_eventCallback = std::move( aCallback );
}


void PLM_SESSION_MANAGER::SetReauthCallback( PLM_REAUTH_PROMPT_CALLBACK aCallback )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_reauthCallback = std::move( aCallback );
}
