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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "dialog_symbol_properties.h"

#include <memory>
#include <set>

#include <bitmaps.h>
#include <wx/tooltip.h>
#include <wx/uiaction.h>
#include <grid_tricks.h>
#include <confirm.h>
#include <kiface_base.h>
#include <pin_numbers.h>
#include <string_utils.h>
#include <template_fieldnames.h>
#include <kiplatform/ui.h>
#include <widgets/grid_icon_text_helpers.h>
#include <widgets/grid_text_helpers.h>
#include <widgets/grid_combobox.h>
#include <widgets/std_bitmap_button.h>
#include <settings/settings_manager.h>
#include <sch_collectors.h>
#include <fields_grid_table.h>
#include <sch_edit_frame.h>
#include <sch_reference_list.h>
#include <schematic.h>
#include <sch_commit.h>
#include <sch_sheet_path.h>
#include <tool/tool_manager.h>
#include <tool/actions.h>

#include <dialog_sim_model.h>
#include <panel_embedded_files.h>
#include <panel_symbol_pin_map.h>
#include <sch_symbol.h>
#include <lib_symbol.h>


wxDEFINE_EVENT( SYMBOL_DELAY_FOCUS, wxCommandEvent );
wxDEFINE_EVENT( SYMBOL_DELAY_SELECTION, wxCommandEvent );


enum PIN_TABLE_COL_ORDER
{
    COL_NUMBER,
    COL_BASE_NAME,
    COL_ALT_NAME,
    COL_TYPE,
    COL_SHAPE,

    COL_COUNT       // keep as last
};


enum GROUP_TABLE_COL_ORDER
{
    GRP_COL_ACTIVE,
    GRP_COL_NAME,
    GRP_COL_DESC,
    GRP_COL_PINS,
    GRP_COL_STATUS,

    GRP_COL_COUNT   // keep as last
};


/**
 * One (pin, alternate) membership of a function group, shown as a child row under its group so
 * that a partially-usable peripheral can be configured without hunting through the pin grid.
 */
struct PIN_FUNCTION_GROUP_MEMBER
{
    wxString pinNumber;
    wxString altName;
    wxString subgroup;      ///< Empty when this member is not part of a cardinality-ruled set.
    bool     optional = false;
    bool     assigned = false;   ///< Cached from the pin grid; the pins remain authoritative.
    bool     available = true;   ///< False when another active group is holding the pin.
    wxString blockedBy;

    /// Members governed by a subgroup rule or flagged optional are the user's choice; the rest
    /// are assigned automatically whenever the group is active.
    bool IsUserChoice() const { return optional || !subgroup.IsEmpty(); }
};


struct PIN_FUNCTION_GROUP_ROW
{
    wxString           groupName;
    wxString           description;
    wxString           bus;
    std::set<wxString> pinNumbers;          ///< Required members, claimed when the group is enabled.
    std::set<wxString> optionalPinNumbers;  ///< Optional members, assigned individually by the user.

    /// Subgroup name -> member pin numbers.  A subgroup is satisfied by assigning at least
    /// `subgroupRules[name].min` of its members (default 1), rather than all of them.
    std::map<wxString, std::set<wxString>> subgroupPinNumbers;

    /// Explicitly declared cardinality rules; a subgroup absent here uses the min-1 default.
    std::map<wxString, PIN_FUNCTION_SUBGROUP_RULE> subgroupRules;

    /// Every (pin, alternate) membership, required members first.  Drives the child rows.
    std::vector<PIN_FUNCTION_GROUP_MEMBER> members;

    /// Expander state of this group's row in the grid.
    GROUP_TYPE         flag = GROUP_COLLAPSED;

    bool               active;
    bool               conflicting;
    wxString           conflictDetail;

    /// True when this group has members whose assignment is the user's decision, and therefore
    /// something worth expanding to show.
    bool HasUserChoices() const
    {
        return std::any_of( members.begin(), members.end(),
                            []( const PIN_FUNCTION_GROUP_MEMBER& m )
                            {
                                return m.IsUserChoice();
                            } );
    }

    /// Minimum number of members that must be assigned for @p aSubgroup.
    int SubgroupMinimum( const wxString& aSubgroup ) const
    {
        auto it = subgroupRules.find( aSubgroup );
        return it != subgroupRules.end() ? it->second.min : PIN_FUNCTION_SUBGROUP_RULE{}.min;
    }
};


static wxString collapsePinRange( const std::set<wxString>& aPins )
{
    // Separate numeric and non-numeric pin numbers
    std::vector<long>     numeric;
    std::vector<wxString> nonNumeric;

    for( const wxString& pin : aPins )
    {
        long val;

        if( pin.ToLong( &val ) )
            numeric.push_back( val );
        else
            nonNumeric.push_back( pin );
    }

    std::sort( numeric.begin(), numeric.end() );

    wxString result;

    // Format numeric pins with range collapsing
    size_t i = 0;

    while( i < numeric.size() )
    {
        long rangeStart = numeric[i];
        long rangeEnd = rangeStart;

        while( i + 1 < numeric.size() && numeric[i + 1] == rangeEnd + 1 )
        {
            rangeEnd = numeric[i + 1];
            i++;
        }

        if( !result.IsEmpty() )
            result += wxS( ", " );

        if( rangeEnd - rangeStart >= 2 )
            result += wxString::Format( wxS( "%ld-%ld" ), rangeStart, rangeEnd );
        else if( rangeEnd != rangeStart )
            result += wxString::Format( wxS( "%ld, %ld" ), rangeStart, rangeEnd );
        else
            result += wxString::Format( wxS( "%ld" ), rangeStart );

        i++;
    }

    // Append non-numeric pins
    for( const wxString& pin : nonNumeric )
    {
        if( !result.IsEmpty() )
            result += wxS( ", " );

        result += pin;
    }

    return result;
}


/**
 * Derive the active pin function groups from a set of pin copies.
 *
 * Mirrors SCH_SYMBOL::GetActivePinFunctionGroups(), but runs over the dialog's *edited* pins so
 * that toggles made in this dialog are visible before they are committed to the symbol.
 */
static std::set<wxString> activeGroupsFromPins( const std::vector<SCH_PIN>& aPins )
{
    std::set<wxString> active;

    for( const SCH_PIN& pin : aPins )
    {
        if( pin.GetAlt().IsEmpty() )
            continue;

        const SCH_PIN* libPin = pin.GetLibPin();

        if( !libPin )
            continue;

        const std::map<wxString, SCH_PIN::ALT>& alternates = libPin->GetAlternates();
        auto                                    it = alternates.find( pin.GetAlt() );

        if( it != alternates.end() && !it->second.m_FunctionGroup.IsEmpty() )
            active.insert( it->second.m_FunctionGroup );
    }

    return active;
}


class PIN_FUNCTION_GROUP_DATA_MODEL : public WX_GRID_TABLE_BASE
{
public:
    PIN_FUNCTION_GROUP_DATA_MODEL() :
            m_readOnlyAttr( nullptr ),
            m_boolColAttr( nullptr ),
            m_expanderAttr( nullptr )
    {
    }

    ~PIN_FUNCTION_GROUP_DATA_MODEL()
    {
        if( m_readOnlyAttr )
            m_readOnlyAttr->DecRef();

        if( m_boolColAttr )
            m_boolColAttr->DecRef();

        if( m_expanderAttr )
            m_expanderAttr->DecRef();

        for( wxGridCellAttr* attr : m_checkboxAttrs )
            attr->DecRef();
    }

    /**
     * @param aEditedPins when non-null, the dialog's working copy of the pins.  Group activity is
     *        derived from these rather than from the committed symbol, so a toggle made in this
     *        dialog survives a unit switch (which rebuilds this grid).
     */
    void CollectGroups( SCH_SYMBOL* aSymbol, int aUnit,
                        const std::vector<SCH_PIN>* aEditedPins = nullptr )
    {
        int oldRowCount = (int) m_displayRows.size();

        m_rows.clear();

        if( !aSymbol || !aSymbol->GetLibSymbolRef() )
        {
            m_displayRows.clear();
            NotifyRowCountChanged( oldRowCount );
            return;
        }

        LIB_SYMBOL* libSym = aSymbol->GetLibSymbolRef().get();
        int bodyStyle = aSymbol->GetBodyStyle();

        // Remember the context so RecomputeConflicts() can re-ask the model later, when only a
        // pin assignment has changed and there is no symbol argument to hand.
        m_libSymbol = libSym;
        m_editedPins = aEditedPins;
        m_unit = aUnit;
        m_bodyStyle = bodyStyle;

        struct GROUP_MEMBERS
        {
            std::set<wxString>                        required;
            std::set<wxString>                        optional;
            std::map<wxString, std::set<wxString>>    subgroups;   // subgroup name -> pin numbers
            std::vector<PIN_FUNCTION_GROUP_MEMBER>    all;
        };

        std::map<wxString, GROUP_MEMBERS> groupPins;

        for( const SCH_PIN* libPin : libSym->GetGraphicalPins( aUnit, bodyStyle ) )
        {
            for( const auto& [altName, alt] : libPin->GetAlternates() )
            {
                if( alt.m_FunctionGroup.IsEmpty() )
                    continue;

                GROUP_MEMBERS& members = groupPins[alt.m_FunctionGroup];

                if( !alt.m_FunctionGroupSubgroup.IsEmpty() )
                {
                    members.subgroups[alt.m_FunctionGroupSubgroup].insert( libPin->GetNumber() );
                }
                else if( alt.m_FunctionGroupOptional )
                {
                    members.optional.insert( libPin->GetNumber() );
                }
                else
                {
                    members.required.insert( libPin->GetNumber() );
                }

                // One entry per (pin, alternate) pair, so a pin carrying two alternates of the
                // same group stays individually addressable.
                members.all.push_back( { libPin->GetNumber(), altName,
                                         alt.m_FunctionGroupSubgroup,
                                         alt.m_FunctionGroupOptional, false, true,
                                         wxEmptyString } );
            }
        }

        // Prefer the dialog's edited pins so uncommitted toggles are not lost on a unit switch.
        const std::set<wxString> activeGroups = aEditedPins
                                                        ? activeGroupsFromPins( *aEditedPins )
                                                        : aSymbol->GetActivePinFunctionGroups();
        const auto& groupDescs = libSym->PinFunctionGroupDescriptions();

        for( auto& [name, pins] : groupPins )
        {
            PIN_FUNCTION_GROUP_ROW row;
            row.groupName = name;
            row.pinNumbers = std::move( pins.required );
            row.optionalPinNumbers = std::move( pins.optional );
            row.subgroupPinNumbers = std::move( pins.subgroups );
            row.members = std::move( pins.all );
            row.active = activeGroups.count( name ) > 0;
            row.conflicting = false;

            // Required members first, then by pin number, so the child rows read in a sensible
            // order rather than whatever the alternate-name map iteration produced.
            std::sort( row.members.begin(), row.members.end(),
                       []( const PIN_FUNCTION_GROUP_MEMBER& a,
                           const PIN_FUNCTION_GROUP_MEMBER& b )
                       {
                           if( a.IsUserChoice() != b.IsUserChoice() )
                               return !a.IsUserChoice();

                           if( a.subgroup != b.subgroup )
                               return a.subgroup < b.subgroup;

                           if( int cmp = PIN_NUMBERS::Compare( a.pinNumber, b.pinNumber ) )
                               return cmp < 0;

                           return a.altName < b.altName;
                       } );

            auto it = groupDescs.find( name );

            if( it != groupDescs.end() )
            {
                row.description = it->second.description;
                row.bus = it->second.bus;
                row.subgroupRules = it->second.subgroups;
            }

            m_rows.push_back( std::move( row ) );
        }

        RecomputeConflicts();
        RebuildDisplayRows();
        BuildAttrs();
        NotifyRowCountChanged( oldRowCount );
    }

