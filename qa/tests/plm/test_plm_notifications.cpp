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
 * @file test_plm_notifications.cpp
 * @brief Tests for PLM notification structures and listener interface.
 */

#include <boost/test/unit_test.hpp>
#include <plm/plm_notifications.h>


BOOST_AUTO_TEST_SUITE( PlmNotifications )


/**
 * Test PLM_NOTIFICATION default construction.
 */
BOOST_AUTO_TEST_CASE( NotificationDefaultConstruction )
{
    PLM_NOTIFICATION notif;

    BOOST_CHECK( notif.type == PLM_NOTIFICATION_TYPE::FILE_MODIFIED );
    BOOST_CHECK( notif.project_id.IsEmpty() );
    BOOST_CHECK( notif.user_id.IsEmpty() );
    BOOST_CHECK( notif.user_name.IsEmpty() );
    BOOST_CHECK( notif.message.IsEmpty() );
    BOOST_CHECK( notif.revision_id.IsEmpty() );
    BOOST_CHECK( notif.new_state == PLM_STATE_CATEGORY::EDITABLE );
    BOOST_CHECK_EQUAL( notif.timestamp, 0 );
}


/**
 * Test PLM_NOTIFICATION construction with parameters.
 */
BOOST_AUTO_TEST_CASE( NotificationParameterizedConstruction )
{
    PLM_NOTIFICATION notif( PLM_NOTIFICATION_TYPE::LOCK_ACQUIRED, wxS( "proj-001" ) );

    BOOST_CHECK( notif.type == PLM_NOTIFICATION_TYPE::LOCK_ACQUIRED );
    BOOST_CHECK( notif.project_id == wxS( "proj-001" ) );
    BOOST_CHECK( notif.new_state == PLM_STATE_CATEGORY::EDITABLE );
}


/**
 * Test LockAcquired factory method.
 */
BOOST_AUTO_TEST_CASE( NotificationLockAcquired )
{
    auto notif = PLM_NOTIFICATION::LockAcquired( wxS( "proj-001" ), wxS( "user1" ),
                                                  wxS( "John Doe" ) );

    BOOST_CHECK( notif.type == PLM_NOTIFICATION_TYPE::LOCK_ACQUIRED );
    BOOST_CHECK( notif.project_id == wxS( "proj-001" ) );
    BOOST_CHECK( notif.user_id == wxS( "user1" ) );
    BOOST_CHECK( notif.user_name == wxS( "John Doe" ) );
    BOOST_CHECK( !notif.message.IsEmpty() );
    BOOST_CHECK( notif.IsLockEvent() );
}


/**
 * Test LockReleased factory method.
 */
BOOST_AUTO_TEST_CASE( NotificationLockReleased )
{
    auto notif = PLM_NOTIFICATION::LockReleased( wxS( "proj-001" ), wxS( "user1" ),
                                                  wxS( "John Doe" ) );

    BOOST_CHECK( notif.type == PLM_NOTIFICATION_TYPE::LOCK_RELEASED );
    BOOST_CHECK( notif.project_id == wxS( "proj-001" ) );
    BOOST_CHECK( notif.IsLockEvent() );
}


/**
 * Test StateChanged factory method.
 */
BOOST_AUTO_TEST_CASE( NotificationStateChanged )
{
    auto notif = PLM_NOTIFICATION::StateChanged( wxS( "proj-001" ),
                                                  PLM_STATE_CATEGORY::RELEASED,
                                                  wxS( "user1" ), wxS( "John Doe" ) );

    BOOST_CHECK( notif.type == PLM_NOTIFICATION_TYPE::STATE_CHANGED );
    BOOST_CHECK( notif.project_id == wxS( "proj-001" ) );
    BOOST_CHECK( notif.new_state == PLM_STATE_CATEGORY::RELEASED );
    BOOST_CHECK( notif.user_id == wxS( "user1" ) );
    BOOST_CHECK( !notif.IsLockEvent() );
}


/**
 * Test RevisionCreated factory method.
 */
BOOST_AUTO_TEST_CASE( NotificationRevisionCreated )
{
    auto notif = PLM_NOTIFICATION::RevisionCreated( wxS( "proj-001" ), wxS( "rev-5" ),
                                                     wxS( "user1" ), wxS( "John Doe" ) );

    BOOST_CHECK( notif.type == PLM_NOTIFICATION_TYPE::REVISION_CREATED );
    BOOST_CHECK( notif.project_id == wxS( "proj-001" ) );
    BOOST_CHECK( notif.revision_id == wxS( "rev-5" ) );
    BOOST_CHECK( !notif.message.IsEmpty() );
    BOOST_CHECK( !notif.IsLockEvent() );
}


/**
 * Test IsForProject method.
 */
BOOST_AUTO_TEST_CASE( NotificationIsForProject )
{
    PLM_NOTIFICATION notif( PLM_NOTIFICATION_TYPE::FILE_MODIFIED, wxS( "proj-001" ) );

    BOOST_CHECK( notif.IsForProject( wxS( "proj-001" ) ) );
    BOOST_CHECK( !notif.IsForProject( wxS( "proj-002" ) ) );
    BOOST_CHECK( !notif.IsForProject( wxEmptyString ) );
}


