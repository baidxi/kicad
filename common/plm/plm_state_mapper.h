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
 * @file plm_state_mapper.h
 * @brief State category mapping for PLM systems.
 *
 * This header defines the PLM_STATE_MAPPER class which maps native PLM state
 * strings (e.g., "In Review", "Released", "Draft") to PLM_STATE_CATEGORY values.
 * The mappings are retrieved from the plugin and cached for the session.
 */

#ifndef PLM_STATE_MAPPER_H
#define PLM_STATE_MAPPER_H

#include "plm_types.h"

#include <wx/string.h>
#include <functional>
#include <map>
#include <mutex>
#include <vector>


/**
 * Information about a mapped state.
 *
 * Contains the native state name from the PLM system along with
 * its mapped category and display information.
 */
struct PLM_STATE_INFO
{
    wxString           native_state;    ///< State name from PLM system
    PLM_STATE_CATEGORY category;        ///< Mapped behavior category
    wxString           display_name;    ///< User-friendly display name
    wxString           description;     ///< Optional description of state
    bool               is_default = false;  ///< True if this is the default state

    PLM_STATE_INFO() : category( PLM_STATE_CATEGORY::EDITABLE ) {}

    PLM_STATE_INFO( const wxString& aNativeState, PLM_STATE_CATEGORY aCategory,
                    const wxString& aDisplayName = wxEmptyString,
                    const wxString& aDescription = wxEmptyString ) :
            native_state( aNativeState ),
            category( aCategory ),
            display_name( aDisplayName.IsEmpty() ? aNativeState : aDisplayName ),
            description( aDescription )
    {
    }
};


/**
 * Callback type for fetching state mappings from a plugin.
 *
 * The callback should return a vector of state info structures
 * representing all known states in the PLM system.
 *
 * @return Vector of state information.
 */
using PLM_STATE_MAPPING_CALLBACK = std::function<std::vector<PLM_STATE_INFO>()>;


/**
 * Maps native PLM state strings to behavior categories.
 *
 * The state mapper translates PLM-specific state names into
 * PLM_STATE_CATEGORY values that KiCad uses to determine what
 * operations are permitted. Mappings are cached after first retrieval.
 *
 * Thread-safe: All public methods are protected by mutex.
 *
 * Usage:
 * @code
 * PLM_STATE_MAPPER mapper;
 *
 * // Set callback to fetch mappings from plugin
 * mapper.SetMappingCallback( [&plugin]() {
 *     return plugin->GetStateMappings();
 * });
 *
 * // Load mappings (or call GetCategory which loads automatically)
 * mapper.LoadMappings();
 *
 * // Get category for a state
 * PLM_STATE_CATEGORY cat = mapper.GetCategory( "In Review" );
 *
 * // Get full state info
 * PLM_STATE_INFO info;
 * if( mapper.GetStateInfo( "Released", info ) )
 * {
 *     // Use info.display_name, info.description, etc.
 * }
 * @endcode
 */
class PLM_STATE_MAPPER
{
public:
    PLM_STATE_MAPPER();
    ~PLM_STATE_MAPPER() = default;

    // Non-copyable
    PLM_STATE_MAPPER( const PLM_STATE_MAPPER& ) = delete;
    PLM_STATE_MAPPER& operator=( const PLM_STATE_MAPPER& ) = delete;

    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------

    /**
     * Set the callback for fetching state mappings.
     *
     * @param aCallback Function that returns state mappings from plugin.
     */
    void SetMappingCallback( PLM_STATE_MAPPING_CALLBACK aCallback );

    /**
     * Load state mappings from the plugin.
     *
     * Fetches mappings using the registered callback. This is called
     * automatically on first access if not called explicitly.
     *
     * @return True if mappings were loaded successfully.
     */
    bool LoadMappings();

    /**
     * Clear cached mappings.
     *
     * Call this when switching PLM connections or on logout.
     */
    void ClearMappings();

    /**
     * Check if mappings have been loaded.
     *
     * @return True if mappings are available.
     */
    bool HasMappings() const;

    //-------------------------------------------------------------------------
    // State Lookup
    //-------------------------------------------------------------------------

    /**
     * Get the category for a native state string.
     *
     * If the state is not found in the mappings, returns EDITABLE as default.
     * Triggers LoadMappings() if not already loaded.
     *
     * @param aNativeState State string from PLM system.
     * @return Mapped category, or EDITABLE if unknown.
     */
    PLM_STATE_CATEGORY GetCategory( const wxString& aNativeState );

    /**
     * Get full state information for a native state.
     *
     * @param aNativeState State string from PLM system.
     * @param aInfo Output state info structure.
     * @return True if state was found.
     */
    bool GetStateInfo( const wxString& aNativeState, PLM_STATE_INFO& aInfo );

    /**
     * Get all known states.
     *
     * @return Vector of all state info structures.
     */
    std::vector<PLM_STATE_INFO> GetAllStates();

    /**
     * Get states by category.
     *
     * @param aCategory The category to filter by.
     * @return Vector of state info structures matching the category.
     */
    std::vector<PLM_STATE_INFO> GetStatesByCategory( PLM_STATE_CATEGORY aCategory );

    /**
     * Get the default state (if one is defined).
     *
     * @param aInfo Output state info structure.
     * @return True if a default state is defined.
     */
    bool GetDefaultState( PLM_STATE_INFO& aInfo );

    //-------------------------------------------------------------------------
    // Display Helpers
    //-------------------------------------------------------------------------

    /**
     * Get a display-friendly category name.
     *
     * @param aCategory The category.
     * @return Human-readable category name.
     */
    static wxString GetCategoryDisplayName( PLM_STATE_CATEGORY aCategory );

    /**
     * Get a description of what a category means.
     *
     * @param aCategory The category.
     * @return Description of category behavior.
     */
    static wxString GetCategoryDescription( PLM_STATE_CATEGORY aCategory );

private:
    /**
     * Ensure mappings are loaded.
     */
    void EnsureMappingsLoaded();

    mutable std::mutex                    m_mutex;
    std::map<wxString, PLM_STATE_INFO>    m_mappings;
    PLM_STATE_MAPPING_CALLBACK            m_mappingCallback;
    bool                                  m_mappingsLoaded = false;
};


#endif // PLM_STATE_MAPPER_H
