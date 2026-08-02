/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2023 Jon Evans <jon@craftyjon.com>
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <fmt/format.h>
#include <google/protobuf/descriptor.h>
#include <wx/app.h>
#include <wx/datetime.h>
#include <wx/event.h>
#include <wx/stdpaths.h>

#include <advanced_config.h>
#include <api/api_handler.h>
#include <api/api_utils.h> // traceApi
#include <api/api_server.h>
#include <api/common/commands/editor_commands.pb.h>
#include <api/common/types/base_types.pb.h>
#include <kiid.h>
#include <kinng.h>
#include <paths.h>
#include <pgm_base.h>
#include <settings/common_settings.h>
#include <string_utils.h>

#include <api/common/envelope.pb.h>

#ifdef __UNIX__
#include <sys/file.h>
#endif

using kiapi::common::ApiRequest, kiapi::common::ApiResponse, kiapi::common::ApiStatusCode;


wxString KICAD_API_SERVER::s_logFileName = "api.log";


wxDEFINE_EVENT( API_REQUEST_EVENT, wxCommandEvent );


KICAD_API_SERVER::KICAD_API_SERVER( bool aAutoStart ) :
        wxEvtHandler(),
        m_token( KIID().AsStdString() ),
        m_readyToReply( false )
{
    if( !aAutoStart )
        return;

    if( !Pgm().GetCommonSettings()->m_Api.enable_server )
    {
        wxLogTrace( traceApi, "Server: disabled by user preferences." );
        return;
    }

    Start();
}


KICAD_API_SERVER::~KICAD_API_SERVER()
{
    Stop();
}


void KICAD_API_SERVER::Start()
{
    if( Running() )
        return;

    wxFileName socket;

    if( m_socketPathOverride.IsEmpty() )
    {
#ifdef __WXMAC__
        socket.AssignDir( wxS( "/tmp" ) );
#else
        socket.AssignDir( wxStandardPaths::Get().GetTempDir() );
#endif
        socket.AppendDir( wxS( "kicad" ) );
        socket.SetFullName( wxS( "api.sock" ) );
    }
    else
    {
        socket.Assign( m_socketPathOverride );

        if( !socket.IsAbsolute() )
            socket.MakeAbsolute();
    }

    if( !PATHS::EnsurePathExists( socket.GetPath() ) )
    {
        wxLogTrace( traceApi, wxString::Format( "Server: socket path %s could not be created",
                                                socket.GetPath() ) );
        return;
    }

#ifndef __WINDOWS__
    // We use non-abstract sockets because macOS and some other non-Linux platforms don't support
    // abstract sockets, which means there might be an old socket to unlink.  In order to try to
    // recover this, we lock a file (which will be unlocked on process exit) and if we get the lock,
    // we know the old socket is orphaned and can be removed.
    wxFileName lockFilePath( socket.GetPath(), wxS( "api.lock" ) );

    int lockFile = open( lockFilePath.GetFullPath().c_str(), O_RDONLY | O_CREAT, 0600 );

    if( lockFile >= 0 && flock( lockFile, LOCK_EX | LOCK_NB ) == 0 )
    {
        if( socket.Exists() )
        {
            wxLogTrace( traceApi, wxString::Format( "Server: cleaning up stale socket path %s",
                                                    socket.GetFullPath() ) );
            wxRemoveFile( socket.GetFullPath() );
        }
    }
#endif

    if( socket.Exists() )
    {
        socket.SetFullName( wxString::Format( wxS( "api-%lu.sock" ), ::wxGetProcessId() ) );

        if( socket.Exists() )
        {
            wxLogTrace( traceApi, wxString::Format( "Server: PID socket path %s already exists!",
                                                    socket.GetFullPath() ) );
            return;
        }
    }

    m_server = std::make_unique<KINNG_REQUEST_SERVER>(
            fmt::format( "ipc://{}", socket.GetFullPath().ToStdString() ) );
    m_server->SetCallback( [&]( std::string* aRequest ) { onApiRequest( aRequest ); } );

    if( !m_server->Start() )
    {
        wxLogTrace( traceApi, "Server: failed to start KINNG listener thread" );
        m_server.reset( nullptr );
        return;
    }

    m_logFilePath.AssignDir( PATHS::GetLogsPath() );
    m_logFilePath.SetName( s_logFileName );

    if( ADVANCED_CFG::GetCfg().m_EnableAPILogging )
    {
        PATHS::EnsurePathExists( PATHS::GetLogsPath() );
        log( fmt::format( "--- KiCad API server started at {} ---\n", SocketPath() ) );
    }

    wxLogTrace( traceApi, wxString::Format( "Server: listening at %s", SocketPath() ) );
    Bind( API_REQUEST_EVENT, &KICAD_API_SERVER::handleApiEvent, this );
}


