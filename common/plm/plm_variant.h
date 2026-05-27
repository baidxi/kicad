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
 * @file plm_variant.h
 * @brief Product variant structures for the PLM client interface.
 *
 * This header defines structures for managing product variants,
 * which allow different configurations of a design to coexist
 * with per-component overrides (substitutions, DNP, etc.).
 */

#ifndef PLM_VARIANT_H
#define PLM_VARIANT_H

#include "plm_types.h"

#include <wx/string.h>
#include <vector>


/**
 * Types of component overrides within a variant.
 */
enum class PLM_VARIANT_OVERRIDE_TYPE
{
    SUBSTITUTE = 0, ///< Replace component with a different part
    DNP = 1,        ///< Do Not Place (component omitted in this variant)
    VALUE = 2,      ///< Change component value only
    CUSTOM = 3      ///< Plugin-specific override
};


/**
 * A per-component override within a variant configuration.
 */
struct PLM_VARIANT_BOM_OVERRIDE
{
    wxString                    reference;          ///< Reference designator (e.g., "R1")
    PLM_VARIANT_OVERRIDE_TYPE   override_type = PLM_VARIANT_OVERRIDE_TYPE::SUBSTITUTE;
    wxString                    original_part_id;   ///< Original part in base BOM
    wxString                    replacement_part_id; ///< Replacement part (for SUBSTITUTE)
    wxString                    new_value;          ///< New value (for VALUE override)
    wxString                    notes;

    PLM_VARIANT_BOM_OVERRIDE() = default;

    PLM_VARIANT_BOM_OVERRIDE( const wxString& aReference,
                              PLM_VARIANT_OVERRIDE_TYPE aType ) :
            reference( aReference ),
            override_type( aType )
    {
    }

    static PLM_VARIANT_BOM_OVERRIDE Substitute( const wxString& aReference,
                                                 const wxString& aOriginal,
                                                 const wxString& aReplacement )
    {
        PLM_VARIANT_BOM_OVERRIDE o( aReference, PLM_VARIANT_OVERRIDE_TYPE::SUBSTITUTE );
        o.original_part_id = aOriginal;
        o.replacement_part_id = aReplacement;
        return o;
    }

    static PLM_VARIANT_BOM_OVERRIDE DoNotPlace( const wxString& aReference )
    {
        return PLM_VARIANT_BOM_OVERRIDE( aReference, PLM_VARIANT_OVERRIDE_TYPE::DNP );
    }
};


/**
 * A named variant configuration with its BOM overrides.
 */
struct PLM_VARIANT_CONFIG
{
    wxString                                config_id;      ///< Unique configuration ID
    wxString                                name;           ///< Display name (e.g., "WiFi-only")
    wxString                                description;
    std::vector<PLM_VARIANT_BOM_OVERRIDE>   overrides;      ///< Component overrides
    bool                                    active = true;

    PLM_VARIANT_CONFIG() = default;

    PLM_VARIANT_CONFIG( const wxString& aId, const wxString& aName ) :
            config_id( aId ),
            name( aName )
    {
    }

    size_t OverrideCount() const { return overrides.size(); }

    size_t DnpCount() const
    {
        size_t count = 0;

        for( const auto& o : overrides )
        {
            if( o.override_type == PLM_VARIANT_OVERRIDE_TYPE::DNP )
                ++count;
        }

        return count;
    }
};


/**
 * Top-level variant definition for a project.
 *
 * Groups all variant configurations for a single project, allowing
 * the PLM system to track which configurations exist and their
 * relationship to the base design.
 */
struct PLM_VARIANT
{
    wxString                            project_id;
    wxString                            base_bom_id;    ///< BOM that variants derive from
    std::vector<PLM_VARIANT_CONFIG>     configs;

    PLM_VARIANT() = default;

    explicit PLM_VARIANT( const wxString& aProjectId ) :
            project_id( aProjectId )
    {
    }

    const PLM_VARIANT_CONFIG* FindConfig( const wxString& aConfigId ) const
    {
        for( const auto& cfg : configs )
        {
            if( cfg.config_id == aConfigId )
                return &cfg;
        }

        return nullptr;
    }
};


/**
 * Result of a variant operation.
 */
struct PLM_VARIANT_RESULT
{
    bool            success = false;
    PLM_ERROR_CODE  error_code = PLM_ERROR_CODE::SUCCESS;
    wxString        error_message;
    wxString        config_id;      ///< ID of the created/updated config

    PLM_VARIANT_RESULT() = default;

    static PLM_VARIANT_RESULT Success( const wxString& aConfigId )
    {
        PLM_VARIANT_RESULT r;
        r.success = true;
        r.config_id = aConfigId;
        return r;
    }

    static PLM_VARIANT_RESULT Failure( PLM_ERROR_CODE aCode, const wxString& aMessage )
    {
        PLM_VARIANT_RESULT r;
        r.error_code = aCode;
        r.error_message = aMessage;
        return r;
    }
};


#endif // PLM_VARIANT_H
