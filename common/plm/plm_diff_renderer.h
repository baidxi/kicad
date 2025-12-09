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
 * @file plm_diff_renderer.h
 * @brief Visual diff rendering for PLM schematic/PCB comparison.
 */

#ifndef PLM_DIFF_RENDERER_H
#define PLM_DIFF_RENDERER_H

#include <kicommon.h>
#include "plm_diff_schematic.h"
#include "plm_diff_pcb.h"

#include <wx/colour.h>
#include <wx/dc.h>
#include <wx/gdicmn.h>
#include <wx/string.h>

#include <functional>
#include <map>
#include <set>
#include <vector>


/**
 * Visual appearance of diff change.
 */
enum class PLM_DIFF_STYLE
{
    ADDED,      ///< Green for added elements
    REMOVED,    ///< Red for removed elements
    MODIFIED,   ///< Blue for modified elements
    MOVED,      ///< Yellow for moved elements
    UNCHANGED   ///< Gray for unchanged elements
};


/**
 * Rendering layer for diff visualization.
 */
enum class PLM_DIFF_LAYER
{
    BASE,       ///< Base version elements
    CHANGED,    ///< Changed elements overlay
    BOTH        ///< Both layers combined
};


/**
 * A renderable diff element.
 */
struct PLM_DIFF_ELEMENT
{
    wxString         uuid;           ///< Element UUID
    PLM_DIFF_STYLE   style;          ///< Visual style
    wxRect           bounds;         ///< Bounding rectangle
    wxString         label;          ///< Display label
    wxString         tooltip;        ///< Hover tooltip text
    bool             selected;       ///< Currently selected
    bool             visible;        ///< Currently visible

    PLM_DIFF_ELEMENT() : style( PLM_DIFF_STYLE::UNCHANGED ), selected( false ), visible( true ) {}
};


/**
 * Rendering configuration.
 */
struct PLM_DIFF_RENDER_CONFIG
{
    wxColour added_color = wxColour( 0, 200, 0, 128 );      ///< Green
    wxColour removed_color = wxColour( 200, 0, 0, 128 );    ///< Red
    wxColour modified_color = wxColour( 0, 100, 200, 128 ); ///< Blue
    wxColour moved_color = wxColour( 200, 200, 0, 128 );    ///< Yellow
    wxColour unchanged_color = wxColour( 128, 128, 128 );   ///< Gray

    int highlight_width = 3;         ///< Width of highlight border
    int selection_width = 5;         ///< Width of selection border
    bool show_labels = true;         ///< Show element labels
    bool show_unchanged = false;     ///< Show unchanged elements
    double opacity = 0.5;            ///< Overlay opacity

    bool show_added = true;          ///< Show added elements
    bool show_removed = true;        ///< Show removed elements
    bool show_modified = true;       ///< Show modified elements
    bool show_moved = true;          ///< Show moved elements
};


/**
 * Hit test result.
 */
struct PLM_DIFF_HIT_RESULT
{
    bool                 hit;        ///< True if hit an element
    wxString             uuid;       ///< UUID of hit element
    PLM_DIFF_STYLE       style;      ///< Style of hit element
    wxString             description; ///< Description for tooltip

    PLM_DIFF_HIT_RESULT() : hit( false ), style( PLM_DIFF_STYLE::UNCHANGED ) {}
};


/**
 * Navigation item for jumping between changes.
 */
struct PLM_DIFF_NAV_ITEM
{
    wxString        uuid;           ///< Element UUID
    PLM_DIFF_STYLE  style;          ///< Change style
    wxPoint         center;         ///< Center point for navigation
    wxString        description;    ///< Brief description

    PLM_DIFF_NAV_ITEM() : style( PLM_DIFF_STYLE::UNCHANGED ) {}
};


