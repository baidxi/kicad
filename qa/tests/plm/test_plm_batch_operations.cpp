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
 * @file test_plm_batch_operations.cpp
 * @brief Tests for PLM batch checkout and checkin operations.
 */

#include <boost/test/unit_test.hpp>
#include "plm_conformance_framework.h"


BOOST_AUTO_TEST_SUITE( PlmBatchOperations )


/**
 * Test batch checkout with empty project list.
 */
BOOST_AUTO_TEST_CASE( BatchCheckoutEmpty )
{
    PLM_MOCK_PLUGIN mock;
    mock.Authenticate( PLM_CREDENTIALS( wxS( "http://test" ), wxS( "user" ), wxS( "pass" ) ) );

    std::vector<wxString> projectIds;
    PLM_CHECKOUT_OPTIONS options;

    PLM_BATCH_RESULT result = mock.BatchCheckout( projectIds, wxS( "/tmp/batch" ), options );

    // Empty batch has no failures, so check counts instead
    BOOST_CHECK_EQUAL( result.total_count, 0 );
    BOOST_CHECK_EQUAL( result.success_count, 0 );
    BOOST_CHECK_EQUAL( result.failure_count, 0 );
    BOOST_CHECK( !result.cancelled );
}


/**
 * Test batch checkout with single project.
 */
BOOST_AUTO_TEST_CASE( BatchCheckoutSingleProject )
{
    PLM_MOCK_PLUGIN mock;
    mock.Authenticate( PLM_CREDENTIALS( wxS( "http://test" ), wxS( "user" ), wxS( "pass" ) ) );

    std::vector<wxString> projectIds;
    projectIds.push_back( wxS( "project1" ) );
    PLM_CHECKOUT_OPTIONS options;

    PLM_BATCH_RESULT result = mock.BatchCheckout( projectIds, wxS( "/tmp/batch" ), options );

    BOOST_CHECK_EQUAL( result.total_count, 1 );
    BOOST_CHECK_EQUAL( result.results.size(), 1 );
    BOOST_CHECK( result.results[0].project_id == wxS( "project1" ) );
}


/**
 * Test batch checkout with multiple projects.
 */
BOOST_AUTO_TEST_CASE( BatchCheckoutMultipleProjects )
{
    PLM_MOCK_PLUGIN mock;
    mock.Authenticate( PLM_CREDENTIALS( wxS( "http://test" ), wxS( "user" ), wxS( "pass" ) ) );

    std::vector<wxString> projectIds;
    projectIds.push_back( wxS( "project1" ) );
    projectIds.push_back( wxS( "project2" ) );
    projectIds.push_back( wxS( "project3" ) );
    PLM_CHECKOUT_OPTIONS options;

    PLM_BATCH_RESULT result = mock.BatchCheckout( projectIds, wxS( "/tmp/batch" ), options );

    BOOST_CHECK_EQUAL( result.total_count, 3 );
    BOOST_CHECK_EQUAL( result.results.size(), 3 );
}


/**
 * Test batch checkout with progress callback.
 */
BOOST_AUTO_TEST_CASE( BatchCheckoutWithProgress )
{
    PLM_MOCK_PLUGIN mock;
    mock.Authenticate( PLM_CREDENTIALS( wxS( "http://test" ), wxS( "user" ), wxS( "pass" ) ) );

    std::vector<wxString> projectIds;
    projectIds.push_back( wxS( "project1" ) );
    projectIds.push_back( wxS( "project2" ) );
    projectIds.push_back( wxS( "project3" ) );
    PLM_CHECKOUT_OPTIONS options;

    std::vector<size_t> progressCalls;

    PLM_BATCH_RESULT result = mock.BatchCheckout(
            projectIds, wxS( "/tmp/batch" ), options,
            [&progressCalls]( size_t current, size_t total, const wxString& projectId, bool success )
            {
                progressCalls.push_back( current );
                return true;
            } );

    BOOST_CHECK_EQUAL( progressCalls.size(), 3 );
    BOOST_CHECK_EQUAL( progressCalls[0], 0 );
    BOOST_CHECK_EQUAL( progressCalls[1], 1 );
    BOOST_CHECK_EQUAL( progressCalls[2], 2 );
}


/**
 * Test batch checkout cancellation via callback.
 */
BOOST_AUTO_TEST_CASE( BatchCheckoutCancellation )
{
    PLM_MOCK_PLUGIN mock;
    mock.Authenticate( PLM_CREDENTIALS( wxS( "http://test" ), wxS( "user" ), wxS( "pass" ) ) );

    std::vector<wxString> projectIds;
    projectIds.push_back( wxS( "project1" ) );
    projectIds.push_back( wxS( "project2" ) );
    projectIds.push_back( wxS( "project3" ) );
    PLM_CHECKOUT_OPTIONS options;

    int callCount = 0;

    PLM_BATCH_RESULT result = mock.BatchCheckout(
            projectIds, wxS( "/tmp/batch" ), options,
            [&callCount]( size_t current, size_t total, const wxString& projectId, bool success )
            {
                callCount++;
                return current < 1;  // Cancel after first item
            } );

    BOOST_CHECK( result.cancelled );
    BOOST_CHECK_EQUAL( callCount, 2 );  // Called for items 0 and 1 before cancel
    BOOST_CHECK_EQUAL( result.results.size(), 2 );
}


