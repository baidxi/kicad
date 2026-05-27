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
 * @file plm_parts.h
 * @brief Part database structures for the PLM client interface.
 *
 * This header defines structures for managing parts/components in a PLM
 * system, including part information, search options, and BOM validation
 * results. These structures support integration with corporate part databases.
 */

#ifndef PLM_PARTS_H
#define PLM_PARTS_H

#include "plm_types.h"
#include "plm_schema.h"

#include <wx/string.h>
#include <map>
#include <vector>


/**
 * Lifecycle state of a part in the PLM system.
 */
enum class PLM_PART_STATE
{
    ACTIVE = 0,         ///< Part is active and available for use
    PREFERRED = 1,      ///< Part is preferred for new designs
    NOT_RECOMMENDED = 2, ///< Part should not be used in new designs
    OBSOLETE = 3,       ///< Part is obsolete, replacement recommended
    END_OF_LIFE = 4,    ///< Part is end-of-life, no longer manufactured
    DISCONTINUED = 5,   ///< Part has been discontinued
    PENDING = 6,        ///< Part is pending approval
    DRAFT = 7           ///< Part is in draft state
};


/**
 * A single part/component in the PLM database.
 *
 * Represents a component that can be used in designs, with full
 * metadata including lifecycle state and custom attributes.
 */
struct PLM_PART
{
    wxString            part_id;          ///< Unique part identifier in PLM
    wxString            part_number;      ///< Human-readable part number
    wxString            manufacturer;     ///< Part manufacturer
    wxString            manufacturer_pn;  ///< Manufacturer part number
    wxString            description;      ///< Part description
    wxString            category;         ///< Part category/family
    wxString            subcategory;      ///< Part subcategory
    PLM_PART_STATE      state = PLM_PART_STATE::ACTIVE; ///< Lifecycle state
    wxString            replacement_id;   ///< Suggested replacement part ID
    wxString            datasheet_url;    ///< Link to datasheet
    wxString            symbol_ref;       ///< Reference to schematic symbol
    wxString            footprint_ref;    ///< Reference to PCB footprint
    PLM_ATTRIBUTE_SET   attributes;       ///< Custom attributes from schema
    int64_t             created_at = 0;   ///< Creation timestamp
    int64_t             modified_at = 0;  ///< Last modification timestamp

    // Extended fields for compliance and vendor management
    std::vector<struct PLM_COMPLIANCE_ENTRY> compliance;     ///< Regulatory compliance records
    std::vector<struct PLM_VENDOR_SOURCE>    vendor_sources; ///< Approved vendor sources

    PLM_PART() = default;

    PLM_PART( const wxString& aPartId, const wxString& aPartNumber,
              const wxString& aDescription ) :
            part_id( aPartId ),
            part_number( aPartNumber ),
            description( aDescription )
    {
    }

    /**
     * Check if part is available for use in new designs.
     * @return True if part can be used in new designs.
     */
    bool IsAvailable() const
    {
        return state == PLM_PART_STATE::ACTIVE || state == PLM_PART_STATE::PREFERRED;
    }

    /**
     * Check if part is obsolete or discontinued.
     * @return True if part should be replaced.
     */
    bool NeedsReplacement() const
    {
        return state == PLM_PART_STATE::OBSOLETE || state == PLM_PART_STATE::END_OF_LIFE ||
               state == PLM_PART_STATE::DISCONTINUED;
    }

    /**
     * Check if part has a suggested replacement.
     * @return True if replacement_id is set.
     */
    bool HasReplacement() const
    {
        return !replacement_id.IsEmpty();
    }

    /**
     * Get human-readable state name.
     * @return State as display string.
     */
    wxString GetStateName() const
    {
        switch( state )
        {
        case PLM_PART_STATE::ACTIVE:          return wxS( "Active" );
        case PLM_PART_STATE::PREFERRED:       return wxS( "Preferred" );
        case PLM_PART_STATE::NOT_RECOMMENDED: return wxS( "Not Recommended" );
        case PLM_PART_STATE::OBSOLETE:        return wxS( "Obsolete" );
        case PLM_PART_STATE::END_OF_LIFE:     return wxS( "End of Life" );
        case PLM_PART_STATE::DISCONTINUED:    return wxS( "Discontinued" );
        case PLM_PART_STATE::PENDING:         return wxS( "Pending" );
        case PLM_PART_STATE::DRAFT:           return wxS( "Draft" );
        default:                              return wxS( "Unknown" );
        }
    }
};


/**
 * Options for searching the part database.
 */
