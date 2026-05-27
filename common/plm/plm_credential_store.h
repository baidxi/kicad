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
 * @file plm_credential_store.h
 * @brief Platform-agnostic interface for secure credential storage.
 *
 * This header defines the abstract interface for storing PLM credentials
 * in the system's secure storage (keychain/keyring/credential manager).
 * Platform-specific implementations are provided for Linux, Windows, and macOS.
 */

#ifndef PLM_CREDENTIAL_STORE_H
#define PLM_CREDENTIAL_STORE_H

#include "plm_auth.h"
#include "plm_types.h"

#include <wx/string.h>
#include <memory>
#include <vector>


/**
 * Result of a credential storage operation.
 *
 * Contains success/failure status and error information.
 */
struct PLM_CREDENTIAL_RESULT
{
    bool           success = false;
    PLM_ERROR_CODE error_code = PLM_ERROR_CODE::SUCCESS;
    wxString       error_message;

    PLM_CREDENTIAL_RESULT() = default;

    /**
     * Create a successful result.
     */
    static PLM_CREDENTIAL_RESULT Success()
    {
        PLM_CREDENTIAL_RESULT result;
        result.success = true;
        return result;
    }

    /**
     * Create a failure result.
     */
    static PLM_CREDENTIAL_RESULT Failure( PLM_ERROR_CODE aCode, const wxString& aMessage )
    {
        PLM_CREDENTIAL_RESULT result;
        result.success = false;
        result.error_code = aCode;
        result.error_message = aMessage;
        return result;
    }
};


/**
 * Abstract interface for secure credential storage.
 *
 * Platform-specific implementations use the native secure storage:
 * - Linux: libsecret (GNOME Keyring) or KWallet
 * - Windows: Windows Credential Manager
 * - macOS: Keychain Services
 *
 * Credentials are keyed by server URL, allowing multiple PLM server
 * credentials to be stored simultaneously.
 *
 * Usage:
 * @code
 * auto store = PLM_CREDENTIAL_STORE::Create();
 *
 * if( store && store->IsAvailable() )
 * {
 *     PLM_CREDENTIALS creds( "https://plm.example.com", "user", "pass" );
 *     auto result = store->StoreCredentials( creds );
 *
 *     if( result.success )
 *     {
 *         PLM_CREDENTIALS retrieved;
 *         if( store->RetrieveCredentials( "https://plm.example.com", retrieved ).success )
 *         {
 *             // Use retrieved credentials
 *         }
 *     }
 * }
 * @endcode
 */
class PLM_CREDENTIAL_STORE
{
public:
    virtual ~PLM_CREDENTIAL_STORE() = default;

    /**
     * Create a platform-appropriate credential store.
     *
     * Returns the appropriate implementation for the current platform.
     * May return nullptr if no secure storage is available.
     *
     * @return Unique pointer to credential store, or nullptr.
     */
    static std::unique_ptr<PLM_CREDENTIAL_STORE> Create();

    /**
     * Check if secure credential storage is available.
     *
     * Returns true if the underlying secure storage system is
     * available and accessible. Even if this returns false,
     * operations may still be attempted (they will fail gracefully).
     *
     * @return True if secure storage is available.
     */
    virtual bool IsAvailable() const = 0;

    /**
     * Get a human-readable name for the storage backend.
     *
     * Returns a description of the underlying storage system,
     * useful for diagnostics and user feedback.
     *
     * @return Storage backend name (e.g., "GNOME Keyring", "Windows Credential Manager").
     */
    virtual wxString GetBackendName() const = 0;

    //-------------------------------------------------------------------------
    // Credential Operations
    //-------------------------------------------------------------------------

    /**
     * Store credentials for a PLM server.
     *
     * If credentials already exist for this server URL, they are replaced.
     * The password is stored securely in the system's credential storage.
     *
     * @param aCredentials Credentials to store.
     * @return Result indicating success or failure.
     */
    virtual PLM_CREDENTIAL_RESULT StoreCredentials( const PLM_CREDENTIALS& aCredentials ) = 0;

    /**
     * Retrieve credentials for a PLM server.
     *
     * Looks up credentials by server URL and populates the output structure.
     * The password is retrieved from secure storage.
     *
     * @param aServerUrl Server URL to look up.
     * @param aCredentials Output credentials structure.
     * @return Result indicating success or failure.
     */
    virtual PLM_CREDENTIAL_RESULT RetrieveCredentials( const wxString& aServerUrl,
                                                       PLM_CREDENTIALS& aCredentials ) = 0;

    /**
     * Delete credentials for a PLM server.
     *
     * Removes the stored credentials from secure storage.
     * Returns success even if no credentials existed.
     *
     * @param aServerUrl Server URL to delete credentials for.
     * @return Result indicating success or failure.
     */
    virtual PLM_CREDENTIAL_RESULT DeleteCredentials( const wxString& aServerUrl ) = 0;

    /**
     * Check if credentials exist for a PLM server.
     *
     * Performs a lightweight check without retrieving the actual credentials.
     *
     * @param aServerUrl Server URL to check.
     * @return True if credentials exist for this server.
     */
    virtual bool HasCredentials( const wxString& aServerUrl ) = 0;

    /**
     * Get list of all stored server URLs.
     *
     * Returns the server URLs for all stored credentials.
     * Useful for presenting a list of saved servers to the user.
     *
     * @return Vector of server URLs with stored credentials.
     */
    virtual std::vector<wxString> GetStoredServers() = 0;

    /**
     * Delete all stored PLM credentials.
     *
     * Removes all credentials from secure storage. Use with caution.
     *
     * @return Result indicating success or failure.
     */
    virtual PLM_CREDENTIAL_RESULT DeleteAllCredentials() = 0;

protected:
    PLM_CREDENTIAL_STORE() = default;

    // Non-copyable
    PLM_CREDENTIAL_STORE( const PLM_CREDENTIAL_STORE& ) = delete;
    PLM_CREDENTIAL_STORE& operator=( const PLM_CREDENTIAL_STORE& ) = delete;

    /**
     * Normalize a server URL for consistent storage keys.
     *
     * Removes trailing slashes and converts to lowercase for
     * consistent credential lookup.
     *
     * @param aServerUrl Server URL to normalize.
     * @return Normalized server URL.
     */
    static wxString NormalizeServerUrl( const wxString& aServerUrl )
    {
        wxString normalized = aServerUrl.Lower();

        while( normalized.EndsWith( wxS( "/" ) ) )
            normalized.RemoveLast();

        return normalized;
    }

    /**
     * Service name used for credential storage.
     *
     * All PLM credentials are stored under this service name
     * to group them together and distinguish from other applications.
     */
    static constexpr const char* SERVICE_NAME = "kicad-plm";
};


#endif // PLM_CREDENTIAL_STORE_H
