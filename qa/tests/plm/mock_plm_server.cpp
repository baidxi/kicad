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

#include "mock_plm_server.h"

#include <ctime>


// MOCK_PLM_SERVER implementation

MOCK_PLM_SERVER::MOCK_PLM_SERVER() {}


void MOCK_PLM_SERVER::Reset()
{
    m_folders.clear();
    m_files.clear();
    m_locks.clear();
    m_revisions.clear();
    m_validUsers.clear();
    m_nextRevisionNumber = 1;

    m_authErrors.Reset();
    m_lockErrors.Reset();
    m_checkoutErrors.Reset();
    m_checkinErrors.Reset();
    m_navigationErrors.Reset();
}


void MOCK_PLM_SERVER::AddFolder( const PLM_FOLDER& aFolder )
{
    m_folders.push_back( aFolder );
}


void MOCK_PLM_SERVER::AddFile( const PLM_FILE_INFO& aFile )
{
    m_files[aFile.id] = aFile;
}


void MOCK_PLM_SERVER::AddRevision( const wxString& aFileId, const PLM_REVISION& aRevision )
{
    m_revisions[aFileId].push_back( aRevision );
}


void MOCK_PLM_SERVER::SetLock( const wxString& aFileId, const PLM_LOCK_INFO& aLock )
{
    m_locks[aFileId] = aLock;
}


std::vector<PLM_FOLDER> MOCK_PLM_SERVER::GetRootFolders() const
{
    std::vector<PLM_FOLDER> roots;

    for( const auto& folder : m_folders )
    {
        if( folder.IsRoot() )
            roots.push_back( folder );
    }

    return roots;
}


std::vector<PLM_FOLDER> MOCK_PLM_SERVER::GetChildFolders( const wxString& aParentId ) const
{
    std::vector<PLM_FOLDER> children;

    for( const auto& folder : m_folders )
    {
        if( folder.parent_id == aParentId )
            children.push_back( folder );
    }

    return children;
}


PLM_LIST_RESULT<PLM_FILE_INFO> MOCK_PLM_SERVER::GetFilesInFolder( const wxString& aFolderId,
                                                                   const PLM_LIST_OPTIONS& aOptions ) const
{
    std::vector<PLM_FILE_INFO> matching;

    for( const auto& [id, file] : m_files )
    {
        if( file.folder_id == aFolderId )
            matching.push_back( file );
    }

    int32_t total = static_cast<int32_t>( matching.size() );
    int32_t start = std::min( aOptions.offset, total );
    int32_t end = std::min( start + aOptions.limit, total );

    std::vector<PLM_FILE_INFO> page( matching.begin() + start, matching.begin() + end );
    return PLM_LIST_RESULT<PLM_FILE_INFO>( page, total, aOptions.offset, aOptions.limit );
}


PLM_FILE_INFO* MOCK_PLM_SERVER::GetFile( const wxString& aFileId )
{
    auto it = m_files.find( aFileId );
    return ( it != m_files.end() ) ? &it->second : nullptr;
}


const PLM_FILE_INFO* MOCK_PLM_SERVER::GetFile( const wxString& aFileId ) const
{
    auto it = m_files.find( aFileId );
    return ( it != m_files.end() ) ? &it->second : nullptr;
}


PLM_LOCK_INFO MOCK_PLM_SERVER::GetLockInfo( const wxString& aFileId ) const
{
    auto it = m_locks.find( aFileId );
    return ( it != m_locks.end() ) ? it->second : PLM_LOCK_INFO::Unlocked();
}


bool MOCK_PLM_SERVER::AcquireLock( const wxString& aFileId, const wxString& aUser )
{
    PLM_LOCK_INFO current = GetLockInfo( aFileId );

    if( current.is_locked && !current.locked_by_me && current.lock_owner != aUser )
        return false;

    int64_t now = static_cast<int64_t>( std::time( nullptr ) );
    m_locks[aFileId] = PLM_LOCK_INFO::Locked( aUser, now, true );
    return true;
}


bool MOCK_PLM_SERVER::ReleaseLock( const wxString& aFileId, const wxString& aUser, bool aForce )
{
    PLM_LOCK_INFO current = GetLockInfo( aFileId );

    if( !current.is_locked )
        return true;

    if( current.lock_owner != aUser && !aForce )
        return false;

    m_locks.erase( aFileId );
    return true;
}


