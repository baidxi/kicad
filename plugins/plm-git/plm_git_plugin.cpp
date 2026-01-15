/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "plm_git_plugin.h"

#include <nlohmann/json.hpp>
#include <plm/plm_dirty_tracker.h>
#include <plm/plm_file.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/process.h>
#include <wx/stdpaths.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>
#include <wx/datetime.h>
#include <wx/tokenzr.h>
#include <wx/uri.h>

#include <ctime>
#include <set>


PLM_GIT_PLUGIN::PLM_GIT_PLUGIN() : m_isAuthenticated( false ), m_isTempClone( false )
{
}


PLM_GIT_PLUGIN::~PLM_GIT_PLUGIN()
{
}


PLM_PLUGIN_INFO PLM_GIT_PLUGIN::GetPluginInfo() const
{
    PLM_PLUGIN_INFO info;
    info.plugin_id = wxS( "plm-git" );
    info.display_name = wxS( "Git PLM Plugin" );
    info.vendor = wxS( "KiCad Developers" );
    info.version = PLM_VERSION( 1, 0, 0 );
    info.description =
            wxS( "Git-based version control for KiCad projects. Supports local and remote "
                 "repositories via HTTPS and SSH." );
    info.vendor_url = wxS( "https://kicad.org" );
    info.icon_name = wxS( "git_icon" );
    return info;
}


PLM_CAPABILITIES PLM_GIT_PLUGIN::GetCapabilities() const
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_CAPABILITIES caps;

    // Core file operations
    caps.supports_checkout = true;
    caps.supports_locking = true;
    caps.supports_revisions = true;
    caps.supports_attributes = true;
    caps.supports_diff = true;
    caps.supports_search = true;
    caps.supports_branching = true;

    // Authentication paths exposed by BuildAuthConfig and the forge providers
    caps.supports_api_keys = true;
    caps.supports_oauth = true;

    // ECO/ECR workflows are forge-backed; require an active provider at runtime
    const bool hasForge = m_forgeProvider != nullptr;
    caps.supports_eco = hasForge;
    caps.supports_eco_create = hasForge;
    caps.supports_eco_transitions = hasForge;
    caps.supports_eco_items = hasForge;
    caps.supports_eco_impact = hasForge;
    caps.supports_ecr_to_eco = hasForge;
    caps.supports_review = hasForge;
    caps.supports_forge_api = hasForge;
    caps.supports_ci_cd = hasForge;

    caps.supports_part_database = true;
    caps.supports_bom_validation = true;
    caps.supports_bom_management = true;
    caps.supports_routing = true;
    caps.supports_state_transitions = true;
    caps.supports_part_qualification = true;
    caps.supports_where_used = true;
    caps.supports_variants = true;
    caps.supports_audit_trail = true;
    caps.supports_folder_creation = true;
    caps.supports_project_creation = true;
    caps.supports_cloning = true;

    // Offline support comes from PLM_DIRTY_TRACKER (WF-OFFL-001)
    caps.supports_offline_mode = true;

    // Shared library subscription/versioning. Slice 1 supports SYMBOL only;
    // FOOTPRINT and 3D return ERR_NOT_SUPPORTED at the per-call layer.
    caps.supports_shared_libraries = true;

    return caps;
}


bool PLM_GIT_PLUGIN::OnLoad()
{
    wxString output;
    int result = RunGitCommand( { wxS( "--version" ) }, output, wxEmptyString );

    if( result != 0 )
        return false;

    InitManagers();

    return true;
}


void PLM_GIT_PLUGIN::OnUnload()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    CleanupTempClone();

    m_isAuthenticated = false;
    m_isTempClone = false;
    m_session = PLM_SESSION();
    m_forgeProvider.reset();
    TeardownManagers();
}


PLM_AUTH_RESULT PLM_GIT_PLUGIN::Authenticate( const PLM_CREDENTIALS& aCredentials )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_AUTH_RESULT result;
    m_repositoryUrl = aCredentials.server_url;
    m_username = aCredentials.username;

    wxURI uri( m_repositoryUrl );
    wxString host = uri.GetServer();

    // Local repository path -- no network auth needed
    if( host.IsEmpty() )
    {
        if( wxDirExists( m_repositoryUrl ) )
        {
            m_localClonePath = m_repositoryUrl;
            m_isTempClone = false;
            m_isAuthenticated = true;
            m_detectedRole = wxS( "admin" );
            m_session.server_url = m_repositoryUrl;
            m_session.token = wxS( "local" );
            m_session.expires_at = static_cast<int64_t>( std::time( nullptr ) ) + 86400LL * 365 * 100;
            m_session.user_info.user_id = m_username.IsEmpty() ? wxS( "local" ) : m_username;
            m_session.user_info.display_name = m_session.user_info.user_id;
            m_session.user_info.roles.push_back( m_detectedRole );

            SetupActivityTracker();
            SetupStateMapper();
            ParseSharedLibrariesConfig( aCredentials );

            result.success = true;
            result.token = m_session.token.Get();
            result.expires_at = m_session.expires_at;
            return result;
        }

        result.success = false;
        result.error_message = wxS( "Invalid repository URL or path" );
        return result;
    }

    // Build GIT_AUTH_CONFIG from PLM_CREDENTIALS
    m_authConfig = BuildAuthConfig( aCredentials );
    m_authHelper.Configure( m_authConfig );

    // Fill in missing HTTPS credentials from the git credential helper now that Configure() is done
    if( ( m_authConfig.method == GIT_AUTH_METHOD::HTTPS_BASIC ||
          m_authConfig.method == GIT_AUTH_METHOD::HTTPS_TOKEN ) &&
        m_authConfig.username.IsEmpty() && m_authConfig.password.IsEmpty() )
    {
        wxString storedUser, storedPass;

        if( m_authHelper.RetrieveCredentials( m_repositoryUrl, storedUser, storedPass ) )
        {
            m_authConfig.username = storedUser;
            m_authConfig.password = storedPass;
            m_authHelper.Configure( m_authConfig );
        }
    }

    // Lightweight connection test instead of full clone
    GIT_AUTH_RESULT authResult = m_authHelper.TestAuth( m_repositoryUrl );

    if( authResult.success )
    {
        m_isAuthenticated = true;

        // Successful git ls-remote with credentials implies at least write access.
        // Platform-specific API queries (GitHub, GitLab) can refine this later.
        m_detectedRole = wxS( "write" );

        m_session.server_url = m_repositoryUrl;
        m_session.token = wxS( "git_session" );
        m_session.expires_at = static_cast<int64_t>( std::time( nullptr ) ) + 86400;
        m_session.user_info.user_id = m_username;
        m_session.user_info.display_name = m_username;
        m_session.user_info.roles.push_back( m_detectedRole );

        if( m_authConfig.store_credentials )
        {
            m_authHelper.StoreCredentials( m_repositoryUrl, m_authConfig.username,
                                           m_authConfig.password );
        }

        // Initialize forge provider if the remote URL points to a recognized platform
        PLM_FORGE_TYPE forgeType = PLM_GIT_FORGE_PROVIDER::DetectForgeType( m_repositoryUrl );

        if( forgeType != PLM_FORGE_TYPE::UNKNOWN )
        {
            m_forgeProvider = std::make_unique<PLM_GIT_FORGE_PROVIDER>(
                    m_repositoryUrl, &m_authHelper, &m_authConfig );
        }

        SetupActivityTracker();
        SetupStateMapper();
        ParseSharedLibrariesConfig( aCredentials );

        result.success = true;
        result.token = m_session.token.Get();
        result.expires_at = m_session.expires_at;
    }
    else
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_AUTH_FAILED;
        result.error_message = authResult.error_message;
    }

    return result;
}


bool PLM_GIT_PLUGIN::Logout()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    CleanupTempClone();

    m_isAuthenticated = false;
    m_isTempClone = false;
    m_detectedRole.Clear();
    m_session = PLM_SESSION();
    m_forgeProvider.reset();
    m_sharedLibrariesCache.clear();
    m_sharedLibArtifactPaths.clear();

    // Keep the tracker alive for any subsequent unauthenticated audit
    // events; clear cached entries from the previous session and unbind
    // the fetch function so it can no longer query GetAuditLog.
    if( m_activityTracker )
    {
        m_activityTracker->ClearActivity();
        m_activityTracker->SetFetchFunction( {} );
    }

    // Same lifecycle for the state mapper: keep it alive but drop the
    // cached mappings and the callback that points back into the plugin.
    if( m_stateMapper )
    {
        m_stateMapper->ClearMappings();
        m_stateMapper->SetMappingCallback( {} );
    }

    RefreshPermissions();
    return true;
}


wxString PLM_GIT_PLUGIN::ExtractRevisionToTemp( const wxString& aRevision,
                                                const wxString& aRepoRelativePath )
{
    wxString content;
    int rc = RunGitCommand( { wxS( "show" ), aRevision + wxS( ":" ) + aRepoRelativePath },
                            content, m_localClonePath );

    if( rc != 0 )
        return wxEmptyString;

    // CreateTempFileName atomically reserves the path; write directly to it
    // rather than appending an extension (which would switch to a non-reserved
    // path and create a TOCTOU window). The diff analyzer is given the file
    // type explicitly via SetBase{Schematic,PCB}/SetNew{Schematic,PCB}.
    wxString tempPath = wxFileName::CreateTempFileName( wxS( "plm_diff_" ) );

    if( tempPath.IsEmpty() )
        return wxEmptyString;

    wxFile out( tempPath, wxFile::write );

    if( !out.IsOpened() || !out.Write( content ) )
    {
        wxRemoveFile( tempPath );
        return wxEmptyString;
    }

    out.Close();
    return tempPath;
}


void PLM_GIT_PLUGIN::AppendComponentChanges( const PLM_DIFF_ANALYSIS_RESULT&   aResult,
                                             const wxString&                   aRepoRelativePath,
                                             std::vector<PLM_REVISION_CHANGE>& aOut )
{
    if( !aResult.success )
        return;

    wxFileName fn( aRepoRelativePath );
    wxString   elementType =
            fn.GetExt() == wxS( "kicad_sch" ) ? wxS( "schematic component" )
                                              : wxS( "pcb footprint" );

    for( const PLM_COMPONENT_CHANGE& change : aResult.changes )
    {
        PLM_CHANGE_TYPE plmType = PLM_CHANGE_TYPE::MODIFIED;

        switch( change.type )
        {
        case PLM_COMPONENT_CHANGE_TYPE::ADDED:    plmType = PLM_CHANGE_TYPE::ADDED;    break;
        case PLM_COMPONENT_CHANGE_TYPE::REMOVED:  plmType = PLM_CHANGE_TYPE::REMOVED;  break;
        case PLM_COMPONENT_CHANGE_TYPE::MOVED:    plmType = PLM_CHANGE_TYPE::MOVED;    break;
        default:                                  plmType = PLM_CHANGE_TYPE::MODIFIED; break;
        }

        wxString description = change.description.IsEmpty()
                ? PLM_DIFF_ANALYZER::GetChangeTypeName( change.type )
                : change.description;

        aOut.emplace_back( plmType, elementType, change.reference, description );
    }
}


wxString PLM_GIT_PLUGIN::GetGitPlmDir()
{
    if( m_localClonePath.IsEmpty() )
        return wxEmptyString;

    wxString gitDir;

    if( RunGitCommand( { wxS( "rev-parse" ), wxS( "--git-dir" ) }, gitDir, m_localClonePath )
        != 0 )
    {
        return wxEmptyString;
    }

    gitDir.Trim();

    if( gitDir.IsEmpty() )
        return wxEmptyString;

    if( wxFileName( gitDir ).IsRelative() )
        gitDir = m_localClonePath + wxFileName::GetPathSeparator() + gitDir;

    wxString plmDir = gitDir + wxFileName::GetPathSeparator() + wxS( "plm" );

    if( !wxDirExists( plmDir ) && !wxFileName::Mkdir( plmDir, wxS_DIR_DEFAULT,
                                                       wxPATH_MKDIR_FULL ) )
    {
        return wxEmptyString;
    }

    return plmDir;
}


void PLM_GIT_PLUGIN::RecordReviewComment( const wxString& aCommentId, const wxString& aReviewId )
{
    if( aCommentId.IsEmpty() || aReviewId.IsEmpty() )
        return;

    wxString plmDir = GetGitPlmDir();

    if( plmDir.IsEmpty() )
        return;

    wxString reviewsPath = plmDir + wxFileName::GetPathSeparator() + wxS( "reviews.json" );

    nlohmann::json doc;
    nlohmann::json comments = nlohmann::json::object();

    if( wxFileExists( reviewsPath ) )
    {
        wxFile readFile( reviewsPath, wxFile::read );

        if( readFile.IsOpened() )
        {
            wxFileOffset size = readFile.Length();

            if( size > 0 )
            {
                std::string bytes;
                bytes.resize( static_cast<size_t>( size ) );
                ssize_t read = readFile.Read( bytes.data(), bytes.size() );

                if( read >= 0 && static_cast<size_t>( read ) == bytes.size() )
                {
                    try
                    {
                        doc = nlohmann::json::parse( bytes );

                        if( doc.contains( "comments" ) && doc["comments"].is_object() )
                            comments = doc["comments"];
                    }
                    catch( const nlohmann::json::exception& )
                    {
                        // Malformed file; overwrite with a fresh map.
                    }
                }
            }

            readFile.Close();
        }
    }

    comments[aCommentId.ToStdString()] = aReviewId.ToStdString();
    doc["comments"] = comments;

    wxFile writeFile( reviewsPath, wxFile::write );

    if( !writeFile.IsOpened() )
        return;

    std::string serialized = doc.dump( 2 );
    writeFile.Write( serialized.data(), serialized.size() );
    writeFile.Close();
}


void PLM_GIT_PLUGIN::RecordPendingPush( const wxString& aBranch )
{
    if( aBranch.IsEmpty() )
        return;

    wxString plmDir = GetGitPlmDir();

    if( plmDir.IsEmpty() )
        return;

    wxString queuePath = plmDir + wxFileName::GetPathSeparator() + wxS( "pending_pushes.json" );

    nlohmann::json doc;
    nlohmann::json pending = nlohmann::json::array();

    if( wxFileExists( queuePath ) )
    {
        wxFile readFile( queuePath, wxFile::read );

        if( readFile.IsOpened() )
        {
            wxFileOffset size = readFile.Length();

            if( size > 0 )
            {
                std::string bytes;
                bytes.resize( static_cast<size_t>( size ) );
                ssize_t read = readFile.Read( bytes.data(), bytes.size() );

                if( read >= 0 && static_cast<size_t>( read ) == bytes.size() )
                {
                    try
                    {
                        doc = nlohmann::json::parse( bytes );

                        if( doc.contains( "pending" ) && doc["pending"].is_array() )
                            pending = doc["pending"];
                    }
                    catch( const nlohmann::json::exception& )
                    {
                        pending = nlohmann::json::array();
                    }
                }
            }

            readFile.Close();
        }
    }

    std::string branchUtf8 = aBranch.ToStdString();

    for( const auto& entry : pending )
    {
        if( entry.is_string() && entry.get<std::string>() == branchUtf8 )
            return; // Already queued.
    }

    pending.push_back( branchUtf8 );
    doc["pending"] = pending;

    wxFile writeFile( queuePath, wxFile::write );

    if( !writeFile.IsOpened() )
        return;

    std::string serialized = doc.dump( 2 );
    writeFile.Write( serialized.data(), serialized.size() );
    writeFile.Close();
}


int PLM_GIT_PLUGIN::DrainPendingPushes()
{
    wxString plmDir = GetGitPlmDir();

    if( plmDir.IsEmpty() )
        return 0;

    wxString queuePath = plmDir + wxFileName::GetPathSeparator() + wxS( "pending_pushes.json" );

    if( !wxFileExists( queuePath ) )
        return 0;

    nlohmann::json doc;
    nlohmann::json pending = nlohmann::json::array();

    {
        wxFile readFile( queuePath, wxFile::read );

        if( !readFile.IsOpened() )
            return 0;

        wxFileOffset size = readFile.Length();

        if( size <= 0 )
            return 0;

        std::string bytes;
        bytes.resize( static_cast<size_t>( size ) );

        if( readFile.Read( bytes.data(), bytes.size() ) != static_cast<ssize_t>( bytes.size() ) )
            return 0;

        readFile.Close();

        try
        {
            doc = nlohmann::json::parse( bytes );

            if( doc.contains( "pending" ) && doc["pending"].is_array() )
                pending = doc["pending"];
        }
        catch( const nlohmann::json::exception& )
        {
            return 0;
        }
    }

    int            pushed = 0;
    nlohmann::json remaining = nlohmann::json::array();

    for( const auto& entry : pending )
    {
        if( !entry.is_string() )
            continue;

        wxString branch = wxString::FromUTF8( entry.get<std::string>() );
        wxString output;

        int rc = RunGitCommand( { wxS( "push" ), wxS( "origin" ), branch }, output,
                                m_localClonePath );

        if( rc == 0 )
            pushed++;
        else
            remaining.push_back( entry );
    }

    if( remaining.empty() )
    {
        wxRemoveFile( queuePath );
    }
    else
    {
        doc["pending"] = remaining;
        wxFile writeFile( queuePath, wxFile::write );

        if( writeFile.IsOpened() )
        {
            std::string serialized = doc.dump( 2 );
            writeFile.Write( serialized.data(), serialized.size() );
            writeFile.Close();
        }
    }

    return pushed;
}


wxString PLM_GIT_PLUGIN::LookupReviewForComment( const wxString& aCommentId )
{
    if( aCommentId.IsEmpty() )
        return wxEmptyString;

    wxString plmDir = GetGitPlmDir();

    if( plmDir.IsEmpty() )
        return wxEmptyString;

    wxString reviewsPath = plmDir + wxFileName::GetPathSeparator() + wxS( "reviews.json" );

    if( !wxFileExists( reviewsPath ) )
        return wxEmptyString;

    wxFile readFile( reviewsPath, wxFile::read );

    if( !readFile.IsOpened() )
        return wxEmptyString;

    wxFileOffset size = readFile.Length();

    if( size <= 0 )
        return wxEmptyString;

    std::string bytes;
    bytes.resize( static_cast<size_t>( size ) );

    if( readFile.Read( bytes.data(), bytes.size() ) != static_cast<ssize_t>( bytes.size() ) )
        return wxEmptyString;

    readFile.Close();

    try
    {
        nlohmann::json doc = nlohmann::json::parse( bytes );

        if( !doc.contains( "comments" ) || !doc["comments"].is_object() )
            return wxEmptyString;

        const auto& comments = doc["comments"];
        auto it = comments.find( aCommentId.ToStdString() );

        if( it == comments.end() || !it->is_string() )
            return wxEmptyString;

        return wxString::FromUTF8( it->get<std::string>() );
    }
    catch( const nlohmann::json::exception& )
    {
        return wxEmptyString;
    }
}


bool PLM_GIT_PLUGIN::IsAuthenticated() const
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );
    return m_isAuthenticated;
}


bool PLM_GIT_PLUGIN::Ping()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated )
        return false;

    // Remote repo that hasn't been cloned yet -- use TestAuth for connectivity check
    if( m_localClonePath.IsEmpty() && !m_repositoryUrl.IsEmpty() )
    {
        GIT_AUTH_RESULT authResult = m_authHelper.TestAuth( m_repositoryUrl );
        return authResult.success;
    }

    if( m_localClonePath.IsEmpty() )
        return false;

    if( !IsGitRepository( m_localClonePath ) )
        return false;

    // For remote repositories, try to contact the remote
    if( !m_repositoryUrl.IsEmpty() )
    {
        wxString output;
        int exitCode = RunGitCommand( { wxS( "ls-remote" ), wxS( "--exit-code" ),
                                        m_repositoryUrl, wxS( "HEAD" ) },
                                      output, m_localClonePath );
        return exitCode == 0;
    }

    return true;
}


PLM_SESSION PLM_GIT_PLUGIN::GetCurrentSession() const
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );
    return m_session;
}


PLM_AUTH_RESULT PLM_GIT_PLUGIN::RefreshSession()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_AUTH_RESULT result;

    if( !m_isAuthenticated )
    {
        result.success = false;
        result.error_message = wxS( "Not authenticated" );
        return result;
    }

    if( !Ping() )
    {
        m_isAuthenticated = false;
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_AUTH_EXPIRED;
        result.error_message = wxS( "Remote repository is no longer reachable" );
        return result;
    }

    m_session.expires_at = static_cast<int64_t>( std::time( nullptr ) ) + 86400;
    result.success = true;
    result.token = m_session.token.Get();
    result.expires_at = m_session.expires_at;
    return result;
}


std::vector<PLM_FOLDER> PLM_GIT_PLUGIN::GetRootFolders()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_FOLDER> folders;

    if( !m_isAuthenticated )
        return folders;

    if( !EnsureClone() )
        return folders;

    wxDir dir( m_localClonePath );

    if( !dir.IsOpened() )
        return folders;

    wxString dirname;
    bool cont = dir.GetFirst( &dirname, wxEmptyString, wxDIR_DIRS );

    while( cont )
    {
        if( !dirname.StartsWith( wxS( "." ) ) )
        {
            PLM_FOLDER folder;
            folder.id = PathToFolderId( dirname );
            folder.name = dirname;
            folder.parent_id = wxEmptyString;

            wxString sep = wxString( wxFileName::GetPathSeparator() );
            wxString fullDirPath = m_localClonePath + sep + dirname;
            wxDir    subDir( fullDirPath );
            bool     hasChild = false;
            wxString subdirName;

            if( subDir.IsOpened() )
                hasChild = subDir.GetFirst( &subdirName, wxEmptyString, wxDIR_DIRS );

            while( hasChild && subdirName.StartsWith( wxS( "." ) ) )
                hasChild = subDir.GetNext( &subdirName );

            folder.has_children = hasChild;
            folders.push_back( folder );
        }

        cont = dir.GetNext( &dirname );
    }

    return folders;
}


std::vector<PLM_FOLDER> PLM_GIT_PLUGIN::GetChildFolders( const wxString& aFolderId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_FOLDER> folders;

    if( !m_isAuthenticated )
        return folders;

    if( !EnsureClone() )
        return folders;

    wxString relativePath = FolderIdToPath( aFolderId );
    wxString fullPath = m_localClonePath + wxFileName::GetPathSeparator() + relativePath;

    wxDir dir( fullPath );

    if( !dir.IsOpened() )
        return folders;

    wxString dirname;
    bool cont = dir.GetFirst( &dirname, wxEmptyString, wxDIR_DIRS );

    while( cont )
    {
        if( !dirname.StartsWith( wxS( "." ) ) )
        {
            PLM_FOLDER folder;
            folder.id = PathToFolderId( relativePath + wxFileName::GetPathSeparator() + dirname );
            folder.name = dirname;
            folder.parent_id = aFolderId;

            wxString sep = wxString( wxFileName::GetPathSeparator() );
            wxString fullDirPath = fullPath + sep + dirname;
            wxDir    subDir( fullDirPath );
            bool     hasChild = false;
            wxString subdirName;

            if( subDir.IsOpened() )
                hasChild = subDir.GetFirst( &subdirName, wxEmptyString, wxDIR_DIRS );

            while( hasChild && subdirName.StartsWith( wxS( "." ) ) )
                hasChild = subDir.GetNext( &subdirName );

            folder.has_children = hasChild;
            folders.push_back( folder );
        }

        cont = dir.GetNext( &dirname );
    }

    return folders;
}


PLM_FOLDER_PATH PLM_GIT_PLUGIN::GetFolderPath( const wxString& aFolderId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_FOLDER_PATH path;
    wxString relativePath = FolderIdToPath( aFolderId );

    wxStringTokenizer tokenizer( relativePath, wxFileName::GetPathSeparator() );

    wxString cumulative;

    while( tokenizer.HasMoreTokens() )
    {
        wxString part = tokenizer.GetNextToken();

        if( !cumulative.IsEmpty() )
            cumulative += wxFileName::GetPathSeparator();

        cumulative += part;

        PLM_PATH_ELEMENT elem;
        elem.id = PathToFolderId( cumulative );
        elem.name = part;
        path.push_back( elem );
    }

    return path;
}


PLM_LIST_RESULT<PLM_FILE_INFO> PLM_GIT_PLUGIN::ListFiles( const wxString&         aFolderId,
                                                          const PLM_LIST_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_LIST_RESULT<PLM_FILE_INFO> result;

    if( !m_isAuthenticated )
        return result;

    if( !EnsureClone() )
        return result;

    wxString relativePath = FolderIdToPath( aFolderId );
    wxString fullPath = m_localClonePath;

    if( !relativePath.IsEmpty() )
        fullPath += wxFileName::GetPathSeparator() + relativePath;

    wxDir dir( fullPath );

    if( !dir.IsOpened() )
        return result;

    wxString filename;
    bool cont = dir.GetFirst( &filename, wxS( "*.kicad_pro" ), wxDIR_FILES );
    std::vector<PLM_FILE_INFO> files;

    while( cont )
    {
        PLM_FILE_INFO info;
        info.id = PathToFolderId( relativePath + wxFileName::GetPathSeparator() + filename );
        info.name = wxFileName( filename ).GetName();
        info.state_category = PLM_STATE_CATEGORY::EDITABLE;
        info.revision = wxS( "HEAD" );

        wxFileName fn( fullPath, filename );
        wxDateTime modTime;

        if( fn.GetTimes( nullptr, &modTime, nullptr ) )
            info.modified_timestamp = modTime.GetTicks();

        files.push_back( info );
        cont = dir.GetNext( &filename );
    }

    result.items = files;
    result.total_count = static_cast<int32_t>( files.size() );

    return result;
}


