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
 * @file plm_schema.h
 * @brief Schema and attribute structures for the PLM client interface.
 *
 * This header defines structures for describing PLM metadata schemas
 * and storing attribute values. Schemas define what metadata fields
 * are available and their types.
 */

#ifndef PLM_SCHEMA_H
#define PLM_SCHEMA_H

#include "plm_types.h"

#include <wx/string.h>
#include <cstdint>
#include <vector>
#include <variant>


/**
 * Definition of a single attribute in the PLM schema.
 *
 * Describes the metadata, type, and constraints for one attribute
 * that can be stored with a PLM-managed file.
 */
struct PLM_ATTRIBUTE_SCHEMA
{
    wxString                 attr_id;           ///< Unique attribute identifier
    wxString                 display_name;      ///< Human-readable name for UI
    wxString                 description;       ///< Help text describing the attribute
    PLM_ATTR_TYPE            type;              ///< Data type of the attribute
    bool                     required = false;  ///< True if attribute must have a value
    bool                     editable_in_kicad = true;  ///< True if KiCad can modify this
    std::vector<wxString>    enum_values;       ///< Valid values for ENUM type
    wxString                 default_value;     ///< Default value for new files
    int                      display_order = 0; ///< Order for UI display

    PLM_ATTRIBUTE_SCHEMA() : type( PLM_ATTR_TYPE::STRING ) {}

    PLM_ATTRIBUTE_SCHEMA( const wxString& aId, const wxString& aDisplayName, PLM_ATTR_TYPE aType,
                          bool aRequired = false, bool aEditableInKicad = true ) :
            attr_id( aId ),
            display_name( aDisplayName ),
            type( aType ),
            required( aRequired ),
            editable_in_kicad( aEditableInKicad )
    {
    }

    /**
     * Check if this is an enumerated type with predefined values.
     * @return True if type is ENUM and has valid values.
     */
    bool IsEnum() const { return type == PLM_ATTR_TYPE::ENUM && !enum_values.empty(); }

    /**
     * Check if a value is valid for this attribute.
     * @param aValue The value to validate.
     * @return True if the value is acceptable.
     */
    bool IsValidValue( const wxString& aValue ) const
    {
        if( required && aValue.IsEmpty() )
            return false;

        if( type == PLM_ATTR_TYPE::ENUM && !aValue.IsEmpty() )
        {
            for( const auto& v : enum_values )
            {
                if( v == aValue )
                    return true;
            }

            return false;
        }

        return true;
    }
};


/**
 * Complete schema definition from a PLM system.
 *
 * Contains all attribute definitions and schema version information.
 */
struct PLM_SCHEMA
{
    wxString                          schema_version;  ///< Schema version identifier
    std::vector<PLM_ATTRIBUTE_SCHEMA> attributes;      ///< List of attribute definitions

    PLM_SCHEMA() = default;

    explicit PLM_SCHEMA( const wxString& aVersion ) : schema_version( aVersion ) {}

    /**
     * Find an attribute definition by ID.
     * @param aAttrId Attribute ID to find.
     * @return Pointer to the attribute schema, or nullptr if not found.
     */
    const PLM_ATTRIBUTE_SCHEMA* FindAttribute( const wxString& aAttrId ) const
    {
        for( const auto& attr : attributes )
        {
            if( attr.attr_id == aAttrId )
                return &attr;
        }

        return nullptr;
    }

    /**
     * Get the number of attributes in the schema.
     * @return Attribute count.
     */
    size_t AttributeCount() const { return attributes.size(); }

    /**
     * Check if schema is empty.
     * @return True if no attributes defined.
     */
    bool IsEmpty() const { return attributes.empty(); }

    /**
     * Get all required attributes.
     * @return Vector of pointers to required attribute schemas.
     */
    std::vector<const PLM_ATTRIBUTE_SCHEMA*> GetRequiredAttributes() const
    {
        std::vector<const PLM_ATTRIBUTE_SCHEMA*> result;

        for( const auto& attr : attributes )
        {
            if( attr.required )
                result.push_back( &attr );
        }

        return result;
    }

