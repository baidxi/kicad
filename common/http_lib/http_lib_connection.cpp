/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2023 Andre F. K. Iwers <iwers11@gmail.com>
 * Copyright (C) redesign and expansion with version 2, 2024 Rosy <rosy@rosy-logic.ch>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <wx/log.h>
#include <fmt/core.h>
#include <wx/translation.h>
#include <ctime>

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>
#include <wx/base64.h>

#include <kicad_curl/kicad_curl_easy.h>
#include <curl/curl.h>

#include <http_lib/http_lib_connection.h>

// This is clunky but at the moment the only way to free the pointer after use without KiCad crashing.
// at this point we can't use smart pointers as there is a problem with the order of how things are deleted/freed
std::unique_ptr<KICAD_CURL_EASY> HTTP_LIB_CONNECTION::createCurlEasyObject()
{
    std::unique_ptr<KICAD_CURL_EASY> aCurl( new KICAD_CURL_EASY() );

    // prepare curl
    aCurl->SetHeader( "Accept", "application/json" );
    aCurl->SetHeader( "Authorization", "Token " + m_source.token );

    return aCurl;
}

bool HTTP_LIB_CONNECTION::checkServerResponse( std::unique_ptr<KICAD_CURL_EASY>& aCurl )
{
    int statusCode = aCurl->GetResponseStatusCode();
    if( statusCode != 200 )
    {
        m_lastError += wxString::Format( _( "API responded with error code: %s" ) + "\n",
                                         httpErrorCodeDescription( statusCode ) );
        return false;
    }

    return true;
}

/*
* HTTP response status codes indicate whether a specific HTTP request has been successfully completed.
* Responses are grouped in five classes:
*    Informational responses (100 ? 199)
*    Successful responses (200 ? 299)
*    Redirection messages (300 ? 399)
*    Client error responses (400 ? 499)
*    Server error responses (500 ? 599)
*
*    see: https://developer.mozilla.org/en-US/docs/Web/HTTP/Status
*/
wxString HTTP_LIB_CONNECTION::httpErrorCodeDescription( uint16_t aHttpCode )
{
    auto codeDescription = []( uint16_t aCode ) -> wxString
    {
        switch( aCode )
        {
        case 100: return wxS( "Continue" );
        case 101: return wxS( "Switching Protocols" );
        case 102: return wxS( "Processing" );
        case 103: return wxS( "Early Hints" );

        case 200: return wxS( "OK" );
        case 201: return wxS( "Created" );
        case 203: return wxS( "Non-Authoritative Information" );
        case 204: return wxS( "No Content" );
        case 205: return wxS( "Reset Content" );
        case 206: return wxS( "Partial Content" );
        case 207: return wxS( "Multi-Status" );
        case 208: return wxS( "Already Reporte" );
        case 226: return wxS( "IM Used" );

        case 300: return wxS( "Multiple Choices" );
        case 301: return wxS( "Moved Permanently" );
        case 302: return wxS( "Found" );
        case 303: return wxS( "See Other" );
        case 304: return wxS( "Not Modified" );
        case 305: return wxS( "Use Proxy (Deprecated)" );
        case 306: return wxS( "Unused" );
        case 307: return wxS( "Temporary Redirect" );
        case 308: return wxS( "Permanent Redirect" );

        case 400: return wxS( "Bad Request" );
        case 401: return wxS( "Unauthorized" );
        case 402: return wxS( "Payment Required (Experimental)" );
        case 403: return wxS( "Forbidden" );
        case 404: return wxS( "Not Found" );
        case 405: return wxS( "Method Not Allowed" );
        case 406: return wxS( "Not Acceptable" );
        case 407: return wxS( "Proxy Authentication Required" );
        case 408: return wxS( "Request Timeout" );
        case 409: return wxS( "Conflict" );
        case 410: return wxS( "Gone" );
        case 411: return wxS( "Length Required" );
        case 412: return wxS( "Payload Too Large" );
        case 414: return wxS( "URI Too Long" );
        case 415: return wxS( "Unsupported Media Type" );
        case 416: return wxS( "Range Not Satisfiable" );
        case 417: return wxS( "Expectation Failed" );
        case 418: return wxS( "I'm a teapot" );
        case 421: return wxS( "Misdirected Request" );
        case 422: return wxS( "Unprocessable Conten" );
        case 423: return wxS( "Locked" );
        case 424: return wxS( "Failed Dependency" );
        case 425: return wxS( "Too Early (Experimental)" );
        case 426: return wxS( "Upgrade Required" );
        case 428: return wxS( "Precondition Required" );
        case 429: return wxS( "Too Many Requests" );
        case 431: return wxS( "Request Header Fields Too Large" );
        case 451: return wxS( "Unavailable For Legal Reasons" );

        case 500: return wxS( "Internal Server Error" );
        case 501: return wxS( "Not Implemented" );
        case 502: return wxS( "Bad Gateway" );
        case 503: return wxS( "Service Unavailable" );
        case 504: return wxS( "Gateway Timeout" );
        case 505: return wxS( "HTTP Version Not Supported" );
        case 506: return wxS( "Variant Also Negotiates" );
        case 507: return wxS( "Insufficient Storag" );
        case 508: return wxS( "Loop Detecte" );
        case 510: return wxS( "Not Extended" );
        case 511: return wxS( "Network Authentication Required" );
        default: return wxS( "Unknown" );
        }
    };

    return wxString::Format( wxS( "%d: %s" ), aHttpCode, codeDescription( aHttpCode ) );
}
