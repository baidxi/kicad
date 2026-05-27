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
 * @file test_plm_error_handling.cpp
 * @brief Tests for PLM error handling, including malformed responses and edge cases.
 */

#include <boost/test/unit_test.hpp>
#include "plm_conformance_framework.h"
#include "mock_plm_server.h"


BOOST_AUTO_TEST_SUITE( PlmErrorHandling )


/**
 * Test error code enumeration values.
 */
BOOST_AUTO_TEST_CASE( ErrorCodeValues )
{
    BOOST_CHECK( PLM_ERROR_CODE::SUCCESS == PLM_ERROR_CODE::SUCCESS );
    BOOST_CHECK( PLM_ERROR_CODE::ERR_INTERNAL != PLM_ERROR_CODE::SUCCESS );
    BOOST_CHECK( PLM_ERROR_CODE::ERR_AUTH_FAILED != PLM_ERROR_CODE::ERR_AUTH_REQUIRED );
    BOOST_CHECK( PLM_ERROR_CODE::ERR_LOCK_CONFLICT != PLM_ERROR_CODE::ERR_LOCK_NOT_HELD );
}


/**
 * Test PLM_AUTH_RESULT factory methods.
 */
BOOST_AUTO_TEST_CASE( AuthResultFactories )
{
    auto success = PLM_AUTH_RESULT::Success( wxS( "token123" ), 12345 );
    BOOST_CHECK( success.success );
    BOOST_CHECK( success.error_code == PLM_ERROR_CODE::SUCCESS );
    BOOST_CHECK( success.token.Get() == wxS( "token123" ) );
    BOOST_CHECK_EQUAL( success.expires_at, 12345 );

    auto failure = PLM_AUTH_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_FAILED, wxS( "Bad password" ) );
    BOOST_CHECK( !failure.success );
    BOOST_CHECK( failure.error_code == PLM_ERROR_CODE::ERR_AUTH_FAILED );
    BOOST_CHECK( failure.error_message == wxS( "Bad password" ) );
}


/**
 * Test PLM_CHECKOUT_RESULT factory methods.
 */
BOOST_AUTO_TEST_CASE( CheckoutResultFactories )
{
    PLM_FILE_INFO info( wxS( "id1" ), wxS( "test.kicad_pcb" ), wxS( "1.0" ),
                        PLM_STATE_CATEGORY::EDITABLE );

    auto success = PLM_CHECKOUT_RESULT::Success( wxS( "/tmp/checkout" ), info );
    BOOST_CHECK( success.success );
    BOOST_CHECK( success.error_code == PLM_ERROR_CODE::SUCCESS );
    BOOST_CHECK( success.local_path == wxS( "/tmp/checkout" ) );
    BOOST_CHECK( success.file_info.id == wxS( "id1" ) );

    auto failure = PLM_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_FILE_NOT_FOUND,
                                                   wxS( "File does not exist" ) );
    BOOST_CHECK( !failure.success );
    BOOST_CHECK( failure.error_code == PLM_ERROR_CODE::ERR_FILE_NOT_FOUND );
    BOOST_CHECK( failure.error_message == wxS( "File does not exist" ) );
}


/**
 * Test PLM_CHECKIN_RESULT factory methods.
 */
BOOST_AUTO_TEST_CASE( CheckinResultFactories )
{
    PLM_FILE_INFO info( wxS( "id1" ), wxS( "test.kicad_pcb" ), wxS( "2.0" ),
                        PLM_STATE_CATEGORY::EDITABLE );

    auto success = PLM_CHECKIN_RESULT::Success( wxS( "2.0" ), info );
    BOOST_CHECK( success.success );
    BOOST_CHECK( success.error_code == PLM_ERROR_CODE::SUCCESS );
    BOOST_CHECK( success.new_revision == wxS( "2.0" ) );
    BOOST_CHECK( success.file_info.revision == wxS( "2.0" ) );

    auto failure = PLM_CHECKIN_RESULT::Failure( PLM_ERROR_CODE::ERR_LOCK_NOT_HELD,
                                                  wxS( "Not locked by you" ) );
    BOOST_CHECK( !failure.success );
    BOOST_CHECK( failure.error_code == PLM_ERROR_CODE::ERR_LOCK_NOT_HELD );
}