/**
 * Visual diff renderer for PLM comparison views.
 *
 * Renders schematic or PCB diffs with color-coded overlays showing
 * added, removed, and modified elements. Integrates with the diff
 * parsers to provide visual feedback for PLM comparison operations.
 *
 * Usage:
 * @code
 * PLM_DIFF_RENDERER renderer;
 *
 * // Load diff data
 * PLM_SCHEMATIC_DIFF_PARSER parser;
 * PLM_SCH_DATA base = parser.ParseSchematic( "base.kicad_sch" );
 * PLM_SCH_DATA newer = parser.ParseSchematic( "new.kicad_sch" );
 * PLM_SCH_DIFF_RESULT diff = parser.ComputeDiff( base, newer );
 *
 * renderer.LoadSchematicDiff( base, diff );
 *
 * // Render to DC
 * renderer.Render( dc, viewRect );
 *
 * // Navigate changes
 * renderer.NavigateToNext();
 * @endcode
 */
class KICOMMON_API PLM_DIFF_RENDERER
{
public:
    PLM_DIFF_RENDERER();
    ~PLM_DIFF_RENDERER();

    /**
     * Load schematic diff data for rendering.
     *
     * @param aBase Base schematic data.
     * @param aDiff Diff result from comparison.
     */
    void LoadSchematicDiff( const PLM_SCH_DATA& aBase, const PLM_SCH_DIFF_RESULT& aDiff );

    /**
     * Load PCB diff data for rendering.
     *
     * @param aBase Base PCB data.
     * @param aDiff Diff result from comparison.
     */
    void LoadPCBDiff( const PLM_PCB_DATA& aBase, const PLM_PCB_DIFF_RESULT& aDiff );

    /**
     * Clear all loaded diff data.
     */
    void Clear();

    /**
     * Check if renderer has data to display.
     * @return True if diff data is loaded.
     */
    bool HasData() const { return !m_elements.empty(); }

    /**
     * Set rendering configuration.
     * @param aConfig Configuration options.
     */
    void SetConfig( const PLM_DIFF_RENDER_CONFIG& aConfig ) { m_config = aConfig; }

    /**
     * Get current configuration.
     * @return Configuration options.
     */
    const PLM_DIFF_RENDER_CONFIG& GetConfig() const { return m_config; }

    /**
     * Set currently visible layer.
     * @param aLayer Layer to display.
     */
    void SetVisibleLayer( PLM_DIFF_LAYER aLayer );

    /**
     * Get currently visible layer.
     * @return Current layer.
     */
    PLM_DIFF_LAYER GetVisibleLayer() const { return m_visibleLayer; }

    /**
     * Toggle visibility of a change style.
     *
     * @param aStyle Style to toggle.
     * @param aVisible True to show, false to hide.
     */
    void SetStyleVisible( PLM_DIFF_STYLE aStyle, bool aVisible );

    /**
     * Check if a style is visible.
     * @param aStyle Style to check.
     * @return True if visible.
     */
    bool IsStyleVisible( PLM_DIFF_STYLE aStyle ) const;

    /**
     * Render diff overlay to device context.
     *
     * @param aDC Device context to render to.
     * @param aViewRect Visible area in world coordinates.
     */
    void Render( wxDC& aDC, const wxRect& aViewRect );

    /**
     * Render legend showing color meanings.
     *
     * @param aDC Device context.
     * @param aPosition Position for legend.
     */
    void RenderLegend( wxDC& aDC, const wxPoint& aPosition );

    /**
     * Hit test at a point.
     *
     * @param aPoint Point in world coordinates.
     * @return Hit result with element info if hit.
     */
    PLM_DIFF_HIT_RESULT HitTest( const wxPoint& aPoint );

    /**
     * Select an element by UUID.
     *
     * @param aUuid UUID of element to select.
     * @return True if element found and selected.
     */
    bool Select( const wxString& aUuid );

    /**
     * Clear selection.
     */
    void ClearSelection();

    /**
     * Get currently selected UUID.
     * @return Selected UUID or empty if none.
     */
    wxString GetSelectedUuid() const { return m_selectedUuid; }

    /**
     * Navigate to next change.
     * @return True if navigation succeeded.
     */
    bool NavigateToNext();