struct PLM_PART_SEARCH_OPTIONS
{
    wxString                    query;              ///< Search query string
    wxString                    category_filter;    ///< Filter by category
    wxString                    manufacturer_filter; ///< Filter by manufacturer
    std::vector<PLM_PART_STATE> state_filter;       ///< Filter by states
    bool                        include_obsolete = false; ///< Include obsolete parts
    bool                        exact_match = false; ///< Require exact match
    wxString                    sort_field;         ///< Field to sort by
    bool                        sort_ascending = true; ///< Sort direction
    size_t                      offset = 0;         ///< Pagination offset
    size_t                      limit = 50;         ///< Results per page
    int64_t                     modified_since = 0; ///< Only return parts modified after this timestamp

    PLM_PART_SEARCH_OPTIONS() = default;

    explicit PLM_PART_SEARCH_OPTIONS( const wxString& aQuery ) : query( aQuery ) {}

    /**
     * Create search for active parts only.
     * @param aQuery Search query.
     * @return Search options filtered to active parts.
     */
    static PLM_PART_SEARCH_OPTIONS ActiveOnly( const wxString& aQuery )
    {
        PLM_PART_SEARCH_OPTIONS opts( aQuery );
        opts.state_filter.push_back( PLM_PART_STATE::ACTIVE );
        opts.state_filter.push_back( PLM_PART_STATE::PREFERRED );
        return opts;
    }

    /**
     * Create search for parts in a specific category.
     * @param aCategory Category to search.
     * @return Search options filtered to category.
     */
    static PLM_PART_SEARCH_OPTIONS ByCategory( const wxString& aCategory )
    {
        PLM_PART_SEARCH_OPTIONS opts;
        opts.category_filter = aCategory;
        return opts;
    }
};


/**
 * Severity levels for BOM validation issues.
 */
enum class PLM_BOM_ISSUE_SEVERITY
{
    ERROR = 0,      ///< Blocks release; must be resolved
    WARNING = 1,    ///< Should be reviewed; does not block
    INFO = 2        ///< Informational finding
};


/**
 * Enriched BOM entry for validation input.
 *
 * Carries per-component data from the schematic so that the validator
 * can check completeness and map issues back to specific reference
 * designators.
 */
struct PLM_BOM_ENTRY
{
    wxString    reference;          ///< Reference designator (e.g., "R1")
    wxString    part_number;        ///< Part number / IPN / MPN
    wxString    value;              ///< Component value (e.g., "10k")
    wxString    footprint;          ///< Footprint reference
    wxString    manufacturer;       ///< Manufacturer name
    wxString    manufacturer_pn;    ///< Manufacturer part number
    double      quantity = 1.0;
    bool        dnp = false;        ///< Do Not Place

    PLM_BOM_ENTRY() = default;

    PLM_BOM_ENTRY( const wxString& aReference, const wxString& aPartNumber,
                   const wxString& aValue = wxEmptyString,
                   const wxString& aFootprint = wxEmptyString ) :
            reference( aReference ),
            part_number( aPartNumber ),
            value( aValue ),
            footprint( aFootprint )
    {
    }
};


/**
 * Issue found during BOM validation.
 */
struct PLM_BOM_ISSUE
{
    wxString       reference;       ///< Component reference (e.g., "R1", "U3")
    wxString       part_number;     ///< Part number in the BOM
    wxString       issue_type;      ///< Type of issue (missing, obsolete, etc.)
    wxString       message;         ///< Human-readable issue description
    wxString               suggested_part;  ///< Suggested replacement part number
    PLM_PART_STATE         part_state = PLM_PART_STATE::ACTIVE; ///< Current part state
    PLM_BOM_ISSUE_SEVERITY severity = PLM_BOM_ISSUE_SEVERITY::ERROR;

    PLM_BOM_ISSUE() = default;

    PLM_BOM_ISSUE( const wxString& aRef, const wxString& aPartNum, const wxString& aType,
                   const wxString& aMessage ) :
            reference( aRef ),
            part_number( aPartNum ),
            issue_type( aType ),
            message( aMessage )
    {
    }

    /**
     * Create issue for missing part.
     */
    static PLM_BOM_ISSUE Missing( const wxString& aRef, const wxString& aPartNum )
    {
        return PLM_BOM_ISSUE( aRef, aPartNum, wxS( "missing" ),
                              wxString::Format( wxS( "Part %s not found in database" ),
                                                aPartNum ) );
    }