/**
 * Test IsLockEvent method for all notification types.
 */
BOOST_AUTO_TEST_CASE( NotificationIsLockEvent )
{
    PLM_NOTIFICATION notif;

    notif.type = PLM_NOTIFICATION_TYPE::LOCK_ACQUIRED;
    BOOST_CHECK( notif.IsLockEvent() );

    notif.type = PLM_NOTIFICATION_TYPE::LOCK_RELEASED;
    BOOST_CHECK( notif.IsLockEvent() );

    notif.type = PLM_NOTIFICATION_TYPE::STATE_CHANGED;
    BOOST_CHECK( !notif.IsLockEvent() );

    notif.type = PLM_NOTIFICATION_TYPE::REVISION_CREATED;
    BOOST_CHECK( !notif.IsLockEvent() );

    notif.type = PLM_NOTIFICATION_TYPE::FILE_MODIFIED;
    BOOST_CHECK( !notif.IsLockEvent() );

    notif.type = PLM_NOTIFICATION_TYPE::FILE_DELETED;
    BOOST_CHECK( !notif.IsLockEvent() );

    notif.type = PLM_NOTIFICATION_TYPE::ATTRIBUTE_CHANGED;
    BOOST_CHECK( !notif.IsLockEvent() );

    notif.type = PLM_NOTIFICATION_TYPE::COMMENT_ADDED;
    BOOST_CHECK( !notif.IsLockEvent() );

    notif.type = PLM_NOTIFICATION_TYPE::ECO_STATUS_CHANGED;
    BOOST_CHECK( !notif.IsLockEvent() );
}


/**
 * Test GetTypeName for all notification types.
 */
BOOST_AUTO_TEST_CASE( NotificationTypeNames )
{
    PLM_NOTIFICATION notif;

    notif.type = PLM_NOTIFICATION_TYPE::LOCK_ACQUIRED;
    BOOST_CHECK( notif.GetTypeName() == wxS( "Lock Acquired" ) );

    notif.type = PLM_NOTIFICATION_TYPE::LOCK_RELEASED;
    BOOST_CHECK( notif.GetTypeName() == wxS( "Lock Released" ) );

    notif.type = PLM_NOTIFICATION_TYPE::STATE_CHANGED;
    BOOST_CHECK( notif.GetTypeName() == wxS( "State Changed" ) );

    notif.type = PLM_NOTIFICATION_TYPE::REVISION_CREATED;
    BOOST_CHECK( notif.GetTypeName() == wxS( "Revision Created" ) );

    notif.type = PLM_NOTIFICATION_TYPE::FILE_MODIFIED;
    BOOST_CHECK( notif.GetTypeName() == wxS( "File Modified" ) );

    notif.type = PLM_NOTIFICATION_TYPE::FILE_DELETED;
    BOOST_CHECK( notif.GetTypeName() == wxS( "File Deleted" ) );

    notif.type = PLM_NOTIFICATION_TYPE::ATTRIBUTE_CHANGED;
    BOOST_CHECK( notif.GetTypeName() == wxS( "Attribute Changed" ) );

    notif.type = PLM_NOTIFICATION_TYPE::COMMENT_ADDED;
    BOOST_CHECK( notif.GetTypeName() == wxS( "Comment Added" ) );

    notif.type = PLM_NOTIFICATION_TYPE::ECO_STATUS_CHANGED;
    BOOST_CHECK( notif.GetTypeName() == wxS( "ECO Status Changed" ) );
}


/**
 * Test PLM_NOTIFICATION_CONFIG default construction.
 */
BOOST_AUTO_TEST_CASE( ConfigDefaultConstruction )
{
    PLM_NOTIFICATION_CONFIG cfg;

    BOOST_CHECK( cfg.subscribe_locks );
    BOOST_CHECK( cfg.subscribe_states );
    BOOST_CHECK( cfg.subscribe_revisions );
    BOOST_CHECK( !cfg.subscribe_modifications );
    BOOST_CHECK( !cfg.subscribe_comments );
    BOOST_CHECK( !cfg.subscribe_eco );
    BOOST_CHECK_EQUAL( cfg.reconnect_delay_ms, 5000 );
    BOOST_CHECK_EQUAL( cfg.max_reconnect_attempts, 10 );
}


/**
 * Test PLM_NOTIFICATION_CONFIG::All factory.
 */
BOOST_AUTO_TEST_CASE( ConfigAll )
{
    auto cfg = PLM_NOTIFICATION_CONFIG::All();

    BOOST_CHECK( cfg.subscribe_locks );
    BOOST_CHECK( cfg.subscribe_states );
    BOOST_CHECK( cfg.subscribe_revisions );
    BOOST_CHECK( cfg.subscribe_modifications );
    BOOST_CHECK( cfg.subscribe_comments );
    BOOST_CHECK( cfg.subscribe_eco );
}


/**
 * Test PLM_NOTIFICATION_CONFIG::Minimal factory.
 */
