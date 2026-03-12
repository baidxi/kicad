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

#include "odoo_rpc_client.h"

#include <kicad_curl/kicad_curl_easy.h>
#include <wx/log.h>


static const char* const traceOdooRpc = "KICAD_ODOO_RPC";


OdooRpcClient::OdooRpcClient() :
        m_timeoutSeconds( 30 ),
        m_lastHttpStatus( 0 ),
        m_requestId( 0 )
{
}


OdooRpcClient::~OdooRpcClient()
{
    if( m_session.is_authenticated )
        Logout();
}


void OdooRpcClient::SetServer( const wxString& aServerUrl, const wxString& aDatabase )
{
    m_serverUrl = aServerUrl;
    m_database = aDatabase;

    if( m_serverUrl.EndsWith( wxS( "/" ) ) )
        m_serverUrl.RemoveLast();
}


void OdooRpcClient::SetTimeout( int aTimeoutSeconds )
{
    m_timeoutSeconds = aTimeoutSeconds;
}


ODOO_RPC_RESULT OdooRpcClient::Authenticate( const wxString& aUsername, const wxString& aPassword )
{
    nlohmann::json params;
    params["db"] = m_database.ToStdString();
    params["login"] = aUsername.ToStdString();
    params["password"] = aPassword.ToStdString();

    ODOO_RPC_RESULT result = SendRequest( wxS( "/web/session/authenticate" ), params );

    if( result.success && result.result.is_object() )
    {
        if( result.result.contains( "uid" ) && result.result["uid"].is_number() )
        {
            m_session.uid = result.result["uid"].get<int>();
            m_session.username = aUsername;
            m_session.database = m_database;
            m_session.is_authenticated = true;

            if( result.result.contains( "partner_display_name" ) )
                m_session.partner_display_name = wxString::FromUTF8(
                        result.result["partner_display_name"].get<std::string>() );
        }
        else if( result.result.contains( "uid" ) && result.result["uid"].is_boolean() &&
                 !result.result["uid"].get<bool>() )
        {
            result.success = false;
            result.error_code = 401;
            result.error_message = wxS( "Invalid username or password" );
        }
    }

    return result;
}


ODOO_RPC_RESULT OdooRpcClient::AuthenticateWithApiKey( const wxString& aUsername,
                                                        const wxString& aApiKey )
{
    return Authenticate( aUsername, aApiKey );
}


bool OdooRpcClient::Logout()
{
    if( !m_session.is_authenticated )
        return true;

    ODOO_RPC_RESULT result = SendRequest( wxS( "/web/session/destroy" ), nlohmann::json::object() );

    m_session = ODOO_SESSION();

    return result.success;
}


bool OdooRpcClient::IsAuthenticated() const
{
    return m_session.is_authenticated;
}


const ODOO_SESSION& OdooRpcClient::GetSession() const
{
    return m_session;
}


ODOO_RPC_RESULT OdooRpcClient::CallMethod( const wxString&       aModel,
                                            const wxString&       aMethod,
                                            const nlohmann::json& aArgs,
                                            const nlohmann::json& aKwargs )
{
    if( !m_session.is_authenticated )
    {
        ODOO_RPC_RESULT result;
        result.success = false;
        result.error_code = 401;
        result.error_message = wxS( "Not authenticated" );
        return result;
    }

    nlohmann::json params;
    params["model"] = aModel.ToStdString();
    params["method"] = aMethod.ToStdString();
    params["args"] = aArgs;
    params["kwargs"] = aKwargs;

    return SendRequest( wxS( "/web/dataset/call_kw" ), params );
}


ODOO_RPC_RESULT OdooRpcClient::Search( const wxString&       aModel,
                                        const nlohmann::json& aDomain,
                                        int                   aLimit,
                                        int                   aOffset )
{
    nlohmann::json args = nlohmann::json::array();
    args.push_back( aDomain );

    nlohmann::json kwargs;

    if( aLimit > 0 )
        kwargs["limit"] = aLimit;

    if( aOffset > 0 )
        kwargs["offset"] = aOffset;

    return CallMethod( aModel, wxS( "search" ), args, kwargs );
}


