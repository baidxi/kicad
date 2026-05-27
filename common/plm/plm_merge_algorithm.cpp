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

#include "plm_merge_algorithm.h"

#include <wx/log.h>

#include <algorithm>
#include <set>


PLM_MERGE_ALGORITHM::PLM_MERGE_ALGORITHM()
{
}


PLM_MERGE_ALGORITHM::~PLM_MERGE_ALGORITHM()
{
}


void PLM_MERGE_ALGORITHM::SetBaseFiles( const wxString& aSchematic, const wxString& aPCB )
{
    m_baseSchematic = aSchematic;
    m_basePCB = aPCB;
}


void PLM_MERGE_ALGORITHM::SetLocalFiles( const wxString& aSchematic, const wxString& aPCB )
{
    m_localSchematic = aSchematic;
    m_localPCB = aPCB;
}


void PLM_MERGE_ALGORITHM::SetServerFiles( const wxString& aSchematic, const wxString& aPCB )
{
    m_serverSchematic = aSchematic;
    m_serverPCB = aPCB;
}


PLM_MERGE_PLAN PLM_MERGE_ALGORITHM::GenerateMergePlan()
{
    PLM_MERGE_PLAN plan;
    m_lastError.clear();

    // Validate we have required files
    if( m_baseSchematic.IsEmpty() || m_localSchematic.IsEmpty() || m_serverSchematic.IsEmpty() )
    {
        m_lastError = wxS( "Base, local, and server schematic files are required" );
        return plan;
    }

    // Analyze local changes (base -> local)
    PLM_DIFF_ANALYSIS_RESULT localAnalysis = AnalyzeLocalChanges();

    if( !localAnalysis.success )
    {
        m_lastError = wxString::Format( wxS( "Failed to analyze local changes: %s" ),
                                        localAnalysis.error );
        return plan;
    }

    // Analyze server changes (base -> server)
    PLM_DIFF_ANALYSIS_RESULT serverAnalysis = AnalyzeServerChanges();

    if( !serverAnalysis.success )
    {
        m_lastError = wxString::Format( wxS( "Failed to analyze server changes: %s" ),
                                        serverAnalysis.error );
        return plan;
    }

    // Merge the changes
    MergeComponentChanges( localAnalysis.changes, serverAnalysis.changes, plan );

    // Calculate summary
    plan.can_auto_merge = plan.conflicts.empty();

    return plan;
}


bool PLM_MERGE_ALGORITHM::CanAutoMerge()
{
    PLM_MERGE_PLAN plan = GenerateMergePlan();
    return plan.can_auto_merge;
}


bool PLM_MERGE_ALGORITHM::ResolveConflict( PLM_MERGE_PLAN& aPlan, size_t aConflictIndex,
                                            PLM_MERGE_RESOLUTION aResolution,
                                            const wxString& aCustomValue )
{
    if( aConflictIndex >= aPlan.conflicts.size() )
        return false;

    PLM_MERGE_CONFLICT& conflict = aPlan.conflicts[aConflictIndex];
    conflict.user_choice = aResolution;

    if( !aCustomValue.IsEmpty() )
        conflict.custom_value = aCustomValue;

    // Create corresponding action
    PLM_MERGE_ACTION action;
    action.element_uuid = conflict.element_uuid;
    action.element_ref = conflict.element_ref;
    action.property_name = conflict.property_name;
    action.resolution = aResolution;
    action.base_value = conflict.base_value;
    action.local_value = conflict.local_value;
    action.server_value = conflict.server_value;

    switch( aResolution )
    {
    case PLM_MERGE_RESOLUTION::TAKE_LOCAL:
        action.merged_value = conflict.local_value;
        ++aPlan.local_kept;
        break;

    case PLM_MERGE_RESOLUTION::TAKE_SERVER:
        action.merged_value = conflict.server_value;
        ++aPlan.server_kept;
        break;

    case PLM_MERGE_RESOLUTION::AUTO_MERGED:
        action.merged_value = aCustomValue.IsEmpty() ? conflict.local_value : aCustomValue;
        ++aPlan.auto_merged;
        break;

    case PLM_MERGE_RESOLUTION::DELETED:
        action.merged_value = wxEmptyString;
        ++aPlan.deleted;
        break;

    default:
        break;
    }

    aPlan.actions.push_back( action );

    // Update can_auto_merge status
    aPlan.can_auto_merge = aPlan.AllConflictsResolved();

    return true;
}


