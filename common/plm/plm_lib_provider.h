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
 * @file plm_lib_provider.h
 * @brief PLM virtual library provider for symbols and footprints.
 */

#ifndef PLM_LIB_PROVIDER_H
#define PLM_LIB_PROVIDER_H

#include <kicommon.h>
#include "plm_lib_reference.h"
#include "plm_part_cache.h"

#include <wx/string.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

class PLM_LIB_VERSION_TRACKER;


/**
 * Result of a library query from PLM.
 */
struct PLM_LIB_QUERY_RESULT
{
    bool     success;           ///< True if query succeeded
    bool     from_cache;        ///< True if result came from cache
    wxString lib_path;          ///< Local path to library file
    wxString revision;          ///< Library revision
    wxString error_message;     ///< Error message if failed
    int64_t  timestamp;         ///< Timestamp of result

    PLM_LIB_QUERY_RESULT() : success( false ), from_cache( false ), timestamp( 0 ) {}

    static PLM_LIB_QUERY_RESULT Success( const wxString& aPath, const wxString& aRevision,
                                          bool aFromCache = false )
    {
        PLM_LIB_QUERY_RESULT result;
        result.success = true;
        result.lib_path = aPath;
        result.revision = aRevision;
        result.from_cache = aFromCache;
        return result;
    }

    static PLM_LIB_QUERY_RESULT Failure( const wxString& aError )
    {
        PLM_LIB_QUERY_RESULT result;
        result.success = false;
        result.error_message = aError;
        return result;
    }
};


/**
 * Result of a library download.
 *
 * Carries the resolved revision back to the provider so the cache entry
 * records the actual SHA/tag the plugin landed, not whatever the caller
 * requested. PLM_LIB_VERSION_TRACKER::UpdateRevision then has a meaningful
 * value to store, which is what makes pinning detection work end-to-end.
 */
struct PLM_LIB_DOWNLOAD_RESULT
{
    bool     success = false;
    wxString revision;          ///< Revision actually fetched (commit SHA, tag, etc.)
    wxString error_message;
};


/**
 * Callback for downloading library from PLM.
 *
 * Called by provider to fetch library data from PLM server.
 *
 * @param aLibId Library identifier in PLM.
 * @param aLibType Library type — plugins need this to extract the right
 *                 artifact from a fetched repository (.kicad_sym vs
 *                 .pretty/ vs .step).
 * @param aRequestedRevision Empty for "latest"; otherwise the revision the
 *                           provider determined the caller wants (e.g. a
 *                           pinned version from the version tracker).
 * @param aLocalPath Path to store downloaded library.
 * @return Result carrying success state and the revision actually written.
 */
using PLM_LIB_DOWNLOAD_CALLBACK =
        std::function<PLM_LIB_DOWNLOAD_RESULT( const wxString& aLibId,
                                                PLM_LIB_TYPE aLibType,
                                                const wxString& aRequestedRevision,
                                                const wxString& aLocalPath )>;


/**
 * Callback for validating library revision.
 *
 * Called to check if cached library is still current.
 *
 * @param aLibId Library identifier.
 * @param aCachedRevision Currently cached revision.
 * @return True if cache is valid, false if update needed.
 */
using PLM_LIB_VALIDATE_CALLBACK = std::function<bool( const wxString& aLibId,
                                                       const wxString& aCachedRevision )>;


/**
 * PLM virtual library provider.
 *
 * Provides symbol and footprint libraries from PLM servers as if they
 * were local libraries. Integrates with KiCad's library system to make
 * PLM-managed libraries available for use in designs.
 *
 * The provider queries PLM for libraries, caches them locally, and
 * validates cached versions against the server on access.
 *
 * Usage:
 * @code
 * PLM_LIB_PROVIDER provider;
 * provider.SetVersionTracker( versionTracker );
 * provider.SetDownloadCallback(
 *         []( const wxString& libId, PLM_LIB_TYPE type,
 *             const wxString& requestedRevision, const wxString& path )
 *         {
 *             PLM_LIB_DOWNLOAD_RESULT r;
 *             r.success = plugin->DownloadLibrary( libId, type, requestedRevision, path );
 *             r.revision = requestedRevision;
 *             return r;
 *         });
 *
 * // Get a symbol library
 * auto result = provider.GetSymbolLibrary( "corporate/symbols/standard" );
 * if( result.success )
 *     LoadSymbolLib( result.lib_path );
 * @endcode
 */
class KICOMMON_API PLM_LIB_PROVIDER
{
public:
    PLM_LIB_PROVIDER();
    ~PLM_LIB_PROVIDER();

    /**
     * Set cache manager for library storage.
     *
     * @param aCache Pointer to cache manager.
     */
    void SetCacheManager( PLM_PART_CACHE* aCache );

    /**
     * Set callback for downloading libraries.
     *
     * @param aCallback Download callback.
     */
    void SetDownloadCallback( PLM_LIB_DOWNLOAD_CALLBACK aCallback );