    /**
     * Create issue for obsolete part.
     */
    static PLM_BOM_ISSUE Obsolete( const wxString& aRef, const wxString& aPartNum,
                                   const wxString& aReplacement = wxEmptyString )
    {
        PLM_BOM_ISSUE issue( aRef, aPartNum, wxS( "obsolete" ),
                             wxString::Format( wxS( "Part %s is obsolete" ), aPartNum ) );
        issue.suggested_part = aReplacement;
        issue.part_state = PLM_PART_STATE::OBSOLETE;
        return issue;
    }

    /**
     * Create issue for not recommended part.
     */
    static PLM_BOM_ISSUE NotRecommended( const wxString& aRef, const wxString& aPartNum,
                                         const wxString& aReplacement = wxEmptyString )
    {
        PLM_BOM_ISSUE issue( aRef, aPartNum, wxS( "not_recommended" ),
                             wxString::Format( wxS( "Part %s is not recommended for new designs" ),
                                               aPartNum ) );
        issue.suggested_part = aReplacement;
        issue.part_state = PLM_PART_STATE::NOT_RECOMMENDED;
        issue.severity = PLM_BOM_ISSUE_SEVERITY::WARNING;
        return issue;
    }

    /**
     * Create issue for a part with no approved vendor on file.
     *
     * Produced when no current product.supplierinfo (or equivalent AVL) row
     * is associated with the part. Severity is WARNING because the part may
     * still be sourced ad-hoc; the BOM is buildable but procurement risk is
     * unmanaged.
     */
    static PLM_BOM_ISSUE NoApprovedVendor( const wxString& aRef, const wxString& aPartNum )
    {
        PLM_BOM_ISSUE issue( aRef, aPartNum, wxS( "no_approved_vendor" ),
                             wxString::Format( wxS( "Part %s has no approved vendor" ),
                                               aPartNum ) );
        issue.severity = PLM_BOM_ISSUE_SEVERITY::WARNING;
        return issue;
    }
};


/**
 * Result of validating a BOM against the part database.
 */
struct PLM_BOM_VALIDATION_RESULT
{
    bool                     all_valid = true;       ///< True if no issues found
    std::vector<PLM_BOM_ISSUE> issues;               ///< List of validation issues
    size_t                   total_parts = 0;        ///< Total parts in BOM
    size_t                   valid_parts = 0;        ///< Parts that passed validation
    size_t                   missing_count = 0;      ///< Parts not found in database
    size_t                   obsolete_count = 0;     ///< Obsolete parts
    size_t                   not_recommended_count = 0; ///< Not recommended parts

    PLM_BOM_VALIDATION_RESULT() = default;

    /**
     * Add an issue to the validation result.
     * @param aIssue Issue to add.
     */
    void AddIssue( const PLM_BOM_ISSUE& aIssue )
    {
        issues.push_back( aIssue );
        all_valid = false;

        if( aIssue.issue_type == wxS( "missing" ) )
            ++missing_count;
        else if( aIssue.issue_type == wxS( "obsolete" ) )
            ++obsolete_count;
        else if( aIssue.issue_type == wxS( "not_recommended" ) )
            ++not_recommended_count;
    }

    /**
     * Get all issues of a specific type.
     * @param aType Issue type to filter.
     * @return Vector of matching issues.
     */
    std::vector<PLM_BOM_ISSUE> GetIssuesByType( const wxString& aType ) const
    {
        std::vector<PLM_BOM_ISSUE> result;

        for( const auto& issue : issues )
        {
            if( issue.issue_type == aType )
                result.push_back( issue );
        }

        return result;
    }

    /**
     * Check if there are critical issues (missing or obsolete parts).
     * @return True if there are blocking issues.
     */
    bool HasCriticalIssues() const
    {
        return missing_count > 0 || obsolete_count > 0;
    }

    /**
     * Get validation pass rate as percentage.
     * @return Percentage of parts that passed (0-100).
     */
    double GetPassRate() const
    {
        if( total_parts == 0 )
            return 100.0;

        return ( static_cast<double>( valid_parts ) / static_cast<double>( total_parts ) ) * 100.0;
    }

    /**
     * Create a successful validation result.
     * @param aTotalParts Number of parts validated.
     * @return Result indicating all parts valid.
     */
    static PLM_BOM_VALIDATION_RESULT Success( size_t aTotalParts )
    {
        PLM_BOM_VALIDATION_RESULT result;
        result.all_valid = true;
        result.total_parts = aTotalParts;
        result.valid_parts = aTotalParts;
        return result;
    }
};


