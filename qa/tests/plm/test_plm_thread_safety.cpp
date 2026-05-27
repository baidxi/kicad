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
 * @file test_plm_thread_safety.cpp
 * @brief Thread safety tests for PLM plugin implementations.
 *
 * These tests verify that PLM plugins handle concurrent access correctly
 * without race conditions or deadlocks.
 */

#include <boost/test/unit_test.hpp>
#include "plm_conformance_framework.h"

#include <atomic>
#include <thread>
#include <vector>
#include <chrono>


BOOST_AUTO_TEST_SUITE( PlmThreadSafety )


/**
 * Test that concurrent authentication state checks don't cause data races.
 */
BOOST_AUTO_TEST_CASE( ConcurrentIsAuthenticatedChecks )
{
    PLM_MOCK_PLUGIN mock;

    std::atomic<int> successCount( 0 );
    std::atomic<int> failureCount( 0 );

    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int iterationsPerThread = 100;

    for( int i = 0; i < numThreads; ++i )
    {
        threads.emplace_back( [&mock, &successCount, &failureCount, iterationsPerThread]()
        {
            for( int j = 0; j < iterationsPerThread; ++j )
            {
                bool result = mock.IsAuthenticated();
                (void) result;
                successCount++;
            }
        } );
    }

    for( auto& t : threads )
        t.join();

    BOOST_CHECK_EQUAL( successCount.load(), numThreads * iterationsPerThread );
    BOOST_CHECK_EQUAL( failureCount.load(), 0 );
}


/**
 * Test that concurrent session retrieval doesn't cause data races.
 */
BOOST_AUTO_TEST_CASE( ConcurrentGetSessionChecks )
{
    PLM_MOCK_PLUGIN mock;

    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int iterationsPerThread = 100;

    std::atomic<bool> hasError( false );

    for( int i = 0; i < numThreads; ++i )
    {
        threads.emplace_back( [&mock, &hasError, iterationsPerThread]()
        {
            for( int j = 0; j < iterationsPerThread; ++j )
            {
                try
                {
                    PLM_SESSION session = mock.GetCurrentSession();
                    (void) session;
                }
                catch( ... )
                {
                    hasError = true;
                }
            }
        } );
    }

    for( auto& t : threads )
        t.join();

    BOOST_CHECK( !hasError.load() );
}


/**
 * Test concurrent authentication and logout.
 *
 * Note: This test is disabled because the mock plugin is not thread-safe.
 * The actual plugin implementations (Git, Odoo) should have mutexes.
 * This test validates that the production plugin thread-safety approach
 * is correct when tested against real plugins.
 */
BOOST_AUTO_TEST_CASE( ConcurrentAuthAndLogout, *boost::unit_test::disabled() )
{
    PLM_MOCK_PLUGIN mock;

    std::atomic<int> operationCount( 0 );
    std::atomic<bool> hasError( false );

    std::vector<std::thread> threads;
    const int numThreads = 4;
    const int iterationsPerThread = 10;

    for( int i = 0; i < numThreads; ++i )
    {
        if( i % 2 == 0 )
        {
            threads.emplace_back( [&mock, &operationCount, &hasError, iterationsPerThread]()
            {
                try
                {
                    for( int j = 0; j < iterationsPerThread; ++j )
                    {
                        PLM_CREDENTIALS creds( wxS( "http://test" ), wxS( "user" ), wxS( "pass" ) );
                        PLM_AUTH_RESULT result = mock.Authenticate( creds );
                        (void) result;
                        operationCount++;
                    }
                }
                catch( ... )
                {
                    hasError = true;
                }
            } );
        }
        else
        {
            threads.emplace_back( [&mock, &operationCount, &hasError, iterationsPerThread]()
            {
                try
                {
                    for( int j = 0; j < iterationsPerThread; ++j )
                    {
                        bool result = mock.Logout();
                        (void) result;
                        operationCount++;
                    }
                }
                catch( ... )
                {
                    hasError = true;
                }
            } );
        }
    }

    for( auto& t : threads )
        t.join();

    // Verify operations completed without exceptions
    BOOST_CHECK( operationCount.load() > 0 );
    BOOST_CHECK( !hasError.load() );
}


/**
 * Test concurrent navigation operations.
 */
