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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "fast_3d_model_dir_ctrl.h"

#if defined( __WXMSW__ )
#include <windows.h>

#include <cwchar>
#include <vector>
#endif


#if defined( __WXMSW__ )
namespace
{

int driveIcon( const wxString& aPath, UINT aDriveType )
{
    switch( aDriveType )
    {
    case DRIVE_REMOVABLE:
        if( aPath.length() > 0 && wxTolower( aPath[0] ) < wxT( 'c' ) )
            return wxFileIconsTable::floppy;

        return wxFileIconsTable::removeable;

    case DRIVE_CDROM:
        return wxFileIconsTable::cdrom;

    default:
        return wxFileIconsTable::drive;
    }
}


wxString driveDisplayName( const wxString& aPath )
{
    return aPath.Left( 2 );
}

}
#endif


FAST_3D_MODEL_DIR_CTRL::FAST_3D_MODEL_DIR_CTRL( wxWindow* aParent, wxWindowID aId,
                                                const wxString& aDir, const wxPoint& aPos,
                                                const wxSize& aSize, long aStyle,
                                                const wxString& aFilter, int aDefaultFilter,
                                                const wxString& aName )
{
    // Create() calls SetupSections(), so construct in two steps to ensure the override is used.
    Init();
    Create( aParent, aId, aDir, aPos, aSize, aStyle, aFilter, aDefaultFilter, aName );
}


void FAST_3D_MODEL_DIR_CTRL::SetupSections()
{
#if defined( __WXMSW__ )
    auto addDriveSection =
            [this]( const wxString& aPath )
            {
                UINT driveType = GetDriveTypeW( aPath.wc_str() );

                if( driveType == DRIVE_NO_ROOT_DIR )
                    return;

                int      icon = driveIcon( aPath, driveType );
                wxString name = driveDisplayName( aPath );

                AddSection( aPath, name, icon );
            };

    DWORD requiredChars = GetLogicalDriveStringsW( 0, nullptr );

    if( requiredChars )
    {
        std::vector<wchar_t> driveStrings( requiredChars + 1 );
        DWORD copiedChars = GetLogicalDriveStringsW( requiredChars + 1, driveStrings.data() );

        if( copiedChars && copiedChars <= requiredChars )
        {
            for( const wchar_t* driveRoot = driveStrings.data(); *driveRoot;
                 driveRoot += std::wcslen( driveRoot ) + 1 )
            {
                addDriveSection( wxString( driveRoot ) );
            }

            return;
        }
    }

    DWORD drives = GetLogicalDrives();

    for( int ii = 0; ii < 26; ++ii )
    {
        if( !( drives & ( 1u << ii ) ) )
            continue;

        wxChar  drive = static_cast<wxChar>( wxT( 'A' ) + ii );
        wxString path;

        path << drive << wxT( ":\\" );

        addDriveSection( path );
    }
#else
    wxGenericDirCtrl::SetupSections();
#endif
}