    /**
     * Refresh each member's cached assigned/available state from the dialog's pins.
     *
     * The pins stay the single source of truth; the child rows only mirror them.  Without this
     * the grid and the pin table could disagree about what is assigned.
     */
    void RefreshMemberAssignments( const std::vector<SCH_PIN>& aPins )
    {
        std::map<wxString, wxString> assigned;   // pin number -> alternate

        for( const SCH_PIN& pin : aPins )
        {
            if( !pin.GetAlt().IsEmpty() )
                assigned[pin.GetNumber()] = pin.GetAlt();
        }

        // Which group is holding each pin, so a blocked member can name its blocker.
        std::map<wxString, wxString> holder;

        for( const PIN_FUNCTION_GROUP_ROW& row : m_rows )
        {
            if( !row.active )
                continue;

            for( const PIN_FUNCTION_GROUP_MEMBER& member : row.members )
            {
                auto it = assigned.find( member.pinNumber );

                if( it != assigned.end() && it->second == member.altName )
                    holder[member.pinNumber] = row.groupName;
            }
        }

        for( PIN_FUNCTION_GROUP_ROW& row : m_rows )
        {
            for( PIN_FUNCTION_GROUP_MEMBER& member : row.members )
            {
                auto it = assigned.find( member.pinNumber );

                member.assigned = ( it != assigned.end() && it->second == member.altName );

                auto holderIt = holder.find( member.pinNumber );
                bool heldByOther = holderIt != holder.end()
                                   && holderIt->second != row.groupName;

                member.available = !heldByOther;
                member.blockedBy = heldByOther ? holderIt->second : wxString();
            }
        }
    }

    /// Flatten m_rows (+ the children of expanded groups) into the grid's row space.
    void RebuildDisplayRows()
    {
        m_displayRows.clear();

        for( int i = 0; i < (int) m_rows.size(); ++i )
        {
            m_displayRows.push_back( { i, -1 } );

            if( m_rows[i].flag != GROUP_EXPANDED )
                continue;

            for( int j = 0; j < (int) m_rows[i].members.size(); ++j )
                m_displayRows.push_back( { i, j } );
        }
    }

    /**
     * Recompute which inactive groups cannot be enabled because an active group is holding a pin
     * they need, or because they share a mutually-exclusive bus.
     *
     * Delegates to LIB_SYMBOL so the rule lives with the model rather than in this dialog, and
     * so that pins held through an *assigned optional or subgroup member* count as claimed --
     * the case the previous required-members-only check missed.
     */
    void RecomputeConflicts()
    {
        for( PIN_FUNCTION_GROUP_ROW& row : m_rows )
        {
            row.conflicting = false;
            row.conflictDetail.clear();
        }

        if( !m_libSymbol )
            return;

        std::set<wxString> activeGroups;

        for( const PIN_FUNCTION_GROUP_ROW& row : m_rows )
        {
            if( row.active )
                activeGroups.insert( row.groupName );
        }

        std::map<wxString, wxString> assignedAlts;

        if( m_editedPins )
        {
            for( const SCH_PIN& pin : *m_editedPins )
            {
                if( !pin.GetAlt().IsEmpty() )
                    assignedAlts[pin.GetNumber()] = pin.GetAlt();
            }
        }

        // Completion state comes from the same core validator that ERC will use, rather than
        // being recomputed here -- two implementations of "is this group complete" would drift.
        m_statuses = m_libSymbol->ValidatePinFunctionGroups( assignedAlts, m_unit, m_bodyStyle );

        const auto conflicts = m_libSymbol->GetConflictingPinFunctionGroups(
                activeGroups, assignedAlts, m_unit, m_bodyStyle );

        for( PIN_FUNCTION_GROUP_ROW& row : m_rows )
        {
            // An active row is never flagged, so it can always be switched back off.
            if( row.active )
                continue;

            auto it = conflicts.find( row.groupName );

            if( it == conflicts.end() )
                continue;

            row.conflicting = true;

            for( const auto& [otherName, sharedPins] : it->second )
            {
                if( !row.conflictDetail.IsEmpty() )
                    row.conflictDetail += wxS( ", " );

                if( sharedPins.empty() )
                {
                    row.conflictDetail += wxString::Format( _( "%s (bus)" ), otherName );
                }
                else
                {
                    row.conflictDetail += wxString::Format( _( "%s (pins %s)" ), otherName,
                                                            collapsePinRange( sharedPins ) );
                }
            }
        }
    }

    void BuildAttrs()
    {
        for( wxGridCellAttr* attr : m_checkboxAttrs )
            attr->DecRef();

        m_checkboxAttrs.clear();

        if( m_readOnlyAttr )
            m_readOnlyAttr->DecRef();

        m_readOnlyAttr = new wxGridCellAttr;
        m_readOnlyAttr->SetReadOnly( true );

        if( m_boolColAttr )
            m_boolColAttr->DecRef();

        m_boolColAttr = new wxGridCellAttr;
        m_boolColAttr->SetRenderer( new wxGridCellBoolRenderer() );
        m_boolColAttr->SetAlignment( wxALIGN_CENTER, wxALIGN_CENTER );

        if( m_expanderAttr )
            m_expanderAttr->DecRef();

        m_expanderAttr = new wxGridCellAttr;
        m_expanderAttr->SetReadOnly( true );
        m_expanderAttr->SetRenderer( new GRID_CELL_TEXT_RENDERER() );

        // One checkbox attr per DISPLAY row, since child rows have their own enabled state.
        for( const GRID_ROW& ref : m_displayRows )
        {
            const PIN_FUNCTION_GROUP_ROW& row = m_rows[ref.groupIndex];

            wxGridCellAttr* attr = new wxGridCellAttr;
            attr->SetRenderer( new wxGridCellBoolRenderer() );
            attr->SetAlignment( wxALIGN_CENTER, wxALIGN_CENTER );

            bool greyed = false;

            if( ref.memberIndex >= 0 )
            {
                const PIN_FUNCTION_GROUP_MEMBER& member = row.members[ref.memberIndex];

                // A member is only the user's to toggle when its group is active, the choice is
                // actually theirs (optional or subgroup), and no other group holds the pin.
                // Required members show as ticked but are implied by the group.
                greyed = !row.active || !member.IsUserChoice() || !member.available;
            }
            else
            {
                greyed = row.conflicting && !row.active;
            }

            if( greyed )
            {
                attr->SetReadOnly( true );
                attr->SetTextColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
            }

            m_checkboxAttrs.push_back( attr );
        }
    }

    int GetNumberRows() override { return (int) m_displayRows.size(); }
    int GetNumberCols() override { return GRP_COL_COUNT; }

    bool IsExpanderColumn( int aCol ) const override { return aCol == GRP_COL_NAME; }

    GROUP_TYPE GetGroupType( int aRow ) const override
    {
        if( aRow < 0 || aRow >= (int) m_displayRows.size() )
            return GROUP_SINGLETON;

        const GRID_ROW& ref = m_displayRows[aRow];

        if( ref.memberIndex >= 0 )
            return CHILD_ITEM;

        // Only groups with something for the user to decide get a disclosure triangle.
        return m_rows[ref.groupIndex].HasUserChoices() ? m_rows[ref.groupIndex].flag
                                                       : GROUP_SINGLETON;
    }

    void ExpandRow( int aRow )
    {
        const GRID_ROW& ref = m_displayRows[aRow];

        if( ref.memberIndex >= 0 )
            return;

        PIN_FUNCTION_GROUP_ROW& group = m_rows[ref.groupIndex];

        if( group.members.empty() )
            return;

        group.flag = GROUP_EXPANDED;
        RebuildDisplayRows();
        BuildAttrs();

        wxGridTableMessage msg( this, wxGRIDTABLE_NOTIFY_ROWS_INSERTED, aRow + 1,
                                (int) group.members.size() );
        GetView()->ProcessTableMessage( msg );
    }

    void CollapseRow( int aRow )
    {
        const GRID_ROW& ref = m_displayRows[aRow];

        if( ref.memberIndex >= 0 )
            return;

        PIN_FUNCTION_GROUP_ROW& group = m_rows[ref.groupIndex];
        int                     removed = (int) group.members.size();

        group.flag = GROUP_COLLAPSED;
        RebuildDisplayRows();
        BuildAttrs();

        wxGridTableMessage msg( this, wxGRIDTABLE_NOTIFY_ROWS_DELETED, aRow + 1, removed );
        GetView()->ProcessTableMessage( msg );
    }

    void ExpandCollapseRow( int aRow )
    {
        if( aRow < 0 || aRow >= (int) m_displayRows.size() )
            return;

        if( GetGroupType( aRow ) == GROUP_COLLAPSED )
            ExpandRow( aRow );
        else if( GetGroupType( aRow ) == GROUP_EXPANDED )
            CollapseRow( aRow );
    }

    /// Expand a group that has user choices, so activating it reveals them in place instead of
    /// leaving the user to find the pins by hand.
    void AutoExpandGroup( const wxString& aGroupName )
    {
        int oldRowCount = (int) m_displayRows.size();

        for( int i = 0; i < (int) m_rows.size(); ++i )
        {
            if( m_rows[i].groupName != aGroupName )
                continue;

            if( m_rows[i].active && m_rows[i].HasUserChoices() )
                m_rows[i].flag = GROUP_EXPANDED;
            else if( !m_rows[i].active )
                m_rows[i].flag = GROUP_COLLAPSED;
        }

        RebuildDisplayRows();
        BuildAttrs();
        NotifyRowCountChanged( oldRowCount );
    }

    /**
     * Tell the grid its row count moved.
     *
     * wxGrid caches the row count independently, so changing m_displayRows without a table
     * message leaves it indexing rows that no longer exist.  The caller ForceRefresh()es
     * afterwards, so the (row, count) here only has to keep the total honest.
     */
    void NotifyRowCountChanged( int aOldRowCount )
    {
        int newRowCount = (int) m_displayRows.size();

        if( newRowCount == aOldRowCount || !GetView() )
            return;

        if( newRowCount > aOldRowCount )
        {
            wxGridTableMessage msg( this, wxGRIDTABLE_NOTIFY_ROWS_INSERTED, aOldRowCount,
                                    newRowCount - aOldRowCount );
            GetView()->ProcessTableMessage( msg );
        }
        else
        {
            wxGridTableMessage msg( this, wxGRIDTABLE_NOTIFY_ROWS_DELETED, newRowCount,
                                    aOldRowCount - newRowCount );
            GetView()->ProcessTableMessage( msg );
        }
    }

    wxString GetColLabelValue( int aCol ) override
    {
        switch( aCol )
        {
        case GRP_COL_ACTIVE: return _( "Active" );
        case GRP_COL_NAME:   return _( "Function Group" );
        case GRP_COL_DESC:   return _( "Description" );
        case GRP_COL_PINS:   return _( "Pins" );
        case GRP_COL_STATUS: return _( "Status" );
        default: wxFAIL;     return wxEmptyString;
        }
    }

    bool IsEmptyCell( int aRow, int aCol ) override { return false; }