    /**
     * Navigate to previous change.
     * @return True if navigation succeeded.
     */
    bool NavigateToPrevious();

    /**
     * Navigate to first change.
     * @return True if navigation succeeded.
     */
    bool NavigateToFirst();

    /**
     * Navigate to change by index.
     *
     * @param aIndex Index of change (0-based).
     * @return True if navigation succeeded.
     */
    bool NavigateTo( size_t aIndex );

    /**
     * Get current navigation index.
     * @return Current index or -1 if no selection.
     */
    int GetCurrentIndex() const { return m_navIndex; }

    /**
     * Get total number of navigable changes.
     * @return Count of changes.
     */
    size_t GetChangeCount() const { return m_navItems.size(); }

    /**
     * Get all navigation items.
     * @return List of navigation items.
     */
    const std::vector<PLM_DIFF_NAV_ITEM>& GetNavigationItems() const { return m_navItems; }

    /**
     * Get bounds of current selection.
     * @return Bounding rectangle or empty if no selection.
     */
    wxRect GetSelectionBounds() const;

    /**
     * Get statistics about loaded diff.
     *
     * @param aAdded Output count of added elements.
     * @param aRemoved Output count of removed elements.
     * @param aModified Output count of modified elements.
     * @param aMoved Output count of moved elements.
     */
    void GetStatistics( size_t& aAdded, size_t& aRemoved, size_t& aModified, size_t& aMoved ) const;

    /**
     * Get color for a diff style.
     * @param aStyle Style to get color for.
     * @return Color for rendering.
     */
    wxColour GetStyleColor( PLM_DIFF_STYLE aStyle ) const;

    /**
     * Get human-readable name for diff style.
     * @param aStyle Style to name.
     * @return Display name.
     */
    static wxString GetStyleName( PLM_DIFF_STYLE aStyle );

private:
    /**
     * Add a schematic element to render list.
     *
     * @param aSymbol Symbol data.
     * @param aStyle Visual style.
     * @param aTooltip Tooltip text.
     */
    void AddSchematicElement( const PLM_SCH_SYMBOL& aSymbol, PLM_DIFF_STYLE aStyle,
                               const wxString& aTooltip );

    /**
     * Add a PCB element to render list.
     *
     * @param aFootprint Footprint data.
     * @param aStyle Visual style.
     * @param aTooltip Tooltip text.
     */
    void AddPCBElement( const PLM_PCB_FOOTPRINT& aFootprint, PLM_DIFF_STYLE aStyle,
                         const wxString& aTooltip );

    /**
     * Build navigation list from loaded elements.
     */
    void BuildNavigationList();

    /**
     * Draw a single element.
     *
     * @param aDC Device context.
     * @param aElement Element to draw.
     */
    void DrawElement( wxDC& aDC, const PLM_DIFF_ELEMENT& aElement );

    /**
     * Draw highlight around element.
     *
     * @param aDC Device context.
     * @param aBounds Element bounds.
     * @param aColor Highlight color.
     * @param aWidth Line width.
     */
    void DrawHighlight( wxDC& aDC, const wxRect& aBounds, const wxColour& aColor, int aWidth );

    std::vector<PLM_DIFF_ELEMENT>  m_elements;      ///< All renderable elements
    std::vector<PLM_DIFF_NAV_ITEM> m_navItems;      ///< Navigation items
    PLM_DIFF_RENDER_CONFIG         m_config;        ///< Render configuration
    PLM_DIFF_LAYER                 m_visibleLayer;  ///< Currently visible layer
    wxString                       m_selectedUuid;  ///< Currently selected element
    int                            m_navIndex;      ///< Current navigation index

    std::map<wxString, PLM_SCH_SYMBOL>    m_schSymbols;   ///< Schematic symbols by UUID
    std::map<wxString, PLM_PCB_FOOTPRINT> m_pcbFootprints; ///< PCB footprints by UUID
};


#endif // PLM_DIFF_RENDERER_H
