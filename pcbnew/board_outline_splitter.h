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

#ifndef BOARD_OUTLINE_SPLITTER_H
#define BOARD_OUTLINE_SPLITTER_H

#include <geometry/shape_poly_set.h>
#include <map>
#include <vector>

#include <wx/string.h>

class BOARD;
class PCB_SHAPE;

/**
 * Result of splitting the Edge.Cuts layer into independent closed board outlines so
 * that each outline can be exported as its own set of fabrication files.
 */
struct BOARD_OUTLINE_SPLIT
{
    /// One polygon set per independent top-level outline.
    std::vector<SHAPE_POLY_SET>     m_regions;

    /// Edge.Cuts graphic item -> index of the outline it belongs to.
    std::map<const PCB_SHAPE*, int> m_shapeToRegion;

    /// Non-empty when the split failed.
    wxString                        m_error;

    size_t OutlineCount() const { return m_regions.size(); }
};

/**
 * Extract the independent closed outlines of the Edge.Cuts layer.
 *
 * Open chains, self-intersecting contours or items that cannot be attached to any
 * outline are reported through #BOARD_OUTLINE_SPLIT::m_error.
 *
 * @return true when the board could be split into outlines (m_error empty).
 */
bool SplitBoardOutlines( BOARD* aBoard, BOARD_OUTLINE_SPLIT& aResult );

/**
 * Outline index containing @a aPoint (with a small tolerance), or -1 when the point is
 * not inside any outline.
 */
int  OutlineIndexForPoint( const VECTOR2I& aPoint, const BOARD_OUTLINE_SPLIT& aSplit );

/**
 * Plot-time filter: return true when @a aItem belongs to outline @a aIndex.  Edge.Cuts
 * graphics are matched through @a aShapeMap (their anchor sits on the contour itself, so
 * a containment test would be unreliable); every other item is matched by its anchor.
 */
bool PassesOutlineFilter( const BOARD_ITEM& aItem, const SHAPE_POLY_SET& aRegion, int aIndex,
                          const std::map<const PCB_SHAPE*, int>& aShapeMap );

#endif // BOARD_OUTLINE_SPLITTER_H
