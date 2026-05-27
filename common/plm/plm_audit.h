/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
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

/**
 * @file plm_audit.h
 * @brief Audit logging structures for the PLM client interface.
 *
 * This header defines structures for tracking PLM operations for
 * compliance, debugging, and analytics purposes. Audit events capture
 * user actions with timestamps and contextual details.
 */

#ifndef PLM_AUDIT_H
#define PLM_AUDIT_H

#include "plm_types.h"

#include <wx/string.h>
#include <chrono>
#include <cstdint>
#include <vector>


/**
 * Types of audit events in the PLM system.
 *
 * Each event type corresponds to a significant user or system action
 * that should be recorded for tracking purposes.
 */
enum class PLM_AUDIT_EVENT_TYPE
{
    // Authentication events (0-9)
    LOGIN = 0,              ///< User logged in to PLM server
    LOGOUT = 1,             ///< User logged out
    LOGIN_FAILED = 2,       ///< Login attempt failed
    SESSION_EXPIRED = 3,    ///< Session timed out
    TOKEN_REFRESHED = 4,    ///< Authentication token refreshed

    // File operations (10-29)
    CHECKOUT = 10,          ///< File checked out for editing
    CHECKIN = 11,           ///< File checked in after editing
    CHECKOUT_CANCELLED = 12, ///< Checkout cancelled, changes discarded
    DOWNLOAD = 13,          ///< File downloaded (read-only)
    UPLOAD = 14,            ///< New file uploaded to PLM

    // Lock operations (30-39)
    LOCK_ACQUIRED = 30,     ///< Lock acquired on file
    LOCK_RELEASED = 31,     ///< Lock released
    LOCK_FORCED = 32,       ///< Lock forcefully broken by another user
    LOCK_EXPIRED = 33,      ///< Lock auto-released due to timeout
    LOCK_DENIED = 34,       ///< Lock request denied

    // Revision operations (40-49)
    REVISION_CREATED = 40,  ///< New revision created
    REVISION_VIEWED = 41,   ///< Revision history viewed
    REVISION_COMPARED = 42, ///< Two revisions compared
    REVISION_RESTORED = 43, ///< Previous revision restored

    // State transitions (50-59)
    STATE_CHANGED = 50,     ///< File state changed (e.g., draft to released)
    ECO_SUBMITTED = 51,     ///< Engineering change order submitted
    ECO_APPROVED = 52,      ///< ECO approved
    ECO_REJECTED = 53,      ///< ECO rejected

    // Attribute operations (60-69)
    ATTRIBUTE_MODIFIED = 60, ///< File attributes modified
    SCHEMA_UPDATED = 61,    ///< Schema definition updated

    // Administrative (70-79)
    PERMISSION_CHANGED = 70, ///< User permissions modified
    POLICY_UPDATED = 71,    ///< Organizational policy changed
    USER_CREATED = 72,      ///< New user account created
    USER_DEACTIVATED = 73,  ///< User account deactivated

    // Errors and warnings (80-89)
    ERROR_OCCURRED = 80,    ///< Operation failed with error
    WARNING_ISSUED = 81,    ///< Non-fatal warning recorded
    CONFLICT_DETECTED = 82, ///< Merge conflict detected

    // System events (90-99)
    SYNC_STARTED = 90,      ///< Background sync started
    SYNC_COMPLETED = 91,    ///< Background sync completed
    CACHE_CLEARED = 92,     ///< Local cache cleared
    PLUGIN_LOADED = 93,     ///< PLM plugin loaded
    PLUGIN_UNLOADED = 94,   ///< PLM plugin unloaded

    // Review events (100-109)
    REVIEW_SUBMITTED = 100,     ///< Design review submitted
    REVIEW_COMMENT_ADDED = 101, ///< Comment added to review
    REVIEW_DECISION = 102,      ///< Review decision recorded
    REVIEW_MERGED = 103,        ///< Review merged
    REVIEW_CLOSED = 104,        ///< Review closed without merge
    REVIEW_REASSIGNED = 105,    ///< Review reassigned to different reviewer

