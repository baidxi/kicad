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

#include "plm_git_auth.h"

#include <wx/filename.h>
#include <wx/process.h>
#include <wx/stdpaths.h>
#include <wx/txtstrm.h>
#include <wx/uri.h>
#include <wx/utils.h>


GIT_AUTH_HELPER::GIT_AUTH_HELPER()
{
}


GIT_AUTH_HELPER::~GIT_AUTH_HELPER()
{
}


void GIT_AUTH_HELPER::Configure( const GIT_AUTH_CONFIG& aConfig )
{
    m_config = aConfig;
}


GIT_AUTH_RESULT GIT_AUTH_HELPER::TestAuth( const wxString& aRemoteUrl )
{
    GIT_AUTH_RESULT result;

    wxArrayString output, errors;

    // Store original environment values to restore later
    std::map<wxString, wxString> originalEnv;

    wxString remoteArg;

    if( IsSshUrl( aRemoteUrl ) )
    {
        auto env = GetAuthEnvironment();

        for( const auto& kv : env )
        {
            wxString oldVal;
            bool hadValue = wxGetEnv( kv.first, &oldVal );
            originalEnv[kv.first] = hadValue ? oldVal : wxString();
            wxSetEnv( kv.first, kv.second );
        }

        remoteArg = aRemoteUrl;
    }
    else
    {
        remoteArg = BuildAuthenticatedUrl( aRemoteUrl );
    }

    // Quote the remote URL if it contains special characters
    wxString cmd = wxS( "git ls-remote --exit-code" );

    if( remoteArg.Contains( wxS( " " ) ) || remoteArg.Contains( wxS( "\"" ) ) ||
        remoteArg.Contains( wxS( "'" ) ) )
    {
        wxString escaped = remoteArg;
        escaped.Replace( wxS( "\"" ), wxS( "\\\"" ) );
        cmd += wxS( " \"" ) + escaped + wxS( "\"" );
    }
    else
    {
        cmd += wxS( " " ) + remoteArg;
    }

    cmd += wxS( " HEAD" );

    long rc = wxExecute( cmd, output, errors, wxEXEC_SYNC | wxEXEC_NOEVENTS );

    // Restore original environment
    for( const auto& kv : originalEnv )
    {
        if( kv.second.IsEmpty() )
            wxUnsetEnv( kv.first );
        else
            wxSetEnv( kv.first, kv.second );
    }

    if( rc == 0 )
    {
        result.success = true;
        result.username = m_config.username;
        result.auth_url = aRemoteUrl;
    }
    else
    {
        result.success = false;

        if( !errors.IsEmpty() )
            result.error_message = errors[0];
        else
            result.error_message = wxS( "Authentication failed" );
    }

    return result;
}


wxString GIT_AUTH_HELPER::BuildAuthenticatedUrl( const wxString& aOriginalUrl )
{
    if( IsSshUrl( aOriginalUrl ) )
        return aOriginalUrl;

    if( m_config.method == GIT_AUTH_METHOD::NONE )
        return aOriginalUrl;

    if( !IsHttpsUrl( aOriginalUrl ) )
        return aOriginalUrl;

    wxString protocol, host, path;

    if( !ParseUrl( aOriginalUrl, protocol, host, path ) )
        return aOriginalUrl;

    wxString username = m_config.username;
    wxString password = m_config.password;

    if( m_config.method == GIT_AUTH_METHOD::CREDENTIAL_HELPER )
    {
        wxString storedUser, storedPass;

        if( RetrieveCredentials( aOriginalUrl, storedUser, storedPass ) )
        {
            username = storedUser;
            password = storedPass;
        }
    }

    if( m_config.method == GIT_AUTH_METHOD::HTTPS_TOKEN )
    {
        username = wxS( "oauth2" );
        password = m_config.password;
    }

    if( username.IsEmpty() && password.IsEmpty() )
        return aOriginalUrl;

    wxString credentials;

    if( !username.IsEmpty() )
    {
        wxString encodedUser = username;
        encodedUser.Replace( wxS( "@" ), wxS( "%40" ) );
        encodedUser.Replace( wxS( ":" ), wxS( "%3A" ) );
        credentials = encodedUser;
    }

    if( !password.IsEmpty() )
    {
        wxString encodedPass = password;
        encodedPass.Replace( wxS( "@" ), wxS( "%40" ) );
        encodedPass.Replace( wxS( ":" ), wxS( "%3A" ) );

        if( !credentials.IsEmpty() )
            credentials += wxS( ":" );

        credentials += encodedPass;
    }

    if( credentials.IsEmpty() )
        return aOriginalUrl;

    return protocol + wxS( "://" ) + credentials + wxS( "@" ) + host + path;
}


