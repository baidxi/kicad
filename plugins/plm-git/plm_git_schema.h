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
 * @file plm_git_schema.h
 * @brief Git-based attribute storage for PLM metadata.
 *
 * Provides attribute storage for Git-based PLM operations using
 * a JSON file (.kicad_plm_attrs) to store project metadata alongside
 * the KiCad project files.
 */

#ifndef PLM_GIT_SCHEMA_H
#define PLM_GIT_SCHEMA_H

#include <plm/plm_schema.h>

#include <wx/string.h>


/**
 * Git attribute storage manager.
 *
 * Manages PLM attributes stored in a JSON file within the Git repository.
 * Attributes are stored per-project in a .kicad_plm_attrs file.
 */
class GIT_SCHEMA_MANAGER
{
public:
    GIT_SCHEMA_MANAGER();
    ~GIT_SCHEMA_MANAGER();

    /**
     * Get the default schema for Git-based projects.
     *
     * Returns a schema with common attributes suitable for version control:
     * - description (string)
     * - author (string)
     * - version (string)
     * - status (enum: draft, review, released, obsolete)
     * - created_date (datetime)
     * - modified_date (datetime)
     *
     * @return Default attribute schema.
     */
    PLM_SCHEMA GetDefaultSchema() const;

    /**
     * Load attributes from a project directory.
     *
     * Reads the .kicad_plm_attrs file from the project directory
     * and returns the attribute values.
     *
     * @param aProjectPath Path to the project directory.
     * @return Attribute values, or empty set if file not found.
     */
    PLM_ATTRIBUTE_SET LoadAttributes( const wxString& aProjectPath );

    /**
     * Save attributes to a project directory.
     *
     * Writes attribute values to the .kicad_plm_attrs file in the
     * project directory.
     *
     * @param aProjectPath Path to the project directory.
     * @param aAttributes Attribute values to save.
     * @return True if save succeeded.
     */
    bool SaveAttributes( const wxString& aProjectPath, const PLM_ATTRIBUTE_SET& aAttributes );

    /**
     * Get the path to the attributes file for a project.
     *
     * @param aProjectPath Path to the project directory.
     * @return Full path to the .kicad_plm_attrs file.
     */
    static wxString GetAttributesFilePath( const wxString& aProjectPath );

    /**
     * Check if a project has stored attributes.
     *
     * @param aProjectPath Path to the project directory.
     * @return True if .kicad_plm_attrs file exists.
     */
    static bool HasAttributes( const wxString& aProjectPath );

private:
    /**
     * Create a default attribute definition.
     *
     * @param aId Attribute ID.
     * @param aName Display name.
     * @param aType Attribute type.
     * @param aRequired Whether attribute is required.
     * @return Attribute schema definition.
     */
    static PLM_ATTRIBUTE_SCHEMA CreateAttributeDef( const wxString& aId, const wxString& aName,
                                                     PLM_ATTR_TYPE aType, bool aRequired = false );

    /**
     * Parse JSON content to attribute set.
     *
     * @param aJson JSON string content.
     * @return Parsed attribute values.
     */
    PLM_ATTRIBUTE_SET ParseJson( const wxString& aJson );

    /**
     * Serialize attribute set to JSON.
     *
     * @param aAttributes Attribute values to serialize.
     * @return JSON string.
     */
    wxString ToJson( const PLM_ATTRIBUTE_SET& aAttributes );
};


#endif // PLM_GIT_SCHEMA_H
