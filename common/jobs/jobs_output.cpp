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
#include <common.h>
#include <wx/filename.h>
#include <wx_filename.h>


wxString JOBS_OUTPUT_HANDLER::ResolveOutputPath( const wxString& aPath,
                                                 const wxString& aJobsetFilePath,
                                                 PROJECT*        aProject )
{
    if( aPath.IsEmpty() )
        return aPath;

    wxString resolved = aPath;

    if( aProject )
    {
        resolved = ExpandTextVars( resolved, aProject );
        resolved = ExpandEnvVarSubstitutions( resolved, aProject );
    }

    if( resolved.StartsWith( "~" ) )
        resolved.Replace( "~", wxGetHomeDir(), false );

    wxFileName fn( resolved );

    if( !fn.IsAbsolute() && !aJobsetFilePath.IsEmpty() )
    {
        wxString jobsetDir = wxFileName( aJobsetFilePath ).GetPath();

        if( !jobsetDir.IsEmpty() )
            fn.MakeAbsolute( jobsetDir );
    }

    fn.Normalize( FN_NORMALIZE_FLAGS );
    return fn.GetFullPath();
}
