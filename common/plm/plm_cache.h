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
 * @file plm_cache.h
 * @brief Caching structures for the PLM client interface.
 *
 * This header defines structures for managing cached PLM data including
 * cache entries with ETag and TTL support, cache configuration, and
 * cache statistics for monitoring performance.
 */

#ifndef PLM_CACHE_H
#define PLM_CACHE_H

#include <wx/string.h>
#include <cstdint>
#include <ctime>
#include <vector>


/**
 * A single cache entry storing PLM data.
 *
 * Supports both ETag-based validation and TTL-based expiration.
 * The cache layer uses whichever mechanism the server supports.
 */
struct PLM_CACHE_ENTRY
{
    wxString    key;              ///< Unique cache key (typically URL or resource ID)
    wxString    data;             ///< Cached data (JSON or other serialized format)
    wxString    etag;             ///< ETag for conditional requests (if server supports)
    wxString    content_type;     ///< MIME type of cached content
    int64_t     expires_at = 0;   ///< Expiration timestamp (Unix epoch seconds)
    int64_t     last_validated = 0; ///< When entry was last validated with server
    int64_t     created_at = 0;   ///< When entry was first cached
    size_t      size_bytes = 0;   ///< Size of data in bytes

    PLM_CACHE_ENTRY() = default;

    PLM_CACHE_ENTRY( const wxString& aKey, const wxString& aData ) :
            key( aKey ),
            data( aData ),
            created_at( std::time( nullptr ) ),
            size_bytes( aData.length() )
    {
    }

    /**
     * Check if this entry has expired based on TTL.
     * @return True if the entry has passed its expiration time.
     */
    bool IsExpired() const
    {
        if( expires_at == 0 )
            return false;

        return std::time( nullptr ) >= expires_at;
    }

    /**
     * Check if this entry has an ETag for validation.
     * @return True if ETag-based validation is possible.
     */
    bool HasETag() const
    {
        return !etag.IsEmpty();
    }

    /**
     * Check if this entry needs revalidation.
     * @param aMaxAge Maximum age in seconds before revalidation.
     * @return True if entry should be validated with the server.
     */
    bool NeedsRevalidation( int64_t aMaxAge ) const
    {
        if( IsExpired() )
            return true;

        int64_t now = std::time( nullptr );
        int64_t age = now - last_validated;

        return age >= aMaxAge;
    }

    /**
     * Update the last validated timestamp to now.
     */
    void MarkValidated()
    {
        last_validated = std::time( nullptr );
    }

    /**
     * Set expiration based on TTL from now.
     * @param aTtlSeconds Time-to-live in seconds.
     */
    void SetTTL( int64_t aTtlSeconds )
    {
        expires_at = std::time( nullptr ) + aTtlSeconds;
    }
};


/**
 * Configuration for the PLM cache layer.
 *
 * Controls caching behavior including validation strategy,
 * size limits, and default TTL values.
 */
/**
 * Types of cached data with different TTL requirements.
 */
enum class PLM_CACHE_TYPE
{
    FOLDER_LISTING,   ///< Folder structure - changes infrequently
    FILE_INFO,        ///< File metadata - changes on state transitions
    FILE_CONTENT,     ///< Actual file data - changes on upload
    SCHEMA,           ///< PLM schema - rarely changes
    PREVIEW,          ///< Preview images - changes with file content
    SEARCH_RESULTS,   ///< Search results - may change frequently
    USER_INFO         ///< User/permission data - session duration
};


struct PLM_CACHE_CONFIG
{
    bool    use_etag = true;            ///< Use ETag-based conditional requests
    bool    use_ttl = true;             ///< Use TTL-based expiration
    int64_t fallback_ttl_seconds = 300; ///< Default TTL when server doesn't specify (5 min)
    int64_t max_ttl_seconds = 86400;    ///< Maximum TTL to accept (24 hours)
    int64_t revalidation_interval = 60; ///< How often to check with server (seconds)
    size_t  max_size_mb = 100;          ///< Maximum cache size in megabytes
    size_t  max_entries = 10000;        ///< Maximum number of cache entries
    bool    persist_to_disk = true;     ///< Save cache to disk between sessions
    wxString cache_directory;           ///< Directory for persistent cache storage

