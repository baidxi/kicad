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
 * @file plm_navigation.h
 * @brief Navigation and folder structures for the PLM client interface.
 *
 * This header defines structures for browsing PLM folder hierarchies
 * and handling paginated list results.
 */

#ifndef PLM_NAVIGATION_H
#define PLM_NAVIGATION_H

#include <wx/string.h>
#include <cstdint>
#include <vector>


/**
 * Represents a folder in the PLM system hierarchy.
 *
 * Folders organize projects and other folders in a tree structure.
 * The has_children flag enables lazy loading of the tree.
 */
struct PLM_FOLDER
{
    wxString id;            ///< Unique folder identifier
    wxString name;          ///< Display name of the folder
    wxString parent_id;     ///< Parent folder ID (empty for root folders)
    bool     has_children = false;  ///< True if folder contains subfolders

    PLM_FOLDER() = default;

    PLM_FOLDER( const wxString& aId, const wxString& aName,
                const wxString& aParentId = wxEmptyString, bool aHasChildren = false ) :
            id( aId ),
            name( aName ),
            parent_id( aParentId ),
            has_children( aHasChildren )
    {
    }

    /**
     * Check if this is a root folder (no parent).
     * @return True if this folder has no parent.
     */
    bool IsRoot() const { return parent_id.IsEmpty(); }
};


/**
 * Options for paginated list queries.
 *
 * Controls pagination, sorting, and filtering of list results.
 * Default values provide sensible starting behavior.
 */
struct PLM_LIST_OPTIONS
{
    int32_t  offset = 0;             ///< Number of items to skip (for pagination)
    int32_t  limit = 50;             ///< Maximum number of items to return
    wxString sort_field;             ///< Field name to sort by (empty for default)
    bool     sort_ascending = true;  ///< True for ascending sort, false for descending

    PLM_LIST_OPTIONS() = default;

    PLM_LIST_OPTIONS( int32_t aOffset, int32_t aLimit,
                      const wxString& aSortField = wxEmptyString, bool aAscending = true ) :
            offset( aOffset ),
            limit( aLimit ),
            sort_field( aSortField ),
            sort_ascending( aAscending )
    {
    }

    /**
     * Create options for the first page with given page size.
     * @param aPageSize Number of items per page.
     * @return Options for page 1.
     */
    static PLM_LIST_OPTIONS FirstPage( int32_t aPageSize = 50 )
    {
        return PLM_LIST_OPTIONS( 0, aPageSize );
    }

    /**
     * Create options for the next page based on current options and result count.
     * @param aItemsReturned Number of items returned in the current page.
     * @return Options for the next page.
     */
    PLM_LIST_OPTIONS NextPage( int32_t aItemsReturned ) const
    {
        PLM_LIST_OPTIONS next = *this;
        next.offset = offset + aItemsReturned;
        return next;
    }

    /**
     * Create options for the previous page.
     * @return Options for the previous page, or first page if already at start.
     */
    PLM_LIST_OPTIONS PreviousPage() const
    {
        PLM_LIST_OPTIONS prev = *this;
        prev.offset = ( offset > limit ) ? ( offset - limit ) : 0;
        return prev;
    }
};


/**
 * Result of a paginated list query.
 *
 * Template class that wraps a vector of items along with pagination metadata.
 * Provides helper methods for determining if more pages exist.
 *
 * @tparam T The type of items in the list.
 */
template <typename T>
struct PLM_LIST_RESULT
{
    std::vector<T> items;            ///< Items returned in this page
    int32_t        total_count = 0;  ///< Total number of items matching query
    int32_t        offset = 0;       ///< Offset used for this query
    int32_t        limit = 0;        ///< Limit used for this query

    PLM_LIST_RESULT() = default;

    PLM_LIST_RESULT( std::vector<T> aItems, int32_t aTotalCount,
                     int32_t aOffset = 0, int32_t aLimit = 50 ) :
            items( std::move( aItems ) ),
            total_count( aTotalCount ),
            offset( aOffset ),
            limit( aLimit )
    {
    }

    /**
     * Check if there are more pages after this one.
     * @return True if more items exist beyond this page.
     */
    bool HasNextPage() const
    {
        return ( offset + static_cast<int32_t>( items.size() ) ) < total_count;
    }

    /**
     * Check if there are pages before this one.
     * @return True if this is not the first page.
     */
    bool HasPreviousPage() const { return offset > 0; }

    /**
     * Get the current page number (1-based).
     * @return Current page number.
     */
    int32_t CurrentPage() const
    {
        if( limit <= 0 )
            return 1;

        return ( offset / limit ) + 1;
    }

