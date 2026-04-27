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

/**
 * @file test_plm_operations_conformance.cpp
 * @brief File operations conformance tests for PLM plugins.
 *
 * These tests verify that PLM plugin file operations
 * conform to the expected interface contract.
 */

#include <boost/test/unit_test.hpp>
#include "plm_conformance_framework.h"

#include <plm/plm_lock.h>
#include <plm/plm_revision.h>

#include <ctime>


BOOST_AUTO_TEST_SUITE( PlmOperationsConformance )


// File Info Tests

BOOST_AUTO_TEST_CASE( FileInfoConstruction )
{
    PLM_FILE_INFO info;
    BOOST_CHECK( info.id.IsEmpty() );
    BOOST_CHECK( info.name.IsEmpty() );
    BOOST_CHECK( info.state_category == PLM_STATE_CATEGORY::EDITABLE );
    BOOST_CHECK( !info.IsLocked() );
    BOOST_CHECK( info.IsEditable() );
}


BOOST_AUTO_TEST_CASE( FileInfoFullConstruction )
{
    int64_t now = static_cast<int64_t>( std::time( nullptr ) );
    PLM_FILE_INFO info( wxS( "proj-123" ), wxS( "My Project" ), wxS( "1.0" ),
                        PLM_STATE_CATEGORY::RELEASED, wxS( "Released" ), wxS( "user1" ), now );

    BOOST_CHECK( info.id == wxS( "proj-123" ) );
    BOOST_CHECK( info.name == wxS( "My Project" ) );
    BOOST_CHECK( info.revision == wxS( "1.0" ) );
    BOOST_CHECK( info.state_category == PLM_STATE_CATEGORY::RELEASED );
    BOOST_CHECK( info.IsReleased() );
    BOOST_CHECK( !info.IsEditable() );
    BOOST_CHECK( info.IsLocked() );
    BOOST_CHECK( info.IsLockedBy( wxS( "user1" ) ) );
    BOOST_CHECK( !info.IsLockedBy( wxS( "user2" ) ) );
}


// Checkout Options Tests

BOOST_AUTO_TEST_CASE( CheckoutOptionsDefaults )
{
    PLM_CHECKOUT_OPTIONS opts;
    BOOST_CHECK( opts.acquire_lock );
    BOOST_CHECK( opts.revision.IsEmpty() );
    BOOST_CHECK( opts.local_path.IsEmpty() );
}


BOOST_AUTO_TEST_CASE( CheckoutOptionsConstruction )
{
    PLM_CHECKOUT_OPTIONS opts( wxS( "/tmp/project" ), false, wxS( "1.5" ) );

    BOOST_CHECK( opts.local_path == wxS( "/tmp/project" ) );
    BOOST_CHECK( !opts.acquire_lock );
    BOOST_CHECK( opts.revision == wxS( "1.5" ) );
}


// Checkin Options Tests

BOOST_AUTO_TEST_CASE( CheckinOptionsDefaults )
{
    PLM_CHECKIN_OPTIONS opts;
    BOOST_CHECK( opts.revision_comment.IsEmpty() );
    BOOST_CHECK( opts.eco_id.IsEmpty() );
    BOOST_CHECK( opts.release_lock );
    BOOST_CHECK( opts.keep_local );
}


BOOST_AUTO_TEST_CASE( CheckinOptionsConstruction )
{
    PLM_CHECKIN_OPTIONS opts( wxS( "Updated schematic" ), false, wxS( "ECO-001" ) );

    BOOST_CHECK( opts.revision_comment == wxS( "Updated schematic" ) );
    BOOST_CHECK( opts.eco_id == wxS( "ECO-001" ) );
    BOOST_CHECK( !opts.release_lock );
}


// Checkout Result Tests

BOOST_AUTO_TEST_CASE( CheckoutResultFactorySuccess )
{
    PLM_FILE_INFO info( wxS( "proj-1" ), wxS( "Project 1" ), wxS( "1.0" ), PLM_STATE_CATEGORY::EDITABLE );
    PLM_CHECKOUT_RESULT result = PLM_CHECKOUT_RESULT::Success( wxS( "/tmp/proj" ), info );

    BOOST_CHECK( result.success );
    BOOST_CHECK( result.local_path == wxS( "/tmp/proj" ) );
    BOOST_CHECK( result.file_info.id == wxS( "proj-1" ) );
    BOOST_CHECK( result.error_message.IsEmpty() );
}


BOOST_AUTO_TEST_CASE( CheckoutResultFactoryFailure )
{
    PLM_CHECKOUT_RESULT result = PLM_CHECKOUT_RESULT::Failure(
            PLM_ERROR_CODE::ERR_FILE_NOT_FOUND, wxS( "Project not found" ) );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_FILE_NOT_FOUND );
    BOOST_CHECK( result.error_message == wxS( "Project not found" ) );
}


