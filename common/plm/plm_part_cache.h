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
 * @file plm_part_cache.h
 * @brief Part library caching for the PLM client interface.
 *
 * This header defines the PLM_PART_CACHE class which handles caching of
 * downloaded symbol and footprint libraries from PLM servers. It tracks
 * library versions and validates cache entries against the PLM server.
 */

#ifndef PLM_PART_CACHE_H
#define PLM_PART_CACHE_H

#include "plm_parts.h"

#include <wx/string.h>
#include <cstdint>
#include <map>
#include <vector>


/**
 * Type of library being cached.
 */
enum class PLM_LIBRARY_TYPE
{
    SYMBOL,     ///< Schematic symbol library
    FOOTPRINT   ///< PCB footprint library
};


/**
 * Information about a cached library.
 */
struct PLM_CACHED_LIBRARY
{
    wxString         library_id;        ///< Unique library identifier from PLM
    wxString         library_name;      ///< Human-readable library name
    PLM_LIBRARY_TYPE type = PLM_LIBRARY_TYPE::SYMBOL; ///< Library type
    wxString         version;           ///< Version string from PLM
    wxString         checksum;          ///< Content checksum for validation
    wxString         local_path;        ///< Path to cached library file
    int64_t          cached_at = 0;     ///< When library was cached
    int64_t          last_validated = 0; ///< Last validation timestamp
    int64_t          last_accessed = 0; ///< Last access timestamp for LRU
    int64_t          size_bytes = 0;    ///< Size of cached library
    bool             is_valid = false;  ///< Whether cache entry is valid

    PLM_CACHED_LIBRARY() = default;

    PLM_CACHED_LIBRARY( const wxString& aId, const wxString& aName, PLM_LIBRARY_TYPE aType ) :
            library_id( aId ),
            library_name( aName ),
            type( aType )
    {
    }

    /**
     * Check if library needs revalidation.
     * @param aMaxAge Maximum age in seconds before revalidation.
     * @return True if library should be validated with server.
     */
    bool NeedsValidation( int64_t aMaxAge ) const;

    /**
     * Get the file extension for this library type.
     * @return File extension without dot.
     */
    wxString GetFileExtension() const
    {
        return ( type == PLM_LIBRARY_TYPE::SYMBOL ) ? wxS( "kicad_sym" ) : wxS( "kicad_mod" );
    }

    /**
     * Check if library file exists on disk.
     * @return True if local_path exists.
     */
    bool FileExists() const;
};


/**
 * Result of a library validation check.
 */
struct PLM_LIBRARY_VALIDATION
{
    wxString library_id;            ///< Library that was validated
    bool     is_valid = false;      ///< Whether cache matches server
    bool     needs_update = false;  ///< Whether library needs re-download
    wxString server_version;        ///< Current version on server
    wxString server_checksum;       ///< Current checksum on server
    wxString message;               ///< Human-readable validation message

    PLM_LIBRARY_VALIDATION() = default;

    /**
     * Create a valid result.
     */
    static PLM_LIBRARY_VALIDATION Valid( const wxString& aLibraryId )
    {
        PLM_LIBRARY_VALIDATION result;
        result.library_id = aLibraryId;
        result.is_valid = true;
        result.message = wxS( "Cache is up to date" );
        return result;
    }

    /**
     * Create an invalid result requiring update.
     */
    static PLM_LIBRARY_VALIDATION NeedsUpdate( const wxString& aLibraryId,
                                                const wxString& aServerVersion,
                                                const wxString& aServerChecksum )
    {
        PLM_LIBRARY_VALIDATION result;
        result.library_id = aLibraryId;
        result.is_valid = false;
        result.needs_update = true;
        result.server_version = aServerVersion;
        result.server_checksum = aServerChecksum;
        result.message = wxS( "Server has newer version" );
        return result;
    }

    /**
     * Create a not found result.
     */
    static PLM_LIBRARY_VALIDATION NotCached( const wxString& aLibraryId )
    {
        PLM_LIBRARY_VALIDATION result;
        result.library_id = aLibraryId;
        result.is_valid = false;
        result.needs_update = true;
        result.message = wxS( "Library not in cache" );
        return result;
    }
};


