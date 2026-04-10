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


#include <cmath>

#include <board.h>
#include <board_design_settings.h>
#include <board_stackup_manager/board_stackup.h>
#include <footprint.h>
#include <pad.h>
#include <pcb_track.h>
#include <zone.h>

#include <drc/drc_engine.h>
#include <drc/drc_item.h>
#include <drc/drc_rule.h>
#include <drc/drc_rtree.h>
#include <drc/drc_test_provider.h>

/*
  Z-clearance (3D dielectric isolation) DRC test.

  Checks that the 3D Pythagorean distance between copper items on different layers meets
  the minimum z_clearance constraint.

  Errors generated:
  - DRCE_Z_CLEARANCE
 */

class DRC_TEST_PROVIDER_Z_CLEARANCE : public DRC_TEST_PROVIDER
{
public:
    DRC_TEST_PROVIDER_Z_CLEARANCE() :
            DRC_TEST_PROVIDER(),
            m_drcEpsilon( 0 )
    {
    }
    virtual ~DRC_TEST_PROVIDER_Z_CLEARANCE() = default;

    virtual bool Run() override;

    virtual const wxString GetName() const override { return wxT( "z_clearance" ); }

private:
    void testLayerPair( PCB_LAYER_ID aLayerA, PCB_LAYER_ID aLayerB, int aDielectricDist, int aMaxXYSearch );

    int sub_e( int aClearance ) { return std::max( 0, aClearance - m_drcEpsilon ); }

    int m_drcEpsilon;
};


bool DRC_TEST_PROVIDER_Z_CLEARANCE::Run()
{
    m_board = m_drcEngine->GetBoard();

    if( !m_drcEngine->HasRulesForConstraintType( Z_CLEARANCE_CONSTRAINT ) )
    {
        REPORT_AUX( wxT( "No z_clearance rules found. Tests not run." ) );
        return true;
    }

    DRC_CONSTRAINT worstConstraint;

    if( !m_drcEngine->QueryWorstConstraint( Z_CLEARANCE_CONSTRAINT, worstConstraint, true ) )
    {
        if( !m_drcEngine->QueryWorstConstraint( Z_CLEARANCE_CONSTRAINT, worstConstraint ) )
            return true;
    }

    int maxRequiredZ = worstConstraint.GetValue().Min();

    if( maxRequiredZ <= 0 )
        return true;

    m_drcEpsilon = m_board->GetDesignSettings().GetDRCEpsilon();

    if( !reportPhase( _( "Checking z-clearance between copper layers..." ) ) )
        return false;

    BOARD_STACKUP& stackup = m_board->GetDesignSettings().GetStackupDescriptor();

    std::vector<PCB_LAYER_ID> copperLayers;

    for( BOARD_STACKUP_ITEM* item : stackup.GetList() )
    {
        if( item->GetType() == BS_ITEM_TYPE_COPPER )
            copperLayers.push_back( item->GetBrdLayerId() );
    }

    if( copperLayers.size() < 2 )
        return true;

    for( int i = 0; i < (int) copperLayers.size(); ++i )
    {
        for( int j = i + 1; j < (int) copperLayers.size(); ++j )
        {
            if( m_drcEngine->IsCancelled() )
                return false;

            PCB_LAYER_ID layerA = copperLayers[i];
            PCB_LAYER_ID layerB = copperLayers[j];

            int dielectricDist = stackup.GetDielectricDistance( layerA, layerB );

            if( dielectricDist >= maxRequiredZ )
                continue;

            // Maximum XY distance that could still produce a 3D violation:
            //   maxXY = sqrt( maxRequiredZ^2 - dielectricDist^2 )
            double maxXYd =
                    std::sqrt( (double) maxRequiredZ * maxRequiredZ - (double) dielectricDist * dielectricDist );
            int maxXYSearch = (int) std::ceil( maxXYd );

            testLayerPair( layerA, layerB, dielectricDist, maxXYSearch );
        }
    }

    return true;
}


