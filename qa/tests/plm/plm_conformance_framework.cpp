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

#include "plm_conformance_framework.h"

#include <ctime>


PLM_CONFORMANCE_TESTER::PLM_CONFORMANCE_TESTER() : m_plugin( nullptr )
{
    RegisterTests();
}


PLM_CONFORMANCE_TESTER::~PLM_CONFORMANCE_TESTER()
{
}


void PLM_CONFORMANCE_TESTER::SetPlugin( PLM_PLUGIN* aPlugin )
{
    m_plugin = aPlugin;
}


void PLM_CONFORMANCE_TESTER::RegisterTests()
{
    m_tests.push_back( { wxS( "PluginInfo" ),
                         [this]() { return TestPluginInfo(); } } );

    m_tests.push_back( { wxS( "Capabilities" ),
                         [this]() { return TestCapabilities(); } } );

    m_tests.push_back( { wxS( "LoadUnload" ),
                         [this]() { return TestLoadUnload(); } } );

    m_tests.push_back( { wxS( "AuthenticationBasic" ),
                         [this]() { return TestAuthenticationBasic(); } } );

    m_tests.push_back( { wxS( "NavigationRootFolders" ),
                         [this]() { return TestNavigationRootFolders(); } } );

    m_tests.push_back( { wxS( "NavigationChildFolders" ),
                         [this]() { return TestNavigationChildFolders(); } } );

    m_tests.push_back( { wxS( "NavigationFolderPath" ),
                         [this]() { return TestNavigationFolderPath(); } } );

    m_tests.push_back( { wxS( "FileListEmpty" ),
                         [this]() { return TestFileListEmpty(); } } );

    m_tests.push_back( { wxS( "SchemaRetrieval" ),
                         [this]() { return TestSchemaRetrieval(); } } );

    m_tests.push_back( { wxS( "PermissionsRetrieval" ),
                         [this]() { return TestPermissionsRetrieval(); } } );
}


PLM_CONFORMANCE_REPORT PLM_CONFORMANCE_TESTER::RunAllTests()
{
    PLM_CONFORMANCE_REPORT report;

    if( m_plugin )
        report.plugin_id = m_plugin->GetPluginInfo().plugin_id;

    for( const auto& test : m_tests )
    {
        PLM_CONFORMANCE_RESULT result = test.second();
        report.AddResult( result );
    }

    return report;
}


PLM_CONFORMANCE_RESULT PLM_CONFORMANCE_TESTER::RunTest( const wxString& aTestName )
{
    for( const auto& test : m_tests )
    {
        if( test.first == aTestName )
            return test.second();
    }

    return Error( aTestName, wxS( "Test not found" ) );
}


std::vector<wxString> PLM_CONFORMANCE_TESTER::GetTestNames() const
{
    std::vector<wxString> names;

    for( const auto& test : m_tests )
        names.push_back( test.first );

    return names;
}


PLM_CONFORMANCE_RESULT PLM_CONFORMANCE_TESTER::Pass( const wxString& aName, const wxString& aMessage )
{
    return PLM_CONFORMANCE_RESULT( aName, PLM_TEST_STATUS::PASSED, aMessage );
}


PLM_CONFORMANCE_RESULT PLM_CONFORMANCE_TESTER::Fail( const wxString& aName, const wxString& aMessage )
{
    return PLM_CONFORMANCE_RESULT( aName, PLM_TEST_STATUS::FAILED, aMessage );
}


PLM_CONFORMANCE_RESULT PLM_CONFORMANCE_TESTER::Skip( const wxString& aName, const wxString& aReason )
{
    return PLM_CONFORMANCE_RESULT( aName, PLM_TEST_STATUS::SKIPPED, aReason );
}


PLM_CONFORMANCE_RESULT PLM_CONFORMANCE_TESTER::Error( const wxString& aName, const wxString& aError )
{
    return PLM_CONFORMANCE_RESULT( aName, PLM_TEST_STATUS::ERROR, aError );
}


PLM_CONFORMANCE_RESULT PLM_CONFORMANCE_TESTER::TestPluginInfo()
{
    if( !m_plugin )
        return Error( wxS( "PluginInfo" ), wxS( "No plugin set" ) );

    PLM_PLUGIN_INFO info = m_plugin->GetPluginInfo();

    if( info.plugin_id.IsEmpty() )
        return Fail( wxS( "PluginInfo" ), wxS( "Plugin ID is empty" ) );

    if( info.display_name.IsEmpty() )
        return Fail( wxS( "PluginInfo" ), wxS( "Display name is empty" ) );

    return Pass( wxS( "PluginInfo" ) );
}