    // CI/CD events (110-119)
    PIPELINE_TRIGGERED = 110,   ///< CI pipeline triggered
    PIPELINE_COMPLETED = 111,   ///< CI pipeline completed
    ARTIFACT_DOWNLOADED = 112,  ///< CI artifact downloaded

    // Custom event range starts at 1000
    CUSTOM = 1000           ///< Plugin-defined custom event
};


/**
 * A single audit event record.
 *
 * Captures a timestamped record of a PLM operation with all relevant
 * context for later analysis or compliance reporting.
 */
struct PLM_AUDIT_EVENT
{
    int64_t              timestamp_ms = 0;  ///< Event time (milliseconds since Unix epoch)
    PLM_AUDIT_EVENT_TYPE event_type;        ///< Type of event
    wxString             event_id;          ///< Unique event identifier (UUID)
    wxString             project_id;        ///< Affected project/file ID
    wxString             project_name;      ///< Human-readable project name
    wxString             user_id;           ///< User who triggered the event
    wxString             user_name;         ///< Human-readable username
    wxString             machine_id;        ///< Machine where event occurred
    wxString             details_json;      ///< Additional details as JSON
    wxString             result;            ///< Operation result (success/failure message)
    bool                 success = true;    ///< Whether the operation succeeded

    PLM_AUDIT_EVENT() : event_type( PLM_AUDIT_EVENT_TYPE::CUSTOM ) {}

    PLM_AUDIT_EVENT( PLM_AUDIT_EVENT_TYPE aType, const wxString& aProjectId,
                     const wxString& aUserId ) :
            event_type( aType ),
            project_id( aProjectId ),
            user_id( aUserId )
    {
        SetTimestampNow();
    }

    /**
     * Set the timestamp to the current time.
     */
    void SetTimestampNow()
    {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>( duration ).count();
    }

