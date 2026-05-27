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
 * @file mock_plm_server.h
 * @brief Mock PLM server for integration testing.
 *
 * Provides an in-memory PLM server simulation with configurable responses
 * and error injection for comprehensive testing.
 */

#ifndef MOCK_PLM_SERVER_H
#define MOCK_PLM_SERVER_H

#include <plm/plm_plugin.h>
#include <plm/plm_lock.h>
#include <plm/plm_revision.h>

#include <map>
#include <set>
#include <memory>


/**
 * Configuration for error injection in mock server.
 */
struct MOCK_ERROR_CONFIG
{
    bool         inject_error = false;        ///< Whether to inject an error
    PLM_ERROR_CODE error_code = PLM_ERROR_CODE::SUCCESS; ///< Error to inject
    wxString     error_message;               ///< Error message to return
    int          fail_after_calls = -1;       ///< Fail after N successful calls (-1 = never)
    int          call_count = 0;              ///< Current call count

    void Reset()
    {
        inject_error = false;
        error_code = PLM_ERROR_CODE::SUCCESS;
        error_message.Clear();
        fail_after_calls = -1;
        call_count = 0;
    }

    bool ShouldFail()
    {
        if( !inject_error )
            return false;

        call_count++;

        if( fail_after_calls > 0 && call_count <= fail_after_calls )
            return false;

        return true;
    }
};


/**
 * In-memory PLM server simulation.
 *
 * Stores files, revisions, locks, and folders in memory and provides
 * configurable responses for testing various scenarios.
 */
class MOCK_PLM_SERVER
{
public:
    MOCK_PLM_SERVER();

    void Reset();

    void AddFolder( const PLM_FOLDER& aFolder );
    void AddFile( const PLM_FILE_INFO& aFile );
    void AddRevision( const wxString& aFileId, const PLM_REVISION& aRevision );
    void SetLock( const wxString& aFileId, const PLM_LOCK_INFO& aLock );

    std::vector<PLM_FOLDER> GetRootFolders() const;
    std::vector<PLM_FOLDER> GetChildFolders( const wxString& aParentId ) const;
    PLM_LIST_RESULT<PLM_FILE_INFO> GetFilesInFolder( const wxString& aFolderId,
                                                     const PLM_LIST_OPTIONS& aOptions ) const;

    PLM_FILE_INFO* GetFile( const wxString& aFileId );
    const PLM_FILE_INFO* GetFile( const wxString& aFileId ) const;

    PLM_LOCK_INFO GetLockInfo( const wxString& aFileId ) const;
    bool AcquireLock( const wxString& aFileId, const wxString& aUser );
    bool ReleaseLock( const wxString& aFileId, const wxString& aUser, bool aForce = false );

    PLM_REVISION_HISTORY GetRevisionHistory( const wxString& aFileId ) const;
    const PLM_REVISION* GetRevision( const wxString& aFileId, const wxString& aRevisionId ) const;
    wxString CreateRevision( const wxString& aFileId, const wxString& aAuthor,
                              const wxString& aComment );

    void SetAuthUser( const wxString& aUsername, bool aValid = true );
    bool ValidateAuth( const wxString& aUsername, const wxString& aPassword ) const;
    bool IsValidUser( const wxString& aUsername ) const;

    // Error injection
    MOCK_ERROR_CONFIG& AuthErrors() { return m_authErrors; }
    MOCK_ERROR_CONFIG& LockErrors() { return m_lockErrors; }
    MOCK_ERROR_CONFIG& CheckoutErrors() { return m_checkoutErrors; }
    MOCK_ERROR_CONFIG& CheckinErrors() { return m_checkinErrors; }
    MOCK_ERROR_CONFIG& NavigationErrors() { return m_navigationErrors; }

private:
    std::vector<PLM_FOLDER>                 m_folders;
    std::map<wxString, PLM_FILE_INFO>       m_files;
    std::map<wxString, PLM_LOCK_INFO>       m_locks;
    std::map<wxString, std::vector<PLM_REVISION>> m_revisions;
    std::set<wxString>                      m_validUsers;

    MOCK_ERROR_CONFIG m_authErrors;
    MOCK_ERROR_CONFIG m_lockErrors;
    MOCK_ERROR_CONFIG m_checkoutErrors;
    MOCK_ERROR_CONFIG m_checkinErrors;
    MOCK_ERROR_CONFIG m_navigationErrors;

