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

#include "plm_diff_renderer.h"

#include <wx/brush.h>
#include <wx/pen.h>

#include <algorithm>


PLM_DIFF_RENDERER::PLM_DIFF_RENDERER() : m_visibleLayer( PLM_DIFF_LAYER::BOTH ), m_navIndex( -1 )
{
}


PLM_DIFF_RENDERER::~PLM_DIFF_RENDERER()
{
}


void PLM_DIFF_RENDERER::LoadSchematicDiff( const PLM_SCH_DATA& aBase,
                                            const PLM_SCH_DIFF_RESULT& aDiff )
{
    Clear();

    for( const auto& sheet : aBase.sheets )
    {
        for( const auto& symbol : sheet.symbols )
            m_schSymbols[symbol.uuid] = symbol;
    }

    for( const auto& change : aDiff.changes )
    {
        if( change.element_type != PLM_SCH_ELEMENT_TYPE::SYMBOL )
            continue;

        PLM_DIFF_STYLE style;

        switch( change.change_type )
        {
        case PLM_SCH_CHANGE_TYPE::ADDED:   style = PLM_DIFF_STYLE::ADDED; break;
        case PLM_SCH_CHANGE_TYPE::REMOVED: style = PLM_DIFF_STYLE::REMOVED; break;
        case PLM_SCH_CHANGE_TYPE::MODIFIED: style = PLM_DIFF_STYLE::MODIFIED; break;
        case PLM_SCH_CHANGE_TYPE::MOVED:   style = PLM_DIFF_STYLE::MOVED; break;
        default: style = PLM_DIFF_STYLE::UNCHANGED; break;
        }

        auto it = m_schSymbols.find( change.uuid );

        if( it != m_schSymbols.end() )
            AddSchematicElement( it->second, style, change.description );
    }

    BuildNavigationList();
}


void PLM_DIFF_RENDERER::LoadPCBDiff( const PLM_PCB_DATA& aBase, const PLM_PCB_DIFF_RESULT& aDiff )
{
    Clear();

    for( const auto& fp : aBase.footprints )
        m_pcbFootprints[fp.uuid] = fp;

    for( const auto& change : aDiff.changes )
    {
        if( change.element_type != PLM_PCB_ELEMENT_TYPE::FOOTPRINT )
            continue;

        PLM_DIFF_STYLE style;

        switch( change.change_type )
        {
        case PLM_PCB_CHANGE_TYPE::ADDED:   style = PLM_DIFF_STYLE::ADDED; break;
        case PLM_PCB_CHANGE_TYPE::REMOVED: style = PLM_DIFF_STYLE::REMOVED; break;
        case PLM_PCB_CHANGE_TYPE::MODIFIED: style = PLM_DIFF_STYLE::MODIFIED; break;
        case PLM_PCB_CHANGE_TYPE::MOVED:   style = PLM_DIFF_STYLE::MOVED; break;
        default: style = PLM_DIFF_STYLE::UNCHANGED; break;
        }

        auto it = m_pcbFootprints.find( change.uuid );

        if( it != m_pcbFootprints.end() )
            AddPCBElement( it->second, style, change.description );
    }

    BuildNavigationList();
}


void PLM_DIFF_RENDERER::Clear()
{
    m_elements.clear();
    m_navItems.clear();
    m_schSymbols.clear();
    m_pcbFootprints.clear();
    m_selectedUuid.Clear();
    m_navIndex = -1;
}


void PLM_DIFF_RENDERER::SetVisibleLayer( PLM_DIFF_LAYER aLayer )
{
    m_visibleLayer = aLayer;
}


void PLM_DIFF_RENDERER::SetStyleVisible( PLM_DIFF_STYLE aStyle, bool aVisible )
{
    switch( aStyle )
    {
    case PLM_DIFF_STYLE::ADDED:    m_config.show_added = aVisible; break;
    case PLM_DIFF_STYLE::REMOVED:  m_config.show_removed = aVisible; break;
    case PLM_DIFF_STYLE::MODIFIED: m_config.show_modified = aVisible; break;
    case PLM_DIFF_STYLE::MOVED:    m_config.show_moved = aVisible; break;
    case PLM_DIFF_STYLE::UNCHANGED: m_config.show_unchanged = aVisible; break;
    }
}


bool PLM_DIFF_RENDERER::IsStyleVisible( PLM_DIFF_STYLE aStyle ) const
{
    switch( aStyle )
    {
    case PLM_DIFF_STYLE::ADDED:    return m_config.show_added;
    case PLM_DIFF_STYLE::REMOVED:  return m_config.show_removed;
    case PLM_DIFF_STYLE::MODIFIED: return m_config.show_modified;
    case PLM_DIFF_STYLE::MOVED:    return m_config.show_moved;
    case PLM_DIFF_STYLE::UNCHANGED: return m_config.show_unchanged;
    default: return true;
    }
}


