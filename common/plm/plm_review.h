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
 * @file plm_review.h
 * @brief Design review and approval structures for the PLM client interface.
 *
 * This header defines structures for managing design review workflows
 * in the PLM system. Reviews allow multi-discipline peer evaluation of
 * design changes with spatial annotations, threaded comments, and
 * formal approval decisions.
 */

#ifndef PLM_REVIEW_H
#define PLM_REVIEW_H

#include "plm_types.h"

#include <wx/string.h>
#include <cstdint>
#include <vector>


/**
 * Status of a design review.
 */
enum class PLM_REVIEW_STATUS
{
    DRAFT = 0,              ///< Review created but not yet submitted
    OPEN = 1,               ///< Review is active, awaiting reviewer decisions
    CHANGES_REQUESTED = 2,  ///< One or more reviewers requested changes
    APPROVED = 3,           ///< All required reviewers approved
    REJECTED = 4,           ///< Review was rejected
    CLOSED = 5,             ///< Review closed without completion
    MERGED = 6              ///< Review completed and changes merged or promoted
};


/**
 * Formal review decision submitted by a reviewer.
 */
enum class PLM_REVIEW_DECISION
{
    PENDING = 0,                ///< Reviewer has not yet responded
    APPROVE = 1,                ///< Approve the design as-is
    APPROVE_WITH_CONDITIONS = 2,///< Approve contingent on minor changes
    REQUEST_CHANGES = 3,        ///< Design needs modifications before approval
    REJECT = 4,                 ///< Design is fundamentally unsuitable
    COMMENT_ONLY = 5            ///< Feedback only, not a formal decision
};


/**
 * Role of a reviewer in a design review.
 */
enum class PLM_REVIEW_ROLE
{
    REVIEWER = 0,       ///< General reviewer, decision is advisory
    REQUIRED = 1,       ///< Required reviewer, must approve before merge
    APPROVER = 2        ///< Final approver with authority to promote
};


/**
 * A reviewer assigned to a design review with their decision state.
 */
struct PLM_REVIEWER
{
    wxString            user_id;
    wxString            display_name;
    PLM_REVIEW_ROLE     role = PLM_REVIEW_ROLE::REVIEWER;
    PLM_REVIEW_DECISION decision = PLM_REVIEW_DECISION::PENDING;
    wxString            comment;
    int64_t             decided_at = 0; ///< Timestamp of decision (0 if pending)

    PLM_REVIEWER() = default;

    PLM_REVIEWER( const wxString& aUserId, const wxString& aDisplayName,
                  PLM_REVIEW_ROLE aRole = PLM_REVIEW_ROLE::REVIEWER ) :
            user_id( aUserId ),
            display_name( aDisplayName ),
            role( aRole )
    {
    }

    bool IsPending() const { return decision == PLM_REVIEW_DECISION::PENDING; }

    bool IsApproval() const
    {
        return decision == PLM_REVIEW_DECISION::APPROVE ||
               decision == PLM_REVIEW_DECISION::APPROVE_WITH_CONDITIONS;
    }

    bool IsRequired() const
    {
        return role == PLM_REVIEW_ROLE::REQUIRED ||
               role == PLM_REVIEW_ROLE::APPROVER;
    }
};


/**
 * A comment or spatial annotation on a design review.
 *
 * Comments can be general (no location), file-specific (file_path set),
 * or spatially anchored to a point on a schematic or PCB (has_position
 * set with coordinates). Threaded replies use parent_comment_id.
 */
struct PLM_REVIEW_COMMENT
{
    wxString    comment_id;
    wxString    review_id;
    wxString    author_id;
    wxString    author_name;
    wxString    body;               ///< Comment text (markdown supported)
    int64_t     created_at = 0;
    int64_t     updated_at = 0;     ///< Last edit timestamp (0 if never edited)

    wxString    parent_comment_id;  ///< Parent comment for threaded replies (empty if top-level)

    // Spatial annotation fields
    wxString    file_path;          ///< Relative path to the design file (empty for general comments)
    wxString    sheet_path;         ///< Schematic sheet path (empty for PCB or general comments)
    double      pos_x = 0.0;       ///< X coordinate in design units
    double      pos_y = 0.0;       ///< Y coordinate in design units
    bool        has_position = false; ///< True if pos_x/pos_y are meaningful

    bool        resolved = false;   ///< True if the comment has been addressed

    PLM_REVIEW_COMMENT() = default;

    bool IsAnnotation() const { return has_position; }
    bool IsReply() const { return !parent_comment_id.IsEmpty(); }
    bool IsGeneral() const { return file_path.IsEmpty() && !has_position; }

    /**
     * Create a general comment not anchored to any design element.
     */
    static PLM_REVIEW_COMMENT General( const wxString& aReviewId,
                                       const wxString& aBody )
    {
        PLM_REVIEW_COMMENT c;
        c.review_id = aReviewId;
        c.body = aBody;
        return c;
    }

    /**
     * Create a spatially-anchored annotation on a design file.
     */
    static PLM_REVIEW_COMMENT Annotation( const wxString& aReviewId,
                                          const wxString& aBody,
                                          const wxString& aFilePath,
                                          double aX, double aY,
                                          const wxString& aSheetPath = wxEmptyString )
    {
        PLM_REVIEW_COMMENT c;
        c.review_id = aReviewId;
        c.body = aBody;
        c.file_path = aFilePath;
        c.sheet_path = aSheetPath;
        c.pos_x = aX;
        c.pos_y = aY;
        c.has_position = true;
        return c;
    }
};


