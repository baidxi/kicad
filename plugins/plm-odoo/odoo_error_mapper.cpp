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

#include "odoo_error_mapper.h"


PLM_ERROR_CODE OdooErrorMapper::FromHttpStatus( int aHttpStatus )
{
    switch( aHttpStatus )
    {
    case 200:
    case 201:
    case 204:
        return PLM_ERROR_CODE::SUCCESS;

    case 401:
        return PLM_ERROR_CODE::ERR_AUTH_FAILED;

    case 403:
        return PLM_ERROR_CODE::ERR_AUTH_INSUFFICIENT;

    case 404:
        return PLM_ERROR_CODE::ERR_FILE_NOT_FOUND;

    case 408:
        return PLM_ERROR_CODE::ERR_NETWORK_TIMEOUT;

    case 409:
        return PLM_ERROR_CODE::ERR_LOCK_CONFLICT;

    case 500:
    case 502:
    case 503:
    case 504:
        return PLM_ERROR_CODE::ERR_INTERNAL;

    default:
        if( aHttpStatus >= 400 && aHttpStatus < 500 )
            return PLM_ERROR_CODE::ERR_INVALID_PARAMETER;
        else if( aHttpStatus >= 500 )
            return PLM_ERROR_CODE::ERR_INTERNAL;

        return PLM_ERROR_CODE::SUCCESS;
    }
}


PLM_ERROR_CODE OdooErrorMapper::FromRpcResult( const ODOO_RPC_RESULT& aRpcResult )
{
    if( aRpcResult.success )
        return PLM_ERROR_CODE::SUCCESS;

    if( IsAuthError( aRpcResult ) )
        return PLM_ERROR_CODE::ERR_AUTH_FAILED;

    if( IsPermissionError( aRpcResult ) )
        return PLM_ERROR_CODE::ERR_AUTH_INSUFFICIENT;

    if( IsNotFoundError( aRpcResult ) )
        return PLM_ERROR_CODE::ERR_FILE_NOT_FOUND;

    if( IsNetworkError( aRpcResult ) )
        return PLM_ERROR_CODE::ERR_NETWORK_UNREACHABLE;

    if( aRpcResult.error_code == 100 )
        return PLM_ERROR_CODE::ERR_AUTH_FAILED;

    if( aRpcResult.error_code == 200 )
        return PLM_ERROR_CODE::ERR_INTERNAL;

    return PLM_ERROR_CODE::ERR_INTERNAL;
}


PLM_ERROR_CODE OdooErrorMapper::FromCurlError( int aCurlError )
{
    switch( aCurlError )
    {
    case 0:
        return PLM_ERROR_CODE::SUCCESS;

    case 6:  // CURLE_COULDNT_RESOLVE_HOST
    case 7:  // CURLE_COULDNT_CONNECT
        return PLM_ERROR_CODE::ERR_NETWORK_UNREACHABLE;

    case 28: // CURLE_OPERATION_TIMEDOUT
        return PLM_ERROR_CODE::ERR_NETWORK_TIMEOUT;

    case 35: // CURLE_SSL_CONNECT_ERROR
    case 51: // CURLE_PEER_FAILED_VERIFICATION
    case 58: // CURLE_SSL_CERTPROBLEM
    case 59: // CURLE_SSL_CIPHER
    case 60: // CURLE_SSL_CACERT
        return PLM_ERROR_CODE::ERR_NETWORK_SSL;

    default:
        return PLM_ERROR_CODE::ERR_NETWORK_UNREACHABLE;
    }
}


wxString OdooErrorMapper::FormatErrorMessage( const ODOO_RPC_RESULT& aRpcResult )
{
    if( IsPermissionError( aRpcResult ) )
        return FormatPermissionError( wxS( "perform this operation" ), aRpcResult );

    wxString message = aRpcResult.error_message;

    if( message.IsEmpty() )
        message = wxS( "Unknown error" );

    if( !aRpcResult.error_data.IsEmpty() )
    {
        wxString data = aRpcResult.error_data;

        if( data.length() > 200 )
            data = data.Left( 200 ) + wxS( "..." );

        message += wxS( "\n\nDetails: " ) + data;
    }

    return message;
}


bool OdooErrorMapper::IsAuthError( const ODOO_RPC_RESULT& aRpcResult )
{
    if( aRpcResult.error_code == 401 || aRpcResult.error_code == 100 )
        return true;

    wxString msg = aRpcResult.error_message.Lower();
    wxString data = aRpcResult.error_data.Lower();

    return msg.Contains( wxS( "session expired" ) ) ||
           msg.Contains( wxS( "invalid session" ) ) ||
           msg.Contains( wxS( "authentication" ) ) ||
           msg.Contains( wxS( "access denied" ) ) ||
           data.Contains( wxS( "session expired" ) ) ||
           data.Contains( wxS( "accessdenied" ) );
}


bool OdooErrorMapper::IsNetworkError( const ODOO_RPC_RESULT& aRpcResult )
{
    wxString msg = aRpcResult.error_message.Lower();

    return msg.Contains( wxS( "connection" ) ) ||
           msg.Contains( wxS( "timeout" ) ) ||
           msg.Contains( wxS( "network" ) ) ||
           msg.Contains( wxS( "unreachable" ) ) ||
           msg.Contains( wxS( "could not resolve" ) );
}


bool OdooErrorMapper::IsPermissionError( const ODOO_RPC_RESULT& aRpcResult )
{
    if( aRpcResult.error_code == 403 )
        return true;

    wxString msg = aRpcResult.error_message.Lower();
    wxString data = aRpcResult.error_data.Lower();

    return msg.Contains( wxS( "permission" ) ) ||
           msg.Contains( wxS( "not allowed" ) ) ||
           msg.Contains( wxS( "forbidden" ) ) ||
           data.Contains( wxS( "accesserror" ) );
}


bool OdooErrorMapper::IsNotFoundError( const ODOO_RPC_RESULT& aRpcResult )
{
    if( aRpcResult.error_code == 404 )
        return true;

    wxString msg = aRpcResult.error_message.Lower();
    wxString data = aRpcResult.error_data.Lower();

    return msg.Contains( wxS( "not found" ) ) ||
           msg.Contains( wxS( "does not exist" ) ) ||
           msg.Contains( wxS( "missing" ) ) ||
           data.Contains( wxS( "missingrecord" ) );
}


wxString OdooErrorMapper::FormatPermissionError( const wxString&        aAction,
                                                 const ODOO_RPC_RESULT& aRpcResult )
{
    wxString message = wxString::Format(
            wxS( "You do not have permission to %s. Check your Odoo group memberships." ),
            aAction );

    if( !aRpcResult.error_message.IsEmpty() )
    {
        wxString detail = aRpcResult.error_message;

        if( detail.length() > 150 )
            detail = detail.Left( 150 ) + wxS( "..." );

        message += wxS( "\n\nServer response: " ) + detail;
    }

    return message;
}
