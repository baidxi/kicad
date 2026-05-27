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
 * @file plm_api_version.h
 * @brief PLM Plugin API version constants and utilities.
 *
 * This header provides version information for the PLM plugin API.
 * Plugin developers should include this header to access version
 * constants and compatibility checking functions.
 *
 * The version follows semantic versioning:
 * - Major version: Incremented for breaking API changes
 * - Minor version: Incremented for backward-compatible additions
 * - Patch version: Incremented for backward-compatible bug fixes
 *
 * Compatibility rules:
 * - Plugins must match the host's major version exactly
 * - Plugin minor version may be <= host minor version
 * - Patch version differences are always compatible
 */

#ifndef PLM_API_VERSION_H
#define PLM_API_VERSION_H

#include <wx/string.h>


/**
 * @name PLM API Version Constants
 * @{
 */

/// Major version - breaking API changes
/// Bumped to 3 in WF-LIBS-002 Slice 1: PLM_PLUGIN gained three more
/// protected manager unique_ptrs (m_libProvider, m_libSync,
/// m_libVersionTracker) and PLM_LIB_DOWNLOAD_CALLBACK gained PLM_LIB_TYPE
/// plus a result struct. Both are layout/ABI breaks.
constexpr int PLM_API_VERSION_MAJOR = 3;

/// Minor version - backward-compatible additions
constexpr int PLM_API_VERSION_MINOR = 0;

/// Patch version - backward-compatible bug fixes
constexpr int PLM_API_VERSION_PATCH = 0;

/// Combined version number: (major * 1000000) + (minor * 1000) + patch
constexpr int PLM_API_VERSION_FULL =
        ( PLM_API_VERSION_MAJOR * 1000000 ) + ( PLM_API_VERSION_MINOR * 1000 ) + PLM_API_VERSION_PATCH;

/// Legacy combined version for backward compatibility: (major * 1000) + minor
constexpr int PLM_API_VERSION = ( PLM_API_VERSION_MAJOR * 1000 ) + PLM_API_VERSION_MINOR;

/** @} */


/**
 * @name Version String Functions
 * @{
 */

/**
 * Get the API version as a string.
 *
 * @return Version string in "major.minor.patch" format.
 */
inline wxString PLM_GetAPIVersionString()
{
    return wxString::Format( wxS( "%d.%d.%d" ),
                             PLM_API_VERSION_MAJOR,
                             PLM_API_VERSION_MINOR,
                             PLM_API_VERSION_PATCH );
}

/**
 * Get the API version as a short string.
 *
 * @return Version string in "major.minor" format.
 */
inline wxString PLM_GetAPIVersionShortString()
{
    return wxString::Format( wxS( "%d.%d" ),
                             PLM_API_VERSION_MAJOR,
                             PLM_API_VERSION_MINOR );
}

/** @} */


/**
 * @name Version Parsing Functions
 * @{
 */

/**
 * Parse a combined version number into components.
 *
 * Handles both legacy format (major * 1000 + minor) and
 * full format (major * 1000000 + minor * 1000 + patch).
 *
 * @param aVersion Combined version number.
 * @param aMajor Output major version.
 * @param aMinor Output minor version.
 * @param aPatch Output patch version (0 for legacy format).
 */
inline void PLM_ParseVersion( int aVersion, int& aMajor, int& aMinor, int& aPatch )
{
    if( aVersion < 0 )
    {
        aMajor = 0;
        aMinor = 0;
        aPatch = 0;
        return;
    }

    // Detect format: full format has values >= 1000000
    if( aVersion >= 1000000 )
    {
        aMajor = aVersion / 1000000;
        aMinor = ( aVersion / 1000 ) % 1000;
        aPatch = aVersion % 1000;
    }
    else
    {
        // Legacy format
        aMajor = aVersion / 1000;
        aMinor = aVersion % 1000;
        aPatch = 0;
    }
}

/**
 * Format a version number as a string.
 *
 * @param aVersion Combined version number.
 * @return Version string.
 */
inline wxString PLM_FormatVersion( int aVersion )
{
    int major, minor, patch;
    PLM_ParseVersion( aVersion, major, minor, patch );

    if( patch > 0 )
        return wxString::Format( wxS( "%d.%d.%d" ), major, minor, patch );

    return wxString::Format( wxS( "%d.%d" ), major, minor );
}

/** @} */


/**
 * @name Version Compatibility Functions
 * @{
 */

/**
 * Check if a plugin version is compatible with the host.
 *
 * @param aPluginVersion Plugin's combined version number.
 * @return True if the plugin is compatible.
 */
inline bool PLM_IsVersionCompatible( int aPluginVersion )
{
    int pluginMajor, pluginMinor, pluginPatch;
    PLM_ParseVersion( aPluginVersion, pluginMajor, pluginMinor, pluginPatch );

    // Major version must match exactly
    return pluginMajor == PLM_API_VERSION_MAJOR;
}

/**
 * Check if a plugin version is compatible with the host.
 *
 * @param aPluginVersion Plugin's combined version number.
 * @param aErrorMsg Output error message if incompatible.
 * @return True if the plugin is compatible.
 */
inline bool PLM_IsVersionCompatible( int aPluginVersion, wxString& aErrorMsg )
{
    int pluginMajor, pluginMinor, pluginPatch;
    PLM_ParseVersion( aPluginVersion, pluginMajor, pluginMinor, pluginPatch );

    aErrorMsg.Clear();

    if( pluginMajor != PLM_API_VERSION_MAJOR )
    {
        aErrorMsg = wxString::Format(
                wxS( "Incompatible API version %d.%d (host requires %d.x)" ),
                pluginMajor, pluginMinor, PLM_API_VERSION_MAJOR );
        return false;
    }

    return true;
}

/**
 * Check if a plugin version is newer than the host.
 *
 * Useful for warning about potential forward compatibility issues.
 *
 * @param aPluginVersion Plugin's combined version number.
 * @return True if plugin is newer than host.
 */
inline bool PLM_IsVersionNewer( int aPluginVersion )
{
    int pluginMajor, pluginMinor, pluginPatch;
    PLM_ParseVersion( aPluginVersion, pluginMajor, pluginMinor, pluginPatch );

    if( pluginMajor != PLM_API_VERSION_MAJOR )
        return pluginMajor > PLM_API_VERSION_MAJOR;

    if( pluginMinor != PLM_API_VERSION_MINOR )
        return pluginMinor > PLM_API_VERSION_MINOR;

    return pluginPatch > PLM_API_VERSION_PATCH;
}

/** @} */


#endif // PLM_API_VERSION_H