void KICAD_API_SERVER::Stop()
{
    if( !Running() )
        return;

    wxLogTrace( traceApi, "Stopping server" );
    Unbind( API_REQUEST_EVENT, &KICAD_API_SERVER::handleApiEvent, this );

    m_server->Stop();
    m_server.reset( nullptr );
}


bool KICAD_API_SERVER::Running() const
{
    return m_server && m_server->Running();
}


void KICAD_API_SERVER::RegisterHandler( API_HANDLER* aHandler )
{
    wxCHECK( aHandler, /* void */ );
    m_handlers.insert( aHandler );
}


void KICAD_API_SERVER::DeregisterHandler( API_HANDLER* aHandler )
{
    m_handlers.erase( aHandler );
}


std::string KICAD_API_SERVER::SocketPath() const
{
    return m_server ? m_server->SocketPath() : "";
}


void KICAD_API_SERVER::onApiRequest( std::string* aRequest )
{
    if( !m_readyToReply.load( std::memory_order_acquire ) )
    {
        ApiResponse notHandled;
        notHandled.mutable_status()->set_status( ApiStatusCode::AS_NOT_READY );
        notHandled.mutable_status()->set_error_message( "KiCad is not ready to reply" );
        m_server->Reply( notHandled.SerializeAsString() );
        log( "Got incoming request but was not yet ready to reply." );
        return;
    }

    wxCommandEvent* evt = new wxCommandEvent( API_REQUEST_EVENT );

    // We don't actually need write access to this string, but client data is non-const
    evt->SetClientData( static_cast<void*>( aRequest ) );

    // Takes ownership and frees the wxCommandEvent
    QueueEvent( evt );
}


void KICAD_API_SERVER::handleApiEvent( wxCommandEvent& aEvent )
{
    std::string& requestString = *static_cast<std::string*>( aEvent.GetClientData() );
    handleApiRequestString( requestString );
}


bool KICAD_API_SERVER::aggregateOpenDocuments( const ApiRequest& aRequest, ApiResponse& aResponse )
{
    using kiapi::common::commands::GetOpenDocuments;
    using kiapi::common::commands::GetOpenDocumentsResponse;
    using kiapi::common::types::DocumentType;

    std::string typeName;

    if( !google::protobuf::Any::ParseAnyTypeUrl( aRequest.message().type_url(), &typeName )
        || typeName != GetOpenDocuments::GetDescriptor()->full_name() )
    {
        return false;
    }

    GetOpenDocuments query;

    if( !aRequest.message().UnpackTo( &query ) || query.type() != DocumentType::DOCTYPE_UNKNOWN )
        return false;

    const google::protobuf::EnumDescriptor* docTypes = kiapi::common::types::DocumentType_descriptor();

    GetOpenDocumentsResponse aggregated;

    for( int ii = 0; ii < docTypes->value_count(); ++ii )
    {
        int docType = docTypes->value( ii )->number();

        if( docType == DocumentType::DOCTYPE_UNKNOWN )
            continue;

        query.set_type( static_cast<DocumentType>( docType ) );

        ApiRequest typedRequest = aRequest;
        typedRequest.mutable_message()->PackFrom( query );

        for( API_HANDLER* handler : m_handlers )
        {
            API_RESULT typedResult = handler->Handle( typedRequest );

            if( !typedResult.has_value() )
            {
                if( typedResult.error().status() == ApiStatusCode::AS_UNHANDLED )
                    continue;

                // A handler that claimed the request but failed leaves the aggregate incomplete;
                // report that failure rather than a silently truncated document list.
                aResponse.mutable_status()->CopyFrom( typedResult.error() );
                return true;
            }

            // Handlers can also report failure in-band, e.g. when the request could not be
            // unpacked into the command type they registered for.
            if( typedResult->status().status() != ApiStatusCode::AS_OK )
            {
                aResponse.mutable_status()->CopyFrom( typedResult->status() );
                return true;
            }

            GetOpenDocumentsResponse partial;

            if( !typedResult->message().UnpackTo( &partial ) )
            {
                aResponse.mutable_status()->set_status( ApiStatusCode::AS_BAD_REQUEST );
                aResponse.mutable_status()->set_error_message(
                        "handler returned an unexpected response to GetOpenDocuments" );
                return true;
            }

            aggregated.mutable_documents()->MergeFrom( partial.documents() );
            break;
        }
    }

    aResponse.mutable_status()->set_status( ApiStatusCode::AS_OK );
    aResponse.mutable_message()->PackFrom( aggregated );
    return true;
}


