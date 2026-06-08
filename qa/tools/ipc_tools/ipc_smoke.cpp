#include <iostream>
#include <optional>
#include <fstream>
#include <string>
#include <set>
#include <cstdint>
#include <cstdlib>
#include <cerrno>

#include <nng/nng.h>
#include <nng/protocol/reqrep0/req.h>

#include <import_export.h>

#include <api/common/envelope.pb.h>
#include <api/common/commands/base_commands.pb.h>
#include <api/common/commands/mcp_commands.pb.h>
#include <api/common/commands/project_commands.pb.h>
#include <api/common/commands/workspace_commands.pb.h>

#include <google/protobuf/util/json_util.h>

using kiapi::common::ApiRequest;
using kiapi::common::ApiResponse;
using kiapi::common::ApiStatusCode;

static void die( const std::string& what, int rv )
{
    std::cerr << what << ": " << nng_strerror( rv ) << "\n";
    std::exit( 2 );
}

static ApiResponse roundTrip( const std::string& url, const ApiRequest& req )
{
    std::string wire = req.SerializeAsString();

    nng_socket sock;
    int        rv = nng_req0_open( &sock );
    if( rv != 0 )
        die( "nng_req0_open", rv );

    rv = nng_dial( sock, url.c_str(), nullptr, 0 );
    if( rv != 0 )
        die( "nng_dial", rv );

    rv = nng_send( sock, (void*) wire.data(), wire.size(), 0 );
    if( rv != 0 )
        die( "nng_send", rv );

    void*  buf = nullptr;
    size_t sz  = 0;
    rv = nng_recv( sock, &buf, &sz, NNG_FLAG_ALLOC );
    if( rv != 0 )
        die( "nng_recv", rv );

    ApiResponse resp;
    if( !resp.ParseFromArray( buf, static_cast<int>( sz ) ) )
    {
        nng_free( buf, sz );
        std::cerr << "Failed to parse ApiResponse (" << sz << " bytes)\n";
        std::exit( 3 );
    }

    nng_free( buf, sz );
    nng_close( sock );
    return resp;
}

template <typename MsgT>
static ApiRequest makeRequest( const std::string& clientName, const std::string& token, const MsgT& msg )
{
    ApiRequest req;
    req.mutable_header()->set_client_name( clientName );
    req.mutable_header()->set_kicad_token( token );
    req.mutable_message()->PackFrom( msg );
    return req;
}

static void printResponse( const std::string& label, const ApiResponse& resp )
{
    std::cout << "=== " << label << " ===\n";
    std::cout << "status=" << static_cast<int>( resp.status().status() ) << "\n";
    if( !resp.status().error_message().empty() )
        std::cout << "error_message=" << resp.status().error_message() << "\n";
    std::cout << "kicad_token=" << resp.header().kicad_token() << "\n";
    std::cout << resp.Utf8DebugString() << "\n";
}

template <typename MsgT>
static bool unpack( const ApiResponse& resp, MsgT* out )
{
    if( resp.status().status() != ApiStatusCode::AS_OK )
        return false;
    return resp.message().UnpackTo( out );
}

static std::optional<bool> structBool( const google::protobuf::Struct& s, const std::string& key )
{
    const auto& f = s.fields();
    auto it = f.find( key );
    if( it == f.end() )
        return std::nullopt;
    if( it->second.kind_case() != google::protobuf::Value::kBoolValue )
        return std::nullopt;
    return it->second.bool_value();
}

static std::optional<std::string> structString( const google::protobuf::Struct& s, const std::string& key )
{
    const auto& f = s.fields();
    auto it = f.find( key );
    if( it == f.end() )
        return std::nullopt;
    if( it->second.kind_case() != google::protobuf::Value::kStringValue )
        return std::nullopt;
    return it->second.string_value();
}

static std::optional<const google::protobuf::Struct*> structStruct( const google::protobuf::Struct& s,
                                                                    const std::string& key )
{
    const auto& f = s.fields();
    auto it = f.find( key );
    if( it == f.end() )
        return std::nullopt;
    if( it->second.kind_case() != google::protobuf::Value::kStructValue )
        return std::nullopt;
    return &it->second.struct_value();
}

static std::optional<int> structListSize( const google::protobuf::Struct& s, const std::string& key )
{
    const auto& f = s.fields();
    auto it = f.find( key );
    if( it == f.end() )
        return std::nullopt;
    if( it->second.kind_case() != google::protobuf::Value::kListValue )
        return std::nullopt;
    return it->second.list_value().values_size();
}

static std::optional<std::int64_t> structInt64( const google::protobuf::Struct& s, const std::string& key )
{
    const auto& f = s.fields();
    auto it = f.find( key );
    if( it == f.end() )
        return std::nullopt;
    if( it->second.kind_case() == google::protobuf::Value::kNumberValue )
    {
        const double d = it->second.number_value();
        if( d != std::floor( d ) )
            return std::nullopt;
        return static_cast<std::int64_t>( d );
    }
    // int64 coordinates are often emitted as strings in protobuf JSON → Struct.
    if( it->second.kind_case() == google::protobuf::Value::kStringValue )
    {
        const std::string& str = it->second.string_value();
        if( str.empty() )
            return std::nullopt;
        char* endPtr = nullptr;
        errno           = 0;
        const long long v = std::strtoll( str.c_str(), &endPtr, 10 );
        if( errno != 0 || endPtr == str.c_str() || *endPtr != '\0' )
            return std::nullopt;
        return static_cast<std::int64_t>( v );
    }
    return std::nullopt;
}

