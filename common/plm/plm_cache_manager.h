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
 * @file plm_cache_manager.h
 * @brief PLM cache directory structure and management.
 *
 * This header defines the PLM_CACHE_MANAGER class which handles
 * cache directory creation, hash generation, and orphan cleanup.
 * The cache structure is: plm-cache/<server-hash>/<project-hash>/
 */

#ifndef PLM_CACHE_MANAGER_H
#define PLM_CACHE_MANAGER_H

#include "plm_cache.h"

#include <wx/string.h>
#include <map>
#include <mutex>
#include <set>
#include <vector>


/**
 * Manages PLM cache directory structure and lifecycle.
 *
 * The cache manager creates a hierarchical directory structure under
 * the KiCad cache path, organized by server and project. This allows
 * efficient cache isolation and cleanup when projects are removed.
 *
 * Directory structure:
 * @code
 * <kicad-cache>/plm-cache/
 *   ├── <server-hash>/
 *   │   ├── <project-hash>/
 *   │   │   ├── metadata/
 *   │   │   ├── files/
 *   │   │   └── previews/
 *   │   └── <project-hash>/
 *   └── <server-hash>/
 * @endcode
 *
 * Usage:
 * @code
 * PLM_CACHE_MANAGER cache;
 * cache.Initialize();
 *
 * wxString projectDir = cache.GetProjectCacheDir( "https://plm.example.com", "PRJ-001" );
 * // Use projectDir for storing cached data
 *
 * // When project is no longer active
 * cache.CleanupOrphanedCaches( activeProjects );
 * @endcode
 */
class PLM_CACHE_MANAGER
{
public:
    /**
     * Construct a cache manager.
     *
     * Does not initialize the cache directory. Call Initialize() before use.
     */
    PLM_CACHE_MANAGER();

    /**
     * Destructor.
     */
    ~PLM_CACHE_MANAGER();

    // Non-copyable
    PLM_CACHE_MANAGER( const PLM_CACHE_MANAGER& ) = delete;
    PLM_CACHE_MANAGER& operator=( const PLM_CACHE_MANAGER& ) = delete;

    //-------------------------------------------------------------------------
    // Initialization
    //-------------------------------------------------------------------------

    /**
     * Initialize the cache manager.
     *
     * Creates the root cache directory if it doesn't exist.
     * Uses configuration to set cache limits.
     *
     * @param aConfig Optional cache configuration.
     * @return True if initialization succeeded.
     */
    bool Initialize( const PLM_CACHE_CONFIG& aConfig = PLM_CACHE_CONFIG() );

    /**
     * Check if the cache manager is initialized.
     *
     * @return True if Initialize() was called successfully.
     */
    bool IsInitialized() const { return m_initialized; }

    /**
     * Get the current cache configuration.
     *
     * @return Current configuration.
     */
    const PLM_CACHE_CONFIG& GetConfig() const { return m_config; }

    /**
     * Update cache configuration.
     *
     * @param aConfig New configuration.
     */
    void SetConfig( const PLM_CACHE_CONFIG& aConfig ) { m_config = aConfig; }

    //-------------------------------------------------------------------------
    // Directory Management
    //-------------------------------------------------------------------------

    /**
     * Get the root PLM cache directory.
     *
     * @return Path to the plm-cache directory.
     */
    wxString GetCacheRootDir() const { return m_cacheRootDir; }

    /**
     * Get the cache directory for a specific server.
     *
     * Creates the directory if it doesn't exist.
     *
     * @param aServerUrl Server URL to get cache directory for.
     * @return Path to the server cache directory.
     */
    wxString GetServerCacheDir( const wxString& aServerUrl );

    /**
     * Get the cache directory for a specific project.
     *
     * Creates the directory structure if it doesn't exist.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @return Path to the project cache directory.
     */
    wxString GetProjectCacheDir( const wxString& aServerUrl, const wxString& aProjectId );

    /**
     * Get the metadata cache subdirectory for a project.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @return Path to the metadata cache directory.
     */
    wxString GetMetadataCacheDir( const wxString& aServerUrl, const wxString& aProjectId );

    /**
     * Get the file cache subdirectory for a project.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @return Path to the file cache directory.
     */
    wxString GetFileCacheDir( const wxString& aServerUrl, const wxString& aProjectId );

    /**
     * Get the preview cache subdirectory for a project.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @return Path to the preview cache directory.
     */
    wxString GetPreviewCacheDir( const wxString& aServerUrl, const wxString& aProjectId );