PLM_LIST_RESULT<PLM_FILE_INFO> PLM_GIT_PLUGIN::SearchFiles( const wxString&         aQuery,
                                                            const PLM_LIST_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_LIST_RESULT<PLM_FILE_INFO> result;

    if( !m_isAuthenticated )
        return result;

    if( !EnsureClone() )
        return result;

    // A `content:<pattern>` query runs `git grep -l` over KiCad design files
    // and returns the .kicad_pro that owns each match. Empty result on no
    // matches; queries without the prefix run filename-only matching.
    const wxString contentPrefix = wxS( "content:" );
    bool           contentSearch = aQuery.Lower().StartsWith( contentPrefix );

    wxString output;

    if( contentSearch )
    {
        wxString pattern = aQuery.Mid( contentPrefix.length() );

        if( pattern.IsEmpty() )
            return result;

        std::vector<wxString> args = { wxS( "grep" ), wxS( "-l" ), wxS( "--no-color" ),
                                       wxS( "-e" ), pattern, wxS( "--" ),
                                       wxS( "*.kicad_pro" ), wxS( "*.kicad_sch" ),
                                       wxS( "*.kicad_pcb" ) };

        // git grep returns 1 when there are no matches; both 0 (matches) and
        // 1 (no matches) are valid outcomes. Higher exit codes indicate real
        // errors (e.g., not in a git repo); leave the file list empty in
        // that case.
        RunGitCommand( args, output, m_localClonePath );
    }
    else
    {
        RunGitCommand( { wxS( "ls-files" ), wxS( "*.kicad_pro" ) }, output, m_localClonePath );
    }

    wxStringTokenizer tokenizer( output, wxS( "\n" ) );
    std::set<wxString> seenProjects;
    std::vector<PLM_FILE_INFO> allFiles;
    wxString lowerQuery = aQuery.Lower();

    while( tokenizer.HasMoreTokens() )
    {
        wxString filepath = tokenizer.GetNextToken().Trim();

        if( filepath.IsEmpty() )
            continue;

        // For content search results, normalize each match back to the
        // owning .kicad_pro so the result list is project-level (matches
        // the filename-search shape) and de-duplicated. Hierarchical
        // KiCad projects keep the .kicad_pro at the project root with
        // child .kicad_sch files in subdirectories, so walk up the parent
        // chain until a .kicad_pro is found or the repo root is reached.
        if( contentSearch )
        {
            wxString projectPath;

            if( filepath.EndsWith( wxS( ".kicad_pro" ) ) )
            {
                projectPath = filepath;
            }
            else
            {
                wxString dir = wxFileName( filepath ).GetPath();

                for( ;; )
                {
                    wxString listing;
                    wxString listPattern = dir.IsEmpty() ? wxS( "*.kicad_pro" )
                                                         : dir + wxS( "/*.kicad_pro" );

                    if( RunGitCommand( { wxS( "ls-files" ), listPattern }, listing,
                                       m_localClonePath ) == 0 )
                    {
                        // git ls-files matches descendants too, so filter to
                        // entries whose immediate parent is `dir`. Otherwise
                        // a root schematic could be mapped to a nested
                        // sub-project that happens to sort first.
                        wxStringTokenizer lineTok( listing, wxS( "\n" ) );

                        while( lineTok.HasMoreTokens() )
                        {
                            wxString candidate = lineTok.GetNextToken().Trim();

                            if( candidate.IsEmpty() )
                                continue;

                            wxString candidateDir = wxFileName( candidate ).GetPath();

                            if( candidateDir == dir )
                            {
                                projectPath = candidate;
                                break;
                            }
                        }

                        if( !projectPath.IsEmpty() )
                            break;
                    }

                    if( dir.IsEmpty() )
                        break;

                    int slash = dir.Find( '/', /* fromEnd = */ true );
                    dir = ( slash == wxNOT_FOUND ) ? wxString() : dir.Mid( 0, slash );
                }

                if( projectPath.IsEmpty() )
                    continue;
            }

            if( !seenProjects.insert( projectPath ).second )
                continue;

            filepath = projectPath;
        }

        wxString lowerPath = filepath.Lower();
        bool matches = contentSearch || aQuery.IsEmpty() || lowerPath.Contains( lowerQuery );

        // Support glob pattern matching with * and ?
        if( !matches && ( aQuery.Contains( wxS( "*" ) ) || aQuery.Contains( wxS( "?" ) ) ) )
            matches = wxMatchWild( aQuery, filepath, false );

        if( matches )
        {
            PLM_FILE_INFO info;
            wxString fullPath = m_localClonePath + wxFileName::GetPathSeparator() + filepath;
            wxFileName fn( fullPath );

            info.id = PathToFolderId( filepath );
            info.name = fn.GetName();
            info.state_category = PLM_STATE_CATEGORY::EDITABLE;
            info.revision = wxS( "HEAD" );

            if( fn.FileExists() )
            {
                wxDateTime modTime = fn.GetModificationTime();
                info.modified_timestamp = modTime.IsValid() ? modTime.GetTicks() : 0;
            }

            allFiles.push_back( info );
        }
    }

    // Apply pagination
    result.total_count = static_cast<int32_t>( allFiles.size() );
    result.offset = aOptions.offset;
    result.limit = aOptions.limit > 0 ? aOptions.limit : static_cast<int32_t>( allFiles.size() );

    if( result.offset < static_cast<int32_t>( allFiles.size() ) )
    {
        auto start = allFiles.begin() + result.offset;
        auto end = ( result.offset + result.limit < static_cast<int32_t>( allFiles.size() ) )
                           ? allFiles.begin() + result.offset + result.limit
                           : allFiles.end();
        result.items.assign( start, end );
    }

    return result;
}


PLM_LIST_RESULT<PLM_FILE_INFO>
PLM_GIT_PLUGIN::SearchWithFilters( const std::map<wxString, wxString>& aFilters,
                                    const PLM_LIST_OPTIONS&             aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    // First get all files matching any name filter
    wxString query;
    auto     nameIt = aFilters.find( wxS( "name" ) );

    if( nameIt != aFilters.end() )
        query = nameIt->second;

    PLM_LIST_OPTIONS opts = aOptions;
    opts.limit = 0;  // Get all results for filtering
    opts.offset = 0;

    PLM_LIST_RESULT<PLM_FILE_INFO> searchResult = SearchFiles( query, opts );

    // Now filter by additional criteria
    std::vector<PLM_FILE_INFO> filtered;

    for( const auto& file : searchResult.items )
    {
        bool matches = true;

        // Filter by state
        auto stateIt = aFilters.find( wxS( "state" ) );

        if( stateIt != aFilters.end() && matches )
        {
            int stateVal = wxAtoi( stateIt->second );

            if( static_cast<int>( file.state_category ) != stateVal )
                matches = false;
        }

        // Filter by modified_after (Unix timestamp)
        auto afterIt = aFilters.find( wxS( "modified_after" ) );

        if( afterIt != aFilters.end() && matches )
        {
            long afterTime = 0;

            if( afterIt->second.ToLong( &afterTime ) && file.modified_timestamp < afterTime )
                matches = false;
        }

        // Filter by modified_before (Unix timestamp)
        auto beforeIt = aFilters.find( wxS( "modified_before" ) );

        if( beforeIt != aFilters.end() && matches )
        {
            long beforeTime = 0;

            if( beforeIt->second.ToLong( &beforeTime ) && file.modified_timestamp > beforeTime )
                matches = false;
        }

        // Filter by folder/category
        auto categoryIt = aFilters.find( wxS( "category" ) );

        if( categoryIt != aFilters.end() && matches )
        {
            if( !file.id.StartsWith( categoryIt->second ) )
                matches = false;
        }

        if( matches )
            filtered.push_back( file );
    }

    // Apply pagination to filtered results
    PLM_LIST_RESULT<PLM_FILE_INFO> result;
    result.total_count = static_cast<int32_t>( filtered.size() );
    result.offset = aOptions.offset;
    result.limit = aOptions.limit > 0 ? aOptions.limit : static_cast<int32_t>( filtered.size() );

    if( result.offset < static_cast<int32_t>( filtered.size() ) )
    {
        auto start = filtered.begin() + result.offset;
        auto end = ( result.offset + result.limit < static_cast<int32_t>( filtered.size() ) )
                           ? filtered.begin() + result.offset + result.limit
                           : filtered.end();
        result.items.assign( start, end );
    }

    return result;
}


std::vector<PLM_SEARCH_FIELD> PLM_GIT_PLUGIN::GetSearchFields()
{
    std::vector<PLM_SEARCH_FIELD> fields;

    fields.push_back( PLM_SEARCH_FIELD::Text( wxS( "name" ), wxS( "Project Name" ) ) );
    fields.push_back( PLM_SEARCH_FIELD::Text( wxS( "category" ), wxS( "Folder Path" ) ) );

    fields.push_back( PLM_SEARCH_FIELD::Dropdown(
            wxS( "state" ), wxS( "State" ),
            { wxS( "Editable" ), wxS( "Locked" ), wxS( "Read-Only" ), wxS( "Conflicted" ) } ) );

    fields.push_back( PLM_SEARCH_FIELD::Date( wxS( "modified_after" ), wxS( "Modified After" ) ) );
    fields.push_back( PLM_SEARCH_FIELD::Date( wxS( "modified_before" ), wxS( "Modified Before" ) ) );

    return fields;
}


PLM_LIST_RESULT<PLM_PART> PLM_GIT_PLUGIN::SearchParts( const PLM_PART_SEARCH_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_LIST_RESULT<PLM_PART> result;

    if( !m_isAuthenticated || m_localClonePath.IsEmpty() )
        return result;

    wxString partsFile = m_localClonePath + wxFileName::GetPathSeparator() + wxS( "parts.csv" );

    if( !wxFileExists( partsFile ) )
        return result;

    time_t fileModTime = wxFileModificationTime( partsFile );
    bool   needsReparse = m_partsCache.empty() || fileModTime != m_partsCacheModTime;

    if( needsReparse )
    {
        m_partsCache.clear();

        wxFileInputStream input( partsFile );

        if( !input.IsOk() )
            return result;

        wxTextInputStream text( input );
        bool                    headerParsed = false;
        std::map<wxString, int> columnIndex;

        while( !input.Eof() )
        {
            wxString line = text.ReadLine();

            if( line.IsEmpty() )
                continue;

            wxArrayString fields;
            wxString      current;
            bool          inQuotes = false;

            for( size_t i = 0; i < line.length(); ++i )
            {
                wxUniChar c = line[i];

                if( c == '"' )
                {
                    inQuotes = !inQuotes;
                }
                else if( c == ',' && !inQuotes )
                {
                    fields.Add( current );
                    current.clear();
                }
                else
                {
                    current += c;
                }
            }

            fields.Add( current );

            if( !headerParsed )
            {
                for( size_t i = 0; i < fields.size(); ++i )
                {
                    wxString header = fields[i].Lower();
                    columnIndex[header] = static_cast<int>( i );
                }

                headerParsed = true;
                continue;
            }

            PLM_PART part;
            auto     getField = [&]( const wxString& name ) -> wxString {
                auto it = columnIndex.find( name.Lower() );

                if( it != columnIndex.end() && it->second < static_cast<int>( fields.size() ) )
                    return fields[it->second];

                return wxEmptyString;
            };

            part.part_number = getField( wxS( "part_number" ) );

            if( part.part_number.IsEmpty() )
                part.part_number = getField( wxS( "mpn" ) );

            part.part_id = part.part_number;
            part.description = getField( wxS( "description" ) );
            part.manufacturer = getField( wxS( "manufacturer" ) );
            part.manufacturer_pn = getField( wxS( "manufacturer_pn" ) );

            if( part.manufacturer_pn.IsEmpty() )
                part.manufacturer_pn = getField( wxS( "mpn" ) );

            part.category = getField( wxS( "category" ) );
            part.datasheet_url = getField( wxS( "datasheet" ) );
            part.symbol_ref = getField( wxS( "symbol" ) );
            part.footprint_ref = getField( wxS( "footprint" ) );

            wxString state = getField( wxS( "state" ) ).Lower();

            if( state == wxS( "obsolete" ) )
                part.state = PLM_PART_STATE::OBSOLETE;
            else if( state == wxS( "eol" ) || state == wxS( "end_of_life" ) )
                part.state = PLM_PART_STATE::END_OF_LIFE;
            else if( state == wxS( "preferred" ) )
                part.state = PLM_PART_STATE::PREFERRED;
            else if( state == wxS( "not_recommended" ) || state == wxS( "nrnd" ) )
                part.state = PLM_PART_STATE::NOT_RECOMMENDED;
            else
                part.state = PLM_PART_STATE::ACTIVE;

            // Vendor columns are optional in the CSV
            wxString vendorName = getField( wxS( "vendor_name" ) );
            wxString vendorPn = getField( wxS( "vendor_part_number" ) );
            wxString price = getField( wxS( "price" ) );
            wxString leadTime = getField( wxS( "lead_time" ) );

            if( !vendorName.IsEmpty() )
                part.attributes.SetValue( wxS( "vendor_name" ), vendorName );

            if( !vendorPn.IsEmpty() )
                part.attributes.SetValue( wxS( "vendor_part_number" ), vendorPn );

            if( !price.IsEmpty() )
                part.attributes.SetValue( wxS( "price" ), price );

            if( !leadTime.IsEmpty() )
                part.attributes.SetValue( wxS( "lead_time" ), leadTime );

            m_partsCache.push_back( part );
        }

        m_partsCacheModTime = fileModTime;
    }

    std::vector<PLM_PART> filteredParts;

    for( const PLM_PART& part : m_partsCache )
    {
        if( !aOptions.include_obsolete && part.NeedsReplacement() )
            continue;

        bool matches = true;

        if( !aOptions.query.IsEmpty() )
        {
            wxString query = aOptions.query.Lower();

            if( aOptions.exact_match )
            {
                matches = part.part_number.Lower() == query;
            }
            else
            {
                matches = part.part_number.Lower().Contains( query )
                          || part.description.Lower().Contains( query )
                          || part.manufacturer.Lower().Contains( query );
            }
        }

        if( matches && !aOptions.category_filter.IsEmpty() )
        {
            matches = part.category.Lower().Contains( aOptions.category_filter.Lower() );
        }

        if( matches && !aOptions.manufacturer_filter.IsEmpty() )
        {
            matches = part.manufacturer.Lower().Contains( aOptions.manufacturer_filter.Lower() );
        }

        if( matches )
            filteredParts.push_back( part );
    }

    result.total_count = static_cast<int32_t>( filteredParts.size() );
    result.offset = static_cast<int32_t>( aOptions.offset );
    result.limit = static_cast<int32_t>( aOptions.limit );

    size_t start = aOptions.offset;
    size_t end = std::min( start + aOptions.limit, filteredParts.size() );

    if( start < filteredParts.size() )
    {
        for( size_t i = start; i < end; ++i )
            result.items.push_back( filteredParts[i] );
    }

    return result;
}


PLM_PART PLM_GIT_PLUGIN::GetPart( const wxString& aPartId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_PART_SEARCH_OPTIONS opts;
    opts.query = aPartId;
    opts.exact_match = true;
    opts.limit = 1;

    PLM_LIST_RESULT<PLM_PART> result = SearchParts( opts );

    if( !result.items.empty() )
        return result.items[0];

    return PLM_PART();
}


PLM_BOM_VALIDATION_RESULT PLM_GIT_PLUGIN::ValidateBOM( const std::vector<wxString>& aPartNumbers )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_BOM_VALIDATION_RESULT result;
    result.total_parts = aPartNumbers.size();

    if( !m_isAuthenticated || aPartNumbers.empty() )
    {
        result.valid_parts = result.total_parts;
        return result;
    }

    PLM_PART_SEARCH_OPTIONS opts;
    opts.include_obsolete = true;
    opts.limit = 10000;

    PLM_LIST_RESULT<PLM_PART> allParts = SearchParts( opts );
    std::map<wxString, PLM_PART> partsMap;

    for( const auto& part : allParts.items )
        partsMap[part.part_number] = part;

    for( const auto& partNum : aPartNumbers )
    {
        if( partNum.IsEmpty() )
        {
            ++result.valid_parts;
            continue;
        }

        auto it = partsMap.find( partNum );

        if( it == partsMap.end() )
        {
            result.AddIssue( PLM_BOM_ISSUE::Missing( partNum, partNum ) );
            continue;
        }

        const PLM_PART& part = it->second;

        if( part.state == PLM_PART_STATE::OBSOLETE ||
            part.state == PLM_PART_STATE::END_OF_LIFE ||
            part.state == PLM_PART_STATE::DISCONTINUED )
        {
            result.AddIssue(
                    PLM_BOM_ISSUE::Obsolete( partNum, partNum, part.replacement_id ) );
        }
        else if( part.state == PLM_PART_STATE::NOT_RECOMMENDED )
        {
            result.AddIssue(
                    PLM_BOM_ISSUE::NotRecommended( partNum, partNum, part.replacement_id ) );
        }
        else
        {
            ++result.valid_parts;
        }
    }

    result.all_valid = result.issues.empty();
    return result;
}


std::vector<PLM_PART_REPLACEMENT> PLM_GIT_PLUGIN::GetReplacements( const wxString& aPartId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_PART_REPLACEMENT> replacements;

    PLM_PART part = GetPart( aPartId );

    if( part.HasReplacement() )
    {
        PLM_PART replacementPart = GetPart( part.replacement_id );

        if( !replacementPart.part_id.IsEmpty() )
        {
            PLM_PART_REPLACEMENT repl( aPartId, replacementPart.part_id,
                                        wxS( "Replacement from parts database" ) );
            replacements.push_back( repl );
        }
    }

    return replacements;
}


bool PLM_GIT_PLUGIN::DownloadPartLibrary( const wxString& aPartId, const wxString& aLocalPath )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || aPartId.IsEmpty() )
        return false;

    PLM_PART part = GetPart( aPartId );

    if( part.part_id.IsEmpty() )
        return false;

    if( !wxDirExists( aLocalPath ) )
        wxMkdir( aLocalPath );

    bool anyDownloaded = false;

    if( !part.symbol_ref.IsEmpty() )
    {
        wxString sourcePath = m_localClonePath + wxFileName::GetPathSeparator() + part.symbol_ref;

        if( wxFileExists( sourcePath ) )
        {
            wxString destPath = aLocalPath + wxFileName::GetPathSeparator() +
                                wxFileName( part.symbol_ref ).GetFullName();

            if( wxCopyFile( sourcePath, destPath ) )
                anyDownloaded = true;
        }
    }

    if( !part.footprint_ref.IsEmpty() )
    {
        wxString sourcePath = m_localClonePath + wxFileName::GetPathSeparator() + part.footprint_ref;

        if( wxFileExists( sourcePath ) )
        {
            wxString destPath = aLocalPath + wxFileName::GetPathSeparator() +
                                wxFileName( part.footprint_ref ).GetFullName();

            if( wxCopyFile( sourcePath, destPath ) )
                anyDownloaded = true;
        }
    }

    return anyDownloaded;
}


PLM_CHECKOUT_RESULT PLM_GIT_PLUGIN::Checkout( const wxString&             aProjectId,
                                               const wxString&             aLocalPath,
                                               const PLM_CHECKOUT_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_CHECKOUT_RESULT result;

    if( !m_isAuthenticated )
    {
        result.success = false;
        result.error_message = wxS( "Not authenticated" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT, aProjectId, false, result.error_message );
        return result;
    }

    if( !EnsureClone() )
    {
        result.success = false;
        result.error_message = wxS( "Failed to clone repository" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT, aProjectId, false, result.error_message );
        return result;
    }

    wxString fetchOutput;
    int fetchRc = RunGitCommand( { wxS( "fetch" ), wxS( "origin" ) }, fetchOutput, m_localClonePath );

    if( fetchRc != 0 )
    {
        result.success = false;
        result.error_message = wxS( "Failed to fetch from remote" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT, aProjectId, false, result.error_message );
        return result;
    }

    wxString currentBranch;
    RunGitCommand( { wxS( "rev-parse" ), wxS( "--abbrev-ref" ), wxS( "HEAD" ) },
                   currentBranch, m_localClonePath );
    currentBranch.Trim();

    int resetRc = RunGitCommand( { wxS( "reset" ), wxS( "--hard" ), wxS( "origin/" ) + currentBranch },
                                 fetchOutput, m_localClonePath );

    if( resetRc != 0 )
    {
        result.success = false;
        result.error_message = wxS( "Failed to sync local clone with remote" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT, aProjectId, false, result.error_message );
        return result;
    }

    wxString savedRef;
    bool     restoreHead = false;

    if( !aOptions.revision.IsEmpty() )
    {
        RunGitCommand( { wxS( "rev-parse" ), wxS( "HEAD" ) }, savedRef, m_localClonePath );
        savedRef.Trim();

        wxString checkoutOutput;
        int checkoutRc = RunGitCommand( { wxS( "checkout" ), aOptions.revision },
                                        checkoutOutput, m_localClonePath );

        if( checkoutRc != 0 )
        {
            result.success = false;
            result.error_message = wxString::Format( wxS( "Failed to checkout revision %s" ),
                                                     aOptions.revision );
            RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT, aProjectId, false,
                              result.error_message );
            return result;
        }

        restoreHead = true;
    }

    if( aOptions.acquire_lock )
    {
        PLM_LOCK_REQUEST lockReq;
        lockReq.project_id = aProjectId;
        PLM_LOCK_RESULT lockResult = Lock( lockReq );

        if( !lockResult.success )
        {
            if( restoreHead )
                RunGitCommand( { wxS( "checkout" ), savedRef }, fetchOutput, m_localClonePath );

            result.success = false;
            result.error_code = lockResult.error_code;
            result.error_message = lockResult.error_message;
            RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT, aProjectId, false,
                              result.error_message );
            return result;
        }
    }

    wxString relativePath = FolderIdToPath( aProjectId );
    wxString sourcePath = m_localClonePath + wxFileName::GetPathSeparator() + relativePath;

    if( !wxDirExists( aLocalPath ) )
        wxMkdir( aLocalPath );

    wxDir sourceDir( wxFileName( sourcePath ).GetPath() );
    wxString basename = wxFileName( sourcePath ).GetName();
    wxString filename;
    bool cont = sourceDir.GetFirst( &filename, basename + wxS( ".*" ), wxDIR_FILES );

    while( cont )
    {
        wxString src = wxFileName( sourcePath ).GetPath() + wxFileName::GetPathSeparator() +
                       filename;
        wxString dst = aLocalPath + wxFileName::GetPathSeparator() + filename;
        wxCopyFile( src, dst );

        PLM_DIRTY_TRACKER::GetInstance().RecordCheckout(
                dst, PLM_DIRTY_TRACKER::CalculateFileHash( dst ) );

        cont = sourceDir.GetNext( &filename );
    }

    // Capture the commit hash while the requested revision is still checked out
    wxString commitHash;
    RunGitCommand( { wxS( "rev-parse" ), wxS( "HEAD" ) }, commitHash, m_localClonePath );
    commitHash.Trim();

    if( restoreHead )
        RunGitCommand( { wxS( "checkout" ), savedRef }, fetchOutput, m_localClonePath );

    WritePLMCheckoutMetadata( aLocalPath, wxS( "plm-git" ), aProjectId, commitHash,
                              m_username, m_repositoryUrl );

    result.success = true;
    result.local_path = aLocalPath;
    result.file_info.id = aProjectId;
    result.file_info.revision = commitHash;
    result.file_info.state_category = PLM_STATE_CATEGORY::EDITABLE;

    PersistProjectState( aLocalPath, aProjectId, commitHash );
    RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT, aProjectId, true, commitHash );

    return result;
}