    /**
     * Get a human-readable name for the event type.
     * @return Event type as display string.
     */
    wxString GetEventTypeName() const
    {
        switch( event_type )
        {
        case PLM_AUDIT_EVENT_TYPE::LOGIN:             return wxS( "Login" );
        case PLM_AUDIT_EVENT_TYPE::LOGOUT:            return wxS( "Logout" );
        case PLM_AUDIT_EVENT_TYPE::LOGIN_FAILED:      return wxS( "Login Failed" );
        case PLM_AUDIT_EVENT_TYPE::SESSION_EXPIRED:   return wxS( "Session Expired" );
        case PLM_AUDIT_EVENT_TYPE::TOKEN_REFRESHED:   return wxS( "Token Refreshed" );
        case PLM_AUDIT_EVENT_TYPE::CHECKOUT:          return wxS( "Checkout" );
        case PLM_AUDIT_EVENT_TYPE::CHECKIN:           return wxS( "Checkin" );
        case PLM_AUDIT_EVENT_TYPE::CHECKOUT_CANCELLED: return wxS( "Checkout Cancelled" );
        case PLM_AUDIT_EVENT_TYPE::DOWNLOAD:          return wxS( "Download" );
        case PLM_AUDIT_EVENT_TYPE::UPLOAD:            return wxS( "Upload" );
        case PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED:     return wxS( "Lock Acquired" );
        case PLM_AUDIT_EVENT_TYPE::LOCK_RELEASED:     return wxS( "Lock Released" );
        case PLM_AUDIT_EVENT_TYPE::LOCK_FORCED:       return wxS( "Lock Forced" );
        case PLM_AUDIT_EVENT_TYPE::LOCK_EXPIRED:      return wxS( "Lock Expired" );
        case PLM_AUDIT_EVENT_TYPE::LOCK_DENIED:       return wxS( "Lock Denied" );
        case PLM_AUDIT_EVENT_TYPE::REVISION_CREATED:  return wxS( "Revision Created" );
        case PLM_AUDIT_EVENT_TYPE::REVISION_VIEWED:   return wxS( "Revision Viewed" );
        case PLM_AUDIT_EVENT_TYPE::REVISION_COMPARED: return wxS( "Revision Compared" );
        case PLM_AUDIT_EVENT_TYPE::REVISION_RESTORED: return wxS( "Revision Restored" );
        case PLM_AUDIT_EVENT_TYPE::STATE_CHANGED:     return wxS( "State Changed" );
        case PLM_AUDIT_EVENT_TYPE::ECO_SUBMITTED:     return wxS( "ECO Submitted" );
        case PLM_AUDIT_EVENT_TYPE::ECO_APPROVED:      return wxS( "ECO Approved" );
        case PLM_AUDIT_EVENT_TYPE::ECO_REJECTED:      return wxS( "ECO Rejected" );
        case PLM_AUDIT_EVENT_TYPE::ATTRIBUTE_MODIFIED: return wxS( "Attribute Modified" );
        case PLM_AUDIT_EVENT_TYPE::SCHEMA_UPDATED:    return wxS( "Schema Updated" );
        case PLM_AUDIT_EVENT_TYPE::PERMISSION_CHANGED: return wxS( "Permission Changed" );
        case PLM_AUDIT_EVENT_TYPE::POLICY_UPDATED:    return wxS( "Policy Updated" );
        case PLM_AUDIT_EVENT_TYPE::USER_CREATED:      return wxS( "User Created" );
        case PLM_AUDIT_EVENT_TYPE::USER_DEACTIVATED:  return wxS( "User Deactivated" );
        case PLM_AUDIT_EVENT_TYPE::ERROR_OCCURRED:    return wxS( "Error" );
        case PLM_AUDIT_EVENT_TYPE::WARNING_ISSUED:    return wxS( "Warning" );
        case PLM_AUDIT_EVENT_TYPE::CONFLICT_DETECTED: return wxS( "Conflict Detected" );
        case PLM_AUDIT_EVENT_TYPE::SYNC_STARTED:      return wxS( "Sync Started" );
        case PLM_AUDIT_EVENT_TYPE::SYNC_COMPLETED:    return wxS( "Sync Completed" );
        case PLM_AUDIT_EVENT_TYPE::CACHE_CLEARED:     return wxS( "Cache Cleared" );
        case PLM_AUDIT_EVENT_TYPE::PLUGIN_LOADED:     return wxS( "Plugin Loaded" );
        case PLM_AUDIT_EVENT_TYPE::PLUGIN_UNLOADED:   return wxS( "Plugin Unloaded" );
        case PLM_AUDIT_EVENT_TYPE::REVIEW_SUBMITTED:  return wxS( "Review Submitted" );
        case PLM_AUDIT_EVENT_TYPE::REVIEW_COMMENT_ADDED: return wxS( "Review Comment Added" );
        case PLM_AUDIT_EVENT_TYPE::REVIEW_DECISION:   return wxS( "Review Decision" );
        case PLM_AUDIT_EVENT_TYPE::REVIEW_MERGED:     return wxS( "Review Merged" );
        case PLM_AUDIT_EVENT_TYPE::REVIEW_CLOSED:     return wxS( "Review Closed" );
        case PLM_AUDIT_EVENT_TYPE::REVIEW_REASSIGNED: return wxS( "Review Reassigned" );
        case PLM_AUDIT_EVENT_TYPE::PIPELINE_TRIGGERED: return wxS( "Pipeline Triggered" );
        case PLM_AUDIT_EVENT_TYPE::PIPELINE_COMPLETED: return wxS( "Pipeline Completed" );
        case PLM_AUDIT_EVENT_TYPE::ARTIFACT_DOWNLOADED: return wxS( "Artifact Downloaded" );
        case PLM_AUDIT_EVENT_TYPE::CUSTOM:            return wxS( "Custom Event" );
        default:                                      return wxS( "Unknown" );
        }
    }

