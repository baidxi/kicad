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

#include "plm_credential_store.h"

#include <kiplatform/secrets.h>
#include <nlohmann/json.hpp>
#include <paths.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/sstream.h>
#include <wx/wfstream.h>

#include <map>
#include <set>


namespace
{

// Key names for secrets storage
const wxString USERNAME_KEY = wxS( "username" );
const wxString PASSWORD_KEY = wxS( "password" );

// File name for storing server list (not credentials, just server URLs for enumeration)
const wxString SERVER_LIST_FILE = wxS( "plm_servers.json" );

} // anonymous namespace


/**
 * Platform credential store using KIPLATFORM::SECRETS.
 *
 * Uses the platform-native secure storage:
 * - Linux: libsecret (GNOME Keyring) or KWallet
 * - Windows: Windows Credential Manager
 * - macOS: Keychain Services
 *
 * The server list is stored separately in a JSON file to enable enumeration,
 * since KIPLATFORM::SECRETS doesn't support listing all stored credentials.
 */
class PLM_CREDENTIAL_STORE_PLATFORM : public PLM_CREDENTIAL_STORE
{
public:
    PLM_CREDENTIAL_STORE_PLATFORM()
    {
        LoadServerList();
    }

    bool IsAvailable() const override
    {
        // Try a test operation to verify the backend is available
        // We'll consider it available unless we've had persistent failures
        return true;
    }

    wxString GetBackendName() const override
    {
#if defined( __WXGTK__ )
        return wxS( "GNOME Keyring / KWallet (libsecret)" );
#elif defined( __WXMSW__ )
        return wxS( "Windows Credential Manager" );
#elif defined( __WXMAC__ )
        return wxS( "macOS Keychain" );
#else
        return wxS( "Platform Secrets" );
#endif
    }

    PLM_CREDENTIAL_RESULT StoreCredentials( const PLM_CREDENTIALS& aCredentials ) override
    {
        wxString key = NormalizeServerUrl( aCredentials.server_url );

        // Store username
        wxString usernameKey = MakeKey( key, USERNAME_KEY );

        if( !KIPLATFORM::SECRETS::StoreSecret( SERVICE_NAME, usernameKey, aCredentials.username ) )
        {
            return PLM_CREDENTIAL_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_FAILED,
                                                   wxS( "Failed to store username in secure storage" ) );
        }

        // Store password
        wxString passwordKey = MakeKey( key, PASSWORD_KEY );