PLM_CHECKIN_RESULT PLM_GIT_PLUGIN::Checkin( const wxString&            aLocalPath,
                                             const PLM_CHECKIN_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_CHECKIN_RESULT result;

    if( !m_isAuthenticated )
    {
        result.success = false;
        result.error_message = wxS( "Not authenticated" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKIN, wxEmptyString, false, result.error_message );
        return result;
    }

    PLM_PERMISSIONS perms = GetPermissions( aLocalPath );

    if( !perms.can_checkin )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_AUTH_INSUFFICIENT;
        result.error_message = wxString::Format(
                wxS( "You do not have permission to check in. Your role: %s. Required: write access" ),
                m_detectedRole );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKIN, wxEmptyString, false, result.error_message );
        return result;
    }

    // Drain any pending pushes from prior offline checkins before any path
    // that might short-circuit (e.g. "nothing to commit" when the user
    // retries after fixing connectivity but with no further file changes).
    DrainPendingPushes();

    // Read the .kicad_plm metadata to determine the project ID and verify ownership
    wxString projectId;
    wxString metadataPath = aLocalPath + wxFileName::GetPathSeparator() + wxS( ".kicad_plm" );

    if( wxFileExists( metadataPath ) )
    {
        wxFile metaFile( metadataPath, wxFile::read );

        if( metaFile.IsOpened() )
        {
            wxString metaContent;
            metaFile.ReadAll( &metaContent );
            metaFile.Close();

            try
            {
                nlohmann::json metadata = nlohmann::json::parse( metaContent.ToStdString() );

                if( metadata.contains( "project_id" ) )
                    projectId = wxString::FromUTF8( metadata["project_id"].get<std::string>() );

                if( metadata.contains( "user" ) )
                {
                    wxString checkoutUser =
                            wxString::FromUTF8( metadata["user"].get<std::string>() );

                    if( !checkoutUser.IsEmpty() && checkoutUser != m_username )
                    {
                        wxString detail = wxString::Format(
                                wxS( "Project was checked out by %s, not the current user %s" ),
                                checkoutUser, m_username );
                        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKIN, projectId, false, detail );
                        return PLM_CHECKIN_RESULT::Failure( PLM_ERROR_CODE::ERR_LOCK_NOT_HELD,
                                                            detail );
                    }
                }
            }
            catch( const nlohmann::json::exception& )
            {
                // Malformed metadata; treat as unmanaged checkin
            }
        }
    }

    wxString output;
    wxString stagePath = wxS( "." );

    if( !projectId.IsEmpty() )
    {
        wxString relativePath = FolderIdToPath( projectId );

        if( !relativePath.IsEmpty() )
        {
            wxString repoProjectDir =
                    m_localClonePath + wxFileName::GetPathSeparator() + relativePath;

            // Copy modified files from the local checkout back into the repo clone
            if( wxDirExists( aLocalPath ) && wxDirExists( repoProjectDir ) )
            {
                wxDir sourceDir( aLocalPath );
                wxString filename;

                wxString dirname;
                bool hasSubdirs = sourceDir.GetFirst( &dirname, wxEmptyString, wxDIR_DIRS );

                if( hasSubdirs )
                    wxLogWarning( wxS( "Subdirectories in checkout are not included in checkin" ) );

                bool cont = sourceDir.GetFirst( &filename, wxEmptyString, wxDIR_FILES );

                while( cont )
                {
                    // Skip PLM metadata files
                    if( filename != wxS( ".kicad_plm" ) )
                    {
                        wxString src =
                                aLocalPath + wxFileName::GetPathSeparator() + filename;
                        wxString dst = repoProjectDir
                                       + wxFileName::GetPathSeparator() + filename;
                        wxCopyFile( src, dst );
                    }

                    cont = sourceDir.GetNext( &filename );
                }
            }

            stagePath = relativePath;
        }
    }

    RunGitCommand( { wxS( "add" ), stagePath }, output, m_localClonePath );

    wxString commitMsg = aOptions.revision_comment.IsEmpty() ? wxS( "KiCad PLM checkin" )
                                                               : aOptions.revision_comment;

    if( !aOptions.eco_id.IsEmpty() )
        commitMsg += wxS( "\n\nECO-ref: " ) + aOptions.eco_id;

    int rc = RunGitCommand( { wxS( "commit" ), wxS( "-m" ), commitMsg }, output, m_localClonePath );

    if( rc != 0 )
    {
        if( output.Contains( wxS( "nothing to commit" ) ) )
        {
            RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKIN, projectId, false,
                              wxS( "No changes to check in" ) );
            return PLM_CHECKIN_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                                wxS( "No changes to check in" ) );
        }

        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_INTERNAL;
        result.error_message = wxS( "Git commit failed" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKIN, projectId, false, result.error_message );
        return result;
    }

    RunGitCommand( { wxS( "rev-parse" ), wxS( "HEAD" ) }, output, m_localClonePath );
    wxString newRevision = output.Trim();

    wxString currentBranch;
    RunGitCommand( { wxS( "rev-parse" ), wxS( "--abbrev-ref" ), wxS( "HEAD" ) }, currentBranch,
                   m_localClonePath );
    currentBranch.Trim();

    rc = RunGitCommand( { wxS( "push" ) }, output, m_localClonePath );

    if( rc != 0 )
    {
        // Local commit succeeded; the remote is unreachable or rejected the
        // push. Keep the commit so the user does not lose work, queue the
        // branch for retry, and surface a non-fatal warning to the caller.
        if( !currentBranch.IsEmpty() )
            RecordPendingPush( currentBranch );

        result.success = true;
        result.new_revision = newRevision;
        result.file_info.revision = newRevision;
        result.file_info.state_category = PLM_STATE_CATEGORY::EDITABLE;
        result.error_message =
                wxS( "Push to remote failed; commit kept locally and queued for retry." );

        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKIN, projectId, true, result.error_message );
        PersistProjectState( aLocalPath, projectId, newRevision );
        return result;
    }

    RunGitCommand( { wxS( "push" ), wxS( "origin" ), wxS( "--tags" ) }, output, m_localClonePath );

    result.success = true;
    result.new_revision = newRevision;
    result.file_info.revision = newRevision;
    result.file_info.state_category = PLM_STATE_CATEGORY::EDITABLE;

    // Mark all tracked files in the checkout directory as clean after successful push
    if( wxDirExists( aLocalPath ) )
    {
        wxDir cleanDir( aLocalPath );
        wxString cleanFilename;
        bool hasCont = cleanDir.GetFirst( &cleanFilename, wxEmptyString, wxDIR_FILES );

        while( hasCont )
        {
            if( cleanFilename != wxS( ".kicad_plm" ) )
            {
                wxString filePath =
                        aLocalPath + wxFileName::GetPathSeparator() + cleanFilename;

                PLM_DIRTY_TRACKER::GetInstance().MarkClean( filePath );
            }

            hasCont = cleanDir.GetNext( &cleanFilename );
        }
    }

    if( aOptions.release_lock && !projectId.IsEmpty() && GetCapabilities().supports_locking )
    {
        PLM_UNLOCK_REQUEST unlockReq( projectId );
        PLM_LOCK_RESULT    unlockResult = Unlock( unlockReq );

        if( !unlockResult.success )
        {
            wxLogWarning( wxS( "Checkin succeeded but lock release failed for project %s" ),
                          projectId );
        }
    }

    wxString commonMetadataPath = aLocalPath + wxFileName::GetPathSeparator() + wxS( ".kicad_plm" );

    if( wxFileExists( commonMetadataPath ) )
        wxRemoveFile( commonMetadataPath );

    if( result.success )
        PersistProjectState( aLocalPath, projectId, result.new_revision );

    RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKIN, projectId, result.success, result.new_revision );

    return result;
}


PLM_FILE_INFO PLM_GIT_PLUGIN::GetFileInfo( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_FILE_INFO info;

    if( !m_isAuthenticated )
        return info;

    wxString relativePath = FolderIdToPath( aProjectId );
    wxString fullPath = m_localClonePath + wxFileName::GetPathSeparator() + relativePath;

    wxFileName fn( fullPath );

    if( !fn.FileExists() && !wxDirExists( fn.GetPath() ) )
        return info;

    info.id = aProjectId;
    info.name = fn.GetName();
    info.state_category = PLM_STATE_CATEGORY::EDITABLE;

    wxString output;
    RunGitCommand( { wxS( "rev-parse" ), wxS( "HEAD" ) }, output, m_localClonePath );
    info.revision = output.Trim();

    wxDateTime modTime;

    if( fn.GetTimes( nullptr, &modTime, nullptr ) )
        info.modified_timestamp = modTime.GetTicks();

    return info;
}


PLM_CHECKOUT_RESULT PLM_GIT_PLUGIN::CreateProject( const wxString&          aFolderId,
                                                    const wxString&          aName,
                                                    const PLM_ATTRIBUTE_SET& aMetadata,
                                                    const wxString&          aTemplateId,
                                                    const PLM_CLONE_OPTIONS& aCloneOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_CHECKOUT_RESULT result;

    if( !m_isAuthenticated )
    {
        result.success = false;
        result.error_message = wxS( "Not authenticated" );
        return result;
    }

    wxString relativePath = FolderIdToPath( aFolderId );
    wxString sep = wxString( wxFileName::GetPathSeparator() );
    wxString projectPath = m_localClonePath;

    if( !relativePath.IsEmpty() )
        projectPath += sep + relativePath;

    projectPath += sep + aName;

    if( wxDirExists( projectPath ) )
    {
        wxString plmAttrsPath = projectPath + sep + wxS( ".kicad_plm_attrs" );

        if( wxFileExists( plmAttrsPath ) )
        {
            result.success = false;
            result.error_code = PLM_ERROR_CODE::ERR_FILE_EXISTS;
            result.error_message = wxString::Format(
                    wxS( "Project '%s' is already registered in PLM" ), aName );
            return result;
        }

        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_FILE_EXISTS;
        result.error_message = wxString::Format(
                wxS( "Directory '%s' already exists" ), aName );
        return result;
    }

    if( !wxMkdir( projectPath ) )
    {
        result.success = false;
        result.error_message = wxS( "Failed to create project directory" );
        return result;
    }

    if( !aTemplateId.IsEmpty() )
    {
        wxString templatePath = FolderIdToPath( aTemplateId );
        wxString sourceDir = m_localClonePath + sep + templatePath;
        wxFileName sourceFn( sourceDir );
        wxString sourceProjectDir = sourceFn.GetPath();

        if( !wxDirExists( sourceProjectDir ) )
        {
            result.success = false;
            result.error_code = PLM_ERROR_CODE::ERR_FILE_NOT_FOUND;
            result.error_message = wxString::Format(
                    wxS( "Source project directory not found: %s" ), templatePath );
            return result;
        }

        wxDir dir( sourceProjectDir );
        wxString filename;
        bool cont = dir.GetFirst( &filename, wxEmptyString, wxDIR_FILES );

        while( cont )
        {
            wxString src = sourceProjectDir + sep + filename;
            wxString dst = projectPath + sep + filename;
            wxCopyFile( src, dst );
            cont = dir.GetNext( &filename );
        }
    }
    else
    {
        wxString proFile = projectPath + sep + aName + wxS( ".kicad_pro" );
        wxFile   file( proFile, wxFile::write );

        if( !file.IsOpened() )
        {
            result.success = false;
            result.error_message = wxS( "Failed to create project file" );
            return result;
        }

        file.Write( wxS( "{\n  \"meta\": {\n    \"filename\": \"" ) + aName +
                    wxS( ".kicad_pro\",\n    \"version\": 1\n  }\n}\n" ) );
        file.Close();
    }

    wxString gitignorePath = projectPath + sep + wxS( ".gitignore" );

    if( !wxFileExists( gitignorePath ) )
    {
        wxFile gitignoreFile( gitignorePath, wxFile::write );

        if( gitignoreFile.IsOpened() )
        {
            gitignoreFile.Write( wxS( "# KiCad backup and autosave\n"
                                       "*-backups/\n"
                                       "_autosave-*\n"
                                       "*.tmp\n"
                                       "*.bak\n"
                                       "\n"
                                       "# Build outputs\n"
                                       "fp-info-cache\n"
                                       "*.kicad_pcb-bak\n"
                                       "*~\n"
                                       "\n"
                                       "# Generated manufacturing outputs\n"
                                       "gerber/\n"
                                       "fabrication/\n" ) );
            gitignoreFile.Close();
        }
    }

    wxString relativeProjectPath = aName;

    if( !relativePath.IsEmpty() )
        relativeProjectPath = relativePath + sep + aName;

    wxString output;
    RunGitCommand( { wxS( "add" ), relativeProjectPath }, output, m_localClonePath );

    wxString commitMsg;

    if( !aTemplateId.IsEmpty() )
    {
        commitMsg = wxString::Format( wxS( "Clone design '%s' from %s" ), aName,
                                      FolderIdToPath( aTemplateId ) );
    }
    else
    {
        commitMsg = wxString::Format( wxS( "Register new design: %s" ), aName );
    }
    int rc = RunGitCommand( { wxS( "commit" ), wxS( "-m" ), commitMsg }, output, m_localClonePath );

    if( rc != 0 )
        wxLogWarning( wxS( "Git commit failed for new project '%s'" ), aName );

    if( rc == 0 )
    {
        rc = RunGitCommand( { wxS( "push" ) }, output, m_localClonePath );

        if( rc != 0 )
            wxLogWarning( wxS( "Git push failed for new project '%s'. Push manually later." ), aName );
    }

    wxString commitHash;
    RunGitCommand( { wxS( "rev-parse" ), wxS( "HEAD" ) }, commitHash, m_localClonePath );
    commitHash = commitHash.Trim();

    result.success = true;
    result.file_info.id = PathToFolderId( relativePath + sep + aName + sep + aName +
                                           wxS( ".kicad_pro" ) );
    result.file_info.name = aName;
    result.file_info.revision = commitHash;
    result.file_info.state_category = PLM_STATE_CATEGORY::EDITABLE;
    result.local_path = projectPath;

    return result;
}


bool PLM_GIT_PLUGIN::DeleteProject( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated )
        return false;

    PLM_PERMISSIONS perms = GetPermissions( aProjectId );

    if( !perms.can_delete )
    {
        wxLogWarning( wxS( "Permission denied: cannot delete project. Your role: %s" ),
                      m_detectedRole );
        return false;
    }

    wxString relativePath = FolderIdToPath( aProjectId );

    wxString output;
    int rc = RunGitCommand( { wxS( "rm" ), wxS( "-rf" ), relativePath }, output, m_localClonePath );

    return rc == 0;
}


PLM_CANCEL_CHECKOUT_RESULT PLM_GIT_PLUGIN::CancelCheckout( const wxString& aLocalPath )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated )
    {
        return PLM_CANCEL_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                                     wxS( "Not authenticated" ) );
    }

    if( aLocalPath.IsEmpty() || !wxDirExists( aLocalPath ) )
    {
        return PLM_CANCEL_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_FILE_NOT_FOUND,
                                                     wxS( "Checkout path not found" ) );
    }

    // Verify checkout ownership via common PLM metadata
    wxString plmMetadataPath = aLocalPath + wxFileName::GetPathSeparator() + wxS( ".kicad_plm" );
    wxString projectId;

    if( wxFileExists( plmMetadataPath ) )
    {
        try
        {
            wxFile metaFile( plmMetadataPath, wxFile::read );

            if( metaFile.IsOpened() )
            {
                wxString metaContent;
                metaFile.ReadAll( &metaContent );
                metaFile.Close();

                nlohmann::json metadata = nlohmann::json::parse( metaContent.ToStdString() );

                if( metadata.contains( "user" ) && metadata["user"].is_string() )
                {
                    wxString metaUser = wxString::FromUTF8( metadata["user"].get<std::string>() );

                    if( !metaUser.IsEmpty() && metaUser != m_username )
                    {
                        return PLM_CANCEL_CHECKOUT_RESULT::Failure(
                                PLM_ERROR_CODE::ERR_FILE_ACCESS_DENIED,
                                wxS( "Checkout belongs to a different user" ) );
                    }
                }

                if( metadata.contains( "project_id" ) && metadata["project_id"].is_string() )
                    projectId = wxString::FromUTF8( metadata["project_id"].get<std::string>() );
            }
        }
        catch( const nlohmann::json::exception& )
        {
            // Malformed metadata, proceed with cancel
        }
    }

    // Checkout copies files to an external directory outside the repo clone.
    // If the path is external, just delete the copied files. If it happens to
    // be inside the repo (legacy behavior), fall back to git revert.
    if( !aLocalPath.StartsWith( m_localClonePath ) )
    {
        wxDir dir( aLocalPath );
        wxString filename;
        bool cont = dir.GetFirst( &filename, wxEmptyString, wxDIR_FILES );

        while( cont )
        {
            wxRemoveFile( aLocalPath + wxFileName::GetPathSeparator() + filename );
            cont = dir.GetNext( &filename );
        }

        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT_CANCELLED, projectId, true );
        return PLM_CANCEL_CHECKOUT_RESULT::Success();
    }

    // Legacy path: the checkout directory is within the repo clone
    wxString output;
    wxString relativePath = aLocalPath.Mid( m_localClonePath.length() );

    if( relativePath.StartsWith( wxFileName::GetPathSeparator() ) )
        relativePath = relativePath.Mid( 1 );

    // Reset any staged changes
    RunGitCommand( { wxS( "reset" ), wxS( "HEAD" ), wxS( "--" ), relativePath }, output,
                   m_localClonePath );

    // Discard uncommitted changes
    int rc = RunGitCommand( { wxS( "checkout" ), wxS( "--" ), relativePath }, output,
                            m_localClonePath );

    if( rc != 0 )
    {
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT_CANCELLED, projectId, false,
                          wxS( "Failed to revert local changes" ) );
        return PLM_CANCEL_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_CANCEL_FAILED,
                                                     wxS( "Failed to revert local changes" ) );
    }

    // Clean untracked files
    RunGitCommand( { wxS( "clean" ), wxS( "-fd" ), relativePath }, output, m_localClonePath );

    RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT_CANCELLED, projectId, true );
    return PLM_CANCEL_CHECKOUT_RESULT::Success();
}


PLM_REVISION_HISTORY PLM_GIT_PLUGIN::GetRevisionHistory( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_REVISION_HISTORY history;

    if( !m_isAuthenticated )
        return history;

    wxString relativePath = FolderIdToPath( aProjectId );
    wxString output;

    int rc = RunGitCommand( { wxS( "log" ), wxS( "--pretty=format:%H|%an|%ae|%at|%s" ),
                              wxS( "--" ), relativePath },
                            output, m_localClonePath );

    if( rc != 0 )
        return history;

    history.revisions = ParseGitLog( output );
    history.file_id = aProjectId;

    return history;
}


PLM_REVISION PLM_GIT_PLUGIN::GetRevision( const wxString& aProjectId, const wxString& aRevisionId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_REVISION revision;

    if( !m_isAuthenticated )
        return revision;

    wxString output;
    int rc = RunGitCommand( { wxS( "show" ), wxS( "-s" ),
                              wxS( "--pretty=format:%H|%an|%ae|%at|%s" ), aRevisionId },
                            output, m_localClonePath );

    if( rc != 0 )
        return revision;

    wxStringTokenizer tokenizer( output, wxS( "|" ) );

    if( tokenizer.CountTokens() >= 5 )
    {
        revision.revision_id = tokenizer.GetNextToken();
        revision.author = tokenizer.GetNextToken();
        wxString email = tokenizer.GetNextToken();
        wxString timestamp = tokenizer.GetNextToken();
        long ts;

        if( timestamp.ToLong( &ts ) )
            revision.timestamp = static_cast<int64_t>( ts );

        revision.comment = tokenizer.GetNextToken();
        revision.revision_number = revision.revision_id.Left( 8 );
        revision.state_category = PLM_STATE_CATEGORY::RELEASED;
    }

    return revision;
}


bool PLM_GIT_PLUGIN::DownloadRevision( const wxString& aProjectId, const wxString& aRevisionId,
                                        const wxString& aLocalPath )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated )
        return false;

    wxString relativePath = FolderIdToPath( aProjectId );
    wxString output;

    // List all files under this path at the given revision
    int rc = RunGitCommand( { wxS( "ls-tree" ), wxS( "-r" ), wxS( "--name-only" ),
                              aRevisionId, relativePath },
                            output, m_localClonePath );

    if( rc != 0 || output.Trim().IsEmpty() )
    {
        // May be a single file; try direct extraction
        rc = RunGitCommand( { wxS( "show" ), aRevisionId + wxS( ":" ) + relativePath },
                            output, m_localClonePath );

        if( rc != 0 )
            return false;

        wxFile file( aLocalPath, wxFile::write );

        if( !file.IsOpened() )
            return false;

        if( !file.Write( output ) )
            return false;

        return true;
    }

    // Multiple files under project path -- extract each to the output directory
    if( !wxDirExists( aLocalPath ) )
        wxFileName::Mkdir( aLocalPath, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );

    bool anyExtracted = false;
    wxStringTokenizer lineTokenizer( output, wxS( "\n" ) );

    while( lineTokenizer.HasMoreTokens() )
    {
        wxString filePath = lineTokenizer.GetNextToken().Trim();

        if( filePath.IsEmpty() )
            continue;

        wxString fileContent;
        rc = RunGitCommand( { wxS( "show" ), aRevisionId + wxS( ":" ) + filePath },
                            fileContent, m_localClonePath );

        if( rc != 0 )
            continue;

        // Strip the project prefix to get relative path within the project
        wxString relativeToProject = filePath;

        if( relativeToProject.StartsWith( relativePath ) )
        {
            relativeToProject = relativeToProject.Mid( relativePath.length() );

            if( relativeToProject.StartsWith( wxFileName::GetPathSeparator() )
                || relativeToProject.StartsWith( wxS( "/" ) ) )
            {
                relativeToProject = relativeToProject.Mid( 1 );
            }
        }

        wxString destPath = aLocalPath + wxFileName::GetPathSeparator() + relativeToProject;
        wxFileName destFn( destPath );
        wxString parentDir = destFn.GetPath();

        if( !parentDir.IsEmpty() && !wxDirExists( parentDir ) )
            wxFileName::Mkdir( parentDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );

        wxFile outFile( destPath, wxFile::write );

        if( !outFile.IsOpened() )
            continue;

        if( !outFile.Write( fileContent ) )
            continue;

        anyExtracted = true;
    }

    return anyExtracted;
}


PLM_REVISION_COMPARE PLM_GIT_PLUGIN::CompareRevisions( const wxString& aProjectId,
                                                        const wxString& aRevision1,
                                                        const wxString& aRevision2 )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_REVISION_COMPARE compare;

    if( !m_isAuthenticated )
        return compare;

    wxString relativePath = FolderIdToPath( aProjectId );
    wxString output;

    int rc = RunGitCommand( { wxS( "diff" ), wxS( "--name-status" ), aRevision1, aRevision2,
                              wxS( "--" ), relativePath },
                            output, m_localClonePath );

    if( rc != 0 )
        return compare;

    compare.base_revision = GetRevision( aProjectId, aRevision1 );
    compare.compare_revision = GetRevision( aProjectId, aRevision2 );

    wxStringTokenizer lineTokenizer( output, wxS( "\n" ) );

    while( lineTokenizer.HasMoreTokens() )
    {
        wxString line = lineTokenizer.GetNextToken().Trim();

        if( line.IsEmpty() )
            continue;

        wxStringTokenizer tabTokenizer( line, wxS( "\t" ) );

        if( tabTokenizer.CountTokens() < 2 )
            continue;

        wxString statusCode = tabTokenizer.GetNextToken();
        wxString filePath = tabTokenizer.GetNextToken();

        PLM_CHANGE_TYPE changeType = PLM_CHANGE_TYPE::MODIFIED;

        if( statusCode == wxS( "A" ) )
            changeType = PLM_CHANGE_TYPE::ADDED;
        else if( statusCode == wxS( "D" ) )
            changeType = PLM_CHANGE_TYPE::REMOVED;
        else if( statusCode.StartsWith( wxS( "R" ) ) || statusCode.StartsWith( wxS( "C" ) ) )
            changeType = PLM_CHANGE_TYPE::MOVED;

        wxFileName fn( filePath );
        wxString ext = fn.GetExt();
        wxString elementType;

        if( ext == wxS( "kicad_sch" ) )
            elementType = wxS( "schematic" );
        else if( ext == wxS( "kicad_pcb" ) )
            elementType = wxS( "pcb" );
        else
            elementType = wxS( "file" );

        wxString changeLabel;

        if( changeType == PLM_CHANGE_TYPE::ADDED )
            changeLabel = wxS( "Added" );
        else if( changeType == PLM_CHANGE_TYPE::REMOVED )
            changeLabel = wxS( "Removed" );
        else if( changeType == PLM_CHANGE_TYPE::MOVED )
            changeLabel = wxS( "Renamed" );
        else
            changeLabel = wxS( "Modified" );

        wxString description = wxString::Format( wxS( "%s %s file %s" ), changeLabel,
                                                 elementType, fn.GetFullName() );

        compare.changes.emplace_back( changeType, elementType, fn.GetFullName(), description );

        // Drill into modified KiCad files for component-level changes. Added
        // and removed files are already represented above; running the
        // analyzer against a missing side would produce duplicate noise.
        if( changeType != PLM_CHANGE_TYPE::MODIFIED )
            continue;

        if( ext != wxS( "kicad_sch" ) && ext != wxS( "kicad_pcb" ) )
            continue;

        wxString basePath = ExtractRevisionToTemp( aRevision1, filePath );
        wxString newPath  = ExtractRevisionToTemp( aRevision2, filePath );

        if( !basePath.IsEmpty() && !newPath.IsEmpty() )
        {
            PLM_DIFF_ANALYZER analyzer;
            PLM_DIFF_ANALYSIS_RESULT analysis;

            if( ext == wxS( "kicad_sch" ) )
            {
                analyzer.SetBaseSchematic( basePath );
                analyzer.SetNewSchematic( newPath );
                analysis = analyzer.AnalyzeSchematic();
            }
            else
            {
                analyzer.SetBasePCB( basePath );
                analyzer.SetNewPCB( newPath );
                analysis = analyzer.AnalyzePCB();
            }

            AppendComponentChanges( analysis, filePath, compare.changes );
        }

        if( !basePath.IsEmpty() )
            wxRemoveFile( basePath );

        if( !newPath.IsEmpty() )
            wxRemoveFile( newPath );
    }

    return compare;
}


