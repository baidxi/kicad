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
 * @file plm_diff_pcb.h
 * @brief PCB diff parsing for PLM merge/diff operations.
 */

#ifndef PLM_DIFF_PCB_H
#define PLM_DIFF_PCB_H

#include <kicommon.h>

#include <wx/string.h>

#include <cstdint>
#include <map>
#include <vector>


/**
 * Type of PCB element for diff tracking.
 */
enum class PLM_PCB_ELEMENT_TYPE
{
    FOOTPRINT,        ///< Component footprint instance
    PAD,              ///< Footprint pad
    TRACK,            ///< Copper track segment
    ARC,              ///< Copper arc segment
    VIA,              ///< Via
    ZONE,             ///< Copper zone (pour)
    GRAPHIC_LINE,     ///< Graphic line
    GRAPHIC_RECT,     ///< Graphic rectangle
    GRAPHIC_CIRCLE,   ///< Graphic circle
    GRAPHIC_ARC,      ///< Graphic arc
    GRAPHIC_POLY,     ///< Graphic polygon
    TEXT,             ///< Text annotation
    TEXTBOX,          ///< Text box
    DIMENSION,        ///< Dimension annotation
    TARGET,           ///< Target marker
    KEEPOUT,          ///< Keep-out zone
    GROUP             ///< Element group
};


/**
 * Copper layer identifier.
 */
enum class PLM_PCB_LAYER
{
    UNKNOWN = -1,
    F_CU = 0,         ///< Front copper
    B_CU = 31,        ///< Back copper
    IN1_CU = 1,       ///< Inner copper 1
    IN2_CU = 2,       ///< Inner copper 2
    F_SILKS = 36,     ///< Front silkscreen
    B_SILKS = 37,     ///< Back silkscreen
    F_MASK = 38,      ///< Front solder mask
    B_MASK = 39,      ///< Back solder mask
    F_PASTE = 34,     ///< Front paste
    B_PASTE = 35,     ///< Back paste
    F_ADHES = 32,     ///< Front adhesive
    B_ADHES = 33,     ///< Back adhesive
    F_FAB = 40,       ///< Front fabrication
    B_FAB = 41,       ///< Back fabrication
    F_CRTYD = 42,     ///< Front courtyard
    B_CRTYD = 43,     ///< Back courtyard
    EDGE_CUTS = 44,   ///< Board edge
    MARGIN = 45,      ///< Margin
    USER_1 = 46,      ///< User layer 1
    MULTI = 100       ///< Multiple layers
};


/**
 * Position and orientation for PCB elements.
 */
struct PLM_PCB_GEOMETRY
{
    int32_t x = 0;            ///< X position (nanometers)
    int32_t y = 0;            ///< Y position (nanometers)
    int32_t rotation = 0;     ///< Rotation in tenths of a degree
    bool    flipped = false;  ///< On back side of board

    bool operator==( const PLM_PCB_GEOMETRY& aOther ) const
    {
        return x == aOther.x && y == aOther.y &&
               rotation == aOther.rotation && flipped == aOther.flipped;
    }

    bool operator!=( const PLM_PCB_GEOMETRY& aOther ) const { return !( *this == aOther ); }
};


/**
 * PCB point for coordinates.
 */
struct PLM_PCB_POINT
{
    int32_t x = 0;
    int32_t y = 0;

    bool operator==( const PLM_PCB_POINT& aOther ) const
    {
        return x == aOther.x && y == aOther.y;
    }

    bool operator!=( const PLM_PCB_POINT& aOther ) const { return !( *this == aOther ); }
};


/**
 * Footprint instance extracted from PCB.
 */
struct PLM_PCB_FOOTPRINT
{
    wxString                     uuid;          ///< Unique identifier
    wxString                     lib_id;        ///< Library ID (lib:footprint)
    wxString                     reference;     ///< Reference designator
    wxString                     value;         ///< Component value
    PLM_PCB_GEOMETRY             geometry;      ///< Position and orientation
    PLM_PCB_LAYER                layer;         ///< Placement layer
    std::map<wxString, wxString> properties;    ///< Additional properties
    std::vector<wxString>        pad_uuids;     ///< UUIDs of pads