// Checkin Result Tests

BOOST_AUTO_TEST_CASE( CheckinResultFactorySuccess )
{
    PLM_FILE_INFO info( wxS( "proj-1" ), wxS( "Project 1" ), wxS( "1.1" ), PLM_STATE_CATEGORY::EDITABLE );
    PLM_CHECKIN_RESULT result = PLM_CHECKIN_RESULT::Success( wxS( "1.1" ), info );

    BOOST_CHECK( result.success );
    BOOST_CHECK( result.new_revision == wxS( "1.1" ) );
    BOOST_CHECK( result.file_info.revision == wxS( "1.1" ) );
}


BOOST_AUTO_TEST_CASE( CheckinResultFactoryFailure )
{
    PLM_CHECKIN_RESULT result = PLM_CHECKIN_RESULT::Failure(
            PLM_ERROR_CODE::ERR_VERSION_CONFLICT, wxS( "Version conflict detected" ) );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_VERSION_CONFLICT );
    BOOST_CHECK( !result.error_message.IsEmpty() );
}


// Lock Info Tests

BOOST_AUTO_TEST_CASE( LockInfoUnlocked )
{
    PLM_LOCK_INFO info = PLM_LOCK_INFO::Unlocked();

    BOOST_CHECK( !info.is_locked );
    BOOST_CHECK( info.lock_owner.IsEmpty() );
    BOOST_CHECK( info.CanModify() );
}


BOOST_AUTO_TEST_CASE( LockInfoLocked )
{
    int64_t now = static_cast<int64_t>( std::time( nullptr ) );
    PLM_LOCK_INFO info = PLM_LOCK_INFO::Locked( wxS( "user1" ), now, true, PLM_LOCK_TYPE::MANDATORY, wxS( "Working" ) );

    BOOST_CHECK( info.is_locked );
    BOOST_CHECK( info.lock_owner == wxS( "user1" ) );
    BOOST_CHECK( info.locked_by_me );
    BOOST_CHECK( info.IsMandatory() );
    BOOST_CHECK( !info.IsAdvisory() );
    BOOST_CHECK( info.CanModify() );
    BOOST_CHECK( info.lock_comment == wxS( "Working" ) );
}


BOOST_AUTO_TEST_CASE( LockInfoLockedByOther )
{
    int64_t now = static_cast<int64_t>( std::time( nullptr ) );
    PLM_LOCK_INFO info = PLM_LOCK_INFO::Locked( wxS( "other_user" ), now, false );

    BOOST_CHECK( info.is_locked );
    BOOST_CHECK( !info.locked_by_me );
    BOOST_CHECK( !info.CanModify() );
}


// Lock Request Tests

BOOST_AUTO_TEST_CASE( LockRequestConstruction )
{
    PLM_LOCK_REQUEST req;
    BOOST_CHECK( req.project_id.IsEmpty() );
    BOOST_CHECK( !req.force );
    BOOST_CHECK( req.comment.IsEmpty() );

    PLM_LOCK_REQUEST req2( wxS( "proj-123" ), true, wxS( "Need exclusive access" ) );
    BOOST_CHECK( req2.project_id == wxS( "proj-123" ) );
    BOOST_CHECK( req2.force );
    BOOST_CHECK( req2.comment == wxS( "Need exclusive access" ) );
}


// Unlock Request Tests

BOOST_AUTO_TEST_CASE( UnlockRequestConstruction )
{
    PLM_UNLOCK_REQUEST req;
    BOOST_CHECK( req.project_id.IsEmpty() );
    BOOST_CHECK( !req.force );

    PLM_UNLOCK_REQUEST req2( wxS( "proj-456" ), true );
    BOOST_CHECK( req2.project_id == wxS( "proj-456" ) );
    BOOST_CHECK( req2.force );
}


// Lock Result Tests

BOOST_AUTO_TEST_CASE( LockResultFactorySuccess )
{
    PLM_LOCK_INFO info = PLM_LOCK_INFO::Locked( wxS( "me" ), 12345, true );
    PLM_LOCK_RESULT result = PLM_LOCK_RESULT::Success( info );

    BOOST_CHECK( result.success );
    BOOST_CHECK( result.lock_info.is_locked );
    BOOST_CHECK( result.lock_info.locked_by_me );
}


BOOST_AUTO_TEST_CASE( LockResultFactoryFailure )
{
    PLM_LOCK_RESULT result = PLM_LOCK_RESULT::Failure(
            PLM_ERROR_CODE::ERR_AUTH_REQUIRED, wxS( "Not authenticated" ) );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_AUTH_REQUIRED );
}