BOOST_AUTO_TEST_CASE( ConfigMinimal )
{
    auto cfg = PLM_NOTIFICATION_CONFIG::Minimal();

    BOOST_CHECK( cfg.subscribe_locks );
    BOOST_CHECK( cfg.subscribe_states );
    BOOST_CHECK( !cfg.subscribe_revisions );
    BOOST_CHECK( !cfg.subscribe_modifications );
    BOOST_CHECK( !cfg.subscribe_comments );
    BOOST_CHECK( !cfg.subscribe_eco );
}


/**
 * Mock notification listener for testing.
 */
class MockNotificationListener : public PLM_NOTIFICATION_LISTENER
{
public:
    std::vector<PLM_NOTIFICATION> received;
    bool                          connected = false;
    bool                          disconnected = false;
    wxString                      lastError;
    wxString                      disconnectReason;

    void OnNotification( const PLM_NOTIFICATION& aNotification ) override
    {
        received.push_back( aNotification );
    }

    void OnConnected() override
    {
        connected = true;
        disconnected = false;
    }

    void OnDisconnected( const wxString& aReason ) override
    {
        connected = false;
        disconnected = true;
        disconnectReason = aReason;
    }

    void OnError( const wxString& aError ) override
    {
        lastError = aError;
    }
};


/**
 * Test notification listener interface.
 */
BOOST_AUTO_TEST_CASE( ListenerInterface )
{
    MockNotificationListener listener;

    BOOST_CHECK( listener.received.empty() );
    BOOST_CHECK( !listener.connected );
    BOOST_CHECK( !listener.disconnected );

    listener.OnConnected();
    BOOST_CHECK( listener.connected );
    BOOST_CHECK( !listener.disconnected );

    auto notif = PLM_NOTIFICATION::LockAcquired( wxS( "proj-001" ), wxS( "user1" ),
                                                  wxS( "John Doe" ) );
    listener.OnNotification( notif );
    BOOST_CHECK_EQUAL( listener.received.size(), 1 );
    BOOST_CHECK( listener.received[0].type == PLM_NOTIFICATION_TYPE::LOCK_ACQUIRED );

    listener.OnDisconnected( wxS( "Server shutdown" ) );
    BOOST_CHECK( !listener.connected );
    BOOST_CHECK( listener.disconnected );
    BOOST_CHECK( listener.disconnectReason == wxS( "Server shutdown" ) );

    listener.OnError( wxS( "Connection error" ) );
    BOOST_CHECK( listener.lastError == wxS( "Connection error" ) );
}


/**
 * Test listener receiving multiple notifications.
 */
BOOST_AUTO_TEST_CASE( ListenerMultipleNotifications )
{
    MockNotificationListener listener;

    listener.OnNotification(
            PLM_NOTIFICATION::LockAcquired( wxS( "proj-001" ), wxS( "u1" ), wxS( "User 1" ) ) );
    listener.OnNotification(
            PLM_NOTIFICATION::RevisionCreated( wxS( "proj-001" ), wxS( "r1" ), wxS( "u1" ),
                                                wxS( "User 1" ) ) );
    listener.OnNotification(
            PLM_NOTIFICATION::LockReleased( wxS( "proj-001" ), wxS( "u1" ), wxS( "User 1" ) ) );

    BOOST_CHECK_EQUAL( listener.received.size(), 3 );
    BOOST_CHECK( listener.received[0].type == PLM_NOTIFICATION_TYPE::LOCK_ACQUIRED );
    BOOST_CHECK( listener.received[1].type == PLM_NOTIFICATION_TYPE::REVISION_CREATED );
    BOOST_CHECK( listener.received[2].type == PLM_NOTIFICATION_TYPE::LOCK_RELEASED );
}


/**
 * Test notification with timestamp.
 */
BOOST_AUTO_TEST_CASE( NotificationWithTimestamp )
{
    PLM_NOTIFICATION notif( PLM_NOTIFICATION_TYPE::FILE_MODIFIED, wxS( "proj-001" ) );
    notif.timestamp = 1704067200;

    BOOST_CHECK_EQUAL( notif.timestamp, 1704067200 );
}


/**
 * Test notification message generation.
 */
BOOST_AUTO_TEST_CASE( NotificationMessages )
{
    auto lockNotif = PLM_NOTIFICATION::LockAcquired( wxS( "p" ), wxS( "u" ), wxS( "Alice" ) );
    BOOST_CHECK( lockNotif.message.Contains( wxS( "Alice" ) ) );

    auto unlockNotif = PLM_NOTIFICATION::LockReleased( wxS( "p" ), wxS( "u" ), wxS( "Bob" ) );
    BOOST_CHECK( unlockNotif.message.Contains( wxS( "Bob" ) ) );

    auto revNotif = PLM_NOTIFICATION::RevisionCreated( wxS( "p" ), wxS( "rev-5" ), wxS( "u" ),
                                                         wxS( "Charlie" ) );
    BOOST_CHECK( revNotif.message.Contains( wxS( "Charlie" ) ) );
    BOOST_CHECK( revNotif.message.Contains( wxS( "rev-5" ) ) );
}


BOOST_AUTO_TEST_SUITE_END()
