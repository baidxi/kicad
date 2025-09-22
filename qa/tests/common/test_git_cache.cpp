/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.TXT for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/gpl-3.0.html
 * or you may search the http://www.gnu.org website for the version 3 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <boost/test/unit_test.hpp>
#include <git/project_git_utils.h>
#include <wx/filename.h>
#include <wx/dir.h>
#include <wx/filefn.h>
#include <wx/textfile.h>
#include <git2.h>
#include <thread>
#include <chrono>

using namespace KIGIT;

/**
 * Test fixture for Git cache tests
 */
struct GitCacheFixture
{
    GitCacheFixture()
    {
        // Initialize git library
        git_libgit2_init();
        
        // Create a temporary directory for our test repository
        tempDir = wxFileName::CreateTempFileName( "kicad_git_cache_test" );
        wxRemoveFile( tempDir );
        wxMkdir( tempDir );
        
        // Initialize a git repository in the temp directory
        git_repository* repo = nullptr;
        int error = git_repository_init( &repo, tempDir.ToUTF8(), 0 );
        if( error == 0 && repo )
        {
            // Create an initial commit to have something to cache
            CreateInitialCommit( repo );
            git_repository_free( repo );
            hasValidRepo = true;
        }
        else
        {
            hasValidRepo = false;
        }

        // Create test file path
        testFile = tempDir + wxFileName::GetPathSeparator() + "test.txt";
        
        // Get reference to the git cache
        gitCache = &PROJECT_GIT_UTILS::GetGitCache();
        
        // Clear cache before each test
        gitCache->InvalidateCache();
    }

    ~GitCacheFixture()
    {
        // Clear cache after test
        if( gitCache )
            gitCache->InvalidateCache();
            
        // Clean up temporary directory
        if( wxDirExists( tempDir ) )
        {
            wxFileName::Rmdir( tempDir, wxPATH_RMDIR_RECURSIVE );
        }
        
        // Shutdown git library
        git_libgit2_shutdown();
    }

    void CreateInitialCommit( git_repository* repo )
    {
        // Create a test file
        wxString testFile = tempDir + wxFileName::GetPathSeparator() + "test.txt";
        wxTextFile file( testFile );
        file.Create();
        file.AddLine( "Initial commit content" );
        file.Write();
        file.Close();

        // Add file to index
        git_index* index = nullptr;
        if( git_repository_index( &index, repo ) == 0 )
        {
            git_index_add_bypath( index, "test.txt" );
            git_index_write( index );
            
            // Create tree from index
            git_oid tree_id;
            if( git_index_write_tree( &tree_id, index ) == 0 )
            {
                git_tree* tree = nullptr;
                if( git_tree_lookup( &tree, repo, &tree_id ) == 0 )
                {
                    // Create signature
                    git_signature* sig = nullptr;
                    if( git_signature_now( &sig, "Test User", "test@example.com" ) == 0 )
                    {
                        // Create commit
                        git_oid commit_id;
                        git_commit_create_v( &commit_id, repo, "HEAD", sig, sig, nullptr,
                                           "Initial commit", tree, 0 );
                        
                        git_signature_free( sig );
                    }
                    git_tree_free( tree );
                }
            }
            git_index_free( index );
        }
    }

    wxString tempDir;
    wxString testFile;
    bool hasValidRepo;
    GitDescribeCache* gitCache;
};

BOOST_FIXTURE_TEST_SUITE( GitCacheTests, GitCacheFixture )

/**
 * Test hash caching functionality
 */
BOOST_AUTO_TEST_CASE( TestHashCaching )
{
    if( !hasValidRepo )
    {
        BOOST_TEST_MESSAGE( "Skipping test - could not create valid git repository" );
        return;
    }

    // First call should compute and cache the result
    wxString hash1 = gitCache->GetCurrentHash( testFile, false );
    BOOST_CHECK( !hash1.IsEmpty() );
    BOOST_CHECK( hash1 != "no hash" );
    
    // Second call should return cached result (should be identical)
    wxString hash2 = gitCache->GetCurrentHash( testFile, false );
    BOOST_CHECK_EQUAL( hash1, hash2 );
    
    // Test short hash
    wxString shortHash1 = gitCache->GetCurrentHash( testFile, true );
    wxString shortHash2 = gitCache->GetCurrentHash( testFile, true );
    BOOST_CHECK_EQUAL( shortHash1, shortHash2 );
    
    // Short hash should be shorter than full hash
    BOOST_CHECK( shortHash1.Length() < hash1.Length() );
    BOOST_CHECK( shortHash1.Length() == 8 ); // Should be 8 characters
}

/**
 * Test describe caching functionality
 */
BOOST_AUTO_TEST_CASE( TestDescribeCaching )
{
    if( !hasValidRepo )
    {
        BOOST_TEST_MESSAGE( "Skipping test - could not create valid git repository" );
        return;
    }

    // First call should compute and cache the result
    wxString describe1 = gitCache->GetCurrentDescribe( testFile );
    BOOST_CHECK( !describe1.IsEmpty() );
    BOOST_CHECK( describe1 != "no revision" );
    
    // Second call should return cached result
    wxString describe2 = gitCache->GetCurrentDescribe( testFile );
    BOOST_CHECK_EQUAL( describe1, describe2 );
    
    // Test short describe
    wxString shortDescribe1 = gitCache->GetCurrentDescribeShort( testFile );
    wxString shortDescribe2 = gitCache->GetCurrentDescribeShort( testFile );
    BOOST_CHECK_EQUAL( shortDescribe1, shortDescribe2 );
    
    // Test tag
    wxString tag1 = gitCache->GetCurrentTag( testFile );
    wxString tag2 = gitCache->GetCurrentTag( testFile );
    BOOST_CHECK_EQUAL( tag1, tag2 );
}

