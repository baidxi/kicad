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

#include "plm_dirty_tracker.h"

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/log.h>

#include <picosha2.h>


PLM_DIRTY_TRACKER& PLM_DIRTY_TRACKER::GetInstance()
{
    static PLM_DIRTY_TRACKER instance;
    return instance;
}


PLM_DIRTY_TRACKER::PLM_DIRTY_TRACKER()
{
}


wxString PLM_DIRTY_TRACKER::NormalizePath( const wxString& aPath )
{
    wxFileName fn( aPath );
    fn.Normalize( wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE );
    return fn.GetFullPath();
}


wxString PLM_DIRTY_TRACKER::CalculateFileHash( const wxString& aFilePath )
{
    if( !wxFileExists( aFilePath ) )
        return wxEmptyString;

    wxFile file( aFilePath, wxFile::read );

    if( !file.IsOpened() )
        return wxEmptyString;

    wxFileOffset fileSize = file.Length();

    if( fileSize <= 0 )
        return wxEmptyString;

    std::vector<unsigned char> buffer( static_cast<size_t>( fileSize ) );

    if( file.Read( buffer.data(), fileSize ) != fileSize )
        return wxEmptyString;

    std::string hash = picosha2::hash256_hex_string( buffer );
    return wxString::FromUTF8( hash );
}


void PLM_DIRTY_TRACKER::RecordCheckout( const wxString& aFilePath, const wxString& aCheckoutHash )
{
    wxString key = NormalizePath( aFilePath );

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        PLM_FILE_STATE& state = m_trackedFiles[key];
        state.file_path = key;
        state.checkout_hash = aCheckoutHash;
        state.checkout_time = std::time( nullptr );
        state.state = PLM_DIRTY_STATE::CLEAN;

        // Calculate current hash
        state.current_hash = CalculateFileHash( key );

        if( state.current_hash.IsEmpty() )
            state.state = PLM_DIRTY_STATE::DELETED;
        else if( state.current_hash != state.checkout_hash )
            state.state = PLM_DIRTY_STATE::MODIFIED;

        state.last_checked = std::time( nullptr );
    }

    wxLogMessage( wxS( "PLM: Recorded checkout for %s" ), aFilePath );
}


void PLM_DIRTY_TRACKER::TrackNewFile( const wxString& aFilePath )
{
    wxString key = NormalizePath( aFilePath );

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        PLM_FILE_STATE& state = m_trackedFiles[key];
        state.file_path = key;
        state.state = PLM_DIRTY_STATE::NEW;
        state.current_hash = CalculateFileHash( key );
        state.last_checked = std::time( nullptr );
    }

    wxLogMessage( wxS( "PLM: Tracking new file %s" ), aFilePath );
    FireStateChange( aFilePath, PLM_DIRTY_STATE::NEW );
}


void PLM_DIRTY_TRACKER::StopTracking( const wxString& aFilePath )
{
    wxString key = NormalizePath( aFilePath );

    std::lock_guard<std::mutex> lock( m_mutex );
    m_trackedFiles.erase( key );
}


void PLM_DIRTY_TRACKER::StopTrackingAll()
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_trackedFiles.clear();
}


bool PLM_DIRTY_TRACKER::IsTracking( const wxString& aFilePath ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );
    wxString key = NormalizePath( aFilePath );
    return m_trackedFiles.find( key ) != m_trackedFiles.end();
}


PLM_DIRTY_STATE PLM_DIRTY_TRACKER::GetState( const wxString& aFilePath ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizePath( aFilePath );
    auto it = m_trackedFiles.find( key );

    if( it == m_trackedFiles.end() )
        return PLM_DIRTY_STATE::UNKNOWN;

    return it->second.state;
}


bool PLM_DIRTY_TRACKER::GetFileState( const wxString& aFilePath, PLM_FILE_STATE& aState ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    wxString key = NormalizePath( aFilePath );
    auto it = m_trackedFiles.find( key );

    if( it == m_trackedFiles.end() )
        return false;

    aState = it->second;
    return true;
}


