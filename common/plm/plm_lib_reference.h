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
 * @file plm_lib_reference.h
 * @brief PLM library reference storage for project files.
 */

#ifndef PLM_LIB_REFERENCE_H
#define PLM_LIB_REFERENCE_H

#include <kicommon.h>

#include <wx/string.h>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>


/**
 * Type of library referenced by PLM.
 */
enum class PLM_LIB_TYPE
{
    SYMBOL,      ///< Symbol library
    FOOTPRINT,   ///< Footprint library
    MODEL_3D     ///< 3D model library
};


/**
 * A reference to a PLM-managed library.
 *
 * Stored with the project to track which PLM libraries are used and
 * their versions at checkout time.
 */
struct PLM_LIB_REFERENCE
{
    wxString      lib_id;         ///< PLM library identifier
    wxString      lib_name;       ///< Display name
    PLM_LIB_TYPE  lib_type;       ///< Type of library
    wxString      revision;       ///< Revision at checkout
    wxString      local_path;     ///< Local cached path
    wxString      server_url;     ///< PLM server URL
    int64_t       sync_timestamp; ///< Last sync time (Unix epoch ms)
    bool          auto_sync;      ///< Auto-sync on project open

    PLM_LIB_REFERENCE() :
            lib_type( PLM_LIB_TYPE::SYMBOL ), sync_timestamp( 0 ), auto_sync( true )
    {
    }

    bool IsValid() const { return !lib_id.IsEmpty(); }

    /**
     * Get string representation of library type.
     */
    wxString GetTypeString() const
    {
        switch( lib_type )
        {
        case PLM_LIB_TYPE::SYMBOL:    return wxS( "symbol" );
        case PLM_LIB_TYPE::FOOTPRINT: return wxS( "footprint" );
        case PLM_LIB_TYPE::MODEL_3D:  return wxS( "3d_model" );
        default:                      return wxS( "unknown" );
        }
    }

    /**
     * Parse library type from string.
     */
    static PLM_LIB_TYPE ParseType( const wxString& aType )
    {
        if( aType == wxS( "symbol" ) )
            return PLM_LIB_TYPE::SYMBOL;

        if( aType == wxS( "footprint" ) )
            return PLM_LIB_TYPE::FOOTPRINT;

        if( aType == wxS( "3d_model" ) )
            return PLM_LIB_TYPE::MODEL_3D;

        return PLM_LIB_TYPE::SYMBOL;
    }
};


/**
 * Result of a library sync operation.
 */
struct PLM_LIB_SYNC_RESULT
{
    wxString lib_id;          ///< Library identifier
    bool     success;         ///< True if sync succeeded
    bool     updated;         ///< True if library was updated
    wxString new_revision;    ///< New revision after sync
    wxString error_message;   ///< Error message if failed

    PLM_LIB_SYNC_RESULT() : success( false ), updated( false ) {}
};


/**
 * Callback for library sync completion.
 */
using PLM_LIB_SYNC_CALLBACK = std::function<void( const PLM_LIB_SYNC_RESULT& aResult )>;


/**
 * PLM library reference manager.
 *
 * Manages library references stored with a project. References track
 * which PLM libraries are used by the project and their versions.
 *
 * Usage:
 * @code
 * PLM_LIB_REFERENCE_MANAGER mgr;
 *
 * // Add a library reference
 * PLM_LIB_REFERENCE ref;
 * ref.lib_id = "corporate/symbols/standard";
 * ref.lib_type = PLM_LIB_TYPE::SYMBOL;
 * ref.revision = "1.2.3";
 * mgr.AddReference( ref );
 *
 * // Save references with project
 * mgr.SaveToFile( "/path/to/project-plm-libs.json" );
 *
 * // Load on project open
 * mgr.LoadFromFile( "/path/to/project-plm-libs.json" );
 * @endcode
 */
