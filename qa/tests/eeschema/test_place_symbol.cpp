/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2025 liu weijian <lweijian210@gmail.com>
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


#include <boost/test/unit_test.hpp>
#include <qa_utils/wx_utils/wx_assert.h>
#include <qa_utils/wx_utils/unit_test_utils.h>
#include <schematic_utils/schematic_file_util.h>

#include <google/protobuf/any.pb.h>
#include <settings/settings_manager.h>
#include <api/common/types/base_types.pb.h>

#include <api/api_design_resource.h>
#include <api/api_sch_placement_command.h>
#include <api/common/types/base_types.pb.h>
#include <api/common/types/design_resources.pb.h>
#include <api/schematic/schematic_commands.pb.h>

#include <schematic.h>
#include <algorithm>


using kiapi::common::types::CommandStatus;
using kiapi::common::types::DocumentType;
using kiapi::common::types::ItemRequestStatus;

void SerializePlaceSymbolResourceCommand(const PLACE_SYMBOL_RESOURCE_COMMAND& placeCommand, google::protobuf::Any& aContainer)
{
    kiapi::schematic::commands::PlaceSymbol  placeSymbol;
    if(auto opt = placeCommand.option; opt && opt->place_repeated_copies)
    {
        placeSymbol.set_place_repeated_copies(*opt->place_repeated_copies);
       
    }
    if(auto opt = placeCommand.option; opt && opt->place_all_units)
    {
        placeSymbol.set_place_all_units(*opt->place_all_units);
    }
    placeSymbol.mutable_symbol_resource()->set_uri(placeCommand.symbol_resource.uri);
    placeSymbol.mutable_symbol_resource()->set_symbol_name(placeCommand.symbol_resource.symbolname);
    aContainer.PackFrom(placeSymbol);
}

bool DeserializePlaceSymbolResourceCommand(const google::protobuf::Any& aContainer, PLACE_SYMBOL_RESOURCE_COMMAND& placeCommand)
{
    kiapi::schematic::commands::PlaceSymbol  placeSymbol;
    if( !aContainer.UnpackTo(&placeSymbol))
    {
        return false;
    }
    if(placeSymbol.has_place_all_units())
    {
        if(!placeCommand.option.has_value())
        {
            placeCommand.option.emplace();
        }
        placeCommand.option->place_all_units = placeSymbol.place_all_units();
    }
    if(placeSymbol.has_place_repeated_copies())
    {
        if(!placeCommand.option.has_value())
        {
            placeCommand.option.emplace();
        }
        placeCommand.option->place_repeated_copies = placeSymbol.place_repeated_copies();
    }

    placeCommand.symbol_resource.uri = placeSymbol.mutable_symbol_resource()->uri();
    placeCommand.symbol_resource.symbolname = placeSymbol.mutable_symbol_resource()->symbol_name();
    return true;
}

bool ComparePlaceSymbolResourceCommand(const PLACE_SYMBOL_RESOURCE_COMMAND& lhs, const PLACE_SYMBOL_RESOURCE_COMMAND& rhs)
{
    nlohmann::json  jsLhs = lhs;
    nlohmann::json  jsRhs = rhs;

    return jsLhs == jsRhs;
}

BOOST_AUTO_TEST_SUITE( PlaceSymbolProto )


BOOST_AUTO_TEST_CASE(PLACE_SYMBOL_TEST)
{
    PLACE_SYMBOL_RESOURCE_COMMAND   placeCommand;
    placeCommand.option.emplace();
    placeCommand.symbol_resource.uri = "https://www.eda.cn/all/2/9048617628902766381.kicad_sym";
    placeCommand.symbol_resource.symbolname = "D-AS321KTR-G1.kicad_sym";
    placeCommand.option->place_all_units = false;
    placeCommand.option->place_repeated_copies = true;

    google::protobuf::Any any;
    BOOST_REQUIRE_NO_THROW( SerializePlaceSymbolResourceCommand(placeCommand, any) );

    PLACE_SYMBOL_RESOURCE_COMMAND    convertCommand{};
    BOOST_CHECK(DeserializePlaceSymbolResourceCommand(any, convertCommand));

    google::protobuf::Any  convertAny;
    BOOST_REQUIRE_NO_THROW( SerializePlaceSymbolResourceCommand(convertCommand, convertAny) );

    
    if(convertAny.SerializeAsString() != any.SerializeAsString())
    {
        BOOST_TEST_FAIL( "Round-tripped protobuf does not match" );
    }

    if(!ComparePlaceSymbolResourceCommand(placeCommand, convertCommand))
    {
        BOOST_TEST_FAIL( "Round-tripped object does not match" );
    }

}


BOOST_AUTO_TEST_SUITE_END()