    /**
     * Check if a cache directory exists for a project.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @return True if the cache directory exists.
     */
    bool HasProjectCache( const wxString& aServerUrl, const wxString& aProjectId ) const;

    //-------------------------------------------------------------------------
    // Hash Generation
    //-------------------------------------------------------------------------

    /**
     * Generate a deterministic hash from a server URL.
     *
     * The hash is filesystem-safe (hex string) and consistent across runs.
     *
     * @param aServerUrl Server URL to hash.
     * @return Hex string hash of the server URL.
     */
    static wxString HashServerUrl( const wxString& aServerUrl );

    /**
     * Generate a deterministic hash from a project ID.
     *
     * @param aProjectId Project identifier to hash.
     * @return Hex string hash of the project ID.
     */
    static wxString HashProjectId( const wxString& aProjectId );

    /**
     * Generate a cache key from query parameters.
     *
     * Creates a unique key for caching query results.
     *
     * @param aEndpoint API endpoint.
     * @param aParams Query parameters as key=value strings.
     * @return Cache key string.
     */
    static wxString GenerateCacheKey( const wxString& aEndpoint,
                                       const std::vector<wxString>& aParams );

    //-------------------------------------------------------------------------
    // Cleanup and Maintenance
    //-------------------------------------------------------------------------

    /**
     * Remove the cache directory for a specific project.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @return True if cleanup succeeded.
     */
    bool RemoveProjectCache( const wxString& aServerUrl, const wxString& aProjectId );

    /**
     * Remove the cache directory for a specific server.
     *
     * Removes all project caches under this server.
     *
     * @param aServerUrl Server URL.
     * @return True if cleanup succeeded.
     */
    bool RemoveServerCache( const wxString& aServerUrl );

    /**
     * Clean up orphaned cache directories.
     *
     * Removes cache directories that are not in the active set.
     * The active set is defined as (server_url, project_id) pairs.
     *
     * @param aActiveServers Set of active server URLs.
     * @param aActiveProjects Map of server URL to set of active project IDs.
     * @return Number of directories removed.
     */
    size_t CleanupOrphanedCaches( const std::set<wxString>& aActiveServers,
                                   const std::map<wxString, std::set<wxString>>& aActiveProjects );

    /**
     * Remove all cache directories.
     *
     * @return True if cleanup succeeded.
     */
    bool ClearAllCaches();

    /**
     * Remove all expired cache entries.
     *
     * Scans all cache entries and removes those that have exceeded their TTL.
     * This method should be called periodically to keep the cache clean.
     *
     * @return Number of entries removed.
     */
    size_t RemoveExpiredEntries();

    /**
     * Run periodic cleanup tasks.
     *
     * Performs all maintenance operations in one call:
     * - Remove expired entries
     * - Enforce size limits
     * - Update statistics
     *
     * Call this method periodically from a background task or timer.
     *
     * @return Total number of entries cleaned up.
     */
    size_t RunPeriodicCleanup();

    /**
     * Get total size of cache in bytes.
     *
     * @return Total cache size.
     */
    size_t GetCacheSize() const;

    /**
     * Get total number of cached files.
     *
     * @return Number of files in cache.
     */
    size_t GetCacheFileCount() const;

    /**
     * Enforce cache size limits.
     *
     * Removes oldest entries until cache is within limits.
     *
     * @return Number of entries removed.
     */
    size_t EnforceSizeLimits();

    //-------------------------------------------------------------------------
    // Metadata Cache Operations
    //-------------------------------------------------------------------------

    /**
     * Store metadata in the cache.
     *
     * Serializes the data to JSON and writes to the metadata cache directory.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKey Unique key for this cache entry.
     * @param aData Data to cache (JSON string).
     * @param aEtag Optional ETag for conditional requests.
     * @param aTtlSeconds Optional TTL override (0 uses config default).
     * @return True if caching succeeded.
     */
    bool StoreMetadata( const wxString& aServerUrl, const wxString& aProjectId,
                        const wxString& aCacheKey, const wxString& aData,
                        const wxString& aEtag = wxEmptyString, int64_t aTtlSeconds = 0 );

    /**
     * Retrieve metadata from the cache.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKey Cache key to look up.
     * @param aEntry Output cache entry with data and metadata.
     * @return True if entry was found and is valid, false otherwise.
     */
    bool RetrieveMetadata( const wxString& aServerUrl, const wxString& aProjectId,
                           const wxString& aCacheKey, PLM_CACHE_ENTRY& aEntry );