void PLM_MERGE_ALGORITHM::ResolveAllConflicts( PLM_MERGE_PLAN& aPlan,
                                                PLM_MERGE_RESOLUTION aResolution )
{
    for( size_t i = 0; i < aPlan.conflicts.size(); ++i )
    {
        if( !aPlan.conflicts[i].IsResolved() )
            ResolveConflict( aPlan, i, aResolution );
    }
}


wxString PLM_MERGE_ALGORITHM::GetResolutionName( PLM_MERGE_RESOLUTION aResolution )
{
    switch( aResolution )
    {
    case PLM_MERGE_RESOLUTION::NO_CHANGE:    return wxS( "No Change" );
    case PLM_MERGE_RESOLUTION::TAKE_LOCAL:   return wxS( "Keep Local" );
    case PLM_MERGE_RESOLUTION::TAKE_SERVER:  return wxS( "Take Server" );
    case PLM_MERGE_RESOLUTION::AUTO_MERGED:  return wxS( "Auto Merged" );
    case PLM_MERGE_RESOLUTION::CONFLICT:     return wxS( "Conflict" );
    case PLM_MERGE_RESOLUTION::DELETED:      return wxS( "Deleted" );
    default: return wxS( "Unknown" );
    }
}


wxString PLM_MERGE_ALGORITHM::GetConflictTypeName( PLM_CONFLICT_TYPE aType )
{
    switch( aType )
    {
    case PLM_CONFLICT_TYPE::BOTH_MODIFIED:
        return wxS( "Both Modified" );

    case PLM_CONFLICT_TYPE::LOCAL_MODIFIED_SERVER_DELETED:
        return wxS( "Modified Locally, Deleted on Server" );

    case PLM_CONFLICT_TYPE::LOCAL_DELETED_SERVER_MODIFIED:
        return wxS( "Deleted Locally, Modified on Server" );

    case PLM_CONFLICT_TYPE::BOTH_ADDED_DIFFERENT:
        return wxS( "Added Differently" );

    case PLM_CONFLICT_TYPE::CONNECTIVITY_CONFLICT:
        return wxS( "Connectivity Conflict" );

    default:
        return wxS( "Unknown" );
    }
}


PLM_DIFF_ANALYSIS_RESULT PLM_MERGE_ALGORITHM::AnalyzeLocalChanges()
{
    PLM_DIFF_ANALYZER analyzer;

    analyzer.SetBaseSchematic( m_baseSchematic );
    analyzer.SetNewSchematic( m_localSchematic );

    if( !m_basePCB.IsEmpty() && !m_localPCB.IsEmpty() )
    {
        analyzer.SetBasePCB( m_basePCB );
        analyzer.SetNewPCB( m_localPCB );
    }

    return analyzer.Analyze();
}


PLM_DIFF_ANALYSIS_RESULT PLM_MERGE_ALGORITHM::AnalyzeServerChanges()
{
    PLM_DIFF_ANALYZER analyzer;

    analyzer.SetBaseSchematic( m_baseSchematic );
    analyzer.SetNewSchematic( m_serverSchematic );

    if( !m_basePCB.IsEmpty() && !m_serverPCB.IsEmpty() )
    {
        analyzer.SetBasePCB( m_basePCB );
        analyzer.SetNewPCB( m_serverPCB );
    }

    return analyzer.Analyze();
}