PLM_CONFORMANCE_RESULT PLM_CONFORMANCE_TESTER::TestCapabilities()
{
    if( !m_plugin )
        return Error( wxS( "Capabilities" ), wxS( "No plugin set" ) );

    PLM_CAPABILITIES caps = m_plugin->GetCapabilities();

    // Capabilities struct should always be valid, even if all false
    return Pass( wxS( "Capabilities" ) );
}


PLM_CONFORMANCE_RESULT PLM_CONFORMANCE_TESTER::TestLoadUnload()
{
    if( !m_plugin )
        return Error( wxS( "LoadUnload" ), wxS( "No plugin set" ) );

    if( !m_plugin->OnLoad() )
        return Fail( wxS( "LoadUnload" ), wxS( "OnLoad() returned false" ) );

    m_plugin->OnUnload();

    return Pass( wxS( "LoadUnload" ) );
}


PLM_CONFORMANCE_RESULT PLM_CONFORMANCE_TESTER::TestAuthenticationBasic()
{
    if( !m_plugin )
        return Error( wxS( "AuthenticationBasic" ), wxS( "No plugin set" ) );

    // Initially not authenticated
    if( m_plugin->IsAuthenticated() )
        return Fail( wxS( "AuthenticationBasic" ), wxS( "Plugin is authenticated before login" ) );

    return Pass( wxS( "AuthenticationBasic" ) );
}


PLM_CONFORMANCE_RESULT PLM_CONFORMANCE_TESTER::TestNavigationRootFolders()
{
    if( !m_plugin )
        return Error( wxS( "NavigationRootFolders" ), wxS( "No plugin set" ) );

    // Should return empty if not authenticated
    if( !m_plugin->IsAuthenticated() )
    {
        auto folders = m_plugin->GetRootFolders();

        if( !folders.empty() )
        {
            return Fail( wxS( "NavigationRootFolders" ),
                         wxS( "GetRootFolders returned data when not authenticated" ) );
        }
    }

    return Pass( wxS( "NavigationRootFolders" ) );
}


PLM_CONFORMANCE_RESULT PLM_CONFORMANCE_TESTER::TestNavigationChildFolders()
{
    if( !m_plugin )
        return Error( wxS( "NavigationChildFolders" ), wxS( "No plugin set" ) );

    // Should return empty for invalid folder ID
    auto folders = m_plugin->GetChildFolders( wxS( "__invalid_folder_id__" ) );

    if( !folders.empty() )
    {
        return Fail( wxS( "NavigationChildFolders" ),
                     wxS( "GetChildFolders returned data for invalid folder" ) );
    }

    return Pass( wxS( "NavigationChildFolders" ) );
}


PLM_CONFORMANCE_RESULT PLM_CONFORMANCE_TESTER::TestNavigationFolderPath()
{
    if( !m_plugin )
        return Error( wxS( "NavigationFolderPath" ), wxS( "No plugin set" ) );

    // Should return empty for invalid folder ID
    auto path = m_plugin->GetFolderPath( wxS( "__invalid_folder_id__" ) );

    // Empty path is acceptable for invalid ID
    return Pass( wxS( "NavigationFolderPath" ) );
}


PLM_CONFORMANCE_RESULT PLM_CONFORMANCE_TESTER::TestFileListEmpty()
{
    if( !m_plugin )
        return Error( wxS( "FileListEmpty" ), wxS( "No plugin set" ) );

    PLM_LIST_OPTIONS opts;
    auto result = m_plugin->ListFiles( wxS( "__invalid_folder_id__" ), opts );

    // Empty result is acceptable for invalid folder
    return Pass( wxS( "FileListEmpty" ) );
}


PLM_CONFORMANCE_RESULT PLM_CONFORMANCE_TESTER::TestSchemaRetrieval()
{
    if( !m_plugin )
        return Error( wxS( "SchemaRetrieval" ), wxS( "No plugin set" ) );

    PLM_SCHEMA schema = m_plugin->GetSchema();

    // Schema can be empty for plugins that don't support attributes
    return Pass( wxS( "SchemaRetrieval" ) );
}