    /**
     * Check if cached metadata exists and is valid.
     *
     * Does not load the full data, just checks existence and expiration.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKey Cache key to check.
     * @param aEtag Output ETag if entry exists and has one.
     * @return True if valid cache entry exists.
     */
    bool HasValidMetadata( const wxString& aServerUrl, const wxString& aProjectId,
                           const wxString& aCacheKey, wxString& aEtag );

    /**
     * Invalidate a specific metadata cache entry.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKey Cache key to invalidate.
     * @return True if entry was found and removed.
     */
    bool InvalidateMetadata( const wxString& aServerUrl, const wxString& aProjectId,
                             const wxString& aCacheKey );

    /**
     * Invalidate all metadata for a project.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @return Number of entries invalidated.
     */
    size_t InvalidateProjectMetadata( const wxString& aServerUrl, const wxString& aProjectId );

    /**
     * Invalidate all metadata for a server.
     *
     * Iterates through all project caches under this server and invalidates
     * their metadata entries.
     *
     * @param aServerUrl Server URL.
     * @return Number of entries invalidated.
     */
    size_t InvalidateServerMetadata( const wxString& aServerUrl );

    /**
     * Invalidate a cache entry and all dependent entries.
     *
     * Performs cascade invalidation by removing the specified entry
     * and all entries whose keys start with the given prefix.
     * Use this when invalidating a folder to also invalidate all file
     * entries within that folder.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKeyPrefix Cache key prefix for cascade invalidation.
     * @return Number of entries invalidated.
     */
    size_t InvalidateCascade( const wxString& aServerUrl, const wxString& aProjectId,
                               const wxString& aCacheKeyPrefix );

    /**
     * Invalidate cache entries matching a pattern.
     *
     * Removes all entries whose keys contain the specified substring.
     * Useful for invalidating all entries related to a specific folder or file.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aPattern Pattern to match in cache keys.
     * @return Number of entries invalidated.
     */
    size_t InvalidateByPattern( const wxString& aServerUrl, const wxString& aProjectId,
                                 const wxString& aPattern );

    /**
     * Update the validation timestamp for a cache entry.
     *
     * Called when server returns 304 Not Modified.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKey Cache key to update.
     * @return True if entry was found and updated.
     */
    bool TouchMetadata( const wxString& aServerUrl, const wxString& aProjectId,
                        const wxString& aCacheKey );

    //-------------------------------------------------------------------------
    // ETag and Conditional Request Support
    //-------------------------------------------------------------------------

    /**
     * Result of a conditional cache lookup.
     *
     * Indicates whether cached data is available, needs validation,
     * or doesn't exist.
     */
    enum class CACHE_LOOKUP_RESULT
    {
        HIT,                ///< Valid cached data available
        NEEDS_VALIDATION,   ///< Cached data exists but needs server validation
        MISS                ///< No cached data exists
    };

    /**
     * Perform a conditional cache lookup.
     *
     * Returns cached data if valid, or indicates whether revalidation
     * is needed. Use this for implementing conditional GET requests.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKey Cache key to look up.
     * @param aEntry Output cache entry (populated on HIT or NEEDS_VALIDATION).
     * @return Lookup result indicating cache state.
     */
    CACHE_LOOKUP_RESULT ConditionalLookup( const wxString& aServerUrl,
                                            const wxString& aProjectId,
                                            const wxString& aCacheKey,
                                            PLM_CACHE_ENTRY& aEntry );

    /**
     * Get ETag for a cached entry.
     *
     * Use this to build If-None-Match headers for conditional requests.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKey Cache key to look up.
     * @return ETag string, or empty if no cached entry or no ETag.
     */
    wxString GetCachedETag( const wxString& aServerUrl, const wxString& aProjectId,
                            const wxString& aCacheKey );

    /**
     * Handle a 304 Not Modified response.
     *
     * Updates the cached entry's validation timestamp and extends TTL.
     * Call this when the server responds with 304 to a conditional request.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKey Cache key to update.
     * @return True if cache entry was found and updated.
     */
    bool HandleNotModified( const wxString& aServerUrl, const wxString& aProjectId,
                            const wxString& aCacheKey );