    /**
     * Get all editable attributes.
     * @return Vector of pointers to editable attribute schemas.
     */
    std::vector<const PLM_ATTRIBUTE_SCHEMA*> GetEditableAttributes() const
    {
        std::vector<const PLM_ATTRIBUTE_SCHEMA*> result;

        for( const auto& attr : attributes )
        {
            if( attr.editable_in_kicad )
                result.push_back( &attr );
        }

        return result;
    }
};


/**
 * A single attribute value.
 *
 * Stores the current value for one attribute along with modification tracking.
 */
struct PLM_ATTRIBUTE_VALUE
{
    wxString attr_id;           ///< Attribute identifier (matches schema)
    wxString value;             ///< Current value as string
    bool     is_modified = false;  ///< True if modified since last sync

    PLM_ATTRIBUTE_VALUE() = default;

    PLM_ATTRIBUTE_VALUE( const wxString& aAttrId, const wxString& aValue,
                         bool aIsModified = false ) :
            attr_id( aAttrId ),
            value( aValue ),
            is_modified( aIsModified )
    {
    }

    /**
     * Set a new value and mark as modified.
     * @param aValue The new value.
     */
    void SetValue( const wxString& aValue )
    {
        if( value != aValue )
        {
            value = aValue;
            is_modified = true;
        }
    }

    /**
     * Clear the modification flag (after sync).
     */
    void ClearModified() { is_modified = false; }
};


/**
 * Collection of attribute values for a file.
 *
 * Manages all attribute values and provides lookup and modification tracking.
 */
struct PLM_ATTRIBUTE_SET
{
    std::vector<PLM_ATTRIBUTE_VALUE> values;  ///< All attribute values

    PLM_ATTRIBUTE_SET() = default;

    /**
     * Find an attribute value by ID.
     * @param aAttrId Attribute ID to find.
     * @return Pointer to the value, or nullptr if not found.
     */
    PLM_ATTRIBUTE_VALUE* FindValue( const wxString& aAttrId )
    {
        for( auto& val : values )
        {
            if( val.attr_id == aAttrId )
                return &val;
        }

        return nullptr;
    }

    /**
     * Find an attribute value by ID (const version).
     * @param aAttrId Attribute ID to find.
     * @return Pointer to the value, or nullptr if not found.
     */
    const PLM_ATTRIBUTE_VALUE* FindValue( const wxString& aAttrId ) const
    {
        for( const auto& val : values )
        {
            if( val.attr_id == aAttrId )
                return &val;
        }

        return nullptr;
    }

    /**
     * Get or create an attribute value.
     * @param aAttrId Attribute ID.
     * @return Reference to the attribute value.
     */
    PLM_ATTRIBUTE_VALUE& GetOrCreate( const wxString& aAttrId )
    {
        PLM_ATTRIBUTE_VALUE* existing = FindValue( aAttrId );

        if( existing )
            return *existing;

        values.emplace_back( aAttrId, wxEmptyString );
        return values.back();
    }

    /**
     * Set an attribute value.
     * @param aAttrId Attribute ID.
     * @param aValue New value.
     */
    void SetValue( const wxString& aAttrId, const wxString& aValue )
    {
        GetOrCreate( aAttrId ).SetValue( aValue );
    }

    /**
     * Check if any attributes have been modified.
     * @return True if any values are marked as modified.
     */
    bool HasModifications() const
    {
        for( const auto& val : values )
        {
            if( val.is_modified )
                return true;
        }

        return false;
    }

    /**
     * Get all modified attribute values.
     * @return Vector of pointers to modified values.
     */
    std::vector<const PLM_ATTRIBUTE_VALUE*> GetModifiedValues() const
    {
        std::vector<const PLM_ATTRIBUTE_VALUE*> result;

        for( const auto& val : values )
        {
            if( val.is_modified )
                result.push_back( &val );
        }

        return result;
    }

    /**
     * Clear all modification flags.
     */
    void ClearAllModified()
    {
        for( auto& val : values )
            val.ClearModified();
    }
};


#endif // PLM_SCHEMA_H