/**
 * Test batch checkin with empty path list.
 */
BOOST_AUTO_TEST_CASE( BatchCheckinEmpty )
{
    PLM_MOCK_PLUGIN mock;
    mock.Authenticate( PLM_CREDENTIALS( wxS( "http://test" ), wxS( "user" ), wxS( "pass" ) ) );

    std::vector<wxString> localPaths;
    PLM_CHECKIN_OPTIONS options;

    PLM_BATCH_RESULT result = mock.BatchCheckin( localPaths, options );

    // Empty batch has no failures, so check counts instead
    BOOST_CHECK_EQUAL( result.total_count, 0 );
    BOOST_CHECK_EQUAL( result.success_count, 0 );
    BOOST_CHECK_EQUAL( result.failure_count, 0 );
    BOOST_CHECK( !result.cancelled );
}


/**
 * Test batch checkin with multiple paths.
 */
BOOST_AUTO_TEST_CASE( BatchCheckinMultiplePaths )
{
    PLM_MOCK_PLUGIN mock;
    mock.Authenticate( PLM_CREDENTIALS( wxS( "http://test" ), wxS( "user" ), wxS( "pass" ) ) );

    std::vector<wxString> localPaths;
    localPaths.push_back( wxS( "/tmp/project1" ) );
    localPaths.push_back( wxS( "/tmp/project2" ) );
    PLM_CHECKIN_OPTIONS options( wxS( "Batch checkin" ) );

    PLM_BATCH_RESULT result = mock.BatchCheckin( localPaths, options );

    BOOST_CHECK_EQUAL( result.total_count, 2 );
    BOOST_CHECK_EQUAL( result.results.size(), 2 );
}


/**
 * Test FindResult helper function.
 */
BOOST_AUTO_TEST_CASE( BatchResultFindResult )
{
    PLM_BATCH_RESULT result;

    PLM_BATCH_ITEM_RESULT item1( wxS( "project1" ), true );
    item1.local_path = wxS( "/tmp/project1" );
    result.AddResult( item1 );

    PLM_BATCH_ITEM_RESULT item2( wxS( "project2" ), false,
                                  PLM_ERROR_CODE::ERR_FILE_NOT_FOUND, wxS( "Not found" ) );
    result.AddResult( item2 );

    const PLM_BATCH_ITEM_RESULT* found1 = result.FindResult( wxS( "project1" ) );
    BOOST_REQUIRE( found1 != nullptr );
    BOOST_CHECK( found1->success );
    BOOST_CHECK( found1->local_path == wxS( "/tmp/project1" ) );

    const PLM_BATCH_ITEM_RESULT* found2 = result.FindResult( wxS( "project2" ) );
    BOOST_REQUIRE( found2 != nullptr );
    BOOST_CHECK( !found2->success );
    BOOST_CHECK( found2->error_code == PLM_ERROR_CODE::ERR_FILE_NOT_FOUND );

    const PLM_BATCH_ITEM_RESULT* notFound = result.FindResult( wxS( "nonexistent" ) );
    BOOST_CHECK( notFound == nullptr );
}


/**
 * Test batch result counting.
 */
BOOST_AUTO_TEST_CASE( BatchResultCounting )
{
    PLM_BATCH_RESULT result;

    result.AddResult( PLM_BATCH_ITEM_RESULT( wxS( "p1" ), true ) );
    result.AddResult( PLM_BATCH_ITEM_RESULT( wxS( "p2" ), true ) );
    result.AddResult( PLM_BATCH_ITEM_RESULT( wxS( "p3" ), false,
                                              PLM_ERROR_CODE::ERR_INTERNAL, wxS( "Error" ) ) );
    result.AddResult( PLM_BATCH_ITEM_RESULT( wxS( "p4" ), true ) );

    BOOST_CHECK_EQUAL( result.total_count, 4 );
    BOOST_CHECK_EQUAL( result.success_count, 3 );
    BOOST_CHECK_EQUAL( result.failure_count, 1 );
    BOOST_CHECK( !result.success );  // Not all succeeded
}


/**
 * Test batch result with all successes.
 */
BOOST_AUTO_TEST_CASE( BatchResultAllSuccess )
{
    PLM_BATCH_RESULT result;

    result.AddResult( PLM_BATCH_ITEM_RESULT( wxS( "p1" ), true ) );
    result.AddResult( PLM_BATCH_ITEM_RESULT( wxS( "p2" ), true ) );
    result.AddResult( PLM_BATCH_ITEM_RESULT( wxS( "p3" ), true ) );

    BOOST_CHECK_EQUAL( result.total_count, 3 );
    BOOST_CHECK_EQUAL( result.success_count, 3 );
    BOOST_CHECK_EQUAL( result.failure_count, 0 );
    BOOST_CHECK( result.success );
}


BOOST_AUTO_TEST_SUITE_END()