    /**
     * Handle a 200 OK response with new data.
     *
     * Updates the cache with the new data and ETag.
     * Call this when the server responds with fresh data.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKey Cache key.
     * @param aData New data to cache.
     * @param aEtag New ETag from server (may be empty).
     * @param aTtlSeconds Optional TTL override (0 uses config default).
     * @return True if caching succeeded.
     */
    bool HandleFreshResponse( const wxString& aServerUrl, const wxString& aProjectId,
                               const wxString& aCacheKey, const wxString& aData,
                               const wxString& aEtag = wxEmptyString, int64_t aTtlSeconds = 0 );

    /**
     * Check if ETag-based validation is supported.
     *
     * Returns true if the cached entry has an ETag for validation.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKey Cache key to check.
     * @return True if ETag validation is available.
     */
    bool SupportsETagValidation( const wxString& aServerUrl, const wxString& aProjectId,
                                  const wxString& aCacheKey );

    //-------------------------------------------------------------------------
    // TTL and Force Refresh Support
    //-------------------------------------------------------------------------

    /**
     * Store metadata with type-specific TTL.
     *
     * Uses the TTL configured for the specific cache type.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKey Cache key.
     * @param aData Data to cache.
     * @param aCacheType Type of cached data for TTL selection.
     * @param aEtag Optional ETag.
     * @return True if caching succeeded.
     */
    bool StoreTypedMetadata( const wxString& aServerUrl, const wxString& aProjectId,
                              const wxString& aCacheKey, const wxString& aData,
                              PLM_CACHE_TYPE aCacheType, const wxString& aEtag = wxEmptyString );

    /**
     * Retrieve metadata with force refresh option.
     *
     * If force refresh is true, treats any cached entry as expired
     * but still returns it for conditional request use.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKey Cache key.
     * @param aEntry Output cache entry.
     * @param aForceRefresh If true, ignores TTL and marks entry as needing validation.
     * @return True if entry exists (may still need validation if force refresh).
     */
    bool RetrieveMetadataEx( const wxString& aServerUrl, const wxString& aProjectId,
                              const wxString& aCacheKey, PLM_CACHE_ENTRY& aEntry,
                              bool aForceRefresh );

    /**
     * Force refresh a cache entry.
     *
     * Marks the entry as expired so the next access will trigger revalidation.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKey Cache key.
     * @return True if entry was found and marked for refresh.
     */
    bool ForceRefresh( const wxString& aServerUrl, const wxString& aProjectId,
                       const wxString& aCacheKey );

    /**
     * Force refresh all cached entries for a project.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @return Number of entries marked for refresh.
     */
    size_t ForceRefreshProject( const wxString& aServerUrl, const wxString& aProjectId );

    /**
     * Check if an entry has expired based on its TTL.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKey Cache key.
     * @return True if entry exists and is expired.
     */
    bool IsExpired( const wxString& aServerUrl, const wxString& aProjectId,
                    const wxString& aCacheKey );

    /**
     * Get the remaining TTL for a cache entry.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKey Cache key.
     * @return Remaining TTL in seconds, or -1 if not found or expired.
     */
    int64_t GetRemainingTTL( const wxString& aServerUrl, const wxString& aProjectId,
                              const wxString& aCacheKey );

    //-------------------------------------------------------------------------
    // Persistence
    //-------------------------------------------------------------------------

    /**
     * Save the cache index to disk.
     *
     * Writes a master index file containing all cache entry metadata.
     * This should be called on shutdown to preserve cache state.
     *
     * @return True if save succeeded.
     */
    bool SaveCacheIndex();

    /**
     * Load the cache index from disk.
     *
     * Reads the master index file and verifies that cached files exist.
     * Missing files are removed from the index.
     * This should be called on startup.
     *
     * @return True if load succeeded or no index exists yet.
     */
    bool LoadCacheIndex();

    /**
     * Verify integrity of cached files.
     *
     * Scans the cache directory and removes entries for missing files.
     *
     * @return Number of invalid entries removed.
     */
    size_t VerifyCacheIntegrity();

    /**
     * Rebuild the cache index from disk.
     *
     * Scans all cache files and rebuilds the index.
     * Use this when the index is corrupt.
     *
     * @return Number of entries recovered.
     */
    size_t RebuildCacheIndex();

    //-------------------------------------------------------------------------
    // Statistics
    //-------------------------------------------------------------------------

    /**
     * Get cache statistics.
     *
     * @return Current cache statistics.
     */
    PLM_CACHE_STATS GetStats() const { return m_stats; }

    /**
     * Reset cache statistics.
     */
    void ResetStats() { m_stats.Reset(); }

