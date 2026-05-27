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
 * @file test_plm_libs.cpp
 * @brief Coverage for the shared-library wiring landed in WF-LIBS-002 Slice 1.
 *
 * Exercises PLM_PLUGIN's InitManagers/TeardownManagers binding of
 * PLM_LIB_PROVIDER, PLM_LIB_SYNC, and PLM_LIB_VERSION_TRACKER plus the
 * callback round-trip into the four virtuals. The Git plugin's concrete
 * overrides are tested separately at integration time (require git on
 * PATH and a network/local repo) and are out of scope for qa_plm.
 */

#include <boost/test/unit_test.hpp>

#include <plm/plm_lib_provider.h>
#include <plm/plm_lib_version_tracker.h>
#include <plm/plm_plugin.h>

#include <wx/file.h>
#include <wx/filename.h>

#include <atomic>

#include "mock_plm_server.h"


namespace
{

/**
 * Minimal PLM_PLUGIN subclass with predictable shared-library responses.
 *
 * The four virtuals return synthetic data and bump call counters so tests
 * can prove the base-class wiring threaded through the right callbacks.
 * Each DownloadLibrary writes a stub file at aLocalPath so the provider's
 * post-download FileExists check passes.
 */
class FAKE_LIB_PLUGIN : public MOCK_FULL_PLM_PLUGIN
{
public:
    bool OnLoad() override
    {
        if( !MOCK_FULL_PLM_PLUGIN::OnLoad() )
            return false;

        InitManagers();
        return true;
    }

    void OnUnload() override
    {
        TeardownManagers();
        MOCK_FULL_PLM_PLUGIN::OnUnload();
    }

    PLM_CAPABILITIES GetCapabilities() const override
    {
        PLM_CAPABILITIES caps = MOCK_FULL_PLM_PLUGIN::GetCapabilities();
        caps.supports_shared_libraries = true;
        return caps;
    }

    std::vector<PLM_LIB_REFERENCE>
    GetAvailableLibraries( const wxString& /*aProjectId*/ = wxEmptyString ) override
    {
        PLM_LIB_REFERENCE ref;
        ref.lib_id = wxS( "fake/symbols" );
        ref.lib_name = ref.lib_id;
        ref.lib_type = PLM_LIB_TYPE::SYMBOL;
        ref.server_url = wxS( "fake://repo" );
        return { ref };
    }

    std::vector<PLM_LIB_VERSION_INFO> GetLibraryVersions( const wxString& aLibId ) override
    {
        ++m_versionsCalls;
        std::vector<PLM_LIB_VERSION_INFO> v;
        PLM_LIB_VERSION_INFO a;
        a.lib_id = aLibId;
        a.revision = wxS( "deadbeef" );
        a.revision_display = wxS( "v1" );
        v.push_back( a );

        PLM_LIB_VERSION_INFO b;
        b.lib_id = aLibId;
        b.revision = wxS( "cafe1234" );
        b.revision_display = wxS( "v2" );
        v.push_back( b );
        return v;
    }

    wxString GetLatestLibraryRevision( const wxString& /*aLibId*/ ) override
    {
        ++m_latestCalls;
        return wxS( "cafe1234" );
    }

    PLM_LIB_DOWNLOAD_RESULT DownloadLibrary( const wxString&        aLibId,
                                              PLM_LIB_TYPE          aLibType,
                                              const wxString&       aRequestedRevision,
                                              const wxString&       aLocalPath ) override
    {
        ++m_downloadCalls;
        m_lastRequestedRevision = aRequestedRevision;
        m_lastLibType = aLibType;
        m_lastLocalPath = aLocalPath;
        m_lastLibId = aLibId;

        wxFileName destFile( aLocalPath );

        if( !destFile.GetPath().IsEmpty() )
            wxFileName::Mkdir( destFile.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );

        wxFile out( aLocalPath, wxFile::write );

        if( !out.IsOpened() )
        {
            PLM_LIB_DOWNLOAD_RESULT bad;
            bad.error_message = wxS( "test fixture: failed to open output" );
            return bad;
        }

        out.Write( wxS( "(kicad_symbol_lib)\n" ) );
        out.Close();

        PLM_LIB_DOWNLOAD_RESULT ok;
        ok.success = true;
        ok.revision = aRequestedRevision.IsEmpty() ? wxS( "cafe1234" ) : aRequestedRevision;
        return ok;
    }