    wxString GetValue( int aRow, int aCol ) override
    {
        const GRID_ROW&               ref = m_displayRows[aRow];
        const PIN_FUNCTION_GROUP_ROW& row = m_rows[ref.groupIndex];

        if( ref.memberIndex >= 0 )
            return memberValue( row, row.members[ref.memberIndex], aCol );

        switch( aCol )
        {
        case GRP_COL_ACTIVE:
            return row.active ? wxS( "1" ) : wxS( "" );

        case GRP_COL_NAME:
            return row.groupName;

        case GRP_COL_DESC:
            return row.description;

        case GRP_COL_PINS:
        {
            wxString pins = collapsePinRange( row.pinNumbers );

            // Subgroups read as "≥1 of: 37, 39, 56, 58" -- the count is the point, and the
            // individual choice lives in the child rows.
            for( const auto& [subgroupName, memberPins] : row.subgroupPinNumbers )
            {
                pins += wxString::Format( _( " (%s: %d+ of %s)" ), subgroupName,
                                          row.SubgroupMinimum( subgroupName ),
                                          collapsePinRange( memberPins ) );
            }

            if( !row.optionalPinNumbers.empty() )
            {
                pins += wxString::Format( _( " (optional: %s)" ),
                                          collapsePinRange( row.optionalPinNumbers ) );
            }

            return pins;
        }

        case GRP_COL_STATUS:
            return groupStatus( row );

        default:
            wxFAIL;
            return wxEmptyString;
        }
    }

    /// Human-readable state of a group row, naming what is actually missing.
    wxString groupStatus( const PIN_FUNCTION_GROUP_ROW& aRow ) const
    {
        if( !aRow.active )
        {
            return aRow.conflicting
                           ? wxString::Format( _( "Conflicts: %s" ), aRow.conflictDetail )
                           : _( "Available" );
        }

        // What is missing (unmet subgroup minimum, or an unassigned required member) comes from
        // the core validator, so the dialog and ERC always agree on what "complete" means.
        auto statusIt = m_statuses.find( aRow.groupName );

        if( statusIt != m_statuses.end()
                && statusIt->second.state == PIN_FUNCTION_GROUP_STATE::INCOMPLETE
                && !statusIt->second.message.IsEmpty() )
        {
            return statusIt->second.message;
        }

        int optionalAssigned = 0;
        int optionalTotal = 0;

        for( const PIN_FUNCTION_GROUP_MEMBER& member : aRow.members )
        {
            if( !member.optional )
                continue;

            optionalTotal++;

            if( member.assigned )
                optionalAssigned++;
        }

        if( optionalTotal > 0 )
        {
            return wxString::Format( _( "Active — %d of %d optional pins assigned" ),
                                     optionalAssigned, optionalTotal );
        }

        return _( "Active" );
    }

    wxString memberValue( const PIN_FUNCTION_GROUP_ROW& aRow,
                          const PIN_FUNCTION_GROUP_MEMBER& aMember, int aCol ) const
    {
        switch( aCol )
        {
        case GRP_COL_ACTIVE:
            return aMember.assigned ? wxS( "1" ) : wxS( "" );

        case GRP_COL_NAME:
            return aMember.altName;

        case GRP_COL_DESC:
            if( !aMember.subgroup.IsEmpty() )
                return aMember.subgroup;
            else if( aMember.optional )
                return _( "Optional" );
            else
                return _( "Required" );

        case GRP_COL_PINS:
            return aMember.pinNumber;

        case GRP_COL_STATUS:
            if( !aMember.available )
                return wxString::Format( _( "Pin %s in use by %s" ), aMember.pinNumber,
                                         aMember.blockedBy );
            else if( aMember.assigned )
                return _( "Assigned" );
            else if( aRow.active && aMember.IsUserChoice() )
                return _( "Click to assign" );
            else
                return wxEmptyString;

        default:
            return wxEmptyString;
        }
    }

    void SetValue( int aRow, int aCol, const wxString& aValue ) override
    {
        if( aCol == GRP_COL_ACTIVE )
            SetValueAsBool( aRow, aCol, !aValue.IsEmpty() && aValue != wxS( "0" ) );
    }

    bool CanGetValueAs( int aRow, int aCol, const wxString& aTypeName ) override
    {
        if( aCol == GRP_COL_ACTIVE && aTypeName == wxGRID_VALUE_BOOL )
            return true;

        return aTypeName == wxGRID_VALUE_STRING;
    }

    bool GetValueAsBool( int aRow, int aCol ) override
    {
        if( aCol != GRP_COL_ACTIVE )
            return false;

        const GRID_ROW& ref = m_displayRows[aRow];

        if( ref.memberIndex >= 0 )
            return m_rows[ref.groupIndex].members[ref.memberIndex].assigned;

        return m_rows[ref.groupIndex].active;
    }

    void SetValueAsBool( int aRow, int aCol, bool aValue ) override
    {
        if( aCol != GRP_COL_ACTIVE )
            return;

        const GRID_ROW& ref = m_displayRows[aRow];

        // Member rows are driven through the dialog (which owns the pins); only the group's own
        // active flag is settable here.
        if( ref.memberIndex >= 0 )
            return;

        PIN_FUNCTION_GROUP_ROW& row = m_rows[ref.groupIndex];

        if( row.conflicting && !row.active )
            return;

        row.active = aValue;
        RecomputeConflicts();
        RebuildDisplayRows();
        BuildAttrs();
    }

    wxGridCellAttr* GetAttr( int aRow, int aCol, wxGridCellAttr::wxAttrKind aKind ) override
    {
        if( aCol == GRP_COL_ACTIVE && aRow < (int) m_checkboxAttrs.size() )
        {
            m_checkboxAttrs[aRow]->IncRef();
            return enhanceAttr( m_checkboxAttrs[aRow], aRow, aCol, aKind );
        }

        if( aCol == GRP_COL_ACTIVE )
        {
            m_boolColAttr->IncRef();
            return enhanceAttr( m_boolColAttr, aRow, aCol, aKind );
        }

        // The expander column needs a renderer that draws the disclosure triangle; the plain
        // string renderer silently omits it.
        if( aCol == GRP_COL_NAME )
        {
            m_expanderAttr->IncRef();
            return enhanceAttr( m_expanderAttr, aRow, aCol, aKind );
        }

        m_readOnlyAttr->IncRef();
        return enhanceAttr( m_readOnlyAttr, aRow, aCol, aKind );
    }

    const std::vector<PIN_FUNCTION_GROUP_ROW>& GetRows() const { return m_rows; }
    bool HasGroups() const { return !m_rows.empty(); }

    /// Resolve a grid row to its group, or nullptr for a child row.
    const PIN_FUNCTION_GROUP_ROW* GroupForRow( int aRow ) const
    {
        if( aRow < 0 || aRow >= (int) m_displayRows.size() )
            return nullptr;

        const GRID_ROW& ref = m_displayRows[aRow];
        return ref.memberIndex >= 0 ? nullptr : &m_rows[ref.groupIndex];
    }

    /// Resolve a grid row to its member, or nullptr for a group row.  @p aGroup receives the
    /// owning group.
    const PIN_FUNCTION_GROUP_MEMBER* MemberForRow( int aRow,
                                                   const PIN_FUNCTION_GROUP_ROW** aGroup ) const
    {
        if( aRow < 0 || aRow >= (int) m_displayRows.size() )
            return nullptr;

        const GRID_ROW& ref = m_displayRows[aRow];

        if( ref.memberIndex < 0 )
            return nullptr;

        if( aGroup )
            *aGroup = &m_rows[ref.groupIndex];

        return &m_rows[ref.groupIndex].members[ref.memberIndex];
    }

    void ToggleRow( int aRow )
    {
        if( aRow < 0 || aRow >= (int) m_displayRows.size() )
            return;

        const GRID_ROW& ref = m_displayRows[aRow];

        if( ref.memberIndex >= 0 )
            return;

        PIN_FUNCTION_GROUP_ROW& row = m_rows[ref.groupIndex];

        if( row.conflicting && !row.active )
            return;

        SetValueAsBool( aRow, GRP_COL_ACTIVE, !row.active );
    }

private:
    /// Index into the grid's row space: one entry per visible row.  memberIndex < 0 marks a
    /// group row.  Kept separate from m_rows so the conflict/validation code never has to know
    /// that child rows exist.
    struct GRID_ROW
    {
        int groupIndex;
        int memberIndex;
    };

    /// Symbol context captured by CollectGroups(), so conflicts can be recomputed when only a
    /// pin assignment changed.  Both pointers are owned by the dialog and outlive this model.
    LIB_SYMBOL*                         m_libSymbol = nullptr;
    const std::vector<SCH_PIN>*         m_editedPins = nullptr;
    int                                 m_unit = 0;
    int                                 m_bodyStyle = 0;

    /// Per-group completion state, from LIB_SYMBOL::ValidatePinFunctionGroups().
    std::map<wxString, PIN_FUNCTION_GROUP_STATUS> m_statuses;

    std::vector<PIN_FUNCTION_GROUP_ROW> m_rows;
    std::vector<GRID_ROW>               m_displayRows;
    std::vector<wxGridCellAttr*>        m_checkboxAttrs;
    wxGridCellAttr*                     m_readOnlyAttr;
    wxGridCellAttr*                     m_boolColAttr;
    wxGridCellAttr*                     m_expanderAttr;
};


class SCH_PIN_TABLE_DATA_MODEL : public WX_GRID_TABLE_BASE, public std::vector<SCH_PIN>
{
public:
    SCH_PIN_TABLE_DATA_MODEL() :
            m_readOnlyAttr( nullptr ),
            m_typeAttr( nullptr ),
            m_shapeAttr( nullptr )
    {
    }

    ~SCH_PIN_TABLE_DATA_MODEL()
    {
        for( wxGridCellAttr* attr : m_nameAttrs )
            attr->DecRef();

        m_readOnlyAttr->DecRef();
        m_typeAttr->DecRef();
        m_shapeAttr->DecRef();
    }

    void SetGroupControlledPins( const std::set<wxString>& aPinNumbers )
    {
        m_groupControlledPins = aPinNumbers;
    }

    void BuildAttrs()
    {
        for( wxGridCellAttr* attr : m_nameAttrs )
            attr->DecRef();

        m_nameAttrs.clear();

        if( m_readOnlyAttr )
            m_readOnlyAttr->DecRef();

        m_readOnlyAttr = new wxGridCellAttr;
        m_readOnlyAttr->SetReadOnly( true );

        for( const SCH_PIN& pin : *this )
        {
            SCH_PIN*        lib_pin = pin.GetLibPin();
            wxGridCellAttr* attr = nullptr;
            bool            groupControlled = m_groupControlledPins.count( pin.GetNumber() ) > 0;

            if( groupControlled )
            {
                // Pin is controlled by an active function group — read-only with tinted background
                attr = new wxGridCellAttr;
                attr->SetReadOnly( true );
                attr->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_INFOBK ) );
            }
            else if( !lib_pin || lib_pin->GetAlternates().empty() )
            {
                attr = new wxGridCellAttr;
                attr->SetReadOnly( true );
                attr->SetBackgroundColour( KIPLATFORM::UI::GetDialogBGColour() );
            }
            else
            {
                wxArrayString choices;
                choices.push_back( lib_pin->GetName() );

                for( const std::pair<const wxString, SCH_PIN::ALT>& alt : lib_pin->GetAlternates() )
                    choices.push_back( alt.first );

                attr = new wxGridCellAttr();
                attr->SetEditor( new GRID_CELL_COMBOBOX( choices ) );
            }

