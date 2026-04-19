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

#ifndef TRACK_WIDTH_H
#define TRACK_WIDTH_H

#include <cmath>
#include <layer_ids.h>

// Forward declarations to avoid circular dependencies
class BOARD;
class BOARD_STACKUP;
class BOARD_STACKUP_ITEM;

/**
 * Utility functions for track width and current capacity calculations
 * based on IPC-2221 formulas.
 */
namespace IPC2221
{
constexpr double UNIT_MIL = 0.0000254; // 1 mil in meters

/**
     * Calculate maximum current capacity for a given track width and copper thickness.
     * 
     * Uses IPC-2221 formula: I = k * dT^0.44 * A^0.725
     * where:
     *   - k = 0.048 for external layers, 0.024 for internal layers
     *   - dT = temperature rise in degrees Celsius
     *   - A = cross-sectional area (width * thickness) in mils²
     * 
     * @param aWidth Track width in meters
     * @param aThickness Copper thickness in meters
     * @param aDeltaT_C Temperature rise in degrees Celsius
     * @param aInternalLayer true for internal layers, false for external layers
     * @return Maximum current in Amperes
     */
inline double CalculateTraceCurrent( double aWidth, double aThickness, double aDeltaT_C, bool aInternalLayer )
{
    double scale = aInternalLayer ? 0.024 : 0.048;

    double width_mil = aWidth / UNIT_MIL;
    double thickness_mil = aThickness / UNIT_MIL;
    double area_mil2 = width_mil * thickness_mil;

    return scale * std::pow( aDeltaT_C, 0.44 ) * std::pow( area_mil2, 0.725 );
}

/**
 * Get copper thickness from board stackup settings.
 * 
 * @param aBoard Pointer to the board
 * @param aLayer Optional layer ID - if specified, gets thickness for that layer,
 *               otherwise returns thickness of first copper layer found
 * @return Copper thickness in meters (defaults to 35e-6 if not found)
 */
double GetCopperThickness( const BOARD* aBoard, PCB_LAYER_ID aLayer = UNDEFINED_LAYER );

} // namespace IPC2221

#endif // TRACK_WIDTH_H