bool PLM_GIT_PLUGIN::RestoreRevision( const wxString& aProjectId, const wxString& aRevisionId,
                                       const wxString& aComment )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated )
        return false;

    wxString relativePath = FolderIdToPath( aProjectId );
    wxString output;

    // Refuse to restore if the working tree has uncommitted changes to this path
    wxString statusOutput;
    int statusRc = RunGitCommand( { wxS( "status" ), wxS( "--porcelain" ), wxS( "--" ),
                                    relativePath },
                                  statusOutput, m_localClonePath );

    if( statusRc == 0 && !statusOutput.Trim().IsEmpty() )
        return false;

    int rc = RunGitCommand( { wxS( "checkout" ), aRevisionId, wxS( "--" ), relativePath }, output,
                            m_localClonePath );

    if( rc != 0 )
        return false;

    wxString commitMsg = aComment.IsEmpty() ? wxString::Format( wxS( "Restored to %s" ), aRevisionId )
                                             : aComment;

    rc = RunGitCommand( { wxS( "commit" ), wxS( "-m" ), commitMsg }, output, m_localClonePath );

    return rc == 0;
}


PLM_LOCK_RESULT PLM_GIT_PLUGIN::Lock( const PLM_LOCK_REQUEST& aRequest )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated )
    {
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED, aRequest.project_id, false,
                          wxS( "Not authenticated" ) );
        return PLM_LOCK_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                         wxS( "Not authenticated" ) );
    }

    if( aRequest.project_id.IsEmpty() )
    {
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED, wxEmptyString, false,
                          wxS( "Project ID is required" ) );
        return PLM_LOCK_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                         wxS( "Project ID is required" ) );
    }

    wxString relativePath = FolderIdToPath( aRequest.project_id );

    // Check for existing lock first
    PLM_LOCK_INFO existingInfo = GetLockInfo( aRequest.project_id );

    if( existingInfo.is_locked && !existingInfo.locked_by_me && !aRequest.force )
    {
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_DENIED, aRequest.project_id, false,
                          wxString::Format( wxS( "Locked by %s" ), existingInfo.lock_owner ) );
        return PLM_LOCK_RESULT::Conflict( existingInfo );
    }

    // Try Git LFS lock
    wxString output;
    std::vector<wxString> args = { wxS( "lfs" ), wxS( "lock" ), relativePath };
    int rc = RunGitCommand( args, output, m_localClonePath );

    if( rc == 0 )
    {
        PLM_LOCK_INFO info = PLM_LOCK_INFO::Locked( m_username, wxDateTime::Now().GetTicks(),
                                                     true, PLM_LOCK_TYPE::MANDATORY );
        info.lock_comment = aRequest.comment;
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED, aRequest.project_id, true,
                          aRequest.comment );
        return PLM_LOCK_RESULT::Success( info );
    }

    // Fall back to advisory lock file
    wxString sep = wxString( wxFileName::GetPathSeparator() );
    wxString lockFilePath = m_localClonePath + sep + relativePath;

    if( wxDirExists( lockFilePath ) )
        lockFilePath += sep + wxS( ".kicad_lock" );
    else
        lockFilePath += wxS( ".kicad_lock" );

    if( wxFileExists( lockFilePath ) && !aRequest.force )
    {
        // Read existing lock file to check owner
        wxFile lockFile( lockFilePath, wxFile::read );

        if( lockFile.IsOpened() )
        {
            wxString content;
            lockFile.ReadAll( &content );
            lockFile.Close();

            try
            {
                nlohmann::json lockData = nlohmann::json::parse( content.ToStdString() );
                wxString owner = wxString::FromUTF8( lockData.value( "owner", "" ) );

                if( !owner.IsEmpty() && owner != m_username )
                {
                    int64_t ts = lockData.value( "timestamp", static_cast<int64_t>( 0 ) );
                    PLM_LOCK_INFO conflictInfo =
                            PLM_LOCK_INFO::Locked( owner, ts, false, PLM_LOCK_TYPE::ADVISORY );
                    RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_DENIED, aRequest.project_id, false,
                                      wxString::Format( wxS( "Locked by %s" ), owner ) );
                    return PLM_LOCK_RESULT::Conflict( conflictInfo );
                }
            }
            catch( const nlohmann::json::exception& )
            {
                // Malformed lock file; overwrite it
            }
        }
    }

    // Snapshot any pre-existing lock file (force-lock, same-owner refresh,
    // or malformed-lock overwrite paths reach this write). On publish
    // failure we restore it; otherwise rollback would lose the previous
    // owner's lock.
    bool     hadExistingLock = wxFileExists( lockFilePath );
    wxString priorContents;

    if( hadExistingLock )
    {
        wxFile snapshot( lockFilePath, wxFile::read );
        bool   readOk = false;

        if( snapshot.IsOpened() )
        {
            readOk = snapshot.ReadAll( &priorContents );
            snapshot.Close();
        }

        if( !readOk )
        {
            wxString detail = wxS( "Could not snapshot existing lock for rollback" );
            RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED, aRequest.project_id, false,
                              detail );
            return PLM_LOCK_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL, detail );
        }
    }

    // Write advisory lock file
    nlohmann::json lockData;
    lockData["owner"] = m_username.ToStdString();
    lockData["timestamp"] = static_cast<int64_t>( std::time( nullptr ) );
    lockData["comment"] = aRequest.comment.ToStdString();

    wxFile lockFile( lockFilePath, wxFile::write );

    if( !lockFile.IsOpened() )
    {
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED, aRequest.project_id, false,
                          wxS( "Failed to create lock file" ) );
        return PLM_LOCK_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                         wxS( "Failed to create lock file" ) );
    }

    wxString jsonStr = wxString::FromUTF8( lockData.dump( 2 ) );
    lockFile.Write( jsonStr );
    lockFile.Close();

    // Stage and commit the lock file. Without the commit peers cannot
    // observe the advisory lock via fetch, so we treat git failure as a
    // real lock failure and roll back both the working tree and the index
    // (otherwise the next user commit would publish the failed lock).
    wxString lockPathRel = relativePath + wxS( ".kicad_lock" );

    if( wxDirExists( m_localClonePath + sep + relativePath ) )
        lockPathRel = relativePath + sep + wxS( ".kicad_lock" );

    wxString commitOutput;
    int addRc = RunGitCommand( { wxS( "add" ), lockFilePath }, commitOutput, m_localClonePath );

    wxString commitMsg = wxS( "Lock: " ) + relativePath;

    if( !aRequest.comment.IsEmpty() )
        commitMsg += wxS( " - " ) + aRequest.comment;

    int commitRc = 0;

    if( addRc == 0 )
    {
        commitRc = RunGitCommand( { wxS( "commit" ), wxS( "-m" ), commitMsg }, commitOutput,
                                  m_localClonePath );
    }

    if( addRc != 0 || commitRc != 0 )
    {
        // Unstage before touching the working tree to keep the index in
        // sync with the on-disk state.
        RunGitCommand( { wxS( "reset" ), wxS( "HEAD" ), wxS( "--" ), lockPathRel }, commitOutput,
                       m_localClonePath );

        if( hadExistingLock )
        {
            wxFile restore( lockFilePath, wxFile::write );

            if( restore.IsOpened() )
            {
                restore.Write( priorContents );
                restore.Close();
            }
        }
        else
        {
            wxRemoveFile( lockFilePath );
        }

        wxString detail = wxS( "Failed to publish advisory lock to repository" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED, aRequest.project_id, false, detail );
        return PLM_LOCK_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL, detail );
    }

    PLM_LOCK_INFO info = PLM_LOCK_INFO::Locked( m_username, std::time( nullptr ),
                                                 true, PLM_LOCK_TYPE::ADVISORY );
    info.lock_comment = aRequest.comment;
    RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED, aRequest.project_id, true,
                      aRequest.comment );
    return PLM_LOCK_RESULT::Success( info );
}


PLM_LOCK_RESULT PLM_GIT_PLUGIN::Unlock( const PLM_UNLOCK_REQUEST& aRequest )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated )
    {
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_RELEASED, aRequest.project_id, false,
                          wxS( "Not authenticated" ) );
        return PLM_LOCK_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                         wxS( "Not authenticated" ) );
    }

    if( aRequest.project_id.IsEmpty() )
    {
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_RELEASED, wxEmptyString, false,
                          wxS( "Project ID is required" ) );
        return PLM_LOCK_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                         wxS( "Project ID is required" ) );
    }

    wxString relativePath = FolderIdToPath( aRequest.project_id );

    // Try Git LFS unlock
    wxString output;
    std::vector<wxString> args = { wxS( "lfs" ), wxS( "unlock" ), relativePath };

    if( aRequest.force )
        args.push_back( wxS( "--force" ) );

    int rc = RunGitCommand( args, output, m_localClonePath );

    if( rc == 0 )
    {
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_RELEASED, aRequest.project_id, true );
        return PLM_LOCK_RESULT::Success( PLM_LOCK_INFO::Unlocked() );
    }

    // Fall back to advisory lock file removal
    wxString sep = wxString( wxFileName::GetPathSeparator() );
    wxString lockFilePath = m_localClonePath + sep + relativePath;

    if( wxDirExists( lockFilePath ) )
        lockFilePath += sep + wxS( ".kicad_lock" );
    else
        lockFilePath += wxS( ".kicad_lock" );

    if( !wxFileExists( lockFilePath ) )
        return PLM_LOCK_RESULT::Success( PLM_LOCK_INFO::Unlocked() );

    // Verify ownership unless force
    if( !aRequest.force )
    {
        wxFile lockFile( lockFilePath, wxFile::read );

        if( lockFile.IsOpened() )
        {
            wxString content;
            lockFile.ReadAll( &content );
            lockFile.Close();

            try
            {
                nlohmann::json lockData = nlohmann::json::parse( content.ToStdString() );
                wxString owner = wxString::FromUTF8( lockData.value( "owner", "" ) );

                if( !owner.IsEmpty() && owner != m_username )
                {
                    int64_t ts = lockData.value( "timestamp", static_cast<int64_t>( 0 ) );
                    PLM_LOCK_INFO conflictInfo =
                            PLM_LOCK_INFO::Locked( owner, ts, false, PLM_LOCK_TYPE::ADVISORY );
                    RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_DENIED, aRequest.project_id, false,
                                      wxString::Format( wxS( "Held by %s" ), owner ) );
                    return PLM_LOCK_RESULT::Conflict( conflictInfo );
                }
            }
            catch( const nlohmann::json::exception& )
            {
                // Malformed lock file; remove it anyway
            }
        }
    }

    // Snapshot the lock file before removal so the unlock can roll back if
    // the git commit fails to publish the deletion. Both opening AND fully
    // reading the file must succeed before any mutation; otherwise the
    // rollback restore could write back partial or empty bytes.
    wxString lockBackup;
    bool     hasSnapshot = false;

    {
        wxFile snapshot( lockFilePath, wxFile::read );

        if( snapshot.IsOpened() )
        {
            hasSnapshot = snapshot.ReadAll( &lockBackup );
            snapshot.Close();
        }
    }

    if( !hasSnapshot )
    {
        wxString detail = wxS( "Could not snapshot existing lock for rollback" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_RELEASED, aRequest.project_id, false, detail );
        return PLM_LOCK_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL, detail );
    }

    if( !wxRemoveFile( lockFilePath ) )
    {
        wxString detail = wxS( "Failed to remove lock file" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_RELEASED, aRequest.project_id, false, detail );
        return PLM_LOCK_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL, detail );
    }

    // The git commit propagates the unlock to peers. If it fails, unstage
    // the deletion and restore the lock file so other users still see the
    // lock and the unlock can be retried.
    wxString lockPathRel = relativePath + wxS( ".kicad_lock" );

    if( wxDirExists( m_localClonePath + sep + relativePath ) )
        lockPathRel = relativePath + sep + wxS( ".kicad_lock" );

    wxString commitOutput;
    int addRc = RunGitCommand( { wxS( "add" ), lockFilePath }, commitOutput, m_localClonePath );
    int commitRc = 0;

    if( addRc == 0 )
    {
        commitRc = RunGitCommand( { wxS( "commit" ), wxS( "-m" ),
                                    wxS( "Unlock: " ) + relativePath },
                                  commitOutput, m_localClonePath );
    }

    if( addRc != 0 || commitRc != 0 )
    {
        RunGitCommand( { wxS( "reset" ), wxS( "HEAD" ), wxS( "--" ), lockPathRel }, commitOutput,
                       m_localClonePath );

        wxFile restore( lockFilePath, wxFile::write );

        if( restore.IsOpened() )
        {
            restore.Write( lockBackup );
            restore.Close();
        }

        wxString detail = wxS( "Failed to publish unlock to repository" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_RELEASED, aRequest.project_id, false, detail );
        return PLM_LOCK_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL, detail );
    }

    RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_RELEASED, aRequest.project_id, true );
    return PLM_LOCK_RESULT::Success( PLM_LOCK_INFO::Unlocked() );
}


PLM_LOCK_INFO PLM_GIT_PLUGIN::GetLockInfo( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || aProjectId.IsEmpty() )
        return PLM_LOCK_INFO::Unlocked();

    wxString relativePath = FolderIdToPath( aProjectId );

    // Try Git LFS locks query
    wxString output;
    int rc = RunGitCommand( { wxS( "lfs" ), wxS( "locks" ), wxS( "--path=" ) + relativePath },
                            output, m_localClonePath );

    if( rc == 0 && !output.Trim().IsEmpty() )
    {
        // Output format: <path>\t<owner>\tID:<id>
        wxStringTokenizer lineTokenizer( output, wxS( "\n" ) );

        while( lineTokenizer.HasMoreTokens() )
        {
            wxString line = lineTokenizer.GetNextToken().Trim();

            if( line.IsEmpty() )
                continue;

            wxStringTokenizer fieldTokenizer( line, wxS( "\t" ) );

            if( fieldTokenizer.CountTokens() >= 2 )
            {
                wxString path = fieldTokenizer.GetNextToken().Trim();
                wxString owner = fieldTokenizer.GetNextToken().Trim();

                bool byMe = ( owner == m_username );

                return PLM_LOCK_INFO::Locked( owner, 0, byMe, PLM_LOCK_TYPE::MANDATORY );
            }
        }
    }

    // Fall back to advisory lock file check
    wxString sep = wxString( wxFileName::GetPathSeparator() );
    wxString lockFilePath = m_localClonePath + sep + relativePath;

    if( wxDirExists( lockFilePath ) )
        lockFilePath += sep + wxS( ".kicad_lock" );
    else
        lockFilePath += wxS( ".kicad_lock" );

    if( !wxFileExists( lockFilePath ) )
        return PLM_LOCK_INFO::Unlocked();

    wxFile lockFile( lockFilePath, wxFile::read );

    if( !lockFile.IsOpened() )
        return PLM_LOCK_INFO::Unlocked();

    wxString content;
    lockFile.ReadAll( &content );
    lockFile.Close();

    try
    {
        nlohmann::json lockData = nlohmann::json::parse( content.ToStdString() );
        wxString owner = wxString::FromUTF8( lockData.value( "owner", "" ) );
        int64_t ts = lockData.value( "timestamp", static_cast<int64_t>( 0 ) );
        wxString comment = wxString::FromUTF8( lockData.value( "comment", "" ) );
        bool byMe = ( owner == m_username );

        PLM_LOCK_INFO info =
                PLM_LOCK_INFO::Locked( owner, ts, byMe, PLM_LOCK_TYPE::ADVISORY, comment );
        return info;
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_LOCK_INFO::Unlocked();
    }
}


PLM_SCHEMA PLM_GIT_PLUGIN::GetSchema()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );
    return PLM_SCHEMA();
}


PLM_ATTRIBUTE_SET PLM_GIT_PLUGIN::GetAttributes( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_ATTRIBUTE_SET attrs;

    if( !m_isAuthenticated || aProjectId.IsEmpty() )
        return attrs;

    if( !EnsureClone() )
        return attrs;

    wxString sep = wxString( wxFileName::GetPathSeparator() );
    wxString relativePath = FolderIdToPath( aProjectId );
    wxString fullPath = m_localClonePath + sep + relativePath;

    // aProjectId may point at a .kicad_pro file rather than a directory.
    wxFileName fn( fullPath );

    if( fn.GetExt() == wxS( "kicad_pro" ) )
        fullPath = fn.GetPath();

    wxString attrPath = fullPath + sep + wxS( "attributes.json" );

    if( !wxFileExists( attrPath ) )
        return attrs;

    wxFile file( attrPath, wxFile::read );

    if( !file.IsOpened() )
        return attrs;

    wxString content;

    if( !file.ReadAll( &content ) )
        return attrs;

    file.Close();

    try
    {
        nlohmann::json doc = nlohmann::json::parse( content.ToStdString() );

        if( !doc.contains( "attributes" ) || !doc["attributes"].is_array() )
            return attrs;

        for( const auto& entry : doc["attributes"] )
        {
            if( !entry.contains( "id" ) || !entry["id"].is_string() )
                continue;

            wxString attrId = wxString::FromUTF8( entry["id"].get<std::string>() );
            wxString value;

            if( entry.contains( "value" ) && entry["value"].is_string() )
                value = wxString::FromUTF8( entry["value"].get<std::string>() );

            attrs.values.emplace_back( attrId, value );
        }
    }
    catch( const nlohmann::json::exception& )
    {
        // Malformed attributes.json -- treat as empty rather than failing.
    }

    return attrs;
}


bool PLM_GIT_PLUGIN::SetAttributes( const wxString&          aProjectId,
                                     const PLM_ATTRIBUTE_SET& aAttributes )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || aProjectId.IsEmpty() )
        return false;

    if( !EnsureClone() )
        return false;

    wxString sep = wxString( wxFileName::GetPathSeparator() );
    wxString relativePath = FolderIdToPath( aProjectId );
    wxString projectDir = m_localClonePath + sep + relativePath;

    // aProjectId may point at a .kicad_pro file rather than a directory.
    wxFileName fn( projectDir );

    if( fn.GetExt() == wxS( "kicad_pro" ) )
        projectDir = fn.GetPath();

    if( !wxDirExists( projectDir ) )
        return false;

    nlohmann::json doc;
    nlohmann::json arr = nlohmann::json::array();

    for( const PLM_ATTRIBUTE_VALUE& attr : aAttributes.values )
    {
        nlohmann::json entry;
        entry["id"] = attr.attr_id.ToStdString();
        entry["value"] = attr.value.ToStdString();
        arr.push_back( entry );
    }

    doc["attributes"] = arr;

    wxString attrPath = projectDir + sep + wxS( "attributes.json" );
    wxFile file( attrPath, wxFile::write );

    if( !file.IsOpened() )
        return false;

    wxString json = wxString::FromUTF8( doc.dump( 2 ) );

    if( !file.Write( json ) )
    {
        file.Close();
        return false;
    }

    file.Close();

    // Stage and commit so peers see the new attributes after fetch.
    wxString output;
    int addRc = RunGitCommand( { wxS( "add" ), attrPath }, output, m_localClonePath );

    if( addRc != 0 )
        return false;

    int commitRc = RunGitCommand( { wxS( "commit" ), wxS( "-m" ),
                                    wxS( "PLM: update attributes for " ) + aProjectId },
                                  output, m_localClonePath );

    bool nothingToCommit = ( commitRc != 0 && output.Contains( wxS( "nothing to commit" ) ) );

    if( commitRc != 0 && !nothingToCommit )
        return false;

    // Push to remote so peers see the new attributes after fetch. Local-only
    // repos still have m_repositoryUrl set (to the on-disk path) but have no
    // configured remote, so probe the actual git config instead. Skip the
    // push when the commit was a no-op. On push failure, undo the
    // just-created commit so a retry can re-publish cleanly.
    if( !nothingToCommit )
    {
        wxString remoteUrl;
        int      remoteRc = RunGitCommand( { wxS( "config" ), wxS( "--get" ),
                                             wxS( "remote.origin.url" ) },
                                           remoteUrl, m_localClonePath );

        if( remoteRc == 0 && !remoteUrl.Trim().IsEmpty() )
        {
            int pushRc = RunGitCommand( { wxS( "push" ) }, output, m_localClonePath );

            if( pushRc != 0 )
            {
                RunGitCommand( { wxS( "reset" ), wxS( "--soft" ), wxS( "HEAD~1" ) }, output,
                               m_localClonePath );
                return false;
            }
        }
    }

    return true;
}


PLM_PERMISSIONS PLM_GIT_PLUGIN::QueryPermissions( const wxString& aProjectId )
{
    if( m_detectedRole == wxS( "admin" ) )
        return PLM_PERMISSIONS::FullAccess();

    if( m_detectedRole == wxS( "write" ) || m_detectedRole == wxS( "push" ) )
        return PLM_PERMISSIONS::StandardEdit();

    if( m_detectedRole == wxS( "read" ) || m_detectedRole == wxS( "pull" ) )
        return PLM_PERMISSIONS::ReadOnly();

    // Fallback for unknown roles
    return PLM_PERMISSIONS::ReadOnly();
}


wxString PLM_GIT_PLUGIN::GetUserDisplayName( const wxString& aUserId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    auto it = m_userDisplayNames.find( aUserId );

    if( it != m_userDisplayNames.end() )
        return it->second;

    return aUserId;
}


int PLM_GIT_PLUGIN::RunGitCommand( const std::vector<wxString>& aArgs, wxString& aOutput,
                                    const wxString& aWorkDir )
{
    // Build command string safely. Git subcommands and options are hardcoded,
    // and paths are validated before use, so this is safe from injection.
    wxString cmd = wxS( "git" );

    for( const wxString& arg : aArgs )
    {
        cmd += wxS( " " );

        // Quote arguments that contain spaces or special characters
        if( arg.Contains( wxS( " " ) ) || arg.Contains( wxS( "\"" ) ) ||
            arg.Contains( wxS( "'" ) ) || arg.Contains( wxS( "\\" ) ) )
        {
            wxString escaped = arg;
            escaped.Replace( wxS( "\\" ), wxS( "\\\\" ) );
            escaped.Replace( wxS( "\"" ), wxS( "\\\"" ) );
            cmd += wxS( "\"" ) + escaped + wxS( "\"" );
        }
        else
        {
            cmd += arg;
        }
    }

    wxArrayString output, errors;
    wxString prevDir;

    if( !aWorkDir.IsEmpty() )
    {
        prevDir = wxGetCwd();
        wxSetWorkingDirectory( aWorkDir );
    }

    long result = wxExecute( cmd, output, errors, wxEXEC_SYNC | wxEXEC_NOEVENTS );

    if( !prevDir.IsEmpty() )
        wxSetWorkingDirectory( prevDir );

    aOutput.Clear();

    for( const wxString& line : output )
        aOutput += line + wxS( "\n" );

    return static_cast<int>( result );
}


bool PLM_GIT_PLUGIN::IsGitRepository( const wxString& aPath )
{
    wxString gitDir = aPath + wxFileName::GetPathSeparator() + wxS( ".git" );
    return wxDirExists( gitDir ) || wxFileExists( gitDir );
}


bool PLM_GIT_PLUGIN::CloneOrFetch( const wxString& aUrl, const wxString& aLocalPath )
{
    wxString output;

    if( wxDirExists( aLocalPath ) && IsGitRepository( aLocalPath ) )
    {
        int rc = RunGitCommand( { wxS( "fetch" ), wxS( "--all" ) }, output, aLocalPath );
        return rc == 0;
    }

    int rc = RunGitCommand( { wxS( "clone" ), aUrl, aLocalPath }, output, wxEmptyString );
    return rc == 0;
}


wxString PLM_GIT_PLUGIN::GetRepositoryRoot( const wxString& aPath )
{
    wxString output;
    int rc = RunGitCommand( { wxS( "rev-parse" ), wxS( "--show-toplevel" ) }, output, aPath );

    if( rc == 0 )
        return output.Trim();

    return wxEmptyString;
}


std::vector<PLM_REVISION> PLM_GIT_PLUGIN::ParseGitLog( const wxString& aLogOutput )
{
    std::vector<PLM_REVISION> revisions;

    wxStringTokenizer lineTokenizer( aLogOutput, wxS( "\n" ) );

    while( lineTokenizer.HasMoreTokens() )
    {
        wxString line = lineTokenizer.GetNextToken().Trim();

        if( line.IsEmpty() )
            continue;

        wxStringTokenizer fieldTokenizer( line, wxS( "|" ) );

        if( fieldTokenizer.CountTokens() >= 5 )
        {
            PLM_REVISION rev;
            rev.revision_id = fieldTokenizer.GetNextToken();
            rev.author = fieldTokenizer.GetNextToken();
            wxString email = fieldTokenizer.GetNextToken();
            wxString timestamp = fieldTokenizer.GetNextToken();
            long ts;

            if( timestamp.ToLong( &ts ) )
                rev.timestamp = static_cast<int64_t>( ts );

            rev.comment = fieldTokenizer.GetNextToken();
            rev.state_category = PLM_STATE_CATEGORY::RELEASED;
            revisions.push_back( rev );
        }
    }

    // Git log returns newest first, so number oldest as 1 and newest as N
    for( size_t i = 0; i < revisions.size(); ++i )
        revisions[i].revision_number = wxString::Format( wxS( "%zu" ), revisions.size() - i );

    return revisions;
}