            m_nameAttrs.push_back( attr );
        }

        if( m_typeAttr )
            m_typeAttr->DecRef();

        m_typeAttr = new wxGridCellAttr;
        m_typeAttr->SetRenderer( new GRID_CELL_ICON_TEXT_RENDERER( PinTypeIcons(), PinTypeNames() ) );
        m_typeAttr->SetReadOnly( true );

        if( m_shapeAttr )
            m_shapeAttr->DecRef();

        m_shapeAttr = new wxGridCellAttr;
        m_shapeAttr->SetRenderer( new GRID_CELL_ICON_TEXT_RENDERER( PinShapeIcons(), PinShapeNames() ) );
        m_shapeAttr->SetReadOnly( true );
    }

    int GetNumberRows() override { return (int) size(); }
    int GetNumberCols() override { return COL_COUNT; }

    wxString GetColLabelValue( int aCol ) override
    {
        switch( aCol )
        {
        case COL_NUMBER:    return _( "Number" );
        case COL_BASE_NAME: return _( "Base Name" );
        case COL_ALT_NAME:  return _( "Alternate Assignment" );
        case COL_TYPE:      return _( "Electrical Type" );
        case COL_SHAPE:     return _( "Graphic Style" );
        default:   wxFAIL;  return wxEmptyString;
        }
    }

    bool IsEmptyCell( int row, int col ) override
    {
        return false;   // don't allow adjacent cell overflow, even if we are actually empty
    }

    bool CanSetValueAs( int aRow, int aCol, const wxString& aTypeName ) override
    {
        // Don't accept random values; must use the popup to change to a known alternate
        return false;
    }

    wxString GetValue( int aRow, int aCol ) override
    {
        return GetValue( at( aRow ), aCol );
    }

    static wxString GetValue( const SCH_PIN& aPin, int aCol )
    {
        if( aCol == COL_ALT_NAME )
        {
            if( !aPin.GetLibPin() || aPin.GetLibPin()->GetAlternates().empty() )
                return wxEmptyString;
            else if( aPin.GetAlt().IsEmpty() )
                return aPin.GetName();
            else
                return aPin.GetAlt();
        }

        switch( aCol )
        {
        case COL_NUMBER:    return aPin.GetNumber();
        case COL_BASE_NAME: return aPin.GetBaseName();
        case COL_TYPE:      return PinTypeNames()[static_cast<int>( aPin.GetType() )];
        case COL_SHAPE:     return PinShapeNames()[static_cast<int>( aPin.GetShape() )];
        default:   wxFAIL;  return wxEmptyString;
        }
    }

    wxGridCellAttr* GetAttr( int aRow, int aCol, wxGridCellAttr::wxAttrKind aKind ) override
    {
        switch( aCol )
        {
        case COL_NUMBER:
        case COL_BASE_NAME:
            m_readOnlyAttr->IncRef();
            return enhanceAttr( m_readOnlyAttr, aRow, aCol, aKind );

        case COL_ALT_NAME:
            m_nameAttrs[ aRow ]->IncRef();
            return enhanceAttr( m_nameAttrs[ aRow ], aRow, aCol, aKind );

        case COL_TYPE:
            m_typeAttr->IncRef();
            return enhanceAttr( m_typeAttr, aRow, aCol, aKind );

        case COL_SHAPE:
            m_shapeAttr->IncRef();
            return enhanceAttr( m_shapeAttr, aRow, aCol, aKind );

        default:
            wxFAIL;
            return nullptr;
        }
    }

    void SetValue( int aRow, int aCol, const wxString &aValue ) override
    {
        SCH_PIN& pin = at( aRow );

        switch( aCol )
        {
        case COL_ALT_NAME:
            if( pin.GetLibPin() && aValue == pin.GetLibPin()->GetName() )
                pin.SetAlt( wxEmptyString );
            else
                pin.SetAlt( aValue );
            break;

        case COL_NUMBER:
        case COL_BASE_NAME:
        case COL_TYPE:
        case COL_SHAPE:
            // Read-only.
            break;

        default:
            wxFAIL;
            break;
        }
    }

    static bool compare( const SCH_PIN& lhs, const SCH_PIN& rhs, int sortCol, bool ascending )
    {
        wxString lhStr = GetValue( lhs, sortCol );
        wxString rhStr = GetValue( rhs, sortCol );

        if( lhStr == rhStr )
        {
            // Secondary sort key is always COL_NUMBER
            sortCol = COL_NUMBER;
            lhStr = GetValue( lhs, sortCol );
            rhStr = GetValue( rhs, sortCol );
        }

        bool res;

        // N.B. To meet the iterator sort conditions, we cannot simply invert the truth
        // to get the opposite sort.  i.e. ~(a<b) != (a>b)
        auto cmp = [ ascending ]( const auto a, const auto b )
                   {
                       if( ascending )
                           return a < b;
                       else
                           return b < a;
                   };

        switch( sortCol )
        {
        case COL_NUMBER:
        case COL_BASE_NAME:
        case COL_ALT_NAME:
            res = cmp( PIN_NUMBERS::Compare( lhStr, rhStr ), 0 );
            break;
        case COL_TYPE:
        case COL_SHAPE:
            res = cmp( lhStr.CmpNoCase( rhStr ), 0 );
            break;
        default:
            res = cmp( StrNumCmp( lhStr, rhStr ), 0 );
            break;
        }

        return res;
    }

    void SortRows( int aSortCol, bool ascending )
    {
        std::sort( begin(), end(),
                   [ aSortCol, ascending ]( const SCH_PIN& lhs, const SCH_PIN& rhs ) -> bool
                   {
                       return compare( lhs, rhs, aSortCol, ascending );
                   } );
    }

protected:
    std::vector<wxGridCellAttr*> m_nameAttrs;
    wxGridCellAttr*              m_readOnlyAttr;
    wxGridCellAttr*              m_typeAttr;
    wxGridCellAttr*              m_shapeAttr;
    std::set<wxString>           m_groupControlledPins;
};


DIALOG_SYMBOL_PROPERTIES::DIALOG_SYMBOL_PROPERTIES( SCH_EDIT_FRAME* aParent, SCH_SYMBOL* aSymbol ) :
        DIALOG_SYMBOL_PROPERTIES_BASE( aParent ),
        m_symbol( nullptr ),
        m_part( nullptr ),
        m_lastRequestedPinsSize( 0, 0 ),
        m_editorShown( false ),
        m_fields( nullptr ),
        m_dataModel( nullptr ),
        m_groupGrid( nullptr ),
        m_groupDataModel( nullptr ),
        m_embeddedFiles( nullptr ),
        m_pinMapPanel( nullptr )
{
    m_symbol = aSymbol;
    m_part = m_symbol->GetLibSymbolRef().get();

    // GetLibSymbolRef() now points to the cached part in the schematic, which should always be
    // there for usual cases, but can be null when opening old schematics not storing the part
    // so we need to handle m_part == nullptr
    // wxASSERT( m_part );

    m_fields = new FIELDS_GRID_TABLE( this, aParent, m_fieldsGrid, m_symbol );

    m_fieldsGrid->SetTable( m_fields );
    m_fieldsGrid->PushEventHandler( new FIELDS_GRID_TRICKS( m_fieldsGrid, this,
                                                            { &aParent->Schematic(), m_part },
                                                            [&]( wxCommandEvent& aEvent )
                                                            {
                                                                OnAddField( aEvent );
                                                            } ) );
    m_fieldsGrid->SetSelectionMode( wxGrid::wxGridSelectRows );
    m_fieldsGrid->ShowHideColumns( "0 1 2 3 4 5 6 7" );
    m_fieldsGrid->SetMinSize( wxSize( -1, 160 ) );
    m_fieldsGrid->OverrideMinSize( 1.0, 1.0 );
    m_shownColumns = m_fieldsGrid->GetShownColumns();

    if( m_symbol->GetEmbeddedFiles() )
    {
        m_embeddedFiles = new PANEL_EMBEDDED_FILES( m_notebook1, m_symbol->GetEmbeddedFiles() );
        m_notebook1->AddPage( m_embeddedFiles, _( "Embedded Files" ) );
    }

    m_pinMapPanel = new PANEL_SYMBOL_PIN_MAP( m_pinMapPage );
    bPinMapPageSizer->Add( m_pinMapPanel, 1, wxEXPAND, 5 );

    if( m_part && m_part->IsMultiBodyStyle() )
    {
        // Multiple body styles are a superclass of alternate pin assignments, so don't allow
        // free-form alternate assignments as well.  (We won't know how to map the alternates
        // back and forth when the body style is changed.)
        m_pinTablePage->Disable();
        m_pinTablePage->SetToolTip( _( "Alternate pin assignments are not available for symbols with multiple "
                                       "body styles." ) );
    }
    else
    {
        m_dataModel = new SCH_PIN_TABLE_DATA_MODEL();

        // Make a copy of the pins for editing
        for( const std::unique_ptr<SCH_PIN>& pin : m_symbol->GetRawPins() )
            m_dataModel->push_back( *pin );

        m_dataModel->SortRows( COL_NUMBER, true );
        m_dataModel->BuildAttrs();

        m_pinGrid->SetTable( m_dataModel );

        // Set up the function group grid above the pin grid
        m_groupDataModel = new PIN_FUNCTION_GROUP_DATA_MODEL();

        int unit = m_symbol->GetUnit();
        // Pass the pin model: it is what the validator and the conflict check read assignments
        // from, so without it both run against an empty map and every group looks INACTIVE.
        m_groupDataModel->CollectGroups( m_symbol, unit, m_dataModel );

        // Always create the function group grid, even when the current unit has no groups, so
        // that switching to a unit that does have groups can reveal it.  Per-unit visibility and
        // sizing are managed by RebuildGroupGrid().
        wxBoxSizer* bMargins = dynamic_cast<wxBoxSizer*>( m_pinGrid->GetContainingSizer() );

        if( bMargins )
        {
            m_groupGrid = new WX_GRID( m_pinTablePage, wxID_ANY, wxDefaultPosition,
                                       wxDefaultSize, 0 );

            m_groupGrid->CreateGrid( 0, 0 );
            m_groupGrid->SetTable( m_groupDataModel );
            m_groupGrid->EnableEditing( false );
            m_groupGrid->EnableGridLines( true );
            m_groupGrid->EnableDragGridSize( false );
            m_groupGrid->SetMargins( 0, 0 );
            m_groupGrid->SetColLabelSize( 24 );
            m_groupGrid->SetColLabelAlignment( wxALIGN_CENTER, wxALIGN_CENTER );
            m_groupGrid->EnableDragRowSize( false );
            m_groupGrid->SetRowLabelSize( 0 );
            m_groupGrid->SetDefaultCellAlignment( wxALIGN_LEFT, wxALIGN_CENTER );
            m_groupGrid->SetSelectionMode( wxGrid::wxGridSelectRows );

            m_groupGrid->SetColSize( GRP_COL_ACTIVE, 60 );
            m_groupGrid->SetColSize( GRP_COL_NAME, 120 );
            m_groupGrid->SetColSize( GRP_COL_DESC, 200 );
            m_groupGrid->SetColSize( GRP_COL_PINS, 140 );
            m_groupGrid->SetColSize( GRP_COL_STATUS, 200 );

            // Insert the group grid before the pin grid in the sizer
            bMargins->Insert( 0, m_groupGrid, 0, wxEXPAND | wxALL, 5 );

            m_groupGrid->Bind( wxEVT_GRID_CELL_LEFT_CLICK,
                               &DIALOG_SYMBOL_PROPERTIES::OnGroupGridCellClicked, this );

            if( m_groupDataModel->HasGroups() )
            {
                // Calculate a reasonable fixed height for the group grid
                int rowHeight = m_groupGrid->GetDefaultRowSize();
                int labelHeight = m_groupGrid->GetColLabelSize();
                int numRows = m_groupDataModel->GetNumberRows();
                int gridHeight = labelHeight + ( rowHeight * std::min( numRows, 6 ) ) + 2;

                m_groupGrid->SetMinSize( wxSize( -1, gridHeight ) );
                m_groupGrid->SetMaxSize( wxSize( -1, gridHeight ) );
            }
            else
            {
                m_groupGrid->Hide();
            }
        }

        // Reflect the existing assignments only.  Applying group activation here would let
        // merely opening the dialog top a subgroup up to its minimum, silently modifying the
        // schematic before the user has touched anything.
        RefreshGroupStatus();
    }

    if( m_part && m_part->IsPower() )
        m_spiceFieldsButton->Hide();

    m_pinGrid->PushEventHandler( new GRID_TRICKS( m_pinGrid ) );
    m_pinGrid->SetSelectionMode( wxGrid::wxGridSelectRows );

    wxFont infoFont = KIUI::GetSmallInfoFont( this );
    m_libraryIDLabel->SetFont( infoFont );
    m_tcLibraryID->SetFont( infoFont );
    m_tcLibraryID->SetBackgroundColour( KIPLATFORM::UI::GetDialogBGColour() );

    wxToolTip::Enable( true );
    SetupStandardButtons();

    // Configure button logos
    m_bpAdd->SetBitmap( KiBitmapBundle( BITMAPS::small_plus ) );
    m_bpDelete->SetBitmap( KiBitmapBundle( BITMAPS::small_trash ) );
    m_bpMoveUp->SetBitmap( KiBitmapBundle( BITMAPS::small_up ) );
    m_bpMoveDown->SetBitmap( KiBitmapBundle( BITMAPS::small_down ) );

    // wxFormBuilder doesn't include this event...
    m_fieldsGrid->Bind( wxEVT_GRID_CELL_CHANGING, &DIALOG_SYMBOL_PROPERTIES::OnGridCellChanging, this );
    m_pinGrid->Bind( wxEVT_GRID_COL_SORT, &DIALOG_SYMBOL_PROPERTIES::OnPinTableColSort, this );
    Bind( SYMBOL_DELAY_FOCUS, &DIALOG_SYMBOL_PROPERTIES::HandleDelayedFocus, this );
    Bind( SYMBOL_DELAY_SELECTION, &DIALOG_SYMBOL_PROPERTIES::HandleDelayedSelection, this );

    wxCommandEvent* evt = new wxCommandEvent( SYMBOL_DELAY_SELECTION );
    evt->SetClientData( new VECTOR2I( 0, FDC_VALUE ) );
    QueueEvent( evt );

    evt = new wxCommandEvent( SYMBOL_DELAY_FOCUS );
    evt->SetClientData( new VECTOR2I( 0, FDC_VALUE ) );
    QueueEvent( evt );

    // Remind user that they are editing the current variant.
    wxString variantName = aParent->Schematic().GetCurrentVariant();

    if( !variantName.IsEmpty() )
    {
        SetTitle( GetTitle() + wxS( " - " ) + variantName + _( " Design Variant" ) );

        m_changeSymbolBtn->SetLabel( _( "Set Variant Symbol..." ) );

        SCH_SHEET_PATH&            currentSheet = aParent->GetCurrentSheet();
        std::optional<SCH_SYMBOL_VARIANT> existingVariant =
                aSymbol->GetVariant( currentSheet, variantName );

        bool hasVariantSymbol = existingVariant.has_value() && existingVariant->m_SymbolOverride.has_value();

        m_clearVariantSymbolBtn->Show( hasVariantSymbol );
    }
    else
    {
        m_clearVariantSymbolBtn->Hide();
    }

    Layout();
    m_fieldsGrid->Layout();

    if( GetSizer() )
        GetSizer()->Fit( this );

    finishDialogSettings();
}


