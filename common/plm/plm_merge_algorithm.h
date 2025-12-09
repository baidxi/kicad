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
 * @file plm_merge_algorithm.h
 * @brief Three-way merge algorithm for PLM design reconciliation.
 */

#ifndef PLM_MERGE_ALGORITHM_H
#define PLM_MERGE_ALGORITHM_H

#include <kicommon.h>
#include "plm_diff_analyzer.h"

#include <wx/string.h>

#include <cstdint>
#include <map>
#include <set>
#include <vector>


/**
 * Result of merging a single element.
 */
enum class PLM_MERGE_RESOLUTION
{
    NO_CHANGE,      ///< Element unchanged in all versions
    TAKE_LOCAL,     ///< Use local version
    TAKE_SERVER,    ///< Use server version
    AUTO_MERGED,    ///< Automatically merged without conflict
    CONFLICT,       ///< Conflict requiring manual resolution
    DELETED         ///< Element deleted in result
};


/**
 * Type of merge conflict.
 */
enum class PLM_CONFLICT_TYPE
{
    BOTH_MODIFIED,       ///< Same property modified differently
    LOCAL_MODIFIED_SERVER_DELETED, ///< Modified locally, deleted on server
    LOCAL_DELETED_SERVER_MODIFIED, ///< Deleted locally, modified on server
    BOTH_ADDED_DIFFERENT, ///< Same ref added with different values
    CONNECTIVITY_CONFLICT ///< Net connections conflict
};


/**
 * A single merge action for one element.
 */
struct PLM_MERGE_ACTION
{
    wxString              element_uuid;     ///< UUID of affected element
    wxString              element_ref;      ///< Reference designator if component
    wxString              property_name;    ///< Property being merged
    PLM_MERGE_RESOLUTION  resolution;       ///< How this was resolved
    wxString              base_value;       ///< Value in base version
    wxString              local_value;      ///< Value in local version
    wxString              server_value;     ///< Value in server version
    wxString              merged_value;     ///< Final merged value
    bool                  affects_schematic; ///< Change in schematic
    bool                  affects_pcb;      ///< Change in PCB

    PLM_MERGE_ACTION() :
            resolution( PLM_MERGE_RESOLUTION::NO_CHANGE ),
            affects_schematic( false ),
            affects_pcb( false )
    {
    }

    bool IsConflict() const { return resolution == PLM_MERGE_RESOLUTION::CONFLICT; }
    bool NeedsAction() const { return resolution != PLM_MERGE_RESOLUTION::NO_CHANGE; }
};


/**
 * A conflict requiring user resolution.
 */
struct PLM_MERGE_CONFLICT
{
    wxString            element_uuid;     ///< UUID of conflicting element
    wxString            element_ref;      ///< Reference designator
    wxString            property_name;    ///< Conflicting property
    PLM_CONFLICT_TYPE   conflict_type;    ///< Type of conflict
    wxString            description;      ///< Human-readable description
    wxString            base_value;       ///< Value in base
    wxString            local_value;      ///< Value in local
    wxString            server_value;     ///< Value in server
    PLM_MERGE_RESOLUTION user_choice;     ///< User's resolution choice
    wxString            custom_value;     ///< Custom merged value if provided

    PLM_MERGE_CONFLICT() :
            conflict_type( PLM_CONFLICT_TYPE::BOTH_MODIFIED ),
            user_choice( PLM_MERGE_RESOLUTION::CONFLICT )
    {
    }

    bool IsResolved() const { return user_choice != PLM_MERGE_RESOLUTION::CONFLICT; }
};


/**
 * Complete merge plan with all actions and conflicts.
 */
struct PLM_MERGE_PLAN
{
    std::vector<PLM_MERGE_ACTION>   actions;       ///< All merge actions
    std::vector<PLM_MERGE_CONFLICT> conflicts;     ///< Unresolved conflicts
    size_t                          auto_merged;   ///< Count auto-merged
    size_t                          local_kept;    ///< Count kept from local
    size_t                          server_kept;   ///< Count taken from server
    size_t                          deleted;       ///< Count deleted
    bool                            can_auto_merge; ///< True if no conflicts

