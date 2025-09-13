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

#include "project_git_utils.h"
#include "git_backend.h"

#include <wx/string.h>
#include <string_utils.h>
#include <git2.h>
#include <chrono>

namespace KIGIT
{

// Cache structure for git describe results
struct GitDescribeCache
{
    wxString projectPath;
    wxString fullDescribe;
    wxString shortDescribe;
    wxString tagOnly;
    std::chrono::steady_clock::time_point lastUpdate;
    static const std::chrono::seconds CACHE_DURATION;
    
    bool isValid( const wxString& path ) const
    {
        auto now = std::chrono::steady_clock::now();
        return projectPath == path && 
               (now - lastUpdate) < CACHE_DURATION;
    }
    
    void update( const wxString& path, const wxString& full, const wxString& short_desc, const wxString& tag )
    {
        projectPath = path;
        fullDescribe = full;
        shortDescribe = short_desc;
        tagOnly = tag;
        lastUpdate = std::chrono::steady_clock::now();
    }
};

const std::chrono::seconds GitDescribeCache::CACHE_DURATION( 30 ); // Cache for 30 seconds
static GitDescribeCache s_gitCache;

git_repository* PROJECT_GIT_UTILS::GetRepositoryForFile( const char* aFilename )
{
    auto gitbackend = GetGitBackend();
    if( !gitbackend )
    {
        // If no backend is available, use libgit2 directly
        git_repository* repo = nullptr;
        git_buf discover_path = { 0 };
        
        // Use git_repository_discover to find the repo
        if( git_repository_discover( &discover_path, aFilename, 0, nullptr ) == 0 )
        {
            if( git_repository_open( &repo, discover_path.ptr ) == 0 )
            {
                git_buf_dispose( &discover_path );
                return repo;
            }
            git_buf_dispose( &discover_path );
        }
        return nullptr;
    }
    
    return gitbackend->GetRepositoryForFile( aFilename );
}

int PROJECT_GIT_UTILS::CreateBranch( git_repository* aRepo, const wxString& aBranchName )
{
    return GetGitBackend()->CreateBranch( aRepo, aBranchName );
}

bool PROJECT_GIT_UTILS::RemoveVCS( git_repository*& aRepo, const wxString& aProjectPath,
                                  bool aRemoveGitDir, wxString* aErrors )
{
    return GetGitBackend()->RemoveVCS( aRepo, aProjectPath, aRemoveGitDir, aErrors );
}

wxString PROJECT_GIT_UTILS::GetCurrentHash( const wxString& aProjectFile, bool aShort )
{
    wxString result = wxT( "no hash" );
    git_repository* repo = PROJECT_GIT_UTILS::GetRepositoryForFile( TO_UTF8( aProjectFile ) );

    if( repo )
    {
        git_reference* head = nullptr;

    if( git_repository_head( &head, repo ) == 0 )
        {
            const git_oid* oid = git_reference_target( head );

            if( oid )
            {
                char buf[41];
                size_t len = aShort ? 8 : 41;
                git_oid_tostr( buf, len, oid );
                result = wxString::FromUTF8( buf );
            }

            git_reference_free( head );
        }

        git_repository_free( repo );
    }

    return result;
}

wxString PROJECT_GIT_UTILS::GetCurrentDescribe( const wxString& aProjectFile )
{
    // Check cache first
    if( s_gitCache.isValid( aProjectFile ) )
    {
        return s_gitCache.fullDescribe;
    }
    
    wxString result = wxT( "no revision" );
    
    // Check if we have a valid project file path
    if( aProjectFile.IsEmpty() )
        return result;
    
    // Convert to C string safely
    std::string projectPath = TO_UTF8( aProjectFile );
    if( projectPath.empty() )
        return result;
    
    try
    {
        git_repository* repo = nullptr;
        
        // Safely call GetRepositoryForFile with null check
        try 
        {
            repo = PROJECT_GIT_UTILS::GetRepositoryForFile( projectPath.c_str() );
        }
        catch( ... )
        {
            return wxT( "no git repo" );
        }

        if( repo )
        {
            git_describe_result* describe_result = nullptr;
            git_describe_options describe_opts = GIT_DESCRIBE_OPTIONS_INIT;
            
            // Configure describe options to match "git describe --tags --always"
            describe_opts.describe_strategy = GIT_DESCRIBE_TAGS;
            describe_opts.show_commit_oid_as_fallback = 1;
            
            if( git_describe_workdir( &describe_result, repo, &describe_opts ) == 0 )
            {
                git_buf buf = { 0 };
                git_describe_format_options format_opts = GIT_DESCRIBE_FORMAT_OPTIONS_INIT;
                format_opts.abbreviated_size = 7;  // Short hash length
                
                if( git_describe_format( &buf, describe_result, &format_opts ) == 0 )
                {
                    if( buf.ptr && buf.size > 0 )
                    {
                        result = wxString::FromUTF8( buf.ptr );
                        
                        // Calculate short version for cache
                        wxString shortResult = result;
                        size_t hashPos = wxString::npos;
                        size_t searchPos = 0;
                        
                        while( true )
                        {
                            size_t pos = result.find( wxT("-g"), searchPos );
                            if( pos == wxString::npos )
                                break;
                            hashPos = pos;
                            searchPos = pos + 1;
                        }
                        
                        if( hashPos != wxString::npos )
                        {
                            shortResult = result.substr( 0, hashPos );
                        }
                        
                        // Calculate tag-only version for cache
                        wxString tagResult = result;
                        // Remove commit count and hash - find first dash that indicates commit count
                        size_t commitCountPos = result.find( wxT("-") );
                        if( commitCountPos != wxString::npos )
                        {
                            // Check if this dash is followed by a number (commit count)
                            if( commitCountPos + 1 < result.length() && 
                                wxIsdigit( result[commitCountPos + 1] ) )
                            {
                                tagResult = result.substr( 0, commitCountPos );
                            }
                        }
                        
                        // Update cache with all versions
                        s_gitCache.update( aProjectFile, result, shortResult, tagResult );
                    }
                    git_buf_dispose( &buf );
                }
                
                git_describe_result_free( describe_result );
            }

            git_repository_free( repo );
        }
        else
        {
            result = wxT( "no git repo" );
        }
    }
    catch( ... )
    {
        result = wxT( "git error" );
    }

    return result;
}

wxString PROJECT_GIT_UTILS::GetCurrentDescribeShort( const wxString& aProjectFile )
{
    // Check cache first
    if( s_gitCache.isValid( aProjectFile ) )
    {
        return s_gitCache.shortDescribe;
    }
    
    // If cache miss, call full describe which will update the cache
    GetCurrentDescribe( aProjectFile );
    
    // Now check cache again
    if( s_gitCache.isValid( aProjectFile ) )
    {
        return s_gitCache.shortDescribe;
    }
    
    // Fallback if something went wrong
    return wxT( "no revision" );
}

wxString PROJECT_GIT_UTILS::GetCurrentTag( const wxString& aProjectFile )
{
    // Check cache first
    if( s_gitCache.isValid( aProjectFile ) )
    {
        return s_gitCache.tagOnly;
    }
    
    // If cache miss, call full describe which will update the cache
    GetCurrentDescribe( aProjectFile );
    
    // Now check cache again
    if( s_gitCache.isValid( aProjectFile ) )
    {
        return s_gitCache.tagOnly;
    }
    
    // Fallback if something went wrong
    return wxT( "no tag" );
}

} // namespace KIGIT
