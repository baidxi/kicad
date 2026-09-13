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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PROJECT_GIT_UTILS_H
#define PROJECT_GIT_UTILS_H

#include <git2.h>
#include <wx/string.h>
#include <import_export.h>

#include <map>
#include <set>

#include <git/git_status_handler.h>

namespace KIGIT
{

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

    /**
     * Compute a working directory path that preserves symlinks from the user's project path.
     *
     * When a project is opened via a symlinked path, git_repository_workdir() returns the
     * canonical (symlink-resolved) path. This causes path mismatches between the tree cache
     * (which uses the user-provided symlinked path) and the git status results (which use
     * canonical paths). This function computes a working directory path that preserves
     * the symlinks from the user's original project path.
     *
     * @param aUserProjectPath The path the user used to open the project (may contain symlinks)
     * @param aCanonicalWorkDir The canonical workdir from git_repository_workdir()
     * @return Working directory path with symlinks preserved, or aCanonicalWorkDir on failure
     */
    static wxString ComputeSymlinkPreservingWorkDir( const wxString& aUserProjectPath,
                                                     const wxString& aCanonicalWorkDir );

    /**
     * Test whether an absolute file path lives inside the current project directory.
     *
     * The git repository may span more than the KiCad project (the project is often a
     * subdirectory of a larger repo).  Commit and amend checklists must only offer files
     * under the project directory, otherwise unrelated repository files leak into the
     * commit (issue #15910).  The project path is normalized to end in a separator before
     * the prefix comparison so that a sibling such as "proj-extra/" is not treated as being
     * inside "proj/".  An empty project path matches nothing.
     *
     * @param aAbsPath Absolute path of the file being tested.
     * @param aProjectPath Absolute path of the project directory.
     * @return True if aAbsPath is within the project directory.
     */
    static bool IsWithinProjectPath( const wxString& aAbsPath, const wxString& aProjectPath );

    /**
     * Point a KIGIT_COMMON at the project it belongs to.
     *
     * The working directory reported by libgit2 is symlink-resolved, while the rest of KiCad
     * speaks in the path the project was opened with; the two have to be reconciled or the
     * status paths never match the project's own (see ComputeSymlinkPreservingWorkDir()).
     * Every entry point that drives a repository for a project needs the same setup, so keep
     * it in one place.
     *
     * @param aCommon Common to configure; does nothing when it holds no repository.
     * @param aProjectPath Path the project was opened with (may contain symlinks).
     * @param aUsername Remote user name from the project local settings.
     * @param aSSHKey SSH key from the project local settings.  An empty key is not offered to
     *                the authentication callback, which reads an empty key as "no keys left".
     */
    static void ConfigureProjectCommon( KIGIT_COMMON* aCommon, const wxString& aProjectPath,
                                        const wxString& aUsername, const wxString& aSSHKey );

    /**
     * Reduce a repository status map to the files a commit checklist should offer.
     *
     * The repository is often larger than the project and holds files KiCad writes but never
     * commits (locks, backups, autosaves).  Commit, amend and the Git check job all present
     * the same checklist, so they share this filter rather than each keeping their own copy
     * of the rules.
     *
     * @param aFileStatus Status map as returned by GIT_STATUS_HANDLER::GetFileStatus().
     * @param aWorkDir Repository working directory, used to make the returned paths relative.
     * @param aProjectPath Absolute path of the project directory.
     * @param aProjectName Project name, used to recognize its archived backup directory.
     * @param aSelectedFiles When given, only these absolute paths are offered, and the project
     *                       local settings file is included if it is one of them because the
     *                       user picked it explicitly.  When null the whole project is offered
     *                       and an untracked local settings file is left out.
     * @return Map of repository relative paths to the raw git status flags of each file.
     */
    static std::map<wxString, int> CollectCommittableFiles(
            const std::map<wxString, FileStatus>& aFileStatus, const wxString& aWorkDir,
            const wxString& aProjectPath, const wxString& aProjectName,
            const std::set<wxString>* aSelectedFiles = nullptr );
};

} // namespace KIGIT

#endif // PROJECT_GIT_UTILS_H