PLM_STATE_CATEGORY PLM_GIT_PLUGIN::GitStatusToPLMState( const wxString& aStatus )
{
    if( aStatus.Contains( wxS( "M" ) ) || aStatus.Contains( wxS( "A" ) ) )
        return PLM_STATE_CATEGORY::EDITABLE;

    return PLM_STATE_CATEGORY::RELEASED;
}


wxString PLM_GIT_PLUGIN::PathToFolderId( const wxString& aPath )
{
    wxString result = aPath;
    result.Replace( wxFileName::GetPathSeparator(), wxS( "/" ) );
    return result;
}


wxString PLM_GIT_PLUGIN::FolderIdToPath( const wxString& aFolderId )
{
    wxString result = aFolderId;
    result.Replace( wxS( "/" ), wxFileName::GetPathSeparator() );
    return result;
}


bool PLM_GIT_PLUGIN::RemoveDirectoryRecursive( const wxString& aPath )
{
    if( !wxDir::Exists( aPath ) )
        return true;

    wxDir dir( aPath );

    if( !dir.IsOpened() )
        return false;

    wxString filename;
    bool cont = dir.GetFirst( &filename );

    while( cont )
    {
        wxString fullPath = aPath + wxFileName::GetPathSeparator() + filename;

        if( wxFileName::DirExists( fullPath ) )
        {
            if( !RemoveDirectoryRecursive( fullPath ) )
                return false;
        }
        else if( wxFileName::FileExists( fullPath ) )
        {
            if( !wxRemoveFile( fullPath ) )
                return false;
        }

        cont = dir.GetNext( &filename );
    }

    return wxRmdir( aPath );
}


void PLM_GIT_PLUGIN::CleanupTempClone()
{
    if( !m_isTempClone || m_localClonePath.IsEmpty() )
        return;

    if( wxDir::Exists( m_localClonePath ) )
        RemoveDirectoryRecursive( m_localClonePath );

    m_localClonePath.Clear();
}


bool PLM_GIT_PLUGIN::EnsureClone()
{
    if( !m_localClonePath.IsEmpty() )
        return true;

    if( m_repositoryUrl.IsEmpty() )
        return false;

    wxString tempDir = wxFileName::GetTempDir() + wxFileName::GetPathSeparator() +
                       wxString::Format( wxS( "kicad_plm_git_%s_%ld_%03d" ),
                                         wxDateTime::Now().Format( wxS( "%Y%m%d%H%M%S" ) ),
                                         static_cast<long>( wxGetProcessId() ),
                                         wxDateTime::Now().GetMillisecond() );

    if( !CloneOrFetch( m_repositoryUrl, tempDir ) )
        return false;

    m_localClonePath = tempDir;
    m_isTempClone = true;
    return true;
}


GIT_AUTH_CONFIG PLM_GIT_PLUGIN::BuildAuthConfig( const PLM_CREDENTIALS& aCredentials )
{
    GIT_AUTH_CONFIG config;

    PLM_AUTH_METHOD method = aCredentials.auth_method;

    if( method == PLM_AUTH_METHOD::AUTO_DETECT )
    {
        GIT_AUTH_METHOD detected = GIT_AUTH_HELPER::DetectAuthMethod( aCredentials.server_url );

        switch( detected )
        {
        case GIT_AUTH_METHOD::HTTPS_TOKEN:
            method = PLM_AUTH_METHOD::TOKEN;
            break;
        case GIT_AUTH_METHOD::SSH_KEY:
            method = PLM_AUTH_METHOD::SSH_KEY;
            break;
        case GIT_AUTH_METHOD::SSH_AGENT:
            method = PLM_AUTH_METHOD::SSH_AGENT;
            break;
        default:
            method = PLM_AUTH_METHOD::PASSWORD;
            break;
        }
    }

    switch( method )
    {
    case PLM_AUTH_METHOD::TOKEN:
        config = GIT_AUTH_CONFIG::HttpsToken( aCredentials.password.Get() );
        break;

    case PLM_AUTH_METHOD::SSH_KEY:
    {
        wxString keyPath = aCredentials.GetProperty( wxS( "ssh_key_path" ),
                                                     GIT_AUTH_HELPER::GetDefaultSshKeyPath() );
        config = GIT_AUTH_CONFIG::SshKey( keyPath, aCredentials.password.Get() );
        break;
    }

    case PLM_AUTH_METHOD::SSH_AGENT:
        config = GIT_AUTH_CONFIG::SshAgent();
        break;

    case PLM_AUTH_METHOD::PASSWORD:
    default:
        config = GIT_AUTH_CONFIG::HttpsBasic( aCredentials.username, aCredentials.password.Get() );
        break;
    }

    config.store_credentials =
            aCredentials.GetProperty( wxS( "store_credentials" ) ) == wxS( "true" );

    return config;
}


//-------------------------------------------------------------------------
// Private Helpers
//-------------------------------------------------------------------------

wxArrayString PLM_GIT_PLUGIN::ParseCSVLine( const wxString& aLine )
{
    wxArrayString fields;
    wxString      current;
    bool          inQuotes = false;

    for( size_t i = 0; i < aLine.length(); ++i )
    {
        wxUniChar c = aLine[i];

        if( c == '"' )
        {
            inQuotes = !inQuotes;
        }
        else if( c == ',' && !inQuotes )
        {
            fields.Add( current );
            current.clear();
        }
        else
        {
            current += c;
        }
    }

    fields.Add( current );
    return fields;
}


bool PLM_GIT_PLUGIN::EnsurePartsCache()
{
    if( m_localClonePath.IsEmpty() )
        return false;

    wxString partsFile = m_localClonePath + wxFileName::GetPathSeparator() + wxS( "parts.csv" );

    if( !wxFileExists( partsFile ) )
        return false;

    time_t fileModTime = wxFileModificationTime( partsFile );

    if( !m_partsCache.empty() && fileModTime == m_partsCacheModTime )
        return true;

    // Force re-read by clearing cache and calling SearchParts
    m_partsCache.clear();
    m_partsCacheModTime = 0;

    PLM_PART_SEARCH_OPTIONS opts;
    opts.include_obsolete = true;
    opts.limit = 100000;
    SearchParts( opts );

    return !m_partsCache.empty() || wxFileExists( partsFile );
}


bool PLM_GIT_PLUGIN::WritePartsCSV()
{
    wxString partsFile = m_localClonePath + wxFileName::GetPathSeparator() + wxS( "parts.csv" );
    wxFile file( partsFile, wxFile::write );

    if( !file.IsOpened() )
        return false;

    file.Write( wxS( "part_number,description,manufacturer,manufacturer_pn,category,"
                      "state,datasheet,symbol,footprint,vendor_name,"
                      "vendor_part_number,price,lead_time\n" ) );

    for( const PLM_PART& part : m_partsCache )
    {
        auto quote = []( const wxString& s ) -> wxString
        {
            if( s.Contains( wxS( "," ) ) || s.Contains( wxS( "\"" ) ) )
            {
                wxString escaped = s;
                escaped.Replace( wxS( "\"" ), wxS( "\"\"" ) );
                return wxS( "\"" ) + escaped + wxS( "\"" );
            }

            return s;
        };

        wxString line;
        line += quote( part.part_number ) + wxS( "," );
        line += quote( part.description ) + wxS( "," );
        line += quote( part.manufacturer ) + wxS( "," );
        line += quote( part.manufacturer_pn ) + wxS( "," );
        line += quote( part.category ) + wxS( "," );
        line += PartStateToString( part.state ) + wxS( "," );
        line += quote( part.datasheet_url ) + wxS( "," );
        line += quote( part.symbol_ref ) + wxS( "," );
        line += quote( part.footprint_ref ) + wxS( "," );
        auto attrVal = [&]( const wxString& aId ) -> wxString
        {
            const PLM_ATTRIBUTE_VALUE* v = part.attributes.FindValue( aId );
            return v ? v->value : wxString();
        };

        line += quote( attrVal( wxS( "vendor_name" ) ) ) + wxS( "," );
        line += quote( attrVal( wxS( "vendor_part_number" ) ) ) + wxS( "," );
        line += quote( attrVal( wxS( "price" ) ) ) + wxS( "," );
        line += quote( attrVal( wxS( "lead_time" ) ) );
        line += wxS( "\n" );
        file.Write( line );
    }

    file.Close();
    m_partsCacheModTime = wxFileModificationTime( partsFile );
    return true;
}


wxString PLM_GIT_PLUGIN::PartStateToString( PLM_PART_STATE aState )
{
    switch( aState )
    {
    case PLM_PART_STATE::ACTIVE:          return wxS( "active" );
    case PLM_PART_STATE::PREFERRED:       return wxS( "preferred" );
    case PLM_PART_STATE::NOT_RECOMMENDED: return wxS( "not_recommended" );
    case PLM_PART_STATE::OBSOLETE:        return wxS( "obsolete" );
    case PLM_PART_STATE::END_OF_LIFE:     return wxS( "eol" );
    case PLM_PART_STATE::DISCONTINUED:    return wxS( "discontinued" );
    case PLM_PART_STATE::PENDING:         return wxS( "pending" );
    case PLM_PART_STATE::DRAFT:           return wxS( "draft" );
    default:                              return wxS( "active" );
    }
}


wxString PLM_GIT_PLUGIN::BOMTypeToSuffix( PLM_BOM_TYPE aType )
{
    switch( aType )
    {
    case PLM_BOM_TYPE::ENGINEERING:    return wxS( "engineering" );
    case PLM_BOM_TYPE::MANUFACTURING:  return wxS( "manufacturing" );
    case PLM_BOM_TYPE::SPARE_PARTS:    return wxS( "spare_parts" );
    case PLM_BOM_TYPE::PHANTOM:        return wxS( "phantom" );
    default:                           return wxS( "engineering" );
    }
}


PLM_BOM_TYPE PLM_GIT_PLUGIN::SuffixToBOMType( const wxString& aSuffix )
{
    if( aSuffix == wxS( "manufacturing" ) )  return PLM_BOM_TYPE::MANUFACTURING;
    if( aSuffix == wxS( "spare_parts" ) )    return PLM_BOM_TYPE::SPARE_PARTS;
    if( aSuffix == wxS( "phantom" ) )        return PLM_BOM_TYPE::PHANTOM;
    return PLM_BOM_TYPE::ENGINEERING;
}


wxString PLM_GIT_PLUGIN::GetProjectPath( const wxString& aProductId ) const
{
    wxString relativePath = const_cast<PLM_GIT_PLUGIN*>( this )->FolderIdToPath( aProductId );
    wxString fullPath = m_localClonePath + wxFileName::GetPathSeparator() + relativePath;

    // The ID might point to a .kicad_pro file; we want the directory
    wxFileName fn( fullPath );

    if( fn.GetExt() == wxS( "kicad_pro" ) )
        return fn.GetPath();

    return fullPath;
}


wxString PLM_GIT_PLUGIN::BuildBOMPath( const wxString& aProductId, PLM_BOM_TYPE aType ) const
{
    wxString projectDir = GetProjectPath( aProductId );
    wxString suffix = BOMTypeToSuffix( aType );
    return projectDir + wxFileName::GetPathSeparator() + wxS( "bom_" ) + suffix + wxS( ".csv" );
}


PLM_BOM PLM_GIT_PLUGIN::ParseBOMFile( const wxString& aFilePath,
                                        const wxString& aProductId ) const
{
    PLM_BOM bom;
    bom.product_id = aProductId;

    wxFileName fn( aFilePath );
    wxString baseName = fn.GetName();

    if( baseName.StartsWith( wxS( "bom_" ) ) )
    {
        wxString suffix = baseName.Mid( 4 );
        bom.type = SuffixToBOMType( suffix );
    }

    bom.bom_id = aProductId + wxS( "/" ) + fn.GetFullName();

    wxFileInputStream input( aFilePath );

    if( !input.IsOk() )
        return bom;

    wxTextInputStream text( input );
    bool headerParsed = false;
    std::map<wxString, int> columnIndex;
    int lineNum = 0;

    while( !input.Eof() )
    {
        wxString line = text.ReadLine();

        if( line.IsEmpty() )
            continue;

        wxArrayString fields = ParseCSVLine( line );

        if( !headerParsed )
        {
            for( size_t i = 0; i < fields.size(); ++i )
                columnIndex[fields[i].Lower()] = static_cast<int>( i );

            headerParsed = true;
            continue;
        }

        auto getField = [&]( const wxString& name ) -> wxString
        {
            auto it = columnIndex.find( name.Lower() );

            if( it != columnIndex.end() && it->second < static_cast<int>( fields.size() ) )
                return fields[it->second];

            return wxEmptyString;
        };

        PLM_BOM_LINE bomLine;
        bomLine.line_id = wxString::Format( wxS( "%d" ), ++lineNum );
        bomLine.part_number = getField( wxS( "part_number" ) );
        bomLine.reference = getField( wxS( "reference" ) );
        bomLine.value = getField( wxS( "value" ) );
        bomLine.description = getField( wxS( "description" ) );
        bomLine.footprint = getField( wxS( "footprint" ) );

        wxString qty = getField( wxS( "quantity" ) );

        if( !qty.IsEmpty() )
        {
            double q;

            if( qty.ToDouble( &q ) )
                bomLine.quantity = q;
        }

        bomLine.uom = getField( wxS( "uom" ) );
        bomLine.dnp = getField( wxS( "dnp" ) ).Lower() == wxS( "true" )
                      || getField( wxS( "dnp" ) ) == wxS( "1" );

        bom.lines.push_back( bomLine );
    }

    return bom;
}


bool PLM_GIT_PLUGIN::WriteBOMFile( const wxString& aFilePath, const PLM_BOM& aBom ) const
{
    wxFile file( aFilePath, wxFile::write );

    if( !file.IsOpened() )
        return false;

    file.Write( wxS( "part_number,reference,value,description,footprint,quantity,uom,dnp\n" ) );

    auto quote = []( const wxString& s ) -> wxString
    {
        if( s.Contains( wxS( "," ) ) || s.Contains( wxS( "\"" ) ) )
        {
            wxString escaped = s;
            escaped.Replace( wxS( "\"" ), wxS( "\"\"" ) );
            return wxS( "\"" ) + escaped + wxS( "\"" );
        }

        return s;
    };

    for( const PLM_BOM_LINE& line : aBom.lines )
    {
        wxString row;
        row += quote( line.part_number ) + wxS( "," );
        row += quote( line.reference ) + wxS( "," );
        row += quote( line.value ) + wxS( "," );
        row += quote( line.description ) + wxS( "," );
        row += quote( line.footprint ) + wxS( "," );
        row += wxString::Format( wxS( "%g" ), line.quantity ) + wxS( "," );
        row += quote( line.uom ) + wxS( "," );
        row += line.dnp ? wxS( "true" ) : wxS( "false" );
        row += wxS( "\n" );
        file.Write( row );
    }

    file.Close();
    return true;
}


bool PLM_GIT_PLUGIN::ReadJsonFile( const wxString& aPath, nlohmann::json& aOut )
{
    if( !wxFileExists( aPath ) )
        return false;

    wxFile file( aPath, wxFile::read );

    if( !file.IsOpened() )
        return false;

    wxString content;
    file.ReadAll( &content );
    file.Close();

    try
    {
        aOut = nlohmann::json::parse( content.ToStdString() );
        return true;
    }
    catch( const nlohmann::json::exception& )
    {
        return false;
    }
}


bool PLM_GIT_PLUGIN::WriteJsonFile( const wxString& aPath, const nlohmann::json& aData )
{
    wxFile file( aPath, wxFile::write );

    if( !file.IsOpened() )
        return false;

    wxString content = wxString::FromUTF8( aData.dump( 2 ) );
    file.Write( content );
    file.Close();
    return true;
}


//-------------------------------------------------------------------------
// ECO Extended Interface
//-------------------------------------------------------------------------

PLM_ECO_CREATE_RESULT PLM_GIT_PLUGIN::CreateECO( const PLM_ECO_CREATE_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_forgeProvider )
    {
        return PLM_ECO_CREATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                                wxS( "ECO requires a forge provider" ) );
    }

    wxString label = aOptions.type == PLM_ECO_TYPE::ECR ? wxS( "ecr" ) : wxS( "eco" );

    wxString body;
    body += wxS( "## Description\n" ) + aOptions.description + wxS( "\n\n" );
    body += wxS( "## Metadata\n" );
    body += wxS( "- Type: " ) + label + wxS( "\n" );
    body += wxString::Format( wxS( "- Priority: %d\n" ),
                              static_cast<int>( aOptions.priority ) );
    body += wxString::Format( wxS( "- Reason: %d\n" ), static_cast<int>( aOptions.reason ) );

    if( !aOptions.reason_detail.IsEmpty() )
        body += wxS( "- Reason detail: " ) + aOptions.reason_detail + wxS( "\n" );

    if( !aOptions.affected_item_ids.empty() )
    {
        body += wxS( "\n## Affected Items\n" );

        for( const wxString& item : aOptions.affected_item_ids )
            body += wxS( "- " ) + item + wxS( "\n" );
    }

    if( !aOptions.impact.IsEmpty() )
    {
        body += wxS( "\n## Impact Assessment\n" );

        if( !aOptions.impact.cost_impact.IsEmpty() )
            body += wxS( "- Cost: " ) + aOptions.impact.cost_impact + wxS( "\n" );

        if( !aOptions.impact.schedule_impact.IsEmpty() )
            body += wxS( "- Schedule: " ) + aOptions.impact.schedule_impact + wxS( "\n" );

        if( !aOptions.impact.manufacturing_impact.IsEmpty() )
            body += wxS( "- Manufacturing: " ) + aOptions.impact.manufacturing_impact + wxS( "\n" );
    }

    // Use forge to create an issue -- the forge CreatePullRequest is for reviews,
    // so we create a PR with a special label as a workaround, or we use the forge
    // to create a labeled PR. Since the forge only has PR operations and no issue
    // API, we create a draft PR to serve as the ECO tracking item.
    PLM_REVIEW_RESULT prResult = m_forgeProvider->CreatePullRequest(
            m_currentBranch.IsEmpty() ? wxS( "main" ) : m_currentBranch,
            wxS( "main" ), aOptions.title, body, {}, true );

    if( !prResult.success )
    {
        return PLM_ECO_CREATE_RESULT::Failure( prResult.error_code, prResult.error_message );
    }

    PLM_ECO eco;
    eco.eco_id = prResult.review.review_id;
    eco.eco_number = wxS( "ECO-" ) + prResult.review.review_id;
    eco.title = aOptions.title;
    eco.description = aOptions.description;
    eco.status = PLM_ECO_STATUS::DRAFT;
    eco.type = aOptions.type;
    eco.reason = aOptions.reason;
    eco.reason_detail = aOptions.reason_detail;
    eco.impact = aOptions.impact;
    eco.priority = wxString::Format( wxS( "%d" ), static_cast<int>( aOptions.priority ) );
    eco.owner_id = m_username;
    eco.owner_name = m_username;
    eco.created_at = static_cast<int64_t>( std::time( nullptr ) );

    return PLM_ECO_CREATE_RESULT::Success( eco );
}


PLM_ECO_UPDATE_RESULT PLM_GIT_PLUGIN::GetECO( const wxString& aEcoId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_forgeProvider )
    {
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                                wxS( "ECO requires a forge provider" ) );
    }

    PLM_REVIEW pr = m_forgeProvider->GetPullRequest( aEcoId );

    if( pr.review_id.IsEmpty() )
    {
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_FILE_NOT_FOUND,
                                                wxS( "ECO not found" ) );
    }

    PLM_ECO eco;
    eco.eco_id = pr.review_id;
    eco.eco_number = wxS( "ECO-" ) + pr.review_id;
    eco.title = pr.title;
    eco.description = pr.description;
    eco.owner_id = pr.submitter_id;
    eco.owner_name = pr.submitter_name;
    eco.created_at = pr.created_at;
    eco.updated_at = pr.updated_at;

    if( pr.status == PLM_REVIEW_STATUS::MERGED )
        eco.status = PLM_ECO_STATUS::COMPLETED;
    else if( pr.status == PLM_REVIEW_STATUS::CLOSED )
        eco.status = PLM_ECO_STATUS::CANCELLED;
    else if( pr.status == PLM_REVIEW_STATUS::APPROVED )
        eco.status = PLM_ECO_STATUS::APPROVED;
    else if( pr.status == PLM_REVIEW_STATUS::OPEN )
        eco.status = PLM_ECO_STATUS::IN_REVIEW;
    else
        eco.status = PLM_ECO_STATUS::DRAFT;

    return PLM_ECO_UPDATE_RESULT::Success( eco );
}


PLM_ECO_UPDATE_RESULT PLM_GIT_PLUGIN::UpdateECOStatus(
        const PLM_ECO_STATUS_TRANSITION& aTransition )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_forgeProvider )
    {
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                                wxS( "ECO requires a forge provider" ) );
    }

    wxString comment = aTransition.comment;

    if( comment.IsEmpty() )
    {
        comment = wxString::Format( wxS( "Status changed to %d" ),
                                    static_cast<int>( aTransition.target_status ) );
    }

    PLM_REVIEW_COMMENT commentObj = PLM_REVIEW_COMMENT::General( aTransition.eco_id, comment );
    m_forgeProvider->AddPullRequestComment( aTransition.eco_id, commentObj );

    if( aTransition.target_status == PLM_ECO_STATUS::COMPLETED )
    {
        m_forgeProvider->MergePullRequest( aTransition.eco_id );
    }

    return GetECO( aTransition.eco_id );
}


PLM_ECO_UPDATE_RESULT PLM_GIT_PLUGIN::UpdateECOItems(
        const wxString& aEcoId, const std::vector<PLM_ECO_ITEM>& aItems )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_forgeProvider )
    {
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                                wxS( "ECO requires a forge provider" ) );
    }

    wxString body = wxS( "## Affected Items Updated\n" );

    for( const PLM_ECO_ITEM& item : aItems )
    {
        body += wxS( "- " ) + item.item_name;

        if( !item.item_type.IsEmpty() )
            body += wxS( " (" ) + item.item_type + wxS( ")" );

        body += wxS( "\n" );
    }

    PLM_REVIEW_COMMENT comment = PLM_REVIEW_COMMENT::General( aEcoId, body );
    m_forgeProvider->AddPullRequestComment( aEcoId, comment );

    return GetECO( aEcoId );
}


PLM_ECO_UPDATE_RESULT PLM_GIT_PLUGIN::UpdateECOImpact( const wxString&       aEcoId,
                                                         const PLM_ECO_IMPACT& aImpact )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_forgeProvider )
    {
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                                wxS( "ECO requires a forge provider" ) );
    }

    wxString body = wxS( "## Impact Assessment Updated\n" );

    if( !aImpact.cost_impact.IsEmpty() )
        body += wxS( "- Cost: " ) + aImpact.cost_impact + wxS( "\n" );

    if( !aImpact.schedule_impact.IsEmpty() )
        body += wxS( "- Schedule: " ) + aImpact.schedule_impact + wxS( "\n" );

    if( !aImpact.manufacturing_impact.IsEmpty() )
        body += wxS( "- Manufacturing: " ) + aImpact.manufacturing_impact + wxS( "\n" );

    if( !aImpact.regulatory_impact.IsEmpty() )
        body += wxS( "- Regulatory: " ) + aImpact.regulatory_impact + wxS( "\n" );

    if( !aImpact.supply_chain_impact.IsEmpty() )
        body += wxS( "- Supply chain: " ) + aImpact.supply_chain_impact + wxS( "\n" );

    PLM_REVIEW_COMMENT comment = PLM_REVIEW_COMMENT::General( aEcoId, body );
    m_forgeProvider->AddPullRequestComment( aEcoId, comment );

    return GetECO( aEcoId );
}


std::vector<PLM_ECO_STATUS_TRANSITION> PLM_GIT_PLUGIN::GetECOHistory(
        const wxString& aEcoId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_ECO_STATUS_TRANSITION> history;

    if( !m_forgeProvider )
        return history;

    std::vector<PLM_REVIEW_COMMENT> comments =
            m_forgeProvider->GetPullRequestComments( aEcoId );

    for( const PLM_REVIEW_COMMENT& c : comments )
    {
        PLM_ECO_STATUS_TRANSITION transition;
        transition.eco_id = aEcoId;
        transition.comment = c.body;
        transition.reviewer_id = c.author_id;
        transition.target_status = PLM_ECO_STATUS::IN_PROGRESS;
        history.push_back( transition );
    }

    return history;
}


PLM_ECO_CREATE_RESULT PLM_GIT_PLUGIN::ConvertECRToECO( const wxString& aEcrId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_forgeProvider )
    {
        return PLM_ECO_CREATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                                wxS( "ECO requires a forge provider" ) );
    }

    PLM_REVIEW_COMMENT comment = PLM_REVIEW_COMMENT::General(
            aEcrId, wxS( "Converted from ECR to ECO" ) );
    m_forgeProvider->AddPullRequestComment( aEcrId, comment );

    PLM_ECO_UPDATE_RESULT ecoResult = GetECO( aEcrId );

    if( !ecoResult.success )
    {
        return PLM_ECO_CREATE_RESULT::Failure( ecoResult.error_code, ecoResult.error_message );
    }

    ecoResult.eco.type = PLM_ECO_TYPE::ECO;
    ecoResult.eco.linked_ecr_id = aEcrId;
    return PLM_ECO_CREATE_RESULT::Success( ecoResult.eco );
}