ODOO_RPC_RESULT OdooRpcClient::Read( const wxString&       aModel,
                                      const nlohmann::json& aIds,
                                      const nlohmann::json& aFields )
{
    nlohmann::json args = nlohmann::json::array();
    args.push_back( aIds );
    args.push_back( aFields );

    return CallMethod( aModel, wxS( "read" ), args, nlohmann::json::object() );
}


ODOO_RPC_RESULT OdooRpcClient::SearchRead( const wxString&       aModel,
                                            const nlohmann::json& aDomain,
                                            const nlohmann::json& aFields,
                                            int                   aLimit,
                                            int                   aOffset,
                                            const wxString&       aOrder )
{
    nlohmann::json args = nlohmann::json::array();
    args.push_back( aDomain );

    nlohmann::json kwargs;
    kwargs["fields"] = aFields;

    if( aLimit > 0 )
        kwargs["limit"] = aLimit;

    if( aOffset > 0 )
        kwargs["offset"] = aOffset;

    if( !aOrder.IsEmpty() )
        kwargs["order"] = aOrder.ToStdString();

    return CallMethod( aModel, wxS( "search_read" ), args, kwargs );
}


ODOO_RPC_RESULT OdooRpcClient::Create( const wxString& aModel, const nlohmann::json& aValues )
{
    nlohmann::json args = nlohmann::json::array();
    args.push_back( aValues );

    return CallMethod( aModel, wxS( "create" ), args, nlohmann::json::object() );
}


ODOO_RPC_RESULT OdooRpcClient::Write( const wxString&       aModel,
                                       const nlohmann::json& aIds,
                                       const nlohmann::json& aValues )
{
    nlohmann::json args = nlohmann::json::array();
    args.push_back( aIds );
    args.push_back( aValues );

    return CallMethod( aModel, wxS( "write" ), args, nlohmann::json::object() );
}


ODOO_RPC_RESULT OdooRpcClient::Unlink( const wxString& aModel, const nlohmann::json& aIds )
{
    nlohmann::json args = nlohmann::json::array();
    args.push_back( aIds );

    return CallMethod( aModel, wxS( "unlink" ), args, nlohmann::json::object() );
}


int OdooRpcClient::GetLastHttpStatus() const
{
    return m_lastHttpStatus;
}


wxString OdooRpcClient::GetLastError() const
{
    return m_lastError;
}


ODOO_RPC_RESULT OdooRpcClient::GetAttachments( const wxString& aModel, int aRecordId )
{
    nlohmann::json domain = nlohmann::json::array();
    domain.push_back( nlohmann::json::array( { "res_model", "=", aModel.ToStdString() } ) );
    domain.push_back( nlohmann::json::array( { "res_id", "=", aRecordId } ) );

    nlohmann::json fields = nlohmann::json::array( { "id", "name", "mimetype", "file_size" } );

    return SearchRead( wxS( "ir.attachment" ), domain, fields, 0, 0, wxS( "name" ) );
}


ODOO_RPC_RESULT OdooRpcClient::DownloadAttachment( int aAttachmentId )
{
    nlohmann::json ids = nlohmann::json::array();
    ids.push_back( aAttachmentId );

    nlohmann::json fields = nlohmann::json::array( { "datas", "name", "mimetype" } );

    ODOO_RPC_RESULT result = Read( wxS( "ir.attachment" ), ids, fields );

    if( result.success && result.result.is_array() && !result.result.empty() )
        result.result = result.result[0];

    return result;
}


ODOO_RPC_RESULT OdooRpcClient::UploadAttachment( const wxString& aModel, int aRecordId,
                                                   const wxString& aFilename,
                                                   const wxString& aData )
{
    nlohmann::json values;
    values["name"] = aFilename.ToStdString();
    values["res_model"] = aModel.ToStdString();
    values["res_id"] = aRecordId;
    values["datas"] = aData.ToStdString();
    values["type"] = "binary";

    return Create( wxS( "ir.attachment" ), values );
}