class PLM_CACHE_MANAGER;


/**
 * Cache statistics for monitoring and diagnostics.
 */
struct PLM_CACHE_STATISTICS
{
    size_t  total_size_bytes = 0;     ///< Total size of cached libraries
    size_t  library_count = 0;        ///< Number of libraries in cache
    size_t  symbol_count = 0;         ///< Number of symbol libraries
    size_t  footprint_count = 0;      ///< Number of footprint libraries
    size_t  valid_count = 0;          ///< Number of valid cache entries
    size_t  invalid_count = 0;        ///< Number of invalid cache entries
    size_t  max_size_bytes = 0;       ///< Configured maximum cache size
    int64_t oldest_entry = 0;         ///< Timestamp of oldest entry
    int64_t newest_entry = 0;         ///< Timestamp of newest entry
    size_t  hit_count = 0;            ///< Number of cache hits
    size_t  miss_count = 0;           ///< Number of cache misses
    size_t  eviction_count = 0;       ///< Number of evicted entries

    double GetHitRate() const
    {
        size_t total = hit_count + miss_count;
        return total > 0 ? static_cast<double>( hit_count ) / total : 0.0;
    }

    double GetUtilization() const
    {
        return max_size_bytes > 0 ? static_cast<double>( total_size_bytes ) / max_size_bytes : 0.0;
    }
};


/**
 * Manages caching of part libraries from PLM servers.
 *
 * The part cache stores downloaded symbol and footprint libraries locally,
 * tracking versions and checksums to efficiently validate against the server.
 * Libraries are stored as standard KiCad library files.
 *
 * Usage:
 * @code
 * PLM_PART_CACHE cache( &cacheManager );
 * cache.Initialize( "https://plm.example.com", "project-001" );
 *
 * // Check if library is cached
 * PLM_LIBRARY_VALIDATION validation = cache.ValidateLibrary( "resistors" );
 * if( validation.needs_update )
 * {
 *     // Download and cache library
 *     cache.CacheLibrary( "resistors", downloadedData, version, checksum );
 * }
 *
 * // Get path to cached library
 * wxString path = cache.GetLibraryPath( "resistors", PLM_LIBRARY_TYPE::SYMBOL );
 * @endcode
 */
class PLM_PART_CACHE
{
public:
    /**
     * Construct a part cache.
     * @param aCacheManager Cache manager for directory operations.
     */
    explicit PLM_PART_CACHE( PLM_CACHE_MANAGER* aCacheManager );

    /**
     * Destructor.
     */
    ~PLM_PART_CACHE();

    // Non-copyable
    PLM_PART_CACHE( const PLM_PART_CACHE& ) = delete;
    PLM_PART_CACHE& operator=( const PLM_PART_CACHE& ) = delete;

    //-------------------------------------------------------------------------
    // Initialization
    //-------------------------------------------------------------------------

    /**
     * Initialize the part cache for a server and project.
     *
     * @param aServerUrl PLM server URL.
     * @param aProjectId Project identifier.
     * @return True if initialization succeeded.
     */
    bool Initialize( const wxString& aServerUrl, const wxString& aProjectId );

    /**
     * Check if the cache is initialized.
     * @return True if Initialize() was called successfully.
     */
    bool IsInitialized() const { return m_initialized; }

    //-------------------------------------------------------------------------
    // Cache Size Management
    //-------------------------------------------------------------------------

    /**
     * Set maximum cache size.
     *
     * When the cache exceeds this size, oldest entries are evicted using LRU.
     * Set to 0 to disable size limit.
     *
     * @param aMaxBytes Maximum cache size in bytes.
     */
    void SetMaxCacheSize( size_t aMaxBytes );

    /**
     * Get the configured maximum cache size.
     * @return Maximum size in bytes, or 0 if unlimited.
     */
    size_t GetMaxCacheSize() const { return m_maxCacheSize; }