/**
 * A design review tracking peer evaluation of design changes.
 *
 * Reviews are the PLM abstraction over Git pull requests, Odoo ECO
 * approval workflows, and commercial PLM review processes. Each review
 * tracks a set of reviewers, their decisions, and the overall status.
 */
struct PLM_REVIEW
{
    wxString                    review_id;
    wxString                    project_id;     ///< Project being reviewed
    wxString                    revision_id;    ///< Specific revision under review
    wxString                    title;
    wxString                    description;
    PLM_REVIEW_STATUS           status = PLM_REVIEW_STATUS::DRAFT;
    wxString                    submitter_id;
    wxString                    submitter_name;
    std::vector<PLM_REVIEWER>   reviewers;
    int64_t                     created_at = 0;
    int64_t                     updated_at = 0;
    wxString                    source_branch;  ///< Source branch (Git-specific, may be empty)
    wxString                    target_branch;  ///< Target branch (Git-specific, may be empty)
    wxString                    web_url;        ///< URL to view review in web interface

    PLM_REVIEW() = default;

    PLM_REVIEW( const wxString& aReviewId, const wxString& aProjectId,
                const wxString& aTitle ) :
            review_id( aReviewId ),
            project_id( aProjectId ),
            title( aTitle )
    {
    }

    bool IsOpen() const
    {
        return status == PLM_REVIEW_STATUS::OPEN ||
               status == PLM_REVIEW_STATUS::CHANGES_REQUESTED;
    }

    bool IsTerminal() const
    {
        return status == PLM_REVIEW_STATUS::APPROVED ||
               status == PLM_REVIEW_STATUS::REJECTED ||
               status == PLM_REVIEW_STATUS::CLOSED ||
               status == PLM_REVIEW_STATUS::MERGED;
    }

    /**
     * Check if all required reviewers have approved.
     */
    bool AllRequiredApproved() const
    {
        for( const auto& reviewer : reviewers )
        {
            if( reviewer.IsRequired() && !reviewer.IsApproval() )
                return false;
        }

        return true;
    }

    /**
     * Count reviewers who have not yet submitted a decision.
     */
    size_t PendingCount() const
    {
        size_t count = 0;

        for( const auto& reviewer : reviewers )
        {
            if( reviewer.IsPending() )
                ++count;
        }

        return count;
    }

    /**
     * Get human-readable status name.
     */
    wxString GetStatusName() const
    {
        switch( status )
        {
        case PLM_REVIEW_STATUS::DRAFT:              return wxS( "Draft" );
        case PLM_REVIEW_STATUS::OPEN:               return wxS( "Open" );
        case PLM_REVIEW_STATUS::CHANGES_REQUESTED:  return wxS( "Changes Requested" );
        case PLM_REVIEW_STATUS::APPROVED:           return wxS( "Approved" );
        case PLM_REVIEW_STATUS::REJECTED:           return wxS( "Rejected" );
        case PLM_REVIEW_STATUS::CLOSED:             return wxS( "Closed" );
        case PLM_REVIEW_STATUS::MERGED:             return wxS( "Merged" );
        default:                                    return wxS( "Unknown" );
        }
    }

    /**
     * Get human-readable decision name.
     */
    static wxString GetDecisionName( PLM_REVIEW_DECISION aDecision )
    {
        switch( aDecision )
        {
        case PLM_REVIEW_DECISION::PENDING:                return wxS( "Pending" );
        case PLM_REVIEW_DECISION::APPROVE:                return wxS( "Approved" );
        case PLM_REVIEW_DECISION::APPROVE_WITH_CONDITIONS: return wxS( "Approved with Conditions" );
        case PLM_REVIEW_DECISION::REQUEST_CHANGES:        return wxS( "Changes Requested" );
        case PLM_REVIEW_DECISION::REJECT:                 return wxS( "Rejected" );
        case PLM_REVIEW_DECISION::COMMENT_ONLY:           return wxS( "Comment" );
        default:                                          return wxS( "Unknown" );
        }
    }
};


/**
 * Options for submitting a design for review.
 */
struct PLM_REVIEW_SUBMIT_OPTIONS
{
    wxString                    title;
    wxString                    description;
    std::vector<wxString>       reviewer_ids;       ///< User IDs to assign as reviewers
    wxString                    source_branch;      ///< Source branch (Git, auto-detected if empty)
    wxString                    target_branch;      ///< Target branch (Git, defaults to main/master)
    bool                        draft = false;      ///< Create as draft (not yet ready for review)

    PLM_REVIEW_SUBMIT_OPTIONS() = default;

    PLM_REVIEW_SUBMIT_OPTIONS( const wxString& aTitle, const wxString& aDescription ) :
            title( aTitle ),
            description( aDescription )
    {
    }
};


/**
 * Result of a review operation.
 */
struct PLM_REVIEW_RESULT
{
    bool            success = false;
    PLM_ERROR_CODE  error_code = PLM_ERROR_CODE::SUCCESS;
    wxString        error_message;
    PLM_REVIEW      review;

    PLM_REVIEW_RESULT() = default;

    static PLM_REVIEW_RESULT Success( const PLM_REVIEW& aReview )
    {
        PLM_REVIEW_RESULT result;
        result.success = true;
        result.review = aReview;
        return result;
    }

    static PLM_REVIEW_RESULT Failure( PLM_ERROR_CODE aCode, const wxString& aMessage )
    {
        PLM_REVIEW_RESULT result;
        result.error_code = aCode;
        result.error_message = aMessage;
        return result;
    }
};


#endif // PLM_REVIEW_H
