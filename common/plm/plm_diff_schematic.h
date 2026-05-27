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
 * @file plm_diff_schematic.h
 * @brief Schematic diff parsing for PLM merge/diff operations.
 */

#ifndef PLM_DIFF_SCHEMATIC_H
#define PLM_DIFF_SCHEMATIC_H

#include <kicommon.h>

#include <wx/string.h>

#include <cstdint>
#include <map>
#include <vector>


/**
 * Type of schematic element for diff tracking.
 */
enum class PLM_SCH_ELEMENT_TYPE
{
    SYMBOL,           ///< Component symbol instance
    WIRE,             ///< Electrical wire connection
    BUS,              ///< Bus connection
    JUNCTION,         ///< Wire junction
    NO_CONNECT,       ///< No-connect marker
    LABEL,            ///< Net label
    GLOBAL_LABEL,     ///< Global net label
    HIER_LABEL,       ///< Hierarchical label
    SHEET,            ///< Sheet instance (sub-schematic)
    SHEET_PIN,        ///< Sheet pin for hierarchical connection
    TEXT,             ///< Text annotation
    TEXTBOX,          ///< Text box
    GRAPHIC_LINE,     ///< Graphic line
    GRAPHIC_RECT,     ///< Graphic rectangle
    GRAPHIC_CIRCLE,   ///< Graphic circle
    GRAPHIC_ARC,      ///< Graphic arc
    BITMAP,           ///< Embedded image
    TABLE,            ///< Table element
    BUS_ENTRY         ///< Bus entry point
};


/**
 * Position and size information for schematic elements.
 */
struct PLM_SCH_GEOMETRY
{
    int32_t x = 0;            ///< X position
    int32_t y = 0;            ///< Y position
    int32_t width = 0;        ///< Width (for rectangles, textboxes)
    int32_t height = 0;       ///< Height (for rectangles, textboxes)
    int32_t rotation = 0;     ///< Rotation in tenths of a degree
    bool    mirrored = false; ///< Horizontal mirror flag

    bool operator==( const PLM_SCH_GEOMETRY& aOther ) const
    {
        return x == aOther.x && y == aOther.y && width == aOther.width &&
               height == aOther.height && rotation == aOther.rotation &&
               mirrored == aOther.mirrored;
    }

    bool operator!=( const PLM_SCH_GEOMETRY& aOther ) const { return !( *this == aOther ); }
};


/**
 * Wire/bus endpoint for connection tracking.
 */
struct PLM_SCH_ENDPOINT
{
    int32_t x = 0;
    int32_t y = 0;

    bool operator==( const PLM_SCH_ENDPOINT& aOther ) const
    {
        return x == aOther.x && y == aOther.y;
    }

    bool operator!=( const PLM_SCH_ENDPOINT& aOther ) const { return !( *this == aOther ); }
};


/**
 * Symbol instance extracted from schematic.
 */
struct PLM_SCH_SYMBOL
{
    wxString         uuid;            ///< Unique identifier
    wxString         lib_id;          ///< Library ID (lib:symbol)
    wxString         reference;       ///< Reference designator (R1, C2, etc.)
    wxString         value;           ///< Component value
    wxString         footprint;       ///< Footprint assignment
    int              unit = 1;        ///< Unit number for multi-unit parts
    PLM_SCH_GEOMETRY geometry;        ///< Position and orientation
    std::map<wxString, wxString> properties; ///< Additional properties

    bool operator==( const PLM_SCH_SYMBOL& aOther ) const
    {
        return uuid == aOther.uuid && lib_id == aOther.lib_id &&
               reference == aOther.reference && value == aOther.value &&
               footprint == aOther.footprint && unit == aOther.unit &&
               geometry == aOther.geometry && properties == aOther.properties;
    }

    bool operator!=( const PLM_SCH_SYMBOL& aOther ) const { return !( *this == aOther ); }
};


/**
 * Wire or bus connection extracted from schematic.
 */
struct PLM_SCH_CONNECTION
{
    wxString          uuid;       ///< Unique identifier
    PLM_SCH_ENDPOINT  start;      ///< Start point
    PLM_SCH_ENDPOINT  end;        ///< End point
    bool              is_bus;     ///< True if bus, false if wire

    bool operator==( const PLM_SCH_CONNECTION& aOther ) const
    {
        return uuid == aOther.uuid && start == aOther.start &&
               end == aOther.end && is_bus == aOther.is_bus;
    }

    bool operator!=( const PLM_SCH_CONNECTION& aOther ) const { return !( *this == aOther ); }
};


/**
 * Label extracted from schematic.
 */
struct PLM_SCH_LABEL
{
    wxString              uuid;       ///< Unique identifier
    wxString              text;       ///< Label text (net name)
    PLM_SCH_ELEMENT_TYPE  type;       ///< Label type (local, global, hierarchical)
    PLM_SCH_GEOMETRY      geometry;   ///< Position and orientation