BOOST_AUTO_TEST_CASE( LockResultConflict )
{
    PLM_LOCK_INFO info = PLM_LOCK_INFO::Locked( wxS( "other_user" ), 12345, false );
    PLM_LOCK_RESULT result = PLM_LOCK_RESULT::Conflict( info );

    BOOST_CHECK( !result.success );
    BOOST_CHECK( result.error_code == PLM_ERROR_CODE::ERR_LOCK_CONFLICT );
    BOOST_CHECK( result.lock_info.lock_owner == wxS( "other_user" ) );
}


// Revision Tests

BOOST_AUTO_TEST_CASE( RevisionConstruction )
{
    PLM_REVISION rev;
    BOOST_CHECK( rev.revision_id.IsEmpty() );
    BOOST_CHECK( rev.state_category == PLM_STATE_CATEGORY::EDITABLE );
    BOOST_CHECK( !rev.IsReleased() );

    int64_t now = static_cast<int64_t>( std::time( nullptr ) );
    PLM_REVISION rev2( wxS( "rev-001" ), wxS( "1.0" ), wxS( "user1" ), now, wxS( "Initial commit" ),
                       PLM_STATE_CATEGORY::RELEASED, wxS( "Released" ) );

    BOOST_CHECK( rev2.revision_id == wxS( "rev-001" ) );
    BOOST_CHECK( rev2.revision_number == wxS( "1.0" ) );
    BOOST_CHECK( rev2.author == wxS( "user1" ) );
    BOOST_CHECK( rev2.IsReleased() );
}


// Revision Change Tests

BOOST_AUTO_TEST_CASE( RevisionChangeConstruction )
{
    PLM_REVISION_CHANGE change;
    BOOST_CHECK( change.change_type == PLM_CHANGE_TYPE::MODIFIED );

    PLM_REVISION_CHANGE change2( PLM_CHANGE_TYPE::ADDED, wxS( "symbol" ), wxS( "U1" ), wxS( "Added resistor" ) );

    BOOST_CHECK( change2.change_type == PLM_CHANGE_TYPE::ADDED );
    BOOST_CHECK( change2.element_type == wxS( "symbol" ) );
    BOOST_CHECK( change2.element_id == wxS( "U1" ) );
    BOOST_CHECK( change2.description == wxS( "Added resistor" ) );
}


// Revision Compare Tests

BOOST_AUTO_TEST_CASE( RevisionCompareEmpty )
{
    PLM_REVISION_COMPARE compare;
    BOOST_CHECK( !compare.HasChanges() );
    BOOST_CHECK( compare.ChangeCount() == 0 );
}


BOOST_AUTO_TEST_CASE( RevisionCompareWithChanges )
{
    PLM_REVISION base( wxS( "r1" ), wxS( "1.0" ), wxS( "user1" ), 1000, wxS( "" ), PLM_STATE_CATEGORY::EDITABLE );
    PLM_REVISION comp( wxS( "r2" ), wxS( "1.1" ), wxS( "user1" ), 2000, wxS( "" ), PLM_STATE_CATEGORY::EDITABLE );

    PLM_REVISION_COMPARE compare( base, comp );

    compare.changes.push_back( PLM_REVISION_CHANGE( PLM_CHANGE_TYPE::ADDED, wxS( "symbol" ), wxS( "U1" ) ) );
    compare.changes.push_back( PLM_REVISION_CHANGE( PLM_CHANGE_TYPE::ADDED, wxS( "symbol" ), wxS( "U2" ) ) );
    compare.changes.push_back( PLM_REVISION_CHANGE( PLM_CHANGE_TYPE::REMOVED, wxS( "wire" ), wxS( "W1" ) ) );
    compare.changes.push_back( PLM_REVISION_CHANGE( PLM_CHANGE_TYPE::MODIFIED, wxS( "footprint" ), wxS( "FP1" ) ) );
    compare.changes.push_back( PLM_REVISION_CHANGE( PLM_CHANGE_TYPE::MOVED, wxS( "symbol" ), wxS( "U3" ) ) );

    BOOST_CHECK( compare.HasChanges() );
    BOOST_CHECK( compare.ChangeCount() == 5 );
    BOOST_CHECK( compare.AddedCount() == 2 );
    BOOST_CHECK( compare.RemovedCount() == 1 );
    BOOST_CHECK( compare.ModifiedCount() == 1 );
    BOOST_CHECK( compare.MovedCount() == 1 );
}


// Revision History Tests