class KICOMMON_API PLM_LIB_REFERENCE_MANAGER
{
public:
    PLM_LIB_REFERENCE_MANAGER();
    ~PLM_LIB_REFERENCE_MANAGER();

    /**
     * Add or update a library reference.
     *
     * @param aRef Library reference.
     */
    void AddReference( const PLM_LIB_REFERENCE& aRef );

    /**
     * Remove a library reference.
     *
     * @param aLibId Library identifier.
     */
    void RemoveReference( const wxString& aLibId );

    /**
     * Check if a library is referenced.
     *
     * @param aLibId Library identifier.
     * @return True if referenced.
     */
    bool HasReference( const wxString& aLibId ) const;

    /**
     * Get a library reference.
     *
     * @param aLibId Library identifier.
     * @return Reference, or nullopt if not found.
     */
    std::optional<PLM_LIB_REFERENCE> GetReference( const wxString& aLibId ) const;

    /**
     * Get all library references.
     *
     * @return Vector of all references.
     */
    std::vector<PLM_LIB_REFERENCE> GetAllReferences() const;

    /**
     * Get references by type.
     *
     * @param aType Library type.
     * @return Vector of matching references.
     */
    std::vector<PLM_LIB_REFERENCE> GetReferencesByType( PLM_LIB_TYPE aType ) const;

    /**
     * Get count of references.
     *
     * @return Number of references.
     */
    size_t GetReferenceCount() const;

    /**
     * Update the revision for a library.
     *
     * @param aLibId Library identifier.
     * @param aNewRevision New revision.
     */
    void UpdateRevision( const wxString& aLibId, const wxString& aNewRevision );

    /**
     * Update the local path for a library.
     *
     * @param aLibId Library identifier.
     * @param aLocalPath New local path.
     */
    void UpdateLocalPath( const wxString& aLibId, const wxString& aLocalPath );

    /**
     * Set auto-sync flag for a library.
     *
     * @param aLibId Library identifier.
     * @param aAutoSync True to enable auto-sync.
     */
    void SetAutoSync( const wxString& aLibId, bool aAutoSync );

    /**
     * Get libraries that need syncing.
     *
     * Returns libraries with auto-sync enabled.
     *
     * @return Vector of library IDs.
     */
    std::vector<wxString> GetLibrariesToSync() const;

    /**
     * Mark a library as synced.
     *
     * Updates sync timestamp to current time.
     *
     * @param aLibId Library identifier.
     */
    void MarkSynced( const wxString& aLibId );

    /**
     * Clear all references.
     */
    void Clear();

    /**
     * Check if references have been modified.
     *
     * @return True if modified since last save/load.
     */
    bool IsModified() const { return m_modified; }

    /**
     * Clear modified flag.
     */
    void ClearModified() { m_modified = false; }

    /**
     * Save references to JSON string.
     *
     * @return JSON representation.
     */
    wxString ToJson() const;

    /**
     * Load references from JSON string.
     *
     * @param aJson JSON string.
     * @return True on success.
     */
    bool FromJson( const wxString& aJson );

    /**
     * Save references to file.
     *
     * @param aPath File path.
     * @return True on success.
     */
    bool SaveToFile( const wxString& aPath ) const;

    /**
     * Load references from file.
     *
     * @param aPath File path.
     * @return True on success.
     */
    bool LoadFromFile( const wxString& aPath );

    /**
     * Get default storage path for project.
     *
     * @param aProjectPath Path to project file.
     * @return Path for library reference file.
     */
    static wxString GetDefaultPath( const wxString& aProjectPath );

    /**
     * Get library type display name.
     *
     * @param aType Library type.
     * @return Localized display name.
     */
    static wxString GetTypeDisplayName( PLM_LIB_TYPE aType );

private:
    std::map<wxString, PLM_LIB_REFERENCE> m_references;
    bool                                   m_modified;
};


#endif // PLM_LIB_REFERENCE_H