    // Per-type TTL settings (in seconds)
    int64_t folder_listing_ttl = 300;   ///< Folder listings TTL (5 min)
    int64_t file_info_ttl = 60;         ///< File metadata TTL (1 min)
    int64_t file_content_ttl = 3600;    ///< File content TTL (1 hour)
    int64_t schema_ttl = 86400;         ///< Schema TTL (24 hours)
    int64_t preview_ttl = 3600;         ///< Preview images TTL (1 hour)
    int64_t search_results_ttl = 30;    ///< Search results TTL (30 sec)
    int64_t user_info_ttl = 3600;       ///< User info TTL (1 hour)

    PLM_CACHE_CONFIG() = default;

    /**
     * Get TTL for a specific cache type.
     * @param aType Cache type.
     * @return TTL in seconds for that type.
     */
    int64_t GetTTLForType( PLM_CACHE_TYPE aType ) const
    {
        switch( aType )
        {
        case PLM_CACHE_TYPE::FOLDER_LISTING:  return folder_listing_ttl;
        case PLM_CACHE_TYPE::FILE_INFO:       return file_info_ttl;
        case PLM_CACHE_TYPE::FILE_CONTENT:    return file_content_ttl;
        case PLM_CACHE_TYPE::SCHEMA:          return schema_ttl;
        case PLM_CACHE_TYPE::PREVIEW:         return preview_ttl;
        case PLM_CACHE_TYPE::SEARCH_RESULTS:  return search_results_ttl;
        case PLM_CACHE_TYPE::USER_INFO:       return user_info_ttl;
        default:                              return fallback_ttl_seconds;
        }
    }

    /**
     * Get maximum cache size in bytes.
     * @return Max size in bytes.
     */
    size_t MaxSizeBytes() const
    {
        return max_size_mb * 1024 * 1024;
    }

    /**
     * Create configuration for aggressive caching.
     * @return Config with longer TTLs and larger limits.
     */
    static PLM_CACHE_CONFIG Aggressive()
    {
        PLM_CACHE_CONFIG config;
        config.fallback_ttl_seconds = 3600;
        config.max_ttl_seconds = 604800;
        config.revalidation_interval = 300;
        config.max_size_mb = 500;
        return config;
    }

    /**
     * Create configuration for minimal caching.
     * @return Config with short TTLs for frequently changing data.
     */
    static PLM_CACHE_CONFIG Minimal()
    {
        PLM_CACHE_CONFIG config;
        config.fallback_ttl_seconds = 30;
        config.max_ttl_seconds = 300;
        config.revalidation_interval = 10;
        config.max_size_mb = 10;
        config.persist_to_disk = false;
        return config;
    }

    /**
     * Create configuration with caching disabled.
     * @return Config that bypasses cache entirely.
     */
    static PLM_CACHE_CONFIG Disabled()
    {
        PLM_CACHE_CONFIG config;
        config.use_etag = false;
        config.use_ttl = false;
        config.max_size_mb = 0;
        config.max_entries = 0;
        config.persist_to_disk = false;
        return config;
    }
};


/**
 * Statistics about cache performance.
 *
 * Used for monitoring and debugging cache behavior.
 */
struct PLM_CACHE_STATS
{
    uint64_t hits = 0;              ///< Number of cache hits
    uint64_t misses = 0;            ///< Number of cache misses
    uint64_t validations = 0;       ///< Number of server validations
    uint64_t validation_hits = 0;   ///< Validations where cache was still valid
    uint64_t evictions = 0;         ///< Number of entries evicted
    size_t   size_bytes = 0;        ///< Current cache size in bytes
    size_t   entry_count = 0;       ///< Current number of entries
    int64_t  oldest_entry = 0;      ///< Timestamp of oldest entry

