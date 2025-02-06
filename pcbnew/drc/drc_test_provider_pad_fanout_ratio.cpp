/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2004-2023 KiCad Developers.
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
#include <thread_pool.h>
#include "base_units.h"
#include "core/typeinfo.h"
#include "geometry/shape_line_chain.h"
#include "geometry/shape_poly_set.h"
#include "layer_ids.h"
#include "math/vector2d.h"
#include "widgets/report_severity.h"
#include <pcb_track.h>
#include <drc/drc_engine.h>
#include <drc/drc_item.h>
#include <drc/drc_rule.h>
#include <drc/drc_test_provider.h>
#include <pad.h>
#include <footprint.h>
#include <math/box2.h>
#include <connectivity/connectivity_data.h>


/*
    Pad fanout test. Checks the width of tracks connected to a pad.
    Errors generated:
    - DRCE_PAD_FANOUT_RATIO
    - DRCE_FOOTPRINT_FANOUT_FORCE
    - DRCE_FOOTPRINT_FANOUT_TORQUE
*/

class DRC_TEST_PROVIDER_PAD_FANOUT_RATIO : public DRC_TEST_PROVIDER
{
public:
    DRC_TEST_PROVIDER_PAD_FANOUT_RATIO()
    {}

    virtual ~DRC_TEST_PROVIDER_PAD_FANOUT_RATIO()
    {}

    virtual bool Run() override;

    virtual const wxString GetName() const override
    {
        return wxT( "pad_fanout" );
    };

    virtual const wxString GetDescription() const override
    {
        return wxT( "Tests Fanout width from pads" );
    }
};