/**
 * Test PLM_LOCK_RESULT factory methods.
 */
BOOST_AUTO_TEST_CASE( LockResultFactories )
{
    int64_t now = static_cast<int64_t>( std::time( nullptr ) );

    PLM_LOCK_INFO lockInfo = PLM_LOCK_INFO::Locked( wxS( "user1" ), now, true );

    auto success = PLM_LOCK_RESULT::Success( lockInfo );
    BOOST_CHECK( success.success );
    BOOST_CHECK( success.error_code == PLM_ERROR_CODE::SUCCESS );
    BOOST_CHECK( success.lock_info.is_locked );
    BOOST_CHECK( success.lock_info.locked_by_me );

    PLM_LOCK_INFO otherLock = PLM_LOCK_INFO::Locked( wxS( "user2" ), now, false );
    auto conflict = PLM_LOCK_RESULT::Conflict( otherLock );
    BOOST_CHECK( !conflict.success );
    BOOST_CHECK( conflict.error_code == PLM_ERROR_CODE::ERR_LOCK_CONFLICT );
    BOOST_CHECK( conflict.lock_info.lock_owner == wxS( "user2" ) );
}


/**
 * Test error injection in mock server - auth errors.
 */
BOOST_AUTO_TEST_CASE( MockServerAuthErrorInjection )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    mock.GetServer().AuthErrors().inject_error = true;
    mock.GetServer().AuthErrors().error_code = PLM_ERROR_CODE::ERR_NETWORK_UNREACHABLE;
    mock.GetServer().AuthErrors().error_message = wxS( "Connection refused" );

    PLM_CREDENTIALS creds( wxS( "http://test" ), wxS( "testuser" ), wxS( "password" ) );
    PLM_AUTH_RESULT result = mock.Authenticate( creds );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_NETWORK_UNREACHABLE );
    BOOST_CHECK( result.error_message == wxS( "Connection refused" ) );

    mock.GetServer().AuthErrors().Reset();

    result = mock.Authenticate( creds );
    BOOST_CHECK( result.success );
}


/**
 * Test error injection in mock server - checkout errors.
 */
BOOST_AUTO_TEST_CASE( MockServerCheckoutErrorInjection )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds( wxS( "http://test" ), wxS( "testuser" ), wxS( "password" ) );
    mock.Authenticate( creds );

    PLM_FILE_INFO file( wxS( "proj1" ), wxS( "test.kicad_pcb" ), wxS( "1.0" ),
                        PLM_STATE_CATEGORY::EDITABLE );
    mock.GetServer().AddFile( file );

    mock.GetServer().CheckoutErrors().inject_error = true;
    mock.GetServer().CheckoutErrors().error_code = PLM_ERROR_CODE::ERR_NETWORK_TIMEOUT;
    mock.GetServer().CheckoutErrors().error_message = wxS( "Network timeout" );

    PLM_CHECKOUT_OPTIONS options;
    PLM_CHECKOUT_RESULT result = mock.Checkout( wxS( "proj1" ), wxS( "/tmp/proj1" ), options );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_NETWORK_TIMEOUT );

    mock.GetServer().CheckoutErrors().Reset();

    result = mock.Checkout( wxS( "proj1" ), wxS( "/tmp/proj1" ), options );
    BOOST_CHECK( result.success );
}


/**
 * Test error injection in mock server - checkin errors.
 */
BOOST_AUTO_TEST_CASE( MockServerCheckinErrorInjection )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds( wxS( "http://test" ), wxS( "testuser" ), wxS( "password" ) );
    mock.Authenticate( creds );

    PLM_FILE_INFO file( wxS( "proj1" ), wxS( "test.kicad_pcb" ), wxS( "1.0" ),
                        PLM_STATE_CATEGORY::EDITABLE );
    mock.GetServer().AddFile( file );

    PLM_CHECKOUT_OPTIONS coOptions;
    mock.Checkout( wxS( "proj1" ), wxS( "/tmp/proj1" ), coOptions );

    mock.GetServer().CheckinErrors().inject_error = true;
    mock.GetServer().CheckinErrors().error_code = PLM_ERROR_CODE::ERR_INVALID_PARAMETER;
    mock.GetServer().CheckinErrors().error_message = wxS( "Missing required field" );

    PLM_CHECKIN_OPTIONS ciOptions( wxS( "test commit" ) );
    PLM_CHECKIN_RESULT result = mock.Checkin( wxS( "/tmp/proj1" ), ciOptions );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_INVALID_PARAMETER );

    mock.GetServer().CheckinErrors().Reset();

    result = mock.Checkin( wxS( "/tmp/proj1" ), ciOptions );
    BOOST_CHECK( result.success );
}