    /**
     * Manually trigger cache eviction to reach target size.
     *
     * Evicts oldest entries until cache is at or below target size.
     *
     * @param aTargetBytes Target size in bytes.
     * @return Number of libraries evicted.
     */
    size_t EvictToSize( size_t aTargetBytes );

    /**
     * Clear all cached libraries.
     *
     * Removes all cached files and resets statistics.
     *
     * @return Number of libraries cleared.
     */
    size_t ClearCache();

    //-------------------------------------------------------------------------
    // Library Caching
    //-------------------------------------------------------------------------

    /**
     * Cache a library from downloaded data.
     *
     * @param aLibraryId Unique library identifier.
     * @param aLibraryName Human-readable library name.
     * @param aType Library type (symbol or footprint).
     * @param aData Library file content.
     * @param aVersion Version string from PLM.
     * @param aChecksum Content checksum.
     * @return True if caching succeeded.
     */
    bool CacheLibrary( const wxString& aLibraryId, const wxString& aLibraryName,
                       PLM_LIBRARY_TYPE aType, const wxString& aData,
                       const wxString& aVersion, const wxString& aChecksum );

    /**
     * Get the local path for a cached library.
     *
     * @param aLibraryId Library identifier.
     * @param aType Library type.
     * @return Path to cached library, or empty if not cached.
     */
    wxString GetLibraryPath( const wxString& aLibraryId, PLM_LIBRARY_TYPE aType ) const;

    /**
     * Check if a library is cached.
     *
     * @param aLibraryId Library identifier.
     * @param aType Library type.
     * @return True if library exists in cache.
     */
    bool HasLibrary( const wxString& aLibraryId, PLM_LIBRARY_TYPE aType ) const;

    /**
     * Get information about a cached library.
     *
     * @param aLibraryId Library identifier.
     * @param aType Library type.
     * @param aLibrary Output library information.
     * @return True if library was found.
     */
    bool GetLibraryInfo( const wxString& aLibraryId, PLM_LIBRARY_TYPE aType,
                         PLM_CACHED_LIBRARY& aLibrary ) const;

    /**
     * Get all cached libraries.
     * @return Vector of cached library information.
     */
    std::vector<PLM_CACHED_LIBRARY> GetCachedLibraries() const;

    /**
     * Get cached libraries by type.
     * @param aType Library type to filter.
     * @return Vector of cached libraries of the specified type.
     */
    std::vector<PLM_CACHED_LIBRARY> GetLibrariesByType( PLM_LIBRARY_TYPE aType ) const;

    //-------------------------------------------------------------------------
    // Version Tracking
    //-------------------------------------------------------------------------

    /**
     * Get the cached version of a library.
     *
     * @param aLibraryId Library identifier.
     * @param aType Library type.
     * @return Version string, or empty if not cached.
     */
    wxString GetLibraryVersion( const wxString& aLibraryId, PLM_LIBRARY_TYPE aType ) const;

    /**
     * Get the cached checksum of a library.
     *
     * @param aLibraryId Library identifier.
     * @param aType Library type.
     * @return Checksum string, or empty if not cached.
     */
    wxString GetLibraryChecksum( const wxString& aLibraryId, PLM_LIBRARY_TYPE aType ) const;

    /**
     * Update the version information for a cached library.
     *
     * Call this after successful validation to update timestamps.
     *
     * @param aLibraryId Library identifier.
     * @param aType Library type.
     * @param aVersion New version string.
     * @param aChecksum New checksum.
     * @return True if update succeeded.
     */
    bool UpdateLibraryVersion( const wxString& aLibraryId, PLM_LIBRARY_TYPE aType,
                               const wxString& aVersion, const wxString& aChecksum );

    //-------------------------------------------------------------------------
    // Validation
    //-------------------------------------------------------------------------

    /**
     * Validate a cached library against server information.
     *
     * Compares the cached version and checksum against server values.
     *
     * @param aLibraryId Library identifier.
     * @param aType Library type.
     * @param aServerVersion Current version on server.
     * @param aServerChecksum Current checksum on server.
     * @return Validation result.
     */
    PLM_LIBRARY_VALIDATION ValidateLibrary( const wxString& aLibraryId, PLM_LIBRARY_TYPE aType,
                                             const wxString& aServerVersion,
                                             const wxString& aServerChecksum );

