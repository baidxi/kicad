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
 * @file plm_eco.h
 * @brief Engineering Change Order structures for the PLM client interface.
 *
 * This header defines structures for managing ECOs (Engineering Change Orders)
 * in the PLM system. ECOs track and approve changes to released designs,
 * requiring specific documentation and approval workflows.
 */

#ifndef PLM_ECO_H
#define PLM_ECO_H

#include "plm_types.h"

#include <wx/string.h>
#include <cstdint>
#include <vector>
#include <map>


/**
 * Distinguishes change requests from change orders.
 */
enum class PLM_ECO_TYPE
{
    ECR = 0,    ///< Engineering Change Request (proposal phase)
    ECO = 1     ///< Engineering Change Order (execution phase)
};


/**
 * Standard reason categories for initiating a change.
 */
enum class PLM_ECO_REASON
{
    CORRECTIVE = 0,             ///< Fix a defect or failure
    PREVENTIVE = 1,             ///< Prevent a potential issue
    COST_REDUCTION = 2,         ///< Reduce manufacturing cost
    PERFORMANCE_IMPROVEMENT = 3, ///< Improve electrical or mechanical performance
    REGULATORY = 4,             ///< Meet new regulatory requirements
    SUPPLY_CHAIN = 5,           ///< Address component availability
    OTHER = 6                   ///< Freeform reason
};


/**
 * Priority levels for change orders.
 */
enum class PLM_ECO_PRIORITY
{
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};


/**
 * Tracks implementation status of individual items within an ECO.
 */
enum class PLM_ECO_ITEM_STATUS
{
    PENDING = 0,        ///< Not yet started
    CHECKED_OUT = 1,    ///< Design checked out for modification
    IN_WORK = 2,        ///< Actively being modified
    IMPLEMENTED = 3,    ///< Changes complete, ready for review
    VERIFIED = 4,       ///< Changes verified (DRC/ERC passed)
    SKIPPED = 5         ///< Item excluded from this ECO
};


/**
 * Status of an Engineering Change Order.
 */
enum class PLM_ECO_STATUS
{
    DRAFT = 0,          ///< ECO is being prepared
    PENDING_REVIEW = 1, ///< ECO submitted for review
    IN_REVIEW = 2,      ///< ECO is under review
    APPROVED = 3,       ///< ECO has been approved
    REJECTED = 4,       ///< ECO was rejected
    IN_PROGRESS = 5,    ///< ECO implementation in progress
    COMPLETED = 6,      ///< ECO implementation complete
    CANCELLED = 7       ///< ECO was cancelled
};


/**
 * Type of field required for ECO submission.
 */
enum class PLM_ECO_FIELD_TYPE
{
    TEXT = 0,           ///< Free text input
    MULTILINE = 1,      ///< Multi-line text area
    DATE = 2,           ///< Date picker
    DROPDOWN = 3,       ///< Selection from options
    CHECKBOX = 4,       ///< Boolean checkbox
    USER = 5,           ///< User selection
    ATTACHMENT = 6      ///< File attachment
};


/**
 * A required field for ECO submission.
 *
 * Describes a field that must be filled in when checking in
 * changes under an ECO.
 */
struct PLM_ECO_FIELD
{
    wxString              field_id;      ///< Field identifier
    wxString              display_name;  ///< Human-readable name
    wxString              description;   ///< Help text for the field
    PLM_ECO_FIELD_TYPE    field_type = PLM_ECO_FIELD_TYPE::TEXT; ///< Field type
    bool                  required = true; ///< True if field must be filled
    std::vector<wxString> options;       ///< Options for dropdown type
    wxString              default_value; ///< Default value (optional)

    PLM_ECO_FIELD() = default;

    PLM_ECO_FIELD( const wxString& aId, const wxString& aDisplayName,
                   PLM_ECO_FIELD_TYPE aType = PLM_ECO_FIELD_TYPE::TEXT,
                   bool aRequired = true ) :
            field_id( aId ),
            display_name( aDisplayName ),
            field_type( aType ),
            required( aRequired )
    {
    }