    PLM_MERGE_PLAN() : auto_merged( 0 ), local_kept( 0 ), server_kept( 0 ),
                       deleted( 0 ), can_auto_merge( true )
    {
    }

    bool HasConflicts() const { return !conflicts.empty(); }
    size_t GetConflictCount() const { return conflicts.size(); }
    size_t GetActionCount() const { return actions.size(); }

    size_t GetUnresolvedConflictCount() const
    {
        size_t count = 0;

        for( const auto& c : conflicts )
        {
            if( !c.IsResolved() )
                ++count;
        }

        return count;
    }

    bool AllConflictsResolved() const
    {
        for( const auto& c : conflicts )
        {
            if( !c.IsResolved() )
                return false;
        }

        return true;
    }

    std::vector<PLM_MERGE_ACTION> GetActionsForElement( const wxString& aRef ) const
    {
        std::vector<PLM_MERGE_ACTION> result;

        for( const auto& a : actions )
        {
            if( a.element_ref == aRef )
                result.push_back( a );
        }

        return result;
    }
};


/**
 * Configuration for merge algorithm.
 */
struct PLM_MERGE_CONFIG
{
    bool prefer_local = false;        ///< Prefer local changes on conflict
    bool prefer_server = false;       ///< Prefer server changes on conflict
    bool auto_merge_moves = true;     ///< Auto-merge position changes
    bool auto_merge_properties = true; ///< Auto-merge non-critical properties
    bool preserve_local_additions = true; ///< Keep locally added components
    bool preserve_server_additions = true; ///< Keep server added components
};


/**
 * Three-way merge algorithm for PLM designs.
 *
 * Compares base (common ancestor), local (user's version), and server
 * (remote version) to generate a merge plan. Automatically resolves
 * non-conflicting changes and identifies conflicts for user resolution.
 *
 * Usage:
 * @code
 * PLM_MERGE_ALGORITHM merger;
 *
 * // Set the three versions
 * merger.SetBaseFiles( "base.kicad_sch", "base.kicad_pcb" );
 * merger.SetLocalFiles( "local.kicad_sch", "local.kicad_pcb" );
 * merger.SetServerFiles( "server.kicad_sch", "server.kicad_pcb" );
 *
 * // Generate merge plan
 * PLM_MERGE_PLAN plan = merger.GenerateMergePlan();
 *
 * if( plan.HasConflicts() )
 * {
 *     // Present conflicts to user
 *     for( auto& conflict : plan.conflicts )
 *     {
 *         // Get user resolution
 *         conflict.user_choice = GetUserChoice( conflict );
 *     }
 * }
 *
 * // Apply resolved plan
 * merger.ApplyMergePlan( plan, "merged.kicad_sch", "merged.kicad_pcb" );
 * @endcode
 */
class KICOMMON_API PLM_MERGE_ALGORITHM
{
public:
    PLM_MERGE_ALGORITHM();
    ~PLM_MERGE_ALGORITHM();

    /**
     * Set base (common ancestor) files.
     * @param aSchematic Base schematic file.
     * @param aPCB Base PCB file (optional).
     */
    void SetBaseFiles( const wxString& aSchematic, const wxString& aPCB = wxEmptyString );

    /**
     * Set local (user's) files.
     * @param aSchematic Local schematic file.
     * @param aPCB Local PCB file (optional).
     */
    void SetLocalFiles( const wxString& aSchematic, const wxString& aPCB = wxEmptyString );

    /**
     * Set server (remote) files.
     * @param aSchematic Server schematic file.
     * @param aPCB Server PCB file (optional).
     */
    void SetServerFiles( const wxString& aSchematic, const wxString& aPCB = wxEmptyString );

    /**
     * Set merge configuration.
     * @param aConfig Configuration options.
     */
    void SetConfig( const PLM_MERGE_CONFIG& aConfig ) { m_config = aConfig; }

