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
 * @file plm_preview_cache.h
 * @brief Preview image caching for the PLM client interface.
 *
 * This header defines the PLM_PREVIEW_CACHE class which handles caching of
 * preview images for PLM files. It supports both server-provided previews
 * and locally generated fallback previews with LRU eviction.
 */

#ifndef PLM_PREVIEW_CACHE_H
#define PLM_PREVIEW_CACHE_H

#include <wx/string.h>
#include <wx/bitmap.h>
#include <cstdint>
#include <list>
#include <map>
#include <vector>


/**
 * Source of a preview image.
 */
enum class PLM_PREVIEW_SOURCE
{
    SERVER,     ///< Preview provided by PLM server
    LOCAL,      ///< Preview generated locally by KiCad
    UNKNOWN     ///< Source unknown or not set
};


/**
 * Information about a cached preview image.
 */
struct PLM_CACHED_PREVIEW
{
    wxString            file_id;           ///< PLM file identifier
    wxString            local_path;        ///< Path to cached preview file
    PLM_PREVIEW_SOURCE  source = PLM_PREVIEW_SOURCE::UNKNOWN; ///< Preview source
    int                 width = 0;         ///< Image width in pixels
    int                 height = 0;        ///< Image height in pixels
    size_t              size_bytes = 0;    ///< File size in bytes
    int64_t             cached_at = 0;     ///< When preview was cached
    int64_t             last_accessed = 0; ///< Last access timestamp (for LRU)
    wxString            file_version;      ///< Version of file this preview represents
    bool                is_valid = false;  ///< Whether cache entry is valid

    PLM_CACHED_PREVIEW() = default;

    PLM_CACHED_PREVIEW( const wxString& aFileId, PLM_PREVIEW_SOURCE aSource ) :
            file_id( aFileId ),
            source( aSource )
    {
    }

    /**
     * Check if preview file exists on disk.
     * @return True if local_path exists.
     */
    bool FileExists() const;

    /**
     * Get source name for display.
     * @return Human-readable source name.
     */
    wxString GetSourceName() const
    {
        switch( source )
        {
        case PLM_PREVIEW_SOURCE::SERVER: return wxS( "Server" );
        case PLM_PREVIEW_SOURCE::LOCAL:  return wxS( "Local" );
        default:                         return wxS( "Unknown" );
        }
    }
};


/**
 * Configuration for preview cache.
 */
struct PLM_PREVIEW_CACHE_CONFIG
{
    size_t  max_size_mb = 50;           ///< Maximum cache size in megabytes
    size_t  max_entries = 1000;         ///< Maximum number of cached previews
    int     default_width = 256;        ///< Default preview width
    int     default_height = 256;       ///< Default preview height
    bool    prefer_server = true;       ///< Prefer server previews over local
    bool    generate_local_fallback = true; ///< Generate local preview if server unavailable

    PLM_PREVIEW_CACHE_CONFIG() = default;

    /**
     * Get maximum cache size in bytes.
     * @return Max size in bytes.
     */
    size_t MaxSizeBytes() const
    {
        return max_size_mb * 1024 * 1024;
    }
};


/**
 * Statistics for preview cache.
 */
struct PLM_PREVIEW_CACHE_STATS
{
    uint64_t hits = 0;              ///< Cache hits
    uint64_t misses = 0;            ///< Cache misses
    uint64_t server_fetches = 0;    ///< Previews fetched from server
    uint64_t local_generates = 0;   ///< Previews generated locally
    uint64_t evictions = 0;         ///< LRU evictions
    size_t   current_size = 0;      ///< Current cache size in bytes
    size_t   entry_count = 0;       ///< Current entry count

    PLM_PREVIEW_CACHE_STATS() = default;

    /**
     * Get cache hit rate as percentage.
     * @return Hit rate (0.0 to 100.0).
     */
    double HitRate() const
    {
        uint64_t total = hits + misses;

        if( total == 0 )
            return 0.0;

        return ( static_cast<double>( hits ) / static_cast<double>( total ) ) * 100.0;
    }

