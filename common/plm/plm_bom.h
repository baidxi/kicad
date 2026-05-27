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
 * @file plm_bom.h
 * @brief Bill of Materials data model for the PLM client interface.
 *
 * This header defines structures for managing BOMs in the PLM system,
 * including hierarchical BOM lines, BOM types, diff/comparison, and
 * result types for BOM CRUD operations.
 */

#ifndef PLM_BOM_H
#define PLM_BOM_H

#include "plm_types.h"
#include "plm_schema.h"

#include <wx/string.h>
#include <cstdint>
#include <vector>


/**
 * Types of Bill of Materials.
 */
enum class PLM_BOM_TYPE
{
    ENGINEERING = 0,    ///< Design intent from KiCad schematic
    MANUFACTURING = 1,  ///< Production-oriented with assembly sequence and consumables
    SPARE_PARTS = 2,    ///< Service/field replacement BOM
    PHANTOM = 3         ///< Sub-assembly grouping, not independently manufactured
};


/**
 * A single line item within a BOM.
 *
 * Represents one component or sub-assembly. For engineering BOMs, each
 * line maps to a placed component in the schematic. For manufacturing
 * BOMs, lines may include consumables, fixtures, and process materials.
 */
struct PLM_BOM_LINE
{
    wxString    line_id;            ///< Unique line identifier within this BOM
    wxString    part_id;            ///< PLM part ID (foreign key to PLM_PART)
    wxString    part_number;        ///< Human-readable part number
    wxString    description;        ///< Line description
    double      quantity = 1.0;     ///< Quantity required
    wxString    uom;                ///< Unit of measure (e.g., "pcs", "m", "ml")
    wxString    reference;          ///< Reference designators (e.g., "R1,R2,R3")
    wxString    value;              ///< Component value (e.g., "10k", "100nF")
    wxString    footprint;          ///< Footprint reference
    bool        dnp = false;        ///< Do Not Place flag
    int         sequence = 0;       ///< Assembly sequence / sort order
    wxString    operation_id;       ///< Manufacturing operation this line belongs to
    wxString    sub_bom_id;         ///< Child BOM ID for hierarchical BOMs
    PLM_ATTRIBUTE_SET attributes;   ///< Custom attributes

    PLM_BOM_LINE() = default;

    PLM_BOM_LINE( const wxString& aPartNumber, double aQuantity,
                  const wxString& aReference = wxEmptyString ) :
            part_number( aPartNumber ),
            quantity( aQuantity ),
            reference( aReference )
    {
    }
};


/**
 * A complete Bill of Materials.
 *
 * Hierarchical BOM structure where each line can optionally reference
 * a child BOM via sub_bom_id. The top-level BOM is associated with a
 * product/project and carries a type distinguishing EBOM from MBOM.
 */
struct PLM_BOM
{
    wxString                    bom_id;         ///< Unique BOM identifier in PLM
    wxString                    product_id;     ///< Product/project this BOM belongs to
    wxString                    product_name;   ///< Human-readable product name
    PLM_BOM_TYPE                type = PLM_BOM_TYPE::ENGINEERING;
    wxString                    revision;       ///< BOM revision label
    wxString                    source_bom_id;  ///< Parent BOM (EBOM for MBOMs)
    std::vector<PLM_BOM_LINE>   lines;          ///< BOM line items
    int64_t                     created_at = 0;
    int64_t                     modified_at = 0;
    wxString                    created_by;
    wxString                    modified_by;
    PLM_ATTRIBUTE_SET           attributes;     ///< Custom BOM-level attributes

    PLM_BOM() = default;

    PLM_BOM( const wxString& aProductId, PLM_BOM_TYPE aType ) :
            product_id( aProductId ),
            type( aType )
    {
    }

    /**
     * Get the number of distinct parts (excluding DNP).
     */
    size_t GetActiveLineCount() const
    {
        size_t count = 0;

        for( const auto& line : lines )
        {
            if( !line.dnp )
                ++count;
        }

        return count;
    }