    /**
     * Get the total number of pages.
     * @return Total page count.
     */
    int32_t TotalPages() const
    {
        if( limit <= 0 )
            return 1;

        return ( total_count + limit - 1 ) / limit;
    }

    /**
     * Check if the result is empty.
     * @return True if no items were returned.
     */
    bool IsEmpty() const { return items.empty(); }

    /**
     * Get the number of items in this page.
     * @return Number of items.
     */
    size_t Count() const { return items.size(); }
};


/**
 * Breadcrumb path element for folder navigation.
 *
 * Used to display the current location in the folder hierarchy
 * and enable navigation to parent folders.
 */
struct PLM_PATH_ELEMENT
{
    wxString id;    ///< Folder ID
    wxString name;  ///< Display name

    PLM_PATH_ELEMENT() = default;

    PLM_PATH_ELEMENT( const wxString& aId, const wxString& aName ) :
            id( aId ),
            name( aName )
    {
    }
};


/**
 * Type alias for a folder path (breadcrumb trail).
 *
 * Ordered from root to current folder.
 */
using PLM_FOLDER_PATH = std::vector<PLM_PATH_ELEMENT>;


/**
 * Type of search field for UI display.
 *
 * Indicates what kind of filter control the UI should display
 * for this search field.
 */
enum class PLM_SEARCH_FIELD_TYPE
{
    TEXT = 0,       ///< Free text input
    DATE = 1,       ///< Date picker
    DATE_RANGE = 2, ///< Date range picker
    DROPDOWN = 3,   ///< Dropdown selection
    CHECKBOX = 4,   ///< Boolean checkbox
    NUMBER = 5,     ///< Numeric input
    USER = 6        ///< User selection (autocomplete)
};


/**
 * Definition of a searchable field.
 *
 * Describes a field that can be used to filter search results.
 * The UI uses this information to build appropriate filter controls.
 */
struct PLM_SEARCH_FIELD
{
    wxString               field_id;      ///< Field identifier for queries
    wxString               display_name;  ///< Human-readable name for UI
    PLM_SEARCH_FIELD_TYPE  field_type = PLM_SEARCH_FIELD_TYPE::TEXT; ///< UI control type
    std::vector<wxString>  options;       ///< Options for dropdown type
    wxString               default_value; ///< Default filter value (optional)
    bool                   required = false; ///< True if filter is required

    PLM_SEARCH_FIELD() = default;

    PLM_SEARCH_FIELD( const wxString& aId, const wxString& aDisplayName,
                      PLM_SEARCH_FIELD_TYPE aType = PLM_SEARCH_FIELD_TYPE::TEXT ) :
            field_id( aId ),
            display_name( aDisplayName ),
            field_type( aType )
    {
    }

    /**
     * Create a text search field.
     * @param aId Field identifier.
     * @param aDisplayName Display name.
     * @return Text search field.
     */
    static PLM_SEARCH_FIELD Text( const wxString& aId, const wxString& aDisplayName )
    {
        return PLM_SEARCH_FIELD( aId, aDisplayName, PLM_SEARCH_FIELD_TYPE::TEXT );
    }

    /**
     * Create a date search field.
     * @param aId Field identifier.
     * @param aDisplayName Display name.
     * @return Date search field.
     */
    static PLM_SEARCH_FIELD Date( const wxString& aId, const wxString& aDisplayName )
    {
        return PLM_SEARCH_FIELD( aId, aDisplayName, PLM_SEARCH_FIELD_TYPE::DATE );
    }

    /**
     * Create a dropdown search field.
     * @param aId Field identifier.
     * @param aDisplayName Display name.
     * @param aOptions Available options.
     * @return Dropdown search field.
     */
    static PLM_SEARCH_FIELD Dropdown( const wxString&              aId,
                                       const wxString&              aDisplayName,
                                       const std::vector<wxString>& aOptions )
    {
        PLM_SEARCH_FIELD field( aId, aDisplayName, PLM_SEARCH_FIELD_TYPE::DROPDOWN );
        field.options = aOptions;
        return field;
    }

    /**
     * Create a user selection search field.
     * @param aId Field identifier.
     * @param aDisplayName Display name.
     * @return User selection field.
     */
    static PLM_SEARCH_FIELD User( const wxString& aId, const wxString& aDisplayName )
    {
        return PLM_SEARCH_FIELD( aId, aDisplayName, PLM_SEARCH_FIELD_TYPE::USER );
    }
};


#endif // PLM_NAVIGATION_H