    bool operator==( const PLM_SCH_LABEL& aOther ) const
    {
        return uuid == aOther.uuid && text == aOther.text &&
               type == aOther.type && geometry == aOther.geometry;
    }

    bool operator!=( const PLM_SCH_LABEL& aOther ) const { return !( *this == aOther ); }
};


/**
 * Text annotation extracted from schematic.
 */
struct PLM_SCH_TEXT
{
    wxString         uuid;       ///< Unique identifier
    wxString         text;       ///< Text content
    bool             is_textbox; ///< True if textbox, false if plain text
    PLM_SCH_GEOMETRY geometry;   ///< Position and size

    bool operator==( const PLM_SCH_TEXT& aOther ) const
    {
        return uuid == aOther.uuid && text == aOther.text &&
               is_textbox == aOther.is_textbox && geometry == aOther.geometry;
    }

    bool operator!=( const PLM_SCH_TEXT& aOther ) const { return !( *this == aOther ); }
};


/**
 * Graphic element extracted from schematic.
 */
struct PLM_SCH_GRAPHIC
{
    wxString              uuid;       ///< Unique identifier
    PLM_SCH_ELEMENT_TYPE  type;       ///< Graphic type
    PLM_SCH_GEOMETRY      geometry;   ///< Position and size
    int32_t               stroke;     ///< Line width

    bool operator==( const PLM_SCH_GRAPHIC& aOther ) const
    {
        return uuid == aOther.uuid && type == aOther.type &&
               geometry == aOther.geometry && stroke == aOther.stroke;
    }

    bool operator!=( const PLM_SCH_GRAPHIC& aOther ) const { return !( *this == aOther ); }
};


/**
 * Sheet instance (sub-schematic) extracted from schematic.
 */
struct PLM_SCH_SHEET
{
    wxString                       uuid;          ///< Unique identifier
    wxString                       name;          ///< Sheet name
    wxString                       filename;      ///< Sheet file name
    PLM_SCH_GEOMETRY               geometry;      ///< Position and size
    std::vector<wxString>          pin_uuids;     ///< UUIDs of sheet pins

    bool operator==( const PLM_SCH_SHEET& aOther ) const
    {
        return uuid == aOther.uuid && name == aOther.name &&
               filename == aOther.filename && geometry == aOther.geometry &&
               pin_uuids == aOther.pin_uuids;
    }

    bool operator!=( const PLM_SCH_SHEET& aOther ) const { return !( *this == aOther ); }
};


/**
 * Single sheet extracted data.
 */
struct PLM_SCH_SHEET_DATA
{
    wxString                         uuid;        ///< Sheet UUID
    wxString                         filename;    ///< Source filename
    std::vector<PLM_SCH_SYMBOL>      symbols;     ///< Symbol instances
    std::vector<PLM_SCH_CONNECTION>  connections; ///< Wires and buses
    std::vector<PLM_SCH_LABEL>       labels;      ///< All label types
    std::vector<PLM_SCH_TEXT>        texts;       ///< Text annotations
    std::vector<PLM_SCH_GRAPHIC>     graphics;    ///< Graphic elements
    std::vector<PLM_SCH_SHEET>       sheets;      ///< Nested sheet references
};


/**
 * Complete schematic data for diff comparison.
 */
struct PLM_SCH_DATA
{
    wxString                            root_filename;   ///< Root schematic file
    wxString                            project_name;    ///< Project name
    std::vector<PLM_SCH_SHEET_DATA>     sheets;          ///< All sheets in schematic
    std::map<wxString, wxString>        title_block;     ///< Title block fields

    size_t GetTotalSymbolCount() const
    {
        size_t count = 0;

        for( const auto& sheet : sheets )
            count += sheet.symbols.size();

        return count;
    }

    size_t GetTotalConnectionCount() const
    {
        size_t count = 0;

        for( const auto& sheet : sheets )
            count += sheet.connections.size();

        return count;
    }
};


/**
 * Change type for diff results.
 */
enum class PLM_SCH_CHANGE_TYPE
{
    ADDED,      ///< Element added in new version
    REMOVED,    ///< Element removed in new version
    MODIFIED,   ///< Element modified between versions
    MOVED       ///< Element moved but otherwise unchanged
};


/**
 * Single change in schematic diff.
 */
struct PLM_SCH_CHANGE
{
    PLM_SCH_CHANGE_TYPE   change_type;     ///< Type of change
    PLM_SCH_ELEMENT_TYPE  element_type;    ///< Type of element changed
    wxString              uuid;            ///< Element UUID
    wxString              sheet_uuid;      ///< Sheet containing element
    wxString              description;     ///< Human-readable description
    wxString              old_value;       ///< Previous value (for modifications)
    wxString              new_value;       ///< New value (for modifications)
};


/**
 * Complete diff result between two schematics.
 */
struct PLM_SCH_DIFF_RESULT
{
    wxString                       base_file;    ///< Base schematic file
    wxString                       new_file;     ///< New schematic file
    std::vector<PLM_SCH_CHANGE>    changes;      ///< All detected changes
    bool                           success;      ///< True if diff completed
    wxString                       error;        ///< Error message if failed

