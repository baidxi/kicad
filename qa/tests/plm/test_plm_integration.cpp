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
 * @file test_plm_integration.cpp
 * @brief Integration tests for PLM plugin workflows.
 *
 * These tests exercise complete workflows using the mock PLM server
 * to verify end-to-end functionality.
 */

#include <boost/test/unit_test.hpp>
#include "mock_plm_server.h"


struct PLM_INTEGRATION_FIXTURE
{
    PLM_INTEGRATION_FIXTURE()
    {
        plugin.OnLoad();
        SetupTestData();
    }

    ~PLM_INTEGRATION_FIXTURE()
    {
        plugin.OnUnload();
    }

    void SetupTestData()
    {
        MOCK_PLM_SERVER& server = plugin.GetServer();

        server.AddFolder( PLM_FOLDER( wxS( "root" ), wxS( "Projects" ) ) );
        server.AddFolder( PLM_FOLDER( wxS( "electronics" ), wxS( "Electronics" ), wxS( "root" ), true ) );
        server.AddFolder( PLM_FOLDER( wxS( "mechanical" ), wxS( "Mechanical" ), wxS( "root" ) ) );
        server.AddFolder( PLM_FOLDER( wxS( "proj1" ), wxS( "Project 1" ), wxS( "electronics" ) ) );

        PLM_FILE_INFO file1( wxS( "file-001" ), wxS( "Main Board" ), wxS( "1.0" ),
                             PLM_STATE_CATEGORY::EDITABLE );
        file1.folder_id = wxS( "electronics" );
        server.AddFile( file1 );

        PLM_FILE_INFO file2( wxS( "file-002" ), wxS( "Power Supply" ), wxS( "2.1" ),
                             PLM_STATE_CATEGORY::RELEASED );
        file2.folder_id = wxS( "electronics" );
        server.AddFile( file2 );

        PLM_FILE_INFO file3( wxS( "file-003" ), wxS( "Enclosure" ), wxS( "1.0" ),
                             PLM_STATE_CATEGORY::EDITABLE );
        file3.folder_id = wxS( "mechanical" );
        server.AddFile( file3 );
    }

    void Authenticate()
    {
        PLM_CREDENTIALS creds;
        creds.server_url = wxS( "https://plm.test.com" );
        creds.username = wxS( "testuser" );
        creds.password = PLM_SECURE_STRING( wxS( "testpass" ) );
        plugin.Authenticate( creds );
    }

    MOCK_FULL_PLM_PLUGIN plugin;
};


BOOST_FIXTURE_TEST_SUITE( PlmIntegration, PLM_INTEGRATION_FIXTURE )


BOOST_AUTO_TEST_CASE( FullAuthenticationWorkflow )
{
    BOOST_CHECK( !plugin.IsAuthenticated() );

    PLM_CREDENTIALS creds;
    creds.server_url = wxS( "https://plm.test.com" );
    creds.username = wxS( "testuser" );
    creds.password = PLM_SECURE_STRING( wxS( "testpass" ) );

    PLM_AUTH_RESULT result = plugin.Authenticate( creds );

    BOOST_CHECK( result.success );
    BOOST_CHECK( plugin.IsAuthenticated() );
    BOOST_CHECK( !result.token.IsEmpty() );

    PLM_SESSION session = plugin.GetCurrentSession();
    BOOST_CHECK( session.user_info.user_id == wxS( "testuser" ) );

    PLM_AUTH_RESULT refresh = plugin.RefreshSession();
    BOOST_CHECK( refresh.success );
    BOOST_CHECK( plugin.IsAuthenticated() );

    bool logoutResult = plugin.Logout();
    BOOST_CHECK( logoutResult );
    BOOST_CHECK( !plugin.IsAuthenticated() );
}


