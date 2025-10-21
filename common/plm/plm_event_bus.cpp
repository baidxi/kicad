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

#include "plm_event_bus.h"

#include <wx/log.h>
#include <algorithm>


PLM_EVENT_BUS& PLM_EVENT_BUS::GetInstance()
{
    static PLM_EVENT_BUS instance;
    return instance;
}


PLM_EVENT_BUS::PLM_EVENT_BUS() :
        m_nextId( 1 ),
        m_publishCount( 0 )
{
}


PLM_SUBSCRIPTION_ID PLM_EVENT_BUS::Subscribe( PLM_EVENT_CALLBACK aCallback,
                                               const PLM_EVENT_FILTER& aFilter )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    Subscription sub;
    sub.id = m_nextId++;
    sub.callback = std::move( aCallback );
    sub.filter = aFilter;

    // Capture ID before move to avoid UB from accessing moved-from object
    PLM_SUBSCRIPTION_ID id = sub.id;
    m_subscriptions.push_back( std::move( sub ) );

    wxLogMessage( wxS( "PLM EventBus: Added subscription %llu (total: %zu)" ),
                  static_cast<unsigned long long>( id ), m_subscriptions.size() );

    return id;
}


void PLM_EVENT_BUS::Unsubscribe( PLM_SUBSCRIPTION_ID aId )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    auto it = std::find_if( m_subscriptions.begin(), m_subscriptions.end(),
                            [aId]( const Subscription& sub ) { return sub.id == aId; } );

    if( it != m_subscriptions.end() )
    {
        m_subscriptions.erase( it );
        wxLogMessage( wxS( "PLM EventBus: Removed subscription %llu (remaining: %zu)" ),
                      static_cast<unsigned long long>( aId ), m_subscriptions.size() );
    }
}


void PLM_EVENT_BUS::UnsubscribeAll()
{
    std::lock_guard<std::mutex> lock( m_mutex );

    size_t count = m_subscriptions.size();
    m_subscriptions.clear();

    wxLogMessage( wxS( "PLM EventBus: Removed all %zu subscriptions" ), count );
}


size_t PLM_EVENT_BUS::GetSubscriberCount() const
{
    std::lock_guard<std::mutex> lock( m_mutex );
    return m_subscriptions.size();
}


void PLM_EVENT_BUS::Publish( const PLM_EVENT& aEvent )
{
    std::vector<PLM_EVENT_CALLBACK> callbacks;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        m_publishCount++;

        // Collect matching callbacks
        for( const auto& sub : m_subscriptions )
        {
            if( sub.filter.Matches( aEvent ) )
                callbacks.push_back( sub.callback );
        }
    }

    // Invoke callbacks outside the lock to prevent deadlocks
    for( const auto& callback : callbacks )
    {
        try
        {
            callback( aEvent );
        }
        catch( const std::exception& e )
        {
            wxLogError( wxS( "PLM EventBus: Callback threw exception: %s" ), e.what() );
        }
        catch( ... )
        {
            wxLogError( wxS( "PLM EventBus: Callback threw unknown exception" ) );
        }
    }
}


void PLM_EVENT_BUS::Publish( PLM_EVENT_TYPE aType,
                              const wxString& aProjectId,
                              const wxString& aMessage )
{
    PLM_EVENT event( aType, aProjectId );
    event.message = aMessage;
    Publish( event );
}


void PLM_EVENT_BUS::PublishNotification( const PLM_NOTIFICATION& aNotification )
{
    Publish( PLM_EVENT::FromNotification( aNotification ) );
}


void PLM_EVENT_BUS::PublishAuditEvent( const PLM_AUDIT_EVENT& aAudit )
{
    Publish( PLM_EVENT::FromAuditEvent( aAudit ) );
}


uint64_t PLM_EVENT_BUS::GetPublishCount() const
{
    std::lock_guard<std::mutex> lock( m_mutex );
    return m_publishCount;
}


void PLM_EVENT_BUS::ResetStats()
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_publishCount = 0;
}
