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

/**
 * @file plm_conformance_framework.h
 * @brief Conformance test framework for PLM plugins.
 *
 * This framework provides base classes and utilities for testing
 * PLM plugin implementations against the PLM_PLUGIN interface contract.
 */

#ifndef PLM_CONFORMANCE_FRAMEWORK_H
#define PLM_CONFORMANCE_FRAMEWORK_H

#include <plm/plm_plugin.h>

#include <memory>
#include <vector>
#include <functional>


/**
 * Test result status.
 */
enum class PLM_TEST_STATUS
{
    PASSED,
    FAILED,
    SKIPPED,
    ERROR
};


/**
 * Single conformance test result.
 */
struct PLM_CONFORMANCE_RESULT
{
    wxString        test_name;
    PLM_TEST_STATUS status;
    wxString        message;
    wxString        details;

    PLM_CONFORMANCE_RESULT() : status( PLM_TEST_STATUS::PASSED ) {}

    PLM_CONFORMANCE_RESULT( const wxString& aName, PLM_TEST_STATUS aStatus,
                            const wxString& aMessage = wxEmptyString ) :
            test_name( aName ),
            status( aStatus ),
            message( aMessage )
    {
    }

    bool Passed() const { return status == PLM_TEST_STATUS::PASSED; }
};


/**
 * Collection of conformance test results.
 */
struct PLM_CONFORMANCE_REPORT
{
    wxString                           plugin_id;
    std::vector<PLM_CONFORMANCE_RESULT> results;
    int                                total_tests = 0;
    int                                passed = 0;
    int                                failed = 0;
    int                                skipped = 0;
    int                                errors = 0;

    void AddResult( const PLM_CONFORMANCE_RESULT& aResult )
    {
        results.push_back( aResult );
        total_tests++;

        switch( aResult.status )
        {
        case PLM_TEST_STATUS::PASSED:  passed++;  break;
        case PLM_TEST_STATUS::FAILED:  failed++;  break;
        case PLM_TEST_STATUS::SKIPPED: skipped++; break;
        case PLM_TEST_STATUS::ERROR:   errors++;  break;
        }
    }

    bool AllPassed() const { return failed == 0 && errors == 0; }

    double PassRate() const
    {
        if( total_tests == 0 )
            return 0.0;

        return ( static_cast<double>( passed ) / total_tests ) * 100.0;
    }
};


/**
 * Base class for PLM conformance test suites.
 *
 * Provides infrastructure for running conformance tests against
 * PLM plugin implementations.
 */
class PLM_CONFORMANCE_TESTER
{
public:
    PLM_CONFORMANCE_TESTER();
    virtual ~PLM_CONFORMANCE_TESTER();

    /**
     * Set the plugin to test.
     *
     * @param aPlugin Plugin instance to test.
     */
    void SetPlugin( PLM_PLUGIN* aPlugin );

    /**
     * Run all conformance tests.
     *
     * @return Test report with all results.
     */
    PLM_CONFORMANCE_REPORT RunAllTests();

    /**
     * Run a specific test by name.
     *
     * @param aTestName Name of test to run.
     * @return Single test result.
     */
    PLM_CONFORMANCE_RESULT RunTest( const wxString& aTestName );

    /**
     * Get list of available test names.
     *
     * @return Vector of test names.
     */
    std::vector<wxString> GetTestNames() const;

protected:
    /**
     * Helper to record a passed test.
     *
     * @param aName Test name.
     * @param aMessage Optional message.
     * @return Passed result.
     */
    PLM_CONFORMANCE_RESULT Pass( const wxString& aName,
                                  const wxString& aMessage = wxEmptyString );

    /**
     * Helper to record a failed test.
     *
     * @param aName Test name.
     * @param aMessage Failure description.
     * @return Failed result.
     */
    PLM_CONFORMANCE_RESULT Fail( const wxString& aName, const wxString& aMessage );

    /**
     * Helper to record a skipped test.
     *
     * @param aName Test name.
     * @param aReason Skip reason.
     * @return Skipped result.
     */
    PLM_CONFORMANCE_RESULT Skip( const wxString& aName, const wxString& aReason );

    /**
     * Helper to record a test error.
     *
     * @param aName Test name.
     * @param aError Error description.
     * @return Error result.
     */
    PLM_CONFORMANCE_RESULT Error( const wxString& aName, const wxString& aError );

    PLM_PLUGIN* m_plugin;

private:
    PLM_CONFORMANCE_RESULT TestPluginInfo();
    PLM_CONFORMANCE_RESULT TestCapabilities();
    PLM_CONFORMANCE_RESULT TestLoadUnload();
    PLM_CONFORMANCE_RESULT TestAuthenticationBasic();
    PLM_CONFORMANCE_RESULT TestNavigationRootFolders();
    PLM_CONFORMANCE_RESULT TestNavigationChildFolders();
    PLM_CONFORMANCE_RESULT TestNavigationFolderPath();
    PLM_CONFORMANCE_RESULT TestFileListEmpty();
    PLM_CONFORMANCE_RESULT TestSchemaRetrieval();
    PLM_CONFORMANCE_RESULT TestPermissionsRetrieval();

