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

#include "plm_icon_provider.h"

#include <wx/dcmemory.h>
#include <wx/image.h>
#include <wx/bmpbndl.h>


PLM_ICON_PROVIDER& PLM_ICON_PROVIDER::GetInstance()
{
    static PLM_ICON_PROVIDER instance;
    return instance;
}


PLM_ICON_PROVIDER::PLM_ICON_PROVIDER()
{
    initCoreIcons();
}


PLM_ICON_PROVIDER::~PLM_ICON_PROVIDER()
{
}


void PLM_ICON_PROVIDER::initCoreIcons()
{
    m_coreIcons[PLM_ICON_STATE::UNMANAGED] =
            PLM_ICON_INFO( wxS( "plm_unmanaged" ), wxS( "Not under PLM control" ),
                           PLM_ICON_STATE::UNMANAGED );

    m_coreIcons[PLM_ICON_STATE::CLEAN] =
            PLM_ICON_INFO( wxS( "plm_clean" ), wxS( "In sync with server" ),
                           PLM_ICON_STATE::CLEAN );

    m_coreIcons[PLM_ICON_STATE::MODIFIED] =
            PLM_ICON_INFO( wxS( "plm_modified" ), wxS( "Local modifications" ),
                           PLM_ICON_STATE::MODIFIED );

    m_coreIcons[PLM_ICON_STATE::LOCKED_BY_ME] =
            PLM_ICON_INFO( wxS( "plm_locked_me" ), wxS( "Locked by you" ),
                           PLM_ICON_STATE::LOCKED_BY_ME );

    m_coreIcons[PLM_ICON_STATE::LOCKED_BY_OTHER] =
            PLM_ICON_INFO( wxS( "plm_locked_other" ), wxS( "Locked by another user" ),
                           PLM_ICON_STATE::LOCKED_BY_OTHER );

    m_coreIcons[PLM_ICON_STATE::CONFLICT] =
            PLM_ICON_INFO( wxS( "plm_conflict" ), wxS( "Conflict with server version" ),
                           PLM_ICON_STATE::CONFLICT );

    m_coreIcons[PLM_ICON_STATE::OUTDATED] =
            PLM_ICON_INFO( wxS( "plm_outdated" ), wxS( "Server has newer version" ),
                           PLM_ICON_STATE::OUTDATED );

    m_coreIcons[PLM_ICON_STATE::PENDING_UPLOAD] =
            PLM_ICON_INFO( wxS( "plm_pending" ), wxS( "Changes pending upload" ),
                           PLM_ICON_STATE::PENDING_UPLOAD );

    m_coreIcons[PLM_ICON_STATE::ERROR_STATE] =
            PLM_ICON_INFO( wxS( "plm_error" ), wxS( "Error state" ),
                           PLM_ICON_STATE::ERROR_STATE );
}


wxBitmap PLM_ICON_PROVIDER::GetIcon( PLM_ICON_STATE aState, int aSize ) const
{
    auto cacheKey = std::make_pair( aState, aSize );
    auto cacheIt = m_iconCache.find( cacheKey );

    if( cacheIt != m_iconCache.end() )
        return cacheIt->second;

    wxBitmap bitmap = createPlaceholderIcon( aState, aSize );
    m_iconCache[cacheKey] = bitmap;

    return bitmap;
}


wxBitmap PLM_ICON_PROVIDER::GetIconById( const wxString& aIconId, int aSize ) const
{
    for( const auto& [state, info] : m_coreIcons )
    {
        if( info.icon_id == aIconId )
            return GetIcon( state, aSize );
    }

    auto pluginIt = m_pluginIcons.find( aIconId );

    if( pluginIt != m_pluginIcons.end() )
    {
        auto loaderIt = m_pluginLoaders.find( aIconId );

        if( loaderIt != m_pluginLoaders.end() )
            return loaderIt->second( aIconId, aSize );
    }

    return createPlaceholderIcon( PLM_ICON_STATE::UNMANAGED, aSize );
}


wxBitmapBundle PLM_ICON_PROVIDER::GetIconBundle( PLM_ICON_STATE aState ) const
{
    wxVector<wxBitmap> bitmaps;

    bitmaps.push_back( GetIcon( aState, 16 ) );
    bitmaps.push_back( GetIcon( aState, 24 ) );
    bitmaps.push_back( GetIcon( aState, 32 ) );

    return wxBitmapBundle::FromBitmaps( bitmaps );
}


