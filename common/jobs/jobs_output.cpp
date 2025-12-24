/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2025 Dmitry Rezvanov <dmitry.rezvanov@yandex.ru>
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
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

#include <jobs/jobs_output.h>
#include <wx/filename.h>
#include <wx_filename.h>

wxString JOBS_OUTPUT_HANDLER::ResolveOutputPath( const wxString& aPath, const wxString& aJobsetFilePath )
{
    if( aPath.IsEmpty() )
        return aPath;

    wxFileName pathFn( aPath );

    // If the path is already absolute, return it as-is
    if( pathFn.IsAbsolute() )
        return aPath;

    // Resolve relative path relative to jobset file directory
    wxFileName jobsetFn( aJobsetFilePath );
    wxString jobsetDir = jobsetFn.GetPath();

    if( jobsetDir.IsEmpty() )
        return aPath; // Can't resolve if we don't have a jobset directory

    // Construct the full path - try as file first, then as directory if needed
    wxString fullPath = jobsetDir + wxFileName::GetPathSeparator() + aPath;
    wxFileName resolvedFn( fullPath );

    // If the original path ends with a separator, treat it as a directory
    if( aPath.EndsWith( wxFileName::GetPathSeparator() ) )
        resolvedFn.AssignDir( fullPath );
    else
        resolvedFn.Assign( fullPath );

    resolvedFn.Normalize( FN_NORMALIZE_FLAGS );
    return resolvedFn.GetFullPath();
}