std::map<wxString, wxString> GIT_AUTH_HELPER::GetAuthEnvironment()
{
    std::map<wxString, wxString> env;

    if( m_config.method == GIT_AUTH_METHOD::SSH_KEY && !m_config.ssh_key_path.IsEmpty() )
    {
        wxString sshCmd = wxS( "ssh -i " ) + m_config.ssh_key_path +
                          wxS( " -o IdentitiesOnly=yes -o StrictHostKeyChecking=accept-new" );
        env[wxS( "GIT_SSH_COMMAND" )] = sshCmd;
    }
    else if( m_config.method == GIT_AUTH_METHOD::SSH_AGENT )
    {
        wxString sshCmd = wxS( "ssh -o StrictHostKeyChecking=accept-new" );
        env[wxS( "GIT_SSH_COMMAND" )] = sshCmd;
    }

    return env;
}


bool GIT_AUTH_HELPER::StoreCredentials( const wxString& aUrl, const wxString& aUsername,
                                         const wxString& aPassword )
{
    return RunCredentialStore( aUrl, aUsername, aPassword );
}


bool GIT_AUTH_HELPER::RetrieveCredentials( const wxString& aUrl, wxString& aUsername,
                                            wxString& aPassword )
{
    return RunCredentialFill( aUrl, aUsername, aPassword );
}


bool GIT_AUTH_HELPER::RemoveCredentials( const wxString& aUrl )
{
    return RunCredentialReject( aUrl );
}


GIT_AUTH_METHOD GIT_AUTH_HELPER::DetectAuthMethod( const wxString& aUrl )
{
    if( IsSshUrl( aUrl ) )
    {
        if( IsSshAgentAvailable() )
            return GIT_AUTH_METHOD::SSH_AGENT;

        wxString keyPath = GetDefaultSshKeyPath();

        if( wxFileExists( keyPath ) )
            return GIT_AUTH_METHOD::SSH_KEY;

        // No key file found; fall back to agent as a last resort
        return GIT_AUTH_METHOD::SSH_AGENT;
    }

    if( IsHttpsUrl( aUrl ) )
    {
        wxString host;
        wxURI uri( aUrl );
        host = uri.GetServer().Lower();

        if( host.Contains( wxS( "github.com" ) ) || host.Contains( wxS( "gitlab.com" ) ) ||
            host.Contains( wxS( "bitbucket.org" ) ) )
        {
            return GIT_AUTH_METHOD::HTTPS_TOKEN;
        }

        return GIT_AUTH_METHOD::HTTPS_BASIC;
    }

    return GIT_AUTH_METHOD::NONE;
}


bool GIT_AUTH_HELPER::IsSshUrl( const wxString& aUrl )
{
    if( aUrl.StartsWith( wxS( "git@" ) ) )
        return true;

    if( aUrl.StartsWith( wxS( "ssh://" ) ) )
        return true;

    return false;
}


bool GIT_AUTH_HELPER::IsHttpsUrl( const wxString& aUrl )
{
    return aUrl.StartsWith( wxS( "https://" ) );
}


wxString GIT_AUTH_HELPER::GetDefaultSshKeyPath()
{
    wxString homePath;

#ifdef _WIN32
    homePath = wxGetenv( wxS( "USERPROFILE" ) );
#else
    homePath = wxGetenv( wxS( "HOME" ) );
#endif

    if( homePath.IsEmpty() )
        homePath = wxStandardPaths::Get().GetUserConfigDir();

    return homePath + wxFileName::GetPathSeparator() + wxS( ".ssh" ) +
           wxFileName::GetPathSeparator() + wxS( "id_rsa" );
}


bool GIT_AUTH_HELPER::IsSshAgentAvailable()
{
    wxString authSock;

#ifdef _WIN32
    authSock = wxGetenv( wxS( "SSH_AUTH_SOCK" ) );

    if( authSock.IsEmpty() )
    {
        wxArrayString output, errors;
        long rc = wxExecute( wxS( "ssh-add -l" ), output, errors, wxEXEC_SYNC );
        return rc == 0;
    }
#else
    authSock = wxGetenv( wxS( "SSH_AUTH_SOCK" ) );
#endif

    if( authSock.IsEmpty() )
        return false;

    wxArrayString output, errors;
    long rc = wxExecute( wxS( "ssh-add -l" ), output, errors, wxEXEC_SYNC );

    return rc == 0 && !output.IsEmpty();
}