BOOST_AUTO_TEST_CASE( NavigationWorkflow )
{
    Authenticate();

    std::vector<PLM_FOLDER> rootFolders = plugin.GetRootFolders();
    BOOST_CHECK( rootFolders.size() == 1 );
    BOOST_CHECK( rootFolders[0].id == wxS( "root" ) );
    BOOST_CHECK( rootFolders[0].name == wxS( "Projects" ) );

    std::vector<PLM_FOLDER> childFolders = plugin.GetChildFolders( wxS( "root" ) );
    BOOST_CHECK( childFolders.size() == 2 );

    PLM_LIST_OPTIONS opts;
    PLM_LIST_RESULT<PLM_FILE_INFO> files = plugin.ListFiles( wxS( "electronics" ), opts );

    BOOST_CHECK( files.Count() == 2 );
    BOOST_CHECK( files.total_count == 2 );
}


BOOST_AUTO_TEST_CASE( CheckoutCheckinWorkflow )
{
    Authenticate();

    PLM_CHECKOUT_OPTIONS checkoutOpts( wxS( "/tmp/test_project" ) );
    PLM_CHECKOUT_RESULT checkoutResult = plugin.Checkout( wxS( "file-001" ), wxS( "/tmp/test_project" ),
                                                           checkoutOpts );

    BOOST_CHECK( checkoutResult.success );
    BOOST_CHECK( checkoutResult.local_path == wxS( "/tmp/test_project" ) );
    BOOST_CHECK( checkoutResult.file_info.id == wxS( "file-001" ) );

    PLM_LOCK_INFO lockInfo = plugin.GetLockInfo( wxS( "file-001" ) );
    BOOST_CHECK( lockInfo.is_locked );
    BOOST_CHECK( lockInfo.locked_by_me );

    PLM_CHECKIN_OPTIONS checkinOpts( wxS( "Updated schematic design" ) );
    PLM_CHECKIN_RESULT checkinResult = plugin.Checkin( wxS( "/tmp/test_project" ), checkinOpts );

    BOOST_CHECK( checkinResult.success );
    BOOST_CHECK( !checkinResult.new_revision.IsEmpty() );

    lockInfo = plugin.GetLockInfo( wxS( "file-001" ) );
    BOOST_CHECK( !lockInfo.is_locked );
}


BOOST_AUTO_TEST_CASE( LockingWorkflow )
{
    Authenticate();

    PLM_LOCK_INFO initialLock = plugin.GetLockInfo( wxS( "file-003" ) );
    BOOST_CHECK( !initialLock.is_locked );

    PLM_LOCK_REQUEST lockReq( wxS( "file-003" ), false, wxS( "Working on enclosure design" ) );
    PLM_LOCK_RESULT lockResult = plugin.Lock( lockReq );

    BOOST_CHECK( lockResult.success );
    BOOST_CHECK( lockResult.lock_info.is_locked );
    BOOST_CHECK( lockResult.lock_info.locked_by_me );

    PLM_LOCK_INFO lockInfo = plugin.GetLockInfo( wxS( "file-003" ) );
    BOOST_CHECK( lockInfo.is_locked );
    BOOST_CHECK( lockInfo.locked_by_me );

    PLM_UNLOCK_REQUEST unlockReq( wxS( "file-003" ) );
    PLM_LOCK_RESULT unlockResult = plugin.Unlock( unlockReq );

    BOOST_CHECK( unlockResult.success );
    BOOST_CHECK( !unlockResult.lock_info.is_locked );
}


