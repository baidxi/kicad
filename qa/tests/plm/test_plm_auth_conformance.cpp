/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file test_plm_auth_conformance.cpp
 * @brief Authentication conformance tests for PLM plugins.
 *
 * These tests verify that PLM plugin authentication behavior
 * conforms to the expected interface contract.
 */

#include <boost/test/unit_test.hpp>
#include "plm_conformance_framework.h"

#include <ctime>


BOOST_AUTO_TEST_SUITE( PlmAuthConformance )


BOOST_AUTO_TEST_CASE( AuthInitialState )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    BOOST_CHECK( !mock.IsAuthenticated() );

    PLM_SESSION session = mock.GetCurrentSession();
    BOOST_CHECK( session.token.IsEmpty() );
    BOOST_CHECK( session.server_url.IsEmpty() );
}


BOOST_AUTO_TEST_CASE( AuthValidCredentials )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds;
    creds.server_url = wxS( "https://plm.example.com" );
    creds.username = wxS( "validuser" );
    creds.password = PLM_SECURE_STRING( wxS( "validpass" ) );

    PLM_AUTH_RESULT result = mock.Authenticate( creds );

    BOOST_CHECK( result.success );
    BOOST_CHECK( mock.IsAuthenticated() );
    BOOST_CHECK( !result.token.IsEmpty() );
    BOOST_CHECK( result.expires_at > 0 );

    PLM_SESSION session = mock.GetCurrentSession();
    BOOST_CHECK( !session.token.IsEmpty() );
    BOOST_CHECK( session.server_url == creds.server_url );
    BOOST_CHECK( session.user_info.user_id == creds.username );
}


BOOST_AUTO_TEST_CASE( AuthInvalidCredentials )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();
    mock.SetAuthenticateSuccess( false );

    PLM_CREDENTIALS creds;
    creds.server_url = wxS( "https://plm.example.com" );
    creds.username = wxS( "invaliduser" );
    creds.password = PLM_SECURE_STRING( wxS( "wrongpass" ) );

    PLM_AUTH_RESULT result = mock.Authenticate( creds );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( !mock.IsAuthenticated() );
    BOOST_CHECK( !result.error_message.IsEmpty() );
}


BOOST_AUTO_TEST_CASE( AuthTokenExpiry )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds;
    creds.server_url = wxS( "https://plm.example.com" );
    creds.username = wxS( "testuser" );
    mock.Authenticate( creds );

    PLM_SESSION session = mock.GetCurrentSession();

    int64_t now = static_cast<int64_t>( std::time( nullptr ) );
    BOOST_CHECK( session.expires_at > now );

    BOOST_CHECK( !session.IsExpired() );
}


BOOST_AUTO_TEST_CASE( AuthLogout )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds;
    creds.server_url = wxS( "https://plm.example.com" );
    creds.username = wxS( "testuser" );
    mock.Authenticate( creds );

    BOOST_CHECK( mock.IsAuthenticated() );

    bool logoutResult = mock.Logout();
    BOOST_CHECK( logoutResult );
    BOOST_CHECK( !mock.IsAuthenticated() );

    PLM_SESSION session = mock.GetCurrentSession();
    BOOST_CHECK( session.token.IsEmpty() );
}


BOOST_AUTO_TEST_CASE( AuthMultipleLogins )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds1;
    creds1.server_url = wxS( "https://plm1.example.com" );
    creds1.username = wxS( "user1" );
    mock.Authenticate( creds1 );

    PLM_SESSION session1 = mock.GetCurrentSession();
    BOOST_CHECK( session1.user_info.user_id == wxS( "user1" ) );

    PLM_CREDENTIALS creds2;
    creds2.server_url = wxS( "https://plm2.example.com" );
    creds2.username = wxS( "user2" );
    mock.Authenticate( creds2 );

    PLM_SESSION session2 = mock.GetCurrentSession();
    BOOST_CHECK( session2.user_info.user_id == wxS( "user2" ) );
    BOOST_CHECK( session2.server_url == creds2.server_url );
}


BOOST_AUTO_TEST_CASE( AuthRefreshSession )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds;
    creds.server_url = wxS( "https://plm.example.com" );
    creds.username = wxS( "testuser" );
    mock.Authenticate( creds );

    PLM_SESSION sessionBefore = mock.GetCurrentSession();
    int64_t expiresBefore = sessionBefore.expires_at;

    PLM_AUTH_RESULT refreshResult = mock.RefreshSession();
    BOOST_CHECK( refreshResult.success );
    BOOST_CHECK( mock.IsAuthenticated() );

    PLM_SESSION sessionAfter = mock.GetCurrentSession();
    BOOST_CHECK( sessionAfter.expires_at >= expiresBefore );
}