    int m_nextRevisionNumber = 1;
};


/**
 * Full-featured mock PLM plugin for integration testing.
 *
 * Uses MOCK_PLM_SERVER as backend and implements all PLM_PLUGIN methods
 * with realistic behavior.
 */
class MOCK_FULL_PLM_PLUGIN : public PLM_PLUGIN
{
public:
    MOCK_FULL_PLM_PLUGIN();
    ~MOCK_FULL_PLM_PLUGIN() override;

    // Access to server for test configuration
    MOCK_PLM_SERVER& GetServer() { return m_server; }

    // PLM_PLUGIN interface
    PLM_PLUGIN_INFO GetPluginInfo() const override;
    PLM_CAPABILITIES GetCapabilities() const override;

    bool OnLoad() override;
    void OnUnload() override;

    PLM_AUTH_RESULT Authenticate( const PLM_CREDENTIALS& aCredentials ) override;
    PLM_AUTH_RESULT RefreshSession() override;
    bool Logout() override;
    bool IsAuthenticated() const override;
    PLM_SESSION GetCurrentSession() const override;

    std::vector<PLM_FOLDER> GetRootFolders() override;
    std::vector<PLM_FOLDER> GetChildFolders( const wxString& aFolderId ) override;
    PLM_FOLDER_PATH GetFolderPath( const wxString& aFolderId ) override;
    PLM_LIST_RESULT<PLM_FILE_INFO> ListFiles( const wxString& aFolderId,
                                              const PLM_LIST_OPTIONS& aOptions ) override;
    PLM_LIST_RESULT<PLM_FILE_INFO> SearchFiles( const wxString& aQuery,
                                                const PLM_LIST_OPTIONS& aOptions ) override;

    PLM_CHECKOUT_RESULT Checkout( const wxString& aProjectId, const wxString& aLocalPath,
                                  const PLM_CHECKOUT_OPTIONS& aOptions ) override;
    PLM_CHECKIN_RESULT Checkin( const wxString& aLocalPath,
                                const PLM_CHECKIN_OPTIONS& aOptions ) override;
    PLM_FILE_INFO GetFileInfo( const wxString& aProjectId ) override;
    PLM_CANCEL_CHECKOUT_RESULT CancelCheckout( const wxString& aLocalPath ) override;

    PLM_LOCK_RESULT Lock( const PLM_LOCK_REQUEST& aRequest ) override;
    PLM_LOCK_RESULT Unlock( const PLM_UNLOCK_REQUEST& aRequest ) override;
    PLM_LOCK_INFO GetLockInfo( const wxString& aProjectId ) override;

    PLM_REVISION_HISTORY GetRevisionHistory( const wxString& aProjectId ) override;
    PLM_REVISION GetRevision( const wxString& aProjectId, const wxString& aRevisionId ) override;
    bool DownloadRevision( const wxString& aProjectId, const wxString& aRevisionId,
                           const wxString& aLocalPath ) override;
    PLM_REVISION_COMPARE CompareRevisions( const wxString& aProjectId,
                                           const wxString& aRevision1,
                                           const wxString& aRevision2 ) override;

    PLM_CHECKOUT_RESULT CreateProject( const wxString&          aFolderId,
                                        const wxString&          aName,
                                        const PLM_ATTRIBUTE_SET& aMetadata,
                                        const wxString&          aTemplateId = wxEmptyString,
                                        const PLM_CLONE_OPTIONS& aCloneOptions =
                                                PLM_CLONE_OPTIONS() ) override;
    bool DeleteProject( const wxString& aProjectId ) override;

    PLM_SCHEMA GetSchema() override;
    PLM_ATTRIBUTE_SET GetAttributes( const wxString& aProjectId ) override;
    bool SetAttributes( const wxString& aProjectId, const PLM_ATTRIBUTE_SET& aAttributes ) override;

    PLM_PERMISSIONS GetPermissions( const wxString& aProjectId ) override;
    wxString GetUserDisplayName( const wxString& aUserId ) override;

private:
    MOCK_PLM_SERVER m_server;
    PLM_SESSION     m_session;
    bool            m_loaded = false;
    bool            m_authenticated = false;

    std::map<wxString, wxString> m_checkedOutFiles;
};


#endif // MOCK_PLM_SERVER_H