/**
 * Test error injection in mock server - lock errors.
 */
BOOST_AUTO_TEST_CASE( MockServerLockErrorInjection )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds( wxS( "http://test" ), wxS( "testuser" ), wxS( "password" ) );
    mock.Authenticate( creds );

    mock.GetServer().LockErrors().inject_error = true;
    mock.GetServer().LockErrors().error_code = PLM_ERROR_CODE::ERR_LOCK_CONFLICT;
    mock.GetServer().LockErrors().error_message = wxS( "Conflict with server" );

    PLM_LOCK_REQUEST request( wxS( "proj1" ) );
    PLM_LOCK_RESULT result = mock.Lock( request );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_LOCK_CONFLICT );
}


/**
 * Test error injection in mock server - navigation errors.
 */
BOOST_AUTO_TEST_CASE( MockServerNavigationErrorInjection )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds( wxS( "http://test" ), wxS( "testuser" ), wxS( "password" ) );
    mock.Authenticate( creds );

    PLM_FOLDER folder( wxS( "root1" ), wxS( "Root Folder" ) );
    mock.GetServer().AddFolder( folder );

    std::vector<PLM_FOLDER> folders = mock.GetRootFolders();
    BOOST_CHECK_EQUAL( folders.size(), 1 );

    mock.GetServer().NavigationErrors().inject_error = true;

    folders = mock.GetRootFolders();
    BOOST_CHECK( folders.empty() );

    mock.GetServer().NavigationErrors().Reset();

    folders = mock.GetRootFolders();
    BOOST_CHECK_EQUAL( folders.size(), 1 );
}


/**
 * Test error code coverage.
 */
BOOST_AUTO_TEST_CASE( AllErrorCodesCovered )
{
    std::vector<PLM_ERROR_CODE> codes = { PLM_ERROR_CODE::SUCCESS,
                                           PLM_ERROR_CODE::ERR_INTERNAL,
                                           PLM_ERROR_CODE::ERR_NETWORK_UNREACHABLE,
                                           PLM_ERROR_CODE::ERR_NETWORK_TIMEOUT,
                                           PLM_ERROR_CODE::ERR_NETWORK_SSL,
                                           PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                           PLM_ERROR_CODE::ERR_AUTH_FAILED,
                                           PLM_ERROR_CODE::ERR_AUTH_EXPIRED,
                                           PLM_ERROR_CODE::ERR_AUTH_INSUFFICIENT,
                                           PLM_ERROR_CODE::ERR_FILE_NOT_FOUND,
                                           PLM_ERROR_CODE::ERR_FILE_ACCESS_DENIED,
                                           PLM_ERROR_CODE::ERR_REVISION_NOT_FOUND,
                                           PLM_ERROR_CODE::ERR_LOCK_CONFLICT,
                                           PLM_ERROR_CODE::ERR_LOCK_NOT_HELD,
                                           PLM_ERROR_CODE::ERR_CANCELLED,
                                           PLM_ERROR_CODE::ERR_NOT_SUPPORTED };

    for( const auto& code : codes )
    {
        auto result = PLM_AUTH_RESULT::Failure( code, wxS( "Test error" ) );
        BOOST_CHECK( result.error_code == code );
    }
}


/**
 * Test checkout without authentication.
 */
BOOST_AUTO_TEST_CASE( CheckoutWithoutAuth )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_CHECKOUT_OPTIONS options;
    PLM_CHECKOUT_RESULT result = mock.Checkout( wxS( "proj1" ), wxS( "/tmp/proj1" ), options );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_AUTH_REQUIRED );
}