//-------------------------------------------------------------------------
// BOM Management Interface
//-------------------------------------------------------------------------

PLM_BOM PLM_GIT_PLUGIN::GetBOM( const wxString& aProductId, PLM_BOM_TYPE aType )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !EnsureClone() )
        return PLM_BOM();

    wxString bomPath = BuildBOMPath( aProductId, aType );

    if( !wxFileExists( bomPath ) )
        return PLM_BOM();

    return ParseBOMFile( bomPath, aProductId );
}


std::vector<PLM_BOM> PLM_GIT_PLUGIN::GetBOMs( const wxString& aProductId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_BOM> boms;

    if( !m_isAuthenticated || !EnsureClone() )
        return boms;

    wxString projectDir = GetProjectPath( aProductId );

    if( !wxDirExists( projectDir ) )
        return boms;

    wxDir dir( projectDir );
    wxString filename;
    bool cont = dir.GetFirst( &filename, wxS( "bom_*.csv" ), wxDIR_FILES );

    while( cont )
    {
        wxString fullPath = projectDir + wxFileName::GetPathSeparator() + filename;
        PLM_BOM bom = ParseBOMFile( fullPath, aProductId );
        boms.push_back( bom );
        cont = dir.GetNext( &filename );
    }

    return boms;
}


PLM_BOM_RESULT PLM_GIT_PLUGIN::CreateBOM( const PLM_BOM& aBom )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !EnsureClone() )
        return PLM_BOM_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED, wxS( "Not ready" ) );

    wxString bomPath = BuildBOMPath( aBom.product_id, aBom.type );

    if( wxFileExists( bomPath ) )
    {
        return PLM_BOM_RESULT::Failure( PLM_ERROR_CODE::ERR_FILE_EXISTS,
                                         wxS( "BOM file already exists" ) );
    }

    if( !WriteBOMFile( bomPath, aBom ) )
        return PLM_BOM_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL, wxS( "Failed to write BOM" ) );

    wxString output;
    wxString relPath = bomPath.Mid( m_localClonePath.length() + 1 );
    RunGitCommand( { wxS( "add" ), relPath }, output, m_localClonePath );
    RunGitCommand( { wxS( "commit" ), wxS( "-m" ),
                     wxS( "Add " ) + BOMTypeToSuffix( aBom.type ) + wxS( " BOM" ) },
                   output, m_localClonePath );

    wxString bomId = aBom.product_id + wxS( "/" ) + wxFileName( bomPath ).GetFullName();
    return PLM_BOM_RESULT::Success( bomId );
}


PLM_BOM_RESULT PLM_GIT_PLUGIN::UpdateBOM( const PLM_BOM& aBom )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !EnsureClone() )
        return PLM_BOM_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED, wxS( "Not ready" ) );

    wxString bomPath = BuildBOMPath( aBom.product_id, aBom.type );

    if( !WriteBOMFile( bomPath, aBom ) )
        return PLM_BOM_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL, wxS( "Failed to write BOM" ) );

    wxString output;
    wxString relPath = bomPath.Mid( m_localClonePath.length() + 1 );
    RunGitCommand( { wxS( "add" ), relPath }, output, m_localClonePath );
    RunGitCommand( { wxS( "commit" ), wxS( "-m" ),
                     wxS( "Update " ) + BOMTypeToSuffix( aBom.type ) + wxS( " BOM" ) },
                   output, m_localClonePath );

    wxString bomId = aBom.product_id + wxS( "/" ) + wxFileName( bomPath ).GetFullName();
    return PLM_BOM_RESULT::Success( bomId );
}


bool PLM_GIT_PLUGIN::DeleteBOM( const wxString& aBomId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !EnsureClone() )
        return false;

    wxString relativePath = FolderIdToPath( aBomId );
    wxString fullPath = m_localClonePath + wxFileName::GetPathSeparator() + relativePath;

    if( !wxFileExists( fullPath ) )
        return false;

    wxRemoveFile( fullPath );

    wxString output;
    RunGitCommand( { wxS( "add" ), relativePath }, output, m_localClonePath );
    RunGitCommand( { wxS( "commit" ), wxS( "-m" ), wxS( "Delete BOM " ) + aBomId },
                   output, m_localClonePath );

    return true;
}


PLM_BOM_DIFF_RESULT PLM_GIT_PLUGIN::CompareBOMs( const PLM_BOM&              aBomA,
                                                    const PLM_BOM&              aBomB,
                                                    const PLM_BOM_DIFF_OPTIONS& aOptions )
{
    PLM_BOM_DIFF_RESULT result;

    // Build lookup from BOM A lines keyed by part_number or reference
    std::map<wxString, const PLM_BOM_LINE*> mapA;

    for( const PLM_BOM_LINE& line : aBomA.lines )
    {
        if( aOptions.ignore_dnp && line.dnp )
            continue;

        wxString key = aOptions.match_by_reference ? line.reference : line.part_number;
        mapA[key] = &line;
    }

    std::map<wxString, bool> visited;

    for( const PLM_BOM_LINE& lineB : aBomB.lines )
    {
        if( aOptions.ignore_dnp && lineB.dnp )
            continue;

        wxString key = aOptions.match_by_reference ? lineB.reference : lineB.part_number;
        auto it = mapA.find( key );

        PLM_BOM_DIFF_ENTRY entry;

        if( it == mapA.end() )
        {
            entry.change_type = PLM_BOM_DIFF_TYPE::ADDED;
            entry.new_line = lineB;
            entry.change_detail = wxS( "Added " ) + lineB.part_number;
            ++result.added_count;
        }
        else
        {
            visited[key] = true;
            const PLM_BOM_LINE& lineA = *it->second;
            bool changed = lineA.quantity != lineB.quantity || lineA.value != lineB.value
                           || lineA.dnp != lineB.dnp;

            if( !aOptions.ignore_sequence && lineA.sequence != lineB.sequence )
                changed = true;

            if( changed )
            {
                entry.change_type = PLM_BOM_DIFF_TYPE::CHANGED;
                entry.old_line = lineA;
                entry.new_line = lineB;
                entry.change_detail = wxS( "Modified " ) + lineB.part_number;
                ++result.changed_count;
            }
            else
            {
                entry.change_type = PLM_BOM_DIFF_TYPE::UNCHANGED;
                entry.old_line = lineA;
                entry.new_line = lineB;
                ++result.unchanged_count;
            }
        }

        result.entries.push_back( entry );
    }

    for( const auto& [key, linePtr] : mapA )
    {
        if( visited.find( key ) == visited.end() )
        {
            PLM_BOM_DIFF_ENTRY entry;
            entry.change_type = PLM_BOM_DIFF_TYPE::REMOVED;
            entry.old_line = *linePtr;
            entry.change_detail = wxS( "Removed " ) + linePtr->part_number;
            result.entries.push_back( entry );
            ++result.removed_count;
        }
    }

    return result;
}


PLM_BOM_VALIDATION_RESULT PLM_GIT_PLUGIN::ValidateBOMEntries(
        const std::vector<PLM_BOM_ENTRY>& aEntries )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<wxString> partNumbers;

    for( const PLM_BOM_ENTRY& entry : aEntries )
    {
        if( !entry.dnp && !entry.part_number.IsEmpty() )
            partNumbers.push_back( entry.part_number );
    }

    PLM_BOM_VALIDATION_RESULT result = ValidateBOM( partNumbers );

    for( const PLM_BOM_ENTRY& entry : aEntries )
    {
        if( entry.quantity <= 0 && !entry.dnp )
        {
            PLM_BOM_ISSUE issue( entry.reference, entry.part_number, wxS( "invalid_quantity" ),
                                  wxString::Format( wxS( "Invalid quantity %g for %s" ),
                                                    entry.quantity, entry.reference ) );
            issue.severity = PLM_BOM_ISSUE_SEVERITY::ERROR;
            result.AddIssue( issue );
        }
    }

    return result;
}


//-------------------------------------------------------------------------
// Manufacturing/Routing Interface
//-------------------------------------------------------------------------

PLM_ROUTING PLM_GIT_PLUGIN::GetRouting( const wxString& aProductId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !EnsureClone() )
        return PLM_ROUTING();

    wxString projectDir = GetProjectPath( aProductId );
    wxString routingPath = projectDir + wxFileName::GetPathSeparator() + wxS( "routing.json" );

    nlohmann::json j;

    if( !ReadJsonFile( routingPath, j ) )
        return PLM_ROUTING();

    PLM_ROUTING routing;
    routing.routing_id = j.value( "routing_id", "" );
    routing.name = wxString::FromUTF8( j.value( "name", "" ) );
    routing.product_id = aProductId;

    if( j.contains( "operations" ) && j["operations"].is_array() )
    {
        for( const auto& opJson : j["operations"] )
        {
            PLM_OPERATION op;
            op.operation_id = wxString::FromUTF8( opJson.value( "operation_id", "" ) );
            op.name = wxString::FromUTF8( opJson.value( "name", "" ) );
            op.sequence = opJson.value( "sequence", 0 );
            op.workcenter_id = wxString::FromUTF8( opJson.value( "workcenter_id", "" ) );
            op.duration_minutes = opJson.value( "duration_minutes", 0.0 );
            op.description = wxString::FromUTF8( opJson.value( "description", "" ) );
            routing.operations.push_back( op );
        }
    }

    return routing;
}


std::vector<PLM_WORKCENTER> PLM_GIT_PLUGIN::GetWorkcenters()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_WORKCENTER> workcenters;

    if( !m_isAuthenticated || !EnsureClone() )
        return workcenters;

    wxString wcPath = m_localClonePath + wxFileName::GetPathSeparator()
                      + wxS( "workcenters.json" );

    nlohmann::json j;

    if( !ReadJsonFile( wcPath, j ) || !j.is_array() )
        return workcenters;

    for( const auto& wcJson : j )
    {
        PLM_WORKCENTER wc;
        wc.workcenter_id = wxString::FromUTF8( wcJson.value( "workcenter_id", "" ) );
        wc.name = wxString::FromUTF8( wcJson.value( "name", "" ) );
        wc.code = wxString::FromUTF8( wcJson.value( "code", "" ) );
        wc.description = wxString::FromUTF8( wcJson.value( "description", "" ) );
        wc.cost_per_hour = wcJson.value( "cost_per_hour", 0.0 );
        wc.active = wcJson.value( "active", true );
        workcenters.push_back( wc );
    }

    return workcenters;
}


PLM_ROUTING_RESULT PLM_GIT_PLUGIN::SaveRouting( const PLM_ROUTING& aRouting )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !EnsureClone() )
    {
        return PLM_ROUTING_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                             wxS( "Not ready" ) );
    }

    wxString projectDir = GetProjectPath( aRouting.product_id );
    wxString routingPath = projectDir + wxFileName::GetPathSeparator() + wxS( "routing.json" );

    nlohmann::json j;
    j["routing_id"] = aRouting.routing_id.ToStdString();
    j["name"] = aRouting.name.ToStdString();
    j["product_id"] = aRouting.product_id.ToStdString();
    j["operations"] = nlohmann::json::array();

    for( const PLM_OPERATION& op : aRouting.operations )
    {
        nlohmann::json opJson;
        opJson["operation_id"] = op.operation_id.ToStdString();
        opJson["name"] = op.name.ToStdString();
        opJson["sequence"] = op.sequence;
        opJson["workcenter_id"] = op.workcenter_id.ToStdString();
        opJson["duration_minutes"] = op.duration_minutes;
        opJson["description"] = op.description.ToStdString();
        j["operations"].push_back( opJson );
    }

    if( !WriteJsonFile( routingPath, j ) )
    {
        return PLM_ROUTING_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                             wxS( "Failed to write routing" ) );
    }

    wxString output;
    wxString relPath = routingPath.Mid( m_localClonePath.length() + 1 );
    RunGitCommand( { wxS( "add" ), relPath }, output, m_localClonePath );
    RunGitCommand( { wxS( "commit" ), wxS( "-m" ), wxS( "Update routing" ) },
                   output, m_localClonePath );

    return PLM_ROUTING_RESULT::Success( aRouting.routing_id );
}


//-------------------------------------------------------------------------
// Lifecycle State Transition Interface
//-------------------------------------------------------------------------

std::vector<PLM_STATE_INFO> PLM_GIT_PLUGIN::GetStateMappings()
{
    return {
        PLM_STATE_INFO( wxS( "draft" ), PLM_STATE_CATEGORY::EDITABLE,
                        wxS( "Draft" ), wxS( "Work in progress" ) ),
        PLM_STATE_INFO( wxS( "in-review" ), PLM_STATE_CATEGORY::PENDING_REVIEW,
                        wxS( "In Review" ), wxS( "Submitted for review" ) ),
        PLM_STATE_INFO( wxS( "released" ), PLM_STATE_CATEGORY::RELEASED,
                        wxS( "Released" ), wxS( "Approved for production" ) ),
    };
}


std::vector<PLM_VALID_TRANSITION> PLM_GIT_PLUGIN::GetValidTransitions(
        const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_VALID_TRANSITION> transitions;

    PLM_STATE_INFO currentState;

    if( !GetCurrentState( aProjectId, currentState ) )
        currentState.native_state = wxS( "draft" );

    if( currentState.native_state == wxS( "draft" ) )
    {
        transitions.emplace_back( wxS( "in-review" ), PLM_STATE_CATEGORY::PENDING_REVIEW,
                                  wxS( "Submit for Review" ) );
        transitions.emplace_back( wxS( "released" ), PLM_STATE_CATEGORY::RELEASED,
                                  wxS( "Release" ) );
    }
    else if( currentState.native_state == wxS( "in-review" ) )
    {
        transitions.emplace_back( wxS( "draft" ), PLM_STATE_CATEGORY::EDITABLE,
                                  wxS( "Return to Draft" ) );
        transitions.emplace_back( wxS( "released" ), PLM_STATE_CATEGORY::RELEASED,
                                  wxS( "Approve and Release" ) );
    }
    else if( currentState.native_state == wxS( "released" ) )
    {
        transitions.emplace_back( wxS( "draft" ), PLM_STATE_CATEGORY::EDITABLE,
                                  wxS( "Revise" ) );
    }

    return transitions;
}


PLM_STATE_TRANSITION_RESULT PLM_GIT_PLUGIN::ChangeState( const wxString& aProjectId,
                                                           const wxString& aTargetState,
                                                           const wxString& aComment )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !EnsureClone() )
    {
        return PLM_STATE_TRANSITION_RESULT::Error( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                                    wxS( "Not ready" ) );
    }

    wxString relativePath = FolderIdToPath( aProjectId );
    wxString tagName = relativePath + wxS( "/state/" ) + aTargetState;
    tagName.Replace( wxFileName::GetPathSeparator(), wxS( "/" ) );

    wxString tagMsg = aComment.IsEmpty()
                              ? wxString::Format( wxS( "State transition to %s" ), aTargetState )
                              : aComment;

    wxString output;
    int rc = RunGitCommand( { wxS( "tag" ), wxS( "-a" ), tagName, wxS( "-m" ), tagMsg },
                            output, m_localClonePath );

    if( rc != 0 )
    {
        return PLM_STATE_TRANSITION_RESULT::Error( PLM_ERROR_CODE::ERR_INTERNAL,
                                                    wxS( "Failed to create state tag" ) );
    }

    PLM_STATE_CATEGORY category = PLM_STATE_CATEGORY::EDITABLE;

    if( aTargetState == wxS( "in-review" ) )
        category = PLM_STATE_CATEGORY::PENDING_REVIEW;
    else if( aTargetState == wxS( "released" ) )
        category = PLM_STATE_CATEGORY::RELEASED;

    // Persist structured state metadata as a git note on the project's
    // current HEAD. refs/notes/plm/lifecycle is mutable per-commit and lets
    // downstream tools answer "what state was this commit in?" without
    // parsing tag names. The tag stays as the authoritative latest-state
    // signal across the whole project history.
    wxString headRev;
    RunGitCommand( { wxS( "rev-parse" ), wxS( "HEAD" ) }, headRev, m_localClonePath );
    headRev.Trim();

    PLM_STATE_TRANSITION_RESULT successResult =
            PLM_STATE_TRANSITION_RESULT::Success( aTargetState, category );

    if( !headRev.IsEmpty() )
    {
        nlohmann::json note;
        note["state"] = aTargetState.ToStdString();
        note["category"] = static_cast<int>( category );
        note["project_id"] = aProjectId.ToStdString();
        note["user"] = m_username.ToStdString();
        note["transition_time"] = static_cast<int64_t>( std::time( nullptr ) );

        if( !aComment.IsEmpty() )
            note["comment"] = aComment.ToStdString();

        std::string serialized = note.dump( 2 );

        // -f overwrites any existing note on this commit so successive
        // transitions on the same head replace rather than collide.
        int noteRc = RunGitCommand( { wxS( "notes" ), wxS( "--ref=plm/lifecycle" ),
                                       wxS( "add" ), wxS( "-f" ), wxS( "-m" ),
                                       wxString::FromUTF8( serialized ), headRev },
                                     output, m_localClonePath );

        if( noteRc != 0 )
        {
            successResult.error_message =
                    wxS( "State tag created, but lifecycle note could not be written" );
        }
        else
        {
            int notePushRc =
                    RunGitCommand( { wxS( "push" ), wxS( "origin" ),
                                     wxS( "refs/notes/plm/lifecycle" ) },
                                   output, m_localClonePath );

            if( notePushRc != 0 )
            {
                successResult.error_message =
                        wxS( "Lifecycle note recorded locally but not pushed to origin" );
            }
        }
    }

    RunGitCommand( { wxS( "push" ), wxS( "origin" ), tagName }, output, m_localClonePath );

    return successResult;
}


bool PLM_GIT_PLUGIN::GetCurrentState( const wxString& aProjectId, PLM_STATE_INFO& aStateInfo )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !EnsureClone() )
        return false;

    // Prefer refs/notes/plm/lifecycle on HEAD when present -- it carries
    // the structured state metadata (state, category, user, time, comment)
    // produced by ChangeState. Fall back to the tag scan for backwards
    // compatibility with checkouts whose state was set before notes
    // existed.
    wxString headRev;
    RunGitCommand( { wxS( "rev-parse" ), wxS( "HEAD" ) }, headRev, m_localClonePath );
    headRev.Trim();

    if( !headRev.IsEmpty() )
    {
        wxString noteOutput;
        int      noteRc = RunGitCommand( { wxS( "notes" ), wxS( "--ref=plm/lifecycle" ),
                                            wxS( "show" ), headRev },
                                          noteOutput, m_localClonePath );

        if( noteRc == 0 && !noteOutput.Trim().IsEmpty() )
        {
            try
            {
                nlohmann::json note = nlohmann::json::parse( noteOutput.ToStdString() );

                // The note attaches to a commit, but multiple projects may
                // share a HEAD (multi-project repos), so trust the note only
                // when its project_id matches. Otherwise fall through to the
                // tag scan below, which is per-project by name.
                bool projectMatches = false;

                if( note.contains( "project_id" ) && note["project_id"].is_string() )
                {
                    wxString notedProject = wxString::FromUTF8(
                            note["project_id"].get<std::string>() );

                    if( notedProject == aProjectId )
                        projectMatches = true;
                }

                if( projectMatches && note.contains( "state" ) && note["state"].is_string() )
                {
                    wxString           state = wxString::FromUTF8( note["state"].get<std::string>() );
                    PLM_STATE_CATEGORY category = PLM_STATE_CATEGORY::EDITABLE;

                    if( note.contains( "category" ) && note["category"].is_number_integer() )
                        category = static_cast<PLM_STATE_CATEGORY>( note["category"].get<int>() );

                    aStateInfo = PLM_STATE_INFO( state, category );
                    return true;
                }
            }
            catch( const nlohmann::json::exception& )
            {
                // Malformed note; fall through to tag-based lookup below.
            }
        }
    }

    wxString relativePath = FolderIdToPath( aProjectId );
    wxString tagPrefix = relativePath + wxS( "/state/" );
    tagPrefix.Replace( wxFileName::GetPathSeparator(), wxS( "/" ) );

    wxString output;
    int rc = RunGitCommand( { wxS( "tag" ), wxS( "-l" ), tagPrefix + wxS( "*" ),
                              wxS( "--sort=-creatordate" ) },
                            output, m_localClonePath );

    if( rc != 0 || output.Trim().IsEmpty() )
    {
        aStateInfo = PLM_STATE_INFO( wxS( "draft" ), PLM_STATE_CATEGORY::EDITABLE,
                                     wxS( "Draft" ) );
        aStateInfo.is_default = true;
        return true;
    }

    wxString latestTag = output.BeforeFirst( '\n' ).Trim();
    wxString state = latestTag.Mid( tagPrefix.length() );

    PLM_STATE_CATEGORY category = PLM_STATE_CATEGORY::EDITABLE;

    if( state == wxS( "in-review" ) )
        category = PLM_STATE_CATEGORY::PENDING_REVIEW;
    else if( state == wxS( "released" ) )
        category = PLM_STATE_CATEGORY::RELEASED;

    aStateInfo = PLM_STATE_INFO( state, category );
    return true;
}


//-------------------------------------------------------------------------
// Review and Approval Interface
//-------------------------------------------------------------------------

PLM_REVIEW_RESULT PLM_GIT_PLUGIN::SubmitForReview(
        const wxString& aProjectId, const PLM_REVIEW_SUBMIT_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_forgeProvider )
    {
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                            wxS( "Review requires a forge provider" ) );
    }

    wxString sourceBranch = aOptions.source_branch;

    if( sourceBranch.IsEmpty() )
    {
        wxString output;
        RunGitCommand( { wxS( "rev-parse" ), wxS( "--abbrev-ref" ), wxS( "HEAD" ) },
                       output, m_localClonePath );
        sourceBranch = output.Trim();
    }

    wxString targetBranch = aOptions.target_branch;

    if( targetBranch.IsEmpty() )
        targetBranch = wxS( "main" );

    return m_forgeProvider->CreatePullRequest( sourceBranch, targetBranch, aOptions.title,
                                               aOptions.description, aOptions.reviewer_ids,
                                               aOptions.draft );
}


std::vector<PLM_REVIEW> PLM_GIT_PLUGIN::GetReviews( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_forgeProvider )
        return {};

    return m_forgeProvider->GetPullRequests();
}


PLM_REVIEW PLM_GIT_PLUGIN::GetReview( const wxString& aReviewId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_forgeProvider )
        return PLM_REVIEW();

    return m_forgeProvider->GetPullRequest( aReviewId );
}


std::vector<PLM_REVIEW_COMMENT> PLM_GIT_PLUGIN::GetReviewComments(
        const wxString& aReviewId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_forgeProvider )
        return {};

    std::vector<PLM_REVIEW_COMMENT> comments = m_forgeProvider->GetPullRequestComments( aReviewId );

    // Cache each (comment_id -> review_id) so a later ResolveReviewComment
    // call can recover the review context the abstract API does not pass in.
    for( const PLM_REVIEW_COMMENT& comment : comments )
    {
        if( !comment.comment_id.IsEmpty() )
        {
            const wxString& reviewId = comment.review_id.IsEmpty() ? aReviewId : comment.review_id;
            RecordReviewComment( comment.comment_id, reviewId );
        }
    }

    return comments;
}


bool PLM_GIT_PLUGIN::AddReviewComment( const PLM_REVIEW_COMMENT& aComment )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_forgeProvider )
        return false;

    bool ok = m_forgeProvider->AddPullRequestComment( aComment.review_id, aComment );

    if( ok && !aComment.comment_id.IsEmpty() && !aComment.review_id.IsEmpty() )
        RecordReviewComment( aComment.comment_id, aComment.review_id );

    return ok;
}


bool PLM_GIT_PLUGIN::ResolveReviewComment( const wxString& aCommentId, bool aResolved )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_forgeProvider )
        return false;

    // Recover the review context from the comment_id via the cached mapping
    // (populated by AddReviewComment / GetReviewComments). Without it we
    // have no review_id to attach the resolution reply to.
    wxString reviewId = LookupReviewForComment( aCommentId );

    if( reviewId.IsEmpty() )
        return false;

    PLM_REVIEW_COMMENT reply;
    reply.review_id = reviewId;
    reply.parent_comment_id = aCommentId;
    reply.body = aResolved ? wxS( "[Resolved]" ) : wxS( "[Reopened]" );

    return m_forgeProvider->AddPullRequestComment( reviewId, reply );
}


PLM_REVIEW_RESULT PLM_GIT_PLUGIN::SubmitReviewDecision( const wxString&     aReviewId,
                                                          PLM_REVIEW_DECISION aDecision,
                                                          const wxString&     aComment )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_forgeProvider )
    {
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                            wxS( "Review requires a forge provider" ) );
    }

    return m_forgeProvider->SubmitPullRequestReview( aReviewId, aDecision, aComment );
}


