/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers.
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

#include <thread_pool.h>
#include <numbers>
#include <pcb_field.h>
#include <pcb_text.h>
#include <pcb_textbox.h>
#include <font/font.h>
#include <footprint.h>
#include <drc/drc_engine.h>
#include <drc/drc_item.h>
#include <drc/drc_rule.h>
#include <drc/drc_test_provider.h>

/*
    Graphical & text line width test. Checks the line width constraints.
    Errors generated:
    - DRCE_LINE_WIDTH
*/

class DRC_TEST_PROVIDER_LINE_WIDTH : public DRC_TEST_PROVIDER
{
public:
    DRC_TEST_PROVIDER_LINE_WIDTH() {}

    virtual ~DRC_TEST_PROVIDER_LINE_WIDTH() {}

    virtual bool Run() override;

    virtual const wxString GetName() const override { return wxT( "line_width" ); };
};


bool DRC_TEST_PROVIDER_LINE_WIDTH::Run()
{
    const int progressDelta = 250;
    int       ii = 0;

    if( m_drcEngine->IsErrorLimitExceeded( DRCE_LINE_WIDTH ) )
    {
        REPORT_AUX( wxT( "Graphic & text line width violations ignored. Tests not run." ) );
        return true; // continue with other tests
    }

    if( !m_drcEngine->HasRulesForConstraintType( LINEWIDTH_CONSTRAINT ) )
    {
        REPORT_AUX( wxT( "No graphic & text line with constraints found. Tests not run." ) );
        return true; // continue with other tests
    }

    if( !reportPhase( _( "Checking graphic & text line width..." ) ) )
        return false; // DRC cancelled

    auto checkLineWidth = [&]( BOARD_ITEM* item ) -> bool
    {
        if( m_drcEngine->IsErrorLimitExceeded( DRCE_LINE_WIDTH ) )
        {
            return false;
        }

        DRC_CONSTRAINT constraint = m_drcEngine->EvalRules( LINEWIDTH_CONSTRAINT, item, nullptr, item->GetLayer() );

        if( constraint.GetSeverity() == RPT_SEVERITY_IGNORE )
            return true;

        int      actualLineWidth = 0;
        bool     error = false;
        wxString extension;

        switch( item->Type() )
        {
        default: error = true; break;
        case PCB_TEXTBOX_T:
            actualLineWidth = static_cast<PCB_TEXTBOX*>( item )->GetBorderWidth();
            extension = _( "(border width)" );
            break;
        case PCB_SHAPE_T:
            PCB_SHAPE* shapeItem = static_cast<PCB_SHAPE*>( item );
            actualLineWidth = shapeItem->GetWidth();

            if( shapeItem->IsSolidFill() && shapeItem->Type() != PCB_ARC_T )
                error = true;

            if( shapeItem->IsHatchedFill() )
                actualLineWidth = std::min( actualLineWidth, shapeItem->GetHatchLineWidth() );

            break;
        }

        if( error == true )
            return true;

        if( constraint.Value().HasMin() && actualLineWidth < constraint.Value().Min() )
        {
            std::shared_ptr<DRC_ITEM> drcItem = DRC_ITEM::Create( LINEWIDTH_CONSTRAINT );
            wxString msg = formatMsg( _( "(%s min line width %s; actual %s)" ), constraint.GetName(), constraint.Value().Min(),
                                      actualLineWidth );

            drcItem->SetErrorMessage( drcItem->GetErrorText( true ) + wxS( " " ) + extension + wxS( " " ) + msg );
            drcItem->SetItems( item );
            drcItem->SetViolatingRule( constraint.GetParentRule() );

            reportViolation( drcItem, item->GetPosition(), item->GetLayer() );
        }

        if( constraint.Value().HasMax() && actualLineWidth > constraint.Value().Max() )
        {
            std::shared_ptr<DRC_ITEM> drcItem = DRC_ITEM::Create( LINEWIDTH_CONSTRAINT );
            wxString msg = formatMsg( _( "(%s max line width %s; actual %s)" ), constraint.GetName(), constraint.Value().Max(),
                                      actualLineWidth );

            drcItem->SetErrorMessage( drcItem->GetErrorText( true ) + wxS( " " ) + extension + wxS( " " ) + msg );
            drcItem->SetItems( item );
            drcItem->SetViolatingRule( constraint.GetParentRule() );

            reportViolation( drcItem, item->GetPosition(), item->GetLayer() );
        }

        return true;
    };

    auto checkFootprint = [&]( const FOOTPRINT* aFootprint ) -> bool
    {
        for( BOARD_ITEM* item : aFootprint->GraphicalItems() )
        {
            if( item->Type() == PCB_SHAPE_T )
                checkLineWidth( item );
        }

        return true;
    };

    thread_pool&                   tp = GetKiCadThreadPool();
    std::vector<std::future<bool>> returns;
    int reportSize = m_drcEngine->GetBoard()->Drawings().size() + m_drcEngine->GetBoard()->Footprints().size();

    returns.reserve( reportSize );

    for( BOARD_ITEM* item : m_drcEngine->GetBoard()->Drawings() )
    {
        switch( item->Type() )
        {
        case PCB_SHAPE_T:
            returns.emplace_back( tp.submit_task(
                    [checkLineWidth, item]
                    {
                        return checkLineWidth( item );
                    } ) );
            break;
        default: break;
        }
    }

    for( const FOOTPRINT* footprint : m_drcEngine->GetBoard()->Footprints() )
    {
        returns.emplace_back( tp.submit_task(
                [checkFootprint, footprint]
                {
                    return checkFootprint( footprint );
                } ) );
    }

    for( std::future<bool>& ret : returns )
    {
        std::future_status status = ret.wait_for( std::chrono::milliseconds( 250 ) );

        while( status != std::future_status::ready )
        {
            reportProgress( ii++, reportSize, progressDelta );
            status = ret.wait_for( std::chrono::milliseconds( 250 ) );
        }
    }

    return !m_drcEngine->IsCancelled();
}


namespace detail
{
static DRC_REGISTER_TEST_PROVIDER<DRC_TEST_PROVIDER_LINE_WIDTH> dummy;
}