    /**
     * Reset all statistics.
     */
    void Reset()
    {
        hits = 0;
        misses = 0;
        server_fetches = 0;
        local_generates = 0;
        evictions = 0;
        current_size = 0;
        entry_count = 0;
    }
};


class PLM_CACHE_MANAGER;


/**
 * Manages caching of preview images for PLM files.
 *
 * The preview cache stores PNG images for quick display of PLM file content.
 * It supports both server-provided previews and locally generated fallbacks.
 * An LRU eviction policy ensures the cache stays within size limits.
 *
 * Usage:
 * @code
 * PLM_PREVIEW_CACHE cache( &cacheManager );
 * cache.Initialize( "https://plm.example.com", "project-001" );
 *
 * // Try to get cached preview
 * wxBitmap preview;
 * if( !cache.GetPreview( "file-123", preview ) )
 * {
 *     // Cache miss - fetch from server or generate locally
 *     wxMemoryBuffer imageData = fetchFromServer( "file-123" );
 *     cache.StoreServerPreview( "file-123", imageData, "v1.0" );
 * }
 * @endcode
 */
class PLM_PREVIEW_CACHE
{
public:
    /**
     * Construct a preview cache.
     * @param aCacheManager Cache manager for directory operations.
     */
    explicit PLM_PREVIEW_CACHE( PLM_CACHE_MANAGER* aCacheManager );

    /**
     * Destructor.
     */
    ~PLM_PREVIEW_CACHE();

    // Non-copyable
    PLM_PREVIEW_CACHE( const PLM_PREVIEW_CACHE& ) = delete;
    PLM_PREVIEW_CACHE& operator=( const PLM_PREVIEW_CACHE& ) = delete;

    //-------------------------------------------------------------------------
    // Initialization
    //-------------------------------------------------------------------------

    /**
     * Initialize the preview cache for a server and project.
     *
     * @param aServerUrl PLM server URL.
     * @param aProjectId Project identifier.
     * @param aConfig Optional cache configuration.
     * @return True if initialization succeeded.
     */
    bool Initialize( const wxString& aServerUrl, const wxString& aProjectId,
                     const PLM_PREVIEW_CACHE_CONFIG& aConfig = PLM_PREVIEW_CACHE_CONFIG() );

    /**
     * Check if the cache is initialized.
     * @return True if Initialize() was called successfully.
     */
    bool IsInitialized() const { return m_initialized; }

    /**
     * Get current configuration.
     * @return Cache configuration.
     */
    const PLM_PREVIEW_CACHE_CONFIG& GetConfig() const { return m_config; }

    //-------------------------------------------------------------------------
    // Preview Access
    //-------------------------------------------------------------------------

    /**
     * Get a cached preview image.
     *
     * Updates the LRU access timestamp on hit.
     *
     * @param aFileId PLM file identifier.
     * @param aBitmap Output bitmap (only valid if returns true).
     * @return True if preview was found and loaded.
     */
    bool GetPreview( const wxString& aFileId, wxBitmap& aBitmap );

    /**
     * Get cached preview path without loading.
     *
     * @param aFileId PLM file identifier.
     * @return Path to cached preview file, or empty if not cached.
     */
    wxString GetPreviewPath( const wxString& aFileId ) const;

    /**
     * Check if a preview is cached.
     *
     * @param aFileId PLM file identifier.
     * @return True if preview exists in cache.
     */
    bool HasPreview( const wxString& aFileId ) const;

    /**
     * Get information about a cached preview.
     *
     * @param aFileId PLM file identifier.
     * @param aPreview Output preview information.
     * @return True if preview was found.
     */
    bool GetPreviewInfo( const wxString& aFileId, PLM_CACHED_PREVIEW& aPreview ) const;

    //-------------------------------------------------------------------------
    // Preview Storage
    //-------------------------------------------------------------------------

