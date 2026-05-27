# KiCad PLM Git Plugin Walkthrough

This document provides an in-depth walkthrough of the Git PLM plugin implementation,
explaining architectural decisions, implementation patterns, and lessons learned.
Use this as a reference when creating your own PLM plugins.

## Architecture Overview

The Git plugin maps PLM concepts to Git primitives:

| PLM Concept | Git Implementation |
|-------------|-------------------|
| Projects | Directories containing .kicad_pro files |
| Folders | Repository subdirectories |
| Revisions | Git commits |
| Checkout | git clone/checkout + branch switch |
| Checkin | git add + commit + push |
| Lock | Git LFS lock or marker file |

### File Structure

```
plugins/plm-git/
├── CMakeLists.txt           # Build configuration
├── plm_git_plugin.h         # Main plugin class header
├── plm_git_plugin.cpp       # Core plugin implementation
├── plm_git_auth.h           # Authentication handling
├── plm_git_auth.cpp         # Auth implementation
├── plm_git_schema.h         # Attribute schema
└── plm_git_schema.cpp       # Schema implementation
```

### Design Decisions

#### Decision 1: Git Command Execution

Rather than using libgit2, we execute git commands directly via subprocess. This has
trade-offs:

Pros:
- No additional library dependency
- Uses user's existing git configuration (SSH keys, credentials helper)
- Easier debugging (same commands users would run manually)
- Full compatibility with git features

Cons:
- Slower than native library
- Platform-specific command parsing
- Output parsing can be fragile

Implementation:

```cpp
int PLM_GIT_PLUGIN::RunGitCommand( const std::vector<wxString>& aArgs,
                                    wxString& aOutput,
                                    const wxString& aWorkDir )
{
    wxArrayString output, errors;
    wxString command = wxS( "git" );

    for( const wxString& arg : aArgs )
        command += wxS( " " ) + arg;

    long result = wxExecute( command, output, errors, wxEXEC_SYNC,
                              nullptr, aWorkDir.IsEmpty() ? nullptr : aWorkDir.c_str() );

    aOutput.Clear();

    for( const wxString& line : output )
        aOutput += line + wxS( "\n" );

    return static_cast<int>( result );
}
```

#### Decision 2: Folder ID Encoding

Folder IDs encode the relative path within the repository using base64:

```cpp
wxString PLM_GIT_PLUGIN::PathToFolderId( const wxString& aPath )
{
    // Use base64 encoding to create a safe ID from path
    // This handles special characters in directory names
    wxString encoded = wxBase64Encode( aPath.ToUTF8(), aPath.Len() );
    return wxS( "git:" ) + encoded;
}

wxString PLM_GIT_PLUGIN::FolderIdToPath( const wxString& aFolderId )
{
    if( !aFolderId.StartsWith( wxS( "git:" ) ) )
        return wxEmptyString;

    wxString encoded = aFolderId.Mid( 4 );
    wxMemoryBuffer decoded = wxBase64Decode( encoded );
    return wxString::FromUTF8( static_cast<const char*>( decoded.GetData() ),
                               decoded.GetDataLen() );
}
```

This approach:
- Preserves special characters and Unicode in paths
- Creates predictable, collision-free IDs
- Is reversible without needing a lookup table

#### Decision 3: Locking Strategy

Git doesn't have native file locking, so we implement it two ways:

1. **Git LFS Locking** (preferred)

   When Git LFS is configured with file locking support, we use it:

   ```cpp
   PLM_LOCK_RESULT PLM_GIT_PLUGIN::Lock( const PLM_LOCK_REQUEST& aRequest )
   {
       wxString output;
       wxString path = GetProjectPath( aRequest.project_id );

       // Try Git LFS lock first
       int result = RunGitCommand( { wxS( "lfs" ), wxS( "lock" ), path }, output );

       if( result == 0 )
       {
           PLM_LOCK_INFO info;
           info.is_locked = true;
           info.locked_by_me = true;
           info.lock_owner = m_username;
           info.lock_timestamp = wxDateTime::Now().GetTicks();
           return PLM_LOCK_RESULT::Success( info );
       }

       // Fall back to advisory lock
       return CreateAdvisoryLock( aRequest );
   }
   ```

2. **Advisory Lock Files**

   When LFS isn't available, we create `.kicad_lock` files:

   ```cpp
   PLM_LOCK_RESULT PLM_GIT_PLUGIN::CreateAdvisoryLock( const PLM_LOCK_REQUEST& aRequest )
   {
       wxString lockFile = GetProjectPath( aRequest.project_id ) + wxS( "/.kicad_lock" );

       // Check if lock already exists
       if( wxFileExists( lockFile ) )
       {
           PLM_LOCK_INFO existing = ReadLockFile( lockFile );

           if( existing.lock_owner != m_username && !aRequest.force )
               return PLM_LOCK_RESULT::Conflict( existing );
       }

       // Create lock file
       WriteLockFile( lockFile, m_username, aRequest.reason );

       // Commit lock file
       RunGitCommand( { wxS( "add" ), lockFile }, output );
       RunGitCommand( { wxS( "commit" ), wxS( "-m" ),
                        wxS( "Lock project: " ) + aRequest.reason }, output );
       RunGitCommand( { wxS( "push" ) }, output );

       return PLM_LOCK_RESULT::Success( ReadLockFile( lockFile ) );
   }
   ```