/**
 * Regulatory compliance status for a part.
 */
enum class PLM_COMPLIANCE_STATUS
{
    UNKNOWN = 0,        ///< Compliance not yet assessed
    COMPLIANT = 1,      ///< Part meets requirements
    NON_COMPLIANT = 2,  ///< Part does not meet requirements
    EXEMPT = 3,         ///< Part is exempt from this regulation
    PENDING = 4         ///< Assessment is in progress
};


/**
 * A regulatory compliance record for a part.
 */
struct PLM_COMPLIANCE_ENTRY
{
    wxString                regulation;     ///< Regulation name (e.g., "RoHS", "REACH")
    PLM_COMPLIANCE_STATUS   status = PLM_COMPLIANCE_STATUS::UNKNOWN;
    wxString                certificate_id; ///< Certificate or declaration reference
    wxString                notes;
    int64_t                 assessed_at = 0;
    int64_t                 expires_at = 0; ///< 0 = no expiry

    PLM_COMPLIANCE_ENTRY() = default;

    PLM_COMPLIANCE_ENTRY( const wxString& aRegulation, PLM_COMPLIANCE_STATUS aStatus ) :
            regulation( aRegulation ),
            status( aStatus )
    {
    }
};


/**
 * An approved vendor source for a part.
 */
struct PLM_VENDOR_SOURCE
{
    wxString    vendor_id;          ///< Vendor identifier in PLM
    wxString    vendor_name;        ///< Human-readable vendor name
    wxString    vendor_part_number; ///< Vendor's part number
    double      unit_price = 0.0;   ///< Unit price in default currency
    wxString    currency;           ///< Price currency code (e.g., "USD")
    int         lead_time_days = 0; ///< Typical lead time
    int         min_order_qty = 0;  ///< Minimum order quantity
    bool        preferred = false;  ///< True if this is the preferred source
    bool        active = true;

    PLM_VENDOR_SOURCE() = default;

    PLM_VENDOR_SOURCE( const wxString& aVendorId, const wxString& aVendorName,
                       const wxString& aVendorPn ) :
            vendor_id( aVendorId ),
            vendor_name( aVendorName ),
            vendor_part_number( aVendorPn )
    {
    }
};


/**
 * A where-used query result row.
 *
 * Identifies a BOM or assembly that references a given part.
 */
struct PLM_WHERE_USED_ENTRY
{
    wxString    parent_id;      ///< ID of the BOM or assembly
    wxString    parent_name;    ///< Human-readable name
    wxString    parent_type;    ///< Type hint (e.g., "bom", "assembly", "project")
    double      quantity = 0.0; ///< Quantity used in this parent
    wxString    reference;      ///< Reference designator(s) if applicable
    int         level = 0;      ///< Depth in multi-level where-used (0 = direct)

    PLM_WHERE_USED_ENTRY() = default;

    PLM_WHERE_USED_ENTRY( const wxString& aParentId, const wxString& aParentName,
                          double aQuantity ) :
            parent_id( aParentId ),
            parent_name( aParentName ),
            quantity( aQuantity )
    {
    }
};


/**
 * Options for creating a new part in the PLM system.
 */
struct PLM_PART_CREATE_OPTIONS
{
    wxString            part_number;
    wxString            description;
    wxString            category;
    wxString            subcategory;
    wxString            manufacturer;
    wxString            manufacturer_pn;
    PLM_PART_STATE      initial_state = PLM_PART_STATE::DRAFT;
    PLM_ATTRIBUTE_SET   attributes;

    PLM_PART_CREATE_OPTIONS() = default;

    PLM_PART_CREATE_OPTIONS( const wxString& aPartNumber, const wxString& aDescription ) :
            part_number( aPartNumber ),
            description( aDescription )
    {
    }
};


/**
 * Suggested replacement for a part.
 */
struct PLM_PART_REPLACEMENT
{
    wxString original_part_id;    ///< Part being replaced
    wxString replacement_part_id; ///< Suggested replacement
    wxString reason;              ///< Why this replacement is suggested
    bool     form_fit_function = false; ///< True if replacement is FFF compatible
    bool     verified = false;    ///< True if replacement has been verified

    PLM_PART_REPLACEMENT() = default;

    PLM_PART_REPLACEMENT( const wxString& aOriginal, const wxString& aReplacement,
                          const wxString& aReason = wxEmptyString ) :
            original_part_id( aOriginal ),
            replacement_part_id( aReplacement ),
            reason( aReason )
    {
    }
};


#endif // PLM_PARTS_H