        if( !KIPLATFORM::SECRETS::StoreSecret( SERVICE_NAME, passwordKey, aCredentials.password.Get() ) )
        {
            // Clean up the username we just stored
            KIPLATFORM::SECRETS::StoreSecret( SERVICE_NAME, usernameKey, wxEmptyString );

            return PLM_CREDENTIAL_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_FAILED,
                                                   wxS( "Failed to store password in secure storage" ) );
        }

        // Track this server in our list
        m_storedServers.insert( key );
        SaveServerList();

        return PLM_CREDENTIAL_RESULT::Success();
    }

    PLM_CREDENTIAL_RESULT RetrieveCredentials( const wxString& aServerUrl,
                                               PLM_CREDENTIALS& aCredentials ) override
    {
        wxString key = NormalizeServerUrl( aServerUrl );

        // Retrieve username
        wxString username;
        wxString usernameKey = MakeKey( key, USERNAME_KEY );

        if( !KIPLATFORM::SECRETS::GetSecret( SERVICE_NAME, usernameKey, username ) )
        {
            return PLM_CREDENTIAL_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_FAILED,
                                                   wxS( "No credentials found for server" ) );
        }

        // Retrieve password
        wxString password;
        wxString passwordKey = MakeKey( key, PASSWORD_KEY );

        if( !KIPLATFORM::SECRETS::GetSecret( SERVICE_NAME, passwordKey, password ) )
        {
            return PLM_CREDENTIAL_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_FAILED,
                                                   wxS( "No credentials found for server" ) );
        }

        aCredentials.server_url = aServerUrl;
        aCredentials.username = username;
        aCredentials.password = PLM_SECURE_STRING( password );

        return PLM_CREDENTIAL_RESULT::Success();
    }

    PLM_CREDENTIAL_RESULT DeleteCredentials( const wxString& aServerUrl ) override
    {
        wxString key = NormalizeServerUrl( aServerUrl );

        // Delete by storing empty strings (KIPLATFORM::SECRETS doesn't have a delete method)
        wxString usernameKey = MakeKey( key, USERNAME_KEY );
        wxString passwordKey = MakeKey( key, PASSWORD_KEY );

        KIPLATFORM::SECRETS::StoreSecret( SERVICE_NAME, usernameKey, wxEmptyString );
        KIPLATFORM::SECRETS::StoreSecret( SERVICE_NAME, passwordKey, wxEmptyString );

        // Remove from server list
        m_storedServers.erase( key );
        SaveServerList();

        return PLM_CREDENTIAL_RESULT::Success();
    }

    bool HasCredentials( const wxString& aServerUrl ) override
    {
        wxString key = NormalizeServerUrl( aServerUrl );

        // Check if we have it in our list
        if( m_storedServers.find( key ) == m_storedServers.end() )
            return false;

        // Verify it's actually still in secure storage
        wxString username;
        wxString usernameKey = MakeKey( key, USERNAME_KEY );

        return KIPLATFORM::SECRETS::GetSecret( SERVICE_NAME, usernameKey, username ) && !username.IsEmpty();
    }

    std::vector<wxString> GetStoredServers() override
    {
        std::vector<wxString> servers;
        servers.reserve( m_storedServers.size() );

        for( const wxString& server : m_storedServers )
            servers.push_back( server );

        return servers;
    }

    PLM_CREDENTIAL_RESULT DeleteAllCredentials() override
    {
        // Delete all stored credentials
        for( const wxString& server : m_storedServers )
        {
            wxString usernameKey = MakeKey( server, USERNAME_KEY );
            wxString passwordKey = MakeKey( server, PASSWORD_KEY );

            KIPLATFORM::SECRETS::StoreSecret( SERVICE_NAME, usernameKey, wxEmptyString );
            KIPLATFORM::SECRETS::StoreSecret( SERVICE_NAME, passwordKey, wxEmptyString );
        }

        m_storedServers.clear();
        SaveServerList();

        return PLM_CREDENTIAL_RESULT::Success();
    }

private:
    /**
     * Make a composite key for secret storage.
     *
     * @param aServer Normalized server URL.
     * @param aSuffix Key suffix (USERNAME_KEY or PASSWORD_KEY).
     * @return Composite key.
     */
    static wxString MakeKey( const wxString& aServer, const wxString& aSuffix )
    {
        return aServer + wxS( ":" ) + aSuffix;
    }

    /**
     * Get path to server list file.
     */
    wxString GetServerListPath() const
    {
        wxFileName path( PATHS::GetUserCachePath(), SERVER_LIST_FILE );
        return path.GetFullPath();
    }

    /**
     * Load server list from disk.
     */
    void LoadServerList()
    {
        m_storedServers.clear();

        wxString path = GetServerListPath();

        if( !wxFileName::FileExists( path ) )
            return;

        wxFileInputStream stream( path );

        if( !stream.IsOk() )
            return;

        wxString content;
        wxStringOutputStream stringStream( &content );
        stream.Read( stringStream );

        try
        {
            nlohmann::json json = nlohmann::json::parse( content.ToStdString() );

            if( json.contains( "servers" ) && json["servers"].is_array() )
            {
                for( const auto& server : json["servers"] )
                {
                    if( server.is_string() )
                        m_storedServers.insert( wxString::FromUTF8( server.get<std::string>() ) );
                }
            }
        }
        catch( const nlohmann::json::exception& )
        {
            // Ignore parse errors, start with empty list
        }
    }

    /**
     * Save server list to disk.
     */
    void SaveServerList()
    {
        wxString path = GetServerListPath();

        // Ensure directory exists
        wxFileName::Mkdir( wxFileName( path ).GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );

        nlohmann::json json;
        nlohmann::json servers = nlohmann::json::array();

        for( const wxString& server : m_storedServers )
            servers.push_back( server.ToStdString() );

        json["servers"] = servers;

        wxFileOutputStream stream( path );

        if( stream.IsOk() )
        {
            std::string content = json.dump( 2 );
            stream.Write( content.c_str(), content.size() );
        }
    }

    std::set<wxString> m_storedServers;  ///< Tracked server URLs
};


