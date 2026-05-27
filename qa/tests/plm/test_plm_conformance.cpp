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

#include <boost/test/unit_test.hpp>
#include "plm_conformance_framework.h"


BOOST_AUTO_TEST_SUITE( PlmConformance )


BOOST_AUTO_TEST_CASE( MockPluginInfo )
{
    PLM_MOCK_PLUGIN mock;
    PLM_PLUGIN_INFO info = mock.GetPluginInfo();

    BOOST_CHECK( !info.plugin_id.IsEmpty() );
    BOOST_CHECK( !info.display_name.IsEmpty() );
    BOOST_CHECK( info.plugin_id == wxS( "plm-mock" ) );
}


BOOST_AUTO_TEST_CASE( MockCapabilities )
{
    PLM_MOCK_PLUGIN mock;
    PLM_CAPABILITIES caps = mock.GetCapabilities();

    BOOST_CHECK( caps.supports_checkout );
    BOOST_CHECK( caps.supports_locking );
    BOOST_CHECK( caps.supports_revisions );
}


BOOST_AUTO_TEST_CASE( MockLoadUnload )
{
    PLM_MOCK_PLUGIN mock;

    BOOST_CHECK( mock.OnLoad() );
    mock.OnUnload();
    BOOST_CHECK( !mock.IsAuthenticated() );
}


BOOST_AUTO_TEST_CASE( MockAuthentication )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    BOOST_CHECK( !mock.IsAuthenticated() );

    PLM_CREDENTIALS creds;
    creds.server_url = wxS( "https://test.example.com" );
    creds.username = wxS( "testuser" );
    creds.password = PLM_SECURE_STRING( wxS( "testpass" ) );

    PLM_AUTH_RESULT result = mock.Authenticate( creds );

    BOOST_CHECK( result.success );
    BOOST_CHECK( mock.IsAuthenticated() );
    BOOST_CHECK( !result.token.IsEmpty() );

    PLM_SESSION session = mock.GetCurrentSession();
    BOOST_CHECK( session.user_info.user_id == wxS( "testuser" ) );

    BOOST_CHECK( mock.Logout() );
    BOOST_CHECK( !mock.IsAuthenticated() );
}


BOOST_AUTO_TEST_CASE( MockAuthenticationFailure )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();
    mock.SetAuthenticateSuccess( false );

    PLM_CREDENTIALS creds;
    creds.server_url = wxS( "https://test.example.com" );
    creds.username = wxS( "testuser" );

    PLM_AUTH_RESULT result = mock.Authenticate( creds );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( !mock.IsAuthenticated() );
}


BOOST_AUTO_TEST_CASE( MockNavigation )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    // Not authenticated - should return empty
    std::vector<PLM_FOLDER> folders = mock.GetRootFolders();
    BOOST_CHECK( folders.empty() );

    // Set up folders and authenticate
    std::vector<PLM_FOLDER> testFolders;
    testFolders.push_back( PLM_FOLDER( wxS( "folder1" ), wxS( "Folder 1" ) ) );
    testFolders.push_back( PLM_FOLDER( wxS( "folder2" ), wxS( "Folder 2" ) ) );
    mock.SetRootFolders( testFolders );

    PLM_CREDENTIALS creds;
    creds.server_url = wxS( "https://test.example.com" );
    creds.username = wxS( "testuser" );
    mock.Authenticate( creds );

    folders = mock.GetRootFolders();
    BOOST_CHECK( folders.size() == 2 );
    BOOST_CHECK( folders[0].name == wxS( "Folder 1" ) );
}


BOOST_AUTO_TEST_CASE( ConformanceTesterBasic )
{
    PLM_MOCK_PLUGIN mock;
    PLM_CONFORMANCE_TESTER tester;
    tester.SetPlugin( &mock );

    std::vector<wxString> testNames = tester.GetTestNames();
    BOOST_CHECK( !testNames.empty() );

    PLM_CONFORMANCE_REPORT report = tester.RunAllTests();
    BOOST_CHECK( report.plugin_id == wxS( "plm-mock" ) );
    BOOST_CHECK( report.total_tests > 0 );
}


BOOST_AUTO_TEST_CASE( ConformanceSingleTest )
{
    PLM_MOCK_PLUGIN mock;
    PLM_CONFORMANCE_TESTER tester;
    tester.SetPlugin( &mock );

    PLM_CONFORMANCE_RESULT result = tester.RunTest( wxS( "PluginInfo" ) );
    BOOST_CHECK( result.Passed() );
    BOOST_CHECK( result.test_name == wxS( "PluginInfo" ) );
}


BOOST_AUTO_TEST_CASE( ConformanceInvalidTest )
{
    PLM_MOCK_PLUGIN mock;
    PLM_CONFORMANCE_TESTER tester;
    tester.SetPlugin( &mock );

    PLM_CONFORMANCE_RESULT result = tester.RunTest( wxS( "NonexistentTest" ) );
    BOOST_CHECK( result.status == PLM_TEST_STATUS::ERROR );
}


BOOST_AUTO_TEST_SUITE_END()
