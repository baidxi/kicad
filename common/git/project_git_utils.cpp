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

// GitDescribeCache implementation

GitDescribeCache::GitDescribeCache()
{
    m_cache.clear();
}

bool GitDescribeCache::CacheEntry::isValid( const wxString& path ) const
{
    auto now = std::chrono::steady_clock::now();
    return projectPath == path && ( now - lastUpdate ) < GitDescribeCache::CACHE_DURATION;
}

void GitDescribeCache::CacheEntry::clear()
{
    projectPath.clear();
    fullHash.clear();
    shortHash.clear();
    fullDescribe.clear();
    shortDescribe.clear();
    tagOnly.clear();
    lastUpdate = std::chrono::steady_clock::time_point{};
}

bool GitDescribeCache::isCacheValid( const wxString& aProjectFile ) const
{
    return m_cache.isValid( aProjectFile );
}

void GitDescribeCache::InvalidateCache( const wxString& aProjectFile )
{
    if( aProjectFile.IsEmpty() || m_cache.projectPath == aProjectFile )
    {
        m_cache.clear();
    }
}

void GitDescribeCache::updateCache( const wxString& aProjectFile )
{
    // Clear current cache
    m_cache.clear();
    m_cache.projectPath = aProjectFile;
    m_cache.lastUpdate = std::chrono::steady_clock::now();

    // Set default values
    m_cache.fullHash = wxT( "no hash" );
    m_cache.shortHash = wxT( "no hash" );
    m_cache.fullDescribe = wxT( "no revision" );
    m_cache.shortDescribe = wxT( "no revision" );
    m_cache.tagOnly = wxT( "no tag" );

    // Check if we have a valid project file path
    if( aProjectFile.IsEmpty() )
        return;

    // Convert to C string safely
    std::string projectPath = TO_UTF8( aProjectFile );
    if( projectPath.empty() )
        return;

    try
    {
        git_repository* repo = nullptr;

        // Safely call GetRepositoryForFile
        try
        {
            repo = PROJECT_GIT_UTILS::GetRepositoryForFile( projectPath.c_str() );
        }
        catch( ... )
        {
            m_cache.fullDescribe = wxT( "no git repo" );
            m_cache.shortDescribe = wxT( "no git repo" );
            return;
        }

        if( !repo )
        {
            m_cache.fullDescribe = wxT( "no git repo" );
            m_cache.shortDescribe = wxT( "no git repo" );
            return;
        }

        // Get hash information
        git_reference* head = nullptr;
        if( git_repository_head( &head, repo ) == 0 )
        {
            const git_oid* oid = git_reference_target( head );
            if( oid )
            {
                char fullBuf[41];
                char shortBuf[9];
                git_oid_tostr( fullBuf, 41, oid );
                git_oid_tostr( shortBuf, 9, oid );
                
                m_cache.fullHash = wxString::FromUTF8( fullBuf );
                m_cache.shortHash = wxString::FromUTF8( shortBuf );
            }
            git_reference_free( head );
        }

        // Get describe information
        git_describe_result* describe_result = nullptr;
        git_describe_options describe_opts = GIT_DESCRIBE_OPTIONS_INIT;

        // Configure describe options to match "git describe --tags --always"
        describe_opts.describe_strategy = GIT_DESCRIBE_TAGS;
        describe_opts.show_commit_oid_as_fallback = 1;

        if( git_describe_workdir( &describe_result, repo, &describe_opts ) == 0 )
        {
            git_buf                     buf = { 0 };
            git_describe_format_options format_opts = GIT_DESCRIBE_FORMAT_OPTIONS_INIT;
            format_opts.abbreviated_size = 7; // Short hash length

            if( git_describe_format( &buf, describe_result, &format_opts ) == 0 )
            {
                if( buf.ptr && buf.size > 0 )
                {
                    m_cache.fullDescribe = wxString::FromUTF8( buf.ptr );

                    // Calculate short version (without hash)
                    wxString shortResult = m_cache.fullDescribe;
                    size_t   hashPos = wxString::npos;
                    size_t   searchPos = 0;

                    while( true )
                    {
                        size_t pos = m_cache.fullDescribe.find( wxT( "-g" ), searchPos );
                        if( pos == wxString::npos )
                            break;
                        hashPos = pos;
                        searchPos = pos + 1;
                    }

                    if( hashPos != wxString::npos )
                    {
                        shortResult = m_cache.fullDescribe.substr( 0, hashPos );
                    }
                    m_cache.shortDescribe = shortResult;

                    // Calculate tag-only version
                    wxString tagResult = m_cache.fullDescribe;
                    size_t commitCountPos = m_cache.fullDescribe.find( wxT( "-" ) );
                    if( commitCountPos != wxString::npos )
                    {
                        // Check if this dash is followed by a number (commit count)
                        if( commitCountPos + 1 < m_cache.fullDescribe.length() && 
                            wxIsdigit( m_cache.fullDescribe[commitCountPos + 1] ) )
                        {
                            tagResult = m_cache.fullDescribe.substr( 0, commitCountPos );
                        }
                    }
                    m_cache.tagOnly = tagResult;
                }
                git_buf_dispose( &buf );
            }

            git_describe_result_free( describe_result );
        }

        git_repository_free( repo );
    }
    catch( ... )
    {
        m_cache.fullDescribe = wxT( "git error" );
        m_cache.shortDescribe = wxT( "git error" );
    }
}

