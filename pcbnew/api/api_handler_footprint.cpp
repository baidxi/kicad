/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2026 Benjamin Chung (ckfinite@gmail.com)
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

#include <api/api_handler_footprint.h>
#include <api/api_pcb_utils.h>
#include <api/api_enums.h>
#include <api/api_utils.h>
#include <board_commit.h>
#include <footprint.h>
#include <footprint_edit_frame.h>
#include <pad.h>
#include <pcb_group.h>
#include <project.h>
#include <zone.h>

#include <api/common/types/base_types.pb.h>

using namespace kiapi::common::commands;
using types::CommandStatus;
using types::DocumentType;
using types::ItemRequestStatus;


API_HANDLER_FOOTPRINT::API_HANDLER_FOOTPRINT( FOOTPRINT_EDIT_FRAME* aFrame ) :
        API_HANDLER_FOOTPRINT( CreateFootprintFrameContext( aFrame ), aFrame )
{
}


API_HANDLER_FOOTPRINT::API_HANDLER_FOOTPRINT( std::shared_ptr<FOOTPRINT_CONTEXT> aContext,
                                              FOOTPRINT_EDIT_FRAME* aFrame ) :
        API_HANDLER_BOARD( std::move( aContext ), aFrame )
{
    registerHandler<GetOpenDocuments, GetOpenDocumentsResponse>(
            &API_HANDLER_FOOTPRINT::handleGetOpenDocuments );
    registerHandler<SaveDocument, Empty>( &API_HANDLER_FOOTPRINT::handleSaveDocument );
    registerHandler<SaveCopyOfDocument, Empty>(
            &API_HANDLER_FOOTPRINT::handleSaveCopyOfDocument );
    registerHandler<RevertDocument, Empty>( &API_HANDLER_FOOTPRINT::handleRevertDocument );

    registerHandler<GetItems, GetItemsResponse>( &API_HANDLER_FOOTPRINT::handleGetItems );
}


FOOTPRINT_EDIT_FRAME* API_HANDLER_FOOTPRINT::frame() const
{
    return static_cast<FOOTPRINT_EDIT_FRAME*>( m_frame );
}


bool API_HANDLER_FOOTPRINT::validateDocumentInternal( const DocumentSpecifier& aDocument ) const
{
    if( aDocument.type() != DocumentType::DOCTYPE_FOOTPRINT )
        return false;

    LIB_ID target_fp = footprintContext()->GetLoadedFPID();

    return 0 == aDocument.lib_id().library_nickname().compare( target_fp.GetUniStringLibNickname() ) &&
        0 == aDocument.lib_id().entry_name().compare( target_fp.GetUniStringLibItemName() );
}

HANDLER_RESULT<FOOTPRINT*> API_HANDLER_FOOTPRINT::validateAndGetFootprint(
        const DocumentSpecifier& aDocument )
{
    if( std::optional<ApiResponseStatus> busy = checkForBusy() )
        return tl::unexpected( *busy );

    HANDLER_RESULT<bool> documentValidation = validateDocument( aDocument );

    if( !documentValidation )
        return tl::unexpected( documentValidation.error() );

    FOOTPRINT* editorFootprint = board()->GetFirstFootprint();

    if( !editorFootprint )
    {
        ApiResponseStatus e;
        e.set_status( ApiStatusCode::AS_BAD_REQUEST );
        e.set_error_message( "no footprint is currently loaded" );
        return tl::unexpected( e );
    }

    return editorFootprint;
}


