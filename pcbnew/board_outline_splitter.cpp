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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <board_outline_splitter.h>

#include <board.h>
#include <board_design_settings.h>
#include <board_item.h>
#include <convert_shape_list_to_polygon.h>
#include <pcb_shape.h>

#include <wx/log.h>


static const wxChar* traceBoardOutlineSplitter = wxT( "KICAD_BOARD_OUTLINE_SPLIT" );


bool SplitBoardOutlines( BOARD* aBoard, BOARD_OUTLINE_SPLIT& aResult )
{
    aResult = BOARD_OUTLINE_SPLIT();

    std::vector<PCB_SHAPE*> shapes;

    for( BOARD_ITEM* item : aBoard->Drawings() )
    {
        if( item->Type() == PCB_SHAPE_T && item->GetLayer() == Edge_Cuts )
            shapes.push_back( static_cast<PCB_SHAPE*>( item ) );
    }

    if( shapes.empty() )
    {
        aResult.m_error = _( "No items on the Edge.Cuts layer." );
        return false;
    }

    SHAPE_POLY_SET combined;

    OUTLINE_ERROR_HANDLER errorHandler =
            [&]( const wxString& aMsg, BOARD_ITEM* aItemA, BOARD_ITEM* aItemB, const VECTOR2I& aPt )
            {
                if( aResult.m_error.IsEmpty() )
                    aResult.m_error = aMsg;
            };

    // GetBoardPolygonOutlines() allows disjoint outlines and promotes nested contours to
    // top-level outlines, so each independent board lands as its own outline.
    if( !aBoard->GetBoardPolygonOutlines( combined, /* aInferOutlineIfNecessary */ false, &errorHandler ) )
    {
        if( aResult.m_error.IsEmpty() )
            aResult.m_error = _( "Edge.Cuts outlines could not be built (open or crossing contours)." );

        wxLogTrace( traceBoardOutlineSplitter, wxS( "SplitBoardOutlines: convert failed" ) );
        return false;
    }

    int outlineCount = combined.OutlineCount();

    if( outlineCount < 1 )
    {
        aResult.m_error = _( "No closed board outline found on the Edge.Cuts layer." );
        return false;
    }

    for( int ii = 0; ii < outlineCount; ++ii )
    {
        SHAPE_POLY_SET region;
        region.AddOutline( combined.COutline( ii ) );
        aResult.m_regions.push_back( region );
    }

    // Attach every edge item to the outline containing its center point.  Independent
    // outlines never overlap, so each item belongs to exactly one of them.
    for( PCB_SHAPE* shape : shapes )
    {
        VECTOR2I center = shape->GetCenter();
        int      regionIdx = -1;

        for( int ii = 0; ii < outlineCount; ++ii )
        {
            // A small tolerance keeps items sitting exactly on the contour attachable
            if( aResult.m_regions[ii].Contains( center, -1, 0 ) )
            {
                regionIdx = ii;
                break;
            }
        }

        if( regionIdx < 0 )
        {
            aResult.m_error = _( "An Edge.Cuts item could not be assigned to any board outline." );
            wxLogTrace( traceBoardOutlineSplitter, wxS( "SplitBoardOutlines: unassigned edge item" ) );
            return false;
        }

        aResult.m_shapeToRegion[shape] = regionIdx;
    }

    wxLogTrace( traceBoardOutlineSplitter, wxS( "SplitBoardOutlines: %d outlines" ), outlineCount );
    return true;
}


int OutlineIndexForPoint( const VECTOR2I& aPoint, const BOARD_OUTLINE_SPLIT& aSplit )
{
    for( size_t ii = 0; ii < aSplit.m_regions.size(); ++ii )
    {
        if( aSplit.m_regions[ii].Contains( aPoint, -1, 0 ) )
            return static_cast<int>( ii );
    }

    return -1;
}


bool PassesOutlineFilter( const BOARD_ITEM& aItem, const SHAPE_POLY_SET& aRegion, int aIndex,
                          const std::map<const PCB_SHAPE*, int>& aShapeMap )
{
    // Edge.Cuts graphics are matched through their owning outline: their anchor sits on
    // the contour itself, so the anchor test would be unreliable.
    if( aItem.GetLayer() == Edge_Cuts )
    {
        auto it = aShapeMap.find( static_cast<const PCB_SHAPE*>( &aItem ) );

        if( it != aShapeMap.end() )
            return it->second == aIndex;

        return true;    // Unmapped edge item: let it through rather than lose it
    }

    return aRegion.Contains( aItem.GetPosition(), -1, 0 );
}