    /**
     * Check if this is an error event.
     * @return True if event represents an error condition.
     */
    bool IsError() const
    {
        return event_type == PLM_AUDIT_EVENT_TYPE::ERROR_OCCURRED ||
               event_type == PLM_AUDIT_EVENT_TYPE::LOGIN_FAILED ||
               event_type == PLM_AUDIT_EVENT_TYPE::LOCK_DENIED ||
               !success;
    }

    /**
     * Check if this is a security-relevant event.
     * @return True if event should be flagged for security review.
     */
    bool IsSecurityRelevant() const
    {
        switch( event_type )
        {
        case PLM_AUDIT_EVENT_TYPE::LOGIN:
        case PLM_AUDIT_EVENT_TYPE::LOGOUT:
        case PLM_AUDIT_EVENT_TYPE::LOGIN_FAILED:
        case PLM_AUDIT_EVENT_TYPE::LOCK_FORCED:
        case PLM_AUDIT_EVENT_TYPE::PERMISSION_CHANGED:
        case PLM_AUDIT_EVENT_TYPE::POLICY_UPDATED:
        case PLM_AUDIT_EVENT_TYPE::USER_CREATED:
        case PLM_AUDIT_EVENT_TYPE::USER_DEACTIVATED:
            return true;
        default:
            return false;
        }
    }
};


/**
 * Filter options for querying audit events.
 */
struct PLM_AUDIT_FILTER
{
    int64_t                            start_time_ms = 0;  ///< Start of time range
    int64_t                            end_time_ms = 0;    ///< End of time range (0 = no limit)
    wxString                           user_id;            ///< Filter by user (empty = all)
    wxString                           project_id;         ///< Filter by project (empty = all)
    std::vector<PLM_AUDIT_EVENT_TYPE>  event_types;        ///< Filter by types (empty = all)
    bool                               errors_only = false; ///< Only show error events
    bool                               security_only = false; ///< Only security-relevant events
    size_t                             limit = 100;        ///< Maximum results to return
    size_t                             offset = 0;         ///< Offset for pagination

    PLM_AUDIT_FILTER() = default;

    /**
     * Create a filter for recent events.
     * @param aHours Number of hours to look back.
     * @return Filter for recent events.
     */
    static PLM_AUDIT_FILTER RecentEvents( int aHours = 24 )
    {
        PLM_AUDIT_FILTER filter;
        auto now = std::chrono::system_clock::now();
        auto start = now - std::chrono::hours( aHours );
        filter.start_time_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>( start.time_since_epoch() )
                        .count();
        return filter;
    }

    /**
     * Create a filter for a specific user's activity.
     * @param aUserId User to filter by.
     * @return Filter for user's events.
     */
    static PLM_AUDIT_FILTER ForUser( const wxString& aUserId )
    {
        PLM_AUDIT_FILTER filter;
        filter.user_id = aUserId;
        return filter;
    }

    /**
     * Create a filter for a specific project's history.
     * @param aProjectId Project to filter by.
     * @return Filter for project's events.
     */
    static PLM_AUDIT_FILTER ForProject( const wxString& aProjectId )
    {
        PLM_AUDIT_FILTER filter;
        filter.project_id = aProjectId;
        return filter;
    }
};


/**
 * Result of an audit log query with pagination support.
 */
struct PLM_AUDIT_QUERY_RESULT
{
    std::vector<PLM_AUDIT_EVENT> events;    ///< Matching audit events
    size_t   total_count = 0;               ///< Total matching events (for pagination)
    bool     has_more = false;              ///< True if more results available
    PLM_ERROR_CODE error = PLM_ERROR_CODE::SUCCESS; ///< Error code if query failed
    wxString error_message;                 ///< Error description if query failed

    PLM_AUDIT_QUERY_RESULT() = default;

    bool IsSuccess() const { return error == PLM_ERROR_CODE::SUCCESS; }
};


#endif // PLM_AUDIT_H