void PLM_MERGE_ALGORITHM::MergeComponentChanges(
        const std::vector<PLM_COMPONENT_CHANGE>& aLocalChanges,
        const std::vector<PLM_COMPONENT_CHANGE>& aServerChanges,
        PLM_MERGE_PLAN& aPlan )
{
    // Build maps by reference for quick lookup
    std::map<wxString, std::vector<const PLM_COMPONENT_CHANGE*>> localByRef;
    std::map<wxString, std::vector<const PLM_COMPONENT_CHANGE*>> serverByRef;

    for( const auto& change : aLocalChanges )
        localByRef[change.reference].push_back( &change );

    for( const auto& change : aServerChanges )
        serverByRef[change.reference].push_back( &change );

    // Collect all affected references
    std::set<wxString> allRefs;

    for( const auto& [ref, changes] : localByRef )
        allRefs.insert( ref );

    for( const auto& [ref, changes] : serverByRef )
        allRefs.insert( ref );

    // Process each reference
    for( const wxString& ref : allRefs )
    {
        auto localIt = localByRef.find( ref );
        auto serverIt = serverByRef.find( ref );

        bool hasLocal = ( localIt != localByRef.end() );
        bool hasServer = ( serverIt != serverByRef.end() );

        if( hasLocal && !hasServer )
        {
            // Only local changes - auto-merge
            for( const auto* change : localIt->second )
            {
                PLM_MERGE_ACTION action = CreateAutoMergeAction( *change, true );
                aPlan.actions.push_back( action );
                ++aPlan.local_kept;
            }
        }
        else if( !hasLocal && hasServer )
        {
            // Only server changes - auto-merge
            for( const auto* change : serverIt->second )
            {
                PLM_MERGE_ACTION action = CreateAutoMergeAction( *change, false );
                aPlan.actions.push_back( action );
                ++aPlan.server_kept;
            }
        }
        else if( hasLocal && hasServer )
        {
            // Both have changes - check for conflicts
            for( const auto* localChange : localIt->second )
            {
                bool foundConflict = false;

                for( const auto* serverChange : serverIt->second )
                {
                    if( ChangesConflict( *localChange, *serverChange ) )
                    {
                        PLM_MERGE_CONFLICT conflict = CreateConflict( *localChange, *serverChange );
                        aPlan.conflicts.push_back( conflict );
                        foundConflict = true;
                        break;
                    }
                }

                if( !foundConflict )
                {
                    // No conflict with server changes - take local
                    PLM_MERGE_ACTION action = CreateAutoMergeAction( *localChange, true );
                    aPlan.actions.push_back( action );
                    ++aPlan.auto_merged;
                }
            }

            // Also process server changes that don't conflict
            for( const auto* serverChange : serverIt->second )
            {
                bool foundConflict = false;

                for( const auto* localChange : localIt->second )
                {
                    if( ChangesConflict( *localChange, *serverChange ) )
                    {
                        foundConflict = true;
                        break;
                    }
                }

                if( !foundConflict )
                {
                    PLM_MERGE_ACTION action = CreateAutoMergeAction( *serverChange, false );
                    aPlan.actions.push_back( action );
                    ++aPlan.auto_merged;
                }
            }
        }
    }
}


bool PLM_MERGE_ALGORITHM::ChangesConflict( const PLM_COMPONENT_CHANGE& aLocal,
                                            const PLM_COMPONENT_CHANGE& aServer )
{
    // Different references can't conflict
    if( aLocal.reference != aServer.reference )
        return false;

    // Check for add/add conflict
    if( aLocal.type == PLM_COMPONENT_CHANGE_TYPE::ADDED &&
        aServer.type == PLM_COMPONENT_CHANGE_TYPE::ADDED )
    {
        return aLocal.new_value != aServer.new_value;
    }

    // Check for modify/delete conflict
    if( aLocal.type == PLM_COMPONENT_CHANGE_TYPE::REMOVED &&
        aServer.type != PLM_COMPONENT_CHANGE_TYPE::REMOVED &&
        aServer.type != PLM_COMPONENT_CHANGE_TYPE::ADDED )
    {
        return true;
    }

    if( aServer.type == PLM_COMPONENT_CHANGE_TYPE::REMOVED &&
        aLocal.type != PLM_COMPONENT_CHANGE_TYPE::REMOVED &&
        aLocal.type != PLM_COMPONENT_CHANGE_TYPE::ADDED )
    {
        return true;
    }

    // Check for same property modified differently
    if( aLocal.type == aServer.type )
    {
        switch( aLocal.type )
        {
        case PLM_COMPONENT_CHANGE_TYPE::VALUE_CHANGED:
        case PLM_COMPONENT_CHANGE_TYPE::FOOTPRINT_CHANGED:
        case PLM_COMPONENT_CHANGE_TYPE::REFERENCE_CHANGED:
        case PLM_COMPONENT_CHANGE_TYPE::PROPERTY_CHANGED:
            return aLocal.new_value != aServer.new_value;

        case PLM_COMPONENT_CHANGE_TYPE::MOVED:
            // Position changes - conflict if both moved to different positions
            return aLocal.new_value != aServer.new_value;

        case PLM_COMPONENT_CHANGE_TYPE::CONNECTIVITY_CHANGED:
            return true;  // Connectivity changes always conflict

        default:
            break;
        }
    }

    return false;
}


