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

#include "plm_lib_update_checker.h"

#include <wx/log.h>

#include <chrono>


PLM_LIB_UPDATE_CHECKER::PLM_LIB_UPDATE_CHECKER() :
        m_versionTracker( nullptr ),
        m_checking( false ),
        m_cancelRequested( false ),
        m_notifyEnabled( true )
{
}


PLM_LIB_UPDATE_CHECKER::~PLM_LIB_UPDATE_CHECKER()
{
    CancelCheck();
    WaitForCheck();
}


void PLM_LIB_UPDATE_CHECKER::SetVersionTracker( PLM_LIB_VERSION_TRACKER* aTracker )
{
    m_versionTracker = aTracker;
}


void PLM_LIB_UPDATE_CHECKER::SetGetRevisionCallback( PLM_GET_REVISION_CALLBACK aCallback )
{
    m_getRevisionCb = aCallback;
}


void PLM_LIB_UPDATE_CHECKER::SetNotifyCallback( PLM_UPDATE_NOTIFY_CALLBACK aCallback )
{
    m_notifyCb = aCallback;
}


PLM_LIB_UPDATE_INFO PLM_LIB_UPDATE_CHECKER::CheckLibrary( const PLM_LIB_REFERENCE& aRef )
{
    PLM_LIB_UPDATE_INFO info;
    info.lib_id = aRef.lib_id;
    info.lib_name = aRef.lib_name;
    info.lib_type = aRef.lib_type;
    info.current_revision = aRef.revision;

    // Check if pinned
    if( m_versionTracker && m_versionTracker->IsPinned( aRef.lib_id ) )
        info.is_pinned = true;

    if( !m_getRevisionCb )
        return info;

    // Query server for latest revision
    wxString latestRev = m_getRevisionCb( aRef.lib_id );

    if( latestRev.IsEmpty() )
        return info;

    info.latest_revision = latestRev;

    return info;
}


PLM_LIB_UPDATE_REPORT PLM_LIB_UPDATE_CHECKER::CheckForUpdates(
        const std::vector<PLM_LIB_REFERENCE>& aRefs )
{
    PLM_LIB_UPDATE_REPORT report;
    report.total_checked = aRefs.size();

    auto startTime = std::chrono::high_resolution_clock::now();

    for( const auto& ref : aRefs )
    {
        if( m_cancelRequested )
            break;

        PLM_LIB_UPDATE_INFO info = CheckLibrary( ref );

        if( info.is_pinned )
        {
            report.pinned++;

            if( info.HasUpdate() )
                report.updates.push_back( info );
        }
        else if( info.HasUpdate() )
        {
            report.updates.push_back( info );
        }
        else if( info.latest_revision.IsEmpty() )
        {
            report.errors++;
        }
        else
        {
            report.up_to_date++;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    report.check_time_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>( endTime - startTime ).count();

    m_lastReport = report;

    // Send notification if enabled and updates found
    if( m_notifyEnabled && m_notifyCb && report.HasUpdates() )
    {
        m_notifyCb( report.GetUpdateCount(), report.GetCriticalCount() );
    }

    return report;
}


void PLM_LIB_UPDATE_CHECKER::CheckForUpdatesAsync( const std::vector<PLM_LIB_REFERENCE>& aRefs,
                                                    PLM_UPDATE_CHECK_CALLBACK aCallback )
{
    if( m_checking )
    {
        wxLogWarning( wxS( "Update check already in progress" ) );
        return;
    }

    m_checking = true;
    m_cancelRequested = false;

    // Copy refs for thread safety
    std::vector<PLM_LIB_REFERENCE> refsCopy = aRefs;

    m_checkThread = std::make_unique<std::thread>( &PLM_LIB_UPDATE_CHECKER::runAsyncCheck, this,
                                                    std::move( refsCopy ),
                                                    std::move( aCallback ) );
}


void PLM_LIB_UPDATE_CHECKER::CancelCheck()
{
    m_cancelRequested = true;
}


void PLM_LIB_UPDATE_CHECKER::WaitForCheck()
{
    if( m_checkThread && m_checkThread->joinable() )
    {
        m_checkThread->join();
        m_checkThread.reset();
    }
}


void PLM_LIB_UPDATE_CHECKER::runAsyncCheck( std::vector<PLM_LIB_REFERENCE> aRefs,
                                             PLM_UPDATE_CHECK_CALLBACK aCallback )
{
    PLM_LIB_UPDATE_REPORT report = CheckForUpdates( aRefs );

    m_checking = false;

    if( aCallback )
        aCallback( report );
}