PLM_REVISION_HISTORY MOCK_PLM_SERVER::GetRevisionHistory( const wxString& aFileId ) const
{
    PLM_REVISION_HISTORY history( aFileId );
    auto it = m_revisions.find( aFileId );

    if( it != m_revisions.end() )
        history.revisions = it->second;

    return history;
}


const PLM_REVISION* MOCK_PLM_SERVER::GetRevision( const wxString& aFileId,
                                                   const wxString& aRevisionId ) const
{
    auto it = m_revisions.find( aFileId );

    if( it == m_revisions.end() )
        return nullptr;

    for( const auto& rev : it->second )
    {
        if( rev.revision_id == aRevisionId )
            return &rev;
    }

    return nullptr;
}


wxString MOCK_PLM_SERVER::CreateRevision( const wxString& aFileId, const wxString& aAuthor,
                                           const wxString& aComment )
{
    wxString revId = wxString::Format( wxS( "%s-r%d" ), aFileId, m_nextRevisionNumber );
    wxString revNum = wxString::Format( wxS( "%d" ), m_nextRevisionNumber );
    m_nextRevisionNumber++;

    int64_t now = static_cast<int64_t>( std::time( nullptr ) );

    PLM_REVISION rev( revId, revNum, aAuthor, now, aComment, PLM_STATE_CATEGORY::EDITABLE );
    m_revisions[aFileId].insert( m_revisions[aFileId].begin(), rev );

    PLM_FILE_INFO* file = GetFile( aFileId );

    if( file )
        file->revision = revNum;

    return revNum;
}


void MOCK_PLM_SERVER::SetAuthUser( const wxString& aUsername, bool aValid )
{
    if( aValid )
        m_validUsers.insert( aUsername );
    else
        m_validUsers.erase( aUsername );
}


bool MOCK_PLM_SERVER::ValidateAuth( const wxString& aUsername, const wxString& /* aPassword */ ) const
{
    return m_validUsers.find( aUsername ) != m_validUsers.end();
}


bool MOCK_PLM_SERVER::IsValidUser( const wxString& aUsername ) const
{
    return m_validUsers.find( aUsername ) != m_validUsers.end();
}


// MOCK_FULL_PLM_PLUGIN implementation

MOCK_FULL_PLM_PLUGIN::MOCK_FULL_PLM_PLUGIN() {}


MOCK_FULL_PLM_PLUGIN::~MOCK_FULL_PLM_PLUGIN()
{
    if( m_loaded )
        OnUnload();
}


PLM_PLUGIN_INFO MOCK_FULL_PLM_PLUGIN::GetPluginInfo() const
{
    PLM_PLUGIN_INFO info;
    info.plugin_id = wxS( "mock-full-plm" );
    info.display_name = wxS( "Mock Full PLM Server" );
    info.version = PLM_VERSION( 1, 0, 0 );
    info.vendor = wxS( "KiCad QA" );
    info.description = wxS( "Full-featured mock PLM plugin for integration testing" );
    return info;
}


PLM_CAPABILITIES MOCK_FULL_PLM_PLUGIN::GetCapabilities() const
{
    PLM_CAPABILITIES caps;
    caps.supports_checkout = true;
    caps.supports_locking = true;
    caps.supports_revisions = true;
    caps.supports_search = true;
    caps.supports_attributes = false;
    return caps;
}


bool MOCK_FULL_PLM_PLUGIN::OnLoad()
{
    m_loaded = true;
    m_server.SetAuthUser( wxS( "testuser" ), true );
    m_server.SetAuthUser( wxS( "admin" ), true );
    return true;
}


void MOCK_FULL_PLM_PLUGIN::OnUnload()
{
    if( m_authenticated )
        Logout();

    m_loaded = false;
}


PLM_AUTH_RESULT MOCK_FULL_PLM_PLUGIN::Authenticate( const PLM_CREDENTIALS& aCredentials )
{
    if( m_server.AuthErrors().ShouldFail() )
    {
        return PLM_AUTH_RESULT::Failure( m_server.AuthErrors().error_code,
                                         m_server.AuthErrors().error_message );
    }

    if( !m_server.ValidateAuth( aCredentials.username, aCredentials.password.Get() ) )
    {
        return PLM_AUTH_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_FAILED,
                                         wxS( "Invalid username or password" ) );
    }

    int64_t now = static_cast<int64_t>( std::time( nullptr ) );
    int64_t expires = now + 3600;

    m_session.token = wxString::Format( wxS( "mock-token-%lld" ), static_cast<long long>( now ) );
    m_session.expires_at = expires;
    m_session.server_url = aCredentials.server_url;
    m_session.user_info = PLM_USER_INFO( aCredentials.username,
                                         aCredentials.username,
                                         aCredentials.username + wxS( "@example.com" ) );
    m_authenticated = true;

    return PLM_AUTH_RESULT::Success( m_session.token.Get(), expires );
}