bool DRC_TEST_PROVIDER_PAD_FANOUT_RATIO::Run()
{
    if( m_drcEngine->IsErrorLimitExceeded( DRCE_PAD_FANOUT_RATIO ) )
    {
        reportAux( wxT( "Pad fanout violations ignored. Tests not run." ) );
        return true;        // continue with other tests
    }

    if( !m_drcEngine->HasRulesForConstraintType( PAD_FANOUT_RATIO_CONSTRAINT )
        && !m_drcEngine->HasRulesForConstraintType( FOOTPRINT_FANOUT_FORCE_CONSTRAINT )
        && !m_drcEngine->HasRulesForConstraintType( FOOTPRINT_FANOUT_TORQUE_CONSTRAINT ) )
    {
        reportAux( wxT( "No fanout constraints found. Tests not run." ) );
        return true;        // continue with other tests
    }

    if( !reportPhase( _( "Checking pad fanouts..." ) ) )
        return false;       // DRC cancelled

    auto checkPadFanoutRatio =
            [&]( const PAD* pad ) -> bool
            {
                if( m_drcEngine->IsErrorLimitExceeded( DRCE_PAD_FANOUT_RATIO ) )
                {
                    return false;
                }

                if( m_drcEngine->IsCancelled() )
                {
                    return false;
                }


                std::shared_ptr<CONNECTIVITY_DATA> connectivity = m_drcEngine->GetBoard()->GetConnectivity();


                for( BOARD_ITEM* other : connectivity->GetConnectedTracks( pad ) )
                {
                    int actual;
                    VECTOR2I p0;

                    if( other->Type() == PCB_ARC_T )
                    {
                        PCB_ARC* arc = static_cast<PCB_ARC*>( other );

                        actual = arc->GetWidth();
                        p0 = arc->GetStart();
                    }
                    else if( other->Type() == PCB_TRACE_T )
                    {
                        PCB_TRACK* track = static_cast<PCB_TRACK*>( other );

                        actual = track->GetWidth();
                        p0 = ( track->GetStart() + track->GetEnd() ) / 2;
                    }
                    else
                    {
                        continue;
                    }

                    const auto& size_vec = pad->GetSize( other->GetLayer() );
                    int         size = size_vec.x < size_vec.y ? size_vec.x : size_vec.y;

                    auto constraint = m_drcEngine->EvalRules( PAD_FANOUT_RATIO_CONSTRAINT, pad, other,
                                                              other->GetLayer() );

                    int constraintWidth = 0;

                    if( constraint.GetSeverity() != RPT_SEVERITY_IGNORE )
                    {
                        if( constraint.Value().HasMax() && actual*100 > constraint.Value().Max()*size )
                        {
                            constraintWidth = static_cast<double>( constraint.Value().Max() ) / 100.0 * size;

                            std::shared_ptr<DRC_ITEM> drcItem = DRC_ITEM::Create( DRCE_PAD_FANOUT_RATIO );
                            wxString msg;

                            msg = formatMsg( _( "(%s max fanout width %s; actual %s)" ),
                                             constraint.GetName(),
                                             constraintWidth,
                                             actual );

                            drcItem->SetErrorMessage( drcItem->GetErrorText() + wxS( " " ) + msg );
                            drcItem->SetItems( pad , other );
                            drcItem->SetViolatingRule( constraint.GetParentRule() );

                            reportViolation( drcItem, p0, other->GetLayer() );
                        }
                    }
                }

                for( ZONE* zone : m_drcEngine->GetBoard()->Zones() )
                {
                    for( PCB_LAYER_ID layer : pad->GetLayerSet() )
                    {
                        if( ! zone->IsOnLayer( layer ) )
                            continue;

                        int actual;

                        const std::shared_ptr<SHAPE_POLY_SET>& zoneFill = zone->GetFilledPolysList( layer );
                        if( zoneFill->IsEmpty() )
                            continue;

                        auto padOutline = pad->GetEffectivePolygon( layer )->Outline( 0 );
                        auto padBBox = pad->GetBoundingBox();

                        int spoke_half_width = m_drcEngine->EvalRules( THERMAL_SPOKE_WIDTH_CONSTRAINT, pad, zone, layer ).m_Value.Min() / 2;
                        
                        for( int jj = 0; jj < zoneFill->OutlineCount(); ++jj )
                        {
                            std::vector<SHAPE_LINE_CHAIN::INTERSECTION> intersections;

                            zoneFill->Outline( jj ).Intersect( padOutline, intersections, true, &padBBox );

                            if( intersections.size() < 2 )
                                continue;

                            actual = spoke_half_width * intersections.size();
                        }


                        const auto& size_vec = pad->GetSize( layer );
                        int         size = size_vec.x < size_vec.y ? size_vec.x : size_vec.y;

                        auto constraint = m_drcEngine->EvalRules( PAD_FANOUT_RATIO_CONSTRAINT, pad, zone,
                                                                  layer );

                        int constraintWidth = 0;

                        if( constraint.GetSeverity() != RPT_SEVERITY_IGNORE )
                        {
                            if( constraint.Value().HasMax() && actual*100 > constraint.Value().Max()*size )
                            {
                                constraintWidth = static_cast<double>( constraint.Value().Max() ) / 100.0 * size;

                                std::shared_ptr<DRC_ITEM> drcItem = DRC_ITEM::Create( DRCE_PAD_FANOUT_RATIO );
                                wxString msg;

                                msg = formatMsg( _( "(%s max fanout width %s; actual %s)" ),
                                                 constraint.GetName(),
                                                 constraintWidth,
                                                 actual );

                                drcItem->SetErrorMessage( drcItem->GetErrorText() + wxS( " " ) + msg );
                                drcItem->SetItems( pad , zone );
                                drcItem->SetViolatingRule( constraint.GetParentRule() );

                                reportViolation( drcItem, pad->GetPosition(), layer );
                            }
                        }
                        
                    }
                }

                return true;
            };

    auto checkFootprintSymmetry = [&]( const FOOTPRINT* footprint ) -> bool
    {
        if(
            m_drcEngine->IsErrorLimitExceeded( DRCE_FOOTPRINT_FANOUT_FORCE ) &&
            m_drcEngine->IsErrorLimitExceeded( DRCE_FOOTPRINT_FANOUT_TORQUE ) )
        {
            return false;
        }

        if( m_drcEngine->IsCancelled() )
        {
            return false;
        }
        
        // pad fanout symmetry is easiest to be calculated using torque and net force.
        // Both need to be net zero for the fanout to be symmetric.
        // Force is calculated using connected width.

        double torque { 0 };
        VECTOR2D force { 0, 0 };

        for( auto pad : footprint->Pads() )
        {
            std::shared_ptr<CONNECTIVITY_DATA> connectivity = m_drcEngine->GetBoard()->GetConnectivity();

            for( BOARD_ITEM* other : connectivity->GetConnectedTracks( pad ) )
            {
                if( other->Type() != PCB_TRACE_T )
                    continue;

                PCB_TRACK* track = static_cast<PCB_TRACK*>( other );

                const auto shape = pad->GetEffectiveShape( track->GetLayer() );

                VECTOR2D direction = VECTOR2D( track->GetEnd() - track->GetStart() );
                VECTOR2D p0 = track->GetStart();

                if( !shape->PointInside( track->GetStart() ) )
                {
                    if( shape->PointInside( track->GetEnd() ) )
                    {
                        p0 = track->GetEnd();
                        direction = VECTOR2D( track->GetStart() - track->GetEnd() );
                    }
                }

                SHAPE_LINE_CHAIN::INTERSECTIONS intersections;

                auto outline = pad->GetEffectivePolygon( track->GetLayer() )->Outline( 0 );

                if( outline.Intersect( SEG( track->GetStart(), track->GetEnd() ), intersections ) )
                    p0 = intersections[0].p;

                p0 = p0 - footprint->GetCenter();

                direction = direction.Resize( pcbIUScale.IUTomm( track->GetWidth() ) );

                force += direction;
                torque += pcbIUScale.IUTomm( p0.x ) * direction.y - pcbIUScale.IUTomm( p0.y ) * direction.x;
            }

            for( ZONE* zone : m_drcEngine->GetBoard()->Zones() )
            {
                for( PCB_LAYER_ID layer : pad->GetLayerSet() )
                {
                    if( ! zone->IsOnLayer( layer ) )
                        continue;
                    // Only external copper layers are interesting for this test, since it is ultimately about solderability.

                    const std::shared_ptr<SHAPE_POLY_SET>& zoneFill = zone->GetFilledPolysList( layer );
                    if( zoneFill->IsEmpty() )
                        continue;

                    auto padOutline = pad->GetEffectivePolygon( layer )->Outline( 0 );
                    auto padBBox = pad->GetBoundingBox();

                    int spoke_half_width = m_drcEngine->EvalRules( THERMAL_SPOKE_WIDTH_CONSTRAINT, pad, zone, layer ).m_Value.Min() / 2;

                    for( int jj = 0; jj < zoneFill->OutlineCount(); ++jj )
                    {
                        std::vector<SHAPE_LINE_CHAIN::INTERSECTION> intersections;

                        zoneFill->Outline( jj ).Intersect( padOutline, intersections, true, &padBBox );

                        if( intersections.size() < 2 )
                            continue;

                        for( auto intersection : intersections )
                        {
                            SEG seg = zoneFill->Outline( jj ).Segment( intersection.index_our );
                            VECTOR2D direction;
                            if( padOutline.PointInside( seg.A ) )
                                direction = seg.B - intersection.p;
                            else
                                direction = seg.A - intersection.p;

                            direction = direction.Resize( pcbIUScale.IUTomm( spoke_half_width ) );

                            VECTOR2D p0 = intersection.p - footprint->GetCenter();

                            force += direction;
                            torque += pcbIUScale.IUTomm( p0.x ) * direction.y - pcbIUScale.IUTomm( p0.y ) * direction.x;
                        }
                    }
                }
            }
        }

        int actual_torque = pcbIUScale.mmToIU( std::sqrt( std::abs( torque ) ) );
        int actual_force = pcbIUScale.mmToIU( force.EuclideanNorm() );

        auto force_constraint = m_drcEngine->EvalRules( FOOTPRINT_FANOUT_FORCE_CONSTRAINT, footprint, nullptr,
                                                  footprint->GetLayer() );

        auto torque_constraint = m_drcEngine->EvalRules( FOOTPRINT_FANOUT_TORQUE_CONSTRAINT, footprint, nullptr,
                                                  footprint->GetLayer() );

        if(
            force_constraint.m_Value.HasMax() &&
            force_constraint.GetSeverity() != RPT_SEVERITY_IGNORE &&
            !m_drcEngine->IsErrorLimitExceeded( DRCE_FOOTPRINT_FANOUT_FORCE ) )
        {
            if( actual_force > force_constraint.m_Value.Max() )
            {
                std::shared_ptr<DRC_ITEM> drcItem = DRC_ITEM::Create( DRCE_FOOTPRINT_FANOUT_FORCE );
                wxString msg;

                msg = formatMsg( _( "(%s footprint fanout force exceeded. %s; actual %s)" ),
                                 force_constraint.GetName(),
                                 force_constraint.m_Value.Max(),
                                 actual_force );

                drcItem->SetErrorMessage( drcItem->GetErrorText() + wxS( " " ) + msg );
                drcItem->SetItems( footprint , nullptr );
                drcItem->SetViolatingRule( force_constraint.GetParentRule() );

                reportViolation( drcItem, footprint->GetPosition(), footprint->GetLayer() );
            }
        }
        if(
            torque_constraint.m_Value.HasMax() &&
            torque_constraint.GetSeverity() != RPT_SEVERITY_IGNORE &&
            !m_drcEngine->IsErrorLimitExceeded( DRCE_FOOTPRINT_FANOUT_TORQUE ) )
        {
            if( actual_torque > torque_constraint.m_Value.Max() )
            {
                std::shared_ptr<DRC_ITEM> drcItem = DRC_ITEM::Create( DRCE_FOOTPRINT_FANOUT_TORQUE );
                wxString msg;

                msg = formatMsg( _( "(%s footprint fanout torque exceeded. %s; actual %s)" ),
                                 torque_constraint.GetName(),
                                 torque_constraint.m_Value.Max(),
                                 actual_torque );

                drcItem->SetErrorMessage( drcItem->GetErrorText() + wxS( " " ) + msg );
                drcItem->SetItems( footprint , nullptr );
                drcItem->SetViolatingRule( torque_constraint.GetParentRule() );

                reportViolation( drcItem, footprint->GetPosition(), footprint->GetLayer() );
            }
        }

        return true;
    };

    const int progressDelta = 250;
    int       ii = 0;

    thread_pool&                   tp = GetKiCadThreadPool();
    std::vector<std::future<bool>> returns;

    returns.reserve( m_drcEngine->GetBoard()->GetPads().size() + m_drcEngine->GetBoard()->Footprints().size() );

    for( PAD* item : m_drcEngine->GetBoard()->GetPads() )
    {
        returns.emplace_back( tp.submit( checkPadFanoutRatio, item ) );
    }

    for( FOOTPRINT* item : m_drcEngine->GetBoard()->Footprints() )
    {
        returns.emplace_back( tp.submit( checkFootprintSymmetry, item ) );
    }

    for( std::future<bool>& ret : returns )
    {
        std::future_status status = ret.wait_for( std::chrono::milliseconds( 250 ) );

        while( status != std::future_status::ready )
        {
            reportProgress( ii++, m_drcEngine->GetBoard()->Tracks().size(), progressDelta );
            status = ret.wait_for( std::chrono::milliseconds( 250 ) );
        }
    }

    reportRuleStatistics();

    return !m_drcEngine->IsCancelled();
}


namespace detail
{
static DRC_REGISTER_TEST_PROVIDER<DRC_TEST_PROVIDER_PAD_FANOUT_RATIO> dummy;
}
