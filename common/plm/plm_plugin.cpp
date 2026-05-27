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
 * @file plm_plugin.cpp
 * @brief Implementation of PLM plugin base class utilities.
 */

#include "plm_plugin.h"
#include "plm_activity_tracker.h"
#include "plm_cache_manager.h"
#include "plm_file.h"
#include "plm_lib_sync.h"
#include "plm_project_binding.h"
#include "plm_state_mapper.h"
#include "plm_state_persistence.h"
#include "plm_sync_state.h"

#include <ctime>
#include <nlohmann/json.hpp>
#include <wx/datetime.h>
#include <wx/file.h>
#include <wx/filename.h>


bool WritePLMCheckoutMetadata( const wxString& aLocalPath,
                               const wxString& aPluginId,
                               const wxString& aProjectId,
                               const wxString& aRevision,
                               const wxString& aUser,
                               const wxString& aServerUrl )
{
    wxString metadataPath = aLocalPath + wxFileName::GetPathSeparator() + wxS( ".kicad_plm" );

    nlohmann::json metadata;
    metadata["plugin_id"] = aPluginId.ToStdString();
    metadata["project_id"] = aProjectId.ToStdString();
    metadata["revision"] = aRevision.ToStdString();
    metadata["user"] = aUser.ToStdString();
    metadata["checkout_time"] = wxDateTime::Now().FormatISOCombined().ToStdString();
    metadata["server_url"] = aServerUrl.ToStdString();

    wxFile file( metadataPath, wxFile::write );

    if( !file.IsOpened() )
        return false;

    std::string content = metadata.dump( 2 );
    size_t written = file.Write( content.c_str(), content.size() );
    file.Close();

    if( written != content.size() )
        return false;

    return true;
}


PLM_PLUGIN::PLM_PLUGIN() = default;
PLM_PLUGIN::~PLM_PLUGIN() = default;


void PLM_PLUGIN::InitManagers()
{
    if( !m_activityTracker )
    {
        m_activityTracker = std::make_unique<PLM_ACTIVITY_TRACKER>();
        m_activityTracker->SetConfig( PLM_ACTIVITY_CONFIG::Default() );
    }

    if( !m_stateMapper )
        m_stateMapper = std::make_unique<PLM_STATE_MAPPER>();

    if( !m_cacheManager )
    {
        m_cacheManager = std::make_unique<PLM_CACHE_MANAGER>();

        if( !m_cacheManager->Initialize() )
            m_cacheManager.reset();
    }

    if( !m_syncStateManager )
        m_syncStateManager = std::make_unique<PLM_SYNC_STATE_MANAGER>();

    if( !m_statePersistence )
    {
        m_statePersistence = std::make_unique<PLM_STATE_PERSISTENCE>();

        if( m_cacheManager )
            m_statePersistence->SetCacheDirectory( m_cacheManager->GetCacheRootDir() );
    }

    // Bind callbacks to plugin virtuals so the lib managers pull from the
    // backend. Restart-survival persistence wires in with project binding.
    if( !m_libVersionTracker )
    {
        m_libVersionTracker = std::make_unique<PLM_LIB_VERSION_TRACKER>();
        m_libVersionTracker->SetServerVersionCallback(
                [this]( const wxString& aLibId )
                { return GetLatestLibraryRevision( aLibId ); } );
    }

    if( !m_libProvider )
    {
        m_libProvider = std::make_unique<PLM_LIB_PROVIDER>();
        m_libProvider->SetVersionTracker( m_libVersionTracker.get() );
        m_libProvider->SetDownloadCallback(
                [this]( const wxString& aLibId, PLM_LIB_TYPE aLibType,
                        const wxString& aRequestedRevision, const wxString& aLocalPath )
                { return DownloadLibrary( aLibId, aLibType, aRequestedRevision, aLocalPath ); } );
    }

    if( !m_libSync )
    {
        m_libSync = std::make_unique<PLM_LIB_SYNC>();
        m_libSync->SetProvider( m_libProvider.get() );
        m_libSync->SetVersionTracker( m_libVersionTracker.get() );
    }
}


void PLM_PLUGIN::TeardownManagers()
{
    m_activityTracker.reset();
    m_stateMapper.reset();
    m_syncStateManager.reset();
    m_statePersistence.reset();
    m_cacheManager.reset();

    // Tear down sync first so any in-flight async work it owns sees a
    // valid provider/tracker; the provider then drops its tracker pointer
    // before the tracker goes away.
    m_libSync.reset();
    m_libProvider.reset();
    m_libVersionTracker.reset();
}


void PLM_PLUGIN::SetupActivityTracker()
{
    if( !m_activityTracker )
    {
        m_activityTracker = std::make_unique<PLM_ACTIVITY_TRACKER>();
        m_activityTracker->SetConfig( PLM_ACTIVITY_CONFIG::Default() );
    }

    m_activityTracker->SetFetchFunction(
            [this]( const wxString& aProjectId, const PLM_AUDIT_FILTER& aFilter )
            {
                PLM_AUDIT_FILTER scoped = aFilter;

                if( !aProjectId.IsEmpty() )
                    scoped.project_id = aProjectId;

                return GetAuditLog( scoped ).events;
            } );
}


void PLM_PLUGIN::SetupStateMapper()
{
    if( !m_stateMapper )
        m_stateMapper = std::make_unique<PLM_STATE_MAPPER>();

    m_stateMapper->SetMappingCallback( [this]() { return GetStateMappings(); } );
    m_stateMapper->LoadMappings();
}


void PLM_PLUGIN::RecordAuditEvent( PLM_AUDIT_EVENT_TYPE aType, const wxString& aProjectId,
                                    bool aSuccess, const wxString& aDetails )
{
    if( !m_activityTracker )
        return;

    const PLM_SESSION& session = GetCurrentSession();

    PLM_AUDIT_EVENT event( aType, aProjectId, session.user_info.user_id );
    event.user_name = session.user_info.display_name;
    event.success = aSuccess;
    event.result = aDetails;

    m_activityTracker->AddActivity( aProjectId, event );
}


void PLM_PLUGIN::PersistProjectState( const wxString& aLocalPath, const wxString& aProjectId,
                                       const wxString& aRevision )
{
    if( !m_syncStateManager || !m_statePersistence || aLocalPath.IsEmpty() )
        return;

    // PLM_STATE_PERSISTENCE::SaveState writes unconditionally; guard
    // against an empty server URL so we never persist a binding that
    // PLM_PROJECT_BINDING::IsValid() would later reject.
    const wxString serverUrl = GetCurrentSession().server_url;

    if( serverUrl.IsEmpty() )
        return;

    m_syncStateManager->InitProject( aLocalPath, aRevision );

    PLM_PROJECT_STATE state;
    state.binding = PLM_PROJECT_BINDING( GetPluginInfo().plugin_id, serverUrl, aProjectId );
    state.binding.UpdateSync( aRevision );

    PLM_SYNC_INFO syncInfo;

    if( m_syncStateManager->GetSyncInfo( aLocalPath, syncInfo ) )
        state.sync_info = syncInfo;
    else
        state.sync_info.local_revision = aRevision;

    state.saved_at = static_cast<int64_t>( std::time( nullptr ) );

    m_statePersistence->SaveState( aLocalPath, state );
}
