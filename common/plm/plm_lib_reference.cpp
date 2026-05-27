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

#include "plm_lib_reference.h"

#include <nlohmann/json.hpp>

#include <wx/filename.h>
#include <wx/ffile.h>
#include <wx/intl.h>
#include <wx/log.h>

#include <chrono>


PLM_LIB_REFERENCE_MANAGER::PLM_LIB_REFERENCE_MANAGER() : m_modified( false )
{
}


PLM_LIB_REFERENCE_MANAGER::~PLM_LIB_REFERENCE_MANAGER()
{
}


void PLM_LIB_REFERENCE_MANAGER::AddReference( const PLM_LIB_REFERENCE& aRef )
{
    if( aRef.lib_id.IsEmpty() )
        return;

    m_references[aRef.lib_id] = aRef;
    m_modified = true;
}


void PLM_LIB_REFERENCE_MANAGER::RemoveReference( const wxString& aLibId )
{
    if( m_references.erase( aLibId ) > 0 )
        m_modified = true;
}


bool PLM_LIB_REFERENCE_MANAGER::HasReference( const wxString& aLibId ) const
{
    return m_references.find( aLibId ) != m_references.end();
}


std::optional<PLM_LIB_REFERENCE> PLM_LIB_REFERENCE_MANAGER::GetReference(
        const wxString& aLibId ) const
{
    auto it = m_references.find( aLibId );

    if( it != m_references.end() )
        return it->second;

    return std::nullopt;
}


std::vector<PLM_LIB_REFERENCE> PLM_LIB_REFERENCE_MANAGER::GetAllReferences() const
{
    std::vector<PLM_LIB_REFERENCE> result;
    result.reserve( m_references.size() );

    for( const auto& [id, ref] : m_references )
        result.push_back( ref );

    return result;
}


std::vector<PLM_LIB_REFERENCE> PLM_LIB_REFERENCE_MANAGER::GetReferencesByType(
        PLM_LIB_TYPE aType ) const
{
    std::vector<PLM_LIB_REFERENCE> result;

    for( const auto& [id, ref] : m_references )
    {
        if( ref.lib_type == aType )
            result.push_back( ref );
    }

    return result;
}


size_t PLM_LIB_REFERENCE_MANAGER::GetReferenceCount() const
{
    return m_references.size();
}


void PLM_LIB_REFERENCE_MANAGER::UpdateRevision( const wxString& aLibId,
                                                 const wxString& aNewRevision )
{
    auto it = m_references.find( aLibId );

    if( it != m_references.end() )
    {
        it->second.revision = aNewRevision;
        m_modified = true;
    }
}


void PLM_LIB_REFERENCE_MANAGER::UpdateLocalPath( const wxString& aLibId,
                                                  const wxString& aLocalPath )
{
    auto it = m_references.find( aLibId );

    if( it != m_references.end() )
    {
        it->second.local_path = aLocalPath;
        m_modified = true;
    }
}


void PLM_LIB_REFERENCE_MANAGER::SetAutoSync( const wxString& aLibId, bool aAutoSync )
{
    auto it = m_references.find( aLibId );

    if( it != m_references.end() )
    {
        it->second.auto_sync = aAutoSync;
        m_modified = true;
    }
}


std::vector<wxString> PLM_LIB_REFERENCE_MANAGER::GetLibrariesToSync() const
{
    std::vector<wxString> result;

    for( const auto& [id, ref] : m_references )
    {
        if( ref.auto_sync )
            result.push_back( id );
    }

    return result;
}


void PLM_LIB_REFERENCE_MANAGER::MarkSynced( const wxString& aLibId )
{
    auto it = m_references.find( aLibId );

    if( it != m_references.end() )
    {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        it->second.sync_timestamp =
                std::chrono::duration_cast<std::chrono::milliseconds>( duration ).count();
        m_modified = true;
    }
}


void PLM_LIB_REFERENCE_MANAGER::Clear()
{
    if( !m_references.empty() )
    {
        m_references.clear();
        m_modified = true;
    }
}