HANDLER_RESULT<GetOpenDocumentsResponse> API_HANDLER_FOOTPRINT::handleGetOpenDocuments(
        const HANDLER_CONTEXT<GetOpenDocuments>& aCtx )
{
    if( aCtx.Request.type() != DocumentType::DOCTYPE_FOOTPRINT )
    {
        ApiResponseStatus e;
        e.set_status( ApiStatusCode::AS_UNHANDLED );
        return tl::unexpected( e );
    }

    GetOpenDocumentsResponse response;
    common::types::DocumentSpecifier doc;

    LIB_ID fpid = footprintContext()->GetLoadedFPID();

    doc.set_type( DocumentType::DOCTYPE_FOOTPRINT );
    doc.mutable_lib_id()->set_library_nickname( fpid.GetUniStringLibNickname() );
    doc.mutable_lib_id()->set_entry_name( fpid.GetUniStringLibItemName() );

    doc.mutable_project()->set_name( project().GetProjectName().ToStdString() );
    doc.mutable_project()->set_path( project().GetProjectDirectory().ToStdString() );

    response.mutable_documents()->Add( std::move( doc ) );
    return response;
}


HANDLER_RESULT<Empty> API_HANDLER_FOOTPRINT::handleSaveDocument(
        const HANDLER_CONTEXT<SaveDocument>& aCtx )
{
    HANDLER_RESULT<FOOTPRINT*> footprint = validateAndGetFootprint( aCtx.Request.document() );

    if( !footprint )
        return tl::unexpected( footprint.error() );

    if( !footprintContext()->SaveFootprint( *footprint ) )
    {
        ApiResponseStatus e;
        e.set_status( ApiStatusCode::AS_BAD_REQUEST );
        e.set_error_message( "failed to save footprint" );
        return tl::unexpected( e );
    }

    return Empty();
}


HANDLER_RESULT<Empty> API_HANDLER_FOOTPRINT::handleSaveCopyOfDocument(
        const HANDLER_CONTEXT<SaveCopyOfDocument>& aCtx )
{
    HANDLER_RESULT<FOOTPRINT*> footprint = validateAndGetFootprint( aCtx.Request.document() );

    if( !footprint )
        return tl::unexpected( footprint.error() );

    wxString newName = wxString::FromUTF8( aCtx.Request.path() );

    if( newName.IsEmpty() )
    {
        ApiResponseStatus e;
        e.set_status( ApiStatusCode::AS_BAD_REQUEST );
        e.set_error_message( "path must contain the new footprint name" );
        return tl::unexpected( e );
    }

    // Clone the footprint so we don't modify the one being edited
    std::unique_ptr<FOOTPRINT> copy( static_cast<FOOTPRINT*>( ( *footprint )->Clone() ) );

    wxString libraryName = footprintContext()->GetLoadedFPID().GetLibNickname();
    copy->SetFPID( LIB_ID( libraryName, newName ) );

    if( !footprintContext()->SaveFootprintInLibrary( copy.get(), libraryName ) )
    {
        ApiResponseStatus e;
        e.set_status( ApiStatusCode::AS_BAD_REQUEST );
        e.set_error_message( fmt::format( "failed to save footprint copy '{}' to library '{}'",
                                          newName.ToStdString(),
                                          libraryName.ToStdString() ) );
        return tl::unexpected( e );
    }

    return Empty();
}


HANDLER_RESULT<Empty> API_HANDLER_FOOTPRINT::handleRevertDocument(
        const HANDLER_CONTEXT<RevertDocument>& aCtx )
{
    if( std::optional<ApiResponseStatus> busy = checkForBusy() )
        return tl::unexpected( *busy );

    HANDLER_RESULT<bool> documentValidation = validateDocument( aCtx.Request.document() );

    if( !documentValidation )
        return tl::unexpected( documentValidation.error() );

    frame()->GetScreen()->SetContentModified( false );
    frame()->RevertFootprint(); // dialog is suppressed by ^

    return Empty();
}


