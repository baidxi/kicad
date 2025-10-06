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
 * @file plm_lifecycle.h
 * @brief Lifecycle state transition structures for the PLM client interface.
 *
 * This header defines structures for managing formal lifecycle state
 * transitions, including preconditions, valid transitions, and
 * transition results with approval routing support.
 */

#ifndef PLM_LIFECYCLE_H
#define PLM_LIFECYCLE_H

#include "plm_types.h"
#include "plm_state_mapper.h"

#include <wx/string.h>
#include <vector>


/**
 * A precondition that must be satisfied before a state transition.
 */
struct PLM_TRANSITION_PRECONDITION
{
    wxString    precondition_id;    ///< Unique identifier
    wxString    description;        ///< Human-readable description of the requirement
    bool        satisfied = false;  ///< Whether this precondition is currently met
    bool        blocking = true;    ///< If true, prevents transition when not satisfied

    PLM_TRANSITION_PRECONDITION() = default;

    PLM_TRANSITION_PRECONDITION( const wxString& aId, const wxString& aDescription,
                                 bool aSatisfied = false, bool aBlocking = true ) :
            precondition_id( aId ),
            description( aDescription ),
            satisfied( aSatisfied ),
            blocking( aBlocking )
    {
    }
};


/**
 * A valid state transition with its preconditions.
 *
 * Describes a transition that is available from the current state,
 * including what preconditions must be met and whether the transition
 * requires approval.
 */
struct PLM_VALID_TRANSITION
{
    wxString            target_state;       ///< Native state name of the target
    PLM_STATE_CATEGORY  target_category;    ///< Category of the target state
    wxString            display_name;       ///< Human-readable transition label
    wxString            description;        ///< Help text for the transition
    bool                requires_approval = false; ///< Whether an approval is needed
    bool                requires_comment = false;  ///< Whether a comment is mandatory
    std::vector<PLM_TRANSITION_PRECONDITION> preconditions;

    PLM_VALID_TRANSITION() : target_category( PLM_STATE_CATEGORY::EDITABLE ) {}

    PLM_VALID_TRANSITION( const wxString& aTargetState, PLM_STATE_CATEGORY aCategory,
                          const wxString& aDisplayName ) :
            target_state( aTargetState ),
            target_category( aCategory ),
            display_name( aDisplayName )
    {
    }

    /**
     * Check if all blocking preconditions are satisfied.
     */
    bool CanTransition() const
    {
        for( const auto& pre : preconditions )
        {
            if( pre.blocking && !pre.satisfied )
                return false;
        }

        return true;
    }

    /**
     * Get unsatisfied blocking preconditions.
     */
    std::vector<PLM_TRANSITION_PRECONDITION> GetBlockers() const
    {
        std::vector<PLM_TRANSITION_PRECONDITION> blockers;

        for( const auto& pre : preconditions )
        {
            if( pre.blocking && !pre.satisfied )
                blockers.push_back( pre );
        }

        return blockers;
    }
};


/**
 * Result of a ChangeState() operation.
 */
struct PLM_STATE_TRANSITION_RESULT
{
    bool            success = false;
    PLM_ERROR_CODE  error_code = PLM_ERROR_CODE::SUCCESS;
    wxString        error_message;
    wxString        new_state;          ///< Native state after transition
    PLM_STATE_CATEGORY new_category;    ///< Category after transition
    wxString        pending_approval_id; ///< Review/approval ID if routed to approval
    std::vector<PLM_TRANSITION_PRECONDITION> failed_preconditions;

    PLM_STATE_TRANSITION_RESULT() : new_category( PLM_STATE_CATEGORY::EDITABLE ) {}

    static PLM_STATE_TRANSITION_RESULT Success( const wxString& aNewState,
                                                PLM_STATE_CATEGORY aCategory )
    {
        PLM_STATE_TRANSITION_RESULT r;
        r.success = true;
        r.new_state = aNewState;
        r.new_category = aCategory;
        return r;
    }

    static PLM_STATE_TRANSITION_RESULT Error( PLM_ERROR_CODE aCode, const wxString& aMessage )
    {
        PLM_STATE_TRANSITION_RESULT r;
        r.error_code = aCode;
        r.error_message = aMessage;
        return r;
    }

    static PLM_STATE_TRANSITION_RESULT PendingApproval( const wxString& aApprovalId )
    {
        PLM_STATE_TRANSITION_RESULT r;
        r.success = true;
        r.pending_approval_id = aApprovalId;
        return r;
    }
};


#endif // PLM_LIFECYCLE_H
