/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2025 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gerber_to_polyset.h"
#include "gerber_file_image.h"
#include "gerber_draw_item.h"


SHAPE_POLY_SET ConvertGerberToPolySet( GERBER_FILE_IMAGE* aImage, int aTolerance )
{
    SHAPE_POLY_SET mergedPolygons;

    if( !aImage )
        return mergedPolygons;

    // Accumulate positive and negative items separately, then perform a single boolean
    // pass for each. This is O(N log N) vs O(N²) for per-item BooleanAdd.
    SHAPE_POLY_SET positivePolygons;
    SHAPE_POLY_SET negativePolygons;

    for( GERBER_DRAW_ITEM* item : aImage->GetItems() )
    {
        SHAPE_POLY_SET itemPoly;

        if( item->m_ShapeAsPolygon.OutlineCount() > 0 )
        {
            itemPoly = item->m_ShapeAsPolygon;
        }
        else if( item->m_ShapeType == GBR_SEGMENT || item->m_ShapeType == GBR_ARC )
        {
            item->ConvertSegmentToPolygon( &itemPoly );
        }
        else if( item->m_Flashed )
        {
            D_CODE* dcode = item->GetDcodeDescr();

            if( dcode )
            {
                dcode->ConvertShapeToPolygon( item );
                itemPoly = item->m_ShapeAsPolygon;
            }
        }

        if( itemPoly.OutlineCount() == 0 )
            continue;

        // Apply AB transformation to get absolute coordinates
        SHAPE_POLY_SET& dest = item->GetLayerPolarity() ? negativePolygons : positivePolygons;

        for( int i = 0; i < itemPoly.OutlineCount(); i++ )
        {
            const SHAPE_LINE_CHAIN& outline = itemPoly.COutline( i );
            dest.NewOutline();

            for( int j = 0; j < outline.PointCount(); j++ )
                dest.Append( item->GetABPosition( outline.CPoint( j ) ) );
        }
    }

    // Single-pass union of all positive items
    positivePolygons.Simplify();

    if( negativePolygons.OutlineCount() > 0 )
    {
        negativePolygons.Simplify();
        positivePolygons.BooleanSubtract( negativePolygons );
    }

    mergedPolygons = std::move( positivePolygons );

    if( aTolerance > 0 )
        mergedPolygons.Inflate( aTolerance, CORNER_STRATEGY::ROUND_ALL_CORNERS, 16 );

    return mergedPolygons;
}