bool PLM_DIRTY_TRACKER::IsModified( const wxString& aFilePath ) const
{
    PLM_DIRTY_STATE state = GetState( aFilePath );

    return state == PLM_DIRTY_STATE::MODIFIED ||
           state == PLM_DIRTY_STATE::NEW ||
           state == PLM_DIRTY_STATE::DELETED;
}


bool PLM_DIRTY_TRACKER::HasModifications() const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    for( const auto& [key, state] : m_trackedFiles )
    {
        if( state.IsDirty() )
            return true;
    }

    return false;
}


std::vector<wxString> PLM_DIRTY_TRACKER::GetTrackedFiles() const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    std::vector<wxString> files;
    files.reserve( m_trackedFiles.size() );

    for( const auto& [key, state] : m_trackedFiles )
        files.push_back( key );

    return files;
}


std::vector<wxString> PLM_DIRTY_TRACKER::GetModifiedFiles() const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    std::vector<wxString> files;

    for( const auto& [key, state] : m_trackedFiles )
    {
        if( state.IsDirty() )
            files.push_back( key );
    }

    return files;
}


std::vector<wxString> PLM_DIRTY_TRACKER::GetFilesByState( PLM_DIRTY_STATE aState ) const
{
    std::lock_guard<std::mutex> lock( m_mutex );

    std::vector<wxString> files;

    for( const auto& [key, state] : m_trackedFiles )
    {
        if( state.state == aState )
            files.push_back( key );
    }

    return files;
}


void PLM_DIRTY_TRACKER::UpdateState( PLM_FILE_STATE& aState )
{
    aState.current_hash = CalculateFileHash( aState.file_path );
    aState.last_checked = std::time( nullptr );

    if( aState.current_hash.IsEmpty() )
    {
        // File doesn't exist
        if( aState.state != PLM_DIRTY_STATE::NEW )
            aState.state = PLM_DIRTY_STATE::DELETED;
    }
    else if( aState.state == PLM_DIRTY_STATE::NEW )
    {
        // New files stay new until checked in
    }
    else if( aState.state == PLM_DIRTY_STATE::CONFLICT )
    {
        // Conflict stays until cleared
    }
    else if( aState.checkout_hash.IsEmpty() )
    {
        // No checkout hash means file wasn't from PLM
        aState.state = PLM_DIRTY_STATE::NEW;
    }
    else if( aState.current_hash == aState.checkout_hash )
    {
        aState.state = PLM_DIRTY_STATE::CLEAN;
    }
    else
    {
        aState.state = PLM_DIRTY_STATE::MODIFIED;
    }
}


void PLM_DIRTY_TRACKER::OnFileSaved( const wxString& aFilePath )
{
    wxString key = NormalizePath( aFilePath );
    PLM_DIRTY_STATE newState;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_trackedFiles.find( key );

        if( it == m_trackedFiles.end() )
            return;

        PLM_DIRTY_STATE oldState = it->second.state;
        UpdateState( it->second );
        newState = it->second.state;

        if( oldState == newState )
            return;
    }

    FireStateChange( aFilePath, newState );
}


void PLM_DIRTY_TRACKER::OnFileDeleted( const wxString& aFilePath )
{
    wxString key = NormalizePath( aFilePath );

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_trackedFiles.find( key );

        if( it == m_trackedFiles.end() )
            return;

        if( it->second.state == PLM_DIRTY_STATE::NEW )
        {
            // New files that are deleted are just removed from tracking
            m_trackedFiles.erase( it );
            return;
        }

        it->second.state = PLM_DIRTY_STATE::DELETED;
        it->second.current_hash.Clear();
        it->second.last_checked = std::time( nullptr );
    }

    FireStateChange( aFilePath, PLM_DIRTY_STATE::DELETED );
}