## Implementation Details

### Authentication

The Git plugin supports multiple authentication methods:

```cpp
PLM_AUTH_RESULT PLM_GIT_PLUGIN::Authenticate( const PLM_CREDENTIALS& aCredentials )
{
    m_repositoryUrl = aCredentials.server_url;

    // Test connection by listing remote refs
    wxString output;
    int result;

    if( aCredentials.auth_method == PLM_AUTH_METHOD::SSH_KEY )
    {
        // SSH authentication uses the SSH agent or key file
        result = RunGitCommand( { wxS( "ls-remote" ), m_repositoryUrl }, output );
    }
    else if( aCredentials.auth_method == PLM_AUTH_METHOD::TOKEN )
    {
        // Token-based auth (GitHub PAT, GitLab tokens)
        wxString authUrl = BuildAuthUrl( m_repositoryUrl, aCredentials.auth_token.Get() );
        result = RunGitCommand( { wxS( "ls-remote" ), authUrl }, output );
    }
    else
    {
        // Username/password with credential helper
        ConfigureCredentials( aCredentials.username, aCredentials.password.Get() );
        result = RunGitCommand( { wxS( "ls-remote" ), m_repositoryUrl }, output );
    }

    if( result != 0 )
    {
        return PLM_AUTH_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_FAILED,
                                          wxS( "Failed to connect to repository" ) );
    }

    m_username = aCredentials.username;
    m_isAuthenticated = true;

    m_session.user_info.user_id = m_username;
    m_session.user_info.display_name = m_username;
    m_session.server_url = m_repositoryUrl;

    return PLM_AUTH_RESULT::Success( wxEmptyString, wxDateTime::Now() + wxTimeSpan::Hours( 24 ) );
}
```

### Folder Navigation

Folders map directly to repository directories:

```cpp
std::vector<PLM_FOLDER> PLM_GIT_PLUGIN::GetChildFolders( const wxString& aFolderId )
{
    std::vector<PLM_FOLDER> folders;
    wxString path = FolderIdToPath( aFolderId );

    if( path.IsEmpty() )
        path = wxS( "." );

    wxString fullPath = m_localClonePath + wxFileName::GetPathSeparator() + path;
    wxDir dir( fullPath );

    if( !dir.IsOpened() )
        return folders;

    wxString dirname;
    bool cont = dir.GetFirst( &dirname, wxEmptyString, wxDIR_DIRS );

    while( cont )
    {
        // Skip hidden directories
        if( !dirname.StartsWith( wxS( "." ) ) )
        {
            PLM_FOLDER folder;
            folder.id = PathToFolderId( path + wxS( "/" ) + dirname );
            folder.name = dirname;
            folder.parent_id = aFolderId;
            folder.is_project_folder = HasKiCadProject( fullPath + wxS( "/" ) + dirname );
            folders.push_back( folder );
        }

        cont = dir.GetNext( &dirname );
    }

    return folders;
}
```

### File Operations

#### Checkout

```cpp
PLM_CHECKOUT_RESULT PLM_GIT_PLUGIN::Checkout( const wxString& aProjectId,
                                               const wxString& aLocalPath,
                                               const PLM_CHECKOUT_OPTIONS& aOptions )
{
    wxString projectPath = FolderIdToPath( aProjectId );

    // Clone repository if needed
    if( !wxDirExists( m_localClonePath ) )
    {
        wxString output;

        if( RunGitCommand( { wxS( "clone" ), m_repositoryUrl, m_localClonePath }, output ) != 0 )
        {
            return PLM_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_CHECKOUT_FAILED,
                                                  wxS( "Failed to clone repository" ) );
        }
    }
    else
    {
        // Fetch latest changes
        wxString output;
        RunGitCommand( { wxS( "fetch" ), wxS( "origin" ) }, output, m_localClonePath );
    }

    // Checkout specific revision if requested
    if( !aOptions.revision.IsEmpty() )
    {
        wxString output;

        if( RunGitCommand( { wxS( "checkout" ), aOptions.revision }, output, m_localClonePath ) != 0 )
        {
            return PLM_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_FILE_NOT_FOUND,
                                                  wxS( "Revision not found" ) );
        }
    }

    // Acquire lock if requested
    if( aOptions.acquire_lock )
    {
        PLM_LOCK_REQUEST lockReq( aProjectId );
        PLM_LOCK_RESULT lockResult = Lock( lockReq );

        if( !lockResult.success )
            return PLM_CHECKOUT_RESULT::Failure( lockResult.error_code, lockResult.error_message );
    }

    // Copy project to local path
    wxString srcPath = m_localClonePath + wxFileName::GetPathSeparator() + projectPath;

    if( !CopyDirectory( srcPath, aLocalPath ) )
    {
        return PLM_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_CHECKOUT_FAILED,
                                              wxS( "Failed to copy files" ) );
    }

    PLM_FILE_INFO fileInfo = GetFileInfo( aProjectId );
    return PLM_CHECKOUT_RESULT::Success( aLocalPath, fileInfo );
}
```