bool GIT_AUTH_HELPER::RunCredentialFill( const wxString& aUrl, wxString& aUsername,
                                          wxString& aPassword )
{
    wxString protocol, host, path;

    if( !ParseUrl( aUrl, protocol, host, path ) )
        return false;

    wxString input = wxS( "protocol=" ) + protocol + wxS( "\n" ) +
                     wxS( "host=" ) + host + wxS( "\n" );

    wxProcess* process = new wxProcess();
    process->Redirect();

    long pid = wxExecute( wxS( "git credential fill" ), wxEXEC_ASYNC, process );

    if( pid == 0 )
    {
        delete process;
        return false;
    }

    wxOutputStream* procStdin = process->GetOutputStream();

    if( procStdin )
    {
        wxScopedCharBuffer utf8 = input.ToUTF8();
        procStdin->Write( utf8.data(), utf8.length() );
        process->CloseOutput();
    }

    // Wait for the process to finish so we can read its full output
    int waitElapsed = 0;

    while( wxProcess::Exists( pid ) && waitElapsed < 30000 )
    {
        wxMilliSleep( 10 );
        waitElapsed += 10;
    }

    if( wxProcess::Exists( pid ) )
    {
        wxProcess::Kill( pid, wxSIGKILL );
        delete process;
        return false;
    }

    wxInputStream* procStdout = process->GetInputStream();

    if( procStdout )
    {
        wxTextInputStream text( *procStdout );

        while( !procStdout->Eof() )
        {
            wxString line = text.ReadLine();

            if( line.StartsWith( wxS( "username=" ) ) )
                aUsername = line.Mid( 9 );
            else if( line.StartsWith( wxS( "password=" ) ) )
                aPassword = line.Mid( 9 );
        }
    }

    delete process;

    return !aUsername.IsEmpty() || !aPassword.IsEmpty();
}


bool GIT_AUTH_HELPER::RunCredentialStore( const wxString& aUrl, const wxString& aUsername,
                                           const wxString& aPassword )
{
    wxString protocol, host, path;

    if( !ParseUrl( aUrl, protocol, host, path ) )
        return false;

    wxString input = wxS( "protocol=" ) + protocol + wxS( "\n" ) +
                     wxS( "host=" ) + host + wxS( "\n" ) +
                     wxS( "username=" ) + aUsername + wxS( "\n" ) +
                     wxS( "password=" ) + aPassword + wxS( "\n" );

    wxProcess* process = new wxProcess();
    process->Redirect();

    long pid = wxExecute( wxS( "git credential approve" ), wxEXEC_ASYNC, process );

    if( pid == 0 )
    {
        delete process;
        return false;
    }

    wxOutputStream* procStdin = process->GetOutputStream();

    if( procStdin )
    {
        wxScopedCharBuffer utf8 = input.ToUTF8();
        procStdin->Write( utf8.data(), utf8.length() );
        process->CloseOutput();
    }

    int waitElapsed = 0;

    while( wxProcess::Exists( pid ) && waitElapsed < 30000 )
    {
        wxMilliSleep( 10 );
        waitElapsed += 10;
    }

    if( wxProcess::Exists( pid ) )
        wxProcess::Kill( pid, wxSIGKILL );

    delete process;

    return true;
}


bool GIT_AUTH_HELPER::RunCredentialReject( const wxString& aUrl )
{
    wxString protocol, host, path;

    if( !ParseUrl( aUrl, protocol, host, path ) )
        return false;

    wxString input = wxS( "protocol=" ) + protocol + wxS( "\n" ) +
                     wxS( "host=" ) + host + wxS( "\n" );

    wxProcess* process = new wxProcess();
    process->Redirect();

    long pid = wxExecute( wxS( "git credential reject" ), wxEXEC_ASYNC, process );

    if( pid == 0 )
    {
        delete process;
        return false;
    }

    wxOutputStream* procStdin = process->GetOutputStream();

    if( procStdin )
    {
        wxScopedCharBuffer utf8 = input.ToUTF8();
        procStdin->Write( utf8.data(), utf8.length() );
        process->CloseOutput();
    }

    int waitElapsed = 0;

    while( wxProcess::Exists( pid ) && waitElapsed < 30000 )
    {
        wxMilliSleep( 10 );
        waitElapsed += 10;
    }

    if( wxProcess::Exists( pid ) )
        wxProcess::Kill( pid, wxSIGKILL );

    delete process;

    return true;
}


bool GIT_AUTH_HELPER::ParseUrl( const wxString& aUrl, wxString& aProtocol, wxString& aHost,
                                 wxString& aPath )
{
    if( IsSshUrl( aUrl ) )
    {
        if( aUrl.StartsWith( wxS( "git@" ) ) )
        {
            aProtocol = wxS( "ssh" );
            wxString remainder = aUrl.Mid( 4 );
            int colonPos = remainder.Find( ':' );

            if( colonPos != wxNOT_FOUND )
            {
                aHost = remainder.Left( colonPos );
                aPath = wxS( "/" ) + remainder.Mid( colonPos + 1 );
            }
            else
            {
                aHost = remainder;
                aPath = wxEmptyString;
            }

            return true;
        }

        wxURI uri( aUrl );
        aProtocol = wxS( "ssh" );
        aHost = uri.GetServer();
        aPath = uri.GetPath();
        return true;
    }

    wxURI uri( aUrl );

    if( !uri.HasServer() )
        return false;

    aProtocol = uri.GetScheme();
    aHost = uri.GetServer();

    if( uri.HasPort() )
        aHost += wxS( ":" ) + uri.GetPort();

    aPath = uri.GetPath();

    return true;
}
