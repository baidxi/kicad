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

#include "plm_activity_tracker.h"

#include <wx/log.h>
#include <algorithm>
#include <thread>


PLM_ACTIVITY_TRACKER::PLM_ACTIVITY_TRACKER() :
        m_config( PLM_ACTIVITY_CONFIG::Default() )
{
}


void PLM_ACTIVITY_TRACKER::SetConfig( const PLM_ACTIVITY_CONFIG& aConfig )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_config = aConfig;

    // Trim existing caches if new limit is smaller
    for( auto& [projectId, cache] : m_cache )
    {
        while( cache.size() > m_config.max_entries )
            cache.pop_back();
    }
}


PLM_ACTIVITY_CONFIG PLM_ACTIVITY_TRACKER::GetConfig() const
{
    std::lock_guard<std::mutex> lock( m_mutex );
    return m_config;
}


void PLM_ACTIVITY_TRACKER::SetFetchFunction( PLM_ACTIVITY_FETCH_FUNC aFunc )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_fetchFunc = std::move( aFunc );
}


bool PLM_ACTIVITY_TRACKER::RefreshActivity( const wxString& aProjectId )
{
    PLM_ACTIVITY_FETCH_FUNC fetchFunc;

    {
        std::lock_guard<std::mutex> lock( m_mutex );
        fetchFunc = m_fetchFunc;
    }

    if( !fetchFunc )
    {
        wxLogWarning( wxS( "PLM Activity: No fetch function configured" ) );
        return false;
    }

    // Build filter to get recent activity
    PLM_AUDIT_FILTER filter = PLM_AUDIT_FILTER::ForProject( aProjectId );
    filter.limit = m_config.max_entries;

    // Get timestamp of most recent cached entry to avoid duplicates
    int64_t latestTs = GetLatestTimestamp( aProjectId );

    if( latestTs > 0 )
        filter.start_time_ms = latestTs + 1;

    try
    {
        std::vector<PLM_AUDIT_EVENT> events = fetchFunc( aProjectId, filter );

        if( !events.empty() )
        {
            AddActivities( aProjectId, events );
            wxLogMessage( wxS( "PLM Activity: Refreshed %zu events for %s" ),
                          events.size(), aProjectId );
        }

        return true;
    }
    catch( const std::exception& e )
    {
        wxLogError( wxS( "PLM Activity: Refresh failed for %s: %s" ),
                    aProjectId, e.what() );
        return false;
    }
}


void PLM_ACTIVITY_TRACKER::RefreshActivityAsync( const wxString&                 aProjectId,
                                                  PLM_ACTIVITY_REFRESH_CALLBACK   aCallback )
{
    std::thread( [this, aProjectId, aCallback]()
    {
        bool success = RefreshActivity( aProjectId );
        size_t count = GetActivityCount( aProjectId );

        if( aCallback )
            aCallback( aProjectId, success, count );
    } ).detach();
}


void PLM_ACTIVITY_TRACKER::AddActivity( const wxString& aProjectId,
                                         const PLM_AUDIT_EVENT& aEvent )
{
    if( !ShouldIncludeEvent( aEvent ) )
        return;

    PLM_ACTIVITY_ENTRY entry = PLM_ACTIVITY_ENTRY::FromAuditEvent( aEvent );
    AddEntryToCache( aProjectId, entry );
    FireChangeEvent( aProjectId, entry );
}


void PLM_ACTIVITY_TRACKER::AddActivities( const wxString& aProjectId,
                                           const std::vector<PLM_AUDIT_EVENT>& aEvents )
{
    for( const auto& event : aEvents )
    {
        if( ShouldIncludeEvent( event ) )
        {
            PLM_ACTIVITY_ENTRY entry = PLM_ACTIVITY_ENTRY::FromAuditEvent( event );
            AddEntryToCache( aProjectId, entry );
            FireChangeEvent( aProjectId, entry );
        }
    }
}


void PLM_ACTIVITY_TRACKER::ClearActivity( const wxString& aProjectId )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    if( aProjectId.IsEmpty() )
    {
        m_cache.clear();
        wxLogMessage( wxS( "PLM Activity: Cleared all cached activity" ) );
    }
    else
    {
        m_cache.erase( aProjectId );
        wxLogMessage( wxS( "PLM Activity: Cleared cached activity for %s" ), aProjectId );
    }
}