DIALOG_SYMBOL_PROPERTIES::~DIALOG_SYMBOL_PROPERTIES()
{
    // Prevents crash bug in wxGrid's d'tor
    m_fieldsGrid->DestroyTable( m_fields );

    if( m_dataModel )
        m_pinGrid->DestroyTable( m_dataModel );

    if( m_groupGrid )
    {
        m_groupGrid->Unbind( wxEVT_GRID_CELL_LEFT_CLICK,
                             &DIALOG_SYMBOL_PROPERTIES::OnGroupGridCellClicked, this );
        m_groupGrid->DestroyTable( m_groupDataModel );
    }
    else
    {
        delete m_groupDataModel;
    }

    m_fieldsGrid->Unbind( wxEVT_GRID_CELL_CHANGING, &DIALOG_SYMBOL_PROPERTIES::OnGridCellChanging, this );
    m_pinGrid->Unbind( wxEVT_GRID_COL_SORT, &DIALOG_SYMBOL_PROPERTIES::OnPinTableColSort, this );
    Unbind( SYMBOL_DELAY_FOCUS, &DIALOG_SYMBOL_PROPERTIES::HandleDelayedFocus, this );
    Unbind( SYMBOL_DELAY_SELECTION, &DIALOG_SYMBOL_PROPERTIES::HandleDelayedSelection, this );

    // Delete the GRID_TRICKS.
    m_fieldsGrid->PopEventHandler( true );
    m_pinGrid->PopEventHandler( true );
}


SCH_EDIT_FRAME* DIALOG_SYMBOL_PROPERTIES::GetParent()
{
    return dynamic_cast<SCH_EDIT_FRAME*>( wxDialog::GetParent() );
}


void DIALOG_SYMBOL_PROPERTIES::SelectPinMapPage()
{
    m_forcePinMapPage = true;
}


bool DIALOG_SYMBOL_PROPERTIES::TransferDataToWindow()
{
    if( !wxDialog::TransferDataToWindow() )
        return false;

    const SCHEMATIC& schematic = GetParent()->Schematic();
    SCH_SHEET_PATH& sheetPath = schematic.CurrentSheet();
    wxString variantName = schematic.GetCurrentVariant();
    std::optional<SCH_SYMBOL_VARIANT> variant = m_symbol->GetVariant( sheetPath, variantName );
    std::set<wxString> defined;

    // Push a copy of each field into m_updateFields
    for( SCH_FIELD& srcField : m_symbol->GetFields() )
    {
        SCH_FIELD field( srcField );

        // change offset to be symbol-relative
        field.Offset( -m_symbol->GetPosition() );
        field.SetText( schematic.ConvertKIIDsToRefs( m_symbol->GetFieldText( field.GetName(), &sheetPath,
                                                                             variantName ) ) );

        defined.insert( field.GetName() );
        m_fields->push_back( field );
    }

    // Add in any template fieldnames not yet defined:
    for( const TEMPLATE_FIELDNAME& templateFieldname :
         schematic.Settings().m_TemplateFieldNames.GetTemplateFieldNames() )
    {
        if( defined.count( templateFieldname.m_Name ) <= 0 )
        {
            SCH_FIELD field( m_symbol, FIELD_T::USER, templateFieldname.m_Name );
            field.SetVisible( templateFieldname.m_Visible );
            m_fields->push_back( field );
        }
    }

    // notify the grid
    wxGridTableMessage msg( m_fields, wxGRIDTABLE_NOTIFY_ROWS_APPENDED, m_fields->GetNumberRows() );
    m_fieldsGrid->ProcessTableMessage( msg );

    // If a multi-unit symbol, set up the unit selector and interchangeable checkbox.
    if( m_symbol->IsMultiUnit() )
    {
        // Ensure symbol unit is the currently selected unit (mandatory in complex hierarchies)
        // from the current sheet path, because it can be modified by previous calculations
        m_symbol->SetUnit( m_symbol->GetUnitSelection( &sheetPath ) );

        for( int ii = 1; ii <= m_symbol->GetUnitCount(); ii++ )
            m_unitChoice->Append( m_symbol->GetUnitDisplayName( ii, false ) );

        if( m_symbol->GetUnit() <= ( int )m_unitChoice->GetCount() )
            m_unitChoice->SetSelection( m_symbol->GetUnit() - 1 );
    }
    else
    {
        m_unitLabel->Enable( false );
        m_unitChoice->Enable( false );
    }

    if( m_part && m_part->IsMultiBodyStyle() )
    {
        if( m_part->HasDeMorganBodyStyles() )
        {
            m_bodyStyleChoice->Append( _( "Standard" ) );
            m_bodyStyleChoice->Append( _( "Alternate" ) );
        }
        else
        {
            wxASSERT( (int)m_part->GetBodyStyleNames().size() == m_part->GetBodyStyleCount() );

            for( int ii = 0; ii < m_part->GetBodyStyleCount(); ii++ )
            {
                try
                {
                    m_bodyStyleChoice->Append( m_part->GetBodyStyleNames().at( ii ) );
                }
                catch( ... )
                {
                    m_bodyStyleChoice->Append( wxT( "???" ) );
                }
            }
        }

        if( m_symbol->GetBodyStyle() <= (int) m_bodyStyleChoice->GetCount() )
            m_bodyStyleChoice->SetSelection( m_symbol->GetBodyStyle() - 1 );
    }
    else
    {
        m_bodyStyle->Enable( false );
        m_bodyStyleChoice->Enable( false );
    }

    // Set the symbol orientation and mirroring.
    int orientation = m_symbol->GetOrientation() & ~( SYM_MIRROR_X | SYM_MIRROR_Y );

    switch( orientation )
    {
    default:
    case SYM_ORIENT_0:   m_orientationCtrl->SetSelection( 0 ); break;
    case SYM_ORIENT_90:  m_orientationCtrl->SetSelection( 1 ); break;
    case SYM_ORIENT_270: m_orientationCtrl->SetSelection( 2 ); break;
    case SYM_ORIENT_180: m_orientationCtrl->SetSelection( 3 ); break;
    }

    int mirror = m_symbol->GetOrientation() & ( SYM_MIRROR_X | SYM_MIRROR_Y );

    switch( mirror )
    {
    default:           m_mirrorCtrl->SetSelection( 0 ) ; break;
    case SYM_MIRROR_X: m_mirrorCtrl->SetSelection( 1 );  break;
    case SYM_MIRROR_Y: m_mirrorCtrl->SetSelection( 2 );  break;
    }

    m_cbExcludeFromSim->SetValue( m_symbol->GetExcludedFromSim( &sheetPath, variantName ) );
    m_cbExcludeFromBom->SetValue( m_symbol->GetExcludedFromBOM( &sheetPath, variantName ) );
    m_cbExcludeFromBoard->SetValue( m_symbol->GetExcludedFromBoard( &sheetPath, variantName ) );
    m_cbExcludeFromPosFiles->SetValue( m_symbol->GetExcludedFromPosFiles( &sheetPath, variantName ) );
    m_cbDNP->SetValue( m_symbol->GetDNP( &sheetPath, variantName ) );

    switch( m_symbol->GetPassthroughMode() )
    {
    case SCH_SYMBOL::PASSTHROUGH_MODE::DEFAULT: m_choicePassthrough->SetSelection( 0 ); break;
    case SCH_SYMBOL::PASSTHROUGH_MODE::BLOCK:   m_choicePassthrough->SetSelection( 1 ); break;
    case SCH_SYMBOL::PASSTHROUGH_MODE::FORCE:   m_choicePassthrough->SetSelection( 2 ); break;
    }

    if( m_part )
    {
        m_ShowPinNumButt->SetValue( m_part->GetShowPinNumbers() );
        m_ShowPinNameButt->SetValue( m_part->GetShowPinNames() );
    }

    // Set the symbol's library name.
    m_tcLibraryID->SetValue( UnescapeString( m_symbol->GetLibId().Format() ) );

    if( m_embeddedFiles && !m_embeddedFiles->TransferDataToWindow() )
        return false;

    m_pinMapPanel->SetSymbol( m_part );
    m_pinMapPanel->TransferDataToWindow();

    m_fieldsGrid->Layout();
    Layout();
    m_fieldsGrid->SetMinVisibleRows( this, 4 );

    // Always open on the General page, unless the caller explicitly asked for the Pin Map
    // tab (the Edit Pin Map button).  This overrides DIALOG_SHIM's remembered-tab restore.
    int targetPage = 0;

    if( m_forcePinMapPage )
    {
        for( size_t page = 0; page < m_notebook1->GetPageCount(); ++page )
        {
            if( m_notebook1->GetPage( page ) == m_pinMapPage )
            {
                targetPage = (int) page;
                break;
            }
        }
    }

    m_notebook1->ChangeSelection( targetPage );

    return true;
}