    /**
     * Set callback for validating cached libraries.
     *
     * @param aCallback Validation callback.
     */
    void SetValidateCallback( PLM_LIB_VALIDATE_CALLBACK aCallback );

    /**
     * Bind a version tracker for pinned-revision lookup.
     *
     * When set, getLibrary() consults the tracker for a pinned revision on
     * each fetch and threads it into the download callback as
     * aRequestedRevision. Without a tracker, requested revision is always
     * empty and the callback resolves "latest".
     *
     * @param aTracker Pointer to tracker (not owned). Pass nullptr to clear.
     */
    void SetVersionTracker( PLM_LIB_VERSION_TRACKER* aTracker );

    /**
     * Get a symbol library from PLM.
     *
     * @param aLibId PLM library identifier.
     * @param aForceRefresh Force download even if cached.
     * @return Query result with local path or error.
     */
    PLM_LIB_QUERY_RESULT GetSymbolLibrary( const wxString& aLibId, bool aForceRefresh = false );

    /**
     * Get a footprint library from PLM.
     *
     * @param aLibId PLM library identifier.
     * @param aForceRefresh Force download even if cached.
     * @return Query result with local path or error.
     */
    PLM_LIB_QUERY_RESULT GetFootprintLibrary( const wxString& aLibId, bool aForceRefresh = false );

    /**
     * Get a 3D model from PLM.
     *
     * @param aModelId PLM model identifier.
     * @param aForceRefresh Force download even if cached.
     * @return Query result with local path or error.
     */
    PLM_LIB_QUERY_RESULT Get3DModel( const wxString& aModelId, bool aForceRefresh = false );

    /**
     * Check if a library is available in cache.
     *
     * @param aLibId Library identifier.
     * @param aLibType Library type.
     * @return True if cached locally.
     */
    bool IsCached( const wxString& aLibId, PLM_LIB_TYPE aLibType ) const;

    /**
     * Validate a cached library against server.
     *
     * @param aLibId Library identifier.
     * @param aLibType Library type.
     * @return True if cache is current.
     */
    bool ValidateCache( const wxString& aLibId, PLM_LIB_TYPE aLibType );

    /**
     * Invalidate cached library.
     *
     * @param aLibId Library identifier.
     * @param aLibType Library type.
     */
    void InvalidateCache( const wxString& aLibId, PLM_LIB_TYPE aLibType );

    /**
     * Get cached library path.
     *
     * @param aLibId Library identifier.
     * @param aLibType Library type.
     * @return Local path, or empty if not cached.
     */
    wxString GetCachedPath( const wxString& aLibId, PLM_LIB_TYPE aLibType ) const;

    /**
     * Get cached library revision.
     *
     * @param aLibId Library identifier.
     * @param aLibType Library type.
     * @return Revision string, or empty if not cached.
     */
    wxString GetCachedRevision( const wxString& aLibId, PLM_LIB_TYPE aLibType ) const;

    /**
     * Pre-fetch libraries for offline use.
     *
     * Downloads specified libraries to cache.
     *
     * @param aReferences Library references to pre-fetch.
     * @return Number of libraries successfully cached.
     */
    size_t PrefetchLibraries( const std::vector<PLM_LIB_REFERENCE>& aReferences );

    /**
     * Clear all cached libraries.
     */
    void ClearCache();

    /**
     * Get count of cached libraries.
     *
     * @return Number of cached libraries.
     */
    size_t GetCacheCount() const;

    /**
     * Check if provider is configured.
     *
     * @return True if download callback is set.
     */
    bool IsConfigured() const { return m_downloadCallback != nullptr; }

    /**
     * Get file extension for library type.
     *
     * @param aLibType Library type.
     * @return File extension including dot.
     */
    static wxString GetLibraryExtension( PLM_LIB_TYPE aLibType );

    /**
     * Get cache subdirectory for library type.
     *
     * @param aLibType Library type.
     * @return Subdirectory name.
     */
    static wxString GetCacheSubdir( PLM_LIB_TYPE aLibType );

private:
    PLM_LIB_QUERY_RESULT getLibrary( const wxString& aLibId, PLM_LIB_TYPE aLibType,
                                      bool aForceRefresh );
    wxString             getCachePath( const wxString& aLibId, PLM_LIB_TYPE aLibType ) const;
    wxString             generateCacheFilename( const wxString& aLibId,
                                                 PLM_LIB_TYPE aLibType ) const;

    struct CacheEntry
    {
        wxString local_path;
        wxString revision;
        int64_t  timestamp;
        bool     validated;
    };

    PLM_PART_CACHE*                          m_cache;
    PLM_LIB_VERSION_TRACKER*                 m_versionTracker;
    PLM_LIB_DOWNLOAD_CALLBACK                m_downloadCallback;
    PLM_LIB_VALIDATE_CALLBACK                m_validateCallback;
    std::map<wxString, CacheEntry>           m_cacheIndex;
    wxString                                 m_cacheDir;
};


#endif // PLM_LIB_PROVIDER_H