void DRC_TEST_PROVIDER_Z_CLEARANCE::testLayerPair( PCB_LAYER_ID aLayerA, PCB_LAYER_ID aLayerB, int aDielectricDist,
                                                   int aMaxXYSearch )
{
    const int delta = 50; // This is the number of tests between 2 calls to the progress bar

    size_t count = 0;
    size_t ii = 0;

    static const std::vector<KICAD_T> itemTypes = {
        PCB_TRACE_T, PCB_ARC_T, PCB_VIA_T, PCB_PAD_T, PCB_ZONE_T,
    };

    // Count items on layerA for progress reporting.
    forEachGeometryItem( itemTypes, LSET().set( aLayerA ),
                         [&]( BOARD_ITEM* item ) -> bool
                         {
                             count++;
                             return true;
                         } );

    forEachGeometryItem(
            itemTypes, LSET().set( aLayerA ),
            [&]( BOARD_ITEM* itemA ) -> bool
            {
                if( m_drcEngine->IsErrorLimitExceeded( DRCE_Z_CLEARANCE ) )
                    return false;

                if( m_drcEngine->IsCancelled() )
                    return false;

                ii++;

                if( ii % delta == 0 )
                    reportProgress( ii, count );

                BOARD_CONNECTED_ITEM* connItemA = dynamic_cast<BOARD_CONNECTED_ITEM*>( itemA );

                if( !connItemA )
                    return true;

                // Skip rule areas
                if( itemA->Type() == PCB_ZONE_T && static_cast<ZONE*>( itemA )->GetIsRuleArea() )
                    return true;

                std::shared_ptr<SHAPE> shapeA = itemA->GetEffectiveShape( aLayerA );

                m_board->m_CopperItemRTreeCache->QueryColliding(
                        itemA, aLayerA, aLayerB,
                        // Filter:
                        [&]( BOARD_ITEM* other ) -> bool
                        {
                            if( m_drcEngine->IsErrorLimitExceeded( DRCE_Z_CLEARANCE ) )
                                return false;

                            BOARD_CONNECTED_ITEM* connOther = dynamic_cast<BOARD_CONNECTED_ITEM*>( other );

                            if( !connOther )
                                return false;

                            // Skip same-net items.
                            int netA = connItemA->GetNetCode();
                            int netB = connOther->GetNetCode();

                            if( netA > 0 && netA == netB )
                                return false;

                            // Use UUID ordering to avoid testing each pair twice.
                            if( itemA->m_Uuid > other->m_Uuid )
                                return false;

                            // Skip rule areas
                            if( other->Type() == PCB_ZONE_T && static_cast<ZONE*>( other )->GetIsRuleArea() )
                            {
                                return false;
                            }

                            return true;
                        },
                        // Visitor:
                        [&]( BOARD_ITEM* other ) -> bool
                        {
                            if( m_drcEngine->IsCancelled() )
                                return false;

                            DRC_CONSTRAINT constraint =
                                    m_drcEngine->EvalRules( Z_CLEARANCE_CONSTRAINT, itemA, other, aLayerA );

                            if( constraint.GetSeverity() == RPT_SEVERITY_IGNORE )
                                return true;

                            int minZ = constraint.GetValue().Min();

                            if( minZ <= 0 || aDielectricDist >= minZ )
                                return true;

                            // Compute the maximum XY distance that would violate for this pair:
                            //   maxXY = sqrt( minZ^2 - zDist^2 )
                            double maxXYd =
                                    std::sqrt( (double) minZ * minZ - (double) aDielectricDist * aDielectricDist );
                            int maxXY = (int) std::ceil( maxXYd );

                            std::shared_ptr<SHAPE> shapeB = other->GetEffectiveShape( aLayerB );

                            int      xyActual = 0;
                            VECTOR2I pos;

                            if( shapeA->Collide( shapeB.get(), sub_e( maxXY ), &xyActual, &pos ) )
                            {
                                // Compute actual 3D distance.
                                double actual3Dd = std::sqrt( (double) xyActual * xyActual
                                                              + (double) aDielectricDist * aDielectricDist );
                                int    actual3D = (int) actual3Dd;

                                if( actual3D < minZ )
                                {
                                    std::shared_ptr<DRC_ITEM> drcItem = DRC_ITEM::Create( DRCE_Z_CLEARANCE );

                                    drcItem->SetErrorDetail( formatMsg( _( "(%s z-clearance %s; actual %s)" ),
                                                                        constraint.GetName(), minZ, actual3D ) );
                                    drcItem->SetItems( itemA, other );
                                    drcItem->SetViolatingRule( constraint.GetParentRule() );

                                    reportViolation( drcItem, pos, aLayerA );
                                }
                            }

                            return !m_drcEngine->IsCancelled();
                        },
                        aMaxXYSearch );

                return true;
            } );
}


static DRC_REGISTER_TEST_PROVIDER<DRC_TEST_PROVIDER_Z_CLEARANCE> dummy;
