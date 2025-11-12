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

#include "plm_state_persistence.h"

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>

#include <nlohmann/json.hpp>
#include <picosha2.h>

#include <fstream>
#include <iomanip>
#include <sstream>


using json = nlohmann::json;


wxString PLM_PERSISTENCE_CONFIG::GetStateFilePath( const wxString& aProjectPath ) const
{
    wxString hash = PLM_STATE_PERSISTENCE::ComputePathHash( aProjectPath );
    wxFileName fn( cache_directory, hash, wxS( "json" ) );
    return fn.GetFullPath();
}


PLM_STATE_PERSISTENCE::PLM_STATE_PERSISTENCE()
{
}


void PLM_STATE_PERSISTENCE::SetConfig( const PLM_PERSISTENCE_CONFIG& aConfig )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_config = aConfig;
}


PLM_PERSISTENCE_CONFIG PLM_STATE_PERSISTENCE::GetConfig() const
{
    std::lock_guard<std::mutex> lock( m_mutex );
    return m_config;
}


void PLM_STATE_PERSISTENCE::SetCacheDirectory( const wxString& aCacheDir )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_config.cache_directory = aCacheDir;
}


PLM_PERSIST_RESULT PLM_STATE_PERSISTENCE::SaveState( const wxString& aProjectPath,
                                                      const PLM_PROJECT_STATE& aState )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    if( !EnsureCacheDirectory() )
    {
        wxLogError( wxS( "PLM State: Failed to create cache directory" ) );
        return PLM_PERSIST_RESULT::WRITE_ERROR;
    }

    wxString filePath = m_config.GetStateFilePath( aProjectPath );
    wxString jsonStr = StateToJson( aState );

    try
    {
        wxFileOutputStream output( filePath );

        if( !output.IsOk() )
        {
            wxLogError( wxS( "PLM State: Failed to open %s for writing" ), filePath );
            return PLM_PERSIST_RESULT::WRITE_ERROR;
        }

        wxTextOutputStream text( output );
        text.WriteString( jsonStr );

        wxLogMessage( wxS( "PLM State: Saved state for %s to %s" ), aProjectPath, filePath );
        return PLM_PERSIST_RESULT::SUCCESS;
    }
    catch( const std::exception& e )
    {
        wxLogError( wxS( "PLM State: Exception saving state: %s" ), e.what() );
        return PLM_PERSIST_RESULT::WRITE_ERROR;
    }
}


PLM_PERSIST_RESULT PLM_STATE_PERSISTENCE::LoadState( const wxString& aProjectPath,
                                                      PLM_PROJECT_STATE& aState ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString filePath = m_config.GetStateFilePath( aProjectPath );

    if( !wxFileExists( filePath ) )
        return PLM_PERSIST_RESULT::FILE_NOT_FOUND;

    try
    {
        wxFileInputStream input( filePath );

        if( !input.IsOk() )
        {
            wxLogError( wxS( "PLM State: Failed to open %s for reading" ), filePath );
            return PLM_PERSIST_RESULT::READ_ERROR;
        }

        wxTextInputStream text( input );
        wxString jsonStr;
        wxString line;

        while( !input.Eof() )
        {
            line = text.ReadLine();
            jsonStr += line + wxS( "\n" );
        }

        PLM_PERSIST_RESULT result = JsonToState( jsonStr, aState );

        if( result == PLM_PERSIST_RESULT::SUCCESS )
        {
            wxLogMessage( wxS( "PLM State: Loaded state for %s from %s" ), aProjectPath, filePath );

            if( m_config.validate_on_load && !aState.IsValid() )
            {
                wxLogWarning( wxS( "PLM State: Loaded state is invalid" ) );
            }
        }

        return result;
    }
    catch( const std::exception& e )
    {
        wxLogError( wxS( "PLM State: Exception loading state: %s" ), e.what() );
        return PLM_PERSIST_RESULT::READ_ERROR;
    }
}


bool PLM_STATE_PERSISTENCE::DeleteState( const wxString& aProjectPath )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString filePath = m_config.GetStateFilePath( aProjectPath );

    if( !wxFileExists( filePath ) )
        return true;

    bool removed = wxRemoveFile( filePath );

    if( removed )
        wxLogMessage( wxS( "PLM State: Deleted state for %s" ), aProjectPath );
    else
        wxLogError( wxS( "PLM State: Failed to delete %s" ), filePath );

    return removed;
}


bool PLM_STATE_PERSISTENCE::HasState( const wxString& aProjectPath ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString filePath = m_config.GetStateFilePath( aProjectPath );
    return wxFileExists( filePath );
}


std::vector<wxString> PLM_STATE_PERSISTENCE::GetPersistedProjects() const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    std::vector<wxString> projects;

    if( m_config.cache_directory.IsEmpty() || !wxDirExists( m_config.cache_directory ) )
        return projects;

    wxDir dir( m_config.cache_directory );

    if( !dir.IsOpened() )
        return projects;

    wxString filename;
    bool cont = dir.GetFirst( &filename, wxS( "*.json" ), wxDIR_FILES );

    while( cont )
    {
        // State files don't contain the original path in the filename, so we can't
        // recover the project path. The caller should track project paths separately.
        wxFileName fn( m_config.cache_directory, filename );
        projects.push_back( fn.GetFullPath() );
        cont = dir.GetNext( &filename );
    }

    return projects;
}


