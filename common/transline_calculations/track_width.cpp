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

#include <transline_calculations/track_width.h>
#include <board.h>
#include <board_design_settings.h>
#include <board_stackup_manager/board_stackup.h>


namespace IPC2221
{

double GetCopperThickness( const BOARD* aBoard, PCB_LAYER_ID aLayer )
{
    constexpr double DEFAULT_COPPER_THICKNESS = 35e-6; // 35 microns (1 oz) in meters

    if( !aBoard )
        return DEFAULT_COPPER_THICKNESS;

    const BOARD_STACKUP& stackup = aBoard->GetDesignSettings().GetStackupDescriptor();

    for( BOARD_STACKUP_ITEM* item : stackup.GetList() )
    {
        if( item->GetType() != BS_ITEM_TYPE_COPPER )
            continue;

        // If specific layer requested, match it
        if( aLayer != UNDEFINED_LAYER )
        {
            if( item->GetBrdLayerId() == aLayer )
            {
                // Convert from internal units (nm) to meters
                return item->GetThickness( 0 ) * 1e-9;
            }
        }
        else
        {
            // Return first copper layer thickness found
            return item->GetThickness( 0 ) * 1e-9;
        }
    }

    return DEFAULT_COPPER_THICKNESS;
}

} // namespace IPC2221
