/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2025 KiCad Developers, see AUTHORS.txt for contributors.
 * @author Fabien Corona <f.corona<at>laposte.net>
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

#include "sync.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>


KIDX_REGISTRY::KIDX_REGISTRY()
{
}


bool KIDX_REGISTRY::IsFileKnown( const std::shared_ptr<IDX_FILE_REGISTRY> aFile ) const
{
    if( !aFile )
        return false;

    for( const auto& file : m_files )
    {
        if( *file == *aFile )
            return true;
    }
    return false;
}


bool KIDX_REGISTRY::AddFile( const std::shared_ptr<IDX_FILE_REGISTRY> aFile )
{
    if( !aFile )
        return false;

    m_files.push_back( aFile );
    return true;
}


void KIDX_REGISTRY::ComputeNext()
{
    for( std::shared_ptr<IDX_FILE_REGISTRY> file : GetFiles() )
    {
        file->m_next = file->IsBaseline();
    }
}
