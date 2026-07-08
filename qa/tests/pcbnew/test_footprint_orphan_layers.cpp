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

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <footprint.h>
#include <layer_utils.h>
#include <lset.h>
#include <pad.h>
#include <pcb_shape.h>

/**
 * Regression test for https://gitlab.com/kicad/code/kicad/-/work_items/24219
 *
 * A PTH and NPTH pads must not cause an orphaned layers error when using a restrictive custom-stackup.
 */
BOOST_AUTO_TEST_SUITE( FootprintOrphanLayers )


BOOST_AUTO_TEST_CASE( OrphanLayerCheckThrouhHolePads )
{
    LSET restrictive = LSET{ F_Cu, B_Cu };
    
    // NPTH pads and PTH pads with NORMAL padstack mode must not report orphan layers.
    {
        FOOTPRINT footprint( nullptr );

        PAD* pthPad = new PAD( &footprint );
        pthPad->SetAttribute( PAD_ATTRIB::PTH );
        footprint.Add( pthPad );
        PAD* npthPad = new PAD( &footprint );
        npthPad->SetAttribute( PAD_ATTRIB::NPTH );
        footprint.Add( npthPad );

        LSET orphanedLayers = LAYER_UTILS::GetOrphanedFootprintLayers( footprint, restrictive );

        BOOST_TEST( orphanedLayers.none() );
    }

    // TODO: FInish tests
    /*
    {
        FOOTPRINT footprint( nullptr );
        PAD*      pthPad = new PAD( &footprint );
        pthPad->SetAttribute( PAD_ATTRIB::PTH );
        pthPad->Padstack().SetMode( PADSTACK::MODE::CUSTOM );
        footprint.Add( pthPad );

        LSET orphanedLayers = LAYER_UTILS::GetOrphanedFootprintLayers( footprint, restrictive );
    }
    */
}

BOOST_AUTO_TEST_SUITE_END()