#### Checkin

```cpp
PLM_CHECKIN_RESULT PLM_GIT_PLUGIN::Checkin( const wxString& aLocalPath,
                                             const PLM_CHECKIN_OPTIONS& aOptions )
{
    // Copy files back to repository
    wxString projectPath = GetProjectPathFromLocalPath( aLocalPath );
    wxString repoPath = m_localClonePath + wxFileName::GetPathSeparator() + projectPath;

    if( !CopyDirectory( aLocalPath, repoPath ) )
    {
        return PLM_CHECKIN_RESULT::Failure( PLM_ERROR_CODE::ERR_CHECKIN_FAILED,
                                            wxS( "Failed to copy files" ) );
    }

    wxString output;

    // Stage changes
    if( RunGitCommand( { wxS( "add" ), projectPath }, output, m_localClonePath ) != 0 )
    {
        return PLM_CHECKIN_RESULT::Failure( PLM_ERROR_CODE::ERR_CHECKIN_FAILED,
                                            wxS( "Failed to stage changes" ) );
    }

    // Commit
    wxString message = aOptions.revision_comment.IsEmpty()
                            ? wxS( "Updated project" )
                            : aOptions.revision_comment;

    if( RunGitCommand( { wxS( "commit" ), wxS( "-m" ), message }, output, m_localClonePath ) != 0 )
    {
        // Check if there are no changes to commit
        if( output.Contains( wxS( "nothing to commit" ) ) )
        {
            return PLM_CHECKIN_RESULT::Failure( PLM_ERROR_CODE::ERR_NO_CHANGES,
                                                wxS( "No changes to check in" ) );
        }

        return PLM_CHECKIN_RESULT::Failure( PLM_ERROR_CODE::ERR_CHECKIN_FAILED,
                                            wxS( "Commit failed" ) );
    }

    // Push
    if( RunGitCommand( { wxS( "push" ) }, output, m_localClonePath ) != 0 )
    {
        return PLM_CHECKIN_RESULT::Failure( PLM_ERROR_CODE::ERR_CHECKIN_FAILED,
                                            wxS( "Push failed" ) );
    }

    // Get new revision ID (commit hash)
    RunGitCommand( { wxS( "rev-parse" ), wxS( "HEAD" ) }, output, m_localClonePath );
    wxString newRevision = output.Trim();

    // Release lock if requested
    if( aOptions.release_lock )
    {
        PLM_UNLOCK_REQUEST unlockReq( GetProjectIdFromPath( projectPath ) );
        Unlock( unlockReq );
    }

    PLM_FILE_INFO fileInfo = GetFileInfo( GetProjectIdFromPath( projectPath ) );
    fileInfo.revision = newRevision;

    return PLM_CHECKIN_RESULT::Success( newRevision, fileInfo );
}
```

### Revision History

Git log provides rich revision history:

```cpp
PLM_REVISION_HISTORY PLM_GIT_PLUGIN::GetRevisionHistory( const wxString& aProjectId )
{
    PLM_REVISION_HISTORY history;
    wxString projectPath = FolderIdToPath( aProjectId );
    wxString output;

    // Format: hash|author|timestamp|subject
    int result = RunGitCommand(
            { wxS( "log" ),
              wxS( "--format=%H|%an|%at|%s" ),
              wxS( "--" ), projectPath },
            output, m_localClonePath );

    if( result != 0 )
        return history;

    history.revisions = ParseGitLog( output );
    return history;
}

std::vector<PLM_REVISION> PLM_GIT_PLUGIN::ParseGitLog( const wxString& aLogOutput )
{
    std::vector<PLM_REVISION> revisions;
    wxStringTokenizer lines( aLogOutput, wxS( "\n" ) );

    while( lines.HasMoreTokens() )
    {
        wxString line = lines.GetNextToken().Trim();

        if( line.IsEmpty() )
            continue;

        wxStringTokenizer parts( line, wxS( "|" ) );

        if( parts.CountTokens() < 4 )
            continue;

        PLM_REVISION rev;
        rev.revision_id = parts.GetNextToken();
        rev.author_id = parts.GetNextToken();
        rev.author_name = rev.author_id;  // Git doesn't separate ID from name

        long timestamp;
        parts.GetNextToken().ToLong( &timestamp );
        rev.created_at = timestamp;

        rev.comment = parts.GetNextToken();
        rev.state = PLM_STATE_CATEGORY::RELEASED;  // All commits are "released"

        revisions.push_back( rev );
    }

    return revisions;
}
```

