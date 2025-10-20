/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2025 Ethan Chien <liangtie.qian@gmail.com>
 * Copyright (C) 2025 KiCad Developers, see AUTHORS.txt for contributors.
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

#include "api_sch_design_resource_mgr.h"
#include "api/api_design_resource_mgr.h"
#include "sch_base_frame.h"
#include <functional>
#include <lib_symbol.h>
#include <sch_field.h>
#include <api/api_design_content.h>
#include <string>
#include <symbol_lib_table.h>
#include <eda_base_frame.h>
#include <project_sch.h>
#include <wx/log.h>


API_SCH_DESIGN_RESOURCE_MGR::API_SCH_DESIGN_RESOURCE_MGR( PROJECT& aProject ) : API_DESIGN_RESOURCE_MGR( aProject )
{
}

API_SCH_DESIGN_RESOURCE_MGR::~API_SCH_DESIGN_RESOURCE_MGR()
{
}


LIB_SYMBOL* API_SCH_DESIGN_RESOURCE_MGR::SaveSymbolToProject( const std::string& aName, const std::string& aContent)
{
    const auto& symbol_file_name = aName;
    if( !WriteProjectResource( SYMBOL_DIR, symbol_file_name, aContent ) )
    {
        wxLogError( "Failed to save symbol %s to project", symbol_file_name );
        return {};
    }
    const auto symbol_nick_name = RemoveFileExt( symbol_file_name );

    const auto save_path = GetProjectResourceSavePath( SYMBOL_DIR, symbol_file_name );
    if(!save_path)
    {
        wxLogError( "Failed to get symbol %s dir", symbol_file_name );
        return {};
    }

    LIB_SYMBOL* lib_symbol = nullptr;;
    try
    {
        IO_RELEASER<SCH_IO> pi( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_KICAD ) );
        LIB_SYMBOL* asymbol = pi->LoadSymbol(*save_path, symbol_nick_name );
        if(asymbol)
        {
            LIB_ID  lib_id = asymbol->GetLibId();
            lib_id.SetLibNickname(symbol_nick_name);
            asymbol->SetLibId(lib_id);
            lib_symbol = new LIB_SYMBOL(*asymbol);
        }
    }
    catch( const IO_ERROR& ioe )
    {
        wxLogError( _( "Failed to save library %s. %s" ), *save_path, ioe.What() );
        return {};
    }
    if( !lib_symbol )
    {
        wxLogError( "Failed to load symbol for %s", symbol_nick_name );
        return {};
    }
    return lib_symbol;
}

