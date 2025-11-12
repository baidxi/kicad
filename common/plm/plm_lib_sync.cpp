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

#include "plm_lib_sync.h"

#include <wx/log.h>

#include <chrono>


PLM_LIB_SYNC::PLM_LIB_SYNC() :
        m_provider( nullptr ),
        m_refMgr( nullptr ),
        m_versionTracker( nullptr ),
        m_syncing( false ),
        m_cancelRequested( false )
{
}


PLM_LIB_SYNC::~PLM_LIB_SYNC()
{
    CancelSync();
    WaitForSync();
}


void PLM_LIB_SYNC::SetProvider( PLM_LIB_PROVIDER* aProvider )
{
    m_provider = aProvider;
}


void PLM_LIB_SYNC::SetReferenceManager( PLM_LIB_REFERENCE_MANAGER* aRefMgr )
{
    m_refMgr = aRefMgr;
}


void PLM_LIB_SYNC::SetVersionTracker( PLM_LIB_VERSION_TRACKER* aTracker )
{
    m_versionTracker = aTracker;
}


PLM_LIB_SYNC_ITEM PLM_LIB_SYNC::SyncLibrary( const PLM_LIB_REFERENCE& aRef, bool aForce )
{
    PLM_LIB_SYNC_ITEM item;
    item.lib_id = aRef.lib_id;
    item.lib_type = aRef.lib_type;
    item.old_revision = aRef.revision;

    if( !m_provider )
    {
        item.status = PLM_SYNC_STATUS::FAILED;
        item.error_message = wxS( "No library provider configured" );
        return item;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // Check if we need to sync
    if( !aForce && m_versionTracker )
    {
        if( m_versionTracker->IsPinned( aRef.lib_id ) )
        {
            item.status = PLM_SYNC_STATUS::SKIPPED;
            item.new_revision = aRef.revision;
            return item;
        }

        auto diff = m_versionTracker->CheckForUpdate( aRef.lib_id );

        if( !diff.update_available )
        {
            item.status = PLM_SYNC_STATUS::SKIPPED;
            item.new_revision = aRef.revision;
            return item;
        }
    }

    // Download library
    PLM_LIB_QUERY_RESULT result;

    switch( aRef.lib_type )
    {
    case PLM_LIB_TYPE::SYMBOL:
        result = m_provider->GetSymbolLibrary( aRef.lib_id, true );
        break;

    case PLM_LIB_TYPE::FOOTPRINT:
        result = m_provider->GetFootprintLibrary( aRef.lib_id, true );
        break;

    case PLM_LIB_TYPE::MODEL_3D:
        result = m_provider->Get3DModel( aRef.lib_id, true );
        break;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    item.sync_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                endTime - startTime ).count();

    if( !result.success )
    {
        item.status = PLM_SYNC_STATUS::FAILED;
        item.error_message = result.error_message;
        return item;
    }

    item.status = PLM_SYNC_STATUS::SUCCEEDED;
    item.new_revision = result.revision;

    // Update references and version tracker
    if( m_refMgr )
    {
        m_refMgr->UpdateRevision( aRef.lib_id, result.revision );
        m_refMgr->UpdateLocalPath( aRef.lib_id, result.lib_path );
        m_refMgr->MarkSynced( aRef.lib_id );
    }

    if( m_versionTracker )
    {
        m_versionTracker->UpdateRevision( aRef.lib_id, result.revision );
    }

    return item;
}


PLM_LIB_SYNC_SUMMARY PLM_LIB_SYNC::SyncLibraries( const std::vector<PLM_LIB_REFERENCE>& aRefs,
                                                   bool aForce )
{
    PLM_LIB_SYNC_SUMMARY summary;
    summary.total_libraries = aRefs.size();

    auto overallStart = std::chrono::high_resolution_clock::now();

    for( const auto& ref : aRefs )
    {
        if( m_cancelRequested )
        {
            PLM_LIB_SYNC_ITEM item;
            item.lib_id = ref.lib_id;
            item.lib_type = ref.lib_type;
            item.status = PLM_SYNC_STATUS::CANCELLED;
            summary.items.push_back( item );
            summary.cancelled++;
            continue;
        }

        PLM_LIB_SYNC_ITEM item = SyncLibrary( ref, aForce );
        summary.items.push_back( item );

        switch( item.status )
        {
        case PLM_SYNC_STATUS::SUCCEEDED:
            summary.succeeded++;
            summary.total_bytes += item.download_bytes;
            break;

        case PLM_SYNC_STATUS::FAILED:
            summary.failed++;
            break;

        case PLM_SYNC_STATUS::SKIPPED:
            summary.skipped++;
            break;

        case PLM_SYNC_STATUS::CANCELLED:
            summary.cancelled++;
            break;

        default:
            break;
        }
    }

    auto overallEnd = std::chrono::high_resolution_clock::now();
    summary.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   overallEnd - overallStart ).count();

    return summary;
}


