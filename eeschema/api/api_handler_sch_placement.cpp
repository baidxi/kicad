/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2025 Liu Weijian <lweijian210@gmail.com>
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

#include <api/api_design_content.h>
#include <optional>
#include <sch_actions.h>
#include <tool/tool_manager.h>
#include <project_sch.h>
#include <api/api_handler_sch.h>
#include <api/api_sch_utils.h>
#include <api/api_utils.h>
#include <magic_enum.hpp>
#include <sch_commit.h>
#include <sch_edit_frame.h>
#include <wx/filename.h>

#include <api/common/types/base_types.pb.h>
#include <wx/log.h>
#include <fmt/format.h>

using namespace kiapi::common::commands;


HANDLER_RESULT<Empty> API_HANDLER_SCH::interactivePlaceSymbol(const HANDLER_CONTEXT<InteractivePlaceSymbol>& aCtx)
{
    std::string symbol_content;
    std::string symbol_name;
    bool place_all_units = true;
    if(aCtx.Request.has_place_all_units())
    {
        place_all_units = aCtx.Request.place_all_units();
    }

    switch (aCtx.Request.symbol_case())
    {
        case InteractivePlaceSymbol::SymbolCase::kSymbolResource:
        {
            std::string symbol_uri = aCtx.Request.symbol_resource().uri();
            
            if(aCtx.Request.symbol_resource().has_symbol_name())
            {
                symbol_name = aCtx.Request.symbol_resource().symbol_name();
            }
            InteractivelyPlaceSymbolURL(symbol_name, symbol_uri, place_all_units);
            break;
        }
        case InteractivePlaceSymbol::SymbolCase::kSymbolContent:
        {
            symbol_content = aCtx.Request.symbol_content().content();
            if(aCtx.Request.symbol_content().has_symbol_name())
            {
                symbol_name    = aCtx.Request.symbol_content().symbol_name();
            }
            InteractivelyPlaceSymbolContent(symbol_name , symbol_content, place_all_units);
            break;
        }
        default:
        {
            break;
        }
            
    }

    return {};
}



void API_HANDLER_SCH::InteractivelyPlaceSymbolURL(const std::string& aName, const std::string& aUrl, bool place_all_units)
{
    std::string symbol_content = API_DESIGN_RESOURCE_MGR::GetSymbolContent(aUrl);
    wxFileName fn(aUrl);
    std::string symbol_name = !aName.empty() ? aName : API_DESIGN_RESOURCE_MGR::FormatToValidFileName( fn.GetFullName().ToStdString() ) ;
    InteractivelyPlaceSymbolContent(symbol_name, symbol_content, place_all_units);

}


void API_HANDLER_SCH::InteractivelyPlaceSymbolContent(const std::string& aName, const std::string& aContent, bool place_all_units)
{
    void( PROJECT_SCH::SchSymbolLibTable( &m_frame->Prj() ) );

    auto libSymbol = m_resourceMgr->SaveSymbolToProject(aName, aContent);

    if( !libSymbol )
    {
        wxLogError( "Failed to save component to project" );
        return;
    }

    PICKED_SYMBOL pickedSymbol;
    pickedSymbol.LibId = libSymbol->GetLIB_ID();
    m_frame->GetToolManager()->RunAction<SCH_ACTIONS::PLACE_SYMBOL_PARAMS>(
            SCH_ACTIONS::placeSymbol,
            { new SCH_SYMBOL( *libSymbol, &m_frame->GetCurrentSheet(), pickedSymbol, {}, &m_frame->Schematic() ), true,
              place_all_units } );

}