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

#pragma once

/*
 * Value types describing pin function groups -- named peripherals (SPI0, UART1, ...) whose member
 * alternates a library symbol tags on its pins.
 *
 * These live in their own header because both LIB_SYMBOL (which owns the group descriptions) and
 * SCH_SYMBOL (which evaluates them against an instance's alternate assignments) need them, and
 * sch_symbol.h deliberately only forward-declares LIB_SYMBOL rather than taking that heavy
 * include.  Everything here is a plain value type over wxString and std containers.
 */

#include <map>
#include <set>
#include <utility>

#include <wx/string.h>


/**
 * A cardinality constraint on a named subgroup of a pin function group: how many of the
 * subgroup's members must be assigned for the enclosing group to be validly configured.
 *
 * Lives on the group description rather than on the member alternates so that the rule has
 * exactly one statement; four chip-selects each claiming their own minimum could contradict
 * one another with no defined resolution.
 */
struct PIN_FUNCTION_SUBGROUP_RULE
{
    /// Minimum number of the subgroup's members that must be assigned.  0 means no constraint.
    int min = 1;

    bool operator==( const PIN_FUNCTION_SUBGROUP_RULE& aOther ) const { return min == aOther.min; }

    bool operator!=( const PIN_FUNCTION_SUBGROUP_RULE& aOther ) const
    {
        return !( *this == aOther );
    }

    bool operator<( const PIN_FUNCTION_SUBGROUP_RULE& aOther ) const { return min < aOther.min; }
};


struct PIN_FUNCTION_GROUP_DESC
{
    wxString description;
    wxString bus;       ///< If non-empty, groups sharing the same bus name are mutually exclusive

    /// Explicitly declared cardinality rules, keyed by subgroup name.  A subgroup that has
    /// members but no entry here uses the PIN_FUNCTION_SUBGROUP_RULE default (min 1), so the
    /// common "at least one of these" case needs no entry at all.
    std::map<wxString, PIN_FUNCTION_SUBGROUP_RULE> subgroups;

    bool operator==( const PIN_FUNCTION_GROUP_DESC& aOther ) const
    {
        return description == aOther.description && bus == aOther.bus
                && subgroups == aOther.subgroups;
    }

    bool operator!=( const PIN_FUNCTION_GROUP_DESC& aOther ) const { return !( *this == aOther ); }

    bool operator<( const PIN_FUNCTION_GROUP_DESC& aOther ) const
    {
        if( description != aOther.description )
            return description < aOther.description;

        if( bus != aOther.bus )
            return bus < aOther.bus;

        return subgroups < aOther.subgroups;
    }
};


/**
 * How completely a pin function group is configured on a particular symbol instance.
 */
enum class PIN_FUNCTION_GROUP_STATE
{
    INACTIVE,     ///< No member alternate is assigned.  Never an error: an unused peripheral.
    INCOMPLETE,   ///< Active, but a required member or a subgroup minimum is unmet.
    COMPLETE      ///< Active and every constraint is satisfied.
};


struct PIN_FUNCTION_GROUP_STATUS
{
    wxString                 groupName;
    PIN_FUNCTION_GROUP_STATE state = PIN_FUNCTION_GROUP_STATE::INACTIVE;

    /// Pin numbers of required members that are not assigned.  INCOMPLETE only.
    std::set<wxString> missingRequiredPins;

    /// Subgroup name -> { assigned count, required minimum } for subgroups below their minimum.
    /// INCOMPLETE only.
    std::map<wxString, std::pair<int, int>> unmetSubgroups;

    /// Localized one-line explanation; empty unless INCOMPLETE.
    wxString message;

    bool IsValid() const { return state != PIN_FUNCTION_GROUP_STATE::INCOMPLETE; }
};