## Lessons Learned

### 1. Handle Network Failures Gracefully

Git operations can fail due to network issues. Always provide meaningful error messages:

```cpp
if( !IsNetworkAvailable() )
{
    return PLM_CHECKOUT_RESULT::Failure(
        PLM_ERROR_CODE::ERR_NETWORK_UNREACHABLE,
        wxS( "Cannot reach remote repository. Check your network connection." )
    );
}
```

### 2. Cache Expensive Operations

Git commands can be slow. Cache results where appropriate:

```cpp
wxString PLM_GIT_PLUGIN::GetUserDisplayName( const wxString& aUserId )
{
    auto it = m_userDisplayNames.find( aUserId );

    if( it != m_userDisplayNames.end() )
        return it->second;

    // Git doesn't have a user lookup, just return the ID
    m_userDisplayNames[aUserId] = aUserId;
    return aUserId;
}
```

### 3. Validate Input Early

Check parameters before executing expensive operations:

```cpp
PLM_LOCK_RESULT PLM_GIT_PLUGIN::Lock( const PLM_LOCK_REQUEST& aRequest )
{
    if( aRequest.project_id.IsEmpty() )
    {
        return PLM_LOCK_RESULT::Failure(
            PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
            wxS( "Project ID is required" )
        );
    }

    if( !m_isAuthenticated )
    {
        return PLM_LOCK_RESULT::Failure(
            PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
            wxS( "Not authenticated" )
        );
    }

    // Continue with lock operation...
}
```

### 4. Support Offline Mode Where Possible

Allow read-only operations when offline:

```cpp
std::vector<PLM_FOLDER> PLM_GIT_PLUGIN::GetRootFolders()
{
    // Local operations work offline
    if( !wxDirExists( m_localClonePath ) )
    {
        if( !m_isAuthenticated )
            return {};

        // Try to clone for first-time use
        wxString output;

        if( RunGitCommand( { wxS( "clone" ), m_repositoryUrl, m_localClonePath }, output ) != 0 )
            return {};
    }

    // Read from local clone
    return GetChildFolders( PathToFolderId( wxS( "." ) ) );
}
```

### 5. Provide Detailed Error Context

Include actionable information in error messages:

```cpp
if( RunGitCommand( { wxS( "push" ) }, output, m_localClonePath ) != 0 )
{
    wxString errorMsg;

    if( output.Contains( wxS( "rejected" ) ) )
    {
        errorMsg = wxS( "Push rejected. Pull latest changes and resolve conflicts." );
    }
    else if( output.Contains( wxS( "permission denied" ) ) )
    {
        errorMsg = wxS( "Permission denied. Check your repository access." );
    }
    else
    {
        errorMsg = wxS( "Push failed: " ) + output;
    }

    return PLM_CHECKIN_RESULT::Failure( PLM_ERROR_CODE::ERR_CHECKIN_FAILED, errorMsg );
}
```

## Testing

The Git plugin uses the conformance test framework plus Git-specific tests:

```cpp
BOOST_AUTO_TEST_CASE( GitRevisionHistory )
{
    PLM_GIT_PLUGIN plugin;
    plugin.OnLoad();

    // Setup test repository
    CreateTestRepository();
    CreateTestCommits( 5 );

    PLM_CREDENTIALS creds;
    creds.server_url = GetTestRepositoryPath();
    plugin.Authenticate( creds );

    PLM_REVISION_HISTORY history = plugin.GetRevisionHistory( wxS( "test-project" ) );

    BOOST_CHECK( history.Count() >= 5 );
    BOOST_CHECK( !history.GetLatest()->revision_id.IsEmpty() );
}
```

## Future Improvements

Potential enhancements for the Git plugin:

1. **Branch Support** - Allow selecting branches for checkout
2. **Merge Conflict UI** - Provide GUI for resolving conflicts
3. **Submodule Support** - Handle projects that use git submodules
4. **Sparse Checkout** - Only download needed project files
5. **Git Hooks Integration** - Run pre/post commit hooks for validation

## References

- [KiCad PLM Plugin Author Guide](../common/plm/PLM_PLUGIN_AUTHOR_GUIDE.md)
- [PLM API Documentation](../common/plm/plm_api.h)
- [Git Documentation](https://git-scm.com/docs)
- [Git LFS](https://git-lfs.github.com/)
