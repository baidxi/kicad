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
 * @file odoo_rpc_client.h
 * @brief JSON-RPC client for OdooPLM server communication.
 *
 * Implements the JSON-RPC 2.0 protocol used by Odoo's web services API.
 * Handles authentication, session management, and model operations.
 */

#ifndef ODOO_RPC_CLIENT_H
#define ODOO_RPC_CLIENT_H

#include <wx/string.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <optional>


/**
 * Result of a JSON-RPC call.
 *
 * Contains either the successful result or error information from the Odoo server.
 */
struct ODOO_RPC_RESULT
{
    bool                            success = false;
    nlohmann::json                  result;
    int                             error_code = 0;
    wxString                        error_message;
    wxString                        error_data;
};


/**
 * Session information from Odoo authentication.
 */
struct ODOO_SESSION
{
    wxString    session_id;
    int         uid = 0;
    wxString    username;
    wxString    partner_display_name;
    wxString    database;
    bool        is_authenticated = false;
};


/**
 * JSON-RPC client for OdooPLM integration.
 *
 * This class implements the JSON-RPC 2.0 protocol used by Odoo's web API.
 * It provides methods for authentication and model operations (search, read,
 * create, write, unlink).
 *
 * Usage:
 * @code
 *   OdooRpcClient client;
 *   client.SetServer( "https://mycompany.odoo.com", "production" );
 *
 *   auto authResult = client.Authenticate( "user@company.com", "password" );
 *   if( authResult.success )
 *   {
 *       auto result = client.SearchRead( "product.product",
 *                                         { { "active", "=", true } },
 *                                         { "name", "default_code" } );
 *   }
 * @endcode
 */
class OdooRpcClient
{
public:
    OdooRpcClient();
    ~OdooRpcClient();

    /**
     * Configure the Odoo server connection.
     * @param aServerUrl Base URL of the Odoo server (e.g., "https://mycompany.odoo.com").
     * @param aDatabase Name of the Odoo database to connect to.
     */
    void SetServer( const wxString& aServerUrl, const wxString& aDatabase );

    /**
     * Set connection timeout in seconds.
     * @param aTimeoutSeconds Timeout value, default is 30 seconds.
     */
    void SetTimeout( int aTimeoutSeconds );

    /**
     * Authenticate with Odoo using username and password.
     * @param aUsername User login (typically email).
     * @param aPassword User password or API key.
     * @return Session information on success, or error details on failure.
     */
    ODOO_RPC_RESULT Authenticate( const wxString& aUsername, const wxString& aPassword );

    /**
     * Authenticate with Odoo using an API key (Odoo 14+).
     * @param aUsername User login (typically email).
     * @param aApiKey API key generated in Odoo user preferences.
     * @return Session information on success, or error details on failure.
     */
    ODOO_RPC_RESULT AuthenticateWithApiKey( const wxString& aUsername, const wxString& aApiKey );

    /**
     * End the current session.
     * @return True if logout was successful.
     */
    bool Logout();

    /**
     * Check if currently authenticated.
     * @return True if a valid session exists.
     */
    bool IsAuthenticated() const;

    /**
     * Get the current session information.
     * @return Current session details.
     */
    const ODOO_SESSION& GetSession() const;

    /**
     * Execute a model method via JSON-RPC.
     * This is the general-purpose method for Odoo model operations.
     * @param aModel Model name (e.g., "product.product").
     * @param aMethod Method name (e.g., "search_read", "create", "write").
     * @param aArgs Positional arguments as JSON array.
     * @param aKwargs Keyword arguments as JSON object.
     * @return RPC result with method return value or error.
     */
    ODOO_RPC_RESULT CallMethod( const wxString&       aModel,
                                 const wxString&       aMethod,
                                 const nlohmann::json& aArgs = nlohmann::json::array(),
                                 const nlohmann::json& aKwargs = nlohmann::json::object() );

    /**
     * Search for records matching a domain filter.
     * @param aModel Model name.
     * @param aDomain Search domain as Odoo domain triplets.
     * @param aLimit Maximum number of records to return (0 = no limit).
     * @param aOffset Number of records to skip.
     * @return Array of matching record IDs.
     */
    ODOO_RPC_RESULT Search( const wxString&       aModel,
                             const nlohmann::json& aDomain,
                             int                   aLimit = 0,
                             int                   aOffset = 0 );