PLM_REVIEW_RESULT PLM_GIT_PLUGIN::MergeReview( const wxString& aReviewId,
                                                 const wxString& aStrategy )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_forgeProvider )
    {
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                            wxS( "Review requires a forge provider" ) );
    }

    PLM_REVIEW_RESULT result = m_forgeProvider->MergePullRequest( aReviewId, aStrategy );

    if( result.success && !m_localClonePath.IsEmpty() )
    {
        wxString output;
        RunGitCommand( { wxS( "pull" ), wxS( "--ff-only" ) }, output, m_localClonePath );
    }

    return result;
}


//-------------------------------------------------------------------------
// Forge Provider Interface
//-------------------------------------------------------------------------

PLM_FORGE_PROVIDER* PLM_GIT_PLUGIN::GetForgeProvider()
{
    return m_forgeProvider.get();
}


//-------------------------------------------------------------------------
// Part Management Extended Interface
//-------------------------------------------------------------------------

PLM_PART PLM_GIT_PLUGIN::CreatePart( const PLM_PART_CREATE_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !EnsureClone() )
        return PLM_PART();

    EnsurePartsCache();

    // Check for duplicate
    for( const PLM_PART& existing : m_partsCache )
    {
        if( existing.part_number == aOptions.part_number )
            return PLM_PART();
    }

    PLM_PART part;
    part.part_id = aOptions.part_number;
    part.part_number = aOptions.part_number;
    part.description = aOptions.description;
    part.category = aOptions.category;
    part.subcategory = aOptions.subcategory;
    part.manufacturer = aOptions.manufacturer;
    part.manufacturer_pn = aOptions.manufacturer_pn;
    part.state = aOptions.initial_state;
    part.attributes = aOptions.attributes;

    m_partsCache.push_back( part );

    if( !WritePartsCSV() )
        return PLM_PART();

    wxString output;
    RunGitCommand( { wxS( "add" ), wxS( "parts.csv" ) }, output, m_localClonePath );
    RunGitCommand( { wxS( "commit" ), wxS( "-m" ),
                     wxS( "Add part " ) + aOptions.part_number },
                   output, m_localClonePath );

    return part;
}


PLM_PART PLM_GIT_PLUGIN::UpdatePart( const PLM_PART& aPart )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !EnsureClone() )
        return PLM_PART();

    EnsurePartsCache();

    bool found = false;

    for( PLM_PART& existing : m_partsCache )
    {
        if( existing.part_number == aPart.part_number || existing.part_id == aPart.part_id )
        {
            existing.description = aPart.description;
            existing.category = aPart.category;
            existing.manufacturer = aPart.manufacturer;
            existing.manufacturer_pn = aPart.manufacturer_pn;
            existing.state = aPart.state;
            existing.datasheet_url = aPart.datasheet_url;
            existing.symbol_ref = aPart.symbol_ref;
            existing.footprint_ref = aPart.footprint_ref;
            existing.attributes = aPart.attributes;
            found = true;
            break;
        }
    }

    if( !found )
        return PLM_PART();

    if( !WritePartsCSV() )
        return PLM_PART();

    wxString output;
    RunGitCommand( { wxS( "add" ), wxS( "parts.csv" ) }, output, m_localClonePath );
    RunGitCommand( { wxS( "commit" ), wxS( "-m" ),
                     wxS( "Update part " ) + aPart.part_number },
                   output, m_localClonePath );

    return aPart;
}


PLM_PART PLM_GIT_PLUGIN::UpdatePartState( const wxString& aPartId,
                                            PLM_PART_STATE  aNewState,
                                            const wxString& aComment )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_PART part = GetPart( aPartId );

    if( part.part_id.IsEmpty() )
        return PLM_PART();

    part.state = aNewState;
    return UpdatePart( part );
}


std::vector<PLM_VENDOR_SOURCE> PLM_GIT_PLUGIN::GetVendorSources( const wxString& aPartId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_VENDOR_SOURCE> sources;

    PLM_PART part = GetPart( aPartId );

    if( part.part_id.IsEmpty() )
        return sources;

    auto attrVal = [&]( const wxString& aId ) -> wxString
    {
        const PLM_ATTRIBUTE_VALUE* v = part.attributes.FindValue( aId );
        return v ? v->value : wxString();
    };

    wxString vendorName = attrVal( wxS( "vendor_name" ) );

    if( vendorName.IsEmpty() )
        return sources;

    PLM_VENDOR_SOURCE source;
    source.vendor_name = vendorName;
    source.vendor_part_number = attrVal( wxS( "vendor_part_number" ) );
    source.active = true;

    wxString price = attrVal( wxS( "price" ) );

    if( !price.IsEmpty() )
    {
        double p;

        if( price.ToDouble( &p ) )
            source.unit_price = p;
    }

    wxString leadTime = attrVal( wxS( "lead_time" ) );

    if( !leadTime.IsEmpty() )
    {
        long lt;

        if( leadTime.ToLong( &lt ) )
            source.lead_time_days = static_cast<int>( lt );
    }

    sources.push_back( source );
    return sources;
}


std::vector<PLM_WHERE_USED_ENTRY> PLM_GIT_PLUGIN::GetWhereUsed( const wxString& aPartId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_WHERE_USED_ENTRY> results;

    if( !m_isAuthenticated || !EnsureClone() )
        return results;

    // Scan all project directories for bom_*.csv files
    wxString output;
    RunGitCommand( { wxS( "ls-files" ), wxS( "**/bom_*.csv" ) }, output, m_localClonePath );

    wxStringTokenizer tokenizer( output, wxS( "\n" ) );

    while( tokenizer.HasMoreTokens() )
    {
        wxString bomRelPath = tokenizer.GetNextToken().Trim();

        if( bomRelPath.IsEmpty() )
            continue;

        wxString bomFullPath = m_localClonePath + wxFileName::GetPathSeparator() + bomRelPath;

        if( !wxFileExists( bomFullPath ) )
            continue;

        wxString parentDir = wxFileName( bomRelPath ).GetPath();
        PLM_BOM bom = ParseBOMFile( bomFullPath, PathToFolderId( parentDir ) );

        for( const PLM_BOM_LINE& line : bom.lines )
        {
            if( line.part_number == aPartId )
            {
                PLM_WHERE_USED_ENTRY entry;
                entry.parent_id = bom.bom_id;
                entry.parent_name = parentDir;
                entry.parent_type = wxS( "bom" );
                entry.quantity = line.quantity;
                entry.reference = line.reference;
                results.push_back( entry );
            }
        }
    }

    return results;
}


//-------------------------------------------------------------------------
// Release Management Interface
//-------------------------------------------------------------------------

std::vector<PLM_RELEASE_CHECK> PLM_GIT_PLUGIN::ValidateForRelease(
        const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_RELEASE_CHECK> checks;

    if( !m_isAuthenticated || !EnsureClone() )
        return checks;

    // Check for uncommitted changes
    wxString statusOutput;
    wxString relativePath = FolderIdToPath( aProjectId );
    RunGitCommand( { wxS( "status" ), wxS( "--porcelain" ), wxS( "--" ), relativePath },
                   statusOutput, m_localClonePath );

    bool cleanTree = statusOutput.Trim().IsEmpty();
    checks.emplace_back( wxS( "clean_tree" ), wxS( "No uncommitted changes" ), cleanTree,
                         cleanTree ? wxS( "Working tree is clean" )
                                   : wxS( "Uncommitted changes detected" ) );

    // Check if BOM exists
    wxString projectDir = GetProjectPath( aProjectId );
    bool hasBom = false;
    wxDir dir( projectDir );

    if( dir.IsOpened() )
    {
        wxString filename;
        hasBom = dir.GetFirst( &filename, wxS( "bom_*.csv" ), wxDIR_FILES );
    }

    checks.emplace_back( wxS( "has_bom" ), wxS( "BOM exists" ), hasBom,
                         hasBom ? wxS( "BOM file found" ) : wxS( "No BOM file in project" ) );

    // Validate parts if BOM exists
    if( hasBom )
    {
        std::vector<PLM_BOM> boms = GetBOMs( aProjectId );

        if( !boms.empty() )
        {
            std::vector<wxString> partNumbers;

            for( const PLM_BOM_LINE& line : boms[0].lines )
            {
                if( !line.dnp && !line.part_number.IsEmpty() )
                    partNumbers.push_back( line.part_number );
            }

            PLM_BOM_VALIDATION_RESULT bomResult = ValidateBOM( partNumbers );
            checks.emplace_back( wxS( "parts_valid" ), wxS( "All parts valid" ),
                                 bomResult.all_valid,
                                 bomResult.all_valid
                                         ? wxS( "All BOM parts are valid" )
                                         : wxString::Format( wxS( "%zu part issues found" ),
                                                             bomResult.issues.size() ) );
        }
    }

    return checks;
}


PLM_RELEASE_RESULT PLM_GIT_PLUGIN::CreateRelease( const wxString&            aProjectId,
                                                    const PLM_RELEASE_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !EnsureClone() )
    {
        return PLM_RELEASE_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                             wxS( "Not ready" ) );
    }

    wxString tagName = wxS( "release/" ) + aOptions.version;
    wxString tagMsg = aOptions.title;

    if( !aOptions.notes.IsEmpty() )
        tagMsg += wxS( "\n\n" ) + aOptions.notes;

    wxString output;
    int rc = RunGitCommand( { wxS( "tag" ), wxS( "-a" ), tagName, wxS( "-m" ), tagMsg },
                            output, m_localClonePath );

    if( rc != 0 )
    {
        return PLM_RELEASE_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                             wxS( "Failed to create release tag" ) );
    }

    RunGitCommand( { wxS( "push" ), wxS( "origin" ), tagName }, output, m_localClonePath );

    PLM_RELEASE release;
    release.release_id = tagName;
    release.project_id = aProjectId;
    release.version = aOptions.version;
    release.version_scheme = aOptions.version_scheme;
    release.title = aOptions.title;
    release.notes = aOptions.notes;
    release.status = aOptions.draft ? PLM_RELEASE_STATUS::DRAFT : PLM_RELEASE_STATUS::READY;
    release.released_by = m_username;
    release.created_at = static_cast<int64_t>( std::time( nullptr ) );

    // Create forge release if available
    if( m_forgeProvider && !aOptions.draft )
    {
        m_forgeProvider->CreateRelease( tagName, aOptions.title, aOptions.notes,
                                        aOptions.draft, false );
    }

    return PLM_RELEASE_RESULT::Success( release );
}


PLM_RELEASE_RESULT PLM_GIT_PLUGIN::FinalizeRelease( const wxString& aReleaseId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !EnsureClone() )
    {
        return PLM_RELEASE_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                             wxS( "Not ready" ) );
    }

    // Add a finalized tag alongside the release tag
    wxString finalizedTag = aReleaseId + wxS( "-finalized" );
    wxString output;
    int rc = RunGitCommand( { wxS( "tag" ), wxS( "-a" ), finalizedTag, wxS( "-m" ),
                              wxS( "Release finalized" ) },
                            output, m_localClonePath );

    if( rc != 0 )
    {
        return PLM_RELEASE_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                             wxS( "Failed to finalize release" ) );
    }

    RunGitCommand( { wxS( "push" ), wxS( "origin" ), finalizedTag }, output, m_localClonePath );

    PLM_RELEASE release;
    release.release_id = aReleaseId;
    release.status = PLM_RELEASE_STATUS::RELEASED;
    release.released_at = static_cast<int64_t>( std::time( nullptr ) );
    return PLM_RELEASE_RESULT::Success( release );
}


std::vector<PLM_RELEASE> PLM_GIT_PLUGIN::ListReleases( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_RELEASE> releases;

    if( !m_isAuthenticated || !EnsureClone() )
        return releases;

    wxString output;
    int rc = RunGitCommand( { wxS( "tag" ), wxS( "-l" ), wxS( "release/*" ),
                              wxS( "--sort=-creatordate" ),
                              wxS( "--format=%(refname:short)|%(creatordate:unix)|%(subject)" ) },
                            output, m_localClonePath );

    if( rc != 0 )
        return releases;

    wxStringTokenizer lineTokenizer( output, wxS( "\n" ) );

    while( lineTokenizer.HasMoreTokens() )
    {
        wxString line = lineTokenizer.GetNextToken().Trim();

        if( line.IsEmpty() || line.Contains( wxS( "-finalized" ) ) )
            continue;

        wxStringTokenizer fieldTokenizer( line, wxS( "|" ) );

        if( fieldTokenizer.CountTokens() < 2 )
            continue;

        PLM_RELEASE release;
        release.release_id = fieldTokenizer.GetNextToken();
        wxString timestamp = fieldTokenizer.GetNextToken();
        long ts;

        if( timestamp.ToLong( &ts ) )
            release.created_at = static_cast<int64_t>( ts );

        if( fieldTokenizer.HasMoreTokens() )
            release.title = fieldTokenizer.GetNextToken();

        release.project_id = aProjectId;
        release.version = release.release_id.Mid( 8 );  // Strip "release/"

        // Check if finalized
        wxString checkOutput;
        int tagRc = RunGitCommand(
                { wxS( "tag" ), wxS( "-l" ), release.release_id + wxS( "-finalized" ) },
                checkOutput, m_localClonePath );

        if( tagRc == 0 && !checkOutput.Trim().IsEmpty() )
            release.status = PLM_RELEASE_STATUS::RELEASED;
        else
            release.status = PLM_RELEASE_STATUS::READY;

        releases.push_back( release );
    }

    // Supplement with forge releases if available
    if( m_forgeProvider )
    {
        std::vector<PLM_FORGE_RELEASE> forgeReleases = m_forgeProvider->GetReleases();

        for( const PLM_FORGE_RELEASE& fr : forgeReleases )
        {
            bool found = false;

            for( PLM_RELEASE& r : releases )
            {
                if( r.release_id == fr.tag_name || r.version == fr.tag_name.Mid( 8 ) )
                {
                    r.web_url = fr.web_url;
                    found = true;
                    break;
                }
            }

            if( !found )
            {
                PLM_RELEASE release;
                release.release_id = fr.tag_name;
                release.title = fr.title;
                release.notes = fr.body;
                release.web_url = fr.web_url;
                release.created_at = fr.created_at;
                release.project_id = aProjectId;
                releases.push_back( release );
            }
        }
    }

    return releases;
}


PLM_RELEASE PLM_GIT_PLUGIN::GetRelease( const wxString& aReleaseId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_RELEASE release;

    if( !m_isAuthenticated || !EnsureClone() )
        return release;

    wxString output;
    int rc = RunGitCommand( { wxS( "tag" ), wxS( "-l" ), aReleaseId,
                              wxS( "--format=%(creatordate:unix)|%(subject)|%(body)" ) },
                            output, m_localClonePath );

    if( rc != 0 || output.Trim().IsEmpty() )
        return release;

    release.release_id = aReleaseId;
    release.version = aReleaseId.Mid( 8 );

    wxStringTokenizer fieldTokenizer( output.BeforeFirst( '\n' ), wxS( "|" ) );

    if( fieldTokenizer.HasMoreTokens() )
    {
        wxString timestamp = fieldTokenizer.GetNextToken();
        long ts;

        if( timestamp.ToLong( &ts ) )
            release.created_at = static_cast<int64_t>( ts );
    }

    if( fieldTokenizer.HasMoreTokens() )
        release.title = fieldTokenizer.GetNextToken();

    if( fieldTokenizer.HasMoreTokens() )
        release.notes = fieldTokenizer.GetNextToken();

    return release;
}


wxString PLM_GIT_PLUGIN::SuggestNextVersion( const wxString&    aProjectId,
                                               PLM_VERSION_SCHEME aScheme )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !EnsureClone() )
        return wxS( "1.0.0" );

    std::vector<PLM_RELEASE> releases = ListReleases( aProjectId );

    if( releases.empty() )
    {
        if( aScheme == PLM_VERSION_SCHEME::CALVER )
            return wxDateTime::Now().Format( wxS( "%Y.%m.%d" ) );

        return wxS( "1.0.0" );
    }

    wxString latest = releases[0].version;

    if( aScheme == PLM_VERSION_SCHEME::CALVER )
        return wxDateTime::Now().Format( wxS( "%Y.%m.%d" ) );

    // Parse semver and increment patch
    wxStringTokenizer verTokenizer( latest, wxS( "." ) );
    long major = 0, minor = 0, patch = 0;

    if( verTokenizer.HasMoreTokens() )
        verTokenizer.GetNextToken().ToLong( &major );

    if( verTokenizer.HasMoreTokens() )
        verTokenizer.GetNextToken().ToLong( &minor );

    if( verTokenizer.HasMoreTokens() )
        verTokenizer.GetNextToken().ToLong( &patch );

    return wxString::Format( wxS( "%ld.%ld.%ld" ), major, minor, patch + 1 );
}


//-------------------------------------------------------------------------
// Deviation Branch Interface
//-------------------------------------------------------------------------

PLM_DEVIATION_BRANCH PLM_GIT_PLUGIN::CreateDeviationBranch( const wxString& aProjectId,
                                                              const wxString& aBaseRevision,
                                                              const wxString& aReason )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_DEVIATION_BRANCH branch;

    if( !m_isAuthenticated || !EnsureClone() )
        return branch;

    wxString branchName = wxS( "deviation/" ) + aBaseRevision + wxS( "-" )
                          + wxDateTime::Now().Format( wxS( "%Y%m%d" ) );

    wxString output;
    int rc = RunGitCommand( { wxS( "checkout" ), wxS( "-b" ), branchName, aBaseRevision },
                            output, m_localClonePath );

    if( rc != 0 )
        return branch;

    RunGitCommand( { wxS( "push" ), wxS( "-u" ), wxS( "origin" ), branchName },
                   output, m_localClonePath );

    branch.branch_id = branchName;
    branch.branch_name = branchName;
    branch.release_id = aBaseRevision;
    branch.project_id = aProjectId;
    branch.reason = aReason;
    branch.created_by = m_username;
    branch.created_at = static_cast<int64_t>( std::time( nullptr ) );

    // Switch back to previous branch
    wxString prevBranch = m_currentBranch.IsEmpty() ? wxS( "main" ) : m_currentBranch;
    RunGitCommand( { wxS( "checkout" ), prevBranch }, output, m_localClonePath );

    return branch;
}


std::vector<PLM_DEVIATION_BRANCH> PLM_GIT_PLUGIN::ListDeviationBranches(
        const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_DEVIATION_BRANCH> branches;

    if( !m_isAuthenticated || !EnsureClone() )
        return branches;

    wxString output;
    int rc = RunGitCommand( { wxS( "branch" ), wxS( "-a" ), wxS( "--list" ),
                              wxS( "*deviation/*" ),
                              wxS( "--format=%(refname:short)|%(creatordate:unix)" ) },
                            output, m_localClonePath );

    if( rc != 0 )
        return branches;

    wxStringTokenizer lineTokenizer( output, wxS( "\n" ) );

    while( lineTokenizer.HasMoreTokens() )
    {
        wxString line = lineTokenizer.GetNextToken().Trim();

        if( line.IsEmpty() )
            continue;

        wxStringTokenizer fieldTokenizer( line, wxS( "|" ) );
        wxString name = fieldTokenizer.HasMoreTokens() ? fieldTokenizer.GetNextToken() : line;

        // Strip remote prefix if present
        if( name.StartsWith( wxS( "origin/" ) ) )
            name = name.Mid( 7 );

        PLM_DEVIATION_BRANCH branch;
        branch.branch_id = name;
        branch.branch_name = name;
        branch.project_id = aProjectId;

        if( fieldTokenizer.HasMoreTokens() )
        {
            long ts;

            if( fieldTokenizer.GetNextToken().ToLong( &ts ) )
                branch.created_at = static_cast<int64_t>( ts );
        }

        branches.push_back( branch );
    }

    return branches;
}


//-------------------------------------------------------------------------
// Variant Configuration Interface
//-------------------------------------------------------------------------

PLM_VARIANT PLM_GIT_PLUGIN::GetVariants( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_VARIANT variant( aProjectId );

    if( !m_isAuthenticated || !EnsureClone() )
        return variant;

    wxString projectDir = GetProjectPath( aProjectId );
    wxString variantsPath = projectDir + wxFileName::GetPathSeparator() + wxS( "variants.json" );

    nlohmann::json j;

    if( !ReadJsonFile( variantsPath, j ) )
        return variant;

    if( j.contains( "base_bom_id" ) )
        variant.base_bom_id = wxString::FromUTF8( j["base_bom_id"].get<std::string>() );

    if( j.contains( "configs" ) && j["configs"].is_array() )
    {
        for( const auto& cfgJson : j["configs"] )
        {
            PLM_VARIANT_CONFIG cfg;
            cfg.config_id = wxString::FromUTF8( cfgJson.value( "config_id", "" ) );
            cfg.name = wxString::FromUTF8( cfgJson.value( "name", "" ) );
            cfg.description = wxString::FromUTF8( cfgJson.value( "description", "" ) );
            cfg.active = cfgJson.value( "active", true );

            if( cfgJson.contains( "overrides" ) && cfgJson["overrides"].is_array() )
            {
                for( const auto& ovrJson : cfgJson["overrides"] )
                {
                    PLM_VARIANT_BOM_OVERRIDE ovr;
                    ovr.reference = wxString::FromUTF8( ovrJson.value( "reference", "" ) );
                    ovr.override_type = static_cast<PLM_VARIANT_OVERRIDE_TYPE>(
                            ovrJson.value( "override_type", 0 ) );
                    ovr.original_part_id =
                            wxString::FromUTF8( ovrJson.value( "original_part_id", "" ) );
                    ovr.replacement_part_id =
                            wxString::FromUTF8( ovrJson.value( "replacement_part_id", "" ) );
                    ovr.new_value = wxString::FromUTF8( ovrJson.value( "new_value", "" ) );
                    cfg.overrides.push_back( ovr );
                }
            }

            variant.configs.push_back( cfg );
        }
    }

    return variant;
}


PLM_VARIANT_RESULT PLM_GIT_PLUGIN::CreateVariant( const wxString&           aProjectId,
                                                    const PLM_VARIANT_CONFIG& aConfig )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !EnsureClone() )
    {
        return PLM_VARIANT_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                             wxS( "Not ready" ) );
    }

    wxString projectDir = GetProjectPath( aProjectId );
    wxString variantsPath = projectDir + wxFileName::GetPathSeparator() + wxS( "variants.json" );

    nlohmann::json j;
    ReadJsonFile( variantsPath, j );

    if( !j.contains( "configs" ) )
        j["configs"] = nlohmann::json::array();

    nlohmann::json cfgJson;
    cfgJson["config_id"] = aConfig.config_id.ToStdString();
    cfgJson["name"] = aConfig.name.ToStdString();
    cfgJson["description"] = aConfig.description.ToStdString();
    cfgJson["active"] = aConfig.active;
    cfgJson["overrides"] = nlohmann::json::array();

    for( const PLM_VARIANT_BOM_OVERRIDE& ovr : aConfig.overrides )
    {
        nlohmann::json ovrJson;
        ovrJson["reference"] = ovr.reference.ToStdString();
        ovrJson["override_type"] = static_cast<int>( ovr.override_type );
        ovrJson["original_part_id"] = ovr.original_part_id.ToStdString();
        ovrJson["replacement_part_id"] = ovr.replacement_part_id.ToStdString();
        ovrJson["new_value"] = ovr.new_value.ToStdString();
        cfgJson["overrides"].push_back( ovrJson );
    }

    j["configs"].push_back( cfgJson );

    if( !WriteJsonFile( variantsPath, j ) )
    {
        return PLM_VARIANT_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                             wxS( "Failed to write variants" ) );
    }

    wxString output;
    wxString relPath = variantsPath.Mid( m_localClonePath.length() + 1 );
    RunGitCommand( { wxS( "add" ), relPath }, output, m_localClonePath );
    RunGitCommand( { wxS( "commit" ), wxS( "-m" ),
                     wxS( "Add variant " ) + aConfig.name },
                   output, m_localClonePath );

    return PLM_VARIANT_RESULT::Success( aConfig.config_id );
}