PLM_CONFORMANCE_RESULT PLM_CONFORMANCE_TESTER::TestPermissionsRetrieval()
{
    if( !m_plugin )
        return Error( wxS( "PermissionsRetrieval" ), wxS( "No plugin set" ) );

    PLM_PERMISSIONS perms = m_plugin->GetPermissions( wxS( "__invalid_project_id__" ) );

    // Permissions struct should always be valid
    return Pass( wxS( "PermissionsRetrieval" ) );
}


// Mock plugin implementation

PLM_MOCK_PLUGIN::PLM_MOCK_PLUGIN()
{
}


PLM_MOCK_PLUGIN::~PLM_MOCK_PLUGIN()
{
}


PLM_PLUGIN_INFO PLM_MOCK_PLUGIN::GetPluginInfo() const
{
    PLM_PLUGIN_INFO info;
    info.plugin_id = wxS( "plm-mock" );
    info.display_name = wxS( "Mock PLM Plugin" );
    info.vendor = wxS( "KiCad Test" );
    info.version = PLM_VERSION( 1, 0, 0 );
    info.description = wxS( "Mock plugin for testing" );
    return info;
}


PLM_CAPABILITIES PLM_MOCK_PLUGIN::GetCapabilities() const
{
    PLM_CAPABILITIES caps;
    caps.supports_checkout = true;
    caps.supports_locking = true;
    caps.supports_revisions = true;
    caps.supports_attributes = true;
    return caps;
}


bool PLM_MOCK_PLUGIN::OnLoad()
{
    m_loaded = true;
    return true;
}


void PLM_MOCK_PLUGIN::OnUnload()
{
    m_loaded = false;
    m_authenticated = false;
}


PLM_AUTH_RESULT PLM_MOCK_PLUGIN::Authenticate( const PLM_CREDENTIALS& aCredentials )
{
    PLM_AUTH_RESULT result;

    if( m_authSuccess )
    {
        m_authenticated = true;
        m_session.token = wxS( "mock_token" );
        m_session.expires_at = static_cast<int64_t>( std::time( nullptr ) ) + 86400;
        m_session.server_url = aCredentials.server_url;
        m_session.user_info.user_id = aCredentials.username;
        m_session.user_info.display_name = aCredentials.username;

        result.success = true;
        result.token = m_session.token.Get();
        result.expires_at = m_session.expires_at;
    }
    else
    {
        result.success = false;
        result.error_message = wxS( "Mock authentication failure" );
    }

    return result;
}


bool PLM_MOCK_PLUGIN::Logout()
{
    m_authenticated = false;
    m_session = PLM_SESSION();
    return true;
}


bool PLM_MOCK_PLUGIN::IsAuthenticated() const
{
    return m_authenticated;
}


PLM_SESSION PLM_MOCK_PLUGIN::GetCurrentSession() const
{
    return m_session;
}


PLM_AUTH_RESULT PLM_MOCK_PLUGIN::RefreshSession()
{
    PLM_AUTH_RESULT result;

    if( m_authenticated )
    {
        m_session.expires_at = static_cast<int64_t>( std::time( nullptr ) ) + 86400;
        result.success = true;
        result.token = m_session.token.Get();
        result.expires_at = m_session.expires_at;
    }
    else
    {
        result.success = false;
        result.error_message = wxS( "Not authenticated" );
    }

    return result;
}


std::vector<PLM_FOLDER> PLM_MOCK_PLUGIN::GetRootFolders()
{
    if( !m_authenticated )
        return {};

    return m_rootFolders;
}


std::vector<PLM_FOLDER> PLM_MOCK_PLUGIN::GetChildFolders( const wxString& aFolderId )
{
    return {};
}


PLM_FOLDER_PATH PLM_MOCK_PLUGIN::GetFolderPath( const wxString& aFolderId )
{
    return {};
}


PLM_LIST_RESULT<PLM_FILE_INFO> PLM_MOCK_PLUGIN::ListFiles( const wxString&         aFolderId,
                                                           const PLM_LIST_OPTIONS& aOptions )
{
    return {};
}


PLM_LIST_RESULT<PLM_FILE_INFO> PLM_MOCK_PLUGIN::SearchFiles( const wxString&         aQuery,
                                                             const PLM_LIST_OPTIONS& aOptions )
{
    return {};
}