void PLM_DIFF_RENDERER::Render( wxDC& aDC, const wxRect& aViewRect )
{
    for( auto& element : m_elements )
    {
        if( !element.visible )
            continue;

        if( !IsStyleVisible( element.style ) )
            continue;

        if( !aViewRect.IsEmpty() && !aViewRect.Intersects( element.bounds ) )
            continue;

        DrawElement( aDC, element );
    }
}


void PLM_DIFF_RENDERER::RenderLegend( wxDC& aDC, const wxPoint& aPosition )
{
    int x = aPosition.x;
    int y = aPosition.y;
    int boxSize = 16;
    int spacing = 4;
    int lineHeight = boxSize + spacing;

    struct LegendItem
    {
        PLM_DIFF_STYLE style;
        wxString       label;
    };

    std::vector<LegendItem> items = { { PLM_DIFF_STYLE::ADDED, wxS( "Added" ) },
                                       { PLM_DIFF_STYLE::REMOVED, wxS( "Removed" ) },
                                       { PLM_DIFF_STYLE::MODIFIED, wxS( "Modified" ) },
                                       { PLM_DIFF_STYLE::MOVED, wxS( "Moved" ) } };

    for( const auto& item : items )
    {
        if( !IsStyleVisible( item.style ) )
            continue;

        wxColour color = GetStyleColor( item.style );
        aDC.SetBrush( wxBrush( color ) );
        aDC.SetPen( wxPen( color.IsOk() ? color : *wxBLACK, 1 ) );
        aDC.DrawRectangle( x, y, boxSize, boxSize );

        aDC.SetTextForeground( *wxBLACK );
        aDC.DrawText( item.label, x + boxSize + spacing, y );

        y += lineHeight;
    }
}


PLM_DIFF_HIT_RESULT PLM_DIFF_RENDERER::HitTest( const wxPoint& aPoint )
{
    PLM_DIFF_HIT_RESULT result;

    for( const auto& element : m_elements )
    {
        if( !element.visible || !IsStyleVisible( element.style ) )
            continue;

        if( element.bounds.Contains( aPoint ) )
        {
            result.hit = true;
            result.uuid = element.uuid;
            result.style = element.style;
            result.description = element.tooltip;
            return result;
        }
    }

    return result;
}


bool PLM_DIFF_RENDERER::Select( const wxString& aUuid )
{
    for( auto& element : m_elements )
    {
        element.selected = ( element.uuid == aUuid );

        if( element.selected )
            m_selectedUuid = aUuid;
    }

    if( m_selectedUuid != aUuid )
    {
        m_selectedUuid.Clear();
        return false;
    }

    for( size_t i = 0; i < m_navItems.size(); ++i )
    {
        if( m_navItems[i].uuid == aUuid )
        {
            m_navIndex = static_cast<int>( i );
            break;
        }
    }

    return true;
}


void PLM_DIFF_RENDERER::ClearSelection()
{
    for( auto& element : m_elements )
        element.selected = false;

    m_selectedUuid.Clear();
}


bool PLM_DIFF_RENDERER::NavigateToNext()
{
    if( m_navItems.empty() )
        return false;

    ++m_navIndex;

    if( m_navIndex >= static_cast<int>( m_navItems.size() ) )
        m_navIndex = 0;

    return Select( m_navItems[m_navIndex].uuid );
}


bool PLM_DIFF_RENDERER::NavigateToPrevious()
{
    if( m_navItems.empty() )
        return false;

    --m_navIndex;

    if( m_navIndex < 0 )
        m_navIndex = static_cast<int>( m_navItems.size() ) - 1;

    return Select( m_navItems[m_navIndex].uuid );
}


bool PLM_DIFF_RENDERER::NavigateToFirst()
{
    if( m_navItems.empty() )
        return false;

    m_navIndex = 0;
    return Select( m_navItems[m_navIndex].uuid );
}


bool PLM_DIFF_RENDERER::NavigateTo( size_t aIndex )
{
    if( aIndex >= m_navItems.size() )
        return false;

    m_navIndex = static_cast<int>( aIndex );
    return Select( m_navItems[m_navIndex].uuid );
}


wxRect PLM_DIFF_RENDERER::GetSelectionBounds() const
{
    for( const auto& element : m_elements )
    {
        if( element.uuid == m_selectedUuid )
            return element.bounds;
    }

    return wxRect();
}


void PLM_DIFF_RENDERER::GetStatistics( size_t& aAdded, size_t& aRemoved, size_t& aModified,
                                         size_t& aMoved ) const
{
    aAdded = 0;
    aRemoved = 0;
    aModified = 0;
    aMoved = 0;

    for( const auto& element : m_elements )
    {
        switch( element.style )
        {
        case PLM_DIFF_STYLE::ADDED:    ++aAdded; break;
        case PLM_DIFF_STYLE::REMOVED:  ++aRemoved; break;
        case PLM_DIFF_STYLE::MODIFIED: ++aModified; break;
        case PLM_DIFF_STYLE::MOVED:    ++aMoved; break;
        default: break;
        }
    }
}


