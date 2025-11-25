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

#ifndef DIALOG_PLM_CONFLICT_H
#define DIALOG_PLM_CONFLICT_H

#include "dialog_plm_conflict_base.h"
#include <wx/string.h>
#include <vector>


/**
 * Resolution choice for a conflict.
 */
enum class PLM_CONFLICT_RESOLUTION
{
    USE_LOCAL,          ///< Keep local version
    USE_SERVER,         ///< Use server version
    MERGE,              ///< Merge both versions
    UNRESOLVED          ///< Not yet resolved
};


/**
 * Merge action for a specific component change in conflict resolution dialog.
 *
 * Note: This is distinct from PLM_MERGE_ACTION in plm_merge_algorithm.h which represents
 * merge algorithm actions. This enum is specifically for the UI conflict resolution workflow.
 */
enum class PLM_CONFLICT_MERGE_ACTION
{
    INCLUDE,            ///< Include this change in merge
    EXCLUDE,            ///< Exclude this change from merge
    UNDECIDED           ///< Not yet decided
};


/**
 * Represents a component-level change within a conflict for the conflict resolution dialog.
 *
 * Note: This is distinct from PLM_COMPONENT_CHANGE in plm_diff_analyzer.h which represents
 * detected changes during diff analysis. This struct is specifically for UI conflict resolution.
 */
struct PLM_CONFLICT_COMPONENT_CHANGE
{
    wxString                   component_id;       ///< Unique identifier for the component
    wxString                   component_name;     ///< Display name (e.g., "R1", "U3")
    wxString                   change_type;        ///< Type of change (added, removed, modified)
    wxString                   local_value;        ///< Local value/state
    wxString                   server_value;       ///< Server value/state
    PLM_CONFLICT_MERGE_ACTION  action = PLM_CONFLICT_MERGE_ACTION::UNDECIDED;

    PLM_CONFLICT_COMPONENT_CHANGE() = default;

    PLM_CONFLICT_COMPONENT_CHANGE( const wxString& aId, const wxString& aName,
                                    const wxString& aChangeType, const wxString& aLocalVal,
                                    const wxString& aServerVal )
            : component_id( aId ), component_name( aName ), change_type( aChangeType ),
              local_value( aLocalVal ), server_value( aServerVal )
    {
    }
};


/**
 * Information about a single conflicting item.
 */
struct PLM_CONFLICT_ITEM
{
    wxString                            item_name;        ///< Name of the conflicting item
    wxString                            local_state;      ///< Description of local state
    wxString                            server_state;     ///< Description of server state
    wxString                            item_path;        ///< Path or identifier of item
    PLM_CONFLICT_RESOLUTION                     resolution = PLM_CONFLICT_RESOLUTION::UNRESOLVED;
    std::vector<PLM_CONFLICT_COMPONENT_CHANGE>  component_changes;  ///< Component-level changes for merge

    PLM_CONFLICT_ITEM() = default;

    PLM_CONFLICT_ITEM( const wxString& aName, const wxString& aLocalState,
                       const wxString& aServerState, const wxString& aPath = wxEmptyString )
            : item_name( aName ), local_state( aLocalState ),
              server_state( aServerState ), item_path( aPath )
    {
    }
};


/**
 * Dialog for resolving PLM conflicts.
 *
 * Displays a list of conflicting items and allows the user to choose
 * which version to keep for each item. Provides a preview area for
 * future diff visualization.
 *
 * Usage:
 * @code
 * DIALOG_PLM_CONFLICT dlg( parent );
 *
 * // Add conflicts
 * dlg.AddConflict( "Sheet1.kicad_sch", "Modified locally", "Modified on server" );
 * dlg.AddConflict( "Board.kicad_pcb", "Added component", "Changed routing" );
 *
 * if( dlg.ShowModal() == wxID_OK )
 * {
 *     auto resolutions = dlg.GetResolutions();
 *     for( const auto& item : resolutions )
 *     {
 *         // Apply resolution
 *     }
 * }
 * @endcode
 */
