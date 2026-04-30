/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
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

#include <boost/test/unit_test.hpp>

#include <map>
#include <string>

#include <api/api_handler_editor.h>
#include <api/common/commands/editor_commands.pb.h>
#include <api/common/types/base_types.pb.h>
#include <kiid.h>

using kiapi::common::ApiStatusCode;
using kiapi::common::commands::CreateItems;
using kiapi::common::commands::CreateItemsResponse;
using kiapi::common::commands::DeleteItems;
using kiapi::common::commands::DeleteItemsResponse;
using kiapi::common::commands::ItemDeletionStatus;
using kiapi::common::commands::ItemStatus;
using kiapi::common::commands::ItemStatusCode;
using kiapi::common::commands::UpdateItems;
using kiapi::common::commands::UpdateItemsResponse;
using kiapi::common::types::DocumentType;
using kiapi::common::types::ItemHeader;
using kiapi::common::types::ItemRequestStatus;


/**
 * Minimal concrete subclass of API_HANDLER_EDITOR used to exercise the protected
 * handleCreateItems / handleUpdateItems / handleDeleteItems entry points without
 * needing a real EDA_BASE_FRAME or document.
 *
 * Tests configure m_idStatusMap to control what deleteItemsInternal() reports per id.
 */
class MOCK_API_HANDLER_EDITOR : public API_HANDLER_EDITOR
{
public:
    MOCK_API_HANDLER_EDITOR() : API_HANDLER_EDITOR( nullptr ) {}

    /// Test-controlled per-id deletion outcome. IDs not present here keep IDS_NONEXISTENT.
    std::map<KIID, ItemDeletionStatus> m_idStatusMap;

    /// Public shims so tests can call the protected handlers directly.
    HANDLER_RESULT<DeleteItemsResponse> callHandleDeleteItems(
            const HANDLER_CONTEXT<DeleteItems>& aCtx )
    {
        return handleDeleteItems( aCtx );
    }

    HANDLER_RESULT<CreateItemsResponse> callHandleCreateItems(
            const HANDLER_CONTEXT<CreateItems>& aCtx )
    {
        return handleCreateItems( aCtx );
    }

    HANDLER_RESULT<UpdateItemsResponse> callHandleUpdateItems(
            const HANDLER_CONTEXT<UpdateItems>& aCtx )
    {
        return handleUpdateItems( aCtx );
    }

protected:
    std::optional<kiapi::common::ApiResponseStatus> checkForBusy() override
    {
        // Base class dereferences m_frame; bypass that for tests.
        return std::nullopt;
    }

    DocumentType thisDocumentType() const override
    {
        return DocumentType::DOCTYPE_PCB;
    }

    bool validateDocumentInternal( const DocumentSpecifier& ) const override
    {
        return true;
    }

    void deleteItemsInternal( std::map<KIID, ItemDeletionStatus>& aItemsToDelete,
                              const std::string& ) override
    {
        for( auto& [id, status] : aItemsToDelete )
        {
            auto it = m_idStatusMap.find( id );

            if( it != m_idStatusMap.end() )
                status = it->second;
            // else leave IDS_NONEXISTENT (the default set by handleDeleteItems)
        }
    }

    HANDLER_RESULT<ItemRequestStatus> handleCreateUpdateItemsInternal( bool,
            const std::string&,
            const kiapi::common::types::ItemHeader&,
            const google::protobuf::RepeatedPtrField<google::protobuf::Any>& aItems,
            std::function<void( ItemStatus, google::protobuf::Any )> aItemHandler ) override
    {
        for( const google::protobuf::Any& item : aItems )
        {
            ItemStatus status;
            status.set_code( ItemStatusCode::ISC_OK );
            aItemHandler( status, item );
        }

        return ItemRequestStatus::IRS_OK;
    }

    std::optional<EDA_ITEM*> getItemFromDocument( const DocumentSpecifier&, const KIID& ) override
    {
        return std::nullopt;
    }

    std::unique_ptr<COMMIT> createCommit() override
    {
        return nullptr;
    }
};


namespace
{

/// Build a header with a valid PCB DocumentSpecifier so validateItemHeaderDocument() accepts it.
ItemHeader makePcbHeader()
{
    ItemHeader header;
    header.mutable_document()->set_type( DocumentType::DOCTYPE_PCB );
    header.mutable_document()->set_board_filename( "test.kicad_pcb" );
    return header;
}

} // namespace


BOOST_AUTO_TEST_SUITE( ApiHandlerEditor )


/**
 * Regression test for the bug where DeleteItemsResponse.deleted_items was always empty:
 * handleDeleteItems() built per-id ItemDeletionResult locals but never appended them.
 *
 * Verifies the response carries one entry per submitted id, with the id and status from
 * deleteItemsInternal() faithfully propagated.
 */