PLM_VARIANT_RESULT PLM_GIT_PLUGIN::UpdateVariant( const PLM_VARIANT_CONFIG& aConfig )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !EnsureClone() )
    {
        return PLM_VARIANT_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                             wxS( "Not ready" ) );
    }

    // We need to find which project this variant belongs to by scanning
    wxString output;
    RunGitCommand( { wxS( "ls-files" ), wxS( "**/variants.json" ) }, output, m_localClonePath );

    wxStringTokenizer tokenizer( output, wxS( "\n" ) );

    while( tokenizer.HasMoreTokens() )
    {
        wxString relPath = tokenizer.GetNextToken().Trim();

        if( relPath.IsEmpty() )
            continue;

        wxString fullPath = m_localClonePath + wxFileName::GetPathSeparator() + relPath;
        nlohmann::json j;

        if( !ReadJsonFile( fullPath, j ) || !j.contains( "configs" ) )
            continue;

        for( auto& cfgJson : j["configs"] )
        {
            if( cfgJson.value( "config_id", "" ) == aConfig.config_id.ToStdString() )
            {
                cfgJson["name"] = aConfig.name.ToStdString();
                cfgJson["description"] = aConfig.description.ToStdString();
                cfgJson["active"] = aConfig.active;
                cfgJson["overrides"] = nlohmann::json::array();

                for( const PLM_VARIANT_BOM_OVERRIDE& ovr : aConfig.overrides )
                {
                    nlohmann::json ovrJson;
                    ovrJson["reference"] = ovr.reference.ToStdString();
                    ovrJson["override_type"] = static_cast<int>( ovr.override_type );
                    ovrJson["original_part_id"] = ovr.original_part_id.ToStdString();
                    ovrJson["replacement_part_id"] = ovr.replacement_part_id.ToStdString();
                    ovrJson["new_value"] = ovr.new_value.ToStdString();
                    cfgJson["overrides"].push_back( ovrJson );
                }

                WriteJsonFile( fullPath, j );

                wxString gitOutput;
                RunGitCommand( { wxS( "add" ), relPath }, gitOutput, m_localClonePath );
                RunGitCommand( { wxS( "commit" ), wxS( "-m" ),
                                 wxS( "Update variant " ) + aConfig.name },
                               gitOutput, m_localClonePath );

                return PLM_VARIANT_RESULT::Success( aConfig.config_id );
            }
        }
    }

    return PLM_VARIANT_RESULT::Failure( PLM_ERROR_CODE::ERR_FILE_NOT_FOUND,
                                         wxS( "Variant config not found" ) );
}


bool PLM_GIT_PLUGIN::DeleteVariant( const wxString& aConfigId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !EnsureClone() )
        return false;

    wxString output;
    RunGitCommand( { wxS( "ls-files" ), wxS( "**/variants.json" ) }, output, m_localClonePath );

    wxStringTokenizer tokenizer( output, wxS( "\n" ) );

    while( tokenizer.HasMoreTokens() )
    {
        wxString relPath = tokenizer.GetNextToken().Trim();

        if( relPath.IsEmpty() )
            continue;

        wxString fullPath = m_localClonePath + wxFileName::GetPathSeparator() + relPath;
        nlohmann::json j;

        if( !ReadJsonFile( fullPath, j ) || !j.contains( "configs" ) )
            continue;

        auto& configs = j["configs"];

        for( auto it = configs.begin(); it != configs.end(); ++it )
        {
            if( (*it).value( "config_id", "" ) == aConfigId.ToStdString() )
            {
                configs.erase( it );
                WriteJsonFile( fullPath, j );

                wxString gitOutput;
                RunGitCommand( { wxS( "add" ), relPath }, gitOutput, m_localClonePath );
                RunGitCommand( { wxS( "commit" ), wxS( "-m" ),
                                 wxS( "Delete variant " ) + aConfigId },
                               gitOutput, m_localClonePath );

                return true;
            }
        }
    }

    return false;
}


//-------------------------------------------------------------------------
// CI/CD Interface
//-------------------------------------------------------------------------

std::vector<PLM_PIPELINE_RUN> PLM_GIT_PLUGIN::GetPipelineRuns(
        const wxString& aProjectId, const PLM_PIPELINE_FILTER& aFilter )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_PIPELINE_RUN> runs;

    if( !m_forgeProvider )
        return runs;

    // Get pipeline status for recent commits
    wxString output;

    if( !m_isAuthenticated || !EnsureClone() )
        return runs;

    wxString logFormat = wxS( "%H" );
    std::vector<wxString> args = { wxS( "log" ), wxS( "--pretty=format:" ) + logFormat };

    if( !aFilter.branch.IsEmpty() )
        args.push_back( aFilter.branch );

    args.push_back( wxString::Format( wxS( "-n%zu" ), aFilter.limit ) );

    int rc = RunGitCommand( args, output, m_localClonePath );

    if( rc != 0 )
        return runs;

    wxStringTokenizer lineTokenizer( output, wxS( "\n" ) );

    while( lineTokenizer.HasMoreTokens() )
    {
        wxString sha = lineTokenizer.GetNextToken().Trim();

        if( sha.IsEmpty() )
            continue;

        PLM_PIPELINE_STATUS status = m_forgeProvider->GetPipelineStatus( sha );

        PLM_PIPELINE_RUN run;
        run.run_id = sha.Left( 8 );
        run.project_id = aProjectId;
        run.commit_sha = sha;
        run.web_url = status.web_url;

        switch( status.overall_state )
        {
        case PLM_PIPELINE_STATE::PASSED:    run.status = PLM_CI_RUN_STATUS::SUCCEEDED; break;
        case PLM_PIPELINE_STATE::FAILED:    run.status = PLM_CI_RUN_STATUS::FAILED; break;
        case PLM_PIPELINE_STATE::RUNNING:   run.status = PLM_CI_RUN_STATUS::RUNNING; break;
        case PLM_PIPELINE_STATE::QUEUED:    run.status = PLM_CI_RUN_STATUS::QUEUED; break;
        case PLM_PIPELINE_STATE::CANCELLED: run.status = PLM_CI_RUN_STATUS::CANCELLED; break;
        default:                            run.status = PLM_CI_RUN_STATUS::UNKNOWN; break;
        }

        for( const PLM_PIPELINE_CHECK& check : status.checks )
        {
            PLM_CI_JOB job;
            job.name = check.name;
            job.log_url = check.details_url;
            job.started_at = check.started_at;
            job.completed_at = check.completed_at;

            switch( check.state )
            {
            case PLM_PIPELINE_STATE::PASSED:    job.status = PLM_CI_RUN_STATUS::SUCCEEDED; break;
            case PLM_PIPELINE_STATE::FAILED:    job.status = PLM_CI_RUN_STATUS::FAILED; break;
            case PLM_PIPELINE_STATE::RUNNING:   job.status = PLM_CI_RUN_STATUS::RUNNING; break;
            default:                            job.status = PLM_CI_RUN_STATUS::UNKNOWN; break;
            }

            run.jobs.push_back( job );
        }

        runs.push_back( run );
    }

    return runs;
}


PLM_PIPELINE_RUN PLM_GIT_PLUGIN::GetPipelineRun( const wxString& aProjectId,
                                                    const wxString& aRunId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_PIPELINE_FILTER filter;
    filter.limit = 50;

    std::vector<PLM_PIPELINE_RUN> runs = GetPipelineRuns( aProjectId, filter );

    for( const PLM_PIPELINE_RUN& run : runs )
    {
        if( run.run_id == aRunId || run.commit_sha.StartsWith( aRunId ) )
            return run;
    }

    return PLM_PIPELINE_RUN();
}


bool PLM_GIT_PLUGIN::DownloadCIArtifact( const wxString& aProjectId,
                                           const wxString& aRunId,
                                           const wxString& aArtifactId,
                                           const wxString& aLocalPath )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_forgeProvider )
        return false;

    return m_forgeProvider->DownloadArtifact( aArtifactId, aLocalPath );
}


PLM_CI_RUN_STATUS PLM_GIT_PLUGIN::GetCommitPipelineStatus( const wxString& aProjectId,
                                                             const wxString& aCommitSha )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_forgeProvider )
        return PLM_CI_RUN_STATUS::UNKNOWN;

    PLM_PIPELINE_STATUS status = m_forgeProvider->GetPipelineStatus( aCommitSha );

    switch( status.overall_state )
    {
    case PLM_PIPELINE_STATE::PASSED:    return PLM_CI_RUN_STATUS::SUCCEEDED;
    case PLM_PIPELINE_STATE::FAILED:    return PLM_CI_RUN_STATUS::FAILED;
    case PLM_PIPELINE_STATE::RUNNING:   return PLM_CI_RUN_STATUS::RUNNING;
    case PLM_PIPELINE_STATE::QUEUED:    return PLM_CI_RUN_STATUS::QUEUED;
    case PLM_PIPELINE_STATE::CANCELLED: return PLM_CI_RUN_STATUS::CANCELLED;
    default:                            return PLM_CI_RUN_STATUS::UNKNOWN;
    }
}


//-------------------------------------------------------------------------
// Audit Extended Interface
//-------------------------------------------------------------------------

PLM_AUDIT_QUERY_RESULT PLM_GIT_PLUGIN::GetAuditLog( const PLM_AUDIT_FILTER& aFilter )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_AUDIT_QUERY_RESULT result;

    if( !m_isAuthenticated || !EnsureClone() )
    {
        result.error = PLM_ERROR_CODE::ERR_AUTH_REQUIRED;
        result.error_message = wxS( "Not ready" );
        return result;
    }

    std::vector<wxString> args = { wxS( "log" ),
                                   wxS( "--pretty=format:%H|%an|%ae|%at|%s" ) };

    if( !aFilter.user_id.IsEmpty() )
    {
        args.push_back( wxS( "--author=" ) + aFilter.user_id );
    }

    if( aFilter.start_time_ms > 0 )
    {
        wxDateTime startTime( static_cast<time_t>( aFilter.start_time_ms / 1000 ) );
        args.push_back( wxS( "--after=" ) + startTime.FormatISOCombined() );
    }

    if( aFilter.end_time_ms > 0 )
    {
        wxDateTime endTime( static_cast<time_t>( aFilter.end_time_ms / 1000 ) );
        args.push_back( wxS( "--before=" ) + endTime.FormatISOCombined() );
    }

    if( !aFilter.project_id.IsEmpty() )
    {
        wxString relativePath = FolderIdToPath( aFilter.project_id );
        args.push_back( wxS( "--" ) );
        args.push_back( relativePath );
    }

    args.push_back( wxString::Format( wxS( "-n%zu" ), aFilter.limit ) );

    if( aFilter.offset > 0 )
        args.push_back( wxString::Format( wxS( "--skip=%zu" ), aFilter.offset ) );

    wxString output;
    int rc = RunGitCommand( args, output, m_localClonePath );

    if( rc != 0 )
        return result;

    wxStringTokenizer lineTokenizer( output, wxS( "\n" ) );

    while( lineTokenizer.HasMoreTokens() )
    {
        wxString line = lineTokenizer.GetNextToken().Trim();

        if( line.IsEmpty() )
            continue;

        wxStringTokenizer fieldTokenizer( line, wxS( "|" ) );

        if( fieldTokenizer.CountTokens() < 5 )
            continue;

        PLM_AUDIT_EVENT event;
        wxString sha = fieldTokenizer.GetNextToken();
        event.event_id = sha;
        event.user_name = fieldTokenizer.GetNextToken();
        event.user_id = fieldTokenizer.GetNextToken();

        wxString timestamp = fieldTokenizer.GetNextToken();
        long ts;

        if( timestamp.ToLong( &ts ) )
            event.timestamp_ms = static_cast<int64_t>( ts ) * 1000;

        wxString subject = fieldTokenizer.GetNextToken();
        event.result = subject;

        if( subject.Contains( wxS( "Lock" ) ) )
            event.event_type = PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED;
        else if( subject.Contains( wxS( "Unlock" ) ) )
            event.event_type = PLM_AUDIT_EVENT_TYPE::LOCK_RELEASED;
        else if( subject.Contains( wxS( "ECO" ) ) )
            event.event_type = PLM_AUDIT_EVENT_TYPE::ECO_SUBMITTED;
        else if( subject.Contains( wxS( "release" ) ) || subject.Contains( wxS( "Release" ) ) )
            event.event_type = PLM_AUDIT_EVENT_TYPE::STATE_CHANGED;
        else
            event.event_type = PLM_AUDIT_EVENT_TYPE::REVISION_CREATED;

        event.success = true;
        result.events.push_back( event );
    }

    result.total_count = result.events.size();
    result.has_more = result.events.size() >= aFilter.limit;
    return result;
}


PLM_AUDIT_QUERY_RESULT PLM_GIT_PLUGIN::GetProjectAuditLog( const wxString& aProjectId,
                                                              size_t          aLimit )
{
    PLM_AUDIT_FILTER filter;
    filter.project_id = aProjectId;
    filter.limit = aLimit;
    return GetAuditLog( filter );
}


//-------------------------------------------------------------------------
// File Operations Extended Interface
//-------------------------------------------------------------------------

PLM_CHECKOUT_RESULT PLM_GIT_PLUGIN::CreateFolder( const wxString& aParentFolderId,
                                                    const wxString& aName )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_CHECKOUT_RESULT result;

    if( !m_isAuthenticated || !EnsureClone() )
    {
        result.success = false;
        result.error_message = wxS( "Not ready" );
        return result;
    }

    wxString parentPath = m_localClonePath;

    if( !aParentFolderId.IsEmpty() )
    {
        wxString relativePath = FolderIdToPath( aParentFolderId );
        parentPath += wxFileName::GetPathSeparator() + relativePath;
    }

    wxString newFolderPath = parentPath + wxFileName::GetPathSeparator() + aName;

    if( wxDirExists( newFolderPath ) )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_FILE_EXISTS;
        result.error_message = wxS( "Folder already exists" );
        return result;
    }

    if( !wxMkdir( newFolderPath ) )
    {
        result.success = false;
        result.error_message = wxS( "Failed to create directory" );
        return result;
    }

    // Add .gitkeep so the empty directory is tracked
    wxString gitkeepPath = newFolderPath + wxFileName::GetPathSeparator() + wxS( ".gitkeep" );
    wxFile gitkeep( gitkeepPath, wxFile::write );

    if( gitkeep.IsOpened() )
        gitkeep.Close();

    wxString relativeNewPath = newFolderPath.Mid( m_localClonePath.length() + 1 );

    wxString output;
    RunGitCommand( { wxS( "add" ), relativeNewPath }, output, m_localClonePath );
    RunGitCommand( { wxS( "commit" ), wxS( "-m" ),
                     wxS( "Create folder " ) + aName },
                   output, m_localClonePath );

    result.success = true;
    result.file_info.id = PathToFolderId( relativeNewPath );
    result.file_info.name = aName;
    result.local_path = newFolderPath;
    return result;
}


std::vector<wxString> PLM_GIT_PLUGIN::GetModifiedFiles( const wxString& aLocalPath )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<wxString> modifiedFiles;

    if( !m_isAuthenticated || !EnsureClone() )
        return modifiedFiles;

    wxString workDir = aLocalPath.IsEmpty() ? m_localClonePath : aLocalPath;

    wxString output;
    int rc = RunGitCommand( { wxS( "status" ), wxS( "--porcelain" ) }, output, workDir );

    if( rc != 0 )
        return modifiedFiles;

    wxStringTokenizer lineTokenizer( output, wxS( "\n" ) );

    while( lineTokenizer.HasMoreTokens() )
    {
        wxString line = lineTokenizer.GetNextToken();

        if( line.length() < 4 )
            continue;

        // git status --porcelain format: XY filename
        wxString filePath = line.Mid( 3 ).Trim();

        if( !filePath.IsEmpty() )
            modifiedFiles.push_back( filePath );
    }

    return modifiedFiles;
}


//-------------------------------------------------------------------------
// Preview Cache Interface
//-------------------------------------------------------------------------

PLM_PREVIEW_CACHE* PLM_GIT_PLUGIN::GetPreviewCache()
{
    return nullptr;
}


//-------------------------------------------------------------------------
// Shared Library Interface
//-------------------------------------------------------------------------

void PLM_GIT_PLUGIN::ParseSharedLibrariesConfig( const PLM_CREDENTIALS& aCredentials )
{
    m_sharedLibrariesCache.clear();
    m_sharedLibArtifactPaths.clear();

    auto it = aCredentials.properties.find( wxS( "shared_libraries" ) );

    if( it == aCredentials.properties.end() || it->second.IsEmpty() )
        return;

    nlohmann::json doc;

    try
    {
        doc = nlohmann::json::parse( it->second.ToStdString() );
    }
    catch( const nlohmann::json::exception& )
    {
        wxLogWarning( wxS( "PLM Git: shared_libraries config is not valid JSON; ignoring" ) );
        return;
    }

    if( !doc.is_array() )
        return;

    for( const auto& entry : doc )
    {
        if( !entry.is_object() )
            continue;

        std::string libId = entry.value( "lib_id", std::string() );
        std::string url = entry.value( "url", std::string() );
        std::string typeStr = entry.value( "type", std::string( "symbol" ) );

        if( libId.empty() || url.empty() )
            continue;

        PLM_LIB_REFERENCE ref;
        ref.lib_id = wxString::FromUTF8( libId );
        ref.lib_name = ref.lib_id;
        ref.server_url = wxString::FromUTF8( url );
        ref.lib_type = PLM_LIB_REFERENCE::ParseType( wxString::FromUTF8( typeStr ) );

        m_sharedLibrariesCache.push_back( ref );

        std::string artifact = entry.value( "artifact_path", std::string() );

        if( !artifact.empty() )
            m_sharedLibArtifactPaths[ref.lib_id] = wxString::FromUTF8( artifact );
    }
}


const PLM_LIB_REFERENCE* PLM_GIT_PLUGIN::FindSharedLibrary( const wxString& aLibId ) const
{
    for( const auto& ref : m_sharedLibrariesCache )
    {
        if( ref.lib_id == aLibId )
            return &ref;
    }

    return nullptr;
}


wxString PLM_GIT_PLUGIN::AuthenticatedLibraryUrl( const wxString& aRawUrl )
{
    // SSH and local paths are passed through unchanged; the existing main-
    // repo flow uses ssh-agent / GIT_SSH_COMMAND or a literal path. Only
    // HTTPS URLs need credential injection.
    if( !aRawUrl.StartsWith( wxS( "https://" ) ) && !aRawUrl.StartsWith( wxS( "http://" ) ) )
        return aRawUrl;

    return m_authHelper.BuildAuthenticatedUrl( aRawUrl );
}


std::vector<PLM_LIB_REFERENCE>
PLM_GIT_PLUGIN::GetAvailableLibraries( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );
    (void) aProjectId;
    return m_sharedLibrariesCache;
}


std::vector<PLM_LIB_VERSION_INFO>
PLM_GIT_PLUGIN::GetLibraryVersions( const wxString& aLibId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_LIB_VERSION_INFO> versions;

    const PLM_LIB_REFERENCE* ref = FindSharedLibrary( aLibId );

    if( !ref )
        return versions;

    wxString url = AuthenticatedLibraryUrl( ref->server_url );
    wxString output;

    int rc = RunGitCommand( { wxS( "ls-remote" ), wxS( "--tags" ), url }, output, wxEmptyString );

    if( rc != 0 )
    {
        wxLogWarning( wxS( "PLM Git: ls-remote --tags failed for %s" ), aLibId );
        return versions;
    }

    wxArrayString lines = wxSplit( output, '\n' );

    for( const wxString& rawLine : lines )
    {
        wxString line = rawLine;
        line.Trim( true ).Trim( false );

        if( line.IsEmpty() )
            continue;

        wxString shaPart = line.BeforeFirst( '\t' );
        wxString refPart = line.AfterFirst( '\t' );

        // Skip peeled tag entries (^{}) which would otherwise duplicate the
        // tag with its underlying commit SHA.
        if( refPart.EndsWith( wxS( "^{}" ) ) )
            continue;

        if( !refPart.StartsWith( wxS( "refs/tags/" ) ) )
            continue;

        PLM_LIB_VERSION_INFO info;
        info.lib_id = aLibId;
        info.lib_name = ref->lib_name;
        info.revision = shaPart;
        info.revision_display = refPart.Mid( wxString( wxS( "refs/tags/" ) ).Length() );
        versions.push_back( info );
    }

    return versions;
}


wxString PLM_GIT_PLUGIN::GetLatestLibraryRevision( const wxString& aLibId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    const PLM_LIB_REFERENCE* ref = FindSharedLibrary( aLibId );

    if( !ref )
        return wxEmptyString;

    wxString url = AuthenticatedLibraryUrl( ref->server_url );
    wxString output;

    int rc = RunGitCommand( { wxS( "ls-remote" ), url, wxS( "HEAD" ) }, output, wxEmptyString );

    if( rc != 0 )
        return wxEmptyString;

    output.Trim( true ).Trim( false );
    return output.BeforeFirst( '\t' );
}


PLM_LIB_DOWNLOAD_RESULT PLM_GIT_PLUGIN::DownloadLibrary( const wxString& aLibId,
                                                          PLM_LIB_TYPE aLibType,
                                                          const wxString& aRequestedRevision,
                                                          const wxString& aLocalPath )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_LIB_DOWNLOAD_RESULT result;

    if( aLibType != PLM_LIB_TYPE::SYMBOL )
    {
        result.error_message = wxS( "Slice 1 supports symbol libraries only" );
        return result;
    }

    const PLM_LIB_REFERENCE* ref = FindSharedLibrary( aLibId );

    if( !ref )
    {
        result.error_message = wxString::Format( wxS( "Unknown library id: %s" ), aLibId );
        return result;
    }

    auto pathIt = m_sharedLibArtifactPaths.find( aLibId );

    if( pathIt == m_sharedLibArtifactPaths.end() || pathIt->second.IsEmpty() )
    {
        result.error_message = wxS( "Library has no artifact_path configured" );
        return result;
    }

    wxString resolvedRevision = aRequestedRevision;

    if( resolvedRevision.IsEmpty() )
        resolvedRevision = GetLatestLibraryRevision( aLibId );

    if( resolvedRevision.IsEmpty() )
    {
        result.error_message = wxS( "Could not resolve a revision to fetch" );
        return result;
    }

    // Clone (or update an existing) cache repo dedicated to this lib id, then
    // copy the configured artifact out to aLocalPath. Lives under the same
    // <userdata>/plm-libs/ root as PLM_LIB_PROVIDER's leaf trees (symbols/
    // footprints/3dmodels/other) but in its own "git-repos" subdir to avoid
    // colliding with any future PLM_LIB_TYPE leaf name. A dedicated
    // SetCacheRootDir setter on PLM_LIB_PROVIDER is deferred to a follow-up.
    wxFileName libRepoRoot( wxStandardPaths::Get().GetUserDataDir(), wxEmptyString );
    libRepoRoot.AppendDir( wxS( "plm-libs" ) );
    libRepoRoot.AppendDir( wxS( "git-repos" ) );
    libRepoRoot.AppendDir( aLibId );
    wxFileName::Mkdir( libRepoRoot.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );

    wxString repoDir = libRepoRoot.GetPath();
    // Use the raw URL for persistent operations (clone, set-url) so HTTPS
    // credentials don't get baked into .git/config on disk. Auth for clone
    // and fetch flows through m_authHelper's env-var setup configured at
    // Authenticate(). ls-remote, which doesn't persist, can use the
    // authenticated form via AuthenticatedLibraryUrl elsewhere.
    const wxString& rawUrl = ref->server_url;
    wxString output;

    bool repoReady = wxDirExists( repoDir + wxFileName::GetPathSeparator() + wxS( ".git" ) );

    if( repoReady )
    {
        // If the configured URL changed since the cache was first cloned, the
        // existing repo points at a stale remote. Check origin and rewrite
        // so subsequent fetches don't silently serve the wrong source.
        wxString currentOrigin;

        if( RunGitCommand( { wxS( "remote" ), wxS( "get-url" ), wxS( "origin" ) },
                           currentOrigin, repoDir ) == 0 )
        {
            currentOrigin.Trim( true ).Trim( false );

            if( currentOrigin != rawUrl )
            {
                RunGitCommand( { wxS( "remote" ), wxS( "set-url" ), wxS( "origin" ), rawUrl },
                               output, repoDir );
            }
        }

        // Best-effort fetch of new tags; ignore failure so an offline session
        // can still serve a previously-fetched revision from cache.
        RunGitCommand( { wxS( "fetch" ), wxS( "--tags" ), wxS( "--prune" ), wxS( "origin" ) },
                       output, repoDir );
    }
    else
    {
        if( RunGitCommand( { wxS( "clone" ), rawUrl, repoDir }, output, wxEmptyString ) != 0 )
        {
            result.error_message = wxString::Format(
                    wxS( "Library clone failed: %s" ), output.Trim( true ).Trim( false ) );
            return result;
        }
    }

    // --detach makes the intent explicit (we don't want a local branch); also
    // works for tag names, SHAs, and branch refs uniformly.
    if( RunGitCommand( { wxS( "checkout" ), wxS( "--detach" ), resolvedRevision },
                       output, repoDir ) != 0 )
    {
        result.error_message = wxString::Format(
                wxS( "checkout %s failed: %s" ), resolvedRevision,
                output.Trim( true ).Trim( false ) );
        return result;
    }

    wxString srcPath = repoDir + wxFileName::GetPathSeparator() + pathIt->second;

    if( !wxFileName::FileExists( srcPath ) )
    {
        result.error_message = wxString::Format(
                wxS( "Artifact not found in lib repo at %s" ), pathIt->second );
        return result;
    }

    // Ensure parent dir exists; aLocalPath is the destination FILE per the
    // PLM_LIB_PROVIDER contract (provider checks wxFileName::FileExists).
    // Skip mkdir when GetPath() is empty (bare filename in CWD) — Mkdir("")
    // would either fail silently or pick up CWD via the path's own logic.
    wxFileName destFile( aLocalPath );

    if( !destFile.GetPath().IsEmpty() )
        wxFileName::Mkdir( destFile.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );

    if( !wxCopyFile( srcPath, aLocalPath, true ) )
    {
        result.error_message = wxS( "Failed to copy artifact to destination" );
        return result;
    }

    result.success = true;
    result.revision = resolvedRevision;
    return result;
}


PLM_PLUGIN_ENTRY_POINTS( PLM_GIT_PLUGIN )