PLM_ICON_STATE PLM_ICON_PROVIDER::GetStateFromFileInfo( PLM_STATE_CATEGORY aStateCategory,
                                                         bool aIsLocked,
                                                         const wxString& aLockOwner,
                                                         bool aHasLocalChanges,
                                                         bool aHasConflict )
{
    if( aHasConflict )
        return PLM_ICON_STATE::CONFLICT;

    if( aIsLocked )
    {
        if( aLockOwner.IsEmpty() )
            return PLM_ICON_STATE::LOCKED_BY_ME;
        else
            return PLM_ICON_STATE::LOCKED_BY_OTHER;
    }

    if( aHasLocalChanges )
        return PLM_ICON_STATE::MODIFIED;

    switch( aStateCategory )
    {
    case PLM_STATE_CATEGORY::EDITABLE:
        return PLM_ICON_STATE::CLEAN;

    case PLM_STATE_CATEGORY::LOCKED:
        return PLM_ICON_STATE::LOCKED_BY_OTHER;

    case PLM_STATE_CATEGORY::RELEASED:
        return PLM_ICON_STATE::CLEAN;

    case PLM_STATE_CATEGORY::PENDING_REVIEW:
        return PLM_ICON_STATE::LOCKED_BY_OTHER;

    default:
        return PLM_ICON_STATE::UNMANAGED;
    }
}


bool PLM_ICON_PROVIDER::RegisterPluginIcon( const wxString& aIconId,
                                             const wxString& aDescription,
                                             PLM_ICON_LOADER aLoader )
{
    if( aIconId.IsEmpty() || !aLoader )
        return false;

    if( m_pluginIcons.find( aIconId ) != m_pluginIcons.end() )
        return false;

    PLM_ICON_INFO info;
    info.icon_id = aIconId;
    info.description = aDescription;
    info.from_plugin = true;
    info.state = PLM_ICON_STATE::UNMANAGED;

    m_pluginIcons[aIconId] = info;
    m_pluginLoaders[aIconId] = aLoader;

    return true;
}


void PLM_ICON_PROVIDER::UnregisterPluginIcon( const wxString& aIconId )
{
    m_pluginIcons.erase( aIconId );
    m_pluginLoaders.erase( aIconId );
}


std::map<wxString, PLM_ICON_INFO> PLM_ICON_PROVIDER::GetRegisteredIcons() const
{
    std::map<wxString, PLM_ICON_INFO> result;

    for( const auto& [state, info] : m_coreIcons )
        result[info.icon_id] = info;

    for( const auto& [id, info] : m_pluginIcons )
        result[id] = info;

    return result;
}


wxString PLM_ICON_PROVIDER::GetStateText( PLM_ICON_STATE aState )
{
    switch( aState )
    {
    case PLM_ICON_STATE::UNMANAGED:       return _( "Not Managed" );
    case PLM_ICON_STATE::CLEAN:           return _( "Up to Date" );
    case PLM_ICON_STATE::MODIFIED:        return _( "Modified" );
    case PLM_ICON_STATE::LOCKED_BY_ME:    return _( "Locked by You" );
    case PLM_ICON_STATE::LOCKED_BY_OTHER: return _( "Locked" );
    case PLM_ICON_STATE::CONFLICT:        return _( "Conflict" );
    case PLM_ICON_STATE::OUTDATED:        return _( "Outdated" );
    case PLM_ICON_STATE::PENDING_UPLOAD:  return _( "Pending Upload" );
    case PLM_ICON_STATE::ERROR_STATE:     return _( "Error" );
    default:                              return _( "Unknown" );
    }
}


wxString PLM_ICON_PROVIDER::GetStateTooltip( PLM_ICON_STATE aState )
{
    switch( aState )
    {
    case PLM_ICON_STATE::UNMANAGED:
        return _( "This file is not managed by the PLM system" );

    case PLM_ICON_STATE::CLEAN:
        return _( "This file is synchronized with the PLM server" );

    case PLM_ICON_STATE::MODIFIED:
        return _( "This file has local modifications that have not been checked in" );

    case PLM_ICON_STATE::LOCKED_BY_ME:
        return _( "You have locked this file for editing" );

    case PLM_ICON_STATE::LOCKED_BY_OTHER:
        return _( "This file is locked by another user" );

    case PLM_ICON_STATE::CONFLICT:
        return _( "This file has conflicting changes between local and server versions" );

    case PLM_ICON_STATE::OUTDATED:
        return _( "A newer version is available on the PLM server" );

    case PLM_ICON_STATE::PENDING_UPLOAD:
        return _( "Local changes are ready to be uploaded to the PLM server" );

    case PLM_ICON_STATE::ERROR_STATE:
        return _( "An error occurred while checking PLM status" );

    default:
        return wxEmptyString;
    }
}


