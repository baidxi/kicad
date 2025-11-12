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
 * @file plm_project_binding.h
 * @brief Project-to-PLM binding structures and management.
 *
 * This header defines structures for binding a KiCad project to a PLM system.
 * Bindings are stored in the .kicad_pro file and loaded when the project opens.
 */

#ifndef PLM_PROJECT_BINDING_H
#define PLM_PROJECT_BINDING_H

#include <kicommon.h>
#include "plm_types.h"

#include <wx/string.h>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>


/**
 * Binding state indicating how the project is bound to PLM.
 */
enum class PLM_BINDING_STATE
{
    UNBOUND,            ///< Project has no PLM binding
    BOUND,              ///< Project is bound to PLM
    BINDING_INVALID,    ///< Binding exists but is invalid (e.g., plugin missing)
    BINDING_OFFLINE     ///< Binding exists but server is unreachable
};


/**
 * Information binding a KiCad project to a PLM project.
 *
 * Stored in the .kicad_pro file and loaded when the project opens.
 */
struct PLM_PROJECT_BINDING
{
    wxString plugin_id;          ///< ID of the PLM plugin to use
    wxString server_url;         ///< PLM server URL
    wxString project_id;         ///< PLM project/file identifier
    wxString project_path;       ///< Path within PLM system
    wxString last_revision;      ///< Last synced revision
    int64_t  last_sync = 0;      ///< Timestamp of last sync (Unix epoch seconds)
    wxString checkout_id;        ///< Checkout transaction ID (if checked out)

    PLM_PROJECT_BINDING() = default;

    PLM_PROJECT_BINDING( const wxString& aPluginId, const wxString& aServerUrl,
                         const wxString& aProjectId, const wxString& aProjectPath = wxEmptyString ) :
            plugin_id( aPluginId ),
            server_url( aServerUrl ),
            project_id( aProjectId ),
            project_path( aProjectPath )
    {
    }

    /**
     * Check if binding has minimum required fields.
     * @return True if all required fields are set.
     */
    bool IsValid() const
    {
        return !plugin_id.IsEmpty() && !server_url.IsEmpty() && !project_id.IsEmpty();
    }

    /**
     * Check if project is currently checked out.
     * @return True if checkout_id is set.
     */
    bool IsCheckedOut() const { return !checkout_id.IsEmpty(); }

    /**
     * Clear checkout state.
     */
    void ClearCheckout() { checkout_id.Clear(); }

    /**
     * Update sync information.
     * @param aRevision The synced revision.
     */
    void UpdateSync( const wxString& aRevision )
    {
        last_revision = aRevision;
        last_sync = std::time( nullptr );
    }

    /**
     * Clear all binding data.
     */
    void Clear()
    {
        plugin_id.Clear();
        server_url.Clear();
        project_id.Clear();
        project_path.Clear();
        last_revision.Clear();
        last_sync = 0;
        checkout_id.Clear();
    }
};


/**
 * Callback type for binding change notifications.
 *
 * @param aLocalPath Local project path.
 * @param aBinding The new binding (empty if unbound).
 */
using PLM_BINDING_CHANGE_CALLBACK = std::function<void( const wxString& aLocalPath,
                                                         const PLM_PROJECT_BINDING& aBinding )>;


/**
 * Manages project-to-PLM bindings.
 *
 * Tracks bindings for open projects and provides methods to create,
 * load, and persist bindings. Bindings are stored in the project file.
 *
 * Thread-safe: All public methods are protected by mutex.
 *
 * Usage:
 * @code
 * PLM_PROJECT_BINDING_MANAGER bindingMgr;
 *
 * // Set change callback
 * bindingMgr.SetChangeCallback( []( const wxString& path, const PLM_PROJECT_BINDING& binding ) {
 *     // Update UI
 * });
 *
 * // Load binding when project opens
 * bindingMgr.LoadBinding( projectPath, binding );
 *
 * // Create binding when checking out from PLM
 * PLM_PROJECT_BINDING binding( "git-plugin", "https://server.com", "proj-123" );
 * bindingMgr.SetBinding( projectPath, binding );
 *
 * // Get current binding
 * PLM_PROJECT_BINDING current;
 * if( bindingMgr.GetBinding( projectPath, current ) )
 * {
 *     // Project is bound to PLM
 * }
 *
 * // Remove binding when closing project
 * bindingMgr.RemoveBinding( projectPath );
 * @endcode
 */