    bool operator==( const PLM_PCB_FOOTPRINT& aOther ) const
    {
        return uuid == aOther.uuid && lib_id == aOther.lib_id &&
               reference == aOther.reference && value == aOther.value &&
               geometry == aOther.geometry && layer == aOther.layer &&
               properties == aOther.properties;
    }

    bool operator!=( const PLM_PCB_FOOTPRINT& aOther ) const { return !( *this == aOther ); }
};


/**
 * Track segment extracted from PCB.
 */
struct PLM_PCB_TRACK
{
    wxString       uuid;       ///< Unique identifier
    PLM_PCB_POINT  start;      ///< Start point
    PLM_PCB_POINT  end;        ///< End point
    int32_t        width;      ///< Track width (nanometers)
    PLM_PCB_LAYER  layer;      ///< Copper layer
    wxString       net;        ///< Net name

    bool operator==( const PLM_PCB_TRACK& aOther ) const
    {
        return uuid == aOther.uuid && start == aOther.start && end == aOther.end &&
               width == aOther.width && layer == aOther.layer && net == aOther.net;
    }

    bool operator!=( const PLM_PCB_TRACK& aOther ) const { return !( *this == aOther ); }
};


/**
 * Via extracted from PCB.
 */
struct PLM_PCB_VIA
{
    wxString       uuid;         ///< Unique identifier
    PLM_PCB_POINT  position;     ///< Center position
    int32_t        size;         ///< Via size (nanometers)
    int32_t        drill;        ///< Drill diameter (nanometers)
    PLM_PCB_LAYER  start_layer;  ///< Start layer
    PLM_PCB_LAYER  end_layer;    ///< End layer
    wxString       net;          ///< Net name

    bool operator==( const PLM_PCB_VIA& aOther ) const
    {
        return uuid == aOther.uuid && position == aOther.position &&
               size == aOther.size && drill == aOther.drill &&
               start_layer == aOther.start_layer && end_layer == aOther.end_layer &&
               net == aOther.net;
    }

    bool operator!=( const PLM_PCB_VIA& aOther ) const { return !( *this == aOther ); }
};


/**
 * Zone (copper pour) extracted from PCB.
 */
struct PLM_PCB_ZONE
{
    wxString                   uuid;       ///< Unique identifier
    wxString                   net;        ///< Net name
    PLM_PCB_LAYER              layer;      ///< Copper layer
    std::vector<PLM_PCB_POINT> outline;    ///< Zone outline points
    int32_t                    priority;   ///< Zone priority

    bool operator==( const PLM_PCB_ZONE& aOther ) const
    {
        return uuid == aOther.uuid && net == aOther.net && layer == aOther.layer &&
               outline == aOther.outline && priority == aOther.priority;
    }

    bool operator!=( const PLM_PCB_ZONE& aOther ) const { return !( *this == aOther ); }
};


/**
 * Graphic element extracted from PCB.
 */
struct PLM_PCB_GRAPHIC
{
    wxString             uuid;       ///< Unique identifier
    PLM_PCB_ELEMENT_TYPE type;       ///< Graphic type
    PLM_PCB_LAYER        layer;      ///< Layer
    PLM_PCB_POINT        start;      ///< Start/center point
    PLM_PCB_POINT        end;        ///< End point
    int32_t              width;      ///< Line width

    bool operator==( const PLM_PCB_GRAPHIC& aOther ) const
    {
        return uuid == aOther.uuid && type == aOther.type && layer == aOther.layer &&
               start == aOther.start && end == aOther.end && width == aOther.width;
    }

    bool operator!=( const PLM_PCB_GRAPHIC& aOther ) const { return !( *this == aOther ); }
};