BOOST_AUTO_TEST_CASE( RevisionHistoryWorkflow )
{
    Authenticate();

    plugin.GetServer().CreateRevision( wxS( "file-001" ), wxS( "user1" ), wxS( "Initial" ) );
    plugin.GetServer().CreateRevision( wxS( "file-001" ), wxS( "user1" ), wxS( "Update 1" ) );
    plugin.GetServer().CreateRevision( wxS( "file-001" ), wxS( "user2" ), wxS( "Update 2" ) );

    PLM_REVISION_HISTORY history = plugin.GetRevisionHistory( wxS( "file-001" ) );

    BOOST_CHECK( !history.IsEmpty() );
    BOOST_CHECK( history.Count() >= 3 );

    const PLM_REVISION* latest = history.GetLatest();
    BOOST_CHECK( latest != nullptr );
    BOOST_CHECK( latest->comment == wxS( "Update 2" ) );

    if( !history.revisions.empty() )
    {
        PLM_REVISION rev = plugin.GetRevision( wxS( "file-001" ), history.revisions[0].revision_id );
        BOOST_CHECK( !rev.revision_id.IsEmpty() );
    }
}


BOOST_AUTO_TEST_CASE( CancelCheckoutWorkflow )
{
    Authenticate();

    PLM_CHECKOUT_OPTIONS checkoutOpts( wxS( "/tmp/cancel_test" ) );
    PLM_CHECKOUT_RESULT checkoutResult = plugin.Checkout( wxS( "file-003" ), wxS( "/tmp/cancel_test" ),
                                                           checkoutOpts );
    BOOST_CHECK( checkoutResult.success );

    PLM_LOCK_INFO lockBefore = plugin.GetLockInfo( wxS( "file-003" ) );
    BOOST_CHECK( lockBefore.is_locked );

    PLM_CANCEL_CHECKOUT_RESULT canceled = plugin.CancelCheckout( wxS( "/tmp/cancel_test" ) );
    BOOST_CHECK( canceled.success );

    PLM_LOCK_INFO lockAfter = plugin.GetLockInfo( wxS( "file-003" ) );
    BOOST_CHECK( !lockAfter.is_locked );
}


BOOST_AUTO_TEST_CASE( AuthenticationRequiredForOperations )
{
    BOOST_CHECK( !plugin.IsAuthenticated() );

    std::vector<PLM_FOLDER> folders = plugin.GetRootFolders();
    BOOST_CHECK( folders.empty() );

    PLM_CHECKOUT_OPTIONS checkoutOpts( wxS( "/tmp/test" ) );
    PLM_CHECKOUT_RESULT checkoutResult = plugin.Checkout( wxS( "file-001" ), wxS( "/tmp/test" ),
                                                           checkoutOpts );
    BOOST_CHECK( !checkoutResult.success );
    BOOST_CHECK( checkoutResult.error_code == PLM_ERROR_CODE::ERR_AUTH_REQUIRED );

    PLM_LOCK_REQUEST lockReq( wxS( "file-001" ) );
    PLM_LOCK_RESULT lockResult = plugin.Lock( lockReq );
    BOOST_CHECK( !lockResult.success );
}


BOOST_AUTO_TEST_CASE( LockConflictHandling )
{
    Authenticate();

    plugin.GetServer().SetLock( wxS( "file-002" ),
                                 PLM_LOCK_INFO::Locked( wxS( "other_user" ), 12345, false ) );

    PLM_LOCK_REQUEST lockReq( wxS( "file-002" ) );
    PLM_LOCK_RESULT result = plugin.Lock( lockReq );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_LOCK_CONFLICT );
    BOOST_CHECK( result.lock_info.lock_owner == wxS( "other_user" ) );

    PLM_LOCK_REQUEST forceLockReq( wxS( "file-002" ), true );
    PLM_LOCK_RESULT forceResult = plugin.Lock( forceLockReq );

    BOOST_CHECK( forceResult.success );
    BOOST_CHECK( forceResult.lock_info.locked_by_me );
}


