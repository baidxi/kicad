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
 * @file plm_revision.h
 * @brief Revision history structures for the PLM client interface.
 * @ingroup plm_revision
 *
 * This header defines structures for tracking and comparing revisions
 * of files in the PLM system.
 */

#ifndef PLM_REVISION_H
#define PLM_REVISION_H

#include "plm_types.h"

#include <wx/string.h>
#include <cstdint>
#include <vector>


/**
 * Represents a single revision of a file in the PLM system.
 *
 * Contains all metadata about a specific revision including author,
 * timestamp, comment, and lifecycle state.
 */
struct PLM_REVISION
{
    wxString           revision_id;         ///< Unique revision identifier
    wxString           revision_number;     ///< Human-readable revision number (e.g., "1.2", "A")
    wxString           author;              ///< Username who created this revision
    int64_t            timestamp = 0;       ///< Creation time (Unix epoch seconds)
    wxString           comment;             ///< Revision comment/description
    wxString           native_state;        ///< PLM-specific state at time of revision
    PLM_STATE_CATEGORY state_category;      ///< Abstracted lifecycle state

    PLM_REVISION() : state_category( PLM_STATE_CATEGORY::EDITABLE ) {}

    PLM_REVISION( const wxString& aRevisionId, const wxString& aRevisionNumber,
                  const wxString& aAuthor, int64_t aTimestamp, const wxString& aComment,
                  PLM_STATE_CATEGORY aStateCategory,
                  const wxString& aNativeState = wxEmptyString ) :
            revision_id( aRevisionId ),
            revision_number( aRevisionNumber ),
            author( aAuthor ),
            timestamp( aTimestamp ),
            comment( aComment ),
            native_state( aNativeState ),
            state_category( aStateCategory )
    {
    }

    /**
     * Check if this revision is in a released state.
     * @return True if the revision has been released.
     */
    bool IsReleased() const { return state_category == PLM_STATE_CATEGORY::RELEASED; }
};


/**
 * Represents a single change between two revisions.
 *
 * Used in revision comparison to describe what changed.
 */
struct PLM_REVISION_CHANGE
{
    PLM_CHANGE_TYPE change_type;    ///< Type of change (added, removed, moved, modified)
    wxString        element_type;   ///< Type of element (symbol, footprint, wire, etc.)
    wxString        element_id;     ///< Identifier of the changed element
    wxString        description;    ///< Human-readable description of the change

    PLM_REVISION_CHANGE() : change_type( PLM_CHANGE_TYPE::MODIFIED ) {}

    PLM_REVISION_CHANGE( PLM_CHANGE_TYPE aType, const wxString& aElementType,
                         const wxString& aElementId, const wxString& aDescription = wxEmptyString ) :
            change_type( aType ),
            element_type( aElementType ),
            element_id( aElementId ),
            description( aDescription )
    {
    }
};


/**
 * Result of comparing two revisions.
 *
 * Contains the base and comparison revisions along with a list of
 * detected changes between them.
 */
struct PLM_REVISION_COMPARE
{
    PLM_REVISION                     base_revision;     ///< The base (older) revision
    PLM_REVISION                     compare_revision;  ///< The revision being compared
    std::vector<PLM_REVISION_CHANGE> changes;           ///< List of changes between revisions

    PLM_REVISION_COMPARE() = default;

    PLM_REVISION_COMPARE( const PLM_REVISION& aBase, const PLM_REVISION& aCompare ) :
            base_revision( aBase ),
            compare_revision( aCompare )
    {
    }

    /**
     * Get the number of changes detected.
     * @return Number of changes.
     */
    size_t ChangeCount() const { return changes.size(); }

    /**
     * Check if there are any changes between revisions.
     * @return True if revisions differ.
     */
    bool HasChanges() const { return !changes.empty(); }

    /**
     * Count changes of a specific type.
     * @param aType The change type to count.
     * @return Number of changes of that type.
     */
    size_t CountByType( PLM_CHANGE_TYPE aType ) const
    {
        size_t count = 0;

        for( const auto& change : changes )
        {
            if( change.change_type == aType )
                ++count;
        }

        return count;
    }

    /**
     * Get count of added elements.
     * @return Number of additions.
     */
    size_t AddedCount() const { return CountByType( PLM_CHANGE_TYPE::ADDED ); }

    /**
     * Get count of removed elements.
     * @return Number of removals.
     */
    size_t RemovedCount() const { return CountByType( PLM_CHANGE_TYPE::REMOVED ); }

    /**
     * Get count of moved elements.
     * @return Number of moves.
     */
    size_t MovedCount() const { return CountByType( PLM_CHANGE_TYPE::MOVED ); }

    /**
     * Get count of modified elements.
     * @return Number of modifications.
     */
    size_t ModifiedCount() const { return CountByType( PLM_CHANGE_TYPE::MODIFIED ); }
};


/**
 * Result of fetching revision history.
 */
struct PLM_REVISION_HISTORY
{
    wxString                  file_id;      ///< File/project identifier
    std::vector<PLM_REVISION> revisions;    ///< List of revisions (newest first)

    PLM_REVISION_HISTORY() = default;

    explicit PLM_REVISION_HISTORY( const wxString& aFileId ) : file_id( aFileId ) {}

    /**
     * Get the latest (most recent) revision.
     * @return Pointer to latest revision, or nullptr if empty.
     */
    const PLM_REVISION* GetLatest() const
    {
        return revisions.empty() ? nullptr : &revisions.front();
    }

    /**
     * Find a revision by its ID.
     * @param aRevisionId Revision ID to find.
     * @return Pointer to the revision, or nullptr if not found.
     */
    const PLM_REVISION* FindById( const wxString& aRevisionId ) const
    {
        for( const auto& rev : revisions )
        {
            if( rev.revision_id == aRevisionId )
                return &rev;
        }

        return nullptr;
    }

    /**
     * Get the number of revisions in history.
     * @return Revision count.
     */
    size_t Count() const { return revisions.size(); }

    /**
     * Check if history is empty.
     * @return True if no revisions exist.
     */
    bool IsEmpty() const { return revisions.empty(); }
};


#endif // PLM_REVISION_H
