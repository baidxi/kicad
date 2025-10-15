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
#include <api/api_design_resource.h>
#include "api_sch_placement_command.h"
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
#include <api/api_design_resource.h>
#include <wx/log.h>
#include <fmt/format.h>


using namespace kiapi::common::commands;
using kiapi::common::types::CommandStatus;
using kiapi::common::types::DocumentType;
using kiapi::common::types::ItemRequestStatus;


namespace
{
    SYMBOL_RESOURCE ConvertProtoToSymbolResource( kiapi::common::types::SymbolResource const& aProto )
    {
        SYMBOL_RESOURCE resource;
        resource.uri = aProto.uri();
        resource.symbolname = aProto.symbol_name();

        return resource;
    }
}

HANDLER_RESULT<Empty> API_HANDLER_SCH::handlePlaceSymbol( const HANDLER_CONTEXT<PlaceSymbol>& aCtx )
{
    if( !aCtx.Request.has_symbol_resource() || aCtx.Request.symbol_resource().uri().empty() )
    {
        return {};
    }
    
    auto symbol_resource = ConvertProtoToSymbolResource( aCtx.Request.symbol_resource() );
    PLACE_SYMBOL_RESOURCE_COMMAND   cmd;
    cmd.symbol_resource = symbol_resource;
    if(aCtx.Request.has_place_all_units())
    {
        if(!cmd.option.has_value())
        {
            cmd.option.emplace();
        }
        cmd.option->place_all_units = aCtx.Request.place_all_units();
    }
    if(aCtx.Request.has_place_repeated_copies())
    {
        if(!cmd.option.has_value())
        {
            cmd.option.emplace();
        }
        cmd.option->place_repeated_copies = aCtx.Request.place_repeated_copies();
    }
    
    placeSymbol(cmd);

    return {};
}

void API_HANDLER_SCH::placeSymbol( PLACE_SYMBOL_RESOURCE_COMMAND const& aCommand )
{
    placeSymbol( API_DESIGN_RESOURCE_MGR::GetSymbolContent(aCommand.symbol_resource), aCommand.option );
}

void API_HANDLER_SCH::placeSymbol( SYMBOL_CONTENT const&   aComponent, std::optional<PLACE_SYMBOL_OPTION> const& aOption )
{
    // Trigger loading the lazy ininitialized project symbol library table.
    void( PROJECT_SCH::SchSymbolLibTable( &m_frame->Prj() ) );

    auto libSymbol = m_resourceMgr->SaveSymbolToProject(aComponent);

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
              true } );
}