wxString GitDescribeCache::GetCurrentHash( const wxString& aProjectFile, bool aShort )
{
    if( !isCacheValid( aProjectFile ) )
    {
        updateCache( aProjectFile );
    }

    return aShort ? m_cache.shortHash : m_cache.fullHash;
}

wxString GitDescribeCache::GetCurrentDescribe( const wxString& aProjectFile )
{
    if( !isCacheValid( aProjectFile ) )
    {
        updateCache( aProjectFile );
    }

    return m_cache.fullDescribe;
}

wxString GitDescribeCache::GetCurrentDescribeShort( const wxString& aProjectFile )
{
    if( !isCacheValid( aProjectFile ) )
    {
        updateCache( aProjectFile );
    }

    return m_cache.shortDescribe;
}

wxString GitDescribeCache::GetCurrentTag( const wxString& aProjectFile )
{
    if( !isCacheValid( aProjectFile ) )
    {
        updateCache( aProjectFile );
    }

    return m_cache.tagOnly;
}

// Global cache instance
static GitDescribeCache s_gitCache;

// PROJECT_GIT_UTILS implementation

git_repository* PROJECT_GIT_UTILS::GetRepositoryForFile( const char* aFilename )
{
    auto gitbackend = GetGitBackend();
    if( !gitbackend )
    {
        // If no backend is available, use libgit2 directly
        git_repository* repo = nullptr;
        git_buf         discover_path = { 0 };

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

GitDescribeCache& PROJECT_GIT_UTILS::GetGitCache()
{
    return s_gitCache;
}

wxString PROJECT_GIT_UTILS::GetCurrentHash( const wxString& aProjectFile, bool aShort )
{
    return s_gitCache.GetCurrentHash( aProjectFile, aShort );
}

wxString PROJECT_GIT_UTILS::GetCurrentDescribe( const wxString& aProjectFile )
{
    return s_gitCache.GetCurrentDescribe( aProjectFile );
}

wxString PROJECT_GIT_UTILS::GetCurrentDescribeShort( const wxString& aProjectFile )
{
    return s_gitCache.GetCurrentDescribeShort( aProjectFile );
}

wxString PROJECT_GIT_UTILS::GetCurrentTag( const wxString& aProjectFile )
{
    return s_gitCache.GetCurrentTag( aProjectFile );
}

} // namespace KIGIT
