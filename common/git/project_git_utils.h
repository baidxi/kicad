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

#ifndef PROJECT_GIT_UTILS_H
#define PROJECT_GIT_UTILS_H

#include <git2.h>
#include <wx/string.h>
#include <import_export.h>
#include <chrono>

namespace KIGIT
{

/**
 * Cache for git repository information with automatic invalidation.
 * Manages cached values for hashes, describes, and tags with proper invalidation.
 */
class APIEXPORT GitDescribeCache
{
public:
    GitDescribeCache();
    ~GitDescribeCache() = default;

    /**
     * Get the current HEAD commit hash for the repository containing the given file.
     *
     * @param aProjectFile Absolute path to any file within the repository.
     * @param aShort       If true, return the short (8 char) hash, otherwise full hash.
     * @return wxString containing the hash or "no hash" if unavailable.
     */
    wxString GetCurrentHash( const wxString& aProjectFile, bool aShort );

    /**
     * Get the current git describe for the repository containing the given file.
     * Returns output similar to "git describe --tags --always".
     *
     * @param aProjectFile Absolute path to any file within the repository.
     * @return wxString containing the describe string or "no revision" if unavailable.
     */
    wxString GetCurrentDescribe( const wxString& aProjectFile );

    /**
     * Get the current git describe in short form (without hash part).
     *
     * @param aProjectFile Absolute path to any file within the repository.
     * @return wxString containing the short describe string or "no revision" if unavailable.
     */
    wxString GetCurrentDescribeShort( const wxString& aProjectFile );

    /**
     * Get the current git tag name only.
     *
     * @param aProjectFile Absolute path to any file within the repository.
     * @return wxString containing the tag name or "no tag" if unavailable.
     */
    wxString GetCurrentTag( const wxString& aProjectFile );

    /**
     * Invalidate cache for the given project file.
     *
     * @param aProjectFile Path to invalidate, or empty to invalidate all.
     */
    void InvalidateCache( const wxString& aProjectFile = wxEmptyString );

private:
    struct CacheEntry
    {
        wxString                              projectPath;
        wxString                              fullHash;
        wxString                              shortHash;
        wxString                              fullDescribe;
        wxString                              shortDescribe;
        wxString                              tagOnly;
        std::chrono::steady_clock::time_point lastUpdate;

        bool isValid( const wxString& path ) const;
        void clear();
    };

    static constexpr std::chrono::seconds CACHE_DURATION{ 30 };

    CacheEntry m_cache;

    /**
     * Update cache with git information for the given project file.
     *
     * @param aProjectFile Path to the project file.
     */
    void updateCache( const wxString& aProjectFile );

    /**
     * Check if cache is valid for the given project file.
     *
     * @param aProjectFile Path to check.
     * @return True if cache is valid and up-to-date.
     */
    bool isCacheValid( const wxString& aProjectFile ) const;
};

/** Utility class with helper functions for project level git operations. */
class APIEXPORT PROJECT_GIT_UTILS
{
public:
    /**
     * Discover and open the repository that contains the given file.
     *
     * @param aFilename Absolute path to a file inside the repository.
     * @return Newly opened git_repository or nullptr on failure.
     */
    static git_repository* GetRepositoryForFile( const char* aFilename );

    /**
     * Create a new branch based on HEAD.
     *
     * @param aRepo       Repository in which to create the branch.
     * @param aBranchName Name of the new branch.
     * @return 0 on success, libgit2 error code on failure.
     */
    static int CreateBranch( git_repository* aRepo, const wxString& aBranchName );

    /**
     * Return the current HEAD commit hash for the repository containing aProjectFile.
     *
     * @param aProjectFile Absolute path to any file within the repository (typically the
     *                     project file path).
     * @param aShort       If true, return the short (8 char) hash, otherwise full hash.
     * @return wxString containing the hash or "no hash" if unavailable.
     */
    static wxString GetCurrentHash( const wxString& aProjectFile, bool aShort );

    /**
     * Return the current git describe for the repository containing aProjectFile.
     * This returns output similar to "git describe --tags --always", showing the
     * last tag plus commit count since that tag (e.g., "v0.1.2-5-g1234567").
     *
     * @param aProjectFile Absolute path to any file within the repository (typically the
     *                     project file path).
     * @return wxString containing the describe string or "no revision" if unavailable.
     */
    static wxString GetCurrentDescribe( const wxString& aProjectFile );

    /**
     * Return the current git describe in short form for the repository containing aProjectFile.
     * This returns the describe string without the hash part (e.g., "v0.1.2-5" instead of "v0.1.2-5-g1234567").
     *
     * @param aProjectFile Absolute path to any file within the repository (typically the
     *                     project file path).
     * @return wxString containing the short describe string or "no revision" if unavailable.
     */
    static wxString GetCurrentDescribeShort( const wxString& aProjectFile );

    /**
     * Return the current git tag for the repository containing aProjectFile.
     * This returns only the pure tag name (e.g., "v0.1.2") if the current commit
     * is exactly on a tag, otherwise returns the tag with commit count.
     *
     * @param aProjectFile Absolute path to any file within the repository (typically the
     *                     project file path).
     * @return wxString containing the tag name or "no tag" if unavailable.
     */
    static wxString GetCurrentTag( const wxString& aProjectFile );

    /**
     * Get the global git cache instance.
     *
     * @return Reference to the global GitDescribeCache instance.
     */
    static GitDescribeCache& GetGitCache();

    /**
     * Remove version control from a directory by freeing the repository and
     * optionally removing the .git directory.
     *
     * @param aRepo Repository to free (will be set to nullptr)
     * @param aProjectPath Path to the project directory
     * @param aRemoveGitDir If true, also remove the .git directory from disk
     * @param aErrors Output parameter for any error messages
     * @return True on success, false on failure
     */
static bool RemoveVCS( git_repository*& aRepo, const wxString& aProjectPath = wxEmptyString,
                      bool aRemoveGitDir = false, wxString* aErrors = nullptr );
};

} // namespace KIGIT

#endif // PROJECT_GIT_UTILS_H