void PLM_DIRTY_TRACKER::RefreshState( const wxString& aFilePath )
{
    wxString key = NormalizePath( aFilePath );
    PLM_DIRTY_STATE newState;
    PLM_DIRTY_STATE oldState;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_trackedFiles.find( key );

        if( it == m_trackedFiles.end() )
            return;

        oldState = it->second.state;
        UpdateState( it->second );
        newState = it->second.state;
    }

    if( oldState != newState )
        FireStateChange( aFilePath, newState );
}


void PLM_DIRTY_TRACKER::RefreshAllStates()
{
    std::vector<std::pair<wxString, PLM_DIRTY_STATE>> changes;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        for( auto& [key, state] : m_trackedFiles )
        {
            PLM_DIRTY_STATE oldState = state.state;
            UpdateState( state );

            if( oldState != state.state )
                changes.emplace_back( key, state.state );
        }
    }

    for( const auto& [path, newState] : changes )
        FireStateChange( path, newState );
}


void PLM_DIRTY_TRACKER::MarkClean( const wxString& aFilePath, const wxString& aNewHash )
{
    wxString key = NormalizePath( aFilePath );

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_trackedFiles.find( key );

        if( it == m_trackedFiles.end() )
            return;

        if( aNewHash.IsEmpty() )
            it->second.checkout_hash = it->second.current_hash;
        else
            it->second.checkout_hash = aNewHash;

        it->second.state = PLM_DIRTY_STATE::CLEAN;
        it->second.checkout_time = std::time( nullptr );
    }

    FireStateChange( aFilePath, PLM_DIRTY_STATE::CLEAN );
}


void PLM_DIRTY_TRACKER::MarkConflict( const wxString& aFilePath )
{
    wxString key = NormalizePath( aFilePath );

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_trackedFiles.find( key );

        if( it == m_trackedFiles.end() )
            return;

        it->second.state = PLM_DIRTY_STATE::CONFLICT;
    }

    FireStateChange( aFilePath, PLM_DIRTY_STATE::CONFLICT );
}


void PLM_DIRTY_TRACKER::ClearConflict( const wxString& aFilePath )
{
    wxString key = NormalizePath( aFilePath );
    PLM_DIRTY_STATE newState;

    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto it = m_trackedFiles.find( key );

        if( it == m_trackedFiles.end() )
            return;

        if( it->second.state != PLM_DIRTY_STATE::CONFLICT )
            return;

        UpdateState( it->second );
        newState = it->second.state;
    }

    FireStateChange( aFilePath, newState );
}


void PLM_DIRTY_TRACKER::SetStateCallback( PLM_DIRTY_STATE_CALLBACK aCallback )
{
    std::lock_guard<std::mutex> lock( m_mutex );
    m_stateCallback = std::move( aCallback );
}


void PLM_DIRTY_TRACKER::FireStateChange( const wxString& aFilePath, PLM_DIRTY_STATE aNewState )
{
    PLM_DIRTY_STATE_CALLBACK callback;

    {
        std::lock_guard<std::mutex> lock( m_mutex );
        callback = m_stateCallback;
    }

    if( callback )
        callback( aFilePath, aNewState );
}


wxString PLM_DIRTY_TRACKER::GetStateDisplayName( PLM_DIRTY_STATE aState )
{
    switch( aState )
    {
    case PLM_DIRTY_STATE::CLEAN:    return _( "Clean" );
    case PLM_DIRTY_STATE::MODIFIED: return _( "Modified" );
    case PLM_DIRTY_STATE::NEW:      return _( "New" );
    case PLM_DIRTY_STATE::DELETED:  return _( "Deleted" );
    case PLM_DIRTY_STATE::CONFLICT: return _( "Conflict" );
    case PLM_DIRTY_STATE::UNKNOWN:  return _( "Unknown" );
    }

    return _( "Unknown" );
}