    /**
     * Create a required text field.
     */
    static PLM_ECO_FIELD Text( const wxString& aId, const wxString& aDisplayName )
    {
        return PLM_ECO_FIELD( aId, aDisplayName, PLM_ECO_FIELD_TYPE::TEXT, true );
    }

    /**
     * Create a required multiline text field.
     */
    static PLM_ECO_FIELD Multiline( const wxString& aId, const wxString& aDisplayName )
    {
        return PLM_ECO_FIELD( aId, aDisplayName, PLM_ECO_FIELD_TYPE::MULTILINE, true );
    }

    /**
     * Create an optional attachment field.
     */
    static PLM_ECO_FIELD Attachment( const wxString& aId, const wxString& aDisplayName )
    {
        return PLM_ECO_FIELD( aId, aDisplayName, PLM_ECO_FIELD_TYPE::ATTACHMENT, false );
    }
};


/**
 * Impact assessment for an engineering change.
 *
 * Each field holds a free-text description of the impact in that category.
 * Empty strings indicate no impact or not yet assessed.
 */
struct PLM_ECO_IMPACT
{
    wxString cost_impact;
    wxString schedule_impact;
    wxString manufacturing_impact;
    wxString regulatory_impact;
    wxString supply_chain_impact;

    PLM_ECO_IMPACT() = default;

    bool IsEmpty() const
    {
        return cost_impact.IsEmpty() && schedule_impact.IsEmpty()
               && manufacturing_impact.IsEmpty() && regulatory_impact.IsEmpty()
               && supply_chain_impact.IsEmpty();
    }
};


/**
 * A single affected item within an ECO, with per-item tracking.
 */
struct PLM_ECO_ITEM
{
    wxString               item_id;          ///< PLM identifier for the affected item
    wxString               item_name;        ///< Display name (part number or file path)
    wxString               item_type;        ///< Type hint (e.g., "schematic", "pcb", "bom")
    PLM_ECO_ITEM_STATUS    status = PLM_ECO_ITEM_STATUS::PENDING;
    wxString               assigned_to;      ///< User ID of assigned engineer
    wxString               assigned_to_name; ///< Display name of assigned engineer
    wxString               base_revision;    ///< Revision before the change
    wxString               new_revision;     ///< Revision after the change (set on checkin)
    wxString               notes;

    PLM_ECO_ITEM() = default;

    PLM_ECO_ITEM( const wxString& aItemId, const wxString& aItemName,
                  const wxString& aItemType = wxEmptyString ) :
            item_id( aItemId ),
            item_name( aItemName ),
            item_type( aItemType )
    {
    }

    bool IsComplete() const
    {
        return status == PLM_ECO_ITEM_STATUS::IMPLEMENTED
               || status == PLM_ECO_ITEM_STATUS::VERIFIED
               || status == PLM_ECO_ITEM_STATUS::SKIPPED;
    }
};


/**
 * Field values for ECO submission.
 */
using PLM_ECO_FIELD_VALUES = std::map<wxString, wxString>;


/**
 * An Engineering Change Order.
 *
 * Represents an ECO that governs changes to released designs.
 * Changes must be associated with an approved ECO and meet
 * all required field requirements.
 */
struct PLM_ECO
{
    wxString                    eco_id;         ///< Unique ECO identifier
    wxString                    eco_number;     ///< Human-readable ECO number
    wxString                    title;          ///< ECO title/summary
    wxString                    description;    ///< Detailed description
    PLM_ECO_STATUS              status = PLM_ECO_STATUS::DRAFT; ///< Current status
    wxString                    owner_id;       ///< ECO owner user ID
    wxString                    owner_name;     ///< ECO owner display name
    std::vector<PLM_ECO_FIELD>  required_fields; ///< Fields required for checkin
    std::vector<wxString>       affected_projects; ///< Projects affected by ECO
    int64_t                     created_at = 0; ///< Creation timestamp
    int64_t                     due_date = 0;   ///< Target completion date
    wxString                    priority;       ///< Priority level