void DIALOG_SYMBOL_PROPERTIES::OnEditSpiceModel( wxCommandEvent& event )
{
    if( !m_fieldsGrid->CommitPendingChanges() )
        return;

    m_fieldsGrid->ClearSelection();

    std::vector<SCH_FIELD> fields;

    for( const SCH_FIELD& field : *m_fields )
        fields.emplace_back( field );

    DIALOG_SIM_MODEL dialog( this, m_parentFrame, *m_symbol, fields );

    if( dialog.ShowModal() != wxID_OK )
        return;

    // Add in any new fields
    for( const SCH_FIELD& editedField : fields )
    {
        bool found = false;

        for( SCH_FIELD& existingField : *m_fields )
        {
            if( existingField.GetName() == editedField.GetName() )
            {
                found = true;
                existingField.SetText( editedField.GetText() );
                break;
            }
        }

        if( !found )
        {
            m_fields->emplace_back( editedField );
            wxGridTableMessage msg( m_fields, wxGRIDTABLE_NOTIFY_ROWS_APPENDED, 1 );
            m_fieldsGrid->ProcessTableMessage( msg );
        }
    }

    // Remove any deleted fields
    for( int ii = (int) m_fields->size() - 1; ii >= 0; --ii )
    {
        SCH_FIELD& existingField = m_fields->at( ii );
        bool       found = false;

        for( SCH_FIELD& editedField : fields )
        {
            if( editedField.GetName() == existingField.GetName() )
            {
                found = true;
                break;
            }
        }

        if( !found )
        {
            m_fieldsGrid->ClearSelection();
            m_fields->erase( m_fields->begin() + ii );

            wxGridTableMessage msg( m_fields, wxGRIDTABLE_NOTIFY_ROWS_DELETED, ii, 1 );
            m_fieldsGrid->ProcessTableMessage( msg );
        }
    }

    OnModify();
    m_fieldsGrid->ForceRefresh();
}


void DIALOG_SYMBOL_PROPERTIES::OnCancelButtonClick( wxCommandEvent& event )
{
    // Running the Footprint Browser gums up the works and causes the automatic cancel
    // stuff to no longer work.  So we do it here ourselves.
    EndDialogShim( wxID_CANCEL );
}


bool DIALOG_SYMBOL_PROPERTIES::Validate()
{
    LIB_ID   id;

    if( !m_fieldsGrid->CommitPendingChanges() || !m_fieldsGrid->Validate() )
        return false;

    // Check for missing field names.
    for( size_t i = 0; i < m_fields->size(); ++i )
    {
        SCH_FIELD& field = m_fields->at( i );

        if( field.IsMandatory() )
            continue;

        wxString fieldName = field.GetName( false );

        if( fieldName.IsEmpty() )
        {
            DisplayErrorMessage( this, _( "Fields must have a name." ) );

            wxCommandEvent *evt = new wxCommandEvent( SYMBOL_DELAY_FOCUS );
            evt->SetClientData( new VECTOR2I( i, FDC_VALUE ) );
            QueueEvent( evt );

            return false;
        }
    }

    return true;
}


bool DIALOG_SYMBOL_PROPERTIES::TransferDataFromWindow()
{
    if( !wxDialog::TransferDataFromWindow() )  // Calls our Validate() method.
        return false;

    if( m_embeddedFiles && !m_embeddedFiles->TransferDataFromWindow() )
        return false;

    if( !m_fieldsGrid->CommitPendingChanges() )
        return false;

    if( !m_pinGrid->CommitPendingChanges() )
        return false;

    if( !m_pinMapPanel->CommitPendingChanges() )
        return false;

    SCH_COMMIT     commit( GetParent() );
    SCH_SCREEN*    currentScreen = GetParent()->GetScreen();
    SCH_SHEET_PATH currentSheet = GetParent()->Schematic().CurrentSheet();
    wxString       currentVariant = GetParent()->Schematic().GetCurrentVariant();
    bool           replaceOnCurrentScreen;

    wxCHECK( currentScreen, false );

    // This needs to be done before the LIB_ID is changed to prevent stale library symbols in
    // the schematic file.
    replaceOnCurrentScreen = currentScreen->Remove( m_symbol );

    // save old cmp in undo list if not already in edit, or moving ...
    if( m_symbol->GetEditFlags() == 0 )
        commit.Modify( m_symbol, currentScreen );

    // Apply pin-map edits after the undo snapshot so undo restores them (issue #2282).
    if( m_part )
        m_pinMapPanel->ApplyToSymbol( m_part );

    // Save current flags which could be modified by next change settings
    EDA_ITEM_FLAGS flags = m_symbol->GetFlags();

    //Set the part selection in multiple part per package
    int unit_selection = m_unitChoice->IsEnabled() ? m_unitChoice->GetSelection() + 1 : 1;
    m_symbol->SetUnitSelection( &GetParent()->GetCurrentSheet(), unit_selection );
    m_symbol->SetUnit( unit_selection );

    int bodyStyle_selection = m_bodyStyleChoice->IsEnabled() ? m_bodyStyleChoice->GetSelection() + 1 : 1;
    m_symbol->SetBodyStyle( bodyStyle_selection );

    switch( m_orientationCtrl->GetSelection() )
    {
    case 0: m_symbol->SetOrientation( SYM_ORIENT_0 );   break;
    case 1: m_symbol->SetOrientation( SYM_ORIENT_90 );  break;
    case 2: m_symbol->SetOrientation( SYM_ORIENT_270 ); break;
    case 3: m_symbol->SetOrientation( SYM_ORIENT_180 ); break;
    }

    switch( m_mirrorCtrl->GetSelection() )
    {
    case 0:                                           break;
    case 1: m_symbol->SetOrientation( SYM_MIRROR_X ); break;
    case 2: m_symbol->SetOrientation( SYM_MIRROR_Y ); break;
    }

    m_symbol->SetShowPinNames( m_ShowPinNameButt->GetValue() );
    m_symbol->SetShowPinNumbers( m_ShowPinNumButt->GetValue() );

    // Restore m_Flag modified by SetUnit() and other change settings from the dialog
    m_symbol->ClearFlags();
    m_symbol->SetFlags( flags );

    // change all field positions from relative to absolute
    for( SCH_FIELD& field : *m_fields )
        field.Offset( m_symbol->GetPosition() );

    int ordinal = 42;   // Arbitrarily larger than any mandatory FIELD_T ids.

    for( SCH_FIELD& field : *m_fields )
    {
        const wxString& fieldName = field.GetCanonicalName();

        if( fieldName.IsEmpty() && field.GetText().IsEmpty() )
            continue;
        else if( fieldName.IsEmpty() )
            field.SetName( _( "untitled" ) );

        const SCH_FIELD* existingField = m_symbol->GetField( fieldName );
        SCH_FIELD* tmp;

        if( !existingField )
        {
            tmp = m_symbol->AddField( field );
            tmp->SetParent( m_symbol );
        }
        else
        {
            wxString defaultText = m_symbol->Schematic()->ConvertRefsToKIIDs( existingField->GetText() );
            tmp = const_cast<SCH_FIELD*>( existingField );

            *tmp = field;

            if( !currentVariant.IsEmpty() )
            {
                // Restore the default field text for existing fields.
                tmp->SetText( defaultText, &currentSheet );

                wxString variantText = m_symbol->Schematic()->ConvertRefsToKIIDs( field.GetText() );
                tmp->SetText( variantText, &currentSheet, currentVariant );
            }
        }

        if( !field.IsMandatory() )
            field.SetOrdinal( ordinal++ );
    }

    for( int ii = (int) m_symbol->GetFields().size() - 1; ii >= 0; ii-- )
    {
        SCH_FIELD& symbolField = m_symbol->GetFields()[ii];

        if( symbolField.IsMandatory() )
            continue;

        bool found = false;

        for( const SCH_FIELD& editedField : *m_fields )
        {
            if( editedField.GetName() == symbolField.GetName() )
            {
                found = true;
                break;
            }
        }

        if( !found )
            m_symbol->GetFields().erase( m_symbol->GetFields().begin() + ii );
    }

    if( currentVariant.IsEmpty() )
    {
        // Reference has a specific initialization, depending on the current active sheet
        // because for a given symbol, in a complex hierarchy, there are more than one
        // reference.
        m_symbol->SetRef( &GetParent()->GetCurrentSheet(), m_fields->GetField( FIELD_T::REFERENCE )->GetText() );
    }

    m_symbol->SetExcludedFromSim( m_cbExcludeFromSim->IsChecked(), &currentSheet, currentVariant );
    m_symbol->SetExcludedFromBOM( m_cbExcludeFromBom->IsChecked(), &currentSheet, currentVariant );
    m_symbol->SetExcludedFromBoard( m_cbExcludeFromBoard->IsChecked(), &currentSheet, currentVariant );
    m_symbol->SetExcludedFromPosFiles( m_cbExcludeFromPosFiles->IsChecked(), &currentSheet, currentVariant );
    m_symbol->SetDNP( m_cbDNP->IsChecked(), &currentSheet, currentVariant );

    switch( m_choicePassthrough->GetSelection() )
    {
    case 0: m_symbol->SetPassthroughMode( SCH_SYMBOL::PASSTHROUGH_MODE::DEFAULT ); break;
    case 1: m_symbol->SetPassthroughMode( SCH_SYMBOL::PASSTHROUGH_MODE::BLOCK );   break;
    case 2: m_symbol->SetPassthroughMode( SCH_SYMBOL::PASSTHROUGH_MODE::FORCE );   break;
    default: break;
    }

    // Update any assignments.  The per-pin alternate is the single source of truth for pin
    // function groups: enabling a group assigns its members' alternates in the pin grid (see
    // SyncPinGridFromGroups()), so persisting the model pins' alternates here captures the
    // active groups implicitly.
    if( m_dataModel )
    {
        for( const SCH_PIN& model_pin : *m_dataModel )
        {
            // map from the edited copy back to the "real" pin(s) in the symbol.
            for( SCH_PIN* src_pin : m_symbol->GetPinsByNumber( model_pin.GetNumber() ) )
                src_pin->SetAlt( model_pin.GetAlt() );
        }
    }

    // Keep fields other than the reference, include/exclude flags, and alternate pin assignements
    // in sync in multi-unit parts.
    m_symbol->SyncOtherUnits( currentSheet, commit, nullptr, currentVariant );

    if( replaceOnCurrentScreen )
        currentScreen->Append( m_symbol );

    if( !commit.Empty() )
        commit.Push( _( "Edit Symbol Properties" ) );

    return true;
}