BOOST_AUTO_TEST_CASE( AuthRefreshWithoutSession )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    BOOST_CHECK( !mock.IsAuthenticated() );

    PLM_AUTH_RESULT result = mock.RefreshSession();
    BOOST_CHECK( !result.success );
    BOOST_CHECK( !result.error_message.IsEmpty() );
}


BOOST_AUTO_TEST_CASE( AuthCredentialsClear )
{
    PLM_CREDENTIALS creds;
    creds.server_url = wxS( "https://plm.example.com" );
    creds.username = wxS( "testuser" );
    creds.password = PLM_SECURE_STRING( wxS( "secretpassword" ) );

    BOOST_CHECK( !creds.server_url.IsEmpty() );
    BOOST_CHECK( !creds.username.IsEmpty() );
    BOOST_CHECK( !creds.password.IsEmpty() );

    creds.Clear();

    BOOST_CHECK( creds.server_url.IsEmpty() );
    BOOST_CHECK( creds.username.IsEmpty() );
    BOOST_CHECK( creds.password.IsEmpty() );
}


BOOST_AUTO_TEST_CASE( AuthSecureStringClear )
{
    PLM_SECURE_STRING secure( wxS( "sensitive_data" ) );
    BOOST_CHECK( !secure.IsEmpty() );
    BOOST_CHECK( secure.Get() == wxS( "sensitive_data" ) );

    secure.Clear();
    BOOST_CHECK( secure.IsEmpty() );
}


BOOST_AUTO_TEST_CASE( AuthSessionValidity )
{
    PLM_SESSION session;

    BOOST_CHECK( !session.IsValid() );
    BOOST_CHECK( session.IsExpired() );

    session.token = wxS( "valid_token" );
    session.expires_at = static_cast<int64_t>( std::time( nullptr ) ) + 3600;

    BOOST_CHECK( session.IsValid() );
    BOOST_CHECK( !session.IsExpired() );
}


BOOST_AUTO_TEST_CASE( AuthSessionClear )
{
    PLM_SESSION session;
    session.token = wxS( "token123" );
    session.expires_at = static_cast<int64_t>( std::time( nullptr ) ) + 3600;
    session.server_url = wxS( "https://plm.example.com" );
    session.user_info = PLM_USER_INFO( wxS( "user1" ), wxS( "User One" ), wxS( "user1@example.com" ) );

    BOOST_CHECK( !session.token.IsEmpty() );
    BOOST_CHECK( session.expires_at > 0 );

    session.Clear();

    BOOST_CHECK( session.token.IsEmpty() );
    BOOST_CHECK( session.expires_at == 0 );
    BOOST_CHECK( session.server_url.IsEmpty() );
    BOOST_CHECK( session.user_info.user_id.IsEmpty() );
}


BOOST_AUTO_TEST_CASE( AuthResultFactorySuccess )
{
    int64_t expires = static_cast<int64_t>( std::time( nullptr ) ) + 3600;
    PLM_AUTH_RESULT result = PLM_AUTH_RESULT::Success( wxS( "token123" ), expires );

    BOOST_CHECK( result.success );
    BOOST_CHECK( result.token.Get() == wxS( "token123" ) );
    BOOST_CHECK( result.expires_at == expires );
    BOOST_CHECK( result.error_message.IsEmpty() );
}


BOOST_AUTO_TEST_CASE( AuthResultFactoryFailure )
{
    PLM_AUTH_RESULT result = PLM_AUTH_RESULT::Failure(
            PLM_ERROR_CODE::ERR_AUTH_FAILED, wxS( "Invalid credentials" ) );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_AUTH_FAILED );
    BOOST_CHECK( result.error_message == wxS( "Invalid credentials" ) );
    BOOST_CHECK( result.token.IsEmpty() );
}


BOOST_AUTO_TEST_CASE( AuthUnloadClearsSession )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds;
    creds.server_url = wxS( "https://plm.example.com" );
    creds.username = wxS( "testuser" );
    mock.Authenticate( creds );

    BOOST_CHECK( mock.IsAuthenticated() );

    mock.OnUnload();

    BOOST_CHECK( !mock.IsAuthenticated() );
}


BOOST_AUTO_TEST_CASE( AuthReauthenticateAfterLogout )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds;
    creds.server_url = wxS( "https://plm.example.com" );
    creds.username = wxS( "testuser" );

    mock.Authenticate( creds );
    BOOST_CHECK( mock.IsAuthenticated() );

    mock.Logout();
    BOOST_CHECK( !mock.IsAuthenticated() );

    mock.Authenticate( creds );
    BOOST_CHECK( mock.IsAuthenticated() );

    PLM_SESSION session = mock.GetCurrentSession();
    BOOST_CHECK( !session.token.IsEmpty() );
}


BOOST_AUTO_TEST_SUITE_END()