std::vector<PLM_ACTIVITY_ENTRY> PLM_ACTIVITY_TRACKER::GetActivity( const wxString& aProjectId,
                                                                    size_t aLimit ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    auto it = m_cache.find( aProjectId );

    if( it == m_cache.end() )
        return {};

    const auto& cache = it->second;

    if( aLimit == 0 || aLimit >= cache.size() )
        return std::vector<PLM_ACTIVITY_ENTRY>( cache.begin(), cache.end() );

    return std::vector<PLM_ACTIVITY_ENTRY>( cache.begin(), cache.begin() + aLimit );
}


std::vector<PLM_ACTIVITY_ENTRY> PLM_ACTIVITY_TRACKER::GetAllActivity( size_t aLimit ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    std::vector<PLM_ACTIVITY_ENTRY> all;

    // Collect all entries
    for( const auto& [projectId, cache] : m_cache )
    {
        all.insert( all.end(), cache.begin(), cache.end() );
    }

    // Sort by timestamp, most recent first
    std::sort( all.begin(), all.end(),
               []( const PLM_ACTIVITY_ENTRY& a, const PLM_ACTIVITY_ENTRY& b )
               {
                   return a.timestamp_ms > b.timestamp_ms;
               } );

    if( aLimit > 0 && aLimit < all.size() )
        all.resize( aLimit );

    return all;
}


size_t PLM_ACTIVITY_TRACKER::GetActivityCount( const wxString& aProjectId ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    auto it = m_cache.find( aProjectId );

    if( it == m_cache.end() )
        return 0;

    return it->second.size();
}


size_t PLM_ACTIVITY_TRACKER::GetTotalActivityCount() const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    size_t total = 0;

    for( const auto& [projectId, cache] : m_cache )
        total += cache.size();

    return total;
}


bool PLM_ACTIVITY_TRACKER::HasActivity( const wxString& aProjectId ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    auto it = m_cache.find( aProjectId );
    return it != m_cache.end() && !it->second.empty();
}


int64_t PLM_ACTIVITY_TRACKER::GetLatestTimestamp( const wxString& aProjectId ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    auto it = m_cache.find( aProjectId );

    if( it == m_cache.end() || it->second.empty() )
        return 0;

    // Cache is ordered most recent first
    return it->second.front().timestamp_ms;
}


void PLM_ACTIVITY_TRACKER::SetChangeCallback( PLM_ACTIVITY_CHANGE_CALLBACK aCallback )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_changeCallback = std::move( aCallback );
}


void PLM_ACTIVITY_TRACKER::AddEntryToCache( const wxString& aProjectId,
                                             const PLM_ACTIVITY_ENTRY& aEntry )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    auto& cache = m_cache[aProjectId];

    // Insert at front (most recent first)
    cache.push_front( aEntry );

    // Enforce size limit
    while( cache.size() > m_config.max_entries )
        cache.pop_back();
}


bool PLM_ACTIVITY_TRACKER::ShouldIncludeEvent( const PLM_AUDIT_EVENT& aEvent ) const
{
    // Check error filter
    if( aEvent.IsError() && !m_config.include_errors )
        return false;

    // Check system event filter
    switch( aEvent.event_type )
    {
    case PLM_AUDIT_EVENT_TYPE::SYNC_STARTED:
    case PLM_AUDIT_EVENT_TYPE::SYNC_COMPLETED:
    case PLM_AUDIT_EVENT_TYPE::CACHE_CLEARED:
    case PLM_AUDIT_EVENT_TYPE::PLUGIN_LOADED:
    case PLM_AUDIT_EVENT_TYPE::PLUGIN_UNLOADED:
        if( !m_config.include_system )
            return false;
        break;
    default:
        break;
    }

    return true;
}


void PLM_ACTIVITY_TRACKER::FireChangeEvent( const wxString& aProjectId,
                                             const PLM_ACTIVITY_ENTRY& aEntry )
{
    PLM_ACTIVITY_CHANGE_CALLBACK callback;

    {
        std::lock_guard<std::mutex> lock( m_mutex );
        callback = m_changeCallback;
    }

    if( callback )
        callback( aProjectId, aEntry );
}