void DIALOG_SYMBOL_PROPERTIES::OnGridCellChanging( wxGridEvent& event )
{
    wxGridCellEditor* editor = m_fieldsGrid->GetCellEditor( event.GetRow(), event.GetCol() );
    wxControl* control = editor->GetControl();

    if( control && control->GetValidator() && !control->GetValidator()->Validate( control ) )
    {
        event.Veto();
        wxCommandEvent *evt = new wxCommandEvent( SYMBOL_DELAY_FOCUS );
        evt->SetClientData( new VECTOR2I( event.GetRow(), event.GetCol() ) );
        QueueEvent( evt );
    }
    else if( event.GetCol() == FDC_NAME )
    {
        wxString newName = event.GetString();

        for( int i = 0; i < m_fieldsGrid->GetNumberRows(); ++i )
        {
            if( i == event.GetRow() )
                continue;

            if( FieldNamesAreDuplicates( newName, m_fieldsGrid->GetCellValue( i, FDC_NAME ) ) )
            {
                DisplayError( this, wxString::Format( _( "Field name '%s' already in use." ),
                                                      newName ) );
                event.Veto();
                wxCommandEvent *evt = new wxCommandEvent( SYMBOL_DELAY_FOCUS );
                evt->SetClientData( new VECTOR2I( event.GetRow(), event.GetCol() ) );
                QueueEvent( evt );
            }
        }
    }

    editor->DecRef();
}


void DIALOG_SYMBOL_PROPERTIES::OnGridEditorShown( wxGridEvent& aEvent )
{
    if( m_fields->at( aEvent.GetRow() ).GetId() == FIELD_T::REFERENCE
            && aEvent.GetCol() == FDC_VALUE )
    {
        wxCommandEvent* evt = new wxCommandEvent( SYMBOL_DELAY_SELECTION );
        evt->SetClientData( new VECTOR2I( aEvent.GetRow(), aEvent.GetCol() ) );
        QueueEvent( evt );
    }

    m_editorShown = true;
}


void DIALOG_SYMBOL_PROPERTIES::OnGridEditorHidden( wxGridEvent& aEvent )
{
    m_editorShown = false;
}


void DIALOG_SYMBOL_PROPERTIES::OnAddField( wxCommandEvent& event )
{
    m_fieldsGrid->OnAddRow(
            [&]() -> std::pair<int, int>
            {
                SCH_FIELD newField( m_symbol, FIELD_T::USER, GetUserFieldName( m_fields->size(), DO_TRANSLATE ) );

                newField.SetTextAngle( m_fields->GetField( FIELD_T::REFERENCE )->GetTextAngle() );
                newField.SetVisible( false );

                m_fields->push_back( newField );

                // notify the grid
                wxGridTableMessage msg( m_fields, wxGRIDTABLE_NOTIFY_ROWS_APPENDED, 1 );
                m_fieldsGrid->ProcessTableMessage( msg );
                OnModify();

                return { m_fields->size() - 1, FDC_NAME };
            } );
}


void DIALOG_SYMBOL_PROPERTIES::OnDeleteField( wxCommandEvent& event )
{
    m_fieldsGrid->OnDeleteRows(
            [&]( int row )
            {
                if( row < m_fields->GetMandatoryRowCount() )
                {
                    DisplayError( this, wxString::Format( _( "The first %d fields are mandatory." ),
                                                          m_fields->GetMandatoryRowCount() ) );
                    return false;
                }

                return true;
            },
            [&]( int row )
            {
                m_fields->erase( m_fields->begin() + row );

                // notify the grid
                wxGridTableMessage msg( m_fields, wxGRIDTABLE_NOTIFY_ROWS_DELETED, row, 1 );
                m_fieldsGrid->ProcessTableMessage( msg );
            } );

    OnModify();
}


void DIALOG_SYMBOL_PROPERTIES::OnMoveUp( wxCommandEvent& event )
{
    m_fieldsGrid->OnMoveRowUp(
            [&]( int row )
            {
                return row > m_fields->GetMandatoryRowCount();
            },
            [&]( int row )
            {
                std::swap( *( m_fields->begin() + row ), *( m_fields->begin() + row - 1 ) );
                m_fieldsGrid->ForceRefresh();
                OnModify();
            } );
}


void DIALOG_SYMBOL_PROPERTIES::OnMoveDown( wxCommandEvent& event )
{
    m_fieldsGrid->OnMoveRowDown(
            [&]( int row )
            {
                return row >= m_fields->GetMandatoryRowCount();
            },
            [&]( int row )
            {
                    std::swap( *( m_fields->begin() + row ), *( m_fields->begin() + row + 1 ) );
                    m_fieldsGrid->ForceRefresh();
                    OnModify();
            } );
}


void DIALOG_SYMBOL_PROPERTIES::OnEditSymbol( wxCommandEvent&  )
{
    if( TransferDataFromWindow() )
        EndDialogShim( SYMBOL_PROPS_EDIT_SCHEMATIC_SYMBOL );
}


void DIALOG_SYMBOL_PROPERTIES::OnEditLibrarySymbol( wxCommandEvent&  )
{
    if( TransferDataFromWindow() )
        EndDialogShim( SYMBOL_PROPS_EDIT_LIBRARY_SYMBOL );
}


void DIALOG_SYMBOL_PROPERTIES::OnUpdateSymbol( wxCommandEvent&  )
{
    if( TransferDataFromWindow() )
        EndDialogShim( SYMBOL_PROPS_WANT_UPDATE_SYMBOL );
}


void DIALOG_SYMBOL_PROPERTIES::OnExchangeSymbol( wxCommandEvent& )
{
    if( !TransferDataFromWindow() )
        return;

    if( !GetParent()->Schematic().GetCurrentVariant().IsEmpty() )
        EndDialogShim( SYMBOL_PROPS_WANT_SET_VARIANT_SYMBOL );
    else
        EndDialogShim( SYMBOL_PROPS_WANT_EXCHANGE_SYMBOL );
}


void DIALOG_SYMBOL_PROPERTIES::OnClearVariantSymbol( wxCommandEvent& )
{
    if( TransferDataFromWindow() )
        EndDialogShim( SYMBOL_PROPS_WANT_CLEAR_VARIANT_SYMBOL );
}


void DIALOG_SYMBOL_PROPERTIES::OnPinTableCellEdited( wxGridEvent& aEvent )
{
    int row = aEvent.GetRow();

    if( m_pinGrid->GetCellValue( row, COL_ALT_NAME ) == m_dataModel->GetValue( row, COL_BASE_NAME ) )
        m_dataModel->SetValue( row, COL_ALT_NAME, wxEmptyString );

    // These are just to get the cells refreshed
    m_dataModel->SetValue( row, COL_TYPE, m_dataModel->GetValue( row, COL_TYPE ) );
    m_dataModel->SetValue( row, COL_SHAPE, m_dataModel->GetValue( row, COL_SHAPE ) );

    // Editing an alternate by hand can complete or break a group, so re-derive the group state
    // from the pins.  Non-mutating, so it will not undo the edit that was just made.
    RefreshGroupStatus();

    if( m_groupGrid )
        m_groupGrid->ForceRefresh();

    OnModify();
}


void DIALOG_SYMBOL_PROPERTIES::OnPinTableColSort( wxGridEvent& aEvent )
{
    int sortCol = aEvent.GetCol();
    bool ascending;

    // This is bonkers, but wxWidgets doesn't tell us ascending/descending in the
    // event, and if we ask it will give us pre-event info.
    if( m_pinGrid->IsSortingBy( sortCol ) )
        // same column; invert ascending
        ascending = !m_pinGrid->IsSortOrderAscending();
    else
        // different column; start with ascending
        ascending = true;

    m_dataModel->SortRows( sortCol, ascending );
    m_dataModel->BuildAttrs();
}


void DIALOG_SYMBOL_PROPERTIES::AdjustPinsGridColumns()
{
    wxGridUpdateLocker deferRepaintsTillLeavingScope( m_pinGrid );

    // Account for scroll bars
    int pinTblWidth = KIPLATFORM::UI::GetUnobscuredSize( m_pinGrid ).x;

    // Stretch the Base Name and Alternate Assignment columns to fit.
    for( int i = 0; i < COL_COUNT; ++i )
    {
        if( i != COL_BASE_NAME && i != COL_ALT_NAME )
            pinTblWidth -= m_pinGrid->GetColSize( i );
    }

    if( pinTblWidth > 2 )
    {
        m_pinGrid->SetColSize( COL_BASE_NAME, pinTblWidth / 2 );
        m_pinGrid->SetColSize( COL_ALT_NAME, pinTblWidth / 2 );
    }
}


void DIALOG_SYMBOL_PROPERTIES::OnUpdateUI( wxUpdateUIEvent& event )
{
    std::bitset<64> shownColumns = m_fieldsGrid->GetShownColumns();

    if( shownColumns != m_shownColumns )
    {
        m_shownColumns = shownColumns;

        if( !m_fieldsGrid->IsCellEditControlShown() )
            m_fieldsGrid->SetGridWidthsDirty();
    }
}


void DIALOG_SYMBOL_PROPERTIES::HandleDelayedFocus( wxCommandEvent& event )
{
    VECTOR2I *loc = static_cast<VECTOR2I*>( event.GetClientData() );

    wxCHECK_RET( loc, wxT( "Missing focus cell location" ) );

    // Run the AutoColumnSizer before setting focus (as it will clear any shown cell edit control
    // if it has to resize that column).
    m_fieldsGrid->RecomputeGridWidths();

    // Handle a delayed focus

    m_fieldsGrid->SetFocus();
    m_fieldsGrid->MakeCellVisible( loc->x, loc->y );
    m_fieldsGrid->SetGridCursor( loc->x, loc->y );

    delete loc;

    CallAfter(
            [this]()
            {
                m_fieldsGrid->EnableCellEditControl( true );
            } );
}


void DIALOG_SYMBOL_PROPERTIES::HandleDelayedSelection( wxCommandEvent& event )
{
    VECTOR2I *loc = static_cast<VECTOR2I*>( event.GetClientData() );

    wxCHECK_RET( loc, wxT( "Missing focus cell location" ) );

    // Handle a delayed selection
    wxGridCellEditor* cellEditor = m_fieldsGrid->GetCellEditor( loc->x, loc->y );

    if( wxTextEntry* txt = dynamic_cast<wxTextEntry*>( cellEditor->GetControl() ) )
        KIUI::SelectReferenceNumber( txt );

    cellEditor->DecRef();   // we're done; must release
    delete loc;
}


void DIALOG_SYMBOL_PROPERTIES::OnSizePinsGrid( wxSizeEvent& event )
{
    wxSize new_size = event.GetSize();

    if( ( !m_editorShown || m_lastRequestedPinsSize != new_size ) && m_pinsSize != new_size )
    {
        m_pinsSize = new_size;

        AdjustPinsGridColumns();
    }

    // We store this value to check whether the dialog is changing size.  This might indicate
    // that the user is scaling the dialog with a grid-cell-editor shown.  Some editors do not
    // close (at least on GTK) when the user drags a dialog corner
    m_lastRequestedPinsSize = new_size;

    // Always propagate for a grid repaint (needed if the height changes, as well as width)
    event.Skip();
}


void DIALOG_SYMBOL_PROPERTIES::OnCheckBox( wxCommandEvent& event )
{
    OnModify();
}