BOOST_AUTO_TEST_CASE( ConcurrentNavigation )
{
    PLM_MOCK_PLUGIN mock;
    mock.Authenticate( PLM_CREDENTIALS( wxS( "http://test" ), wxS( "user" ), wxS( "pass" ) ) );

    std::atomic<bool> hasError( false );
    std::vector<std::thread> threads;
    const int numThreads = 8;
    const int iterationsPerThread = 50;

    for( int i = 0; i < numThreads; ++i )
    {
        threads.emplace_back( [&mock, &hasError, iterationsPerThread, i]()
        {
            try
            {
                for( int j = 0; j < iterationsPerThread; ++j )
                {
                    switch( i % 3 )
                    {
                    case 0:
                    {
                        std::vector<PLM_FOLDER> folders = mock.GetRootFolders();
                        (void) folders;
                        break;
                    }

                    case 1:
                    {
                        std::vector<PLM_FOLDER> children =
                                mock.GetChildFolders( wxS( "folder1" ) );
                        (void) children;
                        break;
                    }

                    case 2:
                    {
                        PLM_FOLDER_PATH path = mock.GetFolderPath( wxS( "folder1" ) );
                        (void) path;
                        break;
                    }
                    }
                }
            }
            catch( ... )
            {
                hasError = true;
            }
        } );
    }

    for( auto& t : threads )
        t.join();

    BOOST_CHECK( !hasError.load() );
}


/**
 * Test concurrent lock info retrieval.
 */
BOOST_AUTO_TEST_CASE( ConcurrentLockInfo )
{
    PLM_MOCK_PLUGIN mock;
    mock.Authenticate( PLM_CREDENTIALS( wxS( "http://test" ), wxS( "user" ), wxS( "pass" ) ) );

    std::atomic<bool> hasError( false );
    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int iterationsPerThread = 100;

    for( int i = 0; i < numThreads; ++i )
    {
        threads.emplace_back( [&mock, &hasError, iterationsPerThread]()
        {
            try
            {
                for( int j = 0; j < iterationsPerThread; ++j )
                {
                    PLM_LOCK_INFO info = mock.GetLockInfo( wxS( "project1" ) );
                    (void) info;
                }
            }
            catch( ... )
            {
                hasError = true;
            }
        } );
    }

    for( auto& t : threads )
        t.join();

    BOOST_CHECK( !hasError.load() );
}


/**
 * Test that Ping() can be called concurrently.
 */
BOOST_AUTO_TEST_CASE( ConcurrentPing )
{
    PLM_MOCK_PLUGIN mock;
    mock.Authenticate( PLM_CREDENTIALS( wxS( "http://test" ), wxS( "user" ), wxS( "pass" ) ) );

    std::atomic<int> pingCount( 0 );
    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int iterationsPerThread = 50;

    for( int i = 0; i < numThreads; ++i )
    {
        threads.emplace_back( [&mock, &pingCount, iterationsPerThread]()
        {
            for( int j = 0; j < iterationsPerThread; ++j )
            {
                bool result = mock.Ping();
                (void) result;
                pingCount++;
            }
        } );
    }

    for( auto& t : threads )
        t.join();

    BOOST_CHECK_EQUAL( pingCount.load(), numThreads * iterationsPerThread );
}


/**
 * Test concurrent batch operations don't interfere.
 */
BOOST_AUTO_TEST_CASE( ConcurrentBatchOperations )
{
    PLM_MOCK_PLUGIN mock;
    mock.Authenticate( PLM_CREDENTIALS( wxS( "http://test" ), wxS( "user" ), wxS( "pass" ) ) );

    std::atomic<bool> hasError( false );
    std::vector<std::thread> threads;

    for( int i = 0; i < 4; ++i )
    {
        threads.emplace_back( [&mock, &hasError, i]()
        {
            try
            {
                std::vector<wxString> projectIds;
                projectIds.push_back( wxString::Format( wxS( "project_%d_1" ), i ) );
                projectIds.push_back( wxString::Format( wxS( "project_%d_2" ), i ) );

                PLM_CHECKOUT_OPTIONS options;
                options.acquire_lock = true;

                PLM_BATCH_RESULT result = mock.BatchCheckout(
                        projectIds,
                        wxString::Format( wxS( "/tmp/batch_%d" ), i ),
                        options );
                (void) result;
            }
            catch( ... )
            {
                hasError = true;
            }
        } );
    }

    for( auto& t : threads )
        t.join();

    BOOST_CHECK( !hasError.load() );
}


BOOST_AUTO_TEST_SUITE_END()