PLM_AUTH_RESULT MOCK_FULL_PLM_PLUGIN::RefreshSession()
{
    if( !m_authenticated )
    {
        return PLM_AUTH_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                         wxS( "Not authenticated" ) );
    }

    int64_t now = static_cast<int64_t>( std::time( nullptr ) );
    int64_t expires = now + 3600;

    m_session.token = wxString::Format( wxS( "mock-token-%lld" ), static_cast<long long>( now ) );
    m_session.expires_at = expires;

    return PLM_AUTH_RESULT::Success( m_session.token.Get(), expires );
}


bool MOCK_FULL_PLM_PLUGIN::Logout()
{
    m_session.Clear();
    m_authenticated = false;
    m_checkedOutFiles.clear();
    return true;
}


bool MOCK_FULL_PLM_PLUGIN::IsAuthenticated() const
{
    return m_authenticated;
}


PLM_SESSION MOCK_FULL_PLM_PLUGIN::GetCurrentSession() const
{
    return m_session;
}


std::vector<PLM_FOLDER> MOCK_FULL_PLM_PLUGIN::GetRootFolders()
{
    if( !m_authenticated )
        return {};

    if( m_server.NavigationErrors().ShouldFail() )
        return {};

    return m_server.GetRootFolders();
}


std::vector<PLM_FOLDER> MOCK_FULL_PLM_PLUGIN::GetChildFolders( const wxString& aFolderId )
{
    if( !m_authenticated )
        return {};

    return m_server.GetChildFolders( aFolderId );
}


PLM_FOLDER_PATH MOCK_FULL_PLM_PLUGIN::GetFolderPath( const wxString& /* aFolderId */ )
{
    return {};
}


PLM_LIST_RESULT<PLM_FILE_INFO> MOCK_FULL_PLM_PLUGIN::ListFiles( const wxString& aFolderId,
                                                                 const PLM_LIST_OPTIONS& aOptions )
{
    if( !m_authenticated )
        return PLM_LIST_RESULT<PLM_FILE_INFO>();

    return m_server.GetFilesInFolder( aFolderId, aOptions );
}


PLM_LIST_RESULT<PLM_FILE_INFO> MOCK_FULL_PLM_PLUGIN::SearchFiles( const wxString& /* aQuery */,
                                                                   const PLM_LIST_OPTIONS& /* aOptions */ )
{
    return PLM_LIST_RESULT<PLM_FILE_INFO>();
}


PLM_CHECKOUT_RESULT MOCK_FULL_PLM_PLUGIN::Checkout( const wxString& aProjectId,
                                                     const wxString& aLocalPath,
                                                     const PLM_CHECKOUT_OPTIONS& aOptions )
{
    if( !m_authenticated )
    {
        return PLM_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                             wxS( "Not authenticated" ) );
    }

    if( m_server.CheckoutErrors().ShouldFail() )
    {
        return PLM_CHECKOUT_RESULT::Failure( m_server.CheckoutErrors().error_code,
                                             m_server.CheckoutErrors().error_message );
    }

    const PLM_FILE_INFO* file = m_server.GetFile( aProjectId );

    if( !file )
    {
        return PLM_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_FILE_NOT_FOUND,
                                             wxS( "File not found" ) );
    }

    if( aOptions.acquire_lock )
    {
        if( !m_server.AcquireLock( aProjectId, m_session.user_info.user_id ) )
        {
            PLM_LOCK_INFO lockInfo = m_server.GetLockInfo( aProjectId );
            return PLM_CHECKOUT_RESULT::Failure(
                    PLM_ERROR_CODE::ERR_LOCK_CONFLICT,
                    wxString::Format( wxS( "File locked by %s" ), lockInfo.lock_owner ) );
        }
    }

    m_checkedOutFiles[aLocalPath] = aProjectId;

    PLM_FILE_INFO resultInfo = *file;
    resultInfo.lock_owner = m_session.user_info.user_id;

    return PLM_CHECKOUT_RESULT::Success( aLocalPath, resultInfo );
}