    /**
     * Store a server-provided preview image.
     *
     * @param aFileId PLM file identifier.
     * @param aImageData PNG image data.
     * @param aFileVersion Version of the file this preview represents.
     * @return True if storage succeeded.
     */
    bool StoreServerPreview( const wxString& aFileId, const wxMemoryBuffer& aImageData,
                             const wxString& aFileVersion = wxEmptyString );

    /**
     * Store a locally generated preview image.
     *
     * @param aFileId PLM file identifier.
     * @param aBitmap Bitmap to store as PNG.
     * @param aFileVersion Version of the file this preview represents.
     * @return True if storage succeeded.
     */
    bool StoreLocalPreview( const wxString& aFileId, const wxBitmap& aBitmap,
                            const wxString& aFileVersion = wxEmptyString );

    /**
     * Check if preview needs update based on file version.
     *
     * @param aFileId PLM file identifier.
     * @param aCurrentVersion Current file version.
     * @return True if cached preview is for a different version.
     */
    bool NeedsUpdate( const wxString& aFileId, const wxString& aCurrentVersion ) const;

    //-------------------------------------------------------------------------
    // LRU Management
    //-------------------------------------------------------------------------

    /**
     * Enforce cache size limits using LRU eviction.
     *
     * Removes least recently used entries until cache is within limits.
     *
     * @return Number of entries evicted.
     */
    size_t EnforceSizeLimits();

    /**
     * Touch an entry to update its LRU timestamp.
     *
     * @param aFileId PLM file identifier.
     */
    void TouchEntry( const wxString& aFileId );

    //-------------------------------------------------------------------------
    // Invalidation
    //-------------------------------------------------------------------------

    /**
     * Invalidate a specific preview.
     *
     * @param aFileId PLM file identifier.
     * @return True if preview was found and removed.
     */
    bool InvalidatePreview( const wxString& aFileId );

    /**
     * Invalidate all previews.
     *
     * @return Number of previews invalidated.
     */
    size_t InvalidateAll();

    //-------------------------------------------------------------------------
    // Statistics
    //-------------------------------------------------------------------------

    /**
     * Get cache statistics.
     * @return Current cache statistics.
     */
    PLM_PREVIEW_CACHE_STATS GetStats() const { return m_stats; }

    /**
     * Reset statistics.
     */
    void ResetStats() { m_stats.Reset(); }

    /**
     * Get total cache size.
     * @return Size in bytes.
     */
    size_t GetTotalSize() const;

    /**
     * Get number of cached previews.
     * @return Entry count.
     */
    size_t GetEntryCount() const { return m_previews.size(); }

private:
    /**
     * Get the preview cache directory.
     * @return Path to preview cache directory.
     */
    wxString GetPreviewCacheDir() const;

    /**
     * Get the full path for a preview file.
     * @param aFileId PLM file identifier.
     * @return Full path to preview file.
     */
    wxString GetPreviewFilePath( const wxString& aFileId ) const;

    /**
     * Load the preview index from disk.
     * @return True if index was loaded successfully.
     */
    bool LoadIndex();

    /**
     * Save the preview index to disk.
     * @return True if index was saved successfully.
     */
    bool SaveIndex();

    /**
     * Update LRU order for an entry.
     * @param aFileId File identifier to move to front of LRU list.
     */
    void UpdateLRU( const wxString& aFileId );

    /**
     * Evict the least recently used entry.
     * @return True if an entry was evicted.
     */
    bool EvictLRU();

    PLM_CACHE_MANAGER*                     m_cacheManager;  ///< Cache manager reference
    wxString                               m_serverUrl;     ///< PLM server URL
    wxString                               m_projectId;     ///< Project identifier
    bool                                   m_initialized = false; ///< Initialization state
    PLM_PREVIEW_CACHE_CONFIG               m_config;        ///< Cache configuration
    PLM_PREVIEW_CACHE_STATS                m_stats;         ///< Cache statistics
    std::map<wxString, PLM_CACHED_PREVIEW> m_previews;      ///< Preview index
    std::list<wxString>                    m_lruOrder;      ///< LRU order (front = most recent)
};


#endif // PLM_PREVIEW_CACHE_H