PLM_MERGE_CONFLICT PLM_MERGE_ALGORITHM::CreateConflict( const PLM_COMPONENT_CHANGE& aLocal,
                                                          const PLM_COMPONENT_CHANGE& aServer )
{
    PLM_MERGE_CONFLICT conflict;

    conflict.element_ref = aLocal.reference;
    conflict.local_value = aLocal.new_value;
    conflict.server_value = aServer.new_value;
    conflict.base_value = aLocal.old_value;

    // Determine conflict type
    if( aLocal.type == PLM_COMPONENT_CHANGE_TYPE::REMOVED )
    {
        conflict.conflict_type = PLM_CONFLICT_TYPE::LOCAL_DELETED_SERVER_MODIFIED;
        conflict.description = wxString::Format(
                wxS( "%s deleted locally but modified on server" ), aLocal.reference );
    }
    else if( aServer.type == PLM_COMPONENT_CHANGE_TYPE::REMOVED )
    {
        conflict.conflict_type = PLM_CONFLICT_TYPE::LOCAL_MODIFIED_SERVER_DELETED;
        conflict.description = wxString::Format(
                wxS( "%s modified locally but deleted on server" ), aLocal.reference );
    }
    else if( aLocal.type == PLM_COMPONENT_CHANGE_TYPE::ADDED &&
             aServer.type == PLM_COMPONENT_CHANGE_TYPE::ADDED )
    {
        conflict.conflict_type = PLM_CONFLICT_TYPE::BOTH_ADDED_DIFFERENT;
        conflict.description = wxString::Format(
                wxS( "%s added differently in both versions" ), aLocal.reference );
    }
    else if( aLocal.type == PLM_COMPONENT_CHANGE_TYPE::CONNECTIVITY_CHANGED ||
             aServer.type == PLM_COMPONENT_CHANGE_TYPE::CONNECTIVITY_CHANGED )
    {
        conflict.conflict_type = PLM_CONFLICT_TYPE::CONNECTIVITY_CONFLICT;
        conflict.description = wxString::Format(
                wxS( "Connectivity conflict for %s" ), aLocal.reference );
    }
    else
    {
        conflict.conflict_type = PLM_CONFLICT_TYPE::BOTH_MODIFIED;
        conflict.description = wxString::Format(
                wxS( "%s modified differently in both versions" ), aLocal.reference );
    }

    // Set property name based on change type
    switch( aLocal.type )
    {
    case PLM_COMPONENT_CHANGE_TYPE::VALUE_CHANGED:
        conflict.property_name = wxS( "Value" );
        break;

    case PLM_COMPONENT_CHANGE_TYPE::FOOTPRINT_CHANGED:
        conflict.property_name = wxS( "Footprint" );
        break;

    case PLM_COMPONENT_CHANGE_TYPE::REFERENCE_CHANGED:
        conflict.property_name = wxS( "Reference" );
        break;

    case PLM_COMPONENT_CHANGE_TYPE::MOVED:
        conflict.property_name = wxS( "Position" );
        break;

    default:
        conflict.property_name = wxS( "Property" );
        break;
    }

    return conflict;
}


PLM_MERGE_ACTION PLM_MERGE_ALGORITHM::CreateAutoMergeAction( const PLM_COMPONENT_CHANGE& aChange,
                                                               bool aIsLocal )
{
    PLM_MERGE_ACTION action;

    action.element_ref = aChange.reference;
    action.affects_schematic = aChange.affects_schematic;
    action.affects_pcb = aChange.affects_pcb;
    action.base_value = aChange.old_value;

    if( aIsLocal )
    {
        action.resolution = PLM_MERGE_RESOLUTION::TAKE_LOCAL;
        action.local_value = aChange.new_value;
        action.merged_value = aChange.new_value;
    }
    else
    {
        action.resolution = PLM_MERGE_RESOLUTION::TAKE_SERVER;
        action.server_value = aChange.new_value;
        action.merged_value = aChange.new_value;
    }

    // Set property name based on change type
    switch( aChange.type )
    {
    case PLM_COMPONENT_CHANGE_TYPE::ADDED:
        action.property_name = wxS( "Component" );
        break;

    case PLM_COMPONENT_CHANGE_TYPE::REMOVED:
        action.resolution = PLM_MERGE_RESOLUTION::DELETED;
        action.property_name = wxS( "Component" );
        break;

    case PLM_COMPONENT_CHANGE_TYPE::VALUE_CHANGED:
        action.property_name = wxS( "Value" );
        break;

    case PLM_COMPONENT_CHANGE_TYPE::FOOTPRINT_CHANGED:
        action.property_name = wxS( "Footprint" );
        break;

    case PLM_COMPONENT_CHANGE_TYPE::REFERENCE_CHANGED:
        action.property_name = wxS( "Reference" );
        break;

    case PLM_COMPONENT_CHANGE_TYPE::MOVED:
        action.property_name = wxS( "Position" );
        break;

    case PLM_COMPONENT_CHANGE_TYPE::PROPERTY_CHANGED:
        action.property_name = wxS( "Property" );
        break;

    case PLM_COMPONENT_CHANGE_TYPE::CONNECTIVITY_CHANGED:
        action.property_name = wxS( "Connectivity" );
        break;

    default:
        action.property_name = wxS( "Unknown" );
        break;
    }

    return action;
}
