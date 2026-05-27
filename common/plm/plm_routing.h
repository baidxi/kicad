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
 * @file plm_routing.h
 * @brief Manufacturing routing and work center structures for the PLM client interface.
 *
 * This header defines structures for managing manufacturing routings,
 * operations, and work centers. Routings define the sequence of
 * manufacturing steps to produce a product.
 */

#ifndef PLM_ROUTING_H
#define PLM_ROUTING_H

#include "plm_types.h"

#include <wx/string.h>
#include <algorithm>
#include <cstdint>
#include <vector>


/**
 * A manufacturing work center where operations are performed.
 *
 * Represents a physical or logical station in the manufacturing process
 * (e.g., SMT line, wave solder, manual assembly bench, test station).
 */
struct PLM_WORKCENTER
{
    wxString    workcenter_id;      ///< Unique identifier
    wxString    name;               ///< Human-readable name (e.g., "SMT Line 1")
    wxString    code;               ///< Short code (e.g., "SMT1")
    wxString    description;
    double      cost_per_hour = 0.0;
    bool        active = true;

    PLM_WORKCENTER() = default;

    PLM_WORKCENTER( const wxString& aId, const wxString& aName ) :
            workcenter_id( aId ),
            name( aName )
    {
    }
};


/**
 * A single manufacturing operation within a routing.
 *
 * Each operation represents a discrete step in the manufacturing
 * process, associated with a work center and having a defined
 * time and sequence.
 */
struct PLM_OPERATION
{
    wxString    operation_id;       ///< Unique identifier
    wxString    name;               ///< Operation name (e.g., "Top-side SMT placement")
    int         sequence = 0;       ///< Execution order
    wxString    workcenter_id;      ///< Work center where this operation runs
    double      duration_minutes = 0.0;  ///< Expected operation duration
    wxString    description;
    wxString    work_instructions;  ///< Reference to work instruction document

    PLM_OPERATION() = default;

    PLM_OPERATION( const wxString& aId, const wxString& aName, int aSequence,
                   const wxString& aWorkcenterId ) :
            operation_id( aId ),
            name( aName ),
            sequence( aSequence ),
            workcenter_id( aWorkcenterId )
    {
    }
};


/**
 * A manufacturing routing defining the sequence of operations
 * to produce a product.
 *
 * Routings are linked to manufacturing BOMs; each BOM line can
 * reference a specific operation within the routing.
 */
struct PLM_ROUTING
{
    wxString                        routing_id;
    wxString                        name;
    wxString                        product_id;     ///< Product this routing applies to
    std::vector<PLM_OPERATION>      operations;
    bool                            active = true;

    PLM_ROUTING() = default;

    PLM_ROUTING( const wxString& aId, const wxString& aName,
                 const wxString& aProductId ) :
            routing_id( aId ),
            name( aName ),
            product_id( aProductId )
    {
    }

    /**
     * Find operation by ID.
     * @return Pointer to operation, or nullptr.
     */
    const PLM_OPERATION* FindOperation( const wxString& aOperationId ) const
    {
        for( const auto& op : operations )
        {
            if( op.operation_id == aOperationId )
                return &op;
        }

        return nullptr;
    }

    /**
     * Get operations sorted by sequence.
     */
    std::vector<const PLM_OPERATION*> GetSortedOperations() const
    {
        std::vector<const PLM_OPERATION*> sorted;
        sorted.reserve( operations.size() );

        for( const auto& op : operations )
            sorted.push_back( &op );

        std::sort( sorted.begin(), sorted.end(),
                   []( const PLM_OPERATION* a, const PLM_OPERATION* b )
                   {
                       return a->sequence < b->sequence;
                   } );

        return sorted;
    }
};


/**
 * Result of a routing query or mutation.
 */
struct PLM_ROUTING_RESULT
{
    bool            success = false;
    wxString        routing_id;
    PLM_ERROR_CODE  error_code = PLM_ERROR_CODE::SUCCESS;
    wxString        error_message;

    PLM_ROUTING_RESULT() = default;

    static PLM_ROUTING_RESULT Success( const wxString& aRoutingId )
    {
        PLM_ROUTING_RESULT r;
        r.success = true;
        r.routing_id = aRoutingId;
        return r;
    }

    static PLM_ROUTING_RESULT Failure( PLM_ERROR_CODE aCode, const wxString& aMessage )
    {
        PLM_ROUTING_RESULT r;
        r.error_code = aCode;
        r.error_message = aMessage;
        return r;
    }
};


#endif // PLM_ROUTING_H