PLM_CHECKIN_RESULT MOCK_FULL_PLM_PLUGIN::Checkin( const wxString& aLocalPath,
                                                   const PLM_CHECKIN_OPTIONS& aOptions )
{
    if( !m_authenticated )
    {
        return PLM_CHECKIN_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                            wxS( "Not authenticated" ) );
    }

    auto it = m_checkedOutFiles.find( aLocalPath );

    if( it == m_checkedOutFiles.end() )
    {
        return PLM_CHECKIN_RESULT::Failure( PLM_ERROR_CODE::ERR_FILE_NOT_FOUND,
                                            wxS( "File not checked out" ) );
    }

    if( m_server.CheckinErrors().ShouldFail() )
    {
        return PLM_CHECKIN_RESULT::Failure( m_server.CheckinErrors().error_code,
                                            m_server.CheckinErrors().error_message );
    }

    wxString projectId = it->second;
    PLM_FILE_INFO* file = m_server.GetFile( projectId );

    if( !file )
    {
        return PLM_CHECKIN_RESULT::Failure( PLM_ERROR_CODE::ERR_FILE_NOT_FOUND,
                                            wxS( "File not found in server" ) );
    }

    wxString newRev = m_server.CreateRevision( projectId, m_session.user_info.user_id,
                                                aOptions.revision_comment );

    if( aOptions.release_lock )
        m_server.ReleaseLock( projectId, m_session.user_info.user_id );

    if( !aOptions.keep_local )
        m_checkedOutFiles.erase( it );

    PLM_FILE_INFO resultInfo = *file;
    resultInfo.revision = newRev;

    if( aOptions.release_lock )
        resultInfo.lock_owner.Clear();

    return PLM_CHECKIN_RESULT::Success( newRev, resultInfo );
}


PLM_FILE_INFO MOCK_FULL_PLM_PLUGIN::GetFileInfo( const wxString& aProjectId )
{
    const PLM_FILE_INFO* file = m_server.GetFile( aProjectId );
    return file ? *file : PLM_FILE_INFO();
}


PLM_CANCEL_CHECKOUT_RESULT MOCK_FULL_PLM_PLUGIN::CancelCheckout( const wxString& aLocalPath )
{
    auto it = m_checkedOutFiles.find( aLocalPath );

    if( it == m_checkedOutFiles.end() )
    {
        return PLM_CANCEL_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_FILE_NOT_FOUND,
                                                     wxS( "Not checked out" ) );
    }

    wxString projectId = it->second;
    m_server.ReleaseLock( projectId, m_session.user_info.user_id );
    m_checkedOutFiles.erase( it );
    return PLM_CANCEL_CHECKOUT_RESULT::Success();
}


PLM_LOCK_RESULT MOCK_FULL_PLM_PLUGIN::Lock( const PLM_LOCK_REQUEST& aRequest )
{
    if( !m_authenticated )
    {
        return PLM_LOCK_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                         wxS( "Not authenticated" ) );
    }

    if( m_server.LockErrors().ShouldFail() )
    {
        return PLM_LOCK_RESULT::Failure( m_server.LockErrors().error_code,
                                         m_server.LockErrors().error_message );
    }

    PLM_LOCK_INFO currentLock = m_server.GetLockInfo( aRequest.project_id );

    if( currentLock.is_locked && currentLock.lock_owner != m_session.user_info.user_id )
    {
        if( !aRequest.force )
            return PLM_LOCK_RESULT::Conflict( currentLock );

        m_server.ReleaseLock( aRequest.project_id, currentLock.lock_owner, true );
    }

    if( m_server.AcquireLock( aRequest.project_id, m_session.user_info.user_id ) )
    {
        PLM_LOCK_INFO newLock = m_server.GetLockInfo( aRequest.project_id );
        newLock.locked_by_me = true;
        return PLM_LOCK_RESULT::Success( newLock );
    }

    return PLM_LOCK_RESULT::Failure( PLM_ERROR_CODE::ERR_LOCK_CONFLICT, wxS( "Failed to acquire lock" ) );
}


