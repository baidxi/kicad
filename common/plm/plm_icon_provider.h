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
 * @file plm_icon_provider.h
 * @brief Status icon provider for PLM file states.
 *
 * This provider manages icons that represent different PLM states for files
 * in the project tree and other UI elements. It supports core icons for
 * standard states and allows plugins to register custom icons.
 */

#ifndef PLM_ICON_PROVIDER_H
#define PLM_ICON_PROVIDER_H

#include <kicommon.h>
#include "plm_types.h"

#include <wx/string.h>
#include <wx/bitmap.h>
#include <map>
#include <memory>
#include <functional>


class wxBitmapBundle;


/**
 * PLM file status for icon display.
 *
 * These states map to specific icons that visually represent the
 * PLM status of a file in the UI.
 */
enum class PLM_ICON_STATE
{
    UNMANAGED,              ///< File not under PLM control
    CLEAN,                  ///< File in sync with PLM server
    MODIFIED,               ///< Local modifications not committed
    LOCKED_BY_ME,           ///< File locked by current user
    LOCKED_BY_OTHER,        ///< File locked by another user
    CONFLICT,               ///< Local and server versions conflict
    OUTDATED,               ///< Server has newer version
    PENDING_UPLOAD,         ///< Local changes pending upload
    ERROR_STATE             ///< Error state (connection issue, etc.)
};


/**
 * Information about a registered icon.
 */
struct PLM_ICON_INFO
{
    wxString          icon_id;          ///< Unique identifier
    wxString          description;      ///< Human-readable description
    wxString          svg_path;         ///< Path to SVG file (for custom icons)
    PLM_ICON_STATE    state;            ///< State this icon represents
    bool              from_plugin;      ///< True if registered by a plugin

    PLM_ICON_INFO() : state( PLM_ICON_STATE::UNMANAGED ), from_plugin( false ) {}

    PLM_ICON_INFO( const wxString& aId, const wxString& aDesc, PLM_ICON_STATE aState ) :
            icon_id( aId ), description( aDesc ), state( aState ), from_plugin( false )
    {
    }
};


/**
 * Callback for custom icon loading from plugins.
 *
 * @param aIconId The icon identifier to load.
 * @param aSize Requested icon size.
 * @return The loaded bitmap, or invalid bitmap if not found.
 */
using PLM_ICON_LOADER = std::function<wxBitmap( const wxString& aIconId, int aSize )>;


/**
 * Provider for PLM status icons.
 *
 * Manages icons that represent file states in the PLM system. Provides
 * core icons for standard states and supports plugin-registered custom icons.
 *
 * Core icons include:
 * - Unmanaged: gray file icon
 * - Clean: green checkmark overlay
 * - Modified: yellow pencil overlay
 * - Locked by me: blue lock overlay
 * - Locked by other: red lock overlay
 * - Conflict: red exclamation overlay
 * - Outdated: orange arrow overlay
 *
 * Usage:
 * @code
 * PLM_ICON_PROVIDER& provider = PLM_ICON_PROVIDER::GetInstance();
 *
 * // Get icon for a state
 * wxBitmap icon = provider.GetIcon( PLM_ICON_STATE::MODIFIED, 16 );
 *
 * // Register custom plugin icon
 * provider.RegisterPluginIcon( "my_custom_state", "My State", loader );
 * @endcode
 */
class KICOMMON_API PLM_ICON_PROVIDER
{
public:
    /**
     * Get the singleton instance.
     *
     * @return Reference to the icon provider.
     */
    static PLM_ICON_PROVIDER& GetInstance();

    /**
     * Get icon for a PLM state.
     *
     * @param aState The state to get icon for.
     * @param aSize Requested icon size in pixels (16, 24, 32, 48).
     * @return Bitmap for the state, or default icon if not found.
     */
    wxBitmap GetIcon( PLM_ICON_STATE aState, int aSize = 16 ) const;

    /**
     * Get icon by identifier.
     *
     * @param aIconId The icon identifier.
     * @param aSize Requested icon size in pixels.
     * @return Bitmap for the icon, or default if not found.
     */
    wxBitmap GetIconById( const wxString& aIconId, int aSize = 16 ) const;