    PLM_CACHE_STATS() = default;

    /**
     * Get cache hit rate as a percentage.
     * @return Hit rate (0.0 to 100.0), or 0 if no requests.
     */
    double HitRate() const
    {
        uint64_t total = hits + misses;

        if( total == 0 )
            return 0.0;

        return ( static_cast<double>( hits ) / static_cast<double>( total ) ) * 100.0;
    }

    /**
     * Get validation efficiency as a percentage.
     * @return Percentage of validations that confirmed cache validity.
     */
    double ValidationEfficiency() const
    {
        if( validations == 0 )
            return 0.0;

        return ( static_cast<double>( validation_hits ) / static_cast<double>( validations ) )
               * 100.0;
    }

    /**
     * Record a cache hit.
     */
    void RecordHit()
    {
        ++hits;
    }

    /**
     * Record a cache miss.
     */
    void RecordMiss()
    {
        ++misses;
    }

    /**
     * Record a validation with result.
     * @param aStillValid True if server confirmed cache was valid.
     */
    void RecordValidation( bool aStillValid )
    {
        ++validations;

        if( aStillValid )
            ++validation_hits;
    }

    /**
     * Record an eviction.
     */
    void RecordEviction()
    {
        ++evictions;
    }

    /**
     * Reset all statistics to zero.
     */
    void Reset()
    {
        hits = 0;
        misses = 0;
        validations = 0;
        validation_hits = 0;
        evictions = 0;
        size_bytes = 0;
        entry_count = 0;
        oldest_entry = 0;
    }

    /**
     * Get total number of requests (hits + misses).
     * @return Total request count.
     */
    uint64_t TotalRequests() const
    {
        return hits + misses;
    }

    /**
     * Get cache size in human-readable format.
     * @return Size string (e.g., "1.5 MB", "256 KB").
     */
    wxString GetSizeString() const
    {
        if( size_bytes >= 1024 * 1024 * 1024 )
        {
            return wxString::Format( wxS( "%.2f GB" ),
                                     static_cast<double>( size_bytes ) / ( 1024.0 * 1024.0 * 1024.0 ) );
        }
        else if( size_bytes >= 1024 * 1024 )
        {
            return wxString::Format( wxS( "%.2f MB" ),
                                     static_cast<double>( size_bytes ) / ( 1024.0 * 1024.0 ) );
        }
        else if( size_bytes >= 1024 )
        {
            return wxString::Format( wxS( "%.2f KB" ),
                                     static_cast<double>( size_bytes ) / 1024.0 );
        }
        else
        {
            return wxString::Format( wxS( "%zu B" ), size_bytes );
        }
    }

    /**
     * Get formatted diagnostic report.
     * @return Multi-line string with all statistics.
     */
    wxString GetDiagnosticReport() const
    {
        wxString report;
        report += wxString::Format( wxS( "Cache Statistics:\n" ) );
        report += wxString::Format( wxS( "  Entries: %zu\n" ), entry_count );
        report += wxString::Format( wxS( "  Size: %s\n" ), GetSizeString() );
        report += wxString::Format( wxS( "  Hit Rate: %.1f%% (%llu hits / %llu total)\n" ),
                                    HitRate(), static_cast<unsigned long long>( hits ),
                                    static_cast<unsigned long long>( TotalRequests() ) );
        report += wxString::Format( wxS( "  Validation Efficiency: %.1f%% (%llu / %llu)\n" ),
                                    ValidationEfficiency(),
                                    static_cast<unsigned long long>( validation_hits ),
                                    static_cast<unsigned long long>( validations ) );
        report += wxString::Format( wxS( "  Evictions: %llu\n" ),
                                    static_cast<unsigned long long>( evictions ) );

        return report;
    }
};


#endif // PLM_CACHE_H