    /**
     * Read specific fields from records.
     * @param aModel Model name.
     * @param aIds Array of record IDs to read.
     * @param aFields Array of field names to retrieve.
     * @return Array of record dictionaries with requested fields.
     */
    ODOO_RPC_RESULT Read( const wxString&       aModel,
                           const nlohmann::json& aIds,
                           const nlohmann::json& aFields );

    /**
     * Search and read in a single call.
     * Combines search() and read() for efficiency.
     * @param aModel Model name.
     * @param aDomain Search domain.
     * @param aFields Fields to retrieve.
     * @param aLimit Maximum records.
     * @param aOffset Records to skip.
     * @param aOrder Sort order (e.g., "name asc, id desc").
     * @return Array of matching records with requested fields.
     */
    ODOO_RPC_RESULT SearchRead( const wxString&       aModel,
                                 const nlohmann::json& aDomain,
                                 const nlohmann::json& aFields,
                                 int                   aLimit = 0,
                                 int                   aOffset = 0,
                                 const wxString&       aOrder = wxEmptyString );

    /**
     * Create a new record.
     * @param aModel Model name.
     * @param aValues Field values for the new record.
     * @return ID of the created record.
     */
    ODOO_RPC_RESULT Create( const wxString& aModel, const nlohmann::json& aValues );

    /**
     * Update existing records.
     * @param aModel Model name.
     * @param aIds Array of record IDs to update.
     * @param aValues Field values to update.
     * @return True on success.
     */
    ODOO_RPC_RESULT Write( const wxString&       aModel,
                            const nlohmann::json& aIds,
                            const nlohmann::json& aValues );

    /**
     * Delete records.
     * @param aModel Model name.
     * @param aIds Array of record IDs to delete.
     * @return True on success.
     */
    ODOO_RPC_RESULT Unlink( const wxString& aModel, const nlohmann::json& aIds );

    /**
     * Get the last HTTP status code.
     * @return HTTP status code from last request.
     */
    int GetLastHttpStatus() const;

    /**
     * Get the last error message.
     * @return Error message from last failed operation.
     */
    wxString GetLastError() const;

    /**
     * Get attachments for a record.
     * @param aModel Model name (e.g., "product.product").
     * @param aRecordId Record ID to get attachments for.
     * @return Array of attachment records with id, name, mimetype, file_size.
     */
    ODOO_RPC_RESULT GetAttachments( const wxString& aModel, int aRecordId );

    /**
     * Download attachment data.
     * Retrieves the base64-encoded datas field for an attachment.
     * @param aAttachmentId Attachment ID.
     * @return Result containing base64-encoded data in result["datas"].
     */
    ODOO_RPC_RESULT DownloadAttachment( int aAttachmentId );

    /**
     * Upload a file as an attachment.
     * @param aModel Model name the attachment is linked to.
     * @param aRecordId Record ID the attachment is linked to.
     * @param aFilename Filename for the attachment.
     * @param aData Base64-encoded file data.
     * @return Result containing the created attachment ID.
     */
    ODOO_RPC_RESULT UploadAttachment( const wxString& aModel, int aRecordId,
                                       const wxString& aFilename, const wxString& aData );

private:
    /**
     * Send a JSON-RPC request to an endpoint.
     * @param aEndpoint URL path (e.g., "/web/session/authenticate").
     * @param aParams Request parameters.
     * @return Parsed response or error.
     */
    ODOO_RPC_RESULT SendRequest( const wxString& aEndpoint, const nlohmann::json& aParams );

    /**
     * Build a JSON-RPC 2.0 request envelope.
     */
    nlohmann::json BuildRpcRequest( const nlohmann::json& aParams );

    /**
     * Parse a JSON-RPC 2.0 response.
     */
    ODOO_RPC_RESULT ParseRpcResponse( const std::string& aResponse );

    wxString        m_serverUrl;
    wxString        m_database;
    ODOO_SESSION    m_session;
    int             m_timeoutSeconds;
    int             m_lastHttpStatus;
    wxString        m_lastError;
    int             m_requestId;
};

#endif // ODOO_RPC_CLIENT_H