    // Extended fields for ECO/ECR lifecycle
    PLM_ECO_TYPE                type = PLM_ECO_TYPE::ECR;
    PLM_ECO_REASON              reason = PLM_ECO_REASON::OTHER;
    wxString                    reason_detail;
    PLM_ECO_IMPACT              impact;
    std::vector<PLM_ECO_ITEM>   items;              ///< Per-item tracking
    wxString                    linked_ecr_id;      ///< Parent ECR (for ECOs)
    int64_t                     updated_at = 0;
    int64_t                     approved_at = 0;
    wxString                    approved_by;        ///< Approver user ID
    wxString                    approved_by_name;   ///< Approver display name

    PLM_ECO() = default;

    PLM_ECO( const wxString& aId, const wxString& aNumber, const wxString& aTitle ) :
            eco_id( aId ),
            eco_number( aNumber ),
            title( aTitle )
    {
    }

    /**
     * Check if the ECO is active and can accept changes.
     * @return True if ECO is approved or in progress.
     */
    bool IsActive() const
    {
        return status == PLM_ECO_STATUS::APPROVED ||
               status == PLM_ECO_STATUS::IN_PROGRESS;
    }

    /**
     * Check if the ECO is closed.
     * @return True if ECO is completed, rejected, or cancelled.
     */
    bool IsClosed() const
    {
        return status == PLM_ECO_STATUS::COMPLETED ||
               status == PLM_ECO_STATUS::REJECTED ||
               status == PLM_ECO_STATUS::CANCELLED;
    }

    /**
     * Check if a project is affected by this ECO.
     * @param aProjectId Project ID to check.
     * @return True if project is in affected list.
     */
    bool AffectsProject( const wxString& aProjectId ) const
    {
        for( const auto& id : affected_projects )
        {
            if( id == aProjectId )
                return true;
        }

        return false;
    }

    /**
     * Get human-readable status name.
     * @return Status as display string.
     */
    wxString GetStatusName() const
    {
        switch( status )
        {
        case PLM_ECO_STATUS::DRAFT:          return wxS( "Draft" );
        case PLM_ECO_STATUS::PENDING_REVIEW: return wxS( "Pending Review" );
        case PLM_ECO_STATUS::IN_REVIEW:      return wxS( "In Review" );
        case PLM_ECO_STATUS::APPROVED:       return wxS( "Approved" );
        case PLM_ECO_STATUS::REJECTED:       return wxS( "Rejected" );
        case PLM_ECO_STATUS::IN_PROGRESS:    return wxS( "In Progress" );
        case PLM_ECO_STATUS::COMPLETED:      return wxS( "Completed" );
        case PLM_ECO_STATUS::CANCELLED:      return wxS( "Cancelled" );
        default:                             return wxS( "Unknown" );
        }
    }

    /**
     * Check if all affected items have been completed.
     * @return True if every item is Implemented, Verified, or Skipped.
     */
    bool AllItemsComplete() const
    {
        if( items.empty() )
            return false;

        for( const auto& item : items )
        {
            if( !item.IsComplete() )
                return false;
        }

        return true;
    }

    /**
     * Get human-readable type name.
     */
    wxString GetTypeName() const
    {
        return type == PLM_ECO_TYPE::ECR ? wxS( "Change Request" ) : wxS( "Change Order" );
    }
};


/**
 * Result of ECO validation.
 */
struct PLM_ECO_VALIDATION_RESULT
{
    bool                     valid = true;  ///< True if validation passed
    std::vector<wxString>    errors;        ///< Validation error messages
    std::vector<wxString>    warnings;      ///< Validation warnings
    wxString                 eco_id;        ///< ECO that was validated against

    PLM_ECO_VALIDATION_RESULT() = default;

    /**
     * Add an error to the result.
     * @param aError Error message.
     */
    void AddError( const wxString& aError )
    {
        errors.push_back( aError );
        valid = false;
    }

    /**
     * Add a warning to the result.
     * @param aWarning Warning message.
     */
    void AddWarning( const wxString& aWarning )
    {
        warnings.push_back( aWarning );
    }

