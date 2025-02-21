/*
* This program source code file is part of KiCad, a free EDA CAD application.
*
* Copyright (C) 2020 Mark Roszko <mark.roszko@gmail.com>
* Copyright (C) 2020-2024 KiCad Developers, see AUTHORS.txt for contributors.
*
* This program is free software: you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation, either version 3 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <kiplatform/app.h>

#include <wx/string.h>
#include <wx/utils.h>

#ifdef _WIN32
#include <windows.h>
#include <strsafe.h>
#include <config.h>
#include <VersionHelpers.h>
#include <iostream>
#include <cstdio>
#endif


bool KIPLATFORM::APP::Init()
{
    AttachConsole( true );
    return true;
}


bool KIPLATFORM::APP::AttachConsole( bool aTryAlloc )
{
#ifdef _WIN32
    if( ::AttachConsole( ATTACH_PARENT_PROCESS ) || ( aTryAlloc && ::AllocConsole() ) )
    {
        #if !defined( __MINGW32__ ) // These redirections create problems on mingw:
                                    // Nothing is printed to the console

        if( ::GetStdHandle( STD_INPUT_HANDLE ) != INVALID_HANDLE_VALUE )
        {
            freopen( "CONIN$", "r", stdin );
            setvbuf( stdin, NULL, _IONBF, 0 );
        }

        if( ::GetStdHandle( STD_OUTPUT_HANDLE ) != INVALID_HANDLE_VALUE )
        {
            freopen( "CONOUT$", "w", stdout );
            setvbuf( stdout, NULL, _IONBF, 0 );
        }

        if( ::GetStdHandle( STD_ERROR_HANDLE ) != INVALID_HANDLE_VALUE )
        {
            freopen( "CONOUT$", "w", stderr );
            setvbuf( stderr, NULL, _IONBF, 0 );
        }
        #endif

        std::ios::sync_with_stdio( true );

        std::wcout.clear();
        std::cout.clear();
        std::wcerr.clear();
        std::cerr.clear();
        std::wcin.clear();
        std::cin.clear();

        return true;
    }
#endif

    return false;
}


bool KIPLATFORM::APP::IsOperatingSystemUnsupported()
{
    // Not implemented on this platform
    return false;
}


bool KIPLATFORM::APP::RegisterApplicationRestart( const wxString& aCommandLine )
{
    // Not implemented on this platform
    return true;
}


bool KIPLATFORM::APP::UnregisterApplicationRestart()
{
    // Not implemented on this platform
    return true;
}


bool KIPLATFORM::APP::SupportsShutdownBlockReason()
{
    return false;
}


void KIPLATFORM::APP::RemoveShutdownBlockReason( wxWindow* aWindow )
{
}


void KIPLATFORM::APP::SetShutdownBlockReason( wxWindow* aWindow, const wxString& aReason )
{
}


void KIPLATFORM::APP::ForceTimerMessagesToBeCreatedIfNecessary()
{
}


void KIPLATFORM::APP::AddDynamicLibrarySearchPath( const wxString& aPath )
{
}