void DIALOG_SYMBOL_PROPERTIES::OnUnitChoice( wxCommandEvent& event )
{
    if( m_dataModel )
    {
        EDA_ITEM_FLAGS flags = m_symbol->GetFlags();

        int unit_selection = m_unitChoice->GetSelection() + 1;

        // We need to select a new unit to build the new unit pin list
        // but we should not change the symbol, so the initial unit will be selected
        // after rebuilding the pin list
        int old_unit = m_symbol->GetUnit();
        m_symbol->SetUnit( unit_selection );

        // The pin model is deliberately NOT rebuilt here.  SCH_SYMBOL::UpdatePins() does not
        // filter by unit ("this data-structure is used for all instances, some of which might
        // have different units"), so a rebuild would return the very same pins while discarding
        // every alternate the user has edited in this dialog -- including their group toggles.
        //
        // The group grid genuinely is per-unit (CollectGroups() walks GetGraphicalPins( aUnit ))
        // so that is still rebuilt, and it now reads activity back from the edited pins.
        RebuildGroupGrid( unit_selection );

        m_symbol->SetUnit( old_unit );

        // Restore m_Flag modified by SetUnit()
        m_symbol->ClearFlags();
        m_symbol->SetFlags( flags );
    }

    OnModify();
}


void DIALOG_SYMBOL_PROPERTIES::onUpdateEditSymbol( wxUpdateUIEvent& event )
{
    event.Enable( m_symbol && m_symbol->GetLibSymbolRef() );
}


void DIALOG_SYMBOL_PROPERTIES::onUpdateEditLibrarySymbol( wxUpdateUIEvent& event )
{
    event.Enable( m_symbol && m_symbol->GetLibSymbolRef() );
}


void DIALOG_SYMBOL_PROPERTIES::OnPageChanging( wxBookCtrlEvent& aEvent )
{
    if( !m_fieldsGrid->CommitPendingChanges() )
        aEvent.Veto();

    if( !m_pinGrid->CommitPendingChanges() )
        aEvent.Veto();
}


void DIALOG_SYMBOL_PROPERTIES::OnGroupGridCellClicked( wxGridEvent& aEvent )
{
    int row = aEvent.GetRow();
    int col = aEvent.GetCol();

    if( !m_groupDataModel )
    {
        aEvent.Skip();
        return;
    }

    // Clicking the name column expands/collapses a group to reveal its members.
    if( m_groupDataModel->IsExpanderColumn( col ) )
    {
        m_groupGrid->ClearSelection();
        m_groupDataModel->ExpandCollapseRow( row );
        sizeGroupGrid();
        m_groupGrid->ForceRefresh();
        return;
    }

    if( col != GRP_COL_ACTIVE )
    {
        aEvent.Skip();
        return;
    }

    const PIN_FUNCTION_GROUP_ROW*    group = nullptr;
    const PIN_FUNCTION_GROUP_MEMBER* member = m_groupDataModel->MemberForRow( row, &group );

    if( member )
    {
        ToggleGroupMember( *group, *member );
    }
    else
    {
        const PIN_FUNCTION_GROUP_ROW* toggled = m_groupDataModel->GroupForRow( row );
        wxString                      groupName = toggled ? toggled->groupName : wxString();

        m_groupDataModel->ToggleRow( row );
        ApplyGroupActivation();

        // Reveal the choices in place rather than leaving the user to find the pins by hand.
        if( !groupName.IsEmpty() )
            m_groupDataModel->AutoExpandGroup( groupName );

        RefreshGroupStatus();
        sizeGroupGrid();
    }

    m_groupGrid->ForceRefresh();
    m_pinGrid->ForceRefresh();

    OnModify();
}


void DIALOG_SYMBOL_PROPERTIES::ToggleGroupMember( const PIN_FUNCTION_GROUP_ROW& aGroup,
                                                  const PIN_FUNCTION_GROUP_MEMBER& aMember )
{
    // Only the user's own choices are toggleable, and only while the group is active and no
    // other group is holding the pin.  Required members follow their group.
    if( !aGroup.active || !aMember.IsUserChoice() || !aMember.available )
        return;

    for( SCH_PIN& pin : *m_dataModel )
    {
        if( pin.GetNumber() != aMember.pinNumber )
            continue;

        // The pins remain the single source of truth: flip the assignment here and let the
        // group grid re-derive its state from it.
        pin.SetAlt( aMember.assigned ? wxString() : aMember.altName );
        break;
    }

    RefreshGroupStatus();
}


void DIALOG_SYMBOL_PROPERTIES::sizeGroupGrid()
{
    if( !m_groupGrid || !m_groupDataModel || !m_groupDataModel->HasGroups() )
        return;

    int rowHeight = m_groupGrid->GetDefaultRowSize();
    int labelHeight = m_groupGrid->GetColLabelSize();
    int numRows = m_groupDataModel->GetNumberRows();
    int gridHeight = labelHeight + ( rowHeight * std::min( numRows, 8 ) ) + 2;

    m_groupGrid->SetMinSize( wxSize( -1, gridHeight ) );
    m_groupGrid->SetMaxSize( wxSize( -1, gridHeight ) );
    m_pinTablePage->Layout();
}


void DIALOG_SYMBOL_PROPERTIES::RebuildGroupGrid( int aUnit )
{
    if( !m_groupDataModel )
        return;

    m_groupDataModel->CollectGroups( m_symbol, aUnit, m_dataModel );

    if( m_groupGrid )
    {
        if( m_groupDataModel->HasGroups() )
        {
            m_groupGrid->Show();
            sizeGroupGrid();
            m_groupGrid->ForceRefresh();
        }
        else
        {
            m_groupGrid->Hide();
        }

        m_pinTablePage->Layout();
    }

    // Non-mutating: a rebuild must not re-apply group activation, or it would undo the user's
    // subgroup and optional-member choices every time the grid refreshes.
    RefreshGroupStatus();
}


void DIALOG_SYMBOL_PROPERTIES::ApplyGroupActivation()
{
    if( !m_dataModel || !m_groupDataModel )
        return;

    std::set<wxString> activeGroups;

    for( const PIN_FUNCTION_GROUP_ROW& row : m_groupDataModel->GetRows() )
    {
        if( row.active )
            activeGroups.insert( row.groupName );
    }

    // Pass 1: clear alternates belonging to groups that are no longer active.  This also frees
    // the optional and subgroup members the user had assigned by hand.
    for( SCH_PIN& pin : *m_dataModel )
    {
        if( pin.GetAlt().IsEmpty() )
            continue;

        SCH_PIN* libPin = pin.GetLibPin();

        if( !libPin )
            continue;

        auto it = libPin->GetAlternates().find( pin.GetAlt() );

        if( it != libPin->GetAlternates().end() && !it->second.m_FunctionGroup.IsEmpty()
                && !activeGroups.count( it->second.m_FunctionGroup ) )
        {
            pin.SetAlt( wxEmptyString );
        }
    }

    // Pass 2: assign the required members of every active group.
    //
    // A pin can carry more than one required alternate of the same group, and GetAlternates() is
    // a map keyed by name, so "just take the first match" would silently pick the alphabetically
    // first one and make the others unreachable.  Instead: keep an already-valid choice, assign
    // when there is exactly one candidate, and leave a genuine multi-way choice to the user.
    for( SCH_PIN& pin : *m_dataModel )
    {
        SCH_PIN* libPin = pin.GetLibPin();

        if( !libPin )
            continue;

        std::vector<wxString> candidates;
        bool                  currentIsCandidate = false;

        for( const auto& [altName, alt] : libPin->GetAlternates() )
        {
            if( alt.m_FunctionGroup.IsEmpty() || !activeGroups.count( alt.m_FunctionGroup ) )
                continue;

            // Subgroup members are governed by their cardinality rule and plain optional members
            // by the user, so neither is force-assigned here.
            if( alt.IsOptionalGroupMember() )
                continue;

            candidates.push_back( altName );

            if( altName == pin.GetAlt() )
                currentIsCandidate = true;
        }

        if( currentIsCandidate )
            continue;                          // the user's existing pick is already valid

        if( candidates.size() == 1 )
            pin.SetAlt( candidates.front() );  // unambiguous
    }

    // Pass 3: top each subgroup up to its minimum, so enabling a group yields a usable
    // peripheral rather than one that is immediately incomplete.
    TopUpSubgroups( activeGroups );

    RefreshGroupStatus();
}


void DIALOG_SYMBOL_PROPERTIES::TopUpSubgroups( const std::set<wxString>& aActiveGroups )
{
    for( const PIN_FUNCTION_GROUP_ROW& row : m_groupDataModel->GetRows() )
    {
        if( !aActiveGroups.count( row.groupName ) )
            continue;

        for( const auto& [subgroupName, memberPins] : row.subgroupPinNumbers )
        {
            // Count what the user already has, and only make up the difference: ticking a group
            // must never override a chip select the user chose by hand.
            int assigned = 0;

            for( SCH_PIN& pin : *m_dataModel )
            {
                if( !memberPins.count( pin.GetNumber() ) || pin.GetAlt().IsEmpty() )
                    continue;

                SCH_PIN* libPin = pin.GetLibPin();

                if( !libPin )
                    continue;

                auto it = libPin->GetAlternates().find( pin.GetAlt() );

                if( it != libPin->GetAlternates().end()
                        && it->second.m_FunctionGroup == row.groupName
                        && it->second.m_FunctionGroupSubgroup == subgroupName )
                {
                    assigned++;
                }
            }

            int needed = row.SubgroupMinimum( subgroupName ) - assigned;

            if( needed <= 0 )
                continue;

            // Deterministic order: pin numbers sort numerically, not lexicographically, so the
            // choice does not depend on "10" sorting before "9".
            std::vector<wxString> ordered( memberPins.begin(), memberPins.end() );
            std::sort( ordered.begin(), ordered.end(),
                       []( const wxString& a, const wxString& b )
                       {
                           return PIN_NUMBERS::Compare( a, b ) < 0;
                       } );

            for( const wxString& pinNumber : ordered )
            {
                if( needed <= 0 )
                    break;

                for( SCH_PIN& pin : *m_dataModel )
                {
                    if( pin.GetNumber() != pinNumber || !pin.GetAlt().IsEmpty() )
                        continue;   // absent, or already doing something else -- never steal it

                    SCH_PIN* libPin = pin.GetLibPin();

                    if( !libPin )
                        continue;

                    for( const auto& [altName, alt] : libPin->GetAlternates() )
                    {
                        if( alt.m_FunctionGroup == row.groupName
                                && alt.m_FunctionGroupSubgroup == subgroupName )
                        {
                            pin.SetAlt( altName );
                            needed--;
                            break;
                        }
                    }

                    break;
                }
            }
        }
    }
}


void DIALOG_SYMBOL_PROPERTIES::RefreshGroupStatus()
{
    if( !m_dataModel || !m_groupDataModel )
        return;

    // Non-mutating: recompute which pins an active group is holding so the pin grid can lock
    // them, without touching any assignment.  Called on every refresh, so it must stay free of
    // side effects -- otherwise a redraw would undo the user's subgroup choice.
    std::set<wxString> groupControlledPins;

    for( const PIN_FUNCTION_GROUP_ROW& row : m_groupDataModel->GetRows() )
    {
        if( row.active )
            groupControlledPins.insert( row.pinNumbers.begin(), row.pinNumbers.end() );
    }

    // Mirror the pin assignments into the child rows so the two views cannot disagree.
    m_groupDataModel->RefreshMemberAssignments( *m_dataModel );

    // Assigning an optional or subgroup member changes which pins are claimed, so the set of
    // groups that can still be enabled changes with it.  Without this, a group whose required
    // pin was just taken by an optional member would keep showing as "Available" and would
    // silently steal that pin when enabled.
    m_groupDataModel->RecomputeConflicts();
    m_groupDataModel->BuildAttrs();

    m_dataModel->SetGroupControlledPins( groupControlledPins );
    m_dataModel->BuildAttrs();
}