    /**
     * Mark a library as validated.
     *
     * Updates the last_validated timestamp.
     *
     * @param aLibraryId Library identifier.
     * @param aType Library type.
     */
    void MarkValidated( const wxString& aLibraryId, PLM_LIBRARY_TYPE aType );

    //-------------------------------------------------------------------------
    // Invalidation
    //-------------------------------------------------------------------------

    /**
     * Invalidate a specific library cache entry.
     *
     * Removes the library from cache.
     *
     * @param aLibraryId Library identifier.
     * @param aType Library type.
     * @return True if library was found and removed.
     */
    bool InvalidateLibrary( const wxString& aLibraryId, PLM_LIBRARY_TYPE aType );

    /**
     * Invalidate all symbol libraries.
     * @return Number of libraries invalidated.
     */
    size_t InvalidateAllSymbols();

    /**
     * Invalidate all footprint libraries.
     * @return Number of libraries invalidated.
     */
    size_t InvalidateAllFootprints();

    /**
     * Invalidate all cached libraries.
     * @return Number of libraries invalidated.
     */
    size_t InvalidateAll();

    //-------------------------------------------------------------------------
    // Statistics
    //-------------------------------------------------------------------------

    /**
     * Get total size of cached libraries.
     * @return Size in bytes.
     */
    size_t GetTotalCacheSize() const;

    /**
     * Get number of cached libraries.
     * @return Total library count.
     */
    size_t GetLibraryCount() const;

    /**
     * Get number of cached symbol libraries.
     * @return Symbol library count.
     */
    size_t GetSymbolLibraryCount() const;

    /**
     * Get number of cached footprint libraries.
     * @return Footprint library count.
     */
    size_t GetFootprintLibraryCount() const;

    /**
     * Get comprehensive cache statistics.
     * @return Statistics structure with all cache metrics.
     */
    PLM_CACHE_STATISTICS GetStatistics() const;

    /**
     * Reset cache hit/miss counters.
     */
    void ResetStatistics();

private:
    /**
     * Generate a cache key for a library.
     * @param aLibraryId Library identifier.
     * @param aType Library type.
     * @return Unique cache key.
     */
    wxString MakeCacheKey( const wxString& aLibraryId, PLM_LIBRARY_TYPE aType ) const;

    /**
     * Get the cache directory for libraries.
     * @return Path to library cache directory.
     */
    wxString GetLibraryCacheDir() const;

    /**
     * Load the library index from disk.
     * @return True if index was loaded successfully.
     */
    bool LoadIndex();

    /**
     * Save the library index to disk.
     * @return True if index was saved successfully.
     */
    bool SaveIndex();

    /**
     * Enforce the cache size limit by evicting oldest entries.
     *
     * Called automatically after adding new entries.
     */
    void EnforceSizeLimit();

    /**
     * Touch a library entry to update its access time.
     *
     * Called when a library is accessed for LRU tracking.
     *
     * @param aKey Cache key to touch.
     */
    void TouchEntry( const wxString& aKey );

    /**
     * Record a cache hit.
     */
    void RecordHit() const { ++m_hitCount; }

    /**
     * Record a cache miss.
     */
    void RecordMiss() const { ++m_missCount; }

    PLM_CACHE_MANAGER*                     m_cacheManager;     ///< Cache manager reference
    wxString                               m_serverUrl;        ///< PLM server URL
    wxString                               m_projectId;        ///< Project identifier
    bool                                   m_initialized = false; ///< Initialization state
    std::map<wxString, PLM_CACHED_LIBRARY> m_libraries;        ///< Cached library index
    size_t                                 m_maxCacheSize = 0; ///< Max cache size (0=unlimited)
    mutable size_t                         m_hitCount = 0;     ///< Cache hit counter
    mutable size_t                         m_missCount = 0;    ///< Cache miss counter
    size_t                                 m_evictionCount = 0; ///< Eviction counter
};


#endif // PLM_PART_CACHE_H