    ~FAKE_LIB_PLUGIN() override
    {
        if( !m_lastLocalPath.IsEmpty() )
            wxRemoveFile( m_lastLocalPath );
    }

    /// Test-only accessors for the protected manager block.
    PLM_LIB_PROVIDER*        ProviderForTest() { return m_libProvider.get(); }
    PLM_LIB_VERSION_TRACKER* VersionTrackerForTest() { return m_libVersionTracker.get(); }

    int          m_versionsCalls = 0;
    int          m_latestCalls = 0;
    int          m_downloadCalls = 0;
    PLM_LIB_TYPE m_lastLibType = PLM_LIB_TYPE::SYMBOL;
    wxString     m_lastRequestedRevision;
    wxString     m_lastLocalPath;
    wxString     m_lastLibId;
};

} // anonymous namespace


BOOST_AUTO_TEST_SUITE( PlmLibsWiring )


BOOST_AUTO_TEST_CASE( ManagersAreConstructedByOnLoad )
{
    FAKE_LIB_PLUGIN plugin;
    BOOST_REQUIRE( plugin.OnLoad() );

    BOOST_CHECK( plugin.GetCapabilities().supports_shared_libraries );

    auto libs = plugin.GetAvailableLibraries();
    BOOST_CHECK_EQUAL( libs.size(), 1u );
    BOOST_CHECK( libs.front().lib_id == wxS( "fake/symbols" ) );

    plugin.OnUnload();
}


BOOST_AUTO_TEST_CASE( CallbackBindingPropagatesRevisionToProvider )
{
    FAKE_LIB_PLUGIN plugin;
    BOOST_REQUIRE( plugin.OnLoad() );

    // Force the provider to use a unique cache slot for this test so it
    // doesn't collide with previously-cached entries from other test runs
    // (cache entries persist on-disk across runs unless wiped).
    static std::atomic<int> sCounter{ 0 };
    wxString libId = wxString::Format(
            wxS( "fake/symbols/test-%d" ), ++sCounter );

    PLM_LIB_VERSION_INFO pin;
    pin.lib_id = libId;
    pin.revision = wxS( "deadbeef" );
    pin.pinned = true;
    plugin.VersionTrackerForTest()->TrackLibrary( pin );

    PLM_LIB_QUERY_RESULT result =
            plugin.ProviderForTest()->GetSymbolLibrary( libId );

    BOOST_CHECK( result.success );
    BOOST_CHECK( !result.revision.IsEmpty() );
    BOOST_CHECK_EQUAL( plugin.m_downloadCalls, 1 );
    BOOST_CHECK( plugin.m_lastRequestedRevision == wxS( "deadbeef" ) );
    BOOST_CHECK( plugin.m_lastLibType == PLM_LIB_TYPE::SYMBOL );

    // Second hit should serve from the in-memory cache index (no extra
    // download). Also proves the cache slot survives within a session.
    PLM_LIB_QUERY_RESULT cached =
            plugin.ProviderForTest()->GetSymbolLibrary( libId );

    BOOST_CHECK( cached.success );
    BOOST_CHECK( cached.from_cache );
    BOOST_CHECK_EQUAL( plugin.m_downloadCalls, 1 );

    plugin.OnUnload();
}


BOOST_AUTO_TEST_CASE( UnpinnedRequestPassesEmptyRevisionToCallback )
{
    FAKE_LIB_PLUGIN plugin;
    BOOST_REQUIRE( plugin.OnLoad() );

    static std::atomic<int> sCounter{ 0 };
    wxString libId = wxString::Format(
            wxS( "fake/symbols/unpinned-%d" ), ++sCounter );

    PLM_LIB_QUERY_RESULT result =
            plugin.ProviderForTest()->GetSymbolLibrary( libId );

    BOOST_CHECK( result.success );
    BOOST_CHECK( plugin.m_lastRequestedRevision.IsEmpty() );

    plugin.OnUnload();
}


BOOST_AUTO_TEST_SUITE_END()