ODOO_RPC_RESULT OdooRpcClient::SendRequest( const wxString&       aEndpoint,
                                             const nlohmann::json& aParams )
{
    ODOO_RPC_RESULT result;

    try
    {
        KICAD_CURL_EASY curl;

        std::string url = m_serverUrl.ToStdString() + aEndpoint.ToStdString();
        curl.SetURL( url );
        curl.SetHeader( "Content-Type", "application/json" );
        curl.SetHeader( "Accept", "application/json" );

        if( !m_session.session_id.IsEmpty() )
        {
            std::string cookie = "session_id=" + m_session.session_id.ToStdString();
            curl.SetHeader( "Cookie", cookie );
        }

        nlohmann::json rpcRequest = BuildRpcRequest( aParams );
        std::string    requestBody = rpcRequest.dump();

        if( wxLog::IsAllowedTraceMask( traceOdooRpc ) )
        {
            nlohmann::json logRequest = rpcRequest;

            if( logRequest.contains( "params" ) )
            {
                if( logRequest["params"].contains( "password" ) )
                    logRequest["params"]["password"] = "***REDACTED***";

                if( logRequest["params"].contains( "api_key" ) )
                    logRequest["params"]["api_key"] = "***REDACTED***";
            }

            wxLogTrace( traceOdooRpc, wxS( "Request to %s: %s" ), aEndpoint, logRequest.dump() );
        }

        curl.SetPostFields( requestBody );

        int curlResult = curl.Perform();

        m_lastHttpStatus = curl.GetResponseStatusCode();

        if( curlResult != 0 )
        {
            result.success = false;
            result.error_code = curlResult;
            result.error_message = wxString::FromUTF8( curl.GetErrorText( curlResult ) );
            m_lastError = result.error_message;
            return result;
        }

        std::string response = curl.GetBuffer();
        wxLogTrace( traceOdooRpc, wxS( "Response: %s" ), response );

        result = ParseRpcResponse( response );

        if( result.success && result.result.is_object() &&
            result.result.contains( "session_id" ) )
        {
            m_session.session_id = wxString::FromUTF8(
                    result.result["session_id"].get<std::string>() );
        }
    }
    catch( const std::exception& e )
    {
        result.success = false;
        result.error_message = wxString::FromUTF8( e.what() );
        m_lastError = result.error_message;
    }

    return result;
}


nlohmann::json OdooRpcClient::BuildRpcRequest( const nlohmann::json& aParams )
{
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["method"] = "call";
    request["params"] = aParams;
    request["id"] = ++m_requestId;

    return request;
}


ODOO_RPC_RESULT OdooRpcClient::ParseRpcResponse( const std::string& aResponse )
{
    ODOO_RPC_RESULT result;

    try
    {
        nlohmann::json response = nlohmann::json::parse( aResponse );

        if( response.contains( "error" ) )
        {
            result.success = false;

            nlohmann::json error = response["error"];

            if( error.contains( "code" ) )
                result.error_code = error["code"].get<int>();

            if( error.contains( "message" ) )
                result.error_message = wxString::FromUTF8( error["message"].get<std::string>() );

            if( error.contains( "data" ) && error["data"].is_object() )
            {
                nlohmann::json data = error["data"];

                if( data.contains( "message" ) )
                    result.error_data = wxString::FromUTF8( data["message"].get<std::string>() );
                else if( data.contains( "debug" ) )
                    result.error_data = wxString::FromUTF8( data["debug"].get<std::string>() );
            }

            m_lastError = result.error_message;

            if( !result.error_data.IsEmpty() )
                m_lastError += wxS( ": " ) + result.error_data;
        }
        else if( response.contains( "result" ) )
        {
            result.success = true;
            result.result = response["result"];
        }
        else
        {
            result.success = false;
            result.error_message = wxS( "Invalid JSON-RPC response: missing result or error" );
            m_lastError = result.error_message;
        }
    }
    catch( const nlohmann::json::parse_error& e )
    {
        result.success = false;
        result.error_message = wxString::Format( wxS( "JSON parse error: %s" ),
                                                  wxString::FromUTF8( e.what() ) );
        m_lastError = result.error_message;
    }
    catch( const std::exception& e )
    {
        result.success = false;
        result.error_message = wxString::FromUTF8( e.what() );
        m_lastError = result.error_message;
    }

    return result;
}