wxColour PLM_DIFF_RENDERER::GetStyleColor( PLM_DIFF_STYLE aStyle ) const
{
    switch( aStyle )
    {
    case PLM_DIFF_STYLE::ADDED:    return m_config.added_color;
    case PLM_DIFF_STYLE::REMOVED:  return m_config.removed_color;
    case PLM_DIFF_STYLE::MODIFIED: return m_config.modified_color;
    case PLM_DIFF_STYLE::MOVED:    return m_config.moved_color;
    case PLM_DIFF_STYLE::UNCHANGED: return m_config.unchanged_color;
    default: return *wxBLACK;
    }
}


wxString PLM_DIFF_RENDERER::GetStyleName( PLM_DIFF_STYLE aStyle )
{
    switch( aStyle )
    {
    case PLM_DIFF_STYLE::ADDED:    return wxS( "Added" );
    case PLM_DIFF_STYLE::REMOVED:  return wxS( "Removed" );
    case PLM_DIFF_STYLE::MODIFIED: return wxS( "Modified" );
    case PLM_DIFF_STYLE::MOVED:    return wxS( "Moved" );
    case PLM_DIFF_STYLE::UNCHANGED: return wxS( "Unchanged" );
    default: return wxS( "Unknown" );
    }
}


void PLM_DIFF_RENDERER::AddSchematicElement( const PLM_SCH_SYMBOL& aSymbol, PLM_DIFF_STYLE aStyle,
                                              const wxString& aTooltip )
{
    PLM_DIFF_ELEMENT element;
    element.uuid = aSymbol.uuid;
    element.style = aStyle;

    int symbolWidth = 100;
    int symbolHeight = 100;
    element.bounds = wxRect( aSymbol.geometry.x - symbolWidth / 2,
                              aSymbol.geometry.y - symbolHeight / 2, symbolWidth, symbolHeight );

    element.label = aSymbol.reference;
    element.tooltip = aTooltip.IsEmpty() ? wxString::Format( wxS( "%s: %s" ), aSymbol.reference,
                                                             GetStyleName( aStyle ) )
                                          : aTooltip;

    m_elements.push_back( element );
}


void PLM_DIFF_RENDERER::AddPCBElement( const PLM_PCB_FOOTPRINT& aFootprint, PLM_DIFF_STYLE aStyle,
                                        const wxString& aTooltip )
{
    PLM_DIFF_ELEMENT element;
    element.uuid = aFootprint.uuid;
    element.style = aStyle;

    int fpWidth = 200;
    int fpHeight = 200;
    element.bounds = wxRect( aFootprint.geometry.x - fpWidth / 2,
                              aFootprint.geometry.y - fpHeight / 2, fpWidth, fpHeight );

    element.label = aFootprint.reference;
    element.tooltip =
            aTooltip.IsEmpty()
                    ? wxString::Format( wxS( "%s: %s" ), aFootprint.reference, GetStyleName( aStyle ) )
                    : aTooltip;

    m_elements.push_back( element );
}


void PLM_DIFF_RENDERER::BuildNavigationList()
{
    m_navItems.clear();

    for( const auto& element : m_elements )
    {
        if( element.style == PLM_DIFF_STYLE::UNCHANGED )
            continue;

        PLM_DIFF_NAV_ITEM item;
        item.uuid = element.uuid;
        item.style = element.style;
        item.center =
                wxPoint( element.bounds.x + element.bounds.width / 2,
                         element.bounds.y + element.bounds.height / 2 );
        item.description = element.label;
        m_navItems.push_back( item );
    }

    std::sort( m_navItems.begin(), m_navItems.end(), []( const PLM_DIFF_NAV_ITEM& a,
                                                          const PLM_DIFF_NAV_ITEM& b )
               { return a.center.y < b.center.y || ( a.center.y == b.center.y
                                                      && a.center.x < b.center.x ); } );
}


void PLM_DIFF_RENDERER::DrawElement( wxDC& aDC, const PLM_DIFF_ELEMENT& aElement )
{
    wxColour color = GetStyleColor( aElement.style );

    if( aElement.selected )
    {
        DrawHighlight( aDC, aElement.bounds, color, m_config.selection_width );
    }
    else
    {
        DrawHighlight( aDC, aElement.bounds, color, m_config.highlight_width );
    }

    if( m_config.show_labels && !aElement.label.IsEmpty() )
    {
        aDC.SetTextForeground( color );
        aDC.DrawText( aElement.label, aElement.bounds.x,
                      aElement.bounds.y - aDC.GetCharHeight() - 2 );
    }
}


void PLM_DIFF_RENDERER::DrawHighlight( wxDC& aDC, const wxRect& aBounds, const wxColour& aColor,
                                        int aWidth )
{
    wxColour fillColor( aColor.Red(), aColor.Green(), aColor.Blue(),
                         static_cast<unsigned char>( 255 * m_config.opacity ) );

    aDC.SetBrush( wxBrush( fillColor ) );
    aDC.SetPen( wxPen( aColor, aWidth ) );
    aDC.DrawRectangle( aBounds );
}