/**
 * Test checkin without authentication.
 */
BOOST_AUTO_TEST_CASE( CheckinWithoutAuth )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_CHECKIN_OPTIONS options( wxS( "test" ) );
    PLM_CHECKIN_RESULT result = mock.Checkin( wxS( "/tmp/proj1" ), options );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_AUTH_REQUIRED );
}


/**
 * Test lock without authentication.
 */
BOOST_AUTO_TEST_CASE( LockWithoutAuth )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_LOCK_REQUEST request( wxS( "proj1" ) );
    PLM_LOCK_RESULT result = mock.Lock( request );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_AUTH_REQUIRED );
}


/**
 * Test unlock without authentication.
 */
BOOST_AUTO_TEST_CASE( UnlockWithoutAuth )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_UNLOCK_REQUEST request( wxS( "proj1" ) );
    PLM_LOCK_RESULT result = mock.Unlock( request );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_AUTH_REQUIRED );
}


/**
 * Test checkout of non-existent file.
 */
BOOST_AUTO_TEST_CASE( CheckoutNonExistentFile )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds( wxS( "http://test" ), wxS( "testuser" ), wxS( "password" ) );
    mock.Authenticate( creds );

    PLM_CHECKOUT_OPTIONS options;
    PLM_CHECKOUT_RESULT result = mock.Checkout( wxS( "nonexistent" ), wxS( "/tmp/proj1" ), options );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_FILE_NOT_FOUND );
}


/**
 * Test checkin of non-checked-out file.
 */
BOOST_AUTO_TEST_CASE( CheckinNonCheckedOutFile )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds( wxS( "http://test" ), wxS( "testuser" ), wxS( "password" ) );
    mock.Authenticate( creds );

    PLM_CHECKIN_OPTIONS options( wxS( "test" ) );
    PLM_CHECKIN_RESULT result = mock.Checkin( wxS( "/tmp/not-checked-out" ), options );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_FILE_NOT_FOUND );
}


/**
 * Test lock conflict detection.
 */
BOOST_AUTO_TEST_CASE( LockConflictDetection )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds( wxS( "http://test" ), wxS( "testuser" ), wxS( "password" ) );
    mock.Authenticate( creds );

    int64_t now = static_cast<int64_t>( std::time( nullptr ) );
    PLM_LOCK_INFO existingLock = PLM_LOCK_INFO::Locked( wxS( "otheruser" ), now, false );
    mock.GetServer().SetLock( wxS( "proj1" ), existingLock );

    PLM_LOCK_REQUEST request( wxS( "proj1" ) );
    PLM_LOCK_RESULT result = mock.Lock( request );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_LOCK_CONFLICT );
    BOOST_CHECK( result.lock_info.lock_owner == wxS( "otheruser" ) );
}


/**
 * Test force lock overrides conflict.
 */
BOOST_AUTO_TEST_CASE( ForceLockOverridesConflict )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds( wxS( "http://test" ), wxS( "testuser" ), wxS( "password" ) );
    mock.Authenticate( creds );

    int64_t now = static_cast<int64_t>( std::time( nullptr ) );
    PLM_LOCK_INFO existingLock = PLM_LOCK_INFO::Locked( wxS( "otheruser" ), now, false );
    mock.GetServer().SetLock( wxS( "proj1" ), existingLock );

    PLM_LOCK_REQUEST request( wxS( "proj1" ), true );
    PLM_LOCK_RESULT result = mock.Lock( request );

    BOOST_CHECK( result.success );
    BOOST_CHECK( result.lock_info.locked_by_me );
}


/**
 * Test unlock of lock held by another user.
 */
BOOST_AUTO_TEST_CASE( UnlockOtherUsersLock )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds( wxS( "http://test" ), wxS( "testuser" ), wxS( "password" ) );
    mock.Authenticate( creds );

    int64_t now = static_cast<int64_t>( std::time( nullptr ) );
    PLM_LOCK_INFO existingLock = PLM_LOCK_INFO::Locked( wxS( "otheruser" ), now, false );
    mock.GetServer().SetLock( wxS( "proj1" ), existingLock );

    PLM_UNLOCK_REQUEST request( wxS( "proj1" ) );
    PLM_LOCK_RESULT result = mock.Unlock( request );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_LOCK_NOT_HELD );
}