void PLM_LIB_SYNC::StartSyncAsync( const std::vector<PLM_LIB_REFERENCE>& aRefs,
                                    PLM_SYNC_PROGRESS_CALLBACK aProgressCb,
                                    PLM_SYNC_COMPLETE_CALLBACK aCompleteCb )
{
    if( m_syncing )
    {
        wxLogWarning( wxS( "Sync already in progress" ) );
        return;
    }

    m_syncing = true;
    m_cancelRequested = false;

    // Copy refs for thread safety
    std::vector<PLM_LIB_REFERENCE> refsCopy = aRefs;

    m_syncThread = std::make_unique<std::thread>( &PLM_LIB_SYNC::runAsyncSync, this,
                                                   std::move( refsCopy ),
                                                   std::move( aProgressCb ),
                                                   std::move( aCompleteCb ) );
}


void PLM_LIB_SYNC::CancelSync()
{
    m_cancelRequested = true;
}


void PLM_LIB_SYNC::WaitForSync()
{
    if( m_syncThread && m_syncThread->joinable() )
    {
        m_syncThread->join();
        m_syncThread.reset();
    }
}


std::vector<wxString> PLM_LIB_SYNC::GetLibrariesNeedingSync(
        const std::vector<PLM_LIB_REFERENCE>& aRefs )
{
    std::vector<wxString> result;

    if( !m_versionTracker )
        return result;

    for( const auto& ref : aRefs )
    {
        auto diff = m_versionTracker->CheckForUpdate( ref.lib_id );

        if( diff.update_available && !diff.is_pinned )
            result.push_back( ref.lib_id );
    }

    return result;
}


bool PLM_LIB_SYNC::HasPendingUpdates( const std::vector<PLM_LIB_REFERENCE>& aRefs )
{
    return !GetLibrariesNeedingSync( aRefs ).empty();
}


void PLM_LIB_SYNC::runAsyncSync( std::vector<PLM_LIB_REFERENCE> aRefs,
                                  PLM_SYNC_PROGRESS_CALLBACK aProgressCb,
                                  PLM_SYNC_COMPLETE_CALLBACK aCompleteCb )
{
    PLM_LIB_SYNC_SUMMARY summary;
    summary.total_libraries = aRefs.size();

    auto overallStart = std::chrono::high_resolution_clock::now();
    size_t index = 0;

    for( const auto& ref : aRefs )
    {
        if( m_cancelRequested )
        {
            PLM_LIB_SYNC_ITEM item;
            item.lib_id = ref.lib_id;
            item.lib_type = ref.lib_type;
            item.status = PLM_SYNC_STATUS::CANCELLED;
            summary.items.push_back( item );
            summary.cancelled++;
            index++;
            continue;
        }

        PLM_LIB_SYNC_ITEM item;
        item.lib_id = ref.lib_id;
        item.lib_type = ref.lib_type;
        item.status = PLM_SYNC_STATUS::IN_PROGRESS;

        // Report progress before sync
        float progress = static_cast<float>( index ) / static_cast<float>( aRefs.size() );

        if( aProgressCb )
        {
            if( !aProgressCb( item, progress ) )
            {
                m_cancelRequested = true;
                item.status = PLM_SYNC_STATUS::CANCELLED;
                summary.items.push_back( item );
                summary.cancelled++;
                index++;
                continue;
            }
        }

        // Perform sync
        item = SyncLibrary( ref, false );
        summary.items.push_back( item );

        switch( item.status )
        {
        case PLM_SYNC_STATUS::SUCCEEDED:
            summary.succeeded++;
            summary.total_bytes += item.download_bytes;
            break;

        case PLM_SYNC_STATUS::FAILED:
            summary.failed++;
            break;

        case PLM_SYNC_STATUS::SKIPPED:
            summary.skipped++;
            break;

        case PLM_SYNC_STATUS::CANCELLED:
            summary.cancelled++;
            break;

        default:
            break;
        }

        index++;
    }

    auto overallEnd = std::chrono::high_resolution_clock::now();
    summary.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   overallEnd - overallStart ).count();

    m_syncing = false;

    if( aCompleteCb )
        aCompleteCb( summary );
}