class KICOMMON_API PLM_PROJECT_BINDING_MANAGER
{
public:
    /**
     * Get the singleton instance.
     *
     * @return Reference to the global binding manager.
     */
    static PLM_PROJECT_BINDING_MANAGER& GetInstance();

    ~PLM_PROJECT_BINDING_MANAGER() = default;

    // Non-copyable
    PLM_PROJECT_BINDING_MANAGER( const PLM_PROJECT_BINDING_MANAGER& ) = delete;
    PLM_PROJECT_BINDING_MANAGER& operator=( const PLM_PROJECT_BINDING_MANAGER& ) = delete;

private:
    PLM_PROJECT_BINDING_MANAGER();

public:

    //-------------------------------------------------------------------------
    // Binding Management
    //-------------------------------------------------------------------------

    /**
     * Set or update binding for a project.
     *
     * @param aLocalPath Local project path.
     * @param aBinding Binding information.
     */
    void SetBinding( const wxString& aLocalPath, const PLM_PROJECT_BINDING& aBinding );

    /**
     * Load binding into the manager (without persisting).
     *
     * Call this when loading a binding from the project file.
     *
     * @param aLocalPath Local project path.
     * @param aBinding Binding information.
     */
    void LoadBinding( const wxString& aLocalPath, const PLM_PROJECT_BINDING& aBinding );

    /**
     * Get binding for a project.
     *
     * @param aLocalPath Local project path.
     * @param aBinding Output binding information.
     * @return True if binding exists.
     */
    bool GetBinding( const wxString& aLocalPath, PLM_PROJECT_BINDING& aBinding ) const;

    /**
     * Remove binding for a project.
     *
     * Call this when project is closed or unbound.
     *
     * @param aLocalPath Local project path.
     */
    void RemoveBinding( const wxString& aLocalPath );

    /**
     * Remove all bindings.
     */
    void RemoveAllBindings();

    /**
     * Check if a project has a binding.
     *
     * @param aLocalPath Local project path.
     * @return True if project is bound to PLM.
     */
    bool HasBinding( const wxString& aLocalPath ) const;

    //-------------------------------------------------------------------------
    // Binding Queries
    //-------------------------------------------------------------------------

    /**
     * Get binding state for a project.
     *
     * @param aLocalPath Local project path.
     * @return Binding state.
     */
    PLM_BINDING_STATE GetBindingState( const wxString& aLocalPath ) const;

    /**
     * Get all bound project paths.
     *
     * @return Vector of local project paths with bindings.
     */
    std::vector<wxString> GetBoundProjects() const;

    /**
     * Find projects bound to a specific server.
     *
     * @param aServerUrl Server URL to search for.
     * @return Vector of local project paths.
     */
    std::vector<wxString> FindProjectsByServer( const wxString& aServerUrl ) const;

    /**
     * Find projects using a specific plugin.
     *
     * @param aPluginId Plugin ID to search for.
     * @return Vector of local project paths.
     */
    std::vector<wxString> FindProjectsByPlugin( const wxString& aPluginId ) const;

    //-------------------------------------------------------------------------
    // Sync State
    //-------------------------------------------------------------------------

    /**
     * Update sync information for a project.
     *
     * @param aLocalPath Local project path.
     * @param aRevision Synced revision.
     */
    void UpdateSync( const wxString& aLocalPath, const wxString& aRevision );

    /**
     * Set checkout state for a project.
     *
     * @param aLocalPath Local project path.
     * @param aCheckoutId Checkout transaction ID.
     */
    void SetCheckoutId( const wxString& aLocalPath, const wxString& aCheckoutId );

    /**
     * Clear checkout state for a project.
     *
     * @param aLocalPath Local project path.
     */
    void ClearCheckout( const wxString& aLocalPath );

    //-------------------------------------------------------------------------
    // Callbacks
    //-------------------------------------------------------------------------

    /**
     * Set callback for binding changes.
     *
     * @param aCallback Change callback function.
     */
    void SetChangeCallback( PLM_BINDING_CHANGE_CALLBACK aCallback );

private:
    /**
     * Normalize a path for consistent keys.
     */
    static wxString NormalizePath( const wxString& aPath );

    /**
     * Fire a binding change event.
     */
    void FireChangeEvent( const wxString& aLocalPath, const PLM_PROJECT_BINDING& aBinding );

    mutable std::mutex                        m_mutex;
    std::map<wxString, PLM_PROJECT_BINDING>   m_bindings;
    PLM_BINDING_CHANGE_CALLBACK               m_changeCallback;
};


#endif // PLM_PROJECT_BINDING_H