BOOST_AUTO_TEST_CASE( RevisionHistoryEmpty )
{
    PLM_REVISION_HISTORY history( wxS( "proj-1" ) );

    BOOST_CHECK( history.file_id == wxS( "proj-1" ) );
    BOOST_CHECK( history.IsEmpty() );
    BOOST_CHECK( history.Count() == 0 );
    BOOST_CHECK( history.GetLatest() == nullptr );
    BOOST_CHECK( history.FindById( wxS( "rev-1" ) ) == nullptr );
}


BOOST_AUTO_TEST_CASE( RevisionHistoryWithRevisions )
{
    PLM_REVISION_HISTORY history( wxS( "proj-1" ) );

    history.revisions.push_back( PLM_REVISION( wxS( "r3" ), wxS( "1.2" ), wxS( "user1" ), 3000, wxS( "" ), PLM_STATE_CATEGORY::EDITABLE ) );
    history.revisions.push_back( PLM_REVISION( wxS( "r2" ), wxS( "1.1" ), wxS( "user1" ), 2000, wxS( "" ), PLM_STATE_CATEGORY::RELEASED ) );
    history.revisions.push_back( PLM_REVISION( wxS( "r1" ), wxS( "1.0" ), wxS( "user1" ), 1000, wxS( "" ), PLM_STATE_CATEGORY::RELEASED ) );

    BOOST_CHECK( !history.IsEmpty() );
    BOOST_CHECK( history.Count() == 3 );

    const PLM_REVISION* latest = history.GetLatest();
    BOOST_CHECK( latest != nullptr );
    BOOST_CHECK( latest->revision_id == wxS( "r3" ) );

    const PLM_REVISION* found = history.FindById( wxS( "r2" ) );
    BOOST_CHECK( found != nullptr );
    BOOST_CHECK( found->revision_number == wxS( "1.1" ) );

    BOOST_CHECK( history.FindById( wxS( "nonexistent" ) ) == nullptr );
}


// Mock Plugin Operation Tests

BOOST_AUTO_TEST_CASE( MockCheckoutNotImplemented )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_CHECKOUT_OPTIONS opts;
    PLM_CHECKOUT_RESULT result = mock.Checkout( wxS( "proj-1" ), wxS( "/tmp/test" ), opts );

    BOOST_CHECK( !result.success );
}


BOOST_AUTO_TEST_CASE( MockCheckinNotImplemented )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_CHECKIN_OPTIONS opts( wxS( "Test checkin" ) );
    PLM_CHECKIN_RESULT result = mock.Checkin( wxS( "/tmp/test" ), opts );

    BOOST_CHECK( !result.success );
}


BOOST_AUTO_TEST_CASE( MockLockNotImplemented )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_LOCK_REQUEST req( wxS( "proj-1" ) );
    PLM_LOCK_RESULT result = mock.Lock( req );

    BOOST_CHECK( !result.success );
}


BOOST_AUTO_TEST_CASE( MockUnlockNotImplemented )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_UNLOCK_REQUEST req( wxS( "proj-1" ) );
    PLM_LOCK_RESULT result = mock.Unlock( req );

    BOOST_CHECK( !result.success );
}


BOOST_AUTO_TEST_CASE( MockGetLockInfo )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_LOCK_INFO info = mock.GetLockInfo( wxS( "proj-1" ) );

    BOOST_CHECK( !info.is_locked );
}


BOOST_AUTO_TEST_CASE( MockRevisionHistory )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_REVISION_HISTORY history = mock.GetRevisionHistory( wxS( "proj-1" ) );

    BOOST_CHECK( history.IsEmpty() );
}


BOOST_AUTO_TEST_CASE( MockGetRevision )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_REVISION rev = mock.GetRevision( wxS( "proj-1" ), wxS( "rev-1" ) );

    BOOST_CHECK( rev.revision_id.IsEmpty() );
}


BOOST_AUTO_TEST_CASE( MockDownloadRevision )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    bool result = mock.DownloadRevision( wxS( "proj-1" ), wxS( "rev-1" ), wxS( "/tmp/test" ) );

    BOOST_CHECK( !result );
}


BOOST_AUTO_TEST_CASE( MockCompareRevisions )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_REVISION_COMPARE compare = mock.CompareRevisions( wxS( "proj-1" ), wxS( "r1" ), wxS( "r2" ) );

    BOOST_CHECK( !compare.HasChanges() );
}


BOOST_AUTO_TEST_CASE( MockCancelCheckout )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_CANCEL_CHECKOUT_RESULT result = mock.CancelCheckout( wxS( "/tmp/test" ) );

    BOOST_CHECK( result.success );
}


BOOST_AUTO_TEST_SUITE_END()