    /**
     * Refresh statistics by scanning the actual cache state.
     *
     * Updates size_bytes and entry_count by scanning the cache directory.
     * Hit/miss counters are preserved.
     */
    void RefreshStats();

    /**
     * Get cache diagnostic report string.
     *
     * Returns a formatted multi-line string suitable for display in
     * diagnostic dialogs or log output.
     *
     * @return Formatted diagnostic report.
     */
    wxString GetDiagnosticReport() const;

    /**
     * Get cache health status.
     *
     * Evaluates cache performance and returns a status indicator.
     *
     * @return "healthy", "degraded", or "poor" based on hit rate and usage.
     */
    wxString GetHealthStatus() const;

    /**
     * Log current statistics to the application log.
     *
     * Useful for debugging and monitoring cache performance.
     */
    void LogStats() const;

private:
    /**
     * Get the full file path for a metadata cache entry.
     *
     * @param aServerUrl Server URL.
     * @param aProjectId Project identifier.
     * @param aCacheKey Cache key.
     * @return Full path to the cache file.
     */
    wxString GetMetadataFilePath( const wxString& aServerUrl, const wxString& aProjectId,
                                  const wxString& aCacheKey );

    /**
     * Read a cache entry from disk.
     *
     * Parses the JSON envelope containing data and metadata.
     *
     * @param aFilePath Path to the cache file.
     * @param aEntry Output cache entry.
     * @return True if read and parse succeeded.
     */
    bool ReadCacheEntry( const wxString& aFilePath, PLM_CACHE_ENTRY& aEntry );

    /**
     * Write a cache entry to disk.
     *
     * Wraps the data in a JSON envelope with metadata.
     *
     * @param aFilePath Path to the cache file.
     * @param aEntry Cache entry to write.
     * @return True if write succeeded.
     */
    bool WriteCacheEntry( const wxString& aFilePath, const PLM_CACHE_ENTRY& aEntry );

    /**
     * Create a directory if it doesn't exist.
     *
     * @param aPath Directory path to create.
     * @return True if directory exists or was created.
     */
    bool EnsureDirectoryExists( const wxString& aPath );

    /**
     * Remove a directory and all its contents.
     *
     * @param aPath Directory path to remove.
     * @return True if removal succeeded.
     */
    bool RemoveDirectory( const wxString& aPath );

    /**
     * Calculate the size of a directory recursively.
     *
     * @param aPath Directory path.
     * @return Total size in bytes.
     */
    size_t CalculateDirectorySize( const wxString& aPath ) const;

    /**
     * Count files in a directory recursively.
     *
     * @param aPath Directory path.
     * @return Number of files.
     */
    size_t CountFiles( const wxString& aPath ) const;

    /**
     * Get the path to the master cache index file.
     *
     * @return Full path to the index file.
     */
    wxString GetCacheIndexPath() const;

    /**
     * Structure to track cache entries in memory.
     */
    struct CacheIndexEntry
    {
        wxString serverUrl;
        wxString projectId;
        wxString cacheKey;
        wxString filePath;
        int64_t  expiresAt = 0;
        int64_t  lastAccessed = 0;
    };

    /**
     * Scan directory for cache files and add to index.
     *
     * @param aServerHash Server hash directory.
     * @param aProjectHash Project hash directory.
     * @return Number of entries added.
     */
    size_t ScanProjectCacheDir( const wxString& aServerHash, const wxString& aProjectHash );

    /**
     * Sort cache index by last accessed time (oldest first).
     *
     * Used for LRU eviction when enforcing size limits.
     */
    void SortIndexByLRU();

    /**
     * Update the in-memory index entry access time.
     *
     * @param aFilePath Path to the cache file.
     */
    void UpdateIndexAccessTime( const wxString& aFilePath );

    /**
     * Remove an entry from the in-memory index.
     *
     * @param aFilePath Path to the cache file.
     * @return True if entry was found and removed.
     */
    bool RemoveFromIndex( const wxString& aFilePath );

    mutable std::recursive_mutex m_mutex;     ///< Protects all member data
    bool             m_initialized = false;   ///< Whether Initialize() was called
    wxString         m_cacheRootDir;          ///< Root cache directory
    PLM_CACHE_CONFIG m_config;                ///< Cache configuration
    PLM_CACHE_STATS  m_stats;                 ///< Cache statistics
    std::vector<CacheIndexEntry> m_cacheIndex; ///< In-memory cache index
};


#endif // PLM_CACHE_MANAGER_H