/**
 * Test force unlock of another user's lock.
 */
BOOST_AUTO_TEST_CASE( ForceUnlockOtherUsersLock )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds( wxS( "http://test" ), wxS( "testuser" ), wxS( "password" ) );
    mock.Authenticate( creds );

    int64_t now = static_cast<int64_t>( std::time( nullptr ) );
    PLM_LOCK_INFO existingLock = PLM_LOCK_INFO::Locked( wxS( "otheruser" ), now, false );
    mock.GetServer().SetLock( wxS( "proj1" ), existingLock );

    PLM_UNLOCK_REQUEST request( wxS( "proj1" ), true );
    PLM_LOCK_RESULT result = mock.Unlock( request );

    BOOST_CHECK( result.success );
    BOOST_CHECK( !result.lock_info.is_locked );
}


/**
 * Test authentication with invalid credentials.
 */
BOOST_AUTO_TEST_CASE( AuthWithInvalidCredentials )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds( wxS( "http://test" ), wxS( "invaliduser" ), wxS( "wrongpassword" ) );
    PLM_AUTH_RESULT result = mock.Authenticate( creds );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_AUTH_FAILED );
}


/**
 * Test refresh session without initial authentication.
 */
BOOST_AUTO_TEST_CASE( RefreshSessionWithoutAuth )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_AUTH_RESULT result = mock.RefreshSession();

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_AUTH_REQUIRED );
}


/**
 * Test cancel checkout of non-existent checkout.
 */
BOOST_AUTO_TEST_CASE( CancelNonExistentCheckout )
{
    MOCK_FULL_PLM_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds( wxS( "http://test" ), wxS( "testuser" ), wxS( "password" ) );
    mock.Authenticate( creds );

    PLM_CANCEL_CHECKOUT_RESULT result = mock.CancelCheckout( wxS( "/tmp/not-checked-out" ) );
    BOOST_CHECK( !result.success );
}


/**
 * Test batch item result error message retention.
 */
BOOST_AUTO_TEST_CASE( BatchItemResultErrorMessage )
{
    PLM_BATCH_ITEM_RESULT item( wxS( "proj1" ), false,
                                PLM_ERROR_CODE::ERR_NETWORK_TIMEOUT,
                                wxS( "Connection timed out after 30 seconds" ) );

    BOOST_CHECK( !item.success );
    BOOST_CHECK( item.project_id == wxS( "proj1" ) );
    BOOST_CHECK( item.error_code == PLM_ERROR_CODE::ERR_NETWORK_TIMEOUT );
    BOOST_CHECK( item.error_message == wxS( "Connection timed out after 30 seconds" ) );
}


/**
 * Test batch result error aggregation.
 */
BOOST_AUTO_TEST_CASE( BatchResultErrorAggregation )
{
    PLM_BATCH_RESULT result;

    result.AddResult( PLM_BATCH_ITEM_RESULT( wxS( "proj1" ), true ) );
    BOOST_CHECK( result.success );
    BOOST_CHECK_EQUAL( result.failure_count, 0 );

    result.AddResult( PLM_BATCH_ITEM_RESULT( wxS( "proj2" ), false,
                                              PLM_ERROR_CODE::ERR_FILE_NOT_FOUND,
                                              wxS( "Not found" ) ) );
    BOOST_CHECK( !result.success );
    BOOST_CHECK_EQUAL( result.failure_count, 1 );

    result.AddResult( PLM_BATCH_ITEM_RESULT( wxS( "proj3" ), false,
                                              PLM_ERROR_CODE::ERR_LOCK_CONFLICT,
                                              wxS( "Locked" ) ) );
    BOOST_CHECK( !result.success );
    BOOST_CHECK_EQUAL( result.failure_count, 2 );
    BOOST_CHECK_EQUAL( result.success_count, 1 );
    BOOST_CHECK_EQUAL( result.total_count, 3 );
}


BOOST_AUTO_TEST_SUITE_END()