    using TestFunction = std::function<PLM_CONFORMANCE_RESULT()>;
    std::vector<std::pair<wxString, TestFunction>> m_tests;

    void RegisterTests();
};


/**
 * Mock PLM plugin for testing.
 *
 * Provides a configurable mock implementation for testing
 * code that uses PLM plugins.
 */
class PLM_MOCK_PLUGIN : public PLM_PLUGIN
{
public:
    PLM_MOCK_PLUGIN();
    ~PLM_MOCK_PLUGIN() override;

    PLM_PLUGIN_INFO  GetPluginInfo() const override;
    PLM_CAPABILITIES GetCapabilities() const override;

    bool OnLoad() override;
    void OnUnload() override;

    PLM_AUTH_RESULT Authenticate( const PLM_CREDENTIALS& aCredentials ) override;
    bool            Logout() override;
    bool            IsAuthenticated() const override;
    PLM_SESSION     GetCurrentSession() const override;
    PLM_AUTH_RESULT RefreshSession() override;

    std::vector<PLM_FOLDER>               GetRootFolders() override;
    std::vector<PLM_FOLDER>               GetChildFolders( const wxString& aFolderId ) override;
    PLM_FOLDER_PATH                       GetFolderPath( const wxString& aFolderId ) override;
    PLM_LIST_RESULT<PLM_FILE_INFO>        ListFiles( const wxString&         aFolderId,
                                                     const PLM_LIST_OPTIONS& aOptions ) override;
    PLM_LIST_RESULT<PLM_FILE_INFO>        SearchFiles( const wxString&         aQuery,
                                                       const PLM_LIST_OPTIONS& aOptions ) override;

    PLM_CHECKOUT_RESULT Checkout( const wxString&             aProjectId,
                                   const wxString&             aLocalPath,
                                   const PLM_CHECKOUT_OPTIONS& aOptions ) override;
    PLM_CHECKIN_RESULT  Checkin( const wxString&            aLocalPath,
                                  const PLM_CHECKIN_OPTIONS& aOptions ) override;
    PLM_FILE_INFO       GetFileInfo( const wxString& aProjectId ) override;
    PLM_CHECKOUT_RESULT CreateProject( const wxString&          aFolderId,
                                        const wxString&          aName,
                                        const PLM_ATTRIBUTE_SET& aMetadata,
                                        const wxString&          aTemplateId = wxEmptyString,
                                        const PLM_CLONE_OPTIONS& aCloneOptions =
                                                PLM_CLONE_OPTIONS() ) override;
    bool                       DeleteProject( const wxString& aProjectId ) override;
    PLM_CANCEL_CHECKOUT_RESULT CancelCheckout( const wxString& aLocalPath ) override;

    PLM_REVISION_HISTORY GetRevisionHistory( const wxString& aProjectId ) override;
    PLM_REVISION         GetRevision( const wxString& aProjectId,
                                       const wxString& aRevisionId ) override;
    bool                 DownloadRevision( const wxString& aProjectId, const wxString& aRevisionId,
                                           const wxString& aLocalPath ) override;
    PLM_REVISION_COMPARE CompareRevisions( const wxString& aProjectId, const wxString& aRevision1,
                                            const wxString& aRevision2 ) override;
    bool                 RestoreRevision( const wxString& aProjectId, const wxString& aRevisionId,
                                          const wxString& aComment ) override;

    PLM_LOCK_RESULT Lock( const PLM_LOCK_REQUEST& aRequest ) override;
    PLM_LOCK_RESULT Unlock( const PLM_UNLOCK_REQUEST& aRequest ) override;
    PLM_LOCK_INFO   GetLockInfo( const wxString& aProjectId ) override;

    PLM_SCHEMA        GetSchema() override;
    PLM_ATTRIBUTE_SET GetAttributes( const wxString& aProjectId ) override;
    bool              SetAttributes( const wxString&          aProjectId,
                                     const PLM_ATTRIBUTE_SET& aAttributes ) override;

    PLM_PERMISSIONS GetPermissions( const wxString& aProjectId ) override;
    wxString        GetUserDisplayName( const wxString& aUserId ) override;

    // Mock configuration
    void SetAuthenticateSuccess( bool aSuccess ) { m_authSuccess = aSuccess; }
    void SetRootFolders( const std::vector<PLM_FOLDER>& aFolders ) { m_rootFolders = aFolders; }
    void SetSchema( const PLM_SCHEMA& aSchema ) { m_schema = aSchema; }

private:
    bool                     m_loaded = false;
    bool                     m_authenticated = false;
    bool                     m_authSuccess = true;
    PLM_SESSION              m_session;
    std::vector<PLM_FOLDER>  m_rootFolders;
    PLM_SCHEMA               m_schema;
};


#endif // PLM_CONFORMANCE_FRAMEWORK_H