    /**
     * Get total component count (sum of quantities, excluding DNP).
     */
    double GetTotalQuantity() const
    {
        double total = 0.0;

        for( const auto& line : lines )
        {
            if( !line.dnp )
                total += line.quantity;
        }

        return total;
    }

    /**
     * Find a line by part number.
     * @return Pointer to matching line, or nullptr.
     */
    const PLM_BOM_LINE* FindLineByPartNumber( const wxString& aPartNumber ) const
    {
        for( const auto& line : lines )
        {
            if( line.part_number == aPartNumber )
                return &line;
        }

        return nullptr;
    }

    /**
     * Check if this BOM has any sub-assemblies.
     */
    bool IsHierarchical() const
    {
        for( const auto& line : lines )
        {
            if( !line.sub_bom_id.IsEmpty() )
                return true;
        }

        return false;
    }

    /**
     * Get human-readable BOM type name.
     */
    wxString GetTypeName() const
    {
        switch( type )
        {
        case PLM_BOM_TYPE::ENGINEERING:    return wxS( "Engineering" );
        case PLM_BOM_TYPE::MANUFACTURING:  return wxS( "Manufacturing" );
        case PLM_BOM_TYPE::SPARE_PARTS:    return wxS( "Spare Parts" );
        case PLM_BOM_TYPE::PHANTOM:        return wxS( "Phantom" );
        default:                           return wxS( "Unknown" );
        }
    }
};


/**
 * Result of a BOM create or update operation.
 */
struct PLM_BOM_RESULT
{
    bool            success = false;
    wxString        bom_id;             ///< ID of the created/updated BOM
    wxString        web_url;            ///< URL to view BOM in PLM web interface
    PLM_ERROR_CODE  error_code = PLM_ERROR_CODE::SUCCESS;
    wxString        error_message;

    PLM_BOM_RESULT() = default;

    static PLM_BOM_RESULT Success( const wxString& aBomId,
                                   const wxString& aWebUrl = wxEmptyString )
    {
        PLM_BOM_RESULT r;
        r.success = true;
        r.bom_id = aBomId;
        r.web_url = aWebUrl;
        return r;
    }

    static PLM_BOM_RESULT Failure( PLM_ERROR_CODE aCode, const wxString& aMessage )
    {
        PLM_BOM_RESULT r;
        r.error_code = aCode;
        r.error_message = aMessage;
        return r;
    }
};


/**
 * Options for BOM diff / comparison.
 */
struct PLM_BOM_DIFF_OPTIONS
{
    bool ignore_sequence = true;      ///< Ignore assembly order differences
    bool ignore_dnp = false;          ///< Exclude DNP lines from comparison
    bool match_by_reference = false;  ///< Match lines by refdes instead of part number

    PLM_BOM_DIFF_OPTIONS() = default;
};


/**
 * Change type for a single BOM line in a diff.
 */
enum class PLM_BOM_DIFF_TYPE
{
    ADDED = 0,
    REMOVED = 1,
    CHANGED = 2,
    UNCHANGED = 3
};


/**
 * A single difference between two BOM versions.
 */
struct PLM_BOM_DIFF_ENTRY
{
    PLM_BOM_DIFF_TYPE   change_type;
    PLM_BOM_LINE        old_line;       ///< Previous state (empty for ADDED)
    PLM_BOM_LINE        new_line;       ///< New state (empty for REMOVED)
    wxString            change_detail;  ///< Human-readable description of what changed

    PLM_BOM_DIFF_ENTRY() : change_type( PLM_BOM_DIFF_TYPE::UNCHANGED ) {}
};


/**
 * Result of comparing two BOMs.
 */
struct PLM_BOM_DIFF_RESULT
{
    std::vector<PLM_BOM_DIFF_ENTRY> entries;
    size_t added_count = 0;
    size_t removed_count = 0;
    size_t changed_count = 0;
    size_t unchanged_count = 0;

    PLM_BOM_DIFF_RESULT() = default;

    bool HasChanges() const { return added_count > 0 || removed_count > 0 || changed_count > 0; }
};


#endif // PLM_BOM_H
