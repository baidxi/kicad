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

#include "core/typeinfo.h"
#include <numbers>
#include <pcb_track.h>
#include <drc/drc_engine.h>
#include <drc/drc_item.h>
#include <drc/drc_rule.h>
#include <drc/drc_test_provider.h>
#include <pad.h>
#include <math/box2.h>
#include <connectivity/connectivity_data.h>


/*
    Pad fanout test. Checks the width of tracks connected to a pad.
    Errors generated:
    - DRCE_PAD_FANOUT_RATIO
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

    if( !m_drcEngine->HasRulesForConstraintType( PAD_FANOUT_RATIO_CONSTRAINT ) )
    {
        reportAux( wxT( "No pad fanout constraints found. Tests not run." ) );
        return true;        // continue with other tests
    }

    if( !reportPhase( _( "Checking pad fanouts..." ) ) )
        return false;       // DRC cancelled

    auto checkPadFanoutWidth =
            [&]( PAD* pad ) -> bool
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
                    int      actual;
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

                            reportViolation( drcItem, p0, pad->GetLayer() );
                        }
                    }
                }

                return true;
            };

    const int progressDelta = 250;
    int       ii = 0;

    for( PAD* pad : m_drcEngine->GetBoard()->GetPads() )
    {
        if( !reportProgress( ii++, m_drcEngine->GetBoard()->Tracks().size(), progressDelta ) )
            break;

        if( !checkPadFanoutWidth( pad ) )
            break;
    }

    reportRuleStatistics();

    return !m_drcEngine->IsCancelled();
}


namespace detail
{
static DRC_REGISTER_TEST_PROVIDER<DRC_TEST_PROVIDER_PAD_FANOUT_RATIO> dummy;
}