size_t PLM_STATE_PERSISTENCE::DeleteAllState()
{
    std::lock_guard<std::mutex> lock( m_mutex );

    size_t count = 0;

    if( m_config.cache_directory.IsEmpty() || !wxDirExists( m_config.cache_directory ) )
        return count;

    wxDir dir( m_config.cache_directory );

    if( !dir.IsOpened() )
        return count;

    wxString filename;
    bool cont = dir.GetFirst( &filename, wxS( "*.json" ), wxDIR_FILES );

    while( cont )
    {
        wxString filePath = wxFileName( m_config.cache_directory, filename ).GetFullPath();

        if( wxRemoveFile( filePath ) )
            count++;

        cont = dir.GetNext( &filename );
    }

    wxLogMessage( wxS( "PLM State: Deleted %zu state files" ), count );
    return count;
}


size_t PLM_STATE_PERSISTENCE::CleanupStaleState( int64_t aMaxAgeSeconds )
{
    std::lock_guard<std::mutex> lock( m_mutex );

    size_t count = 0;
    int64_t now = std::time( nullptr );
    int64_t cutoff = now - aMaxAgeSeconds;

    if( m_config.cache_directory.IsEmpty() || !wxDirExists( m_config.cache_directory ) )
        return count;

    wxDir dir( m_config.cache_directory );

    if( !dir.IsOpened() )
        return count;

    wxString filename;
    bool cont = dir.GetFirst( &filename, wxS( "*.json" ), wxDIR_FILES );

    while( cont )
    {
        wxString filePath = wxFileName( m_config.cache_directory, filename ).GetFullPath();

        // Check file modification time
        wxFileName fn( filePath );
        wxDateTime modTime = fn.GetModificationTime();

        if( modTime.IsValid() && modTime.GetTicks() < cutoff )
        {
            if( wxRemoveFile( filePath ) )
                count++;
        }

        cont = dir.GetNext( &filename );
    }

    if( count > 0 )
        wxLogMessage( wxS( "PLM State: Cleaned up %zu stale state files" ), count );

    return count;
}


wxString PLM_STATE_PERSISTENCE::GetStateFilePath( const wxString& aProjectPath ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );
    return m_config.GetStateFilePath( aProjectPath );
}


wxString PLM_STATE_PERSISTENCE::ComputePathHash( const wxString& aProjectPath )
{
    // Use SHA-256 for collision resistance. std::hash only provides 64 bits and no
    // uniqueness guarantees, which could cause state file collisions between projects.
    std::string pathUtf8 = aProjectPath.ToStdString( wxConvUTF8 );
    std::string hash = picosha2::hash256_hex_string( pathUtf8 );

    // Return first 32 hex characters (128 bits) for reasonable filename length
    // while maintaining extremely low collision probability
    return wxString::FromUTF8( hash.substr( 0, 32 ) );
}


bool PLM_STATE_PERSISTENCE::EnsureCacheDirectory() const
{
    if( m_config.cache_directory.IsEmpty() )
        return false;

    if( wxDirExists( m_config.cache_directory ) )
        return true;

    return wxFileName::Mkdir( m_config.cache_directory, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );
}


wxString PLM_STATE_PERSISTENCE::StateToJson( const PLM_PROJECT_STATE& aState ) const
{
    json j;

    j["version"] = aState.version;
    j["saved_at"] = aState.saved_at > 0 ? aState.saved_at : std::time( nullptr );

    // Binding
    json binding;
    binding["plugin_id"] = aState.binding.plugin_id.ToStdString();
    binding["server_url"] = aState.binding.server_url.ToStdString();
    binding["project_id"] = aState.binding.project_id.ToStdString();
    binding["project_path"] = aState.binding.project_path.ToStdString();
    binding["last_revision"] = aState.binding.last_revision.ToStdString();
    binding["last_sync"] = aState.binding.last_sync;
    binding["checkout_id"] = aState.binding.checkout_id.ToStdString();
    j["binding"] = binding;

    // Sync info
    json sync;
    sync["project_path"] = aState.sync_info.project_path.ToStdString();
    sync["state"] = static_cast<int>( aState.sync_info.state );
    sync["local_revision"] = aState.sync_info.local_revision.ToStdString();
    sync["server_revision"] = aState.sync_info.server_revision.ToStdString();
    sync["last_sync"] = aState.sync_info.last_sync;
    sync["last_checked"] = aState.sync_info.last_checked;
    sync["has_local_changes"] = aState.sync_info.has_local_changes;
    sync["has_server_changes"] = aState.sync_info.has_server_changes;
    j["sync_info"] = sync;

    // File states
    json files = json::array();

    for( const auto& [path, fileState] : aState.file_states )
    {
        json fs;
        fs["file_path"] = fileState.file_path.ToStdString();
        fs["state"] = static_cast<int>( fileState.state );
        fs["checkout_hash"] = fileState.checkout_hash.ToStdString();
        fs["current_hash"] = fileState.current_hash.ToStdString();
        fs["checkout_time"] = fileState.checkout_time;
        fs["last_modified"] = fileState.last_modified;
        fs["last_checked"] = fileState.last_checked;
        files.push_back( fs );
    }

    j["file_states"] = files;

    return wxString::FromUTF8( j.dump( 2 ) );
}