int main( int argc, char** argv )
{
    if( argc < 3 )
    {
        std::cerr << "Usage: qa_ipc_smoke <ipc_url> <command>\n"
                     "Commands: ping | get_version | get_project_info | workspace_overview | mcp_list_tools | mcp_call_tool\n"
                     "  mcp_call_tool requires: <tool_name>\n";
        return 1;
    }

    const std::string url = argv[1];
    const std::string cmd = argv[2];

    const std::string clientName = "qa-ipc-smoke";
    const std::string token      = ""; // empty token is accepted; server returns its token

    if( cmd == "ping" )
    {
        kiapi::common::commands::Ping ping;
        ApiResponse                   resp = roundTrip( url, makeRequest( clientName, token, ping ) );
        printResponse( "Ping", resp );
        return 0;
    }
    else if( cmd == "get_version" )
    {
        kiapi::common::commands::GetVersion gv;
        ApiResponse                         resp = roundTrip( url, makeRequest( clientName, token, gv ) );
        printResponse( "GetVersion", resp );

        kiapi::common::commands::GetVersionResponse out;
        if( unpack( resp, &out ) )
            std::cout << out.Utf8DebugString() << "\n";

        return 0;
    }
    else if( cmd == "get_project_info" )
    {
        kiapi::common::commands::GetProjectInfo gpi;
        ApiResponse                            resp = roundTrip( url, makeRequest( clientName, token, gpi ) );
        printResponse( "GetProjectInfo", resp );

        kiapi::common::commands::GetProjectInfoResponse out;
        if( unpack( resp, &out ) )
        {
            std::cout << "loaded=" << ( out.loaded() ? "true" : "false" ) << "\n";
            std::cout << "project_name=" << out.project_name() << "\n";
            std::cout << "project_directory=" << out.project_directory() << "\n";
            std::cout << "project_file_path=" << out.project_file_path() << "\n";
        }

        return 0;
    }
    else if( cmd == "workspace_overview" )
    {
        kiapi::common::commands::GetWorkspaceOverview g;
        ApiResponse                                   resp = roundTrip( url, makeRequest( clientName, token, g ) );
        printResponse( "GetWorkspaceOverview", resp );

        kiapi::common::commands::GetWorkspaceOverviewResponse out;
        if( unpack( resp, &out ) )
        {
            std::cout << "project_loaded=" << ( out.project().loaded() ? "true" : "false" ) << "\n";
            std::cout << "open_schematic_documents=" << out.open_schematic_documents_size() << "\n";
            std::cout << "open_pcb_documents=" << out.open_pcb_documents_size() << "\n";
        }

        return 0;
    }
    else if( cmd == "mcp_list_tools" )
    {
        kiapi::common::commands::ListTools req;
        ApiResponse                        resp = roundTrip( url, makeRequest( clientName, token, req ) );
        // Do not print full resp.Utf8DebugString(): ListTools payloads include large input_schema_json
        // per tool and can stall qa_ipc_smoke for minutes on CI/automation timeouts.
        std::cout << "=== MCP ListTools ===\n";
        std::cout << "status=" << static_cast<int>( resp.status().status() ) << "\n";
        if( !resp.status().error_message().empty() )
            std::cout << "error_message=" << resp.status().error_message() << "\n";
        std::cout << "kicad_token=" << resp.header().kicad_token() << "\n";

        kiapi::common::commands::ListToolsResponse out;
        if( unpack( resp, &out ) )
        {
            std::cout << "tools=" << out.tools_size() << "\n";
            for( const auto& t : out.tools() )
                std::cout << "- " << t.name() << "\n";
        }

        return 0;
    }
    else if( cmd == "mcp_call_tool" )
    {
        if( argc < 4 )
        {
            std::cerr << "Usage: qa_ipc_smoke <ipc_url> mcp_call_tool <tool_name>\n";
            return 1;
        }

        kiapi::common::commands::CallTool req;
        req.set_name( argv[3] );
        // V0 tools accept empty args

        ApiResponse resp = roundTrip( url, makeRequest( clientName, token, req ) );
        printResponse( "MCP CallTool", resp );

        kiapi::common::commands::CallToolResponse out;
        if( unpack( resp, &out ) )
        {
            // Emit stable, greppable summaries for V0 tools.
            if( req.name() == "get_project_info" )
            {
                auto loaded = structBool( out.result(), "loaded" ).value_or( false );
                std::cout << "loaded=" << ( loaded ? "true" : "false" ) << "\n";
                std::cout << "project_name=" << structString( out.result(), "project_name" ).value_or( "" ) << "\n";
                std::cout << "project_directory="
                          << structString( out.result(), "project_directory" ).value_or( "" ) << "\n";
                std::cout << "project_file_path="
                          << structString( out.result(), "project_file_path" ).value_or( "" ) << "\n";
            }
            else if( req.name() == "get_workspace_overview" )
            {
                const google::protobuf::Struct* proj =
                        structStruct( out.result(), "project" ).value_or( nullptr );
                const bool loaded = proj ? structBool( *proj, "loaded" ).value_or( false ) : false;
                std::cout << "project_loaded=" << ( loaded ? "true" : "false" ) << "\n";
                std::cout << "open_schematic_documents="
                          << structListSize( out.result(), "open_schematic_documents" ).value_or( 0 ) << "\n";
                std::cout << "open_pcb_documents=" << structListSize( out.result(), "open_pcb_documents" ).value_or( 0 )
                          << "\n";
            }
            else
            {
                std::cout << out.result().Utf8DebugString() << "\n";
            }
        }

        return 0;
    }
    else if( cmd == "mcp_call_tool_args" )
    {
        if( argc < 5 )
        {
            std::cerr << "Usage: qa_ipc_smoke <ipc_url> mcp_call_tool_args <tool_name> <args_json>\n";
            return 1;
        }

        kiapi::common::commands::CallTool req;
        req.set_name( argv[3] );

        google::protobuf::Struct args;
        std::string argsJson = argv[4];
        if( !argsJson.empty() && argsJson.front() == '@' )
        {
            const std::string path = argsJson.substr( 1 );
            std::ifstream     f( path, std::ios::binary );
            if( !f )
            {
                std::cerr << "Failed to open args_json file: " << path << "\n";
                return 1;
            }
            argsJson.assign( std::istreambuf_iterator<char>( f ), std::istreambuf_iterator<char>() );

            // PowerShell may write UTF-8 with BOM; strip it to keep JSON parsing strict.
            if( !argsJson.empty() && argsJson.front() == '\xEF' && argsJson.size() >= 3
                && static_cast<unsigned char>( argsJson[1] ) == 0xBB
                && static_cast<unsigned char>( argsJson[2] ) == 0xBF )
            {
                argsJson.erase( 0, 3 );
            }
            else if( !argsJson.empty() && argsJson.front() == '\uFEFF' )
            {
                argsJson.erase( 0, 1 );
            }
        }

        if( !google::protobuf::util::JsonStringToMessage( argsJson, &args ).ok() )
        {
            std::cerr << "Failed to parse args_json as protobuf Struct: " << argsJson << "\n";
            return 1;
        }

        req.mutable_arguments()->CopyFrom( args );

        ApiResponse resp = roundTrip( url, makeRequest( clientName, token, req ) );

        // Avoid printResponse's full ApiResponse.Utf8DebugString() for tools returning very large
        // Struct payloads (see pcb_get_board_items_session branch below).
        // Also omit full ApiResponse dump for pcb_measure_distance_session so harness stdout stays bounded.
        // get_net_classes / get_board_stackup: CallToolResponse embeds electrical/stackup protos; full
        // Utf8DebugString can stall qa_ipc_smoke on Windows (stdout-heavy) long enough to trip harness timeouts.
        if( req.name() == "pcb_get_board_items_session" || req.name() == "pcb_get_net_items_session"
            || req.name() == "pcb_measure_distance_session"             || req.name() == "get_pcb_item_geometry_session"
            || req.name() == "get_pcb_clearance_context_session"
            || req.name() == "get_pcb_nearby_items_context_session"
            || req.name() == "get_pcb_drc_violations_context_session" || req.name() == "get_pcb_sync_readiness_session"
            || req.name() == "pcb_update_from_schematic_preview_session"
            || req.name() == "pcb_update_from_schematic_session"
            || req.name() == "save_board_session"
            || req.name() == "get_net_classes"
            || req.name() == "get_board_stackup" || req.name() == "library_footprint_search_session"
            || req.name() == "library_footprint_inspect_session" )
        {
            std::cout << "=== MCP CallTool(maybe args) ===\n";
            std::cout << "status=" << static_cast<int>( resp.status().status() ) << "\n";
            if( !resp.status().error_message().empty() )
                std::cout << "error_message=" << resp.status().error_message() << "\n";
            std::cout << "kicad_token=" << resp.header().kicad_token() << "\n";
        }
        else
        {
            printResponse( "MCP CallTool(maybe args)", resp );
        }

        kiapi::common::commands::CallToolResponse out;
        if( unpack( resp, &out ) )
        {
            if( req.name() == "get_project_info" )
            {
                auto loaded = structBool( out.result(), "loaded" ).value_or( false );
                std::cout << "loaded=" << ( loaded ? "true" : "false" ) << "\n";
                std::cout << "project_name=" << structString( out.result(), "project_name" ).value_or( "" )
                          << "\n";
                std::cout << "project_directory="
                          << structString( out.result(), "project_directory" ).value_or( "" ) << "\n";
                std::cout << "project_file_path="
                          << structString( out.result(), "project_file_path" ).value_or( "" ) << "\n";
            }
            else if( req.name() == "get_workspace_overview" )
            {
                const google::protobuf::Struct* proj =
                        structStruct( out.result(), "project" ).value_or( nullptr );
                const bool loaded = proj ? structBool( *proj, "loaded" ).value_or( false ) : false;
                std::cout << "project_loaded=" << ( loaded ? "true" : "false" ) << "\n";
                std::cout << "open_schematic_documents="
                          << structListSize( out.result(), "open_schematic_documents" ).value_or( 0 ) << "\n";
                std::cout << "open_pcb_documents=" << structListSize( out.result(), "open_pcb_documents" ).value_or( 0 )
                          << "\n";
            }
            else if( req.name() == "qa_open_editor_frame" )
            {
                auto opened = structBool( out.result(), "opened" ).value_or( false );
                std::cout << "opened=" << ( opened ? "true" : "false" ) << "\n";
                std::cout << "type=" << structString( out.result(), "type" ).value_or( "" ) << "\n";
                std::cout << "filename=" << structString( out.result(), "filename" ).value_or( "" ) << "\n";
            }
            else if( req.name() == "get_pcb_items" )
            {
                int items_count = 0;
                const auto fields = out.result().fields();
                auto       it     = fields.find( "items" );
                if( it != fields.end() && it->second.kind_case() == google::protobuf::Value::kListValue )
                    items_count = static_cast<int>( it->second.list_value().values_size() );

                std::cout << "items_count=" << items_count << "\n";

                // Best-effort extraction of a real KIID so harness can call *_by_id.
                // The serialized BOARD_ITEMs expose an `id` struct with a `value` string.
                std::string first_item_id;
                if( it != fields.end() && it->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    const auto& vals = it->second.list_value().values();
                    if( vals.size() > 0 )
                    {
                        const auto& firstVal = vals.Get( 0 );
                        if( firstVal.kind_case() == google::protobuf::Value::kStructValue )
                        {
                            const auto& itemStruct = firstVal.struct_value();
                            auto         itemFields = itemStruct.fields();
                        auto         itId = itemFields.find( "id" );
                        if( itId != itemFields.end() && itId->second.kind_case() == google::protobuf::Value::kStructValue )
                        {
                            const auto& idStruct = itId->second.struct_value();
                            auto         idFields = idStruct.fields();
                            auto         itVal = idFields.find( "value" );
                            if( itVal != idFields.end() && itVal->second.kind_case() == google::protobuf::Value::kStringValue )
                                first_item_id = itVal->second.string_value();
                        }
                        }
                    }
                }

                std::cout << "first_item_id=" << first_item_id << "\n";
            }
            else if( req.name() == "get_pcb_items_by_id" )
            {
                int items_count = 0;
                const auto fields = out.result().fields();
                auto       it     = fields.find( "items" );
                if( it != fields.end() && it->second.kind_case() == google::protobuf::Value::kListValue )
                    items_count = static_cast<int>( it->second.list_value().values_size() );

                std::cout << "items_count=" << items_count << "\n";

                // QA: one BoardGraphicShape segment end (Dwgs.User line) when a single item is returned.
                std::string endSummary = "end_x_nm=absent;end_y_nm=absent";
                if( items_count == 1 && it != fields.end()
                    && it->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    const auto& vals = it->second.list_value().values();
                    if( vals.size() == 1 && vals.Get( 0 ).kind_case() == google::protobuf::Value::kStructValue )
                    {
                        const auto& itemSt = vals.Get( 0 ).struct_value();
                        if( auto shapePtr = structStruct( itemSt, "shape" ) )
                        {
                            if( auto segPtr = structStruct( **shapePtr, "segment" ) )
                            {
                                if( auto endPtr = structStruct( **segPtr, "end" ) )
                                {
                                    auto ox = structInt64( **endPtr, "x_nm" );
                                    auto oy = structInt64( **endPtr, "y_nm" );
                                    if( ox && oy )
                                    {
                                        endSummary =
                                                "end_x_nm=" + std::to_string( *ox ) + ";end_y_nm=" + std::to_string( *oy );
                                    }
                                }
                            }
                        }
                    }
                }

                std::cout << "pcb_graphic_segment_end_summary=" << endSummary << "\n";
            }
            else if( req.name() == "get_pad_shape_as_polygon" )
            {
                int count = 0;
                int padsCount = 0;
                int polygonsCount = 0;

                const auto fields = out.result().fields();

                auto itCount = fields.find( "count" );
                if( itCount != fields.end() && itCount->second.kind_case() == google::protobuf::Value::kNumberValue )
                    count = static_cast<int>( itCount->second.number_value() );

                auto itPads = fields.find( "pads" );
                if( itPads != fields.end() && itPads->second.kind_case() == google::protobuf::Value::kListValue )
                    padsCount = static_cast<int>( itPads->second.list_value().values_size() );

                auto itPolygons = fields.find( "polygons" );
                if( itPolygons != fields.end()
                        && itPolygons->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    polygonsCount = static_cast<int>( itPolygons->second.list_value().values_size() );
                }

                std::cout << "count=" << count << "\n";
                std::cout << "pads_count=" << padsCount << "\n";
                std::cout << "polygons_count=" << polygonsCount << "\n";
            }
            else if( req.name() == "hit_test_pcb" )
            {
                const bool hit = structBool( out.result(), "hit" ).value_or( false );

                int result = 0;
                const auto fields = out.result().fields();
                auto       it     = fields.find( "result" );
                if( it != fields.end() && it->second.kind_case() == google::protobuf::Value::kNumberValue )
                    result = static_cast<int>( it->second.number_value() );

                std::cout << "hit=" << ( hit ? "true" : "false" ) << "\n";
                std::cout << "result=" << result << "\n";
            }
            else if( req.name() == "hit_test_sch" )
            {
                const bool hit = structBool( out.result(), "hit" ).value_or( false );

                int result = 0;
                const auto fields = out.result().fields();
                auto       it     = fields.find( "result" );
                if( it != fields.end() && it->second.kind_case() == google::protobuf::Value::kNumberValue )
                    result = static_cast<int>( it->second.number_value() );

                std::cout << "hit=" << ( hit ? "true" : "false" ) << "\n";
                std::cout << "result=" << result << "\n";
            }
            else if( req.name() == "get_selection_pcb" )
            {
                int count = 0;
                const auto fields = out.result().fields();

                // Prefer explicit `count` if present.
                auto itCount = fields.find( "count" );
                if( itCount != fields.end() && itCount->second.kind_case() == google::protobuf::Value::kNumberValue )
                {
                    count = static_cast<int>( itCount->second.number_value() );
                }
                else
                {
                    // Fall back to list size for `items`.
                    auto itItems = fields.find( "items" );
                    if( itItems != fields.end() && itItems->second.kind_case() == google::protobuf::Value::kListValue )
                        count = static_cast<int>( itItems->second.list_value().values_size() );
                }

                std::cout << "count=" << count << "\n";
            }
            else if( req.name() == "get_selection_sch" )
            {
                int count = 0;
                const auto fields = out.result().fields();

                auto itCount = fields.find( "count" );
                if( itCount != fields.end() && itCount->second.kind_case() == google::protobuf::Value::kNumberValue )
                {
                    count = static_cast<int>( itCount->second.number_value() );
                }
                else
                {
                    auto itItems = fields.find( "items" );
                    if( itItems != fields.end() && itItems->second.kind_case() == google::protobuf::Value::kListValue )
                        count = static_cast<int>( itItems->second.list_value().values_size() );
                }

                std::cout << "count=" << count << "\n";
            }
            else if( req.name() == "get_sch_items_by_id" )
            {
                int count = 0;
                std::string first_item_id;
                const auto fields = out.result().fields();

                auto itCount = fields.find( "count" );
                if( itCount != fields.end() && itCount->second.kind_case() == google::protobuf::Value::kNumberValue )
                {
                    count = static_cast<int>( itCount->second.number_value() );
                }
                else
                {
                    auto itItems = fields.find( "items" );
                    if( itItems != fields.end() && itItems->second.kind_case() == google::protobuf::Value::kListValue )
                        count = static_cast<int>( itItems->second.list_value().values_size() );
                }

                std::cout << "count=" << count << "\n";

                // Best-effort extraction of the first item KIID string from `items[0].id.value`.
                auto itItemsById = fields.find( "items" );
                if( itItemsById != fields.end()
                        && itItemsById->second.kind_case()
                               == google::protobuf::Value::kListValue )
                {
                    const auto& vals = itItemsById->second.list_value().values();
                    if( !vals.empty()
                            && vals.Get( 0 ).kind_case()
                                   == google::protobuf::Value::kStructValue )
                    {
                        const auto& itemStruct = vals.Get( 0 ).struct_value();
                        auto         itemFields = itemStruct.fields();
                        auto         itId = itemFields.find( "id" );
                        if( itId != itemFields.end()
                                && itId->second.kind_case()
                                       == google::protobuf::Value::kStructValue )
                        {
                            const auto& idStruct = itId->second.struct_value();
                            auto         idFields = idStruct.fields();
                            auto         itVal = idFields.find( "value" );
                            if( itVal != idFields.end()
                                    && itVal->second.kind_case()
                                           == google::protobuf::Value::kStringValue )
                                first_item_id = itVal->second.string_value();
                        }
                    }
                }

                std::cout << "first_item_id=" << first_item_id << "\n";

                // QA: schematic Line wire end (nm) when a single item is returned.
                std::string schWireEndX = "absent";
                std::string schWireEndY = "absent";
                auto        itItemsWire = fields.find( "items" );
                if( count == 1 && itItemsWire != fields.end()
                    && itItemsWire->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    const auto& valsW = itItemsWire->second.list_value().values();
                    if( valsW.size() == 1 && valsW.Get( 0 ).kind_case() == google::protobuf::Value::kStructValue )
                    {
                        const auto& itemSt = valsW.Get( 0 ).struct_value();
                        if( auto endPtr = structStruct( itemSt, "end" ) )
                        {
                            if( auto ex = structInt64( **endPtr, "x_nm" ) )
                                schWireEndX = std::to_string( *ex );
                            if( auto ey = structInt64( **endPtr, "y_nm" ) )
                                schWireEndY = std::to_string( *ey );
                        }
                    }
                }
                std::cout << "sch_wire_end_x_nm=" << schWireEndX << "\n";
                std::cout << "sch_wire_end_y_nm=" << schWireEndY << "\n";
            }
            else if( req.name() == "get_sch_items" )
            {
                int count = 0;
                std::string first_item_id;
                const auto fields = out.result().fields();

                auto itCount = fields.find( "count" );
                if( itCount != fields.end() && itCount->second.kind_case() == google::protobuf::Value::kNumberValue )
                {
                    count = static_cast<int>( itCount->second.number_value() );
                }
                else
                {
                    auto itItems = fields.find( "items" );
                    if( itItems != fields.end() && itItems->second.kind_case() == google::protobuf::Value::kListValue )
                        count = static_cast<int>( itItems->second.list_value().values_size() );
                }

                // Best-effort extraction of the first item KIID string from `items[0].id.value`.
                auto itItems = fields.find( "items" );
                if( itItems != fields.end() && itItems->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    const auto& vals = itItems->second.list_value().values();
                    if( !vals.empty() && vals.Get( 0 ).kind_case() == google::protobuf::Value::kStructValue )
                    {
                        const auto& itemStruct = vals.Get( 0 ).struct_value();
                        auto         itemFields = itemStruct.fields();
                        auto         itId = itemFields.find( "id" );
                        if( itId != itemFields.end() && itId->second.kind_case() == google::protobuf::Value::kStructValue )
                        {
                            const auto& idStruct = itId->second.struct_value();
                            auto         idFields = idStruct.fields();
                            auto         itVal = idFields.find( "value" );
                            if( itVal != idFields.end() && itVal->second.kind_case() == google::protobuf::Value::kStringValue )
                                first_item_id = itVal->second.string_value();
                        }
                    }
                }

                std::cout << "count=" << count << "\n";

                std::cout << "first_item_id=" << first_item_id << "\n";

                if( itItems != fields.end() && itItems->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    const auto& vals = itItems->second.list_value().values();
                    for( int i = 0; i < vals.size(); ++i )
                    {
                        if( vals.Get( i ).kind_case() != google::protobuf::Value::kStructValue )
                            continue;

                        const auto& itemStruct = vals.Get( i ).struct_value();
                        auto        itemFields = itemStruct.fields();
                        auto        itId = itemFields.find( "id" );
                        if( itId == itemFields.end()
                            || itId->second.kind_case() != google::protobuf::Value::kStructValue )
                        {
                            continue;
                        }

                        const auto& idStruct = itId->second.struct_value();
                        auto        idFields = idStruct.fields();
                        auto        itVal = idFields.find( "value" );
                        if( itVal != idFields.end()
                            && itVal->second.kind_case() == google::protobuf::Value::kStringValue )
                        {
                            std::cout << "sch_item_id=" << itVal->second.string_value() << "\n";
                        }
                    }
                }
            }
            else if( req.name() == "get_symbol_pins_by_id" )
            {
                int count = 0;
                std::string first_pin_id;
                std::string first_pin_type_name;
                std::string first_pin_orientation_name;
                std::string first_pin_net_name;
                std::string first_pin_net_name_source;
                std::string first_pin_net_name_default;
                int         nets_count = 0;
                std::string first_net_net_name;
                std::string first_net_net_name_source;
                std::string first_net_net_name_default;
                int         first_net_pins_count = 0;
                std::string first_net_first_pin_id;
                int first_pin_x_iu = 0;
                int first_pin_y_iu = 0;
                int first_pin_orientation = -1;
                int first_pin_length_iu = -1;
                int first_pin_root_x_iu = 0;
                int first_pin_root_y_iu = 0;
                bool first_pin_is_power = false;
                bool first_pin_is_global_power = false;
                bool first_pin_is_local_power = false;
                const auto fields = out.result().fields();

                auto itCount = fields.find( "count" );
                if( itCount != fields.end()
                        && itCount->second.kind_case() == google::protobuf::Value::kNumberValue )
                    count = static_cast<int>( itCount->second.number_value() );

                // Best-effort extraction of first pin KIID value.
                auto itPins = fields.find( "pins" );
                if( itPins == fields.end() )
                    itPins = fields.find( "items" );

                if( itPins != fields.end() && itPins->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    const auto& vals = itPins->second.list_value().values();
                    if( !vals.empty() && vals.Get( 0 ).kind_case() == google::protobuf::Value::kStructValue )
                    {
                        const auto& pinStruct = vals.Get( 0 ).struct_value();
                        auto         pinFields = pinStruct.fields();
                        auto         itId = pinFields.find( "id" );
                        if( itId != pinFields.end() )
                        {
                            if( itId->second.kind_case() == google::protobuf::Value::kStringValue )
                            {
                                first_pin_id = itId->second.string_value();
                            }
                            else if( itId->second.kind_case()
                                     == google::protobuf::Value::kStructValue )
                            {
                                // Back-compat for the `Any`-to-Struct case where `id` was nested as `{value: ...}`.
                                const auto& idStruct = itId->second.struct_value();
                                auto         idFields = idStruct.fields();
                                auto         itVal = idFields.find( "value" );
                                if( itVal != idFields.end()
                                        && itVal->second.kind_case()
                                               == google::protobuf::Value::kStringValue )
                                    first_pin_id = itVal->second.string_value();
                            }
                        }

                        // Optional geometric/orientation fields (added to get_symbol_pins_by_id).
                        auto itTypeName = pinFields.find( "pin_type_name" );
                        if( itTypeName != pinFields.end()
                                && itTypeName->second.kind_case()
                                       == google::protobuf::Value::kStringValue )
                            first_pin_type_name = itTypeName->second.string_value();

                        auto itPos = pinFields.find( "position" );
                        if( itPos != pinFields.end()
                                && itPos->second.kind_case() == google::protobuf::Value::kStructValue )
                        {
                            auto posFields = itPos->second.struct_value().fields();

                            auto itX = posFields.find( "x_iu" );
                            if( itX != posFields.end()
                                    && itX->second.kind_case() == google::protobuf::Value::kNumberValue )
                                first_pin_x_iu = static_cast<int>( itX->second.number_value() );

                            auto itY = posFields.find( "y_iu" );
                            if( itY != posFields.end()
                                    && itY->second.kind_case() == google::protobuf::Value::kNumberValue )
                                first_pin_y_iu = static_cast<int>( itY->second.number_value() );
                        }

                        auto itOri = pinFields.find( "orientation" );
                        if( itOri != pinFields.end()
                                && itOri->second.kind_case() == google::protobuf::Value::kNumberValue )
                            first_pin_orientation = static_cast<int>( itOri->second.number_value() );

                        auto itOriName = pinFields.find( "orientation_name" );
                        if( itOriName != pinFields.end()
                                && itOriName->second.kind_case()
                                       == google::protobuf::Value::kStringValue )
                            first_pin_orientation_name = itOriName->second.string_value();

                        auto itLen = pinFields.find( "length_iu" );
                        if( itLen != pinFields.end()
                                && itLen->second.kind_case() == google::protobuf::Value::kNumberValue )
                            first_pin_length_iu = static_cast<int>( itLen->second.number_value() );

                        auto itRoot = pinFields.find( "pin_root" );
                        if( itRoot != pinFields.end()
                                && itRoot->second.kind_case() == google::protobuf::Value::kStructValue )
                        {
                            auto rootFields = itRoot->second.struct_value().fields();
                            auto itRX = rootFields.find( "x_iu" );
                            if( itRX != rootFields.end()
                                    && itRX->second.kind_case() == google::protobuf::Value::kNumberValue )
                                first_pin_root_x_iu = static_cast<int>( itRX->second.number_value() );
                            auto itRY = rootFields.find( "y_iu" );
                            if( itRY != rootFields.end()
                                    && itRY->second.kind_case() == google::protobuf::Value::kNumberValue )
                                first_pin_root_y_iu = static_cast<int>( itRY->second.number_value() );
                        }

                        auto itIsPower = pinFields.find( "is_power" );
                        if( itIsPower != pinFields.end()
                                && itIsPower->second.kind_case() == google::protobuf::Value::kBoolValue )
                            first_pin_is_power = itIsPower->second.bool_value();

                        auto itIsGlobalPower = pinFields.find( "is_global_power" );
                        if( itIsGlobalPower != pinFields.end()
                                && itIsGlobalPower->second.kind_case()
                                       == google::protobuf::Value::kBoolValue )
                            first_pin_is_global_power = itIsGlobalPower->second.bool_value();

                        auto itIsLocalPower = pinFields.find( "is_local_power" );
                        if( itIsLocalPower != pinFields.end()
                                && itIsLocalPower->second.kind_case()
                                       == google::protobuf::Value::kBoolValue )
                            first_pin_is_local_power = itIsLocalPower->second.bool_value();

                        auto itNetName = pinFields.find( "net_name" );
                        if( itNetName != pinFields.end()
                                && itNetName->second.kind_case()
                                       == google::protobuf::Value::kStringValue )
                            first_pin_net_name = itNetName->second.string_value();

                        auto itNetNameSource = pinFields.find( "net_name_source" );
                        if( itNetNameSource != pinFields.end()
                                && itNetNameSource->second.kind_case()
                                       == google::protobuf::Value::kStringValue )
                            first_pin_net_name_source = itNetNameSource->second.string_value();

                        auto itNetNameDefault = pinFields.find( "net_name_default" );
                        if( itNetNameDefault != pinFields.end()
                                && itNetNameDefault->second.kind_case()
                                       == google::protobuf::Value::kStringValue )
                            first_pin_net_name_default = itNetNameDefault->second.string_value();
                    }
                }

                // Optional grouped connectivity view (`nets`) added to get_symbol_pins_by_id.
                auto itNets = fields.find( "nets" );
                if( itNets != fields.end()
                        && itNets->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    nets_count = static_cast<int>( itNets->second.list_value().values_size() );
                    const auto& netVals = itNets->second.list_value().values();
                    if( !netVals.empty() && netVals.Get( 0 ).kind_case()
                                              == google::protobuf::Value::kStructValue )
                    {
                        const auto&         netStruct = netVals.Get( 0 ).struct_value();
                        auto                 netFields = netStruct.fields();
                        auto                 itNetName = netFields.find( "net_name" );
                        if( itNetName != netFields.end()
                                && itNetName->second.kind_case() == google::protobuf::Value::kStringValue )
                            first_net_net_name = itNetName->second.string_value();

                        auto itNetNameSource = netFields.find( "net_name_source" );
                        if( itNetNameSource != netFields.end()
                                && itNetNameSource->second.kind_case() == google::protobuf::Value::kStringValue )
                            first_net_net_name_source = itNetNameSource->second.string_value();

                        auto itNetNameDefault = netFields.find( "net_name_default" );
                        if( itNetNameDefault != netFields.end()
                                && itNetNameDefault->second.kind_case() == google::protobuf::Value::kStringValue )
                            first_net_net_name_default = itNetNameDefault->second.string_value();

                        auto itNetPins = netFields.find( "pins" );
                        if( itNetPins != netFields.end()
                                && itNetPins->second.kind_case() == google::protobuf::Value::kListValue )
                        {
                            const auto& netPinVals = itNetPins->second.list_value().values();
                            first_net_pins_count = static_cast<int>( netPinVals.size() );
                            if( !netPinVals.empty()
                                    && netPinVals.Get( 0 ).kind_case()
                                           == google::protobuf::Value::kStructValue )
                            {
                                const auto& pinRefStruct = netPinVals.Get( 0 ).struct_value();
                                auto        refPinFields = pinRefStruct.fields();
                                auto        itRefId = refPinFields.find( "id" );
                                if( itRefId != refPinFields.end()
                                        && itRefId->second.kind_case()
                                               == google::protobuf::Value::kStringValue )
                                    first_net_first_pin_id = itRefId->second.string_value();
                            }
                        }
                    }
                }

                std::cout << "count=" << count << "\n";
                std::cout << "first_pin_id=" << first_pin_id << "\n";
                std::cout << "first_pin_type_name=" << first_pin_type_name << "\n";
                std::cout << "first_pin_x_iu=" << first_pin_x_iu << "\n";
                std::cout << "first_pin_y_iu=" << first_pin_y_iu << "\n";
                std::cout << "first_pin_orientation=" << first_pin_orientation << "\n";
                std::cout << "first_pin_orientation_name=" << first_pin_orientation_name << "\n";
                std::cout << "first_pin_length_iu=" << first_pin_length_iu << "\n";
                std::cout << "first_pin_root_x_iu=" << first_pin_root_x_iu << "\n";
                std::cout << "first_pin_root_y_iu=" << first_pin_root_y_iu << "\n";
                std::cout << "first_pin_is_power=" << ( first_pin_is_power ? "true" : "false" ) << "\n";
                std::cout << "first_pin_is_global_power="
                          << ( first_pin_is_global_power ? "true" : "false" ) << "\n";
                std::cout << "first_pin_is_local_power="
                          << ( first_pin_is_local_power ? "true" : "false" ) << "\n";
                std::cout << "first_pin_net_name=" << first_pin_net_name << "\n";
                std::cout << "first_pin_net_name_source=" << first_pin_net_name_source
                          << "\n";
                std::cout << "first_pin_net_name_default=" << first_pin_net_name_default << "\n";
                std::cout << "nets_count=" << nets_count << "\n";
                std::cout << "first_net_net_name=" << first_net_net_name << "\n";
                std::cout << "first_net_net_name_source=" << first_net_net_name_source << "\n";
                std::cout << "first_net_net_name_default=" << first_net_net_name_default << "\n";
                std::cout << "first_net_pins_count=" << first_net_pins_count << "\n";
                std::cout << "first_net_first_pin_id=" << first_net_first_pin_id << "\n";
            }
            else if( req.name() == "get_symbol_fields_by_id" )
            {
                std::string reference;
                std::string value;
                std::string lib_id;
                bool        is_power_symbol = false;
                bool        is_global_power_symbol = false;
                bool        is_local_power_symbol = false;

                const auto fields = out.result().fields();

                auto itRef = fields.find( "reference" );
                if( itRef != fields.end()
                        && itRef->second.kind_case() == google::protobuf::Value::kStringValue )
                    reference = itRef->second.string_value();

                auto itVal = fields.find( "value" );
                if( itVal != fields.end()
                        && itVal->second.kind_case() == google::protobuf::Value::kStringValue )
                    value = itVal->second.string_value();

                auto itLib = fields.find( "lib_id" );
                if( itLib != fields.end()
                        && itLib->second.kind_case() == google::protobuf::Value::kStringValue )
                    lib_id = itLib->second.string_value();

                auto itIsPower = fields.find( "is_power_symbol" );
                if( itIsPower != fields.end()
                        && itIsPower->second.kind_case() == google::protobuf::Value::kBoolValue )
                    is_power_symbol = itIsPower->second.bool_value();

                auto itIsGlobalPower = fields.find( "is_global_power_symbol" );
                if( itIsGlobalPower != fields.end()
                        && itIsGlobalPower->second.kind_case() == google::protobuf::Value::kBoolValue )
                    is_global_power_symbol = itIsGlobalPower->second.bool_value();

                auto itIsLocalPower = fields.find( "is_local_power_symbol" );
                if( itIsLocalPower != fields.end()
                        && itIsLocalPower->second.kind_case() == google::protobuf::Value::kBoolValue )
                    is_local_power_symbol = itIsLocalPower->second.bool_value();

                std::cout << "reference=" << reference << "\n";
                std::cout << "value=" << value << "\n";
                std::cout << "lib_id=" << lib_id << "\n";
                std::cout << "is_power_symbol=" << ( is_power_symbol ? "true" : "false" ) << "\n";
                std::cout << "is_global_power_symbol="
                          << ( is_global_power_symbol ? "true" : "false" ) << "\n";
                std::cout << "is_local_power_symbol="
                          << ( is_local_power_symbol ? "true" : "false" ) << "\n";
            }
            else if( req.name() == "get_board_stackup" )
            {
                int layerCount = 0;
                int enabledCopper = 0;

                const auto fields = out.result().fields();

                auto itLayer = fields.find( "layer_count" );
                if( itLayer != fields.end() && itLayer->second.kind_case() == google::protobuf::Value::kNumberValue )
                    layerCount = static_cast<int>( itLayer->second.number_value() );

                auto itCopper = fields.find( "enabled_copper_layer_count" );
                if( itCopper != fields.end() && itCopper->second.kind_case() == google::protobuf::Value::kNumberValue )
                    enabledCopper = static_cast<int>( itCopper->second.number_value() );

                std::cout << "layer_count=" << layerCount << "\n";
                std::cout << "enabled_copper_layer_count=" << enabledCopper << "\n";
            }
            else if( req.name() == "library_symbol_search_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto numInt = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                int         itemsCount = 0;
                bool        itemsFieldPresent = false;
                std::string firstItems;
                auto        itItems = fields.find( "items" );
                if( itItems != fields.end()
                        && itItems->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    itemsFieldPresent = true;
                    const auto& vals = itItems->second.list_value().values();
                    itemsCount = static_cast<int>( vals.size() );

                    const int firstN = std::min<int>( 5, itemsCount );
                    for( int i = 0; i < firstN; ++i )
                    {
                        const auto& v = vals.Get( i );
                        if( v.kind_case() != google::protobuf::Value::kStructValue )
                            continue;

                        const auto& entryFields = v.struct_value().fields();
                        auto        itQualified = entryFields.find( "qualified_name" );
                        if( itQualified == entryFields.end()
                                || itQualified->second.kind_case()
                                           != google::protobuf::Value::kStringValue )
                        {
                            continue;
                        }

                        if( !firstItems.empty() )
                            firstItems += ";";
                        firstItems += itQualified->second.string_value();
                    }
                }

                if( firstItems.empty() )
                    firstItems = "none";

                std::cout << "search_ok=" << numFlag( "search_ok" ) << "\n";
                std::cout << "query=" << strField( "query" ) << "\n";
                std::cout << "library_scope=" << strField( "library_scope" ) << "\n";
                std::cout << "count=" << numInt( "count" ) << "\n";
                std::cout << "items_present=" << ( itemsFieldPresent ? "true" : "false" ) << "\n";
                std::cout << "first_items=" << firstItems << "\n";
                std::cout << "diagnostics=" << strField( "diagnostics" ) << "\n";
                std::cout << "diagnostics_present="
                          << ( strField( "diagnostics" ) == "absent" ? "false" : "true" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "library_footprint_search_session" )
            {
                // R115 — omit full ApiResponse dump (printResponse); keep Struct debug lines for harness regex.
                // Trim large items[] if max_results is high (defense in depth vs stdout pipe pressure).
                google::protobuf::Struct trimmed;
                trimmed.CopyFrom( out.result() );
                auto* fields = trimmed.mutable_fields();
                auto  itItems = fields->find( "items" );
                if( itItems != fields->end()
                    && itItems->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    const int n = itItems->second.list_value().values_size();
                    const int keepN = std::min<int>( 5, n );
                    if( n > keepN )
                    {
                        google::protobuf::ListValue kept;
                        for( int i = 0; i < keepN; ++i )
                            *kept.add_values() = itItems->second.list_value().values().Get( i );

                        google::protobuf::Value placeholder;
                        placeholder.set_string_value(
                                std::string( "omitted_items_entries(n=" ) + std::to_string( n - keepN ) + ")" );
                        *kept.add_values() = std::move( placeholder );

                        google::protobuf::Value listVal;
                        *listVal.mutable_list_value() = std::move( kept );
                        ( *fields )["items"] = std::move( listVal );
                    }
                }
                std::cout << trimmed.Utf8DebugString() << "\n";
            }
            else if( req.name() == "library_footprint_inspect_session" )
            {
                // R115 — result is compact scalars; skip full ApiResponse dump only.
                std::cout << out.result().Utf8DebugString() << "\n";
            }
            else if( req.name() == "get_nets" )
            {
                int         count = 0;
                std::string firstNetName;
                int         nonEmptyNameCount = 0;

                const auto fields = out.result().fields();
                auto       itNets = fields.find( "nets" );
                if( itNets != fields.end() && itNets->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    const auto& vals = itNets->second.list_value().values();
                    count = static_cast<int>( vals.size() );

                    if( vals.size() > 0 )
                    {
                        for( const auto& netVal : vals )
                        {
                            if( netVal.kind_case() != google::protobuf::Value::kStructValue )
                                continue;

                            const auto& netStruct = netVal.struct_value();
                            auto         netFields = netStruct.fields();
                            auto         itName = netFields.find( "name" );
                            if( itName != netFields.end()
                                    && itName->second.kind_case() == google::protobuf::Value::kStringValue
                                    && !itName->second.string_value().empty() )
                            {
                                nonEmptyNameCount++;
                                if( firstNetName.empty() )
                                    firstNetName = itName->second.string_value();
                            }
                        }
                    }
                }

                std::cout << "count=" << count << "\n";
                std::cout << "first_net_name=" << firstNetName << "\n";
                std::cout << "non_empty_name_count=" << nonEmptyNameCount << "\n";
            }
            else if( req.name() == "get_bounding_box_pcb" )
            {
                int  count = 0;
                bool hasFirstBox = false;

                const auto fields = out.result().fields();
                auto itBoxes = fields.find( "boxes" );
                if( itBoxes != fields.end()
                        && itBoxes->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    const auto& vals = itBoxes->second.list_value().values();
                    count = static_cast<int>( vals.size() );
                    hasFirstBox = vals.size() > 0;
                }

                std::cout << "count=" << count << "\n";
                std::cout << "has_first_box=" << ( hasFirstBox ? "true" : "false" ) << "\n";
            }
            else if( req.name() == "get_visible_layers" || req.name() == "set_visible_layers" )
            {
                int         count = 0;
                int         firstLayer = -1;
                std::string layersCsv;

                const auto fields = out.result().fields();
                auto itLayers = fields.find( "layers" );
                if( itLayers != fields.end()
                        && itLayers->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    const auto& vals = itLayers->second.list_value().values();
                    count = static_cast<int>( vals.size() );
                    if( vals.size() > 0 && vals.Get( 0 ).kind_case() == google::protobuf::Value::kNumberValue )
                        firstLayer = static_cast<int>( vals.Get( 0 ).number_value() );

                    std::string tmp;
                    for( int i = 0; i < static_cast<int>( vals.size() ); ++i )
                    {
                        const auto& v = vals.Get( i );
                        if( v.kind_case() != google::protobuf::Value::kNumberValue )
                            continue;

                        const std::string s = std::to_string( static_cast<int>( v.number_value() ) );
                        if( tmp.empty() )
                            tmp = s;
                        else
                            tmp += "," + s;
                    }
                    layersCsv = tmp;
                }

                std::cout << "count=" << count << "\n";
                std::cout << "first_layer=" << firstLayer << "\n";
                if( !layersCsv.empty() )
                    std::cout << "layers=" << layersCsv << "\n";
            }
            else if( req.name() == "get_board_enabled_layers" )
            {
                int count = 0;
                int firstLayer = -1;
                std::string layersCsv;

                const auto fields = out.result().fields();
                auto itLayers = fields.find( "layers" );
                if( itLayers != fields.end()
                        && itLayers->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    const auto& vals = itLayers->second.list_value().values();
                    count = static_cast<int>( vals.size() );
                    if( vals.size() > 0 && vals.Get( 0 ).kind_case() == google::protobuf::Value::kNumberValue )
                        firstLayer = static_cast<int>( vals.Get( 0 ).number_value() );

                    // Best-effort CSV of enabled layer enum values for deterministic test selection.
                    std::string tmp;
                    for( int i = 0; i < static_cast<int>( vals.size() ); ++i )
                    {
                        const auto& v = vals.Get( i );
                        if( v.kind_case() != google::protobuf::Value::kNumberValue )
                            continue;

                        const std::string s = std::to_string( static_cast<int>( v.number_value() ) );
                        if( tmp.empty() )
                            tmp = s;
                        else
                            tmp += "," + s;
                    }
                    layersCsv = tmp;
                }

                std::cout << "count=" << count << "\n";
                std::cout << "first_layer=" << firstLayer << "\n";
                if( !layersCsv.empty() )
                    std::cout << "layers=" << layersCsv << "\n";
            }
            else if( req.name() == "get_board_layer_names" )
            {
                int         count = 0;
                std::string firstLayerName;

                const auto fields = out.result().fields();
                auto itLayers = fields.find( "layers" );
                if( itLayers != fields.end()
                        && itLayers->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    const auto& vals = itLayers->second.list_value().values();
                    count = static_cast<int>( vals.size() );

                    if( vals.size() > 0 && vals.Get( 0 ).kind_case() == google::protobuf::Value::kStructValue )
                    {
                        const auto& firstFields = vals.Get( 0 ).struct_value().fields();
                        auto        itName = firstFields.find( "name" );
                        if( itName != firstFields.end()
                                && itName->second.kind_case() == google::protobuf::Value::kStringValue )
                        {
                            firstLayerName = itName->second.string_value();
                        }
                    }
                }

                std::cout << "count=" << count << "\n";
                std::cout << "first_layer_name=" << firstLayerName << "\n";
            }
            else if( req.name() == "get_active_layer" )
            {
                int         activeLayer = -1;
                std::string activeLayerName;

                const auto fields = out.result().fields();
                auto       itLayer = fields.find( "layer" );
                if( itLayer != fields.end() && itLayer->second.kind_case() == google::protobuf::Value::kNumberValue )
                    activeLayer = static_cast<int>( itLayer->second.number_value() );

                auto itName = fields.find( "name" );
                if( itName != fields.end() && itName->second.kind_case() == google::protobuf::Value::kStringValue )
                    activeLayerName = itName->second.string_value();

                std::cout << "active_layer=" << activeLayer << "\n";
                std::cout << "active_layer_name=" << activeLayerName << "\n";
            }
            else if( req.name() == "set_active_layer" )
            {
                int         activeLayer = -1;
                std::string activeLayerName;

                const auto fields = out.result().fields();
                auto       itLayer = fields.find( "layer" );
                if( itLayer != fields.end()
                        && itLayer->second.kind_case() == google::protobuf::Value::kNumberValue )
                    activeLayer = static_cast<int>( itLayer->second.number_value() );

                auto itName = fields.find( "name" );
                if( itName != fields.end()
                        && itName->second.kind_case() == google::protobuf::Value::kStringValue )
                    activeLayerName = itName->second.string_value();

                std::cout << "active_layer=" << activeLayer << "\n";
                std::cout << "active_layer_name=" << activeLayerName << "\n";
            }
            else if( req.name() == "get_board_editor_appearance_settings" )
            {
                const auto fields = out.result().fields();
                const bool hasInactive = fields.find( "inactive_layer_display" ) != fields.end();
                const bool hasNetColor = fields.find( "net_color_display" ) != fields.end();
                const bool hasBoardFlip = fields.find( "board_flip" ) != fields.end();
                const bool hasRatsnest = fields.find( "ratsnest_display" ) != fields.end();

                std::cout << "output_shape_summary="
                          << "inactive_layer_display=" << ( hasInactive ? "present" : "missing" ) << ";"
                          << "net_color_display=" << ( hasNetColor ? "present" : "missing" ) << ";"
                          << "board_flip=" << ( hasBoardFlip ? "present" : "missing" ) << ";"
                          << "ratsnest_display=" << ( hasRatsnest ? "present" : "missing" ) << "\n";

                int inactiveLayerDisplay = -1;
                int netColorDisplay = -1;
                int boardFlip = -1;
                int ratsnestDisplay = -1;

                auto itInactive = fields.find( "inactive_layer_display" );
                if( itInactive != fields.end()
                        && itInactive->second.kind_case() == google::protobuf::Value::kNumberValue )
                    inactiveLayerDisplay = static_cast<int>( itInactive->second.number_value() );

                auto itNetColor = fields.find( "net_color_display" );
                if( itNetColor != fields.end()
                        && itNetColor->second.kind_case() == google::protobuf::Value::kNumberValue )
                    netColorDisplay = static_cast<int>( itNetColor->second.number_value() );

                auto itBoardFlip = fields.find( "board_flip" );
                if( itBoardFlip != fields.end()
                        && itBoardFlip->second.kind_case() == google::protobuf::Value::kNumberValue )
                    boardFlip = static_cast<int>( itBoardFlip->second.number_value() );

                auto itRatsnest = fields.find( "ratsnest_display" );
                if( itRatsnest != fields.end()
                        && itRatsnest->second.kind_case() == google::protobuf::Value::kNumberValue )
                    ratsnestDisplay = static_cast<int>( itRatsnest->second.number_value() );

                std::cout << "output_value_summary="
                          << "inactive_layer_display=" << inactiveLayerDisplay << ";"
                          << "net_color_display=" << netColorDisplay << ";"
                          << "board_flip=" << boardFlip << ";"
                          << "ratsnest_display=" << ratsnestDisplay << "\n";
            }
            else if( req.name() == "get_board_origin" )
            {
                const auto fields = out.result().fields();
                const bool hasOrigin = fields.find( "origin" ) != fields.end();
                const bool hasGridOrigin = fields.find( "grid_origin" ) != fields.end();
                const bool hasDrillOrigin = fields.find( "drill_origin" ) != fields.end();

                std::cout << "origin_shape_summary="
                          << "origin=" << ( hasOrigin ? "present" : "missing" ) << ";"
                          << "grid_origin=" << ( hasGridOrigin ? "present" : "missing" ) << ";"
                          << "drill_origin=" << ( hasDrillOrigin ? "present" : "missing" ) << "\n";

                // If the tool returns the expected Vector2 structs, extract numeric values.
                int64_t originX = -1;
                int64_t originY = -1;
                int64_t gridOriginX = -1;
                int64_t gridOriginY = -1;
                int64_t drillOriginX = -1;
                int64_t drillOriginY = -1;

                auto extractVector2 = []( const auto& structValue, int64_t& outX, int64_t& outY )
                {
                    const auto& sfields = structValue.fields();
                    // Accept either {x_nm,y_nm} or {x,y} depending on protobuf-to-struct mapping.
                    auto itX = sfields.find( "x_nm" );
                    if( itX == sfields.end() )
                        itX = sfields.find( "x" );
                    auto itY = sfields.find( "y_nm" );
                    if( itY == sfields.end() )
                        itY = sfields.find( "y" );

                    if( itX == sfields.end() || itY == sfields.end() )
                        return;

                    auto parseValue = []( const google::protobuf::Value& v ) -> std::optional<int64_t>
                    {
                        switch( v.kind_case() )
                        {
                        case google::protobuf::Value::kNumberValue:
                            return static_cast<int64_t>( v.number_value() );
                        case google::protobuf::Value::kStringValue:
                        {
                            try
                            {
                                return std::stoll( v.string_value() );
                            }
                            catch( ... )
                            {
                                return std::nullopt;
                            }
                        }
                        default:
                            return std::nullopt;
                        }
                    };

                    auto px = parseValue( itX->second );
                    auto py = parseValue( itY->second );
                    if( px && py )
                    {
                        outX = *px;
                        outY = *py;
                    }
                };

                if( hasOrigin && fields.at( "origin" ).kind_case() == google::protobuf::Value::kStructValue )
                    extractVector2( fields.at( "origin" ).struct_value(), originX, originY );

                if( hasGridOrigin
                        && fields.at( "grid_origin" ).kind_case() == google::protobuf::Value::kStructValue )
                    extractVector2( fields.at( "grid_origin" ).struct_value(), gridOriginX, gridOriginY );

                if( hasDrillOrigin
                        && fields.at( "drill_origin" ).kind_case() == google::protobuf::Value::kStructValue )
                    extractVector2( fields.at( "drill_origin" ).struct_value(), drillOriginX, drillOriginY );

                std::cout << "output_value_summary="
                          << "origin_x_nm=" << originX << ";"
                          << "origin_y_nm=" << originY << ";"
                          << "grid_origin_x_nm=" << gridOriginX << ";"
                          << "grid_origin_y_nm=" << gridOriginY << ";"
                          << "drill_origin_x_nm=" << drillOriginX << ";"
                          << "drill_origin_y_nm=" << drillOriginY << "\n";
            }
            else if( req.name() == "create_pcb_items" )
            {
                const auto fields = out.result().fields();
                std::string createdId;
                double      itemStatusCode = -1.0;
                double      layerVal = -1.0;
                std::string itemType;

                auto itId = fields.find( "created_item_id" );
                if( itId != fields.end() && itId->second.kind_case() == google::protobuf::Value::kStringValue )
                    createdId = itId->second.string_value();

                auto itCode = fields.find( "item_status_code" );
                if( itCode != fields.end()
                        && itCode->second.kind_case() == google::protobuf::Value::kNumberValue )
                    itemStatusCode = itCode->second.number_value();

                auto itLayer = fields.find( "layer" );
                if( itLayer != fields.end()
                        && itLayer->second.kind_case() == google::protobuf::Value::kNumberValue )
                    layerVal = itLayer->second.number_value();

                auto itTy = fields.find( "item_type" );
                if( itTy != fields.end() && itTy->second.kind_case() == google::protobuf::Value::kStringValue )
                    itemType = itTy->second.string_value();

                std::cout << "created_item_id=" << createdId << "\n";
                std::cout << "item_status_code=" << static_cast<int>( itemStatusCode ) << "\n";
                std::cout << "layer=" << static_cast<int>( layerVal ) << "\n";
                std::cout << "item_type=" << itemType << "\n";
            }
            else if( req.name() == "delete_pcb_items" )
            {
                const auto fields = out.result().fields();
                std::string deletedId;
                double      deletionStatus = -1.0;

                auto itId = fields.find( "deleted_item_id" );
                if( itId != fields.end() && itId->second.kind_case() == google::protobuf::Value::kStringValue )
                    deletedId = itId->second.string_value();

                auto itSt = fields.find( "deletion_status" );
                if( itSt != fields.end()
                        && itSt->second.kind_case() == google::protobuf::Value::kNumberValue )
                    deletionStatus = itSt->second.number_value();

                std::cout << "deleted_item_id=" << deletedId << "\n";
                std::cout << "deletion_status=" << static_cast<int>( deletionStatus ) << "\n";
            }
            else if( req.name() == "update_pcb_items" )
            {
                const auto fields = out.result().fields();
                std::string itemId;
                double      itemStatusCode = -1.0;
                std::int64_t prevEndX = 0;
                std::int64_t prevEndY = 0;
                std::int64_t endX = 0;
                std::int64_t endY = 0;
                double       layerVal = -1.0;
                std::string itemType;
                bool         hasPrevX = false;
                bool         hasPrevY = false;
                bool         hasEndX = false;
                bool         hasEndY = false;

                auto itId = fields.find( "item_id" );
                if( itId != fields.end() && itId->second.kind_case() == google::protobuf::Value::kStringValue )
                    itemId = itId->second.string_value();

                auto itCode = fields.find( "item_status_code" );
                if( itCode != fields.end()
                        && itCode->second.kind_case() == google::protobuf::Value::kNumberValue )
                    itemStatusCode = itCode->second.number_value();

                auto itPx = fields.find( "previous_end_x_nm" );
                if( itPx != fields.end()
                        && itPx->second.kind_case() == google::protobuf::Value::kNumberValue )
                {
                    prevEndX = static_cast<std::int64_t>( itPx->second.number_value() );
                    hasPrevX = true;
                }

                auto itPy = fields.find( "previous_end_y_nm" );
                if( itPy != fields.end()
                        && itPy->second.kind_case() == google::protobuf::Value::kNumberValue )
                {
                    prevEndY = static_cast<std::int64_t>( itPy->second.number_value() );
                    hasPrevY = true;
                }

                auto itEx = fields.find( "end_x_nm" );
                if( itEx != fields.end()
                        && itEx->second.kind_case() == google::protobuf::Value::kNumberValue )
                {
                    endX = static_cast<std::int64_t>( itEx->second.number_value() );
                    hasEndX = true;
                }

                auto itEy = fields.find( "end_y_nm" );
                if( itEy != fields.end()
                        && itEy->second.kind_case() == google::protobuf::Value::kNumberValue )
                {
                    endY = static_cast<std::int64_t>( itEy->second.number_value() );
                    hasEndY = true;
                }

                auto itLayer = fields.find( "layer" );
                if( itLayer != fields.end()
                        && itLayer->second.kind_case() == google::protobuf::Value::kNumberValue )
                    layerVal = itLayer->second.number_value();

                auto itTy = fields.find( "item_type" );
                if( itTy != fields.end() && itTy->second.kind_case() == google::protobuf::Value::kStringValue )
                    itemType = itTy->second.string_value();

                std::cout << "item_id=" << itemId << "\n";
                std::cout << "item_status_code=" << static_cast<int>( itemStatusCode ) << "\n";
                std::cout << "previous_end_x_nm=" << ( hasPrevX ? std::to_string( prevEndX ) : "absent" ) << "\n";
                std::cout << "previous_end_y_nm=" << ( hasPrevY ? std::to_string( prevEndY ) : "absent" ) << "\n";
                std::cout << "end_x_nm=" << ( hasEndX ? std::to_string( endX ) : "absent" ) << "\n";
                std::cout << "end_y_nm=" << ( hasEndY ? std::to_string( endY ) : "absent" ) << "\n";
                std::cout << "layer=" << static_cast<int>( layerVal ) << "\n";
                std::cout << "item_type=" << itemType << "\n";
            }
            else if( req.name() == "pcb_graphic_annotation_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto numInt = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                std::cout << "created_item_id=" << strField( "created_item_id" ) << "\n";
                std::cout << "create_ok=" << numFlag( "create_ok" ) << "\n";
                std::cout << "verify_ok=" << numFlag( "verify_ok" ) << "\n";
                std::cout << "cleanup_ok=" << numFlag( "cleanup_ok" ) << "\n";
                std::cout << "board_state_restored=" << numFlag( "board_state_restored" ) << "\n";
                std::cout << "shape_count_initial=" << numInt( "shape_count_initial" ) << "\n";
                std::cout << "shape_count_final=" << numInt( "shape_count_final" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "pcb_fabrication_origin_prepare_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto numInt = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                std::cout << "origin_intent=" << strField( "origin_intent" ) << "\n";
                std::cout << "target_origin_x_nm=" << numInt( "target_origin_x_nm" ) << "\n";
                std::cout << "target_origin_y_nm=" << numInt( "target_origin_y_nm" ) << "\n";
                std::cout << "intent_resolution_ok=" << numFlag( "intent_resolution_ok" ) << "\n";
                std::cout << "dry_run_ok=" << numFlag( "dry_run_ok" ) << "\n";
                std::cout << "apply_ok=" << numFlag( "apply_ok" ) << "\n";
                std::cout << "verify_ok=" << numFlag( "verify_ok" ) << "\n";
                std::cout << "rollback_ok=" << numFlag( "rollback_ok" ) << "\n";
                std::cout << "board_state_restored=" << numFlag( "board_state_restored" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "pcb_origin_decision_correction_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto numInt = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                std::cout << "decision_mode=" << strField( "decision_mode" ) << "\n";
                std::cout << "origin_intent=" << strField( "origin_intent" ) << "\n";
                std::cout << "target_origin_x_nm=" << numInt( "target_origin_x_nm" ) << "\n";
                std::cout << "target_origin_y_nm=" << numInt( "target_origin_y_nm" ) << "\n";
                std::cout << "decision_taken_ok=" << numFlag( "decision_taken_ok" ) << "\n";
                std::cout << "correction_needed_detected=" << numFlag( "correction_needed_detected" ) << "\n";
                std::cout << "apply_ok=" << numFlag( "apply_ok" ) << "\n";
                std::cout << "verify_ok=" << numFlag( "verify_ok" ) << "\n";
                std::cout << "rollback_ok=" << numFlag( "rollback_ok" ) << "\n";
                std::cout << "board_state_restored=" << numFlag( "board_state_restored" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "create_sch_items" )
            {
                const auto fields = out.result().fields();
                std::string createdId;
                double      itemStatusCode = -1.0;
                double      layerVal = -1.0;
                std::string itemType;

                auto itId = fields.find( "created_item_id" );
                if( itId != fields.end() && itId->second.kind_case() == google::protobuf::Value::kStringValue )
                    createdId = itId->second.string_value();

                auto itCode = fields.find( "item_status_code" );
                if( itCode != fields.end()
                        && itCode->second.kind_case() == google::protobuf::Value::kNumberValue )
                    itemStatusCode = itCode->second.number_value();

                auto itLayer = fields.find( "layer" );
                if( itLayer != fields.end()
                        && itLayer->second.kind_case() == google::protobuf::Value::kNumberValue )
                    layerVal = itLayer->second.number_value();

                auto itTy = fields.find( "item_type" );
                if( itTy != fields.end() && itTy->second.kind_case() == google::protobuf::Value::kStringValue )
                    itemType = itTy->second.string_value();

                std::cout << "created_item_id=" << createdId << "\n";
                std::cout << "item_status_code=" << static_cast<int>( itemStatusCode ) << "\n";
                std::cout << "layer=" << static_cast<int>( layerVal ) << "\n";
                std::cout << "item_type=" << itemType << "\n";
            }
            else if( req.name() == "delete_sch_items" )
            {
                const auto fields = out.result().fields();
                std::string deletedId;
                double      deletionStatus = -1.0;

                auto itId = fields.find( "deleted_item_id" );
                if( itId != fields.end() && itId->second.kind_case() == google::protobuf::Value::kStringValue )
                    deletedId = itId->second.string_value();

                auto itSt = fields.find( "deletion_status" );
                if( itSt != fields.end()
                        && itSt->second.kind_case() == google::protobuf::Value::kNumberValue )
                    deletionStatus = itSt->second.number_value();

                std::cout << "deleted_item_id=" << deletedId << "\n";
                std::cout << "deletion_status=" << static_cast<int>( deletionStatus ) << "\n";
            }
            else if( req.name() == "update_sch_items" )
            {
                const auto fields = out.result().fields();
                std::string updatedId;
                double      itemStatusCode = -1.0;
                double      endX = 0.0;
                double      endY = 0.0;
                bool        hasEndX = false;
                bool        hasEndY = false;
                double      layerVal = -1.0;
                std::string itemType;

                auto itId = fields.find( "updated_item_id" );
                if( itId != fields.end() && itId->second.kind_case() == google::protobuf::Value::kStringValue )
                    updatedId = itId->second.string_value();

                auto itCode = fields.find( "item_status_code" );
                if( itCode != fields.end()
                        && itCode->second.kind_case() == google::protobuf::Value::kNumberValue )
                    itemStatusCode = itCode->second.number_value();

                auto itEx = fields.find( "end_x_nm" );
                if( itEx != fields.end() && itEx->second.kind_case() == google::protobuf::Value::kNumberValue )
                {
                    endX = itEx->second.number_value();
                    hasEndX = true;
                }

                auto itEy = fields.find( "end_y_nm" );
                if( itEy != fields.end() && itEy->second.kind_case() == google::protobuf::Value::kNumberValue )
                {
                    endY = itEy->second.number_value();
                    hasEndY = true;
                }

                auto itLayer = fields.find( "layer" );
                if( itLayer != fields.end()
                        && itLayer->second.kind_case() == google::protobuf::Value::kNumberValue )
                    layerVal = itLayer->second.number_value();

                auto itTy = fields.find( "item_type" );
                if( itTy != fields.end() && itTy->second.kind_case() == google::protobuf::Value::kStringValue )
                    itemType = itTy->second.string_value();

                std::cout << "updated_item_id=" << updatedId << "\n";
                std::cout << "item_status_code=" << static_cast<int>( itemStatusCode ) << "\n";
                std::cout << "end_x_nm=" << ( hasEndX ? std::to_string( static_cast<int64_t>( endX ) ) : "absent" )
                          << "\n";
                std::cout << "end_y_nm=" << ( hasEndY ? std::to_string( static_cast<int64_t>( endY ) ) : "absent" )
                          << "\n";
                std::cout << "layer=" << static_cast<int>( layerVal ) << "\n";
                std::cout << "item_type=" << itemType << "\n";
            }
            else if( req.name() == "sch_wire_edit_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto numCount = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                };

                std::string blocker = "absent";
                auto itBl = fields.find( "exact_blocker_if_any" );
                if( itBl != fields.end() && itBl->second.kind_case() == google::protobuf::Value::kStringValue )
                    blocker = itBl->second.string_value();

                std::string sid = "absent";
                auto itId = fields.find( "created_item_id" );
                if( itId != fields.end() && itId->second.kind_case() == google::protobuf::Value::kStringValue )
                    sid = itId->second.string_value();

                std::cout << "created_item_id=" << sid << "\n";
                std::cout << "create_ok=" << numFlag( "create_ok" ) << "\n";
                std::cout << "read_ok=" << numFlag( "read_ok" ) << "\n";
                std::cout << "update_ok=" << numFlag( "update_ok" ) << "\n";
                std::cout << "rollback_ok=" << numFlag( "rollback_ok" ) << "\n";
                std::cout << "delete_ok=" << numFlag( "delete_ok" ) << "\n";
                std::cout << "line_count_restored=" << numFlag( "line_count_restored" ) << "\n";
                std::cout << "sch_line_count_initial=" << numCount( "sch_line_count_initial" ) << "\n";
                std::cout << "sch_line_count_final=" << numCount( "sch_line_count_final" ) << "\n";
                std::cout << "exact_blocker_if_any=" << blocker << "\n";
            }
            else if( req.name() == "get_board_extent" )
            {
                const auto fields = out.result().fields();
                bool hasExtent = false;
                bool hasPosition = false;
                bool hasSize = false;
                bool hasPosX = false;
                bool hasPosY = false;
                bool hasSizeX = false;
                bool hasSizeY = false;

                auto itExtent = fields.find( "extent" );
                if( itExtent != fields.end()
                        && itExtent->second.kind_case() == google::protobuf::Value::kStructValue )
                {
                    hasExtent = true;
                    const auto& efields = itExtent->second.struct_value().fields();
                    auto itPos = efields.find( "position" );
                    auto itSize = efields.find( "size" );
                    hasPosition = ( itPos != efields.end()
                                    && itPos->second.kind_case() == google::protobuf::Value::kStructValue );
                    hasSize = ( itSize != efields.end()
                                && itSize->second.kind_case() == google::protobuf::Value::kStructValue );

                    if( hasPosition )
                    {
                        const auto& pfields = itPos->second.struct_value().fields();
                        hasPosX = pfields.find( "x_nm" ) != pfields.end();
                        hasPosY = pfields.find( "y_nm" ) != pfields.end();
                    }

                    if( hasSize )
                    {
                        const auto& sfields = itSize->second.struct_value().fields();
                        hasSizeX = sfields.find( "x_nm" ) != sfields.end();
                        hasSizeY = sfields.find( "y_nm" ) != sfields.end();
                    }
                }

                std::cout << "extent_shape_summary="
                          << "extent=" << ( hasExtent ? "present" : "missing" ) << ";"
                          << "position=" << ( hasPosition ? "present" : "missing" ) << ";"
                          << "size=" << ( hasSize ? "present" : "missing" ) << ";"
                          << "position_x_nm=" << ( hasPosX ? "present" : "missing" ) << ";"
                          << "position_y_nm=" << ( hasPosY ? "present" : "missing" ) << ";"
                          << "size_x_nm=" << ( hasSizeX ? "present" : "missing" ) << ";"
                          << "size_y_nm=" << ( hasSizeY ? "present" : "missing" ) << "\n";
            }
            else if( req.name() == "get_net_classes" )
            {
                google::protobuf::util::JsonPrintOptions popts;
                popts.add_whitespace = false;
                popts.preserve_proto_field_names = true;

                std::string j;
                if( google::protobuf::util::MessageToJsonString( out.result(), &j, popts ).ok() )
                    std::cout << "result_json=" << j << "\n";

                std::cout << "net_classes_count="
                          << structListSize( out.result(), "net_classes" ).value_or( 0 ) << "\n";
            }
            else if( req.name() == "set_net_classes" )
            {
                std::cout << "set_net_classes_ok=true\n";
            }
            else if( req.name() == "netclass_patch_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto numInt = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                std::cout << "class_name=" << strField( "class_name" ) << "\n";
                std::cout << "track_width_nm=" << numInt( "track_width_nm" ) << "\n";
                std::cout << "dry_run_ok=" << numFlag( "dry_run_ok" ) << "\n";
                std::cout << "apply_ok=" << numFlag( "apply_ok" ) << "\n";
                std::cout << "verify_ok=" << numFlag( "verify_ok" ) << "\n";
                std::cout << "rollback_ok=" << numFlag( "rollback_ok" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "electrical_profile_prepare_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto numInt = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                std::cout << "profile_intent=" << strField( "profile_intent" ) << "\n";
                std::cout << "class_name=" << strField( "class_name" ) << "\n";
                std::cout << "track_width_nm=" << numInt( "track_width_nm" ) << "\n";
                std::cout << "intent_resolution_ok=" << numFlag( "intent_resolution_ok" ) << "\n";
                std::cout << "dry_run_ok=" << numFlag( "dry_run_ok" ) << "\n";
                std::cout << "apply_ok=" << numFlag( "apply_ok" ) << "\n";
                std::cout << "verify_ok=" << numFlag( "verify_ok" ) << "\n";
                std::cout << "rollback_ok=" << numFlag( "rollback_ok" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "sch_wire_quick_fix_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto numInt = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                std::cout << "target_wire_id=" << strField( "target_wire_id" ) << "\n";
                std::cout << "target_resolved_ok=" << numFlag( "target_resolved_ok" ) << "\n";
                std::cout << "create_or_update_ok=" << numFlag( "create_or_update_ok" ) << "\n";
                std::cout << "verify_ok=" << numFlag( "verify_ok" ) << "\n";
                std::cout << "rollback_ok=" << numFlag( "rollback_ok" ) << "\n";
                std::cout << "cleanup_ok=" << numFlag( "cleanup_ok" ) << "\n";
                std::cout << "original_end_x_nm=" << numInt( "original_end_x_nm" ) << "\n";
                std::cout << "original_end_y_nm=" << numInt( "original_end_y_nm" ) << "\n";
                std::cout << "updated_end_x_nm=" << numInt( "updated_end_x_nm" ) << "\n";
                std::cout << "updated_end_y_nm=" << numInt( "updated_end_y_nm" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "sch_pin_connection_fix_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                std::cout << "target_symbol_id=" << strField( "target_symbol_id" ) << "\n";
                std::cout << "target_pin_id=" << strField( "target_pin_id" ) << "\n";
                std::cout << "target_wire_id=" << strField( "target_wire_id" ) << "\n";
                std::cout << "target_pin_resolved_ok=" << numFlag( "target_pin_resolved_ok" ) << "\n";
                std::cout << "target_wire_resolved_ok=" << numFlag( "target_wire_resolved_ok" ) << "\n";
                std::cout << "apply_ok=" << numFlag( "apply_ok" ) << "\n";
                std::cout << "verify_ok=" << numFlag( "verify_ok" ) << "\n";
                std::cout << "rollback_ok=" << numFlag( "rollback_ok" ) << "\n";
                std::cout << "cleanup_ok=" << numFlag( "cleanup_ok" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "sch_connection_repair_decision_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                std::cout << "decision_mode=" << strField( "decision_mode" ) << "\n";
                std::cout << "target_symbol_id=" << strField( "target_symbol_id" ) << "\n";
                std::cout << "target_pin_id=" << strField( "target_pin_id" ) << "\n";
                std::cout << "target_wire_id=" << strField( "target_wire_id" ) << "\n";
                std::cout << "decision_taken_ok=" << numFlag( "decision_taken_ok" ) << "\n";
                std::cout << "repair_needed_detected=" << numFlag( "repair_needed_detected" ) << "\n";
                std::cout << "apply_ok=" << numFlag( "apply_ok" ) << "\n";
                std::cout << "verify_ok=" << numFlag( "verify_ok" ) << "\n";
                std::cout << "rollback_ok=" << numFlag( "rollback_ok" ) << "\n";
                std::cout << "cleanup_ok=" << numFlag( "cleanup_ok" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "cross_context_preflight_correction_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                std::cout << "decision_taken_ok=" << numFlag( "decision_taken_ok" ) << "\n";
                std::cout << "chosen_branch=" << strField( "chosen_branch" ) << "\n";
                std::cout << "correction_needed_detected=" << numFlag( "correction_needed_detected" ) << "\n";
                std::cout << "single_correction_enforced=" << numFlag( "single_correction_enforced" ) << "\n";
                std::cout << "apply_ok=" << numFlag( "apply_ok" ) << "\n";
                std::cout << "verify_ok=" << numFlag( "verify_ok" ) << "\n";
                std::cout << "rollback_ok=" << numFlag( "rollback_ok" ) << "\n";
                std::cout << "cleanup_or_state_restored_ok=" << numFlag( "cleanup_or_state_restored_ok" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "workflow_preflight_gate_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                std::cout << "decision_taken_ok=" << numFlag( "decision_taken_ok" ) << "\n";
                std::cout << "gate_decision=" << strField( "gate_decision" ) << "\n";
                std::cout << "chosen_branch=" << strField( "chosen_branch" ) << "\n";
                std::cout << "single_correction_enforced=" << numFlag( "single_correction_enforced" ) << "\n";
                std::cout << "apply_ok=" << numFlag( "apply_ok" ) << "\n";
                std::cout << "verify_ok=" << numFlag( "verify_ok" ) << "\n";
                std::cout << "rollback_ok=" << numFlag( "rollback_ok" ) << "\n";
                std::cout << "cleanup_or_state_restored_ok=" << numFlag( "cleanup_or_state_restored_ok" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "workflow_next_best_action_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                std::cout << "recommendation_taken_ok=" << numFlag( "recommendation_taken_ok" ) << "\n";
                std::cout << "recommended_action=" << strField( "recommended_action" ) << "\n";
                std::cout << "execute_mode=" << strField( "execute_mode" ) << "\n";
                std::cout << "single_action_enforced=" << numFlag( "single_action_enforced" ) << "\n";
                std::cout << "apply_ok=" << numFlag( "apply_ok" ) << "\n";
                std::cout << "verify_ok=" << numFlag( "verify_ok" ) << "\n";
                std::cout << "rollback_ok=" << numFlag( "rollback_ok" ) << "\n";
                std::cout << "cleanup_or_state_restored_ok=" << numFlag( "cleanup_or_state_restored_ok" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "workflow_handoff_readiness_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                std::cout << "handoff_decision_taken_ok=" << numFlag( "handoff_decision_taken_ok" ) << "\n";
                std::cout << "handoff_target=" << strField( "handoff_target" ) << "\n";
                std::cout << "handoff_decision=" << strField( "handoff_decision" ) << "\n";
                std::cout << "chosen_branch=" << strField( "chosen_branch" ) << "\n";
                std::cout << "single_action_enforced=" << numFlag( "single_action_enforced" ) << "\n";
                std::cout << "apply_ok=" << numFlag( "apply_ok" ) << "\n";
                std::cout << "verify_ok=" << numFlag( "verify_ok" ) << "\n";
                std::cout << "rollback_ok=" << numFlag( "rollback_ok" ) << "\n";
                std::cout << "cleanup_or_state_restored_ok=" << numFlag( "cleanup_or_state_restored_ok" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "workflow_assistant_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                std::cout << "assistant_decision_taken_ok=" << numFlag( "assistant_decision_taken_ok" ) << "\n";
                std::cout << "assistant_intent=" << strField( "assistant_intent" ) << "\n";
                std::cout << "chosen_workflow=" << strField( "chosen_workflow" ) << "\n";
                std::cout << "recommended_action=" << strField( "recommended_action" ) << "\n";
                std::cout << "single_action_enforced=" << numFlag( "single_action_enforced" ) << "\n";
                std::cout << "apply_ok=" << numFlag( "apply_ok" ) << "\n";
                std::cout << "verify_ok=" << numFlag( "verify_ok" ) << "\n";
                std::cout << "rollback_ok=" << numFlag( "rollback_ok" ) << "\n";
                std::cout << "cleanup_or_state_restored_ok=" << numFlag( "cleanup_or_state_restored_ok" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "simulation_directive_inspect" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                std::cout << "parse_ok=" << numFlag( "parse_ok" ) << "\n";
                std::cout << "simulation_type=" << strField( "simulation_type" ) << "\n";
                std::cout << "normalized_directive=" << strField( "normalized_directive" ) << "\n";
                std::string diagnostics = "absent";
                auto        itDiag = fields.find( "diagnostics" );
                if( itDiag != fields.end() && itDiag->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    diagnostics.clear();
                    const auto& vals = itDiag->second.list_value().values();
                    for( int i = 0; i < vals.size(); ++i )
                    {
                        if( i > 0 )
                            diagnostics += ";";
                        diagnostics += vals[i].kind_case() == google::protobuf::Value::kStringValue
                                               ? vals[i].string_value()
                                               : "non_string";
                    }
                    if( diagnostics.empty() )
                        diagnostics = "empty";
                }
                std::cout << "diagnostics_present="
                          << ( fields.find( "diagnostics" ) != fields.end() ? "true" : "false" ) << "\n";
                std::cout << "diagnostics=" << diagnostics << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "simulation_netlist_preview_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                std::string diagnostics = "absent";
                auto        itDiag = fields.find( "diagnostics" );
                if( itDiag != fields.end() && itDiag->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    diagnostics.clear();
                    const auto& vals = itDiag->second.list_value().values();
                    for( int i = 0; i < vals.size(); ++i )
                    {
                        if( i > 0 )
                            diagnostics += ";";
                        diagnostics += vals[i].kind_case() == google::protobuf::Value::kStringValue
                                               ? vals[i].string_value()
                                               : "non_string";
                    }
                    if( diagnostics.empty() )
                        diagnostics = "empty";
                }

                const std::string netlistText = strField( "netlist_text" );
                std::cout << "preview_ok=" << numFlag( "preview_ok" ) << "\n";
                std::cout << "netlist_text_present="
                          << ( !netlistText.empty() && netlistText != "absent" ? "true" : "false" ) << "\n";
                std::cout << "netlist_line_count=" << numFlag( "netlist_line_count" ) << "\n";
                std::cout << "diagnostics_present="
                          << ( fields.find( "diagnostics" ) != fields.end() ? "true" : "false" ) << "\n";
                std::cout << "diagnostics=" << diagnostics << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "simulation_model_library_inspect" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                std::string diagnostics = "absent";
                auto        itDiag = fields.find( "diagnostics" );
                if( itDiag != fields.end() && itDiag->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    diagnostics.clear();
                    const auto& vals = itDiag->second.list_value().values();
                    for( int i = 0; i < vals.size(); ++i )
                    {
                        if( i > 0 )
                            diagnostics += ";";
                        diagnostics += vals[i].kind_case() == google::protobuf::Value::kStringValue
                                               ? vals[i].string_value()
                                               : "non_string";
                    }
                    if( diagnostics.empty() )
                        diagnostics = "empty";
                }

                const std::string libraryKind = strField( "library_kind" );
                std::cout << "inspect_ok=" << numFlag( "inspect_ok" ) << "\n";
                std::cout << "library_kind=" << libraryKind << "\n";
                std::cout << "library_kind_valid="
                          << ( libraryKind == "model_only" || libraryKind == "subckt_only" || libraryKind == "mixed"
                                       ? "true"
                                       : "false" )
                          << "\n";
                std::cout << "models_count=" << numFlag( "models_count" ) << "\n";
                std::cout << "models_summary=" << strField( "models_summary" ) << "\n";
                std::cout << "diagnostics_present="
                          << ( fields.find( "diagnostics" ) != fields.end() ? "true" : "false" ) << "\n";
                std::cout << "diagnostics=" << diagnostics << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "simulation_preflight_readiness_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                auto listString = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kListValue )
                        return "absent";
                    std::string outList;
                    const auto& vals = it->second.list_value().values();
                    for( int i = 0; i < vals.size(); ++i )
                    {
                        if( i > 0 )
                            outList += ";";
                        outList += vals[i].kind_case() == google::protobuf::Value::kStringValue
                                           ? vals[i].string_value()
                                           : "non_string";
                    }
                    return outList;
                };

                std::cout << "preflight_decision_taken_ok=" << numFlag( "preflight_decision_taken_ok" ) << "\n";
                std::cout << "preflight_decision=" << strField( "preflight_decision" ) << "\n";
                std::cout << "ready_checks_present="
                          << ( fields.find( "ready_checks" ) != fields.end() ? "true" : "false" ) << "\n";
                std::cout << "ready_checks=" << listString( "ready_checks" ) << "\n";
                std::cout << "blocking_checks_present="
                          << ( fields.find( "blocking_checks" ) != fields.end() ? "true" : "false" ) << "\n";
                std::cout << "blocking_checks=" << listString( "blocking_checks" ) << "\n";
                std::cout << "diagnostics_present="
                          << ( fields.find( "diagnostics" ) != fields.end() ? "true" : "false" ) << "\n";
                std::cout << "diagnostics=" << listString( "diagnostics" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "simulation_run_status_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                auto listString = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kListValue )
                        return "absent";

                    std::string outList;
                    const auto& vals = it->second.list_value().values();
                    for( int i = 0; i < vals.size(); ++i )
                    {
                        if( i > 0 )
                            outList += ";";
                        outList += vals[i].kind_case() == google::protobuf::Value::kStringValue
                                           ? vals[i].string_value()
                                           : "non_string";
                    }
                    return outList;
                };

                std::cout << "run_decision_taken_ok=" << numFlag( "run_decision_taken_ok" ) << "\n";
                std::cout << "run_status=" << strField( "run_status" ) << "\n";
                std::cout << "started_ok=" << numFlag( "started_ok" ) << "\n";
                std::cout << "finished_ok=" << numFlag( "finished_ok" ) << "\n";
                std::cout << "duration_ms=" << numFlag( "duration_ms" ) << "\n";
                std::cout << "diagnostics_present="
                          << ( fields.find( "diagnostics" ) != fields.end() ? "true" : "false" ) << "\n";
                std::cout << "diagnostics=" << listString( "diagnostics" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "simulation_run_minimal_summary_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                auto listString = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kListValue )
                        return "absent";

                    std::string outList;
                    const auto& vals = it->second.list_value().values();
                    for( int i = 0; i < vals.size(); ++i )
                    {
                        if( i > 0 )
                            outList += ";";
                        outList += vals[i].kind_case() == google::protobuf::Value::kStringValue
                                           ? vals[i].string_value()
                                           : "non_string";
                    }
                    return outList;
                };

                std::cout << "run_summary_ok=" << numFlag( "run_summary_ok" ) << "\n";
                std::cout << "run_status=" << strField( "run_status" ) << "\n";
                std::cout << "plot_name=" << strField( "plot_name" ) << "\n";
                std::cout << "vectors_count=" << numFlag( "vectors_count" ) << "\n";
                std::cout << "duration_ms=" << numFlag( "duration_ms" ) << "\n";
                std::cout << "diagnostics_present="
                          << ( fields.find( "diagnostics" ) != fields.end() ? "true" : "false" ) << "\n";
                std::cout << "diagnostics=" << listString( "diagnostics" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "simulation_run_vector_catalog_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                auto listString = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kListValue )
                        return "absent";

                    std::string outList;
                    const auto& vals = it->second.list_value().values();
                    for( int i = 0; i < vals.size(); ++i )
                    {
                        if( i > 0 )
                            outList += ";";
                        outList += vals[i].kind_case() == google::protobuf::Value::kStringValue
                                           ? vals[i].string_value()
                                           : "non_string";
                    }
                    return outList;
                };

                std::cout << "catalog_ok=" << numFlag( "catalog_ok" ) << "\n";
                std::cout << "run_status=" << strField( "run_status" ) << "\n";
                std::cout << "plot_name=" << strField( "plot_name" ) << "\n";
                std::cout << "vectors_count=" << numFlag( "vectors_count" ) << "\n";
                std::cout << "vector_names_present="
                          << ( fields.find( "vector_names" ) != fields.end() ? "true" : "false" ) << "\n";
                std::cout << "vector_names=" << listString( "vector_names" ) << "\n";
                std::cout << "duration_ms=" << numFlag( "duration_ms" ) << "\n";
                std::cout << "diagnostics_present="
                          << ( fields.find( "diagnostics" ) != fields.end() ? "true" : "false" ) << "\n";
                std::cout << "diagnostics=" << listString( "diagnostics" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "simulation_vector_scalar_probe_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto numValue = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( it->second.number_value() );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                auto listString = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kListValue )
                        return "absent";

                    std::string outList;
                    const auto& vals = it->second.list_value().values();
                    for( int i = 0; i < vals.size(); ++i )
                    {
                        if( i > 0 )
                            outList += ";";
                        outList += vals[i].kind_case() == google::protobuf::Value::kStringValue
                                           ? vals[i].string_value()
                                           : "non_string";
                    }
                    return outList;
                };

                std::cout << "probe_ok=" << numFlag( "probe_ok" ) << "\n";
                std::cout << "run_status=" << strField( "run_status" ) << "\n";
                std::cout << "plot_name=" << strField( "plot_name" ) << "\n";
                std::cout << "vector_name=" << strField( "vector_name" ) << "\n";
                std::cout << "sample_position_used=" << strField( "sample_position_used" ) << "\n";
                std::cout << "scalar_value=" << numValue( "scalar_value" ) << "\n";
                std::cout << "duration_ms=" << numFlag( "duration_ms" ) << "\n";
                std::cout << "diagnostics_present="
                          << ( fields.find( "diagnostics" ) != fields.end() ? "true" : "false" ) << "\n";
                std::cout << "diagnostics=" << listString( "diagnostics" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "simulation_xaxis_probe_session" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                auto numValue = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( it->second.number_value() );
                };

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        return "absent";
                    return it->second.string_value();
                };

                auto listString = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kListValue )
                        return "absent";

                    std::string outList;
                    const auto& vals = it->second.list_value().values();
                    for( int i = 0; i < vals.size(); ++i )
                    {
                        if( i > 0 )
                            outList += ";";
                        outList += vals[i].kind_case() == google::protobuf::Value::kStringValue
                                           ? vals[i].string_value()
                                           : "non_string";
                    }
                    return outList;
                };

                std::cout << "axis_ok=" << numFlag( "axis_ok" ) << "\n";
                std::cout << "run_status=" << strField( "run_status" ) << "\n";
                std::cout << "plot_name=" << strField( "plot_name" ) << "\n";
                std::cout << "x_axis_name=" << strField( "x_axis_name" ) << "\n";
                std::cout << "x_axis_first_value=" << numValue( "x_axis_first_value" ) << "\n";
                std::cout << "duration_ms=" << numFlag( "duration_ms" ) << "\n";
                std::cout << "diagnostics_present="
                          << ( fields.find( "diagnostics" ) != fields.end() ? "true" : "false" ) << "\n";
                std::cout << "diagnostics=" << listString( "diagnostics" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";
                std::cout << "session_log=" << strField( "session_log" ) << "\n";
            }
            else if( req.name() == "get_pcb_drc_violations_context_session" )
            {
                google::protobuf::Struct trimmed;
                trimmed.CopyFrom( out.result() );
                auto* fields = trimmed.mutable_fields();
                auto  itList = fields->find( "violations" );
                if( itList != fields->end() && itList->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    const int n = itList->second.list_value().values_size();
                    google::protobuf::Value placeholder;
                    placeholder.set_string_value(
                            std::string( "omitted_violations_entries(n=" ) + std::to_string( n ) + ")" );
                    ( *fields )["violations"] = std::move( placeholder );
                }
                std::cout << trimmed.Utf8DebugString() << "\n";

                auto strField = [&]( const char* aKey ) -> std::string
                {
                    auto it = out.result().fields().find( aKey );
                    if( it == out.result().fields().end() )
                        return "absent";
                    if( it->second.kind_case() == google::protobuf::Value::kStringValue )
                        return it->second.string_value();
                    return "absent";
                };
                auto boolField = [&]( const char* aKey ) -> std::string
                {
                    auto it = out.result().fields().find( aKey );
                    if( it == out.result().fields().end() )
                        return "absent";
                    if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                        return it->second.bool_value() ? "true" : "false";
                    return "absent";
                };
                auto numField = [&]( const char* aKey ) -> std::string
                {
                    auto it = out.result().fields().find( aKey );
                    if( it == out.result().fields().end()
                            || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int64_t>( it->second.number_value() ) );
                };

                std::cout << "drc_ok=" << boolField( "ok" ) << "\n";
                std::cout << "is_official_drc=" << boolField( "is_official_drc" ) << "\n";
                std::cout << "drc_was_run=" << boolField( "drc_was_run" ) << "\n";
                std::cout << "drc_source=" << strField( "drc_source" ) << "\n";
                std::cout << "violations_count=" << numField( "violations_count" ) << "\n";
                std::cout << "returned_count=" << numField( "returned_count" ) << "\n";
                std::cout << "truncated=" << boolField( "truncated" ) << "\n";
                std::cout << "exact_blocker_if_any=" << strField( "exact_blocker_if_any" ) << "\n";

                auto itViolations = out.result().fields().find( "violations" );
                if( itViolations != out.result().fields().end()
                        && itViolations->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    std::set<std::string> types;
                    std::set<std::string> nets;
                    std::string           firstType;

                    for( const google::protobuf::Value& rowVal :
                         itViolations->second.list_value().values() )
                    {
                        if( rowVal.kind_case() != google::protobuf::Value::kStructValue )
                            continue;

                        const google::protobuf::Struct& row = rowVal.struct_value();
                        auto                            itType = row.fields().find( "type" );

                        if( itType != row.fields().end()
                                && itType->second.kind_case() == google::protobuf::Value::kStringValue )
                        {
                            const std::string t = itType->second.string_value();
                            types.insert( t );

                            if( firstType.empty() )
                                firstType = t;
                        }

                        auto itNets = row.fields().find( "net_names" );

                        if( itNets != row.fields().end()
                                && itNets->second.kind_case() == google::protobuf::Value::kListValue )
                        {
                            for( const google::protobuf::Value& netVal :
                                 itNets->second.list_value().values() )
                            {
                                if( netVal.kind_case() == google::protobuf::Value::kStringValue )
                                    nets.insert( netVal.string_value() );
                            }
                        }
                    }

                    auto joinSet = []( const std::set<std::string>& aSet ) -> std::string
                    {
                        std::string outStr;
                        for( const std::string& s : aSet )
                        {
                            if( !outStr.empty() )
                                outStr += ",";

                            outStr += s;
                        }
                        return outStr.empty() ? "none" : outStr;
                    };

                    std::cout << "first_violation_type="
                              << ( firstType.empty() ? "none" : firstType ) << "\n";
                    std::cout << "drc_violation_types=" << joinSet( types ) << "\n";
                    std::cout << "drc_violation_net_names=" << joinSet( nets ) << "\n";
                }
                else
                {
                    std::cout << "first_violation_type=absent\n";
                    std::cout << "drc_violation_types=absent\n";
                    std::cout << "drc_violation_net_names=absent\n";
                }
            }
            else if( req.name() == "get_project_context_snapshot" )
            {
                const auto fields = out.result().fields();

                auto numFlag = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        return "absent";
                    return std::to_string( static_cast<int>( it->second.number_value() ) );
                };

                std::cout << "project_info_present=" << numFlag( "project_info_present" ) << "\n";
                std::cout << "workspace_present=" << numFlag( "workspace_present" ) << "\n";
                std::cout << "open_documents_present=" << numFlag( "open_documents_present" ) << "\n";
                std::cout << "net_classes_present=" << numFlag( "net_classes_present" ) << "\n";
                std::cout << "nets_present=" << numFlag( "nets_present" ) << "\n";
                std::cout << "snapshot_summary_present=" << numFlag( "snapshot_summary_present" ) << "\n";

                auto itSum = fields.find( "snapshot_summary" );
                if( itSum != fields.end() && itSum->second.kind_case() == google::protobuf::Value::kStructValue )
                {
                    const google::protobuf::Struct& sum = itSum->second.struct_value();
                    auto sumBool = [&]( const char* aKey ) -> std::string
                    {
                        auto it = sum.fields().find( aKey );
                        if( it == sum.fields().end() )
                            return "absent";
                        if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                            return it->second.bool_value() ? "true" : "false";
                        return "absent";
                    };
                    auto sumNum = [&]( const char* aKey ) -> std::string
                    {
                        auto it = sum.fields().find( aKey );
                        if( it == sum.fields().end()
                                || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                            return "absent";
                        return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                    };

                    std::cout << "snapshot_project_loaded=" << sumBool( "project_loaded" ) << "\n";
                    std::cout << "snapshot_open_schematic_documents_count="
                              << sumNum( "open_schematic_documents_count" ) << "\n";
                    std::cout << "snapshot_open_pcb_documents_count=" << sumNum( "open_pcb_documents_count" )
                              << "\n";
                    std::cout << "snapshot_net_classes_count=" << sumNum( "net_classes_count" ) << "\n";
                    std::cout << "snapshot_nets_entry_count=" << sumNum( "nets_entry_count" ) << "\n";
                    auto sumStr = [&]( const char* aKey ) -> std::string
                    {
                        auto it = sum.fields().find( aKey );
                        if( it == sum.fields().end()
                                || it->second.kind_case() != google::protobuf::Value::kStringValue )
                            return "absent";
                        return it->second.string_value();
                    };
                    std::cout << "snapshot_open_pcb_board_absolute_path_present="
                              << sumBool( "open_pcb_board_absolute_path_present" ) << "\n";
                    std::cout << "snapshot_open_pcb_board_absolute_path="
                              << sumStr( "open_pcb_board_absolute_path" ) << "\n";
                    std::string absPathsCsv;
                    auto        itAbsPaths = sum.fields().find( "open_pcb_board_absolute_paths" );
                    if( itAbsPaths != sum.fields().end()
                        && itAbsPaths->second.kind_case() == google::protobuf::Value::kListValue )
                    {
                        for( const google::protobuf::Value& pv :
                             itAbsPaths->second.list_value().values() )
                        {
                            if( pv.kind_case() != google::protobuf::Value::kStringValue )
                                continue;
                            if( !absPathsCsv.empty() )
                                absPathsCsv += ";";
                            absPathsCsv += pv.string_value();
                        }
                    }
                    std::cout << "snapshot_open_pcb_board_absolute_paths="
                              << ( absPathsCsv.empty() ? "absent" : absPathsCsv ) << "\n";
                }

                auto itOpenDocs = fields.find( "open_documents" );
                if( itOpenDocs != fields.end()
                    && itOpenDocs->second.kind_case() == google::protobuf::Value::kStructValue )
                {
                    const google::protobuf::Struct& openDocs = itOpenDocs->second.struct_value();
                    auto                            itPcbList = openDocs.fields().find( "open_pcb_documents" );
                    int                             pcbDocCount = 0;
                    std::string                     pcb0Abs;
                    std::string                     pcb0BoardAbs;

                    if( itPcbList != openDocs.fields().end()
                        && itPcbList->second.kind_case() == google::protobuf::Value::kListValue )
                    {
                        pcbDocCount = itPcbList->second.list_value().values_size();

                        if( pcbDocCount > 0 )
                        {
                            const google::protobuf::Value& firstDoc =
                                    itPcbList->second.list_value().values( 0 );

                            if( firstDoc.kind_case() == google::protobuf::Value::kStructValue )
                            {
                                const google::protobuf::Struct& doc = firstDoc.struct_value();
                                auto                            itAbs = doc.fields().find( "absolute_path" );
                                if( itAbs != doc.fields().end()
                                    && itAbs->second.kind_case()
                                               == google::protobuf::Value::kStringValue )
                                {
                                    pcb0Abs = itAbs->second.string_value();
                                }

                                auto itBoardAbs = doc.fields().find( "board_absolute_path" );
                                if( itBoardAbs != doc.fields().end()
                                    && itBoardAbs->second.kind_case()
                                               == google::protobuf::Value::kStringValue )
                                {
                                    pcb0BoardAbs = itBoardAbs->second.string_value();
                                }
                            }
                        }
                    }

                    std::cout << "open_documents_pcb_count=" << pcbDocCount << "\n";
                    std::cout << "open_documents_pcb0_absolute_path="
                              << ( pcb0Abs.empty() ? "absent" : pcb0Abs ) << "\n";
                    std::cout << "open_documents_pcb0_board_absolute_path="
                              << ( pcb0BoardAbs.empty() ? "absent" : pcb0BoardAbs ) << "\n";
                }

                std::cout << "drc_summary_present=" << numFlag( "drc_summary_present" ) << "\n";

                auto itDrc = fields.find( "drc_summary" );
                if( itDrc != fields.end() && itDrc->second.kind_case() == google::protobuf::Value::kStructValue )
                {
                    const google::protobuf::Struct& drc = itDrc->second.struct_value();
                    auto drcBool = [&]( const char* aKey ) -> std::string
                    {
                        auto it = drc.fields().find( aKey );
                        if( it == drc.fields().end() )
                            return "absent";
                        if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                            return it->second.bool_value() ? "true" : "false";
                        return "absent";
                    };
                    auto drcStr = [&]( const char* aKey ) -> std::string
                    {
                        auto it = drc.fields().find( aKey );
                        if( it == drc.fields().end()
                                || it->second.kind_case() != google::protobuf::Value::kStringValue )
                            return "absent";
                        return it->second.string_value();
                    };
                    auto drcNum = [&]( const char* aKey ) -> std::string
                    {
                        auto it = drc.fields().find( aKey );
                        if( it == drc.fields().end()
                                || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                            return "absent";
                        return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                    };

                    std::cout << "drc_summary_available=" << drcBool( "available" ) << "\n";
                    std::cout << "drc_summary_requested=" << drcBool( "requested" ) << "\n";
                    std::cout << "drc_summary_run_mode=" << drcStr( "run_mode" ) << "\n";
                    std::cout << "drc_summary_is_official_drc=" << drcBool( "is_official_drc" ) << "\n";
                    std::cout << "drc_summary_drc_was_run=" << drcBool( "drc_was_run" ) << "\n";
                    std::cout << "drc_summary_drc_source=" << drcStr( "drc_source" ) << "\n";
                    std::cout << "drc_summary_violations_count=" << drcNum( "violations_count" ) << "\n";
                    std::cout << "drc_summary_returned_count=" << drcNum( "returned_count" ) << "\n";
                    std::cout << "drc_summary_truncated=" << drcBool( "truncated" ) << "\n";
                    std::cout << "drc_summary_has_clearance_violations="
                              << drcBool( "has_clearance_violations" ) << "\n";
                    std::cout << "drc_summary_exact_blocker_if_any=" << drcStr( "exact_blocker_if_any" )
                              << "\n";

                    std::string typesCsv;
                    auto        itTypes = drc.fields().find( "types" );
                    if( itTypes != drc.fields().end()
                        && itTypes->second.kind_case() == google::protobuf::Value::kListValue )
                    {
                        for( const google::protobuf::Value& tv : itTypes->second.list_value().values() )
                        {
                            if( tv.kind_case() != google::protobuf::Value::kStringValue )
                                continue;
                            if( !typesCsv.empty() )
                                typesCsv += ",";
                            typesCsv += tv.string_value();
                        }
                    }
                    std::cout << "drc_summary_types=" << ( typesCsv.empty() ? "absent" : typesCsv ) << "\n";
                }

                std::cout << "project_readiness_summary_present="
                          << numFlag( "project_readiness_summary_present" ) << "\n";

                auto itReadiness = fields.find( "project_readiness_summary" );
                if( itReadiness != fields.end()
                    && itReadiness->second.kind_case() == google::protobuf::Value::kStructValue )
                {
                    const google::protobuf::Struct& readiness = itReadiness->second.struct_value();
                    auto readinessBool = [&]( const char* aKey ) -> std::string
                    {
                        auto it = readiness.fields().find( aKey );
                        if( it == readiness.fields().end() )
                            return "absent";
                        if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                            return it->second.bool_value() ? "true" : "false";
                        return "absent";
                    };
                    auto readinessStr = [&]( const char* aKey ) -> std::string
                    {
                        auto it = readiness.fields().find( aKey );
                        if( it == readiness.fields().end() )
                            return "absent";
                        if( it->second.kind_case() == google::protobuf::Value::kStringValue )
                            return it->second.string_value();
                        return "absent";
                    };
                    auto readinessNullableBool = [&]( const char* aKey ) -> std::string
                    {
                        auto it = readiness.fields().find( aKey );
                        if( it == readiness.fields().end() )
                            return "absent";
                        if( it->second.kind_case() == google::protobuf::Value::kNullValue )
                            return "null";
                        if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                            return it->second.bool_value() ? "true" : "false";
                        return "absent";
                    };
                    auto readinessListCsv = [&]( const char* aKey ) -> std::string
                    {
                        auto it = readiness.fields().find( aKey );
                        if( it == readiness.fields().end()
                            || it->second.kind_case() != google::protobuf::Value::kListValue )
                        {
                            return "absent";
                        }

                        std::string csv;
                        for( const google::protobuf::Value& lv : it->second.list_value().values() )
                        {
                            if( lv.kind_case() != google::protobuf::Value::kStringValue )
                                continue;
                            if( !csv.empty() )
                                csv += ",";
                            csv += lv.string_value();
                        }
                        return csv.empty() ? "absent" : csv;
                    };

                    std::cout << "readiness_available=" << readinessBool( "available" ) << "\n";
                    std::cout << "readiness_project_loaded=" << readinessBool( "project_loaded" ) << "\n";
                    std::cout << "readiness_schematic_open=" << readinessBool( "schematic_open" ) << "\n";
                    std::cout << "readiness_pcb_open=" << readinessBool( "pcb_open" ) << "\n";
                    std::cout << "readiness_pcb_absolute_path_present="
                              << readinessBool( "pcb_absolute_path_present" ) << "\n";
                    std::cout << "readiness_drc_available=" << readinessBool( "drc_available" ) << "\n";
                    std::cout << "readiness_drc_summary_present="
                              << readinessBool( "drc_summary_present" ) << "\n";
                    std::cout << "readiness_drc_has_violations="
                              << readinessNullableBool( "drc_has_violations" ) << "\n";
                    std::cout << "readiness_drc_has_clearance_violations="
                              << readinessNullableBool( "drc_has_clearance_violations" ) << "\n";
                    std::cout << "readiness_footprints_summary_available="
                              << readinessBool( "footprints_summary_available" ) << "\n";
                    auto readinessNullableNum = [&]( const char* aKey ) -> std::string
                    {
                        auto it = readiness.fields().find( aKey );
                        if( it == readiness.fields().end() )
                            return "absent";
                        if( it->second.kind_case() == google::protobuf::Value::kNullValue )
                            return "null";
                        if( it->second.kind_case() == google::protobuf::Value::kNumberValue )
                            return std::to_string(
                                    static_cast<std::int64_t>( it->second.number_value() ) );
                        if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                            return it->second.bool_value() ? "true" : "false";
                        return "absent";
                    };
                    std::cout << "readiness_symbols_count="
                              << readinessNullableNum( "symbols_count" ) << "\n";
                    std::cout << "readiness_symbols_without_footprint_count="
                              << readinessNullableNum( "symbols_without_footprint_count" ) << "\n";
                    std::cout << "readiness_sch_to_pcb_ready="
                              << readinessNullableNum( "sch_to_pcb_ready" ) << "\n";
                    std::cout << "readiness_symbols_with_invalid_footprint_count="
                              << readinessNullableNum( "symbols_with_invalid_footprint_count" )
                              << "\n";
                    std::cout << "readiness_sch_to_pcb_ready_validated="
                              << readinessNullableNum( "sch_to_pcb_ready_validated" ) << "\n";
                    std::cout << "readiness_footprint_validation_performed="
                              << readinessBool( "footprint_validation_performed" ) << "\n";
                    std::cout << "readiness_footprints_scope="
                              << readinessStr( "footprints_scope" ) << "\n";
                    std::cout << "readiness_symbols_instances_count="
                              << readinessNullableNum( "symbols_instances_count" ) << "\n";
                    std::cout << "readiness_footprints_summary_diagnostic="
                              << readinessStr( "footprints_summary_diagnostic" ) << "\n";
                    std::cout << "readiness_exact_blocker_if_any="
                              << readinessStr( "exact_blocker_if_any" ) << "\n";
                    std::cout << "readiness_recommended_next_actions="
                              << readinessListCsv( "recommended_next_actions" ) << "\n";
                    std::cout << "readiness_recommended_tools="
                              << readinessListCsv( "recommended_tools" ) << "\n";
                    std::cout << "readiness_hierarchy_assign_instance_safe_enabled="
                              << readinessBool( "hierarchy_assign_instance_safe_enabled" ) << "\n";
                    std::cout << "readiness_recommended_footprint_assignment_workflow="
                              << readinessStr( "recommended_footprint_assignment_workflow" ) << "\n";
                    std::cout << "readiness_fallback_footprint_assignment_workflow="
                              << readinessStr( "fallback_footprint_assignment_workflow" ) << "\n";
                    std::cout << "readiness_recommended_assign_tool="
                              << readinessStr( "recommended_assign_tool" ) << "\n";
                    std::cout << "readiness_recommended_assign_scope="
                              << readinessStr( "recommended_assign_scope" ) << "\n";
                }

                std::cout << "footprints_summary_present=" << numFlag( "footprints_summary_present" ) << "\n";

                auto itFpSum = fields.find( "footprints_summary" );
                if( itFpSum != fields.end()
                    && itFpSum->second.kind_case() == google::protobuf::Value::kStructValue )
                {
                    const google::protobuf::Struct& fpSum = itFpSum->second.struct_value();
                    auto fpSumBool = [&]( const char* aKey ) -> std::string
                    {
                        auto it = fpSum.fields().find( aKey );
                        if( it == fpSum.fields().end() )
                            return "absent";
                        if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                            return it->second.bool_value() ? "true" : "false";
                        return "absent";
                    };
                    auto fpSumNum = [&]( const char* aKey ) -> std::string
                    {
                        auto it = fpSum.fields().find( aKey );
                        if( it == fpSum.fields().end()
                                || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        {
                            return "absent";
                        }
                        return std::to_string(
                                static_cast<std::int64_t>( it->second.number_value() ) );
                    };
                    auto fpSumStr = [&]( const char* aKey ) -> std::string
                    {
                        auto it = fpSum.fields().find( aKey );
                        if( it == fpSum.fields().end()
                                || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        {
                            return "absent";
                        }
                        return it->second.string_value();
                    };
                    std::cout << "footprints_summary_available=" << fpSumBool( "available" ) << "\n";
                    std::cout << "footprints_summary_scope=" << fpSumStr( "scope" ) << "\n";
                    std::cout << "footprints_summary_symbols_count=" << fpSumNum( "symbols_count" )
                              << "\n";
                    std::cout << "footprints_summary_symbols_instances_count="
                              << fpSumNum( "symbols_instances_count" ) << "\n";
                    std::cout << "footprints_summary_symbols_without_footprint_count="
                              << fpSumNum( "symbols_without_footprint_count" ) << "\n";
                    std::cout << "footprints_summary_sch_to_pcb_ready="
                              << fpSumBool( "sch_to_pcb_ready" ) << "\n";
                    auto fpSumNullableNum = [&]( const char* aKey ) -> std::string
                    {
                        auto it = fpSum.fields().find( aKey );
                        if( it == fpSum.fields().end() )
                            return "absent";
                        if( it->second.kind_case() == google::protobuf::Value::kNullValue )
                            return "null";
                        if( it->second.kind_case() == google::protobuf::Value::kNumberValue )
                            return std::to_string(
                                    static_cast<std::int64_t>( it->second.number_value() ) );
                        if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                            return it->second.bool_value() ? "true" : "false";
                        return "absent";
                    };
                    std::cout << "footprints_summary_symbols_with_invalid_footprint_count="
                              << fpSumNullableNum( "symbols_with_invalid_footprint_count" ) << "\n";
                    std::cout << "footprints_summary_sch_to_pcb_ready_validated="
                              << fpSumNullableNum( "sch_to_pcb_ready_validated" ) << "\n";
                }

                std::cout << "snapshot_pcb_sync_summary_present="
                          << numFlag( "pcb_sync_summary_present" ) << "\n";

                auto itPcbSync = fields.find( "pcb_sync_summary" );
                if( itPcbSync != fields.end()
                    && itPcbSync->second.kind_case() == google::protobuf::Value::kStructValue )
                {
                    const google::protobuf::Struct& pcbSync = itPcbSync->second.struct_value();
                    auto syncBool = [&]( const char* aKey ) -> std::string
                    {
                        auto it = pcbSync.fields().find( aKey );
                        if( it == pcbSync.fields().end() )
                            return "absent";
                        if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                            return it->second.bool_value() ? "true" : "false";
                        return "absent";
                    };
                    auto syncStr = [&]( const char* aKey ) -> std::string
                    {
                        auto it = pcbSync.fields().find( aKey );
                        if( it == pcbSync.fields().end()
                                || it->second.kind_case() != google::protobuf::Value::kStringValue )
                        {
                            return "absent";
                        }
                        return it->second.string_value();
                    };
                    auto syncNum = [&]( const char* aKey ) -> std::string
                    {
                        auto it = pcbSync.fields().find( aKey );
                        if( it == pcbSync.fields().end()
                                || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                        {
                            return "absent";
                        }
                        return std::to_string(
                                static_cast<std::int64_t>( it->second.number_value() ) );
                    };
                    auto syncNullableBool = [&]( const char* aKey ) -> std::string
                    {
                        auto it = pcbSync.fields().find( aKey );
                        if( it == pcbSync.fields().end() )
                            return "absent";
                        if( it->second.kind_case() == google::protobuf::Value::kNullValue )
                            return "null";
                        if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                            return it->second.bool_value() ? "true" : "false";
                        return "absent";
                    };
                    auto syncNullableNum = [&]( const char* aKey ) -> std::string
                    {
                        auto it = pcbSync.fields().find( aKey );
                        if( it == pcbSync.fields().end() )
                            return "absent";
                        if( it->second.kind_case() == google::protobuf::Value::kNullValue )
                            return "null";
                        if( it->second.kind_case() == google::protobuf::Value::kNumberValue )
                            return std::to_string(
                                    static_cast<std::int64_t>( it->second.number_value() ) );
                        return "absent";
                    };

                    std::cout << "snapshot_pcb_sync_likely=" << syncBool( "pcb_sync_likely" ) << "\n";
                    std::cout << "snapshot_needs_update_from_schematic="
                              << syncBool( "needs_update_from_schematic" ) << "\n";
                    std::cout << "snapshot_pcb_sync_missing_footprints_count="
                              << syncNum( "missing_footprints_count" ) << "\n";
                    std::cout << "snapshot_pcb_sync_orphan_footprints_count="
                              << syncNum( "orphan_footprints_count" ) << "\n";
                    std::cout << "snapshot_pcb_sync_preview_available="
                              << syncBool( "preview_available" ) << "\n";
                    std::cout << "snapshot_pcb_update_needed=" << syncNullableBool( "pcb_update_needed" )
                              << "\n";
                    std::cout << "snapshot_pcb_sync_would_add_footprints_count="
                              << syncNullableNum( "would_add_footprints_count" ) << "\n";
                    std::cout << "snapshot_pcb_sync_recommended_next_tool="
                              << syncStr( "recommended_next_tool" ) << "\n";
                    std::cout << "snapshot_pcb_sync_recommended_next_action="
                              << syncStr( "recommended_next_action" ) << "\n";
                    std::cout << "snapshot_pcb_sync_exact_blocker_if_any="
                              << syncStr( "exact_blocker_if_any" ) << "\n";
                }
            }
            else if( req.name() == "schematic_footprint_readiness_session" )
            {
                const auto fields = out.result().fields();
                auto fpBool = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() )
                        return "absent";
                    if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                        return it->second.bool_value() ? "true" : "false";
                    return "absent";
                };
                auto fpStr = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end()
                            || it->second.kind_case() != google::protobuf::Value::kStringValue )
                    {
                        return "absent";
                    }
                    return it->second.string_value();
                };
                auto fpNum = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end()
                            || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                    {
                        return "absent";
                    }
                    return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                };
                auto fpNullable = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() )
                        return "absent";
                    if( it->second.kind_case() == google::protobuf::Value::kNullValue )
                        return "null";
                    if( it->second.kind_case() == google::protobuf::Value::kNumberValue )
                        return std::to_string(
                                static_cast<std::int64_t>( it->second.number_value() ) );
                    if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                        return it->second.bool_value() ? "true" : "false";
                    return "absent";
                };

                std::cout << "footprint_readiness_ok=" << fpBool( "ok" ) << "\n";
                std::cout << "footprint_readiness_scope=" << fpStr( "scope" ) << "\n";
                std::cout << "footprint_readiness_schematic_filename="
                          << fpStr( "schematic_filename" ) << "\n";
                std::cout << "footprint_readiness_sheets_count=" << fpNum( "sheets_count" ) << "\n";
                std::cout << "footprint_readiness_symbols_count=" << fpNum( "symbols_count" ) << "\n";
                std::cout << "footprint_readiness_symbols_instances_count="
                          << fpNum( "symbols_instances_count" ) << "\n";
                std::cout << "footprint_readiness_symbols_with_footprint_count="
                          << fpNum( "symbols_with_footprint_count" ) << "\n";
                std::cout << "footprint_readiness_symbols_without_footprint_count="
                          << fpNum( "symbols_without_footprint_count" ) << "\n";
                std::cout << "footprint_readiness_sch_to_pcb_ready=" << fpBool( "sch_to_pcb_ready" )
                          << "\n";
                std::cout << "footprint_readiness_truncated=" << fpBool( "truncated" ) << "\n";
                std::cout << "footprint_readiness_validate_footprints="
                          << fpBool( "validate_footprints" ) << "\n";
                std::cout << "footprint_readiness_footprint_validation_available="
                          << fpBool( "footprint_validation_available" ) << "\n";
                std::cout << "footprint_readiness_footprint_validation_performed="
                          << fpBool( "footprint_validation_performed" ) << "\n";
                std::cout << "footprint_readiness_symbols_with_invalid_footprint_count="
                          << fpNullable( "symbols_with_invalid_footprint_count" ) << "\n";
                std::cout << "footprint_readiness_symbols_with_unvalidated_footprint_count="
                          << fpNullable( "symbols_with_unvalidated_footprint_count" ) << "\n";
                std::cout << "footprint_readiness_sch_to_pcb_ready_validated="
                          << fpNullable( "sch_to_pcb_ready_validated" ) << "\n";
                std::cout << "footprint_readiness_exact_blocker_if_any="
                          << fpStr( "exact_blocker_if_any" ) << "\n";
                std::cout << "footprint_readiness_diagnostics=" << fpStr( "diagnostics" ) << "\n";
                std::cout << "footprint_readiness_hierarchy_assign_instance_safe_enabled="
                          << fpBool( "hierarchy_assign_instance_safe_enabled" ) << "\n";
                std::cout << "footprint_readiness_recommended_footprint_assignment_workflow="
                          << fpStr( "recommended_footprint_assignment_workflow" ) << "\n";
                std::cout << "footprint_readiness_fallback_footprint_assignment_workflow="
                          << fpStr( "fallback_footprint_assignment_workflow" ) << "\n";
                std::cout << "footprint_readiness_recommended_assign_tool="
                          << fpStr( "recommended_assign_tool" ) << "\n";
                std::cout << "footprint_readiness_recommended_assign_scope="
                          << fpStr( "recommended_assign_scope" ) << "\n";

                int invalidFootprintsReturned = 0;
                auto  itInvalid = fields.find( "invalid_footprints" );
                if( itInvalid != fields.end()
                    && itInvalid->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    invalidFootprintsReturned = itInvalid->second.list_value().values_size();
                }
                std::cout << "footprint_readiness_invalid_footprints_returned="
                          << invalidFootprintsReturned << "\n";

                int symbolsReturned = 0;
                auto  itSymbols = fields.find( "symbols" );
                if( itSymbols != fields.end()
                    && itSymbols->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    symbolsReturned = itSymbols->second.list_value().values_size();
                }
                std::cout << "footprint_readiness_symbols_returned=" << symbolsReturned << "\n";

                std::string firstSheetPath = "absent";
                std::string firstInstancePath = "absent";
                if( itSymbols != fields.end()
                    && itSymbols->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    const auto& symVals = itSymbols->second.list_value().values();
                    if( symVals.size() > 0
                        && symVals.Get( 0 ).kind_case() == google::protobuf::Value::kStructValue )
                    {
                        const auto& symFields = symVals.Get( 0 ).struct_value().fields();
                        auto        itSheetPath = symFields.find( "sheet_path" );
                        if( itSheetPath != symFields.end()
                            && itSheetPath->second.kind_case()
                                       == google::protobuf::Value::kStringValue )
                        {
                            firstSheetPath = itSheetPath->second.string_value();
                        }
                        auto itInstancePath = symFields.find( "instance_path" );
                        if( itInstancePath != symFields.end()
                            && itInstancePath->second.kind_case()
                                       == google::protobuf::Value::kStringValue )
                        {
                            firstInstancePath = itInstancePath->second.string_value();
                        }
                    }
                }
                std::cout << "footprint_readiness_first_sheet_path=" << firstSheetPath << "\n";
                std::cout << "footprint_readiness_first_instance_path=" << firstInstancePath << "\n";
            }
            else if( req.name() == "get_pcb_sync_readiness_session" )
            {
                const auto fields = out.result().fields();
                auto       psrBool = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() )
                        return "absent";
                    if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                        return it->second.bool_value() ? "true" : "false";
                    return "absent";
                };
                auto psrStr = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end()
                            || it->second.kind_case() != google::protobuf::Value::kStringValue )
                    {
                        return "absent";
                    }
                    return it->second.string_value();
                };
                auto psrNum = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end()
                            || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                    {
                        return "absent";
                    }
                    return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                };

                std::cout << "pcb_sync_readiness_ok=" << psrBool( "ok" ) << "\n";
                std::cout << "pcb_sync_readiness_read_only=" << psrBool( "read_only" ) << "\n";
                std::cout << "pcb_sync_likely=" << psrBool( "pcb_sync_likely" ) << "\n";
                std::cout << "needs_update_from_schematic=" << psrBool( "needs_update_from_schematic" )
                          << "\n";
                std::cout << "sch_symbols_count=" << psrNum( "sch_symbols_count" ) << "\n";
                std::cout << "sch_symbols_with_footprint_count="
                          << psrNum( "sch_symbols_with_footprint_count" ) << "\n";
                std::cout << "pcb_footprints_count=" << psrNum( "pcb_footprints_count" ) << "\n";
                std::cout << "expected_footprints_count=" << psrNum( "expected_footprints_count" )
                          << "\n";
                std::cout << "matched_footprints_count=" << psrNum( "matched_footprints_count" )
                          << "\n";
                std::cout << "missing_footprints_count=" << psrNum( "missing_footprints_count" )
                          << "\n";
                std::cout << "orphan_footprints_count=" << psrNum( "orphan_footprints_count" )
                          << "\n";
                std::cout << "recommended_next_tool=" << psrStr( "recommended_next_tool" ) << "\n";
                std::cout << "recommended_next_action=" << psrStr( "recommended_next_action" )
                          << "\n";
                std::cout << "exact_blocker_if_any=" << psrStr( "exact_blocker_if_any" ) << "\n";
            }
            else if( req.name() == "pcb_update_from_schematic_preview_session" )
            {
                const auto fields = out.result().fields();
                auto       pupBool = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() )
                        return "absent";
                    if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                        return it->second.bool_value() ? "true" : "false";
                    return "absent";
                };
                auto pupStr = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end()
                            || it->second.kind_case() != google::protobuf::Value::kStringValue )
                    {
                        return "absent";
                    }
                    return it->second.string_value();
                };
                auto pupNum = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end()
                            || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                    {
                        return "absent";
                    }
                    return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                };

                std::cout << "pcb_update_preview_ok=" << pupBool( "ok" ) << "\n";
                std::cout << "dry_run=" << pupBool( "dry_run" ) << "\n";
                std::cout << "read_only=" << pupBool( "read_only" ) << "\n";
                std::cout << "pcb_update_needed=" << pupBool( "pcb_update_needed" ) << "\n";
                std::cout << "would_add_footprints_count=" << pupNum( "would_add_footprints_count" )
                          << "\n";
                std::cout << "would_remove_orphan_footprints_count="
                          << pupNum( "would_remove_orphan_footprints_count" ) << "\n";
                std::cout << "would_update_nets_count=" << pupNum( "would_update_nets_count" )
                          << "\n";
                std::cout << "would_replace_footprints_count="
                          << pupNum( "would_replace_footprints_count" ) << "\n";
                std::cout << "actions_count=" << pupNum( "actions_count" ) << "\n";
                std::cout << "returned_actions_count=" << pupNum( "returned_actions_count" )
                          << "\n";
                std::cout << "board_mutation_detected=" << pupBool( "board_mutation_detected" )
                          << "\n";
                std::cout << "recommended_next_action=" << pupStr( "recommended_next_action" )
                          << "\n";
                std::cout << "exact_blocker_if_any=" << pupStr( "exact_blocker_if_any" ) << "\n";
                std::cout << "pcb_update_preview_diagnostics=" << pupStr( "diagnostics" ) << "\n";
            }
            else if( req.name() == "pcb_update_from_schematic_session" )
            {
                const auto fields = out.result().fields();
                auto       puaBool = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() )
                        return "absent";
                    if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                        return it->second.bool_value() ? "true" : "false";
                    return "absent";
                };
                auto puaStr = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end()
                            || it->second.kind_case() != google::protobuf::Value::kStringValue )
                    {
                        return "absent";
                    }
                    return it->second.string_value();
                };
                auto puaNum = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end()
                            || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                    {
                        return "absent";
                    }
                    return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                };

                std::cout << "pcb_update_from_schematic_ok=" << puaBool( "ok" ) << "\n";
                std::cout << "applied=" << puaBool( "applied" ) << "\n";
                std::cout << "dry_run=" << puaBool( "dry_run" ) << "\n";
                std::cout << "preview_checked=" << puaBool( "preview_checked" ) << "\n";
                std::cout << "board_mutation_performed=" << puaBool( "board_mutation_performed" )
                          << "\n";
                std::cout << "footprints_before_count=" << puaNum( "footprints_before_count" )
                          << "\n";
                std::cout << "footprints_after_count=" << puaNum( "footprints_after_count" ) << "\n";
                std::cout << "added_footprints_count=" << puaNum( "added_footprints_count" ) << "\n";
                std::cout << "removed_orphan_footprints_count="
                          << puaNum( "removed_orphan_footprints_count" ) << "\n";
                std::cout << "updated_nets_count=" << puaNum( "updated_nets_count" ) << "\n";
                std::cout << "replaced_footprints_count=" << puaNum( "replaced_footprints_count" )
                          << "\n";
                std::cout << "delete_orphans=" << puaBool( "delete_orphans" ) << "\n";
                std::cout << "recommended_next_tool=" << puaStr( "recommended_next_tool" ) << "\n";
                std::cout << "recommended_next_action=" << puaStr( "recommended_next_action" )
                          << "\n";
                std::cout << "exact_blocker_if_any=" << puaStr( "exact_blocker_if_any" ) << "\n";
                std::cout << "pcb_update_from_schematic_diagnostics=" << puaStr( "diagnostics" )
                          << "\n";
                std::cout << "pcb_update_post_apply_drc_summary_present="
                          << puaBool( "post_apply_drc_summary_present" ) << "\n";
                std::cout << "pcb_update_post_apply_drc_blocker="
                          << puaStr( "post_apply_drc_blocker" ) << "\n";

                auto itPostDrc = fields.find( "post_apply_drc_summary" );
                if( itPostDrc != fields.end()
                    && itPostDrc->second.kind_case() == google::protobuf::Value::kStructValue )
                {
                    const auto& padrc = itPostDrc->second.struct_value().fields();
                    auto        padrcBool = [&]( const char* aKey ) -> std::string
                    {
                        auto it = padrc.find( aKey );
                        if( it == padrc.end() )
                            return "absent";
                        if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                            return it->second.bool_value() ? "true" : "false";
                        return "absent";
                    };
                    auto padrcNum = [&]( const char* aKey ) -> std::string
                    {
                        auto it = padrc.find( aKey );
                        if( it == padrc.end()
                                || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                            return "absent";
                        return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                    };

                    std::cout << "pcb_update_post_apply_drc_available=" << padrcBool( "available" )
                              << "\n";
                    std::cout << "pcb_update_post_apply_drc_violations_count="
                              << padrcNum( "violations_count" ) << "\n";
                }
                else
                {
                    std::cout << "pcb_update_post_apply_drc_available=absent\n";
                    std::cout << "pcb_update_post_apply_drc_violations_count=absent\n";
                }
            }
            else if( req.name() == "save_board_session" )
            {
                const auto fields = out.result().fields();
                auto       sbsBool = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() )
                        return "absent";
                    if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                        return it->second.bool_value() ? "true" : "false";
                    return "absent";
                };
                auto sbsStr = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end()
                            || it->second.kind_case() != google::protobuf::Value::kStringValue )
                    {
                        return "absent";
                    }
                    return it->second.string_value();
                };

                std::cout << "save_board_ok=" << sbsBool( "ok" ) << "\n";
                std::cout << "save_board_saved=" << sbsBool( "saved" ) << "\n";
                std::cout << "save_board_confirm_save=" << sbsBool( "confirm_save" ) << "\n";
                std::cout << "save_board_file_mtime_before=" << sbsStr( "file_mtime_before" )
                          << "\n";
                std::cout << "save_board_file_mtime_after=" << sbsStr( "file_mtime_after" ) << "\n";

                const std::string mtimeBefore = sbsStr( "file_mtime_before" );
                const std::string mtimeAfter = sbsStr( "file_mtime_after" );
                const std::string mtimeChanged =
                        ( mtimeBefore != "absent" && mtimeAfter != "absent"
                          && mtimeBefore != mtimeAfter )
                                ? "true"
                                : "false";
                std::cout << "save_board_file_mtime_changed=" << mtimeChanged << "\n";
                std::cout << "save_board_exact_blocker_if_any=" << sbsStr( "exact_blocker_if_any" )
                          << "\n";
            }
            else if( req.name() == "get_schematic_active_sheet_context_session" )
            {
                const auto fields = out.result().fields();
                auto       ascBool = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() )
                        return "absent";
                    if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                        return it->second.bool_value() ? "true" : "false";
                    return "absent";
                };
                auto ascStr = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end()
                            || it->second.kind_case() != google::protobuf::Value::kStringValue )
                    {
                        return "absent";
                    }
                    return it->second.string_value();
                };
                auto ascNum = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end()
                            || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                    {
                        return "absent";
                    }
                    return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                };

                std::cout << "active_sheet_context_ok=" << ascBool( "ok" ) << "\n";
                std::cout << "active_sheet_context_available=" << ascBool( "active_sheet_available" )
                          << "\n";
                std::cout << "active_sheet_context_schematic_filename="
                          << ascStr( "schematic_filename" ) << "\n";
                std::cout << "active_sheet_context_active_sheet_path="
                          << ascStr( "active_sheet_path" ) << "\n";
                std::cout << "active_sheet_context_active_sheet_file="
                          << ascStr( "active_sheet_file" ) << "\n";
                std::cout << "active_sheet_context_is_root=" << ascBool( "active_sheet_is_root" )
                          << "\n";
                std::cout << "active_sheet_context_is_shared=" << ascBool( "active_sheet_is_shared" )
                          << "\n";
                std::cout << "active_sheet_context_symbols_count=" << ascNum( "symbols_count" )
                          << "\n";
                std::cout << "active_sheet_context_truncated=" << ascBool( "truncated" ) << "\n";
                std::cout << "active_sheet_context_exact_blocker_if_any="
                          << ascStr( "exact_blocker_if_any" ) << "\n";
                std::cout << "active_sheet_context_diagnostics=" << ascStr( "diagnostics" ) << "\n";

                int symbolsReturned = 0;
                std::string firstSymbolId;
                auto        itSymbols = fields.find( "symbols" );
                if( itSymbols != fields.end()
                    && itSymbols->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    symbolsReturned = itSymbols->second.list_value().values_size();
                    if( symbolsReturned > 0
                        && itSymbols->second.list_value().values( 0 ).kind_case()
                                   == google::protobuf::Value::kStructValue )
                    {
                        const auto& symFields =
                                itSymbols->second.list_value().values( 0 ).struct_value().fields();
                        auto itSymId = symFields.find( "symbol_id" );
                        if( itSymId != symFields.end()
                            && itSymId->second.kind_case()
                                       == google::protobuf::Value::kStringValue )
                        {
                            firstSymbolId = itSymId->second.string_value();
                        }
                    }
                }
                std::cout << "active_sheet_context_symbols_returned=" << symbolsReturned << "\n";
                std::cout << "active_sheet_context_first_symbol_id=" << firstSymbolId << "\n";
            }
            else if( req.name() == "navigate_schematic_sheet_session" )
            {
                const auto fields = out.result().fields();
                auto       navBool = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() )
                        return "absent";
                    if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                        return it->second.bool_value() ? "true" : "false";
                    return "absent";
                };
                auto navStr = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end()
                            || it->second.kind_case() != google::protobuf::Value::kStringValue )
                    {
                        return "absent";
                    }
                    return it->second.string_value();
                };

                std::cout << "navigate_sheet_ok=" << navBool( "ok" ) << "\n";
                std::cout << "navigate_sheet_dry_run=" << navBool( "dry_run" ) << "\n";
                std::cout << "navigate_sheet_schematic_filename=" << navStr( "schematic_filename" )
                          << "\n";
                std::cout << "navigate_sheet_requested_sheet_path=" << navStr( "requested_sheet_path" )
                          << "\n";
                std::cout << "navigate_sheet_requested_instance_path="
                          << navStr( "requested_instance_path" ) << "\n";
                std::cout << "navigate_sheet_resolved_sheet_path=" << navStr( "resolved_sheet_path" )
                          << "\n";
                std::cout << "navigate_sheet_resolved_from=" << navStr( "resolved_from" ) << "\n";
                std::cout << "navigate_sheet_active_sheet_path_before="
                          << navStr( "active_sheet_path_before" ) << "\n";
                std::cout << "navigate_sheet_active_sheet_path_after="
                          << navStr( "active_sheet_path_after" ) << "\n";
                std::cout << "navigate_sheet_active_sheet_name=" << navStr( "active_sheet_name" )
                          << "\n";
                std::cout << "navigate_sheet_active_sheet_file=" << navStr( "active_sheet_file" )
                          << "\n";
                std::cout << "navigate_sheet_context_changed=" << navBool( "context_changed" ) << "\n";
                std::cout << "navigate_sheet_post_navigation_verified="
                          << navBool( "post_navigation_verified" ) << "\n";
                std::cout << "navigate_sheet_exact_blocker_if_any=" << navStr( "exact_blocker_if_any" )
                          << "\n";
                std::cout << "navigate_sheet_diagnostics=" << navStr( "diagnostics" ) << "\n";
            }
            else if( req.name() == "schematic_assign_footprint_session" )
            {
                const auto fields = out.result().fields();
                auto       afBool = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end() )
                        return "absent";
                    if( it->second.kind_case() == google::protobuf::Value::kBoolValue )
                        return it->second.bool_value() ? "true" : "false";
                    return "absent";
                };
                auto afStr = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end()
                            || it->second.kind_case() != google::protobuf::Value::kStringValue )
                    {
                        return "absent";
                    }
                    return it->second.string_value();
                };
                auto afNum = [&]( const char* aKey ) -> std::string
                {
                    auto it = fields.find( aKey );
                    if( it == fields.end()
                            || it->second.kind_case() != google::protobuf::Value::kNumberValue )
                    {
                        return "absent";
                    }
                    return std::to_string( static_cast<std::int64_t>( it->second.number_value() ) );
                };

                std::cout << "assign_footprint_ok=" << afBool( "ok" ) << "\n";
                std::cout << "assign_footprint_scope=" << afStr( "scope" ) << "\n";
                std::cout << "assign_footprint_changed=" << afBool( "changed" ) << "\n";
                std::cout << "assign_footprint_dry_run=" << afBool( "dry_run" ) << "\n";
                std::cout << "assign_footprint_instance_targeted=" << afBool( "instance_targeted" )
                          << "\n";
                std::cout << "assign_footprint_affected_instances_count="
                          << afNum( "affected_instances_count" ) << "\n";
                std::cout << "assign_footprint_shared_sheet_detected=" << afBool( "shared_sheet_detected" )
                          << "\n";
                std::cout << "assign_footprint_global_update=" << afBool( "global_update" ) << "\n";
                std::cout << "assign_footprint_instance_path=" << afStr( "instance_path" ) << "\n";
                std::cout << "assign_footprint_sheet_path=" << afStr( "sheet_path" ) << "\n";
                std::cout << "assign_footprint_exact_blocker_if_any=" << afStr( "exact_blocker_if_any" )
                          << "\n";
                std::cout << "assign_footprint_old_footprint=" << afStr( "old_footprint" ) << "\n";
                std::cout << "assign_footprint_new_footprint=" << afStr( "new_footprint" ) << "\n";
            }
            else if( req.name() == "pcb_get_board_items_session"
                     || req.name() == "pcb_get_net_items_session" )
            {
                // Full Struct Utf8DebugString can take minutes when items_summary is large; emit a
                // trimmed copy for console harnesses (regex-based probes still match scalar fields).
                google::protobuf::Struct trimmed;
                trimmed.CopyFrom( out.result() );
                auto* fields = trimmed.mutable_fields();
                auto    itList = fields->find( "items_summary" );
                if( itList != fields->end() && itList->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    const int n = itList->second.list_value().values_size();
                    google::protobuf::Value placeholder;
                    placeholder.set_string_value(
                            std::string( "omitted_items_summary_entries(n=" ) + std::to_string( n ) + ")" );
                    ( *fields )["items_summary"] = std::move( placeholder );
                }
                std::cout << trimmed.Utf8DebugString() << "\n";

                std::string first_item_id;
                auto        itItemsRaw = out.result().fields().find( "items_summary" );
                if( itItemsRaw != out.result().fields().end()
                    && itItemsRaw->second.kind_case() == google::protobuf::Value::kListValue )
                {
                    const auto& vals = itItemsRaw->second.list_value().values();
                    if( vals.size() > 0 && vals.Get( 0 ).kind_case() == google::protobuf::Value::kStructValue )
                    {
                        const auto& row = vals.Get( 0 ).struct_value().fields();
                        auto        itId = row.find( "id" );
                        if( itId != row.end()
                            && itId->second.kind_case() == google::protobuf::Value::kStringValue )
                        {
                            first_item_id = itId->second.string_value();
                        }
                    }
                }
                std::cout << "first_item_id=" << first_item_id << "\n";
            }
            else
            {
                std::cout << out.result().Utf8DebugString() << "\n";
            }
        }

        return 0;
    }

    std::cerr << "Unknown command: " << cmd << "\n";
    return 1;
}