    size_t GetAddedCount() const
    {
        size_t count = 0;

        for( const auto& c : changes )
        {
            if( c.change_type == PLM_SCH_CHANGE_TYPE::ADDED )
                ++count;
        }

        return count;
    }

    size_t GetRemovedCount() const
    {
        size_t count = 0;

        for( const auto& c : changes )
        {
            if( c.change_type == PLM_SCH_CHANGE_TYPE::REMOVED )
                ++count;
        }

        return count;
    }

    size_t GetModifiedCount() const
    {
        size_t count = 0;

        for( const auto& c : changes )
        {
            if( c.change_type == PLM_SCH_CHANGE_TYPE::MODIFIED )
                ++count;
        }

        return count;
    }

    std::vector<PLM_SCH_CHANGE> GetChangesByType( PLM_SCH_ELEMENT_TYPE aType ) const
    {
        std::vector<PLM_SCH_CHANGE> result;

        for( const auto& c : changes )
        {
            if( c.element_type == aType )
                result.push_back( c );
        }

        return result;
    }
};


/**
 * Schematic diff parser for PLM operations.
 *
 * Parses KiCad schematic files and extracts element data for comparison.
 * Supports multi-sheet schematics and handles all standard schematic
 * element types.
 *
 * Usage:
 * @code
 * PLM_SCHEMATIC_DIFF_PARSER parser;
 *
 * PLM_SCH_DATA base = parser.ParseSchematic( "base.kicad_sch" );
 * PLM_SCH_DATA newer = parser.ParseSchematic( "new.kicad_sch" );
 *
 * PLM_SCH_DIFF_RESULT diff = parser.ComputeDiff( base, newer );
 *
 * for( const auto& change : diff.changes )
 *     wxLogMessage( "%s: %s", change.uuid, change.description );
 * @endcode
 */
class KICOMMON_API PLM_SCHEMATIC_DIFF_PARSER
{
public:
    PLM_SCHEMATIC_DIFF_PARSER();
    ~PLM_SCHEMATIC_DIFF_PARSER();

    /**
     * Parse a schematic file and extract element data.
     *
     * @param aFilename Path to schematic file (.kicad_sch).
     * @return Extracted schematic data.
     */
    PLM_SCH_DATA ParseSchematic( const wxString& aFilename );

    /**
     * Parse a schematic file including all hierarchical sheets.
     *
     * @param aFilename Path to root schematic file.
     * @return Extracted schematic data with all sheets.
     */
    PLM_SCH_DATA ParseSchematicHierarchy( const wxString& aFilename );

    /**
     * Compute diff between two schematic data sets.
     *
     * @param aBase Base (older) schematic data.
     * @param aNew New schematic data.
     * @return Diff result with all changes.
     */
    PLM_SCH_DIFF_RESULT ComputeDiff( const PLM_SCH_DATA& aBase, const PLM_SCH_DATA& aNew );

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
     * Parse a single schematic sheet.
     *
     * @param aFilename Sheet file path.
     * @param aContent File content.
     * @return Parsed sheet data.
     */
    PLM_SCH_SHEET_DATA ParseSheet( const wxString& aFilename, const wxString& aContent );

    /**
     * Parse symbol from S-expression.
     *
     * @param aContent Symbol S-expression content.
     * @return Parsed symbol.
     */
    PLM_SCH_SYMBOL ParseSymbol( const wxString& aContent );

    /**
     * Parse wire or bus from S-expression.
     *
     * @param aContent Wire/bus S-expression content.
     * @param aIsBus True if parsing bus.
     * @return Parsed connection.
     */
    PLM_SCH_CONNECTION ParseConnection( const wxString& aContent, bool aIsBus );

    /**
     * Parse label from S-expression.
     *
     * @param aContent Label S-expression content.
     * @param aType Label type.
     * @return Parsed label.
     */
    PLM_SCH_LABEL ParseLabel( const wxString& aContent, PLM_SCH_ELEMENT_TYPE aType );

    /**
     * Compare symbols for changes.
     *
     * @param aBase Base symbol.
     * @param aNew New symbol.
     * @param aSheetUuid Sheet UUID.
     * @param aChanges Output changes.
     */
    void CompareSymbols( const PLM_SCH_SYMBOL& aBase, const PLM_SCH_SYMBOL& aNew,
                         const wxString& aSheetUuid, std::vector<PLM_SCH_CHANGE>& aChanges );

    /**
     * Compare connections for changes.
     *
     * @param aBase Base connection.
     * @param aNew New connection.
     * @param aSheetUuid Sheet UUID.
     * @param aChanges Output changes.
     */
    void CompareConnections( const PLM_SCH_CONNECTION& aBase, const PLM_SCH_CONNECTION& aNew,
                             const wxString& aSheetUuid, std::vector<PLM_SCH_CHANGE>& aChanges );

    wxString m_lastError; ///< Last error message
};


#endif // PLM_DIFF_SCHEMATIC_H