PLM_PERSIST_RESULT PLM_STATE_PERSISTENCE::JsonToState( const wxString& aJson,
                                                        PLM_PROJECT_STATE& aState ) const
{
    try
    {
        json j = json::parse( aJson.ToStdString() );

        aState.Clear();

        // Version check
        if( j.contains( "version" ) )
        {
            aState.version = j["version"].get<int>();

            if( aState.version > m_config.format_version )
                return PLM_PERSIST_RESULT::VERSION_MISMATCH;
        }

        if( j.contains( "saved_at" ) )
            aState.saved_at = j["saved_at"].get<int64_t>();

        // Binding
        if( j.contains( "binding" ) )
        {
            const json& binding = j["binding"];

            if( binding.contains( "plugin_id" ) )
                aState.binding.plugin_id = wxString::FromUTF8( binding["plugin_id"].get<std::string>() );

            if( binding.contains( "server_url" ) )
                aState.binding.server_url = wxString::FromUTF8( binding["server_url"].get<std::string>() );

            if( binding.contains( "project_id" ) )
                aState.binding.project_id = wxString::FromUTF8( binding["project_id"].get<std::string>() );

            if( binding.contains( "project_path" ) )
                aState.binding.project_path = wxString::FromUTF8( binding["project_path"].get<std::string>() );

            if( binding.contains( "last_revision" ) )
                aState.binding.last_revision = wxString::FromUTF8( binding["last_revision"].get<std::string>() );

            if( binding.contains( "last_sync" ) )
                aState.binding.last_sync = binding["last_sync"].get<int64_t>();

            if( binding.contains( "checkout_id" ) )
                aState.binding.checkout_id = wxString::FromUTF8( binding["checkout_id"].get<std::string>() );
        }

        // Sync info
        if( j.contains( "sync_info" ) )
        {
            const json& sync = j["sync_info"];

            if( sync.contains( "project_path" ) )
                aState.sync_info.project_path = wxString::FromUTF8( sync["project_path"].get<std::string>() );

            if( sync.contains( "state" ) )
                aState.sync_info.state = static_cast<PLM_SYNC_STATE>( sync["state"].get<int>() );

            if( sync.contains( "local_revision" ) )
                aState.sync_info.local_revision = wxString::FromUTF8( sync["local_revision"].get<std::string>() );

            if( sync.contains( "server_revision" ) )
                aState.sync_info.server_revision = wxString::FromUTF8( sync["server_revision"].get<std::string>() );

            if( sync.contains( "last_sync" ) )
                aState.sync_info.last_sync = sync["last_sync"].get<int64_t>();

            if( sync.contains( "last_checked" ) )
                aState.sync_info.last_checked = sync["last_checked"].get<int64_t>();

            if( sync.contains( "has_local_changes" ) )
                aState.sync_info.has_local_changes = sync["has_local_changes"].get<bool>();

            if( sync.contains( "has_server_changes" ) )
                aState.sync_info.has_server_changes = sync["has_server_changes"].get<bool>();
        }

        // File states
        if( j.contains( "file_states" ) && j["file_states"].is_array() )
        {
            for( const auto& fs : j["file_states"] )
            {
                PLM_FILE_STATE fileState;

                if( fs.contains( "file_path" ) )
                    fileState.file_path = wxString::FromUTF8( fs["file_path"].get<std::string>() );

                if( fs.contains( "state" ) )
                    fileState.state = static_cast<PLM_DIRTY_STATE>( fs["state"].get<int>() );

                if( fs.contains( "checkout_hash" ) )
                    fileState.checkout_hash = wxString::FromUTF8( fs["checkout_hash"].get<std::string>() );

                if( fs.contains( "current_hash" ) )
                    fileState.current_hash = wxString::FromUTF8( fs["current_hash"].get<std::string>() );

                if( fs.contains( "checkout_time" ) )
                    fileState.checkout_time = fs["checkout_time"].get<int64_t>();

                if( fs.contains( "last_modified" ) )
                    fileState.last_modified = fs["last_modified"].get<int64_t>();

                if( fs.contains( "last_checked" ) )
                    fileState.last_checked = fs["last_checked"].get<int64_t>();

                if( !fileState.file_path.IsEmpty() )
                    aState.file_states[fileState.file_path] = fileState;
            }
        }

        return PLM_PERSIST_RESULT::SUCCESS;
    }
    catch( const json::exception& e )
    {
        wxLogError( wxS( "PLM State: JSON parse error: %s" ), e.what() );
        return PLM_PERSIST_RESULT::FILE_CORRUPT;
    }
    catch( const std::exception& e )
    {
        wxLogError( wxS( "PLM State: Parse error: %s" ), e.what() );
        return PLM_PERSIST_RESULT::FILE_CORRUPT;
    }
}