class DIALOG_PLM_CONFLICT : public DIALOG_PLM_CONFLICT_BASE
{
public:
    DIALOG_PLM_CONFLICT( wxWindow* parent );
    ~DIALOG_PLM_CONFLICT() override;

    /**
     * Add a conflict to the list.
     *
     * @param aItemName Name of the conflicting item.
     * @param aLocalState Description of local state.
     * @param aServerState Description of server state.
     * @param aItemPath Optional path or identifier for the item.
     */
    void AddConflict( const wxString& aItemName, const wxString& aLocalState,
                      const wxString& aServerState, const wxString& aItemPath = wxEmptyString );

    /**
     * Add component-level changes to an existing conflict for merge UI.
     *
     * @param aConflictIndex Index of the conflict to add changes to.
     * @param aChanges Vector of component changes.
     */
    void AddComponentChanges( int aConflictIndex,
                              const std::vector<PLM_CONFLICT_COMPONENT_CHANGE>& aChanges );

    /**
     * Add a single component change to an existing conflict.
     *
     * @param aConflictIndex Index of the conflict.
     * @param aChange The component change to add.
     */
    void AddComponentChange( int aConflictIndex, const PLM_CONFLICT_COMPONENT_CHANGE& aChange );

    /**
     * Get all conflict items with their resolutions.
     *
     * @return Vector of conflict items.
     */
    std::vector<PLM_CONFLICT_ITEM> GetResolutions() const;

    /**
     * Check if all conflicts have been resolved.
     *
     * @return True if all conflicts have a resolution selected.
     */
    bool AllConflictsResolved() const;

    /**
     * Get the merged result for a specific conflict based on cherry-picked changes.
     *
     * @param aConflictIndex Index of the conflict.
     * @return Vector of component changes that are included in the merge.
     */
    std::vector<PLM_CONFLICT_COMPONENT_CHANGE> GetMergeResult( int aConflictIndex ) const;

    /**
     * Toggle a component change's merge action.
     *
     * @param aConflictIndex Index of the conflict.
     * @param aChangeIndex Index of the component change.
     * @param aInclude True to include in merge, false to exclude.
     */
    void SetComponentMergeAction( int aConflictIndex, int aChangeIndex, bool aInclude );

    /**
     * Enable merge mode for a specific conflict.
     * Sets resolution to MERGE and allows component-level cherry-picking.
     *
     * @param aConflictIndex Index of the conflict.
     */
    void EnableMergeMode( int aConflictIndex );

protected:
    void onConflictSelected( wxDataViewEvent& event ) override;

private:
    /**
     * Initialize the conflict list columns.
     */
    void initConflictList();

    /**
     * Update the preview panel with diff information.
     *
     * @param aIndex Index of the selected conflict.
     */
    void updatePreview( int aIndex );

    /**
     * Update the merge preview showing selected component changes.
     *
     * @param aIndex Index of the selected conflict.
     */
    void updateMergePreview( int aIndex );

    /**
     * Format component changes as preview text.
     *
     * @param aChanges Vector of component changes.
     * @param aShowSelection True to show include/exclude status.
     * @return Formatted text for display.
     */
    wxString formatComponentChanges( const std::vector<PLM_CONFLICT_COMPONENT_CHANGE>& aChanges,
                                     bool aShowSelection ) const;

    /**
     * Validate that all conflicts have resolutions.
     */
    bool TransferDataFromWindow() override;

    std::vector<PLM_CONFLICT_ITEM> m_conflicts;

    static constexpr int COL_ITEM = 0;
    static constexpr int COL_LOCAL = 1;
    static constexpr int COL_SERVER = 2;
    static constexpr int COL_RESOLUTION = 3;
};

#endif // DIALOG_PLM_CONFLICT_H