    /**
     * Get current configuration.
     * @return Configuration options.
     */
    const PLM_MERGE_CONFIG& GetConfig() const { return m_config; }

    /**
     * Generate a merge plan.
     *
     * Analyzes all three versions and creates a plan for merging them.
     *
     * @return Merge plan with actions and conflicts.
     */
    PLM_MERGE_PLAN GenerateMergePlan();

    /**
     * Check if merge can proceed automatically.
     *
     * @return True if no conflicts exist.
     */
    bool CanAutoMerge();

    /**
     * Resolve a conflict with a specific choice.
     *
     * @param aPlan Merge plan to update.
     * @param aConflictIndex Index of conflict to resolve.
     * @param aResolution Resolution choice.
     * @param aCustomValue Custom value if resolution is custom.
     * @return True if conflict was resolved.
     */
    bool ResolveConflict( PLM_MERGE_PLAN& aPlan, size_t aConflictIndex,
                          PLM_MERGE_RESOLUTION aResolution,
                          const wxString& aCustomValue = wxEmptyString );

    /**
     * Resolve all conflicts with same choice.
     *
     * @param aPlan Merge plan to update.
     * @param aResolution Resolution to apply to all conflicts.
     */
    void ResolveAllConflicts( PLM_MERGE_PLAN& aPlan, PLM_MERGE_RESOLUTION aResolution );

    /**
     * Get human-readable name for resolution type.
     * @param aResolution Resolution type.
     * @return Display name.
     */
    static wxString GetResolutionName( PLM_MERGE_RESOLUTION aResolution );

    /**
     * Get human-readable name for conflict type.
     * @param aType Conflict type.
     * @return Display name.
     */
    static wxString GetConflictTypeName( PLM_CONFLICT_TYPE aType );

    /**
     * Check if last operation had an error.
     * @return True if error occurred.
     */
    bool HasError() const { return !m_lastError.IsEmpty(); }

    /**
     * Get the last error message.
     * @return Error message.
     */
    wxString GetLastError() const { return m_lastError; }

private:
    /**
     * Analyze changes between base and local.
     * @return Analysis result.
     */
    PLM_DIFF_ANALYSIS_RESULT AnalyzeLocalChanges();

    /**
     * Analyze changes between base and server.
     * @return Analysis result.
     */
    PLM_DIFF_ANALYSIS_RESULT AnalyzeServerChanges();

    /**
     * Merge component changes into plan.
     * @param aLocalChanges Local change list.
     * @param aServerChanges Server change list.
     * @param aPlan Plan to populate.
     */
    void MergeComponentChanges( const std::vector<PLM_COMPONENT_CHANGE>& aLocalChanges,
                                const std::vector<PLM_COMPONENT_CHANGE>& aServerChanges,
                                PLM_MERGE_PLAN& aPlan );

    /**
     * Check if two changes conflict.
     * @param aLocal Local change.
     * @param aServer Server change.
     * @return True if changes conflict.
     */
    bool ChangesConflict( const PLM_COMPONENT_CHANGE& aLocal,
                          const PLM_COMPONENT_CHANGE& aServer );

    /**
     * Create a conflict record.
     * @param aLocal Local change.
     * @param aServer Server change.
     * @return Conflict record.
     */
    PLM_MERGE_CONFLICT CreateConflict( const PLM_COMPONENT_CHANGE& aLocal,
                                        const PLM_COMPONENT_CHANGE& aServer );

    /**
     * Create a merge action for auto-resolved change.
     * @param aChange Change to convert.
     * @param aIsLocal True if from local, false if from server.
     * @return Merge action.
     */
    PLM_MERGE_ACTION CreateAutoMergeAction( const PLM_COMPONENT_CHANGE& aChange, bool aIsLocal );

    wxString           m_baseSchematic;
    wxString           m_basePCB;
    wxString           m_localSchematic;
    wxString           m_localPCB;
    wxString           m_serverSchematic;
    wxString           m_serverPCB;
    PLM_MERGE_CONFIG   m_config;
    wxString           m_lastError;
};


#endif // PLM_MERGE_ALGORITHM_H