PLM_LOCK_RESULT MOCK_FULL_PLM_PLUGIN::Unlock( const PLM_UNLOCK_REQUEST& aRequest )
{
    if( !m_authenticated )
    {
        return PLM_LOCK_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                         wxS( "Not authenticated" ) );
    }

    PLM_LOCK_INFO currentLock = m_server.GetLockInfo( aRequest.project_id );

    if( !currentLock.is_locked )
        return PLM_LOCK_RESULT::Success( PLM_LOCK_INFO::Unlocked() );

    if( currentLock.lock_owner != m_session.user_info.user_id && !aRequest.force )
    {
        return PLM_LOCK_RESULT::Failure( PLM_ERROR_CODE::ERR_LOCK_NOT_HELD,
                                         wxS( "Lock held by another user" ),
                                         currentLock );
    }

    if( m_server.ReleaseLock( aRequest.project_id, m_session.user_info.user_id, aRequest.force ) )
        return PLM_LOCK_RESULT::Success( PLM_LOCK_INFO::Unlocked() );

    return PLM_LOCK_RESULT::Failure( PLM_ERROR_CODE::ERR_LOCK_NOT_HELD, wxS( "Failed to release lock" ) );
}


PLM_LOCK_INFO MOCK_FULL_PLM_PLUGIN::GetLockInfo( const wxString& aProjectId )
{
    PLM_LOCK_INFO info = m_server.GetLockInfo( aProjectId );

    if( info.is_locked && info.lock_owner == m_session.user_info.user_id )
        info.locked_by_me = true;

    return info;
}


PLM_REVISION_HISTORY MOCK_FULL_PLM_PLUGIN::GetRevisionHistory( const wxString& aProjectId )
{
    return m_server.GetRevisionHistory( aProjectId );
}


PLM_REVISION MOCK_FULL_PLM_PLUGIN::GetRevision( const wxString& aProjectId,
                                                 const wxString& aRevisionId )
{
    const PLM_REVISION* rev = m_server.GetRevision( aProjectId, aRevisionId );
    return rev ? *rev : PLM_REVISION();
}


bool MOCK_FULL_PLM_PLUGIN::DownloadRevision( const wxString& aProjectId,
                                              const wxString& aRevisionId,
                                              const wxString& /* aLocalPath */ )
{
    const PLM_REVISION* rev = m_server.GetRevision( aProjectId, aRevisionId );
    return rev != nullptr;
}


PLM_REVISION_COMPARE MOCK_FULL_PLM_PLUGIN::CompareRevisions( const wxString& aProjectId,
                                                              const wxString& aRevision1,
                                                              const wxString& aRevision2 )
{
    PLM_REVISION_COMPARE result;

    const PLM_REVISION* rev1 = m_server.GetRevision( aProjectId, aRevision1 );
    const PLM_REVISION* rev2 = m_server.GetRevision( aProjectId, aRevision2 );

    if( rev1 )
        result.base_revision = *rev1;

    if( rev2 )
        result.compare_revision = *rev2;

    return result;
}


PLM_CHECKOUT_RESULT MOCK_FULL_PLM_PLUGIN::CreateProject( const wxString&          aFolderId,
                                                          const wxString&          aName,
                                                          const PLM_ATTRIBUTE_SET& /* aMetadata */,
                                                          const wxString&          /* aTemplateId */,
                                                          const PLM_CLONE_OPTIONS& /* aCloneOptions */ )
{
    if( !m_authenticated )
    {
        return PLM_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                             wxS( "Not authenticated" ) );
    }

    wxString projectId = wxString::Format( wxS( "proj-%d" ), rand() );

    PLM_FILE_INFO file( projectId, aName, wxS( "1.0" ), PLM_STATE_CATEGORY::EDITABLE );
    file.folder_id = aFolderId;
    m_server.AddFile( file );

    return PLM_CHECKOUT_RESULT::Success( wxEmptyString, file );
}


bool MOCK_FULL_PLM_PLUGIN::DeleteProject( const wxString& /* aProjectId */ )
{
    if( !m_authenticated )
        return false;

    return true;
}


PLM_SCHEMA MOCK_FULL_PLM_PLUGIN::GetSchema()
{
    return PLM_SCHEMA();
}


PLM_ATTRIBUTE_SET MOCK_FULL_PLM_PLUGIN::GetAttributes( const wxString& /* aProjectId */ )
{
    return PLM_ATTRIBUTE_SET();
}


bool MOCK_FULL_PLM_PLUGIN::SetAttributes( const wxString& /* aProjectId */,
                                           const PLM_ATTRIBUTE_SET& /* aAttributes */ )
{
    if( !m_authenticated )
        return false;

    return true;
}


PLM_PERMISSIONS MOCK_FULL_PLM_PLUGIN::GetPermissions( const wxString& /* aProjectId */ )
{
    return PLM_PERMISSIONS::FullAccess();
}


wxString MOCK_FULL_PLM_PLUGIN::GetUserDisplayName( const wxString& aUserId )
{
    return aUserId;
}