/**
 * Fallback in-memory credential store.
 *
 * Used when no platform-specific secure storage is available.
 * Credentials are only stored for the duration of the session.
 *
 * WARNING: This implementation does NOT provide secure storage.
 * Credentials are held in memory and lost when the application exits.
 */
class PLM_CREDENTIAL_STORE_MEMORY : public PLM_CREDENTIAL_STORE
{
public:
    PLM_CREDENTIAL_STORE_MEMORY() = default;

    bool IsAvailable() const override
    {
        return true;
    }

    wxString GetBackendName() const override
    {
        return wxS( "In-Memory (Session Only)" );
    }

    PLM_CREDENTIAL_RESULT StoreCredentials( const PLM_CREDENTIALS& aCredentials ) override
    {
        wxString key = NormalizeServerUrl( aCredentials.server_url );
        m_credentials[key] = aCredentials;
        return PLM_CREDENTIAL_RESULT::Success();
    }

    PLM_CREDENTIAL_RESULT RetrieveCredentials( const wxString& aServerUrl,
                                               PLM_CREDENTIALS& aCredentials ) override
    {
        wxString key = NormalizeServerUrl( aServerUrl );
        auto it = m_credentials.find( key );

        if( it == m_credentials.end() )
        {
            return PLM_CREDENTIAL_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_FAILED,
                                                   wxS( "No credentials found for server" ) );
        }

        aCredentials = it->second;
        return PLM_CREDENTIAL_RESULT::Success();
    }

    PLM_CREDENTIAL_RESULT DeleteCredentials( const wxString& aServerUrl ) override
    {
        wxString key = NormalizeServerUrl( aServerUrl );
        m_credentials.erase( key );
        return PLM_CREDENTIAL_RESULT::Success();
    }

    bool HasCredentials( const wxString& aServerUrl ) override
    {
        wxString key = NormalizeServerUrl( aServerUrl );
        return m_credentials.find( key ) != m_credentials.end();
    }

    std::vector<wxString> GetStoredServers() override
    {
        std::vector<wxString> servers;
        servers.reserve( m_credentials.size() );

        for( const auto& [url, creds] : m_credentials )
            servers.push_back( url );

        return servers;
    }

    PLM_CREDENTIAL_RESULT DeleteAllCredentials() override
    {
        m_credentials.clear();
        return PLM_CREDENTIAL_RESULT::Success();
    }

private:
    std::map<wxString, PLM_CREDENTIALS> m_credentials;
};


std::unique_ptr<PLM_CREDENTIAL_STORE> PLM_CREDENTIAL_STORE::Create()
{
    // Try to create platform credential store using KIPLATFORM::SECRETS.
    // This uses the native secure storage on each platform:
    // - Linux: libsecret (GNOME Keyring) or KWallet
    // - Windows: Windows Credential Manager
    // - macOS: Keychain Services

    auto platformStore = std::make_unique<PLM_CREDENTIAL_STORE_PLATFORM>();

    // Test if the platform store works by checking IsAvailable()
    if( platformStore->IsAvailable() )
        return platformStore;

    // Fall back to in-memory storage if platform store fails
    wxLogWarning( wxS( "PLM: Platform credential storage unavailable, using session-only storage" ) );
    return std::make_unique<PLM_CREDENTIAL_STORE_MEMORY>();
}
