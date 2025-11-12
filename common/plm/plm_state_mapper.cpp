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

#include "plm_state_mapper.h"

#include <wx/intl.h>
#include <wx/log.h>


PLM_STATE_MAPPER::PLM_STATE_MAPPER()
{
}


void PLM_STATE_MAPPER::SetMappingCallback( PLM_STATE_MAPPING_CALLBACK aCallback )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_mappingCallback = std::move( aCallback );
    m_mappingsLoaded = false;
    m_mappings.clear();
}


bool PLM_STATE_MAPPER::LoadMappings()
{
    std::lock_guard<std::mutex> lock( m_mutex );

    if( !m_mappingCallback )
    {
        wxLogWarning( wxS( "PLM: No state mapping callback registered" ) );
        return false;
    }

    m_mappings.clear();

    std::vector<PLM_STATE_INFO> states = m_mappingCallback();

    for( const auto& state : states )
        m_mappings[state.native_state] = state;

    m_mappingsLoaded = true;

    wxLogMessage( wxS( "PLM: Loaded %zu state mappings" ), m_mappings.size() );
    return true;
}


void PLM_STATE_MAPPER::ClearMappings()
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_mappings.clear();
    m_mappingsLoaded = false;
}


bool PLM_STATE_MAPPER::HasMappings() const
{
    std::lock_guard<std::mutex> lock( m_mutex );
    return m_mappingsLoaded && !m_mappings.empty();
}


void PLM_STATE_MAPPER::EnsureMappingsLoaded()
{
    // Note: caller must hold mutex
    if( !m_mappingsLoaded && m_mappingCallback )
    {
        std::vector<PLM_STATE_INFO> states = m_mappingCallback();

        for( const auto& state : states )
            m_mappings[state.native_state] = state;

        m_mappingsLoaded = true;

        wxLogMessage( wxS( "PLM: Auto-loaded %zu state mappings" ), m_mappings.size() );
    }
}


PLM_STATE_CATEGORY PLM_STATE_MAPPER::GetCategory( const wxString& aNativeState )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    EnsureMappingsLoaded();

    auto it = m_mappings.find( aNativeState );

    if( it != m_mappings.end() )
        return it->second.category;

    // Unknown states default to EDITABLE for safety
    wxLogDebug( wxS( "PLM: Unknown state '%s', defaulting to EDITABLE" ), aNativeState );
    return PLM_STATE_CATEGORY::EDITABLE;
}


bool PLM_STATE_MAPPER::GetStateInfo( const wxString& aNativeState, PLM_STATE_INFO& aInfo )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    EnsureMappingsLoaded();

    auto it = m_mappings.find( aNativeState );

    if( it != m_mappings.end() )
    {
        aInfo = it->second;
        return true;
    }

    return false;
}


std::vector<PLM_STATE_INFO> PLM_STATE_MAPPER::GetAllStates()
{
    std::lock_guard<std::mutex> lock( m_mutex );

    EnsureMappingsLoaded();

    std::vector<PLM_STATE_INFO> result;
    result.reserve( m_mappings.size() );

    for( const auto& [key, info] : m_mappings )
        result.push_back( info );

    return result;
}


std::vector<PLM_STATE_INFO> PLM_STATE_MAPPER::GetStatesByCategory( PLM_STATE_CATEGORY aCategory )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    EnsureMappingsLoaded();

    std::vector<PLM_STATE_INFO> result;

    for( const auto& [key, info] : m_mappings )
    {
        if( info.category == aCategory )
            result.push_back( info );
    }

    return result;
}


bool PLM_STATE_MAPPER::GetDefaultState( PLM_STATE_INFO& aInfo )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    EnsureMappingsLoaded();

    for( const auto& [key, info] : m_mappings )
    {
        if( info.is_default )
        {
            aInfo = info;
            return true;
        }
    }

    return false;
}


wxString PLM_STATE_MAPPER::GetCategoryDisplayName( PLM_STATE_CATEGORY aCategory )
{
    switch( aCategory )
    {
    case PLM_STATE_CATEGORY::EDITABLE:
        return _( "Editable" );

    case PLM_STATE_CATEGORY::LOCKED:
        return _( "Locked" );

    case PLM_STATE_CATEGORY::RELEASED:
        return _( "Released" );

    case PLM_STATE_CATEGORY::PENDING_REVIEW:
        return _( "Pending Review" );
    }

    return _( "Unknown" );
}


wxString PLM_STATE_MAPPER::GetCategoryDescription( PLM_STATE_CATEGORY aCategory )
{
    switch( aCategory )
    {
    case PLM_STATE_CATEGORY::EDITABLE:
        return _( "Project can be modified and checked in" );

    case PLM_STATE_CATEGORY::LOCKED:
        return _( "Project is locked by another user; read-only access" );

    case PLM_STATE_CATEGORY::RELEASED:
        return _( "Project is released; changes require a formal change process" );

    case PLM_STATE_CATEGORY::PENDING_REVIEW:
        return _( "Project is submitted for review; modifications restricted" );
    }

    return _( "Unknown state" );
}