HANDLER_RESULT<GetItemsResponse> API_HANDLER_FOOTPRINT::handleGetItems(
        const HANDLER_CONTEXT<GetItems>& aCtx )
{
    if( std::optional<ApiResponseStatus> busy = checkForBusy() )
        return tl::unexpected( *busy );

    if( !validateItemHeaderDocument( aCtx.Request.header() ) )
    {
        ApiResponseStatus e;
        // No message needed for AS_UNHANDLED; this is an internal flag for the API server
        e.set_status( ApiStatusCode::AS_UNHANDLED );
        return tl::unexpected( e );
    }

    GetItemsResponse response;

    FOOTPRINT* footprint = board()->GetFirstFootprint();
    std::vector<BOARD_ITEM*> items;
    std::set<KICAD_T> typesRequested, typesInserted;
    bool handledAnything = false;


    for( int typeRaw : aCtx.Request.types() )
    {
        auto typeMessage = static_cast<common::types::KiCadObjectType>( typeRaw );
        KICAD_T type = FromProtoEnum<KICAD_T>( typeMessage );

        if( type == TYPE_NOT_INIT )
            continue;

        typesRequested.emplace( type );

        if( typesInserted.count( type ) )
            continue;

        switch( type )
        {

        case PCB_PAD_T:
        {
            handledAnything = true;

            std::copy( footprint->Pads().begin(), footprint->Pads().end(),
                        std::back_inserter( items ) );

            typesInserted.insert( PCB_PAD_T );
            break;
        }

        case PCB_SHAPE_T:
        case PCB_TEXT_T:
        case PCB_TEXTBOX_T:
        case PCB_BARCODE_T:
        {
            handledAnything = true;
            bool inserted = false;

            for( BOARD_ITEM* item : footprint->GraphicalItems() )
            {
                if( item->Type() == type )
                {
                    items.emplace_back( item );
                    inserted = true;
                }
            }

            if( inserted )
                typesInserted.insert( type );

            break;
        }

        case PCB_DIMENSION_T:
        {
            handledAnything = true;
            bool inserted = false;

            for( BOARD_ITEM* item : footprint->GraphicalItems() )
            {
                switch( item->Type() )
                {
                    case PCB_DIM_ALIGNED_T:
                    case PCB_DIM_CENTER_T:
                    case PCB_DIM_RADIAL_T:
                    case PCB_DIM_ORTHOGONAL_T:
                    case PCB_DIM_LEADER_T:
                        items.emplace_back( item );
                        inserted = true;
                        break;
                    default:
                        break;
                }
            }
            // we have to add the dimension subtypes to the requested to get them out
            typesRequested.insert( {PCB_DIM_ALIGNED_T, PCB_DIM_CENTER_T, PCB_DIM_RADIAL_T, PCB_DIM_ORTHOGONAL_T, PCB_DIM_LEADER_T } );

            if( inserted )
                typesInserted.insert( {PCB_DIM_ALIGNED_T, PCB_DIM_CENTER_T, PCB_DIM_RADIAL_T, PCB_DIM_ORTHOGONAL_T, PCB_DIM_LEADER_T } );

            break;
        }

        case PCB_ZONE_T:
        {
            handledAnything = true;

            std::copy( footprint->Zones().begin(), footprint->Zones().end(),
                       std::back_inserter( items ) );

            typesInserted.insert( PCB_ZONE_T );
            break;
        }

        case PCB_GROUP_T:
        {
            handledAnything = true;

            std::copy( footprint->Groups().begin(), footprint->Groups().end(),
                       std::back_inserter( items ) );

            typesInserted.insert( PCB_GROUP_T );
            break;
        }
        default:
            break;
        }
    }


    if( !handledAnything )
    {
        ApiResponseStatus e;
        e.set_status( ApiStatusCode::AS_BAD_REQUEST );
        e.set_error_message( "none of the requested types are valid for a Board object" );
        return tl::unexpected( e );
    }

    for( const BOARD_ITEM* item : items )
    {
        if( !typesRequested.count( item->Type() ) )
            continue;

        google::protobuf::Any itemBuf;
        item->Serialize( itemBuf );
        response.mutable_items()->Add( std::move( itemBuf ) );
    }

    response.set_status( ItemRequestStatus::IRS_OK );
    return response;
}


BOARD_ITEM_CONTAINER* API_HANDLER_FOOTPRINT::getDefaultContainer()
{
    return board()->GetFirstFootprint();
}