    /**
     * Check if there are any issues.
     * @return True if there are errors or warnings.
     */
    bool HasIssues() const
    {
        return !errors.empty() || !warnings.empty();
    }

    /**
     * Create a successful validation result.
     * @param aEcoId ECO that was validated.
     * @return Success result.
     */
    static PLM_ECO_VALIDATION_RESULT Success( const wxString& aEcoId = wxEmptyString )
    {
        PLM_ECO_VALIDATION_RESULT result;
        result.eco_id = aEcoId;
        return result;
    }

    /**
     * Create a failed validation result.
     * @param aError Error message.
     * @return Failure result.
     */
    static PLM_ECO_VALIDATION_RESULT Failure( const wxString& aError )
    {
        PLM_ECO_VALIDATION_RESULT result;
        result.AddError( aError );
        return result;
    }
};


/**
 * Options for creating a new ECO or ECR.
 */
struct PLM_ECO_CREATE_OPTIONS
{
    PLM_ECO_TYPE                type = PLM_ECO_TYPE::ECR;
    wxString                    title;
    wxString                    description;
    PLM_ECO_REASON              reason = PLM_ECO_REASON::OTHER;
    wxString                    reason_detail;
    PLM_ECO_PRIORITY            priority = PLM_ECO_PRIORITY::NORMAL;
    std::vector<wxString>       affected_item_ids;
    PLM_ECO_IMPACT              impact;
    int64_t                     due_date = 0;
    wxString                    linked_ecr_id;      ///< ECR that spawned this ECO
    PLM_ECO_FIELD_VALUES        custom_fields;

    PLM_ECO_CREATE_OPTIONS() = default;
};


/**
 * Result of creating an ECO or ECR.
 */
struct PLM_ECO_CREATE_RESULT
{
    bool           success = false;
    PLM_ERROR_CODE error_code = PLM_ERROR_CODE::SUCCESS;
    wxString       error_message;
    PLM_ECO        eco;

    PLM_ECO_CREATE_RESULT() = default;

    static PLM_ECO_CREATE_RESULT Success( const PLM_ECO& aEco )
    {
        PLM_ECO_CREATE_RESULT result;
        result.success = true;
        result.eco = aEco;
        return result;
    }

    static PLM_ECO_CREATE_RESULT Failure( PLM_ERROR_CODE aCode, const wxString& aMessage )
    {
        PLM_ECO_CREATE_RESULT result;
        result.error_code = aCode;
        result.error_message = aMessage;
        return result;
    }
};


/**
 * Describes a requested state transition on an ECO.
 */
struct PLM_ECO_STATUS_TRANSITION
{
    wxString       eco_id;
    PLM_ECO_STATUS target_status;
    wxString       comment;
    wxString       reviewer_id;     ///< User performing the transition (empty = current user)

    PLM_ECO_STATUS_TRANSITION() : target_status( PLM_ECO_STATUS::DRAFT ) {}

    PLM_ECO_STATUS_TRANSITION( const wxString& aEcoId, PLM_ECO_STATUS aTarget,
                               const wxString& aComment = wxEmptyString ) :
            eco_id( aEcoId ),
            target_status( aTarget ),
            comment( aComment )
    {
    }
};


/**
 * Result of an ECO update or state transition.
 */
struct PLM_ECO_UPDATE_RESULT
{
    bool           success = false;
    PLM_ERROR_CODE error_code = PLM_ERROR_CODE::SUCCESS;
    wxString       error_message;
    PLM_ECO        eco;

    PLM_ECO_UPDATE_RESULT() = default;

    static PLM_ECO_UPDATE_RESULT Success( const PLM_ECO& aEco )
    {
        PLM_ECO_UPDATE_RESULT result;
        result.success = true;
        result.eco = aEco;
        return result;
    }

    static PLM_ECO_UPDATE_RESULT Failure( PLM_ERROR_CODE aCode, const wxString& aMessage )
    {
        PLM_ECO_UPDATE_RESULT result;
        result.error_code = aCode;
        result.error_message = aMessage;
        return result;
    }
};


#endif // PLM_ECO_H