/**
 * Text annotation extracted from PCB.
 */
struct PLM_PCB_TEXT
{
    wxString         uuid;       ///< Unique identifier
    wxString         text;       ///< Text content
    PLM_PCB_GEOMETRY geometry;   ///< Position and orientation
    PLM_PCB_LAYER    layer;      ///< Layer

    bool operator==( const PLM_PCB_TEXT& aOther ) const
    {
        return uuid == aOther.uuid && text == aOther.text &&
               geometry == aOther.geometry && layer == aOther.layer;
    }

    bool operator!=( const PLM_PCB_TEXT& aOther ) const { return !( *this == aOther ); }
};


/**
 * Complete PCB data for diff comparison.
 */
struct PLM_PCB_DATA
{
    wxString                       filename;      ///< Source filename
    wxString                       board_uuid;    ///< Board UUID
    std::vector<PLM_PCB_FOOTPRINT> footprints;    ///< All footprints
    std::vector<PLM_PCB_TRACK>     tracks;        ///< All track segments
    std::vector<PLM_PCB_VIA>       vias;          ///< All vias
    std::vector<PLM_PCB_ZONE>      zones;         ///< All zones
    std::vector<PLM_PCB_GRAPHIC>   graphics;      ///< All graphic elements
    std::vector<PLM_PCB_TEXT>      texts;         ///< All text annotations
    int32_t                        layer_count;   ///< Number of copper layers

    size_t GetTotalElementCount() const
    {
        return footprints.size() + tracks.size() + vias.size() +
               zones.size() + graphics.size() + texts.size();
    }
};


/**
 * Change type for PCB diff results.
 */
enum class PLM_PCB_CHANGE_TYPE
{
    ADDED,      ///< Element added in new version
    REMOVED,    ///< Element removed in new version
    MODIFIED,   ///< Element modified between versions
    MOVED       ///< Element moved but otherwise unchanged
};


/**
 * Single change in PCB diff.
 */
struct PLM_PCB_CHANGE
{
    PLM_PCB_CHANGE_TYPE   change_type;     ///< Type of change
    PLM_PCB_ELEMENT_TYPE  element_type;    ///< Type of element changed
    wxString              uuid;            ///< Element UUID
    wxString              description;     ///< Human-readable description
    wxString              old_value;       ///< Previous value (for modifications)
    wxString              new_value;       ///< New value (for modifications)
    PLM_PCB_LAYER         layer;           ///< Layer of element
};


/**
 * Complete diff result between two PCBs.
 */
struct PLM_PCB_DIFF_RESULT
{
    wxString                     base_file;    ///< Base PCB file
    wxString                     new_file;     ///< New PCB file
    std::vector<PLM_PCB_CHANGE>  changes;      ///< All detected changes
    bool                         success;      ///< True if diff completed
    wxString                     error;        ///< Error message if failed

    size_t GetAddedCount() const
    {
        size_t count = 0;

        for( const auto& c : changes )
        {
            if( c.change_type == PLM_PCB_CHANGE_TYPE::ADDED )
                ++count;
        }

        return count;
    }

    size_t GetRemovedCount() const
    {
        size_t count = 0;

        for( const auto& c : changes )
        {
            if( c.change_type == PLM_PCB_CHANGE_TYPE::REMOVED )
                ++count;
        }

        return count;
    }

    size_t GetModifiedCount() const
    {
        size_t count = 0;

        for( const auto& c : changes )
        {
            if( c.change_type == PLM_PCB_CHANGE_TYPE::MODIFIED )
                ++count;
        }

        return count;
    }

    std::vector<PLM_PCB_CHANGE> GetChangesByType( PLM_PCB_ELEMENT_TYPE aType ) const
    {
        std::vector<PLM_PCB_CHANGE> result;

        for( const auto& c : changes )
        {
            if( c.element_type == aType )
                result.push_back( c );
        }

        return result;
    }