wxString PLM_LIB_REFERENCE_MANAGER::ToJson() const
{
    nlohmann::json j;
    j["version"] = 1;
    j["libraries"] = nlohmann::json::array();

    for( const auto& [id, ref] : m_references )
    {
        nlohmann::json lib;
        lib["lib_id"] = ref.lib_id.ToStdString();
        lib["lib_name"] = ref.lib_name.ToStdString();
        lib["lib_type"] = ref.GetTypeString().ToStdString();
        lib["revision"] = ref.revision.ToStdString();
        lib["local_path"] = ref.local_path.ToStdString();
        lib["server_url"] = ref.server_url.ToStdString();
        lib["sync_timestamp"] = ref.sync_timestamp;
        lib["auto_sync"] = ref.auto_sync;

        j["libraries"].push_back( lib );
    }

    return wxString::FromUTF8( j.dump( 2 ) );
}


bool PLM_LIB_REFERENCE_MANAGER::FromJson( const wxString& aJson )
{
    try
    {
        nlohmann::json j = nlohmann::json::parse( aJson.ToStdString() );

        if( !j.contains( "version" ) || j["version"].get<int>() > 1 )
            return false;

        m_references.clear();

        if( j.contains( "libraries" ) && j["libraries"].is_array() )
        {
            for( const auto& lib : j["libraries"] )
            {
                PLM_LIB_REFERENCE ref;

                if( lib.contains( "lib_id" ) )
                    ref.lib_id = wxString::FromUTF8( lib["lib_id"].get<std::string>() );

                if( lib.contains( "lib_name" ) )
                    ref.lib_name = wxString::FromUTF8( lib["lib_name"].get<std::string>() );

                if( lib.contains( "lib_type" ) )
                    ref.lib_type =
                            PLM_LIB_REFERENCE::ParseType(
                                    wxString::FromUTF8( lib["lib_type"].get<std::string>() ) );

                if( lib.contains( "revision" ) )
                    ref.revision = wxString::FromUTF8( lib["revision"].get<std::string>() );

                if( lib.contains( "local_path" ) )
                    ref.local_path = wxString::FromUTF8( lib["local_path"].get<std::string>() );

                if( lib.contains( "server_url" ) )
                    ref.server_url = wxString::FromUTF8( lib["server_url"].get<std::string>() );

                if( lib.contains( "sync_timestamp" ) )
                    ref.sync_timestamp = lib["sync_timestamp"].get<int64_t>();

                if( lib.contains( "auto_sync" ) )
                    ref.auto_sync = lib["auto_sync"].get<bool>();

                if( !ref.lib_id.IsEmpty() )
                    m_references[ref.lib_id] = ref;
            }
        }

        m_modified = false;
        return true;
    }
    catch( const nlohmann::json::exception& e )
    {
        wxLogError( wxS( "Failed to parse library reference JSON: %s" ), e.what() );
        return false;
    }
}


bool PLM_LIB_REFERENCE_MANAGER::SaveToFile( const wxString& aPath ) const
{
    wxString json = ToJson();

    wxFFile file( aPath, wxS( "w" ) );

    if( !file.IsOpened() )
    {
        wxLogError( wxS( "Failed to open file for writing: %s" ), aPath );
        return false;
    }

    if( !file.Write( json ) )
    {
        wxLogError( wxS( "Failed to write library references to file: %s" ), aPath );
        return false;
    }

    return true;
}


bool PLM_LIB_REFERENCE_MANAGER::LoadFromFile( const wxString& aPath )
{
    if( !wxFileName::FileExists( aPath ) )
        return false;

    wxFFile file( aPath, wxS( "r" ) );

    if( !file.IsOpened() )
    {
        wxLogError( wxS( "Failed to open library reference file: %s" ), aPath );
        return false;
    }

    wxString json;

    if( !file.ReadAll( &json ) )
    {
        wxLogError( wxS( "Failed to read library reference file: %s" ), aPath );
        return false;
    }

    return FromJson( json );
}


wxString PLM_LIB_REFERENCE_MANAGER::GetDefaultPath( const wxString& aProjectPath )
{
    wxFileName fn( aProjectPath );
    fn.SetName( fn.GetName() + wxS( "-plm-lib-refs" ) );
    fn.SetExt( wxS( "json" ) );
    return fn.GetFullPath();
}


wxString PLM_LIB_REFERENCE_MANAGER::GetTypeDisplayName( PLM_LIB_TYPE aType )
{
    switch( aType )
    {
    case PLM_LIB_TYPE::SYMBOL:    return _( "Symbol Library" );
    case PLM_LIB_TYPE::FOOTPRINT: return _( "Footprint Library" );
    case PLM_LIB_TYPE::MODEL_3D:  return _( "3D Model Library" );
    default:                      return _( "Unknown" );
    }
}