BOOST_AUTO_TEST_CASE( DeleteItems_HappyPath_PopulatesPerIdResults )
{
    MOCK_API_HANDLER_EDITOR handler;

    KIID idOk;
    KIID idMissing;
    KIID idImmutable;

    handler.m_idStatusMap[idOk]        = ItemDeletionStatus::IDS_OK;
    handler.m_idStatusMap[idImmutable] = ItemDeletionStatus::IDS_IMMUTABLE;
    // idMissing intentionally absent -> remains IDS_NONEXISTENT

    HANDLER_CONTEXT<DeleteItems> ctx;
    ctx.ClientName = "test_client";
    *ctx.Request.mutable_header() = makePcbHeader();
    ctx.Request.add_item_ids()->set_value( idOk.AsStdString() );
    ctx.Request.add_item_ids()->set_value( idMissing.AsStdString() );
    ctx.Request.add_item_ids()->set_value( idImmutable.AsStdString() );

    HANDLER_RESULT<DeleteItemsResponse> result = handler.callHandleDeleteItems( ctx );

    BOOST_REQUIRE( result.has_value() );

    const DeleteItemsResponse& response = *result;
    BOOST_CHECK( response.status() == ItemRequestStatus::IRS_OK );
    BOOST_REQUIRE_EQUAL( response.deleted_items_size(), 3 );

    std::map<std::string, ItemDeletionStatus> got;

    for( const auto& entry : response.deleted_items() )
        got[entry.id().value()] = entry.status();

    BOOST_CHECK( got[idOk.AsStdString()]        == ItemDeletionStatus::IDS_OK );
    BOOST_CHECK( got[idMissing.AsStdString()]   == ItemDeletionStatus::IDS_NONEXISTENT );
    BOOST_CHECK( got[idImmutable.AsStdString()] == ItemDeletionStatus::IDS_IMMUTABLE );
}


BOOST_AUTO_TEST_CASE( DeleteItems_EmptyList_ReturnsBadRequest )
{
    MOCK_API_HANDLER_EDITOR handler;

    HANDLER_CONTEXT<DeleteItems> ctx;
    ctx.ClientName = "test_client";
    *ctx.Request.mutable_header() = makePcbHeader();
    // no item_ids appended

    HANDLER_RESULT<DeleteItemsResponse> result = handler.callHandleDeleteItems( ctx );

    BOOST_REQUIRE( !result.has_value() );
    BOOST_CHECK( result.error().status() == ApiStatusCode::AS_BAD_REQUEST );
}


BOOST_AUTO_TEST_CASE( DeleteItems_WrongDocumentType_ReturnsUnhandled )
{
    MOCK_API_HANDLER_EDITOR handler;

    HANDLER_CONTEXT<DeleteItems> ctx;
    ctx.ClientName = "test_client";
    ctx.Request.mutable_header()->mutable_document()->set_type( DocumentType::DOCTYPE_SCHEMATIC );
    ctx.Request.add_item_ids()->set_value( KIID().AsStdString() );

    HANDLER_RESULT<DeleteItemsResponse> result = handler.callHandleDeleteItems( ctx );

    BOOST_REQUIRE( !result.has_value() );
    BOOST_CHECK( result.error().status() == ApiStatusCode::AS_UNHANDLED );
}


/**
 * Sanity check on the sibling Create handler: response.created_items must be populated
 * when the internal item handler is invoked. Mirrors the DeleteItems regression check.
 */
BOOST_AUTO_TEST_CASE( CreateItems_PopulatesCreatedItemsList )
{
    MOCK_API_HANDLER_EDITOR handler;

    HANDLER_CONTEXT<CreateItems> ctx;
    ctx.ClientName = "test_client";
    *ctx.Request.mutable_header() = makePcbHeader();
    ctx.Request.add_items();
    ctx.Request.add_items();

    HANDLER_RESULT<CreateItemsResponse> result = handler.callHandleCreateItems( ctx );

    BOOST_REQUIRE( result.has_value() );
    BOOST_CHECK( result->status() == ItemRequestStatus::IRS_OK );
    BOOST_CHECK_EQUAL( result->created_items_size(), 2 );

    for( const auto& entry : result->created_items() )
        BOOST_CHECK( entry.status().code() == ItemStatusCode::ISC_OK );
}


BOOST_AUTO_TEST_CASE( UpdateItems_PopulatesUpdatedItemsList )
{
    MOCK_API_HANDLER_EDITOR handler;

    HANDLER_CONTEXT<UpdateItems> ctx;
    ctx.ClientName = "test_client";
    *ctx.Request.mutable_header() = makePcbHeader();
    ctx.Request.add_items();
    ctx.Request.add_items();

    HANDLER_RESULT<UpdateItemsResponse> result = handler.callHandleUpdateItems( ctx );

    BOOST_REQUIRE( result.has_value() );
    BOOST_CHECK( result->status() == ItemRequestStatus::IRS_OK );
    BOOST_CHECK_EQUAL( result->updated_items_size(), 2 );

    for( const auto& entry : result->updated_items() )
        BOOST_CHECK( entry.status().code() == ItemStatusCode::ISC_OK );
}


BOOST_AUTO_TEST_SUITE_END()