/**
 * Test cache invalidation
 */
BOOST_AUTO_TEST_CASE( TestCacheInvalidation )
{
    if( !hasValidRepo )
    {
        BOOST_TEST_MESSAGE( "Skipping test - could not create valid git repository" );
        return;
    }

    // Get initial values
    wxString hash1 = gitCache->GetCurrentHash( testFile, false );
    wxString describe1 = gitCache->GetCurrentDescribe( testFile );
    
    // Invalidate cache
    gitCache->InvalidateCache( testFile );
    
    // Get values again - should recompute but be the same
    wxString hash2 = gitCache->GetCurrentHash( testFile, false );
    wxString describe2 = gitCache->GetCurrentDescribe( testFile );
    
    // Values should be the same (same repository state)
    BOOST_CHECK_EQUAL( hash1, hash2 );
    BOOST_CHECK_EQUAL( describe1, describe2 );
}

/**
 * Test cache invalidation with empty path (invalidate all)
 */
BOOST_AUTO_TEST_CASE( TestCacheInvalidationAll )
{
    if( !hasValidRepo )
    {
        BOOST_TEST_MESSAGE( "Skipping test - could not create valid git repository" );
        return;
    }

    // Get initial values
    wxString hash1 = gitCache->GetCurrentHash( testFile, false );
    
    // Invalidate all cache entries
    gitCache->InvalidateCache(); // Empty string should invalidate all
    
    // Get value again - should recompute
    wxString hash2 = gitCache->GetCurrentHash( testFile, false );
    
    // Values should be the same (same repository state)
    BOOST_CHECK_EQUAL( hash1, hash2 );
}

/**
 * Test behavior with non-existent file/repository
 */
BOOST_AUTO_TEST_CASE( TestNonExistentRepository )
{
    wxString nonExistentFile = "/non/existent/path/file.txt";
    
    // All methods should return appropriate "no" values
    wxString hash = gitCache->GetCurrentHash( nonExistentFile, false );
    BOOST_CHECK_EQUAL( hash, "no hash" );
    
    wxString shortHash = gitCache->GetCurrentHash( nonExistentFile, true );
    BOOST_CHECK_EQUAL( shortHash, "no hash" );
    
    wxString describe = gitCache->GetCurrentDescribe( nonExistentFile );
    BOOST_CHECK_EQUAL( describe, "no git repo" );
    
    wxString shortDescribe = gitCache->GetCurrentDescribeShort( nonExistentFile );
    BOOST_CHECK_EQUAL( shortDescribe, "no git repo" );
    
    wxString tag = gitCache->GetCurrentTag( nonExistentFile );
    BOOST_CHECK_EQUAL( tag, "no tag" );
}

/**
 * Test cache consistency across different getter methods
 */
BOOST_AUTO_TEST_CASE( TestCacheConsistency )
{
    if( !hasValidRepo )
    {
        BOOST_TEST_MESSAGE( "Skipping test - could not create valid git repository" );
        return;
    }

    // Call different methods - they should all populate the cache from the same update
    wxString hash = gitCache->GetCurrentHash( testFile, false );
    wxString describe = gitCache->GetCurrentDescribe( testFile );
    wxString shortDescribe = gitCache->GetCurrentDescribeShort( testFile );
    wxString tag = gitCache->GetCurrentTag( testFile );
    
    // All should be non-empty and not error values
    BOOST_CHECK( !hash.IsEmpty() && hash != "no hash" );
    BOOST_CHECK( !describe.IsEmpty() && describe != "no revision" );
    BOOST_CHECK( !shortDescribe.IsEmpty() && shortDescribe != "no revision" );
    BOOST_CHECK( !tag.IsEmpty() );
    
    // Calling again should return identical cached values
    BOOST_CHECK_EQUAL( hash, gitCache->GetCurrentHash( testFile, false ) );
    BOOST_CHECK_EQUAL( describe, gitCache->GetCurrentDescribe( testFile ) );
    BOOST_CHECK_EQUAL( shortDescribe, gitCache->GetCurrentDescribeShort( testFile ) );
    BOOST_CHECK_EQUAL( tag, gitCache->GetCurrentTag( testFile ) );
}

/**
 * Test that different project paths don't interfere with each other
 */
BOOST_AUTO_TEST_CASE( TestMultipleProjectPaths )
{
    if( !hasValidRepo )
    {
        BOOST_TEST_MESSAGE( "Skipping test - could not create valid git repository" );
        return;
    }

    wxString file1 = testFile;
    wxString file2 = "/non/existent/path/file.txt";
    
    // Get values for valid repository
    wxString hash1 = gitCache->GetCurrentHash( file1, false );
    
    // Get values for non-existent repository
    wxString hash2 = gitCache->GetCurrentHash( file2, false );
    
    // They should be different
    BOOST_CHECK( hash1 != hash2 );
    BOOST_CHECK_EQUAL( hash2, "no hash" );
    
    // Calling again should return same cached values
    BOOST_CHECK_EQUAL( hash1, gitCache->GetCurrentHash( file1, false ) );
    BOOST_CHECK_EQUAL( hash2, gitCache->GetCurrentHash( file2, false ) );
}

BOOST_AUTO_TEST_SUITE_END()