    std::vector<PLM_PCB_CHANGE> GetChangesByLayer( PLM_PCB_LAYER aLayer ) const
    {
        std::vector<PLM_PCB_CHANGE> result;

        for( const auto& c : changes )
        {
            if( c.layer == aLayer )
                result.push_back( c );
        }

        return result;
    }
};


/**
 * PCB diff parser for PLM operations.
 *
 * Parses KiCad PCB files and extracts element data for comparison.
 * Handles all standard PCB element types including footprints, tracks,
 * vias, zones, and graphics.
 *
 * Usage:
 * @code
 * PLM_PCB_DIFF_PARSER parser;
 *
 * PLM_PCB_DATA base = parser.ParsePCB( "base.kicad_pcb" );
 * PLM_PCB_DATA newer = parser.ParsePCB( "new.kicad_pcb" );
 *
 * PLM_PCB_DIFF_RESULT diff = parser.ComputeDiff( base, newer );
 *
 * for( const auto& change : diff.changes )
 *     wxLogMessage( "%s: %s", change.uuid, change.description );
 * @endcode
 */
class KICOMMON_API PLM_PCB_DIFF_PARSER
{
public:
    PLM_PCB_DIFF_PARSER();
    ~PLM_PCB_DIFF_PARSER();

    /**
     * Parse a PCB file and extract element data.
     *
     * @param aFilename Path to PCB file (.kicad_pcb).
     * @return Extracted PCB data.
     */
    PLM_PCB_DATA ParsePCB( const wxString& aFilename );

    /**
     * Compute diff between two PCB data sets.
     *
     * @param aBase Base (older) PCB data.
     * @param aNew New PCB data.
     * @return Diff result with all changes.
     */
    PLM_PCB_DIFF_RESULT ComputeDiff( const PLM_PCB_DATA& aBase, const PLM_PCB_DATA& aNew );

    /**
     * Check if last operation had an error.
     * @return True if error occurred.
     */
    bool HasError() const { return !m_lastError.IsEmpty(); }

    /**
     * Get the last error message.
     * @return Error message.
     */
    wxString GetLastError() const { return m_lastError; }

private:
    /**
     * Parse footprint from S-expression.
     *
     * @param aContent Footprint S-expression content.
     * @return Parsed footprint.
     */
    PLM_PCB_FOOTPRINT ParseFootprint( const wxString& aContent );

    /**
     * Parse track segment from S-expression.
     *
     * @param aContent Track S-expression content.
     * @return Parsed track.
     */
    PLM_PCB_TRACK ParseTrack( const wxString& aContent );

    /**
     * Parse via from S-expression.
     *
     * @param aContent Via S-expression content.
     * @return Parsed via.
     */
    PLM_PCB_VIA ParseVia( const wxString& aContent );

    /**
     * Parse zone from S-expression.
     *
     * @param aContent Zone S-expression content.
     * @return Parsed zone.
     */
    PLM_PCB_ZONE ParseZone( const wxString& aContent );

    /**
     * Parse layer name to enum.
     *
     * @param aLayerName Layer name string.
     * @return Layer enum value.
     */
    PLM_PCB_LAYER ParseLayer( const wxString& aLayerName );

    /**
     * Compare footprints for changes.
     *
     * @param aBase Base footprint.
     * @param aNew New footprint.
     * @param aChanges Output changes.
     */
    void CompareFootprints( const PLM_PCB_FOOTPRINT& aBase, const PLM_PCB_FOOTPRINT& aNew,
                            std::vector<PLM_PCB_CHANGE>& aChanges );

    /**
     * Compare tracks for changes.
     *
     * @param aBase Base track.
     * @param aNew New track.
     * @param aChanges Output changes.
     */
    void CompareTracks( const PLM_PCB_TRACK& aBase, const PLM_PCB_TRACK& aNew,
                        std::vector<PLM_PCB_CHANGE>& aChanges );

    wxString m_lastError; ///< Last error message
};


#endif // PLM_DIFF_PCB_H
