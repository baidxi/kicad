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

#include "plm_token_manager.h"

#include <wx/log.h>


PLM_TOKEN_MANAGER::~PLM_TOKEN_MANAGER()
{
    ClearAllTokens();
}


wxString PLM_TOKEN_MANAGER::NormalizeUrl( const wxString& aUrl )
{
    wxString normalized = aUrl.Lower();

    while( normalized.EndsWith( wxS( "/" ) ) )
        normalized.RemoveLast();

    return normalized;
}


void PLM_TOKEN_MANAGER::StoreToken( const wxString& aServerUrl, const wxString& aToken,
                                    int64_t aExpiresAt )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizeUrl( aServerUrl );
    m_tokens[key] = PLM_TOKEN_ENTRY( aToken, aExpiresAt, aServerUrl );

    wxLogMessage( wxS( "PLM: Token stored for %s, expires in %lld seconds" ),
                  aServerUrl, m_tokens[key].GetRemainingTime() );
}


bool PLM_TOKEN_MANAGER::GetToken( const wxString& aServerUrl, wxString& aToken )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizeUrl( aServerUrl );
    auto it = m_tokens.find( key );

    if( it == m_tokens.end() )
        return false;

    PLM_TOKEN_ENTRY& entry = it->second;

    // Check if expired
    if( entry.IsExpired() )
    {
        wxLogMessage( wxS( "PLM: Token expired for %s" ), aServerUrl );

        if( m_expiryCallback )
            m_expiryCallback( aServerUrl, true );

        return false;
    }

    // Check if needs proactive refresh
    if( entry.NeedsRefresh() && m_refreshCallback )
    {
        wxLogMessage( wxS( "PLM: Token needs refresh for %s, %lld seconds remaining" ),
                      aServerUrl, entry.GetRemainingTime() );

        if( m_expiryCallback )
            m_expiryCallback( aServerUrl, false );
    }

    aToken = entry.token.Get();
    return true;
}


bool PLM_TOKEN_MANAGER::HasValidToken( const wxString& aServerUrl ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizeUrl( aServerUrl );
    auto it = m_tokens.find( key );

    if( it == m_tokens.end() )
        return false;

    return !it->second.IsExpired();
}


void PLM_TOKEN_MANAGER::ClearToken( const wxString& aServerUrl )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizeUrl( aServerUrl );
    auto it = m_tokens.find( key );

    if( it != m_tokens.end() )
    {
        it->second.Clear();
        m_tokens.erase( it );
        wxLogMessage( wxS( "PLM: Token cleared for %s" ), aServerUrl );
    }
}


void PLM_TOKEN_MANAGER::ClearAllTokens()
{
    std::lock_guard<std::mutex> lock( m_mutex );

    for( auto& [key, entry] : m_tokens )
        entry.Clear();

    m_tokens.clear();

    wxLogMessage( wxS( "PLM: All tokens cleared" ) );
}


int64_t PLM_TOKEN_MANAGER::GetRemainingTime( const wxString& aServerUrl ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizeUrl( aServerUrl );
    auto it = m_tokens.find( key );

    if( it == m_tokens.end() )
        return 0;

    return it->second.GetRemainingTime();
}


std::vector<wxString> PLM_TOKEN_MANAGER::GetServersWithTokens() const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    std::vector<wxString> servers;
    servers.reserve( m_tokens.size() );

    for( const auto& [key, entry] : m_tokens )
    {
        if( !entry.IsExpired() )
            servers.push_back( entry.server_url );
    }

    return servers;
}


void PLM_TOKEN_MANAGER::SetRefreshCallback( PLM_TOKEN_REFRESH_CALLBACK aCallback )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_refreshCallback = std::move( aCallback );
}


void PLM_TOKEN_MANAGER::SetExpiryCallback( PLM_TOKEN_EXPIRY_CALLBACK aCallback )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_expiryCallback = std::move( aCallback );
}


bool PLM_TOKEN_MANAGER::RefreshToken( const wxString& aServerUrl, const PLM_CREDENTIALS& aCredentials )
{
    PLM_TOKEN_REFRESH_CALLBACK callback;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        if( !m_refreshCallback )
        {
            wxLogWarning( wxS( "PLM: No refresh callback registered" ) );
            return false;
        }

        wxString key = NormalizeUrl( aServerUrl );
        auto it = m_tokens.find( key );

        if( it != m_tokens.end() )
        {
            // Mark as refreshing to prevent duplicate refresh attempts
            it->second.is_refreshing = true;
        }

        callback = m_refreshCallback;
    }

    // Call refresh outside of lock to avoid deadlock
    wxLogMessage( wxS( "PLM: Refreshing token for %s" ), aServerUrl );
    PLM_AUTH_RESULT result = callback( aServerUrl, aCredentials );

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        wxString key = NormalizeUrl( aServerUrl );
        auto it = m_tokens.find( key );

        if( result.success )
        {
            // Store new token
            m_tokens[key] = PLM_TOKEN_ENTRY( result.token.Get(), result.expires_at, aServerUrl );
            wxLogMessage( wxS( "PLM: Token refreshed for %s, expires in %lld seconds" ),
                          aServerUrl, m_tokens[key].GetRemainingTime() );
            return true;
        }
        else
        {
            // Clear the refreshing flag on failure
            if( it != m_tokens.end() )
                it->second.is_refreshing = false;

            wxLogWarning( wxS( "PLM: Token refresh failed for %s: %s" ),
                          aServerUrl, result.error_message );
            return false;
        }
    }
}


void PLM_TOKEN_MANAGER::CheckAndRefreshTokens(
        std::function<PLM_CREDENTIALS( const wxString& )> aCredentialsGetter )
{
    std::vector<std::pair<wxString, PLM_CREDENTIALS>> toRefresh;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        if( !m_refreshCallback )
            return;

        for( auto& [key, entry] : m_tokens )
        {
            if( entry.NeedsRefresh() && !entry.is_refreshing )
            {
                PLM_CREDENTIALS creds = aCredentialsGetter( entry.server_url );
                toRefresh.emplace_back( entry.server_url, creds );
                entry.is_refreshing = true;
            }
        }
    }

    // Refresh outside of lock
    for( const auto& [serverUrl, creds] : toRefresh )
        RefreshToken( serverUrl, creds );
}