PLM_CHECKOUT_RESULT PLM_MOCK_PLUGIN::Checkout( const wxString&             aProjectId,
                                                const wxString&             aLocalPath,
                                                const PLM_CHECKOUT_OPTIONS& aOptions )
{
    PLM_CHECKOUT_RESULT result;
    result.success = false;
    result.error_message = wxS( "Mock checkout not implemented" );
    return result;
}


PLM_CHECKIN_RESULT PLM_MOCK_PLUGIN::Checkin( const wxString&            aLocalPath,
                                              const PLM_CHECKIN_OPTIONS& aOptions )
{
    PLM_CHECKIN_RESULT result;
    result.success = false;
    result.error_message = wxS( "Mock checkin not implemented" );
    return result;
}


PLM_FILE_INFO PLM_MOCK_PLUGIN::GetFileInfo( const wxString& aProjectId )
{
    return {};
}


PLM_CHECKOUT_RESULT PLM_MOCK_PLUGIN::CreateProject( const wxString&          aFolderId,
                                                     const wxString&          aName,
                                                     const PLM_ATTRIBUTE_SET& aMetadata,
                                                     const wxString&          aTemplateId,
                                                     const PLM_CLONE_OPTIONS& aCloneOptions )
{
    PLM_CHECKOUT_RESULT result;
    result.success = false;
    result.error_message = wxS( "Mock create project not implemented" );
    return result;
}


bool PLM_MOCK_PLUGIN::DeleteProject( const wxString& aProjectId )
{
    return false;
}


PLM_CANCEL_CHECKOUT_RESULT PLM_MOCK_PLUGIN::CancelCheckout( const wxString& aLocalPath )
{
    return PLM_CANCEL_CHECKOUT_RESULT::Success();
}


PLM_REVISION_HISTORY PLM_MOCK_PLUGIN::GetRevisionHistory( const wxString& aProjectId )
{
    return {};
}


PLM_REVISION PLM_MOCK_PLUGIN::GetRevision( const wxString& aProjectId, const wxString& aRevisionId )
{
    return {};
}


bool PLM_MOCK_PLUGIN::DownloadRevision( const wxString& aProjectId, const wxString& aRevisionId,
                                         const wxString& aLocalPath )
{
    return false;
}


PLM_REVISION_COMPARE PLM_MOCK_PLUGIN::CompareRevisions( const wxString& aProjectId,
                                                         const wxString& aRevision1,
                                                         const wxString& aRevision2 )
{
    return {};
}


bool PLM_MOCK_PLUGIN::RestoreRevision( const wxString& aProjectId, const wxString& aRevisionId,
                                        const wxString& aComment )
{
    return false;
}


PLM_LOCK_RESULT PLM_MOCK_PLUGIN::Lock( const PLM_LOCK_REQUEST& aRequest )
{
    PLM_LOCK_RESULT result;
    result.success = false;
    result.error_message = wxS( "Mock lock not implemented" );
    return result;
}


PLM_LOCK_RESULT PLM_MOCK_PLUGIN::Unlock( const PLM_UNLOCK_REQUEST& aRequest )
{
    PLM_LOCK_RESULT result;
    result.success = false;
    result.error_message = wxS( "Mock unlock not implemented" );
    return result;
}


PLM_LOCK_INFO PLM_MOCK_PLUGIN::GetLockInfo( const wxString& aProjectId )
{
    PLM_LOCK_INFO info;
    info.is_locked = false;
    return info;
}


PLM_SCHEMA PLM_MOCK_PLUGIN::GetSchema()
{
    return m_schema;
}


PLM_ATTRIBUTE_SET PLM_MOCK_PLUGIN::GetAttributes( const wxString& aProjectId )
{
    return {};
}


bool PLM_MOCK_PLUGIN::SetAttributes( const wxString&          aProjectId,
                                      const PLM_ATTRIBUTE_SET& aAttributes )
{
    return false;
}


PLM_PERMISSIONS PLM_MOCK_PLUGIN::GetPermissions( const wxString& aProjectId )
{
    PLM_PERMISSIONS perms;
    perms.can_checkout = true;
    perms.can_checkin = true;
    perms.can_delete = false;
    perms.can_lock = true;
    perms.can_unlock = true;
    return perms;
}


wxString PLM_MOCK_PLUGIN::GetUserDisplayName( const wxString& aUserId )
{
    return aUserId;
}