void KICAD_API_SERVER::handleApiRequestString( std::string& aRequestString )
{
    ApiRequest request;

    if( !request.ParseFromString( aRequestString ) )
    {
        ApiResponse error;
        error.mutable_header()->set_kicad_token( m_token );
        error.mutable_status()->set_status( ApiStatusCode::AS_BAD_REQUEST );
        error.mutable_status()->set_error_message( "request could not be parsed" );
        m_server->Reply( error.SerializeAsString() );

        if( ADVANCED_CFG::GetCfg().m_EnableAPILogging )
            log( "Response (ERROR): " + error.Utf8DebugString() );

        return;
    }

    if( ADVANCED_CFG::GetCfg().m_EnableAPILogging )
        log( "Request: " + request.Utf8DebugString() );

    if( !request.header().kicad_token().empty() &&
        request.header().kicad_token().compare( m_token ) != 0 )
    {
        ApiResponse error;
        error.mutable_header()->set_kicad_token( m_token );
        error.mutable_status()->set_status( ApiStatusCode::AS_TOKEN_MISMATCH );
        error.mutable_status()->set_error_message(
                "the provided kicad_token did not match this KiCad instance's token" );
        m_server->Reply( error.SerializeAsString() );

        if( ADVANCED_CFG::GetCfg().m_EnableAPILogging )
            log( "Response (ERROR): " + error.Utf8DebugString() );

        return;
    }

    API_RESULT result;
    ApiResponse aggregate;

    if( aggregateOpenDocuments( request, aggregate ) )
    {
        result = std::move( aggregate );
    }
    else
    {
        for( API_HANDLER* handler : m_handlers )
        {
            result = handler->Handle( request );

            if( result.has_value() )
                break;
            else if( result.error().status() != ApiStatusCode::AS_UNHANDLED )
                break;
        }
    }

    // Note: at the point we call Reply(), we no longer own requestString.

    if( result.has_value() )
    {
        result->mutable_header()->set_kicad_token( m_token );
        m_server->Reply( result->SerializeAsString() );

        if( ADVANCED_CFG::GetCfg().m_EnableAPILogging )
            log( "Response: " + result->Utf8DebugString() );
    }
    else
    {
        ApiResponse error;
        error.mutable_status()->CopyFrom( result.error() );
        error.mutable_header()->set_kicad_token( m_token );

        if( result.error().status() == ApiStatusCode::AS_UNHANDLED )
        {
            std::string type = "<unparseable Any>";
            google::protobuf::Any::ParseAnyTypeUrl( request.message().type_url(), &type );
            std::string msg = fmt::format( "no handler available for request of type {}", type );
            error.mutable_status()->set_error_message( msg );
        }

        m_server->Reply( error.SerializeAsString() );

        if( ADVANCED_CFG::GetCfg().m_EnableAPILogging )
            log( "Response (ERROR): " + error.Utf8DebugString() );
    }
}


void KICAD_API_SERVER::log( const std::string& aOutput )
{
    FILE* fp = wxFopen( m_logFilePath.GetFullPath(), wxT( "a" ) );

    if( !fp )
        return;

    wxString out;
    wxDateTime now = wxDateTime::Now();

    fprintf( fp, "%s", TO_UTF8( out.Format( wxS( "%s: %s" ),
                                            now.FormatISOCombined(), aOutput ) ) );
    fclose( fp );
}