BOOST_AUTO_TEST_CASE( ErrorInjection )
{
    Authenticate();

    plugin.GetServer().CheckoutErrors().inject_error = true;
    plugin.GetServer().CheckoutErrors().error_code = PLM_ERROR_CODE::ERR_NETWORK_TIMEOUT;
    plugin.GetServer().CheckoutErrors().error_message = wxS( "Network timeout" );

    PLM_CHECKOUT_OPTIONS opts( wxS( "/tmp/test" ) );
    PLM_CHECKOUT_RESULT result = plugin.Checkout( wxS( "file-001" ), wxS( "/tmp/test" ), opts );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_NETWORK_TIMEOUT );
    BOOST_CHECK( result.error_message == wxS( "Network timeout" ) );

    plugin.GetServer().CheckoutErrors().Reset();

    PLM_CHECKOUT_RESULT result2 = plugin.Checkout( wxS( "file-001" ), wxS( "/tmp/test" ), opts );
    BOOST_CHECK( result2.success );
}


BOOST_AUTO_TEST_CASE( FailAfterNCalls )
{
    Authenticate();

    plugin.GetServer().CheckoutErrors().inject_error = true;
    plugin.GetServer().CheckoutErrors().fail_after_calls = 2;
    plugin.GetServer().CheckoutErrors().error_code = PLM_ERROR_CODE::ERR_NETWORK_UNREACHABLE;

    PLM_CHECKOUT_OPTIONS opts( wxS( "/tmp/test" ) );

    PLM_CHECKOUT_RESULT result1 = plugin.Checkout( wxS( "file-001" ), wxS( "/tmp/test1" ), opts );
    BOOST_CHECK( result1.success );

    PLM_CHECKOUT_RESULT result2 = plugin.Checkout( wxS( "file-003" ), wxS( "/tmp/test2" ), opts );
    BOOST_CHECK( result2.success );

    PLM_CHECKOUT_RESULT result3 = plugin.Checkout( wxS( "file-002" ), wxS( "/tmp/test3" ), opts );
    BOOST_CHECK( !result3.success );
    BOOST_CHECK( result3.error_code == PLM_ERROR_CODE::ERR_NETWORK_UNREACHABLE );
}


BOOST_AUTO_TEST_CASE( PaginatedFileList )
{
    Authenticate();

    MOCK_PLM_SERVER& server = plugin.GetServer();

    for( int i = 10; i < 35; i++ )
    {
        PLM_FILE_INFO file( wxString::Format( wxS( "file-%03d" ), i ),
                            wxString::Format( wxS( "Test File %d" ), i ),
                            wxS( "1.0" ), PLM_STATE_CATEGORY::EDITABLE );
        file.folder_id = wxS( "proj1" );
        server.AddFile( file );
    }

    PLM_LIST_OPTIONS opts( 0, 10 );
    PLM_LIST_RESULT<PLM_FILE_INFO> page1 = plugin.ListFiles( wxS( "proj1" ), opts );

    BOOST_CHECK( page1.Count() == 10 );
    BOOST_CHECK( page1.total_count == 25 );
    BOOST_CHECK( page1.HasNextPage() );
    BOOST_CHECK( !page1.HasPreviousPage() );
    BOOST_CHECK( page1.CurrentPage() == 1 );
    BOOST_CHECK( page1.TotalPages() == 3 );

    PLM_LIST_OPTIONS opts2 = opts.NextPage( 10 );
    PLM_LIST_RESULT<PLM_FILE_INFO> page2 = plugin.ListFiles( wxS( "proj1" ), opts2 );

    BOOST_CHECK( page2.Count() == 10 );
    BOOST_CHECK( page2.HasNextPage() );
    BOOST_CHECK( page2.HasPreviousPage() );
    BOOST_CHECK( page2.CurrentPage() == 2 );

    PLM_LIST_OPTIONS opts3 = opts2.NextPage( 10 );
    PLM_LIST_RESULT<PLM_FILE_INFO> page3 = plugin.ListFiles( wxS( "proj1" ), opts3 );

    BOOST_CHECK( page3.Count() == 5 );
    BOOST_CHECK( !page3.HasNextPage() );
    BOOST_CHECK( page3.HasPreviousPage() );
    BOOST_CHECK( page3.CurrentPage() == 3 );
}


BOOST_AUTO_TEST_SUITE_END()
