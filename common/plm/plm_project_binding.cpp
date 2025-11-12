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

#include "plm_project_binding.h"

#include <wx/filename.h>
#include <wx/log.h>


PLM_PROJECT_BINDING_MANAGER& PLM_PROJECT_BINDING_MANAGER::GetInstance()
{
    static PLM_PROJECT_BINDING_MANAGER instance;
    return instance;
}


PLM_PROJECT_BINDING_MANAGER::PLM_PROJECT_BINDING_MANAGER()
{
}


wxString PLM_PROJECT_BINDING_MANAGER::NormalizePath( const wxString& aPath )
{
    wxFileName fn( aPath );
    fn.Normalize( wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE );
    return fn.GetFullPath();
}


void PLM_PROJECT_BINDING_MANAGER::SetBinding( const wxString& aLocalPath,
                                               const PLM_PROJECT_BINDING& aBinding )
{
    wxString key = NormalizePath( aLocalPath );

    {
        std::lock_guard<std::mutex> lock( m_mutex );
        m_bindings[key] = aBinding;
    }

    wxLogMessage( wxS( "PLM: Set binding for %s -> %s/%s" ),
                  aLocalPath, aBinding.server_url, aBinding.project_id );

    FireChangeEvent( aLocalPath, aBinding );
}


void PLM_PROJECT_BINDING_MANAGER::LoadBinding( const wxString& aLocalPath,
                                                const PLM_PROJECT_BINDING& aBinding )
{
    wxString key = NormalizePath( aLocalPath );

    {
        std::lock_guard<std::mutex> lock( m_mutex );
        m_bindings[key] = aBinding;
    }

    wxLogMessage( wxS( "PLM: Loaded binding for %s" ), aLocalPath );
}


bool PLM_PROJECT_BINDING_MANAGER::GetBinding( const wxString& aLocalPath,
                                               PLM_PROJECT_BINDING& aBinding ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizePath( aLocalPath );
    auto it = m_bindings.find( key );

    if( it == m_bindings.end() )
        return false;

    aBinding = it->second;
    return true;
}


void PLM_PROJECT_BINDING_MANAGER::RemoveBinding( const wxString& aLocalPath )
{
    wxString key = NormalizePath( aLocalPath );
    bool removed = false;

    {
        std::lock_guard<std::mutex> lock( m_mutex );
        auto it = m_bindings.find( key );

        if( it != m_bindings.end() )
        {
            m_bindings.erase( it );
            removed = true;
        }
    }

    if( removed )
    {
        wxLogMessage( wxS( "PLM: Removed binding for %s" ), aLocalPath );
        FireChangeEvent( aLocalPath, PLM_PROJECT_BINDING() );
    }
}


void PLM_PROJECT_BINDING_MANAGER::RemoveAllBindings()
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_bindings.clear();
    wxLogMessage( wxS( "PLM: Removed all bindings" ) );
}


bool PLM_PROJECT_BINDING_MANAGER::HasBinding( const wxString& aLocalPath ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizePath( aLocalPath );
    return m_bindings.find( key ) != m_bindings.end();
}


PLM_BINDING_STATE PLM_PROJECT_BINDING_MANAGER::GetBindingState( const wxString& aLocalPath ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizePath( aLocalPath );
    auto it = m_bindings.find( key );

    if( it == m_bindings.end() )
        return PLM_BINDING_STATE::UNBOUND;

    if( !it->second.IsValid() )
        return PLM_BINDING_STATE::BINDING_INVALID;

    return PLM_BINDING_STATE::BOUND;
}


std::vector<wxString> PLM_PROJECT_BINDING_MANAGER::GetBoundProjects() const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    std::vector<wxString> projects;
    projects.reserve( m_bindings.size() );

    for( const auto& [path, binding] : m_bindings )
        projects.push_back( path );

    return projects;
}


std::vector<wxString> PLM_PROJECT_BINDING_MANAGER::FindProjectsByServer(
        const wxString& aServerUrl ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    std::vector<wxString> projects;

    for( const auto& [path, binding] : m_bindings )
    {
        if( binding.server_url == aServerUrl )
            projects.push_back( path );
    }

    return projects;
}


std::vector<wxString> PLM_PROJECT_BINDING_MANAGER::FindProjectsByPlugin(
        const wxString& aPluginId ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    std::vector<wxString> projects;

    for( const auto& [path, binding] : m_bindings )
    {
        if( binding.plugin_id == aPluginId )
            projects.push_back( path );
    }

    return projects;
}


void PLM_PROJECT_BINDING_MANAGER::UpdateSync( const wxString& aLocalPath,
                                               const wxString& aRevision )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizePath( aLocalPath );
    auto it = m_bindings.find( key );

    if( it != m_bindings.end() )
    {
        it->second.UpdateSync( aRevision );
        wxLogMessage( wxS( "PLM: Updated sync for %s to revision %s" ), aLocalPath, aRevision );
    }
}


void PLM_PROJECT_BINDING_MANAGER::SetCheckoutId( const wxString& aLocalPath,
                                                  const wxString& aCheckoutId )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizePath( aLocalPath );
    auto it = m_bindings.find( key );

    if( it != m_bindings.end() )
    {
        it->second.checkout_id = aCheckoutId;
        wxLogMessage( wxS( "PLM: Set checkout ID for %s" ), aLocalPath );
    }
}


void PLM_PROJECT_BINDING_MANAGER::ClearCheckout( const wxString& aLocalPath )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizePath( aLocalPath );
    auto it = m_bindings.find( key );

    if( it != m_bindings.end() )
    {
        it->second.ClearCheckout();
        wxLogMessage( wxS( "PLM: Cleared checkout for %s" ), aLocalPath );
    }
}


void PLM_PROJECT_BINDING_MANAGER::SetChangeCallback( PLM_BINDING_CHANGE_CALLBACK aCallback )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_changeCallback = std::move( aCallback );
}


void PLM_PROJECT_BINDING_MANAGER::FireChangeEvent( const wxString& aLocalPath,
                                                    const PLM_PROJECT_BINDING& aBinding )
{
    PLM_BINDING_CHANGE_CALLBACK callback;

    {
        std::lock_guard<std::mutex> lock( m_mutex );
        callback = m_changeCallback;
    }

    if( callback )
        callback( aLocalPath, aBinding );
}