    /**
     * Get bitmap bundle for a PLM state.
     *
     * Returns a bundle containing multiple resolutions for proper scaling.
     *
     * @param aState The state to get bundle for.
     * @return Bitmap bundle for the state.
     */
    wxBitmapBundle GetIconBundle( PLM_ICON_STATE aState ) const;

    /**
     * Get the icon state from file info.
     *
     * Determines the appropriate icon state based on file information
     * from the PLM system.
     *
     * @param aStateCategory File state category.
     * @param aIsLocked Whether file is locked.
     * @param aLockOwner Lock owner (empty if not locked or locked by current user).
     * @param aHasLocalChanges Whether there are uncommitted local changes.
     * @param aHasConflict Whether there's a conflict with server version.
     * @return The appropriate icon state.
     */
    static PLM_ICON_STATE GetStateFromFileInfo( PLM_STATE_CATEGORY aStateCategory,
                                                 bool aIsLocked,
                                                 const wxString& aLockOwner,
                                                 bool aHasLocalChanges,
                                                 bool aHasConflict );

    /**
     * Register a custom icon from a plugin.
     *
     * @param aIconId Unique identifier for the icon.
     * @param aDescription Human-readable description.
     * @param aLoader Callback to load the icon bitmap.
     * @return True if registration succeeded.
     */
    bool RegisterPluginIcon( const wxString& aIconId, const wxString& aDescription,
                             PLM_ICON_LOADER aLoader );

    /**
     * Unregister a plugin icon.
     *
     * @param aIconId The icon identifier to remove.
     */
    void UnregisterPluginIcon( const wxString& aIconId );

    /**
     * Get information about registered icons.
     *
     * @return Map of icon IDs to icon information.
     */
    std::map<wxString, PLM_ICON_INFO> GetRegisteredIcons() const;

    /**
     * Get display text for an icon state.
     *
     * @param aState The state to get text for.
     * @return Human-readable state name.
     */
    static wxString GetStateText( PLM_ICON_STATE aState );

    /**
     * Get tooltip text for an icon state.
     *
     * @param aState The state to get tooltip for.
     * @return Descriptive tooltip text.
     */
    static wxString GetStateTooltip( PLM_ICON_STATE aState );

    /**
     * Check if a state indicates the file needs attention.
     *
     * @param aState The state to check.
     * @return True if user action may be needed.
     */
    static bool StateNeedsAttention( PLM_ICON_STATE aState );

    /**
     * Clear cached icons to force reload.
     *
     * Call this when icon theme changes or plugin icons are updated.
     */
    void ClearCache();

private:
    PLM_ICON_PROVIDER();
    ~PLM_ICON_PROVIDER();

    PLM_ICON_PROVIDER( const PLM_ICON_PROVIDER& ) = delete;
    PLM_ICON_PROVIDER& operator=( const PLM_ICON_PROVIDER& ) = delete;

    /**
     * Initialize core icons.
     */
    void initCoreIcons();

    /**
     * Create a placeholder icon for states without custom icons.
     *
     * @param aState The state to create placeholder for.
     * @param aSize Icon size.
     * @return Generated placeholder bitmap.
     */
    wxBitmap createPlaceholderIcon( PLM_ICON_STATE aState, int aSize ) const;

    /**
     * Get color for a state (used in placeholder icons).
     *
     * @param aState The state.
     * @return Color associated with the state.
     */
    wxColour getStateColor( PLM_ICON_STATE aState ) const;

    std::map<PLM_ICON_STATE, PLM_ICON_INFO>  m_coreIcons;      ///< Core icon definitions
    std::map<wxString, PLM_ICON_INFO>        m_pluginIcons;    ///< Plugin-registered icons
    std::map<wxString, PLM_ICON_LOADER>      m_pluginLoaders;  ///< Plugin icon loaders
    mutable std::map<std::pair<PLM_ICON_STATE, int>, wxBitmap> m_iconCache; ///< Cached bitmaps
};


#endif // PLM_ICON_PROVIDER_H
