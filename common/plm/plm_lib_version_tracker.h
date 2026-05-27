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
 * @file plm_lib_version_tracker.h
 * @brief PLM library version tracking for symbol/footprint libraries.
 */

#ifndef PLM_LIB_VERSION_TRACKER_H
#define PLM_LIB_VERSION_TRACKER_H

#include <kicommon.h>

#include <wx/string.h>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>


/**
 * Information about a tracked library version.
 */
struct PLM_LIB_VERSION_INFO
{
    wxString lib_id;             ///< Library identifier in PLM
    wxString lib_name;           ///< Display name
    wxString revision;           ///< PLM revision identifier
    wxString revision_display;   ///< Human-readable revision
    wxString local_path;         ///< Path to local cached copy
    int64_t  checkout_timestamp; ///< When this version was checked out
    int64_t  server_timestamp;   ///< Server modification time
    bool     pinned = false;     ///< True if version is pinned

    PLM_LIB_VERSION_INFO() : checkout_timestamp( 0 ), server_timestamp( 0 ) {}

    bool IsValid() const { return !lib_id.IsEmpty() && !revision.IsEmpty(); }
};


/**
 * Result of comparing library versions.
 */
struct PLM_LIB_VERSION_DIFF
{
    wxString lib_id;                 ///< Library identifier
    wxString local_revision;         ///< Current local revision
    wxString server_revision;        ///< Latest server revision
    bool     update_available;       ///< True if server has newer version
    bool     is_pinned;              ///< True if pinned (won't auto-update)
    wxString change_summary;         ///< Summary of changes

    PLM_LIB_VERSION_DIFF() : update_available( false ), is_pinned( false ) {}
};


/**
 * Callback for version check completion.
 *
 * @param aLibId Library that was checked.
 * @param aDiff Version difference result.
 */
using PLM_VERSION_CHECK_CALLBACK = std::function<void( const wxString& aLibId,
                                                        const PLM_LIB_VERSION_DIFF& aDiff )>;


/**
 * Callback for checking server version.
 *
 * Called by the version tracker to query the PLM server for current version.
 *
 * @param aLibId Library to check.
 * @return Latest server revision, or empty string on error.
 */
using PLM_SERVER_VERSION_CALLBACK = std::function<wxString( const wxString& aLibId )>;


/**
 * PLM library version tracker.
 *
 * Tracks versions of PLM-managed symbol and footprint libraries used by a project.
 * Stores version information at checkout and provides version comparison to detect
 * when updates are available from the server.
 *
 * Usage:
 * @code
 * PLM_LIB_VERSION_TRACKER tracker;
 * tracker.SetServerVersionCallback( []( const wxString& libId ) {
 *     return plugin->GetLibraryRevision( libId );
 * });
 *
 * // Record library version at checkout
 * PLM_LIB_VERSION_INFO info;
 * info.lib_id = "symbols/standard";
 * info.revision = "rev-123";
 * tracker.TrackLibrary( info );
 *
 * // Check for updates
 * auto diffs = tracker.CheckAllForUpdates();
 * for( const auto& diff : diffs )
 * {
 *     if( diff.update_available )
 *         wxLogMessage( "Update available for %s", diff.lib_id );
 * }
 * @endcode
 */
class KICOMMON_API PLM_LIB_VERSION_TRACKER
{
public:
    PLM_LIB_VERSION_TRACKER();
    ~PLM_LIB_VERSION_TRACKER();

    /**
     * Set callback for querying server versions.
     *
     * @param aCallback Callback function.
     */
    void SetServerVersionCallback( PLM_SERVER_VERSION_CALLBACK aCallback );

    /**
     * Track a library version.
     *
     * @param aInfo Library version information.
     */
    void TrackLibrary( const PLM_LIB_VERSION_INFO& aInfo );

    /**
     * Remove a library from tracking.
     *
     * @param aLibId Library identifier.
     */
    void UntrackLibrary( const wxString& aLibId );

    /**
     * Check if a library is being tracked.
     *
     * @param aLibId Library identifier.
     * @return True if tracked.
     */
    bool IsTracked( const wxString& aLibId ) const;

    /**
     * Get version info for a tracked library.
     *
     * @param aLibId Library identifier.
     * @return Version info, or nullopt if not tracked.
     */
    std::optional<PLM_LIB_VERSION_INFO> GetVersionInfo( const wxString& aLibId ) const;

    /**
     * Get all tracked library version info.
     *
     * @return Vector of all tracked library info.
     */
    std::vector<PLM_LIB_VERSION_INFO> GetAllTracked() const;

    /**
     * Get count of tracked libraries.
     *
     * @return Number of tracked libraries.
     */
    size_t GetTrackedCount() const;

    /**
     * Pin a library to its current version.
     *
     * Pinned libraries won't be flagged for auto-update.
     *
     * @param aLibId Library identifier.
     * @param aPinned True to pin, false to unpin.
     */
    void SetPinned( const wxString& aLibId, bool aPinned );

    /**
     * Check if a library is pinned.
     *
     * @param aLibId Library identifier.
     * @return True if pinned.
     */
    bool IsPinned( const wxString& aLibId ) const;

    /**
     * Update the recorded revision for a library.
     *
     * Called after downloading an updated version.
     *
     * @param aLibId Library identifier.
     * @param aNewRevision New revision identifier.
     * @param aNewTimestamp New server timestamp.
     */
    void UpdateRevision( const wxString& aLibId, const wxString& aNewRevision,
                         int64_t aNewTimestamp = 0 );

    /**
     * Check a single library for updates.
     *
     * @param aLibId Library identifier.
     * @return Version diff result.
     */
    PLM_LIB_VERSION_DIFF CheckForUpdate( const wxString& aLibId );

    /**
     * Check all tracked libraries for updates.
     *
     * @return Vector of version diffs for all tracked libraries.
     */
    std::vector<PLM_LIB_VERSION_DIFF> CheckAllForUpdates();

    /**
     * Check for updates asynchronously.
     *
     * Results delivered via callback.
     *
     * @param aCallback Callback for each library checked.
     */
    void CheckAllForUpdatesAsync( PLM_VERSION_CHECK_CALLBACK aCallback );

    /**
     * Get libraries that have updates available.
     *
     * @param aIncludePinned Include pinned libraries in results.
     * @return Vector of library IDs with available updates.
     */
    std::vector<wxString> GetLibrariesWithUpdates( bool aIncludePinned = false );

    /**
     * Clear all tracked libraries.
     */
    void Clear();

    /**
     * Save tracked versions to JSON.
     *
     * @return JSON string representation.
     */
    wxString ToJson() const;

    /**
     * Load tracked versions from JSON.
     *
     * @param aJson JSON string.
     * @return True on success.
     */
    bool FromJson( const wxString& aJson );

    /**
     * Save tracked versions to file.
     *
     * @param aPath File path.
     * @return True on success.
     */
    bool SaveToFile( const wxString& aPath ) const;

    /**
     * Load tracked versions from file.
     *
     * @param aPath File path.
     * @return True on success.
     */
    bool LoadFromFile( const wxString& aPath );

    /**
     * Get the default storage path for a project.
     *
     * @param aProjectPath Path to project file.
     * @return Path for library version tracking file.
     */
    static wxString GetDefaultPath( const wxString& aProjectPath );

private:
    std::map<wxString, PLM_LIB_VERSION_INFO> m_tracked;
    PLM_SERVER_VERSION_CALLBACK              m_serverCallback;
};


#endif // PLM_LIB_VERSION_TRACKER_H