bool PLM_ICON_PROVIDER::StateNeedsAttention( PLM_ICON_STATE aState )
{
    switch( aState )
    {
    case PLM_ICON_STATE::CONFLICT:
    case PLM_ICON_STATE::LOCKED_BY_OTHER:
    case PLM_ICON_STATE::OUTDATED:
    case PLM_ICON_STATE::ERROR_STATE:
        return true;

    default:
        return false;
    }
}


void PLM_ICON_PROVIDER::ClearCache()
{
    m_iconCache.clear();
}


wxBitmap PLM_ICON_PROVIDER::createPlaceholderIcon( PLM_ICON_STATE aState, int aSize ) const
{
    wxBitmap bitmap( aSize, aSize, 32 );
    bitmap.UseAlpha();

    wxMemoryDC dc( bitmap );
    dc.SetBackground( *wxTRANSPARENT_BRUSH );
    dc.Clear();

    wxColour color = getStateColor( aState );

    int margin = aSize / 8;
    int circleSize = aSize - 2 * margin;

    dc.SetPen( wxPen( color, 1 ) );
    dc.SetBrush( wxBrush( color ) );

    dc.DrawCircle( aSize / 2, aSize / 2, circleSize / 2 );

    if( aState == PLM_ICON_STATE::LOCKED_BY_ME || aState == PLM_ICON_STATE::LOCKED_BY_OTHER )
    {
        dc.SetPen( *wxWHITE_PEN );
        int lockY = aSize / 2 - aSize / 8;
        int lockW = aSize / 3;
        int lockH = aSize / 4;
        dc.DrawRectangle( aSize / 2 - lockW / 2, lockY, lockW, lockH );

        int archY = lockY - aSize / 8;
        dc.DrawArc( aSize / 2 - lockW / 4, archY, aSize / 2 + lockW / 4, archY,
                    aSize / 2, lockY );
    }
    else if( aState == PLM_ICON_STATE::CLEAN )
    {
        dc.SetPen( wxPen( *wxWHITE, 2 ) );
        int checkX = aSize / 3;
        int checkY = aSize / 2;
        dc.DrawLine( checkX, checkY, aSize / 2, aSize * 2 / 3 );
        dc.DrawLine( aSize / 2, aSize * 2 / 3, aSize * 2 / 3, aSize / 3 );
    }
    else if( aState == PLM_ICON_STATE::MODIFIED || aState == PLM_ICON_STATE::PENDING_UPLOAD )
    {
        dc.SetPen( wxPen( *wxWHITE, 2 ) );
        dc.DrawLine( aSize / 2, aSize / 3, aSize / 2, aSize / 2 + 2 );
        dc.DrawPoint( aSize / 2, aSize * 2 / 3 );
    }
    else if( aState == PLM_ICON_STATE::CONFLICT || aState == PLM_ICON_STATE::ERROR_STATE )
    {
        dc.SetPen( wxPen( *wxWHITE, 2 ) );
        dc.DrawLine( aSize / 3, aSize / 3, aSize * 2 / 3, aSize * 2 / 3 );
        dc.DrawLine( aSize * 2 / 3, aSize / 3, aSize / 3, aSize * 2 / 3 );
    }
    else if( aState == PLM_ICON_STATE::OUTDATED )
    {
        dc.SetPen( wxPen( *wxWHITE, 2 ) );
        dc.DrawLine( aSize / 2, aSize / 3, aSize / 2, aSize * 2 / 3 );
        dc.DrawLine( aSize / 3, aSize / 2, aSize / 2, aSize / 3 );
        dc.DrawLine( aSize * 2 / 3, aSize / 2, aSize / 2, aSize / 3 );
    }

    dc.SelectObject( wxNullBitmap );

    return bitmap;
}


wxColour PLM_ICON_PROVIDER::getStateColor( PLM_ICON_STATE aState ) const
{
    switch( aState )
    {
    case PLM_ICON_STATE::UNMANAGED:       return wxColour( 128, 128, 128 );
    case PLM_ICON_STATE::CLEAN:           return wxColour( 76, 175, 80 );
    case PLM_ICON_STATE::MODIFIED:        return wxColour( 255, 193, 7 );
    case PLM_ICON_STATE::LOCKED_BY_ME:    return wxColour( 33, 150, 243 );
    case PLM_ICON_STATE::LOCKED_BY_OTHER: return wxColour( 244, 67, 54 );
    case PLM_ICON_STATE::CONFLICT:        return wxColour( 244, 67, 54 );
    case PLM_ICON_STATE::OUTDATED:        return wxColour( 255, 152, 0 );
    case PLM_ICON_STATE::PENDING_UPLOAD:  return wxColour( 156, 39, 176 );
    case PLM_ICON_STATE::ERROR_STATE:     return wxColour( 244, 67, 54 );
    default:                              return wxColour( 128, 128, 128 );
    }
}
