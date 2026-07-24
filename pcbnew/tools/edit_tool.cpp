/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2013-2023 CERN
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 * @author Maciej Suminski <maciej.suminski@cern.ch>
 * @author Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
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

#include <macros.h>
#include <advanced_config.h>
#include <clipboard.h>
#include <limits>
#include <kiplatform/ui.h>
#include <gal/graphics_abstraction_layer.h>
#include <board.h>
#include <board_design_settings.h>
#include <collectors.h>
#include <footprint.h>
#include <increment.h>
#include <pcb_shape.h>
#include <pcb_group.h>
#include <constraints/constraint_copy.h>
#include <constraints/pcb_constraint.h>
#include <pcb_point.h>
#include <pcb_target.h>
#include <pcb_textbox.h>
#include <pcb_table.h>
#include <pcb_generator.h>
#include <zone.h>
#include <pad.h>
#include <pcb_edit_frame.h>
#include <drawing_sheet/ds_proxy_view_item.h>
#include <kiway.h>
#include <status_popup.h>
#include <tool/action_manager.h>
#include <tool/selection_conditions.h>
#include <tool/tool_manager.h>
#include <tools/pcb_actions.h>
#include <tools/pcb_selection_tool.h>
#include <tools/constraint_edit_tool.h>
#include <tools/edit_tool.h>
#include <tools/item_modification_routine.h>
#include <tools/pcb_picker_tool.h>
#include <tools/pcb_point_editor.h>
#include <tools/tool_event_utils.h>
#include <tools/pcb_grid_helper.h>
#include <tools/pad_tool.h>
#include <view/view_controls.h>
#include <connectivity/connectivity_algo.h>
#include <pcbnew_id.h>
#include <core/kicad_algo.h>
#include <fix_board_shape.h>
#include <bitmaps.h>
#include <functional>
using namespace std::placeholders;
#include "kicad_clipboard.h"
#include <wx/hyperlink.h>
#include <router/router_tool.h>
#include <dialog_get_footprint_by_name.h>
#include <dialogs/dialog_move_exact.h>
#include <dialogs/dialog_track_via_properties.h>
#include <dialogs/dialog_tablecell_properties.h>
#include <dialogs/dialog_table_properties.h>
#include <dialogs/dialog_multi_unit_entry.h>
#include <dialogs/dialog_unit_entry.h>
#include <pcb_reference_image.h>
#include <algorithm>
#include <cmath>
#include <vector>

const unsigned int EDIT_TOOL::COORDS_PADDING = pcbIUScale.mmToIU( 20 );

static bool itemHasEditableCorners( BOARD_ITEM* aItem )
{
    if( !aItem )
        return false;

    if( aItem->Type() == PCB_SHAPE_T )
    {
        PCB_SHAPE* shape = static_cast<PCB_SHAPE*>( aItem );
        return shape->GetShape() == SHAPE_T::POLY;
    }

    if( aItem->Type() == PCB_ZONE_T )
    {
        ZONE* zone = static_cast<ZONE*>( aItem );

        if( zone->IsTeardropArea() )
            return false;

        return true;
    }

    return false;
}

static bool selectionHasEditableCorners( const SELECTION& aSelection )
{
    if( aSelection.GetSize() != 1 )
        return false;

    BOARD_ITEM* item = dynamic_cast<BOARD_ITEM*>( aSelection.Front() );
    return itemHasEditableCorners( item );
}

static const std::vector<KICAD_T> padTypes = { PCB_PAD_T };

static const std::vector<KICAD_T> footprintTypes = { PCB_FOOTPRINT_T };

static const std::vector<KICAD_T> groupTypes = { PCB_GROUP_T };

static const std::vector<KICAD_T> trackTypes = { PCB_TRACE_T, PCB_ARC_T, PCB_VIA_T };

static const std::vector<KICAD_T> matchPropertiesTypes = { PCB_TRACE_T, PCB_ARC_T, PCB_VIA_T };

static const std::vector<KICAD_T> extendTrackTypes = { PCB_TRACE_T };

static const std::vector<KICAD_T> trimTrackTypes = { PCB_TRACE_T, PCB_ARC_T };

static const std::vector<KICAD_T> baseConnectedTypes = { PCB_PAD_T, PCB_VIA_T, PCB_TRACE_T, PCB_ARC_T };

static const std::vector<KICAD_T> connectedTypes = { PCB_TRACE_T, PCB_ARC_T, PCB_VIA_T, PCB_PAD_T, PCB_ZONE_T };

static const std::vector<KICAD_T> routableTypes = { PCB_TRACE_T, PCB_ARC_T, PCB_VIA_T, PCB_PAD_T, PCB_FOOTPRINT_T };


// Types with no Mirror() override, which would fall through to the warning-dialog
// BOARD_ITEM::Mirror. Free pads are handled specially by the tool but not by PCB_GROUP::Mirror.
static const std::vector<KICAD_T> nonMirrorableTypes = {
    PCB_FOOTPRINT_T, PCB_PAD_T, PCB_TARGET_T, PCB_REFERENCE_IMAGE_T,
};


// A group is mirrorable only if none of its members hit BOARD_ITEM::Mirror.
static bool groupMirrorable( const PCB_GROUP* aGroup )
{
    bool ok = true;

    aGroup->RunOnChildren(
            [&]( BOARD_ITEM* aChild )
            {
                if( aChild->IsType( nonMirrorableTypes ) )
                    ok = false;
            },
            RECURSE_MODE::RECURSE );

    return ok;
}


// True if at least one selected item can be mirrored. A group counts only if all its members can.
static bool selectionMirrorable( const SELECTION& aSelection )
{
    for( EDA_ITEM* item : aSelection )
    {
        if( item->Type() == PCB_GROUP_T )
        {
            if( groupMirrorable( static_cast<PCB_GROUP*>( item ) ) )
                return true;
        }
        else if( item->IsType( EDIT_TOOL::MirrorableItems ) )
        {
            return true;
        }
    }

    return false;
}


EDIT_TOOL::EDIT_TOOL() :
        PCB_TOOL_BASE( "pcbnew.InteractiveEdit" ),
        m_selectionTool( nullptr ),
        m_dragging( false ),
        m_inMoveWithReference( false )
{
}


void EDIT_TOOL::Reset( RESET_REASON aReason )
{
    m_dragging = false;
    m_inMoveWithReference = false;

    m_statusPopup = std::make_unique<STATUS_TEXT_POPUP>( getEditFrame<PCB_BASE_EDIT_FRAME>() );
}


static std::shared_ptr<CONDITIONAL_MENU> makeMirrorRotateMenu( TOOL_INTERACTIVE* aTool )
{
    auto menu = std::make_shared<CONDITIONAL_MENU>( aTool );

    menu->SetIcon( BITMAPS::special_tools );
    menu->SetUntranslatedTitle( _HKI( "Mirror / Rotate" ) );

    auto canMirror = []( const SELECTION& aSelection )
    {
        if( SELECTION_CONDITIONS::OnlyTypes( padTypes )( aSelection ) )
            return false;

        return selectionMirrorable( aSelection );
    };

    menu->AddItem( PCB_ACTIONS::rotateCcw, SELECTION_CONDITIONS::NotEmpty );
    menu->AddItem( PCB_ACTIONS::rotateCw, SELECTION_CONDITIONS::NotEmpty );
    menu->AddItem( PCB_ACTIONS::mirrorH, canMirror );
    menu->AddItem( PCB_ACTIONS::mirrorV, canMirror );

    return menu;
}


static std::shared_ptr<CONDITIONAL_MENU> makeRoutingToolsMenu( TOOL_INTERACTIVE* aTool )
{
    auto menu = std::make_shared<CONDITIONAL_MENU>( aTool );

    menu->SetIcon( BITMAPS::special_tools );
    menu->SetUntranslatedTitle( _HKI( "Routing" ) );

    auto notMovingCondition = []( const SELECTION& aSelection )
    {
        return aSelection.Empty() || !aSelection.Front()->IsMoving();
    };

    const SELECTION_CONDITION isRoutable =
            SELECTION_CONDITIONS::NotEmpty && SELECTION_CONDITIONS::HasTypes( routableTypes ) && notMovingCondition;

    menu->AddItem( PCB_ACTIONS::routerRouteSelected, isRoutable );
    menu->AddItem( PCB_ACTIONS::routerRouteSelectedFromEnd, isRoutable );
    menu->AddItem( PCB_ACTIONS::unrouteSelected, isRoutable );
    menu->AddItem( PCB_ACTIONS::unrouteSegment, isRoutable );
    menu->AddItem( PCB_ACTIONS::routerAutorouteSelected, isRoutable );

    return menu;
}


static std::shared_ptr<CONDITIONAL_MENU> makePositioningToolsMenu( TOOL_INTERACTIVE* aTool )
{
    auto menu = std::make_shared<CONDITIONAL_MENU>( aTool );

    menu->SetIcon( BITMAPS::special_tools );
    menu->SetUntranslatedTitle( _HKI( "Position" ) );

    auto notMovingCondition = []( const SELECTION& aSelection )
    {
        return aSelection.Empty() || !aSelection.Front()->IsMoving();
    };

    menu->AddItem( PCB_ACTIONS::moveExact, SELECTION_CONDITIONS::NotEmpty && notMovingCondition );
    menu->AddItem( PCB_ACTIONS::moveWithReference, SELECTION_CONDITIONS::NotEmpty && notMovingCondition );
    menu->AddItem( PCB_ACTIONS::moveIndividually, SELECTION_CONDITIONS::MoreThan( 1 ) && notMovingCondition );
    menu->AddItem( PCB_ACTIONS::positionRelative, SELECTION_CONDITIONS::NotEmpty && notMovingCondition );
    menu->AddItem( PCB_ACTIONS::interactiveOffsetTool, SELECTION_CONDITIONS::NotEmpty && notMovingCondition );
    return menu;
};


static std::shared_ptr<CONDITIONAL_MENU> makeShapeModificationMenu( TOOL_INTERACTIVE* aTool )
{
    auto menu = std::make_shared<CONDITIONAL_MENU>( aTool );

    menu->SetUntranslatedTitle( _HKI( "Shape Modification" ) );

    static const std::vector<KICAD_T> filletChamferTypes = { PCB_SHAPE_LOCATE_POLY_T, PCB_SHAPE_LOCATE_RECT_T,
                                                             PCB_SHAPE_LOCATE_SEGMENT_T };

    static const std::vector<KICAD_T> healShapesTypes = { PCB_SHAPE_LOCATE_SEGMENT_T, PCB_SHAPE_LOCATE_ARC_T,
                                                          PCB_SHAPE_LOCATE_BEZIER_T };

    static const std::vector<KICAD_T> lineExtendTypes = { PCB_SHAPE_LOCATE_SEGMENT_T };

    static const std::vector<KICAD_T> polygonBooleanTypes = { PCB_SHAPE_LOCATE_RECT_T, PCB_SHAPE_LOCATE_POLY_T,
                                                              PCB_SHAPE_LOCATE_CIRCLE_T };

    static const std::vector<KICAD_T> polygonSimplifyTypes = { PCB_SHAPE_LOCATE_POLY_T, PCB_ZONE_T };

    auto hasCornerCondition = [aTool]( const SELECTION& aSelection )
    {
        PCB_POINT_EDITOR* pt_tool = aTool->GetManager()->GetTool<PCB_POINT_EDITOR>();

        return pt_tool && pt_tool->HasCorner();
    };

    auto hasMidpointCondition = [aTool]( const SELECTION& aSelection )
    {
        PCB_POINT_EDITOR* pt_tool = aTool->GetManager()->GetTool<PCB_POINT_EDITOR>();

        return pt_tool && pt_tool->HasMidpoint();
    };

    auto canAddCornerCondition = []( const SELECTION& aSelection )
    {
        const EDA_ITEM* item = aSelection.Front();

        return item && PCB_POINT_EDITOR::CanAddCorner( *item );
    };

    auto canChamferCornerCondition = []( const SELECTION& aSelection )
    {
        const EDA_ITEM* item = aSelection.Front();

        return item && PCB_POINT_EDITOR::CanChamferCorner( *item );
    };

    auto canRemoveCornerCondition = [aTool]( const SELECTION& aSelection )
    {
        PCB_POINT_EDITOR* pt_tool = aTool->GetManager()->GetTool<PCB_POINT_EDITOR>();

        return pt_tool && pt_tool->CanRemoveCorner( aSelection );
    };

    // clang-format off

    // Shape cleanup
    menu->AddItem( PCB_ACTIONS::healShapes,       SELECTION_CONDITIONS::HasTypes( healShapesTypes ) );
    menu->AddItem( PCB_ACTIONS::simplifyPolygons, SELECTION_CONDITIONS::HasTypes( polygonSimplifyTypes ) );

    menu->AddSeparator( SELECTION_CONDITIONS::OnlyTypes( filletChamferTypes ) );

    // Shape corner modifications
    menu->AddItem( PCB_ACTIONS::filletLines,    SELECTION_CONDITIONS::OnlyTypes( filletChamferTypes ) );
    menu->AddItem( PCB_ACTIONS::chamferLines,   SELECTION_CONDITIONS::OnlyTypes( filletChamferTypes ) );
    menu->AddItem( PCB_ACTIONS::dogboneCorners, SELECTION_CONDITIONS::OnlyTypes( filletChamferTypes ) );
    menu->AddItem( PCB_ACTIONS::extendLines,    SELECTION_CONDITIONS::OnlyTypes( lineExtendTypes )
                                                    && SELECTION_CONDITIONS::Count( 2 ) );

    menu->AddSeparator( SELECTION_CONDITIONS::Count( 1 ) );

    // Point editor corner operations
    menu->AddItem( PCB_ACTIONS::pointEditorMoveCorner,    hasCornerCondition );
    menu->AddItem( PCB_ACTIONS::pointEditorMoveMidpoint,  hasMidpointCondition );
    menu->AddItem( PCB_ACTIONS::pointEditorAddCorner,     SELECTION_CONDITIONS::Count( 1 ) && canAddCornerCondition );
    menu->AddItem( PCB_ACTIONS::pointEditorRemoveCorner,  SELECTION_CONDITIONS::Count( 1 ) && canRemoveCornerCondition );
    menu->AddItem( PCB_ACTIONS::pointEditorChamferCorner, SELECTION_CONDITIONS::Count( 1 ) && canChamferCornerCondition );
    menu->AddItem( PCB_ACTIONS::editVertices,             selectionHasEditableCorners );

    menu->AddSeparator( SELECTION_CONDITIONS::OnlyTypes( polygonBooleanTypes )
                            && SELECTION_CONDITIONS::MoreThan( 1 ) );

    // Polygon boolean operations
    menu->AddItem( PCB_ACTIONS::mergePolygons,     SELECTION_CONDITIONS::OnlyTypes( polygonBooleanTypes )
                                                       && SELECTION_CONDITIONS::MoreThan( 1 ) );
    menu->AddItem( PCB_ACTIONS::subtractPolygons,  SELECTION_CONDITIONS::OnlyTypes( polygonBooleanTypes )
                                                       && SELECTION_CONDITIONS::MoreThan( 1 ) );
    menu->AddItem( PCB_ACTIONS::intersectPolygons, SELECTION_CONDITIONS::OnlyTypes( polygonBooleanTypes )
                                                       && SELECTION_CONDITIONS::MoreThan( 1 ) );
    // clang-format on

    return menu;
};


// Gate-swap submenu and helpers
class GATE_SWAP_MENU : public ACTION_MENU
{
public:
    GATE_SWAP_MENU() :
            ACTION_MENU( true )
    {
        SetIcon( BITMAPS::swap );
        SetTitle( _( "Swap Gate Nets..." ) );
    }


    // We're looking for a selection of pad(s) that belong to a single footprint with multiple units.
    // Ignore non-pad items since we might have grabbed some traces inside the pad, etc.
    static const FOOTPRINT* GetSingleEligibleFootprint( const SELECTION& aSelection )
    {
        const FOOTPRINT* single = nullptr;

        for( const EDA_ITEM* it : aSelection )
        {
            if( it->Type() != PCB_PAD_T )
                continue;

            const PAD*       pad = static_cast<const PAD*>( static_cast<const BOARD_ITEM*>( it ) );
            const FOOTPRINT* fp = pad->GetParentFootprint();

            if( !fp )
                continue;

            const auto& units = fp->GetUnitInfo();

            if( units.size() < 2 )
                continue;

            const wxString& padNum = pad->GetNumber();
            bool            inAnyUnit = false;

            for( const auto& u : units )
            {
                for( const auto& pnum : u.m_pins )
                {
                    if( pnum == padNum )
                    {
                        inAnyUnit = true;
                        break;
                    }
                }

                if( inAnyUnit )
                    break;
            }

            if( !inAnyUnit )
                continue;

            if( !single )
                single = fp;
            else if( single != fp )
                return nullptr;
        }

        return single;
    }


    static std::unordered_set<wxString> CollectSelectedPadNumbers( const SELECTION& aSelection,
                                                                   const FOOTPRINT* aFootprint )
    {
        std::unordered_set<wxString> padNums;

        for( const EDA_ITEM* it : aSelection )
        {
            if( it->Type() != PCB_PAD_T )
                continue;

            const PAD* pad = static_cast<const PAD*>( static_cast<const BOARD_ITEM*>( it ) );

            if( pad->GetParentFootprint() != aFootprint )
                continue;

            padNums.insert( pad->GetNumber() );
        }

        return padNums;
    }


    // Make a list of the unit names that have any pad selected
    static std::vector<int> GetUnitsHitIndices( const FOOTPRINT*                    aFootprint,
                                                const std::unordered_set<wxString>& aSelPadNums )
    {
        std::vector<int> indices;

        const auto& units = aFootprint->GetUnitInfo();

        for( size_t i = 0; i < units.size(); ++i )
        {
            bool hasAny = false;

            for( const auto& pn : units[i].m_pins )
            {
                if( aSelPadNums.count( pn ) )
                {
                    hasAny = true;
                    break;
                }
            }

            if( hasAny )
                indices.push_back( static_cast<int>( i ) );
        }

        return indices;
    }


    // Gate swapping requires the swapped units to have equal pin counts
    static bool EqualPinCounts( const FOOTPRINT* aFootprint, const std::vector<int>& aUnitIndices )
    {
        if( aUnitIndices.empty() )
            return false;

        const auto&  units = aFootprint->GetUnitInfo();
        const size_t cnt = units[static_cast<size_t>( aUnitIndices.front() )].m_pins.size();

        for( int idx : aUnitIndices )
        {
            if( units[static_cast<size_t>( idx )].m_pins.size() != cnt )
                return false;
        }

        return true;
    }


    // Used when we have exactly one source unit selected; find all other units with equal pin counts
    static std::vector<int> GetCompatibleTargets( const FOOTPRINT* aFootprint, int aSourceIdx )
    {
        std::vector<int> targets;

        const auto&  units = aFootprint->GetUnitInfo();
        const size_t pinCount = units[static_cast<size_t>( aSourceIdx )].m_pins.size();

        for( size_t i = 0; i < units.size(); ++i )
        {
            if( static_cast<int>( i ) == aSourceIdx )
                continue;

            if( units[i].m_pins.size() != pinCount )
                continue;

            targets.push_back( static_cast<int>( i ) );
        }

        return targets;
    }

protected:
    ACTION_MENU* create() const override { return new GATE_SWAP_MENU(); }

    // The gate swap menu dynamically populates itself based on current selection of pads
    // on a single multi-unit footprint.
    //
    // If there is exactly one unit with any pad selected, we build a menu of available swaps
    // with all other units with equal pin counts.
    void update() override
    {
        Clear();

        PCB_SELECTION_TOOL* selTool = getToolManager()->GetTool<PCB_SELECTION_TOOL>();
        const SELECTION&    sel = selTool->GetSelection();

        const FOOTPRINT* fp = GetSingleEligibleFootprint( sel );

        if( !fp )
            return;

        std::unordered_set<wxString> selPadNums = CollectSelectedPadNumbers( sel, fp );

        std::vector<int> unitsHit = GetUnitsHitIndices( fp, selPadNums );

        if( unitsHit.size() != 1 )
            return;

        const int        sourceIdx = unitsHit.front();
        std::vector<int> targets = GetCompatibleTargets( fp, sourceIdx );

        for( int idx : targets )
        {
            wxString label;
            label.Printf( _( "Swap with %s" ), fp->GetUnitInfo()[static_cast<size_t>( idx )].m_unitName );
            Append( ID_POPUP_PCB_SWAP_UNIT_BASE + idx, label );
        }
    }


    OPT_TOOL_EVENT eventHandler( const wxMenuEvent& aEvent ) override
    {
        int id = aEvent.GetId();

        if( id >= ID_POPUP_PCB_SWAP_UNIT_BASE && id <= ID_POPUP_PCB_SWAP_UNIT_LAST )
        {
            PCB_SELECTION_TOOL* selTool = getToolManager()->GetTool<PCB_SELECTION_TOOL>();
            const SELECTION&    sel = selTool->GetSelection();

            const FOOTPRINT* fp = GetSingleEligibleFootprint( sel );

            if( !fp )
                return OPT_TOOL_EVENT();

            const auto& units = fp->GetUnitInfo();
            const int   targetIdx = id - ID_POPUP_PCB_SWAP_UNIT_BASE;

            if( targetIdx < 0 || targetIdx >= static_cast<int>( units.size() ) )
                return OPT_TOOL_EVENT();

            TOOL_EVENT evt = PCB_ACTIONS::swapGateNets.MakeEvent();
            evt.SetParameter( units[targetIdx].m_unitName );

            return OPT_TOOL_EVENT( evt );
        }

        return OPT_TOOL_EVENT();
    }
};


static std::shared_ptr<ACTION_MENU> makeGateSwapMenu( TOOL_INTERACTIVE* aTool )
{
    auto menu = std::make_shared<GATE_SWAP_MENU>();
    menu->SetTool( aTool );
    return menu;
};


bool EDIT_TOOL::Init()
{
    // Find the selection tool, so they can cooperate
    m_selectionTool = m_toolMgr->GetTool<PCB_SELECTION_TOOL>();

    std::shared_ptr<CONDITIONAL_MENU> routingSubMenu = makeRoutingToolsMenu( this );
    m_selectionTool->GetToolMenu().RegisterSubMenu( routingSubMenu );

    std::shared_ptr<CONDITIONAL_MENU> positioningToolsSubMenu = makePositioningToolsMenu( this );
    m_selectionTool->GetToolMenu().RegisterSubMenu( positioningToolsSubMenu );

    std::shared_ptr<CONDITIONAL_MENU> mirrorRotateSubMenu = makeMirrorRotateMenu( this );
    m_selectionTool->GetToolMenu().RegisterSubMenu( mirrorRotateSubMenu );

    std::shared_ptr<CONDITIONAL_MENU> shapeModificationSubMenu = makeShapeModificationMenu( this );
    m_selectionTool->GetToolMenu().RegisterSubMenu( shapeModificationSubMenu );

    std::shared_ptr<ACTION_MENU> gateSwapSubMenu = makeGateSwapMenu( this );
    m_selectionTool->GetToolMenu().RegisterSubMenu( gateSwapSubMenu );

    auto fpAttributesMenu = std::make_shared<CONDITIONAL_MENU>( this );
    fpAttributesMenu->SetUntranslatedTitle( _HKI( "Attributes" ) );
    fpAttributesMenu->AddCheckItem( PCB_ACTIONS::toggleExcludeFromBOM, SELECTION_CONDITIONS::ShowAlways );
    fpAttributesMenu->AddCheckItem( PCB_ACTIONS::toggleExcludeFromPosFiles, SELECTION_CONDITIONS::ShowAlways );
    m_selectionTool->GetToolMenu().RegisterSubMenu( fpAttributesMenu );

    auto positioningToolsCondition = [this]( const SELECTION& aSel )
    {
        std::shared_ptr<CONDITIONAL_MENU> subMenu = makePositioningToolsMenu( this );
        subMenu->Evaluate( aSel );
        return subMenu->GetMenuItemCount() > 0;
    };

    auto shapeModificationCondition = [this]( const SELECTION& aSel )
    {
        std::shared_ptr<CONDITIONAL_MENU> subMenu = makeShapeModificationMenu( this );
        subMenu->Evaluate( aSel );
        return subMenu->GetMenuItemCount() > 0;
    };

    // Does selection map to a single eligible footprint and exactly one unit?
    auto gateSwapSingleUnitOnOneFootprint = []( const SELECTION& aSelection )
    {
        const FOOTPRINT* fp = GATE_SWAP_MENU::GetSingleEligibleFootprint( aSelection );

        if( !fp )
            return false;

        std::unordered_set<wxString> selPadNums = GATE_SWAP_MENU::CollectSelectedPadNumbers( aSelection, fp );

        std::vector<int> unitsHit = GATE_SWAP_MENU::GetUnitsHitIndices( fp, selPadNums );

        if( unitsHit.size() != 1 )
            return false;

        const int        sourceIdx = unitsHit.front();
        std::vector<int> targets = GATE_SWAP_MENU::GetCompatibleTargets( fp, sourceIdx );
        return !targets.empty();
    };

    // Does selection map to a single eligible footprint and more than one unit with equal pin counts?
    auto gateSwapMultipleUnitsOnOneFootprint = []( const SELECTION& aSelection )
    {
        const FOOTPRINT* fp = GATE_SWAP_MENU::GetSingleEligibleFootprint( aSelection );

        if( !fp )
            return false;

        std::unordered_set<wxString> selPadNums = GATE_SWAP_MENU::CollectSelectedPadNumbers( aSelection, fp );

        std::vector<int> unitsHit = GATE_SWAP_MENU::GetUnitsHitIndices( fp, selPadNums );

        if( unitsHit.size() < 2 )
            return false;

        return GATE_SWAP_MENU::EqualPinCounts( fp, unitsHit );
    };

    auto propertiesCondition = [this]( const SELECTION& aSel )
    {
        if( aSel.GetSize() == 0 )
        {
            if( getView()->IsLayerVisible( LAYER_SCHEMATIC_DRAWINGSHEET ) )
            {
                DS_PROXY_VIEW_ITEM* ds = canvas()->GetDrawingSheet();
                VECTOR2D            cursor = getViewControls()->GetCursorPosition( false );

                if( ds && ds->HitTestDrawingSheetItems( getView(), cursor ) )
                    return true;
            }

            return false;
        }

        if( aSel.GetSize() == 1 )
            return true;

        for( EDA_ITEM* item : aSel )
        {
            if( !dynamic_cast<PCB_TRACK*>( item ) )
                return false;
        }

        return true;
    };

    auto inFootprintEditor = [this]( const SELECTION& aSelection )
    {
        return m_isFootprintEditor;
    };

    auto canMirror = [this]( const SELECTION& aSelection )
    {
        if( !m_isFootprintEditor && SELECTION_CONDITIONS::OnlyTypes( padTypes )( aSelection ) )
        {
            return false;
        }

        // Gates the whole "Mirror / Rotate" submenu, so keep it open for any group. Rotate works
        // on a group with footprints, only the mirror items inside disable (see makeMirrorRotateMenu).
        if( SELECTION_CONDITIONS::HasTypes( groupTypes )( aSelection ) )
            return true;

        return SELECTION_CONDITIONS::HasTypes( EDIT_TOOL::MirrorableItems )( aSelection );
    };

    auto singleFootprintCondition =
            SELECTION_CONDITIONS::OnlyTypes( footprintTypes ) && SELECTION_CONDITIONS::Count( 1 );

    auto multipleFootprintsCondition = []( const SELECTION& aSelection )
    {
        bool foundFirst = false;

        for( EDA_ITEM* item : aSelection )
        {
            if( item->Type() == PCB_FOOTPRINT_T )
            {
                if( foundFirst )
                    return true;
                else
                    foundFirst = true;
            }
        }

        return false;
    };

    auto excludeFromBOMCond = [this]( const SELECTION& aSel )
    {
        wxString variantName;
        int      checked = 0, unchecked = 0;

        if( BOARD* board = frame()->GetBoard() )
            variantName = board->GetCurrentVariant();

        for( const EDA_ITEM* item : aSel )
        {
            if( item->Type() == PCB_FOOTPRINT_T )
            {
                if( static_cast<const FOOTPRINT*>( item )->GetExcludedFromBOMForVariant( variantName ) )
                    checked++;
                else
                    unchecked++;
            }
        }

        return checked > 0 && unchecked == 0;
    };

    auto excludeFromPosFilesCond = [this]( const SELECTION& aSel )
    {
        wxString variantName;
        int      checked = 0, unchecked = 0;

        if( BOARD* board = frame()->GetBoard() )
            variantName = board->GetCurrentVariant();

        for( const EDA_ITEM* item : aSel )
        {
            if( item->Type() == PCB_FOOTPRINT_T )
            {
                if( static_cast<const FOOTPRINT*>( item )->GetExcludedFromPosFilesForVariant( variantName ) )
                    checked++;
                else
                    unchecked++;
            }
        }

        return checked > 0 && unchecked == 0;
    };

    auto noActiveToolCondition = [this]( const SELECTION& aSelection )
    {
        return frame()->ToolStackIsEmpty();
    };

    auto notMovingCondition = []( const SELECTION& aSelection )
    {
        return aSelection.Empty() || !aSelection.Front()->IsMoving();
    };

    auto noItemsCondition = [this]( const SELECTION& aSelections ) -> bool
    {
        return frame()->GetBoard() && !frame()->GetBoard()->IsEmpty();
    };

    auto isSkippable = [this]( const SELECTION& aSelection )
    {
        return frame()->IsCurrentTool( PCB_ACTIONS::moveIndividually );
    };

    SELECTION_CONDITION isRoutable = SELECTION_CONDITIONS::NotEmpty && SELECTION_CONDITIONS::HasTypes( routableTypes )
                                     && notMovingCondition && !inFootprintEditor;

    const auto canCopyAsText = SELECTION_CONDITIONS::NotEmpty
                               && SELECTION_CONDITIONS::OnlyTypes( {
                                       PCB_FIELD_T,
                                       PCB_TEXT_T,
                                       PCB_TEXTBOX_T,
                                       PCB_DIM_ALIGNED_T,
                                       PCB_DIM_LEADER_T,
                                       PCB_DIM_CENTER_T,
                                       PCB_DIM_RADIAL_T,
                                       PCB_DIM_ORTHOGONAL_T,
                                       PCB_TABLE_T,
                                       PCB_TABLECELL_T,
                               } );

    // Add context menu entries that are displayed when selection tool is active
    CONDITIONAL_MENU& menu = m_selectionTool->GetToolMenu().GetMenu();

    // clang-format off
    menu.AddItem( ACTIONS::selectAll,             noItemsCondition );
    menu.AddItem( ACTIONS::unselectAll,           noItemsCondition );
    menu.AddSeparator();

    menu.AddItem( PCB_ACTIONS::skip,              isSkippable );
    menu.AddItem( PCB_ACTIONS::move,                    SELECTION_CONDITIONS::NotEmpty && notMovingCondition );

    menu.AddItem( PCB_ACTIONS::drag45Degree,      SELECTION_CONDITIONS::Count( 1 )
                                                      && SELECTION_CONDITIONS::OnlyTypes( GENERAL_COLLECTOR::DraggableItems ) );
    menu.AddItem( PCB_ACTIONS::dragFreeAngle,     SELECTION_CONDITIONS::Count( 1 )
                                                      && SELECTION_CONDITIONS::OnlyTypes( GENERAL_COLLECTOR::DraggableItems )
                                                      && !SELECTION_CONDITIONS::OnlyTypes( footprintTypes ) );
    menu.AddItem( PCB_ACTIONS::flip,              SELECTION_CONDITIONS::NotEmpty );

    menu.AddItem( PCB_ACTIONS::swap,              SELECTION_CONDITIONS::MoreThan( 1 ) );
    menu.AddItem( PCB_ACTIONS::swapPadNets,       SELECTION_CONDITIONS::MoreThan( 1 )
                                                      && SELECTION_CONDITIONS::OnlyTypes( padTypes ) );
    menu.AddItem( PCB_ACTIONS::swapGateNets,      gateSwapMultipleUnitsOnOneFootprint );
    menu.AddMenu( gateSwapSubMenu.get(),          gateSwapSingleUnitOnOneFootprint );

    menu.AddSeparator();

    menu.AddItem( PCB_ACTIONS::matchProperties,   SELECTION_CONDITIONS::Count( 1 )
                                                  && SELECTION_CONDITIONS::OnlyTypes( matchPropertiesTypes ) );
    
    menu.AddItem( PCB_ACTIONS::extendTrack,       SELECTION_CONDITIONS::Count( 1 )
                                                  && SELECTION_CONDITIONS::OnlyTypes( extendTrackTypes ) );

    menu.AddItem( PCB_ACTIONS::trimTrack,         SELECTION_CONDITIONS::Count( 1 )
                                                  && SELECTION_CONDITIONS::OnlyTypes( trimTrackTypes ) );

    menu.AddSeparator();

    menu.AddItem( PCB_ACTIONS::breakTrack,        SELECTION_CONDITIONS::Count( 1 )
                                                      && SELECTION_CONDITIONS::OnlyTypes( trackTypes ) );

    menu.AddItem( PCB_ACTIONS::filletTracks,      SELECTION_CONDITIONS::OnlyTypes( trackTypes ) );

    menu.AddSeparator();

    menu.AddItem( PCB_ACTIONS::packAndMoveFootprints, SELECTION_CONDITIONS::MoreThan( 1 )
                                                      && SELECTION_CONDITIONS::HasType( PCB_FOOTPRINT_T ) );

    menu.AddItem( PCB_ACTIONS::assignNetClass,    SELECTION_CONDITIONS::OnlyTypes( connectedTypes )
                                                      && !inFootprintEditor );
    menu.AddItem( PCB_ACTIONS::inspectClearance,  SELECTION_CONDITIONS::Count( 2 ) );

    // Footprint actions
    menu.AddSeparator();
    menu.AddItem( PCB_ACTIONS::editFpInFpEditor,  singleFootprintCondition );
    menu.AddItem( PCB_ACTIONS::updateFootprint,   singleFootprintCondition );
    menu.AddItem( PCB_ACTIONS::updateFootprints,  multipleFootprintsCondition );
    menu.AddItem( PCB_ACTIONS::changeFootprint,   singleFootprintCondition );
    menu.AddItem( PCB_ACTIONS::changeFootprints,  multipleFootprintsCondition );
    menu.AddMenu( fpAttributesMenu.get(),         singleFootprintCondition || multipleFootprintsCondition );

    // Add the submenu for the special tools: modfiers and positioning tools
    menu.AddSeparator( 100 );
    menu.AddMenu( routingSubMenu.get(), isRoutable, 100 );
    menu.AddMenu( mirrorRotateSubMenu.get(), canMirror, 100 );
    menu.AddMenu( shapeModificationSubMenu.get(), shapeModificationCondition, 100 );
    menu.AddMenu( positioningToolsSubMenu.get(),  positioningToolsCondition, 100 );

    menu.AddSeparator( 150 );
    menu.AddItem( ACTIONS::cut,                   SELECTION_CONDITIONS::NotEmpty, 150 );
    menu.AddItem( ACTIONS::copy,                  SELECTION_CONDITIONS::NotEmpty, 150 );
    menu.AddItem( PCB_ACTIONS::copyWithReference, SELECTION_CONDITIONS::NotEmpty && notMovingCondition, 150 );
    menu.AddItem( ACTIONS::copyAsText,            canCopyAsText, 150 );

    // Selection tool handles the context menu for some other tools, such as the Picker.
    // Don't add things like Paste when another tool is active.
    menu.AddItem( ACTIONS::paste,                 noActiveToolCondition, 150 );
    menu.AddItem( ACTIONS::pasteSpecial,          noActiveToolCondition && !inFootprintEditor, 150 );
    menu.AddItem( ACTIONS::duplicate,             SELECTION_CONDITIONS::NotEmpty, 150 );
    menu.AddItem( ACTIONS::doDelete,              SELECTION_CONDITIONS::NotEmpty, 150 );

    menu.AddSeparator( 2000 );
    menu.AddItem( PCB_ACTIONS::properties,        propertiesCondition, 2000 );
    // clang-format on

    ACTION_MANAGER* mgr = m_toolMgr->GetActionManager();
    mgr->SetConditions( PCB_ACTIONS::toggleExcludeFromBOM, ACTION_CONDITIONS().Check( excludeFromBOMCond ) );
    mgr->SetConditions( PCB_ACTIONS::toggleExcludeFromPosFiles, ACTION_CONDITIONS().Check( excludeFromPosFilesCond ) );

    return true;
}


/**
 * @return a reference to the footprint found by its reference on the current board. The
 *         reference is entered by the user from a dialog (by awxTextCtlr, or a list of
 *         available references)
 */
static FOOTPRINT* GetFootprintFromBoardByReference( PCB_BASE_FRAME& aFrame )
{
    wxString          footprintName;
    wxArrayString     fplist;
    const FOOTPRINTS& footprints = aFrame.GetBoard()->Footprints();

    // Build list of available fp references, to display them in dialog
    for( FOOTPRINT* fp : footprints )
        fplist.Add( fp->GetReference() + wxT( "    ( " ) + fp->GetValue() + wxT( " )" ) );

    fplist.Sort();

    DIALOG_GET_FOOTPRINT_BY_NAME dlg( &aFrame, fplist );

    if( dlg.ShowModal() != wxID_OK ) //Aborted by user
        return nullptr;

    footprintName = dlg.GetValue();
    footprintName.Trim( true );
    footprintName.Trim( false );

    if( !footprintName.IsEmpty() )
    {
        for( FOOTPRINT* fp : footprints )
        {
            if( fp->GetReference().CmpNoCase( footprintName ) == 0 )
                return fp;
        }
    }

    return nullptr;
}


int EDIT_TOOL::GetAndPlace( const TOOL_EVENT& aEvent )
{
    // GetAndPlace makes sense only in board editor, although it is also called
    // in fpeditor, that shares the same EDIT_TOOL list
    if( IsFootprintEditor() )
        return 0;

    PCB_SELECTION_TOOL* selectionTool = m_toolMgr->GetTool<PCB_SELECTION_TOOL>();
    FOOTPRINT*          fp = GetFootprintFromBoardByReference( *frame() );

    if( fp )
    {
        m_toolMgr->RunAction( ACTIONS::selectionClear );
        m_toolMgr->RunAction<EDA_ITEM*>( ACTIONS::selectItem, fp );

        selectionTool->GetSelection().SetReferencePoint( fp->GetPosition() );
        m_toolMgr->PostAction( PCB_ACTIONS::move );
    }

    return 0;
}


bool EDIT_TOOL::invokeInlineRouter( int aDragMode )
{
    ROUTER_TOOL* theRouter = m_toolMgr->GetTool<ROUTER_TOOL>();

    if( !theRouter )
        return false;

    // don't allow switch from moving to dragging
    if( m_dragging )
    {
        wxBell();
        return false;
    }

    // make sure we don't accidentally invoke inline routing mode while the router is already
    // active!
    if( theRouter->IsToolActive() )
        return false;

    if( theRouter->CanInlineDrag( aDragMode ) )
    {
        m_toolMgr->RunAction( PCB_ACTIONS::routerInlineDrag, aDragMode );
        return true;
    }

    return false;
}


bool EDIT_TOOL::isRouterActive() const
{
    ROUTER_TOOL* router = m_toolMgr->GetTool<ROUTER_TOOL>();

    return router && router->RoutingInProgress();
}


int EDIT_TOOL::Drag( const TOOL_EVENT& aEvent )
{
    if( !m_toolMgr->GetTool<ROUTER_TOOL>() )
    {
        wxBell();
        return false; // don't drag when no router tool (i.e. fp editor)
    }

    if( m_toolMgr->GetTool<ROUTER_TOOL>()->IsToolActive() )
    {
        wxBell();
        return false; // don't drag when router is already active
    }

    if( m_dragging )
    {
        wxBell();
        return false; // don't do a router drag when already in an EDIT_TOOL drag
    }

    int mode = PNS::DM_ANY;

    if( aEvent.IsAction( &PCB_ACTIONS::dragFreeAngle ) )
        mode |= PNS::DM_FREE_ANGLE;

    PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                sTool->FilterCollectorForFreePads( aCollector );
                sTool->FilterCollectorForHierarchy( aCollector, true );

                std::vector<PCB_TRACK*> tracks;
                std::vector<PCB_TRACK*> vias;
                std::vector<FOOTPRINT*> footprints;

                // Gather items from the collector into per-type vectors
                const auto gatherItemsByType = [&]()
                {
                    for( EDA_ITEM* item : aCollector )
                    {
                        if( PCB_TRACK* track = dynamic_cast<PCB_TRACK*>( item ) )
                        {
                            if( track->Type() == PCB_VIA_T )
                                vias.push_back( track );
                            else
                                tracks.push_back( track );
                        }
                        else if( FOOTPRINT* footprint = dynamic_cast<FOOTPRINT*>( item ) )
                        {
                            footprints.push_back( footprint );
                        }
                    }
                };

                // Initial gathering of items
                gatherItemsByType();

                if( !sTool->GetSelection().IsHover() && footprints.size() )
                {
                    // Remove non-footprints so box-selection will drag footprints.
                    for( int ii = aCollector.GetCount() - 1; ii >= 0; --ii )
                    {
                        if( aCollector[ii]->Type() != PCB_FOOTPRINT_T )
                            aCollector.Remove( ii );
                    }
                }
                else if( tracks.size() || vias.size() )
                {
                    /*
                     * First trim down selection to active layer, tracks vs zones, etc.
                     */
                    if( aCollector.GetCount() > 1 )
                    {
                        sTool->GuessSelectionCandidates( aCollector, aPt );

                        // Re-gather items after trimming to update counts
                        tracks.clear();
                        vias.clear();
                        footprints.clear();

                        gatherItemsByType();
                    }

                    /*
                     * If we have a knee between two tracks, or a via attached to two tracks,
                     * then drop the selection to a single item.  We don't want a selection
                     * disambiguation menu when it doesn't matter which items is picked.
                     */
                    auto connected = []( PCB_TRACK* track, const VECTOR2I& pt )
                    {
                        return track->GetStart() == pt || track->GetEnd() == pt;
                    };

                    if( tracks.size() == 2 && vias.size() == 0 )
                    {
                        if( connected( tracks[0], tracks[1]->GetStart() )
                            || connected( tracks[0], tracks[1]->GetEnd() ) )
                        {
                            aCollector.Remove( tracks[1] );
                        }
                    }
                    else if( tracks.size() == 2 && vias.size() == 1 )
                    {
                        if( connected( tracks[0], vias[0]->GetPosition() )
                            && connected( tracks[1], vias[0]->GetPosition() ) )
                        {
                            aCollector.Remove( tracks[0] );
                            aCollector.Remove( tracks[1] );
                        }
                    }
                }

                sTool->FilterCollectorForLockedItems( aCollector );
            } );

    m_selectionTool->ReportFilteredLockedItems();

    if( selection.Empty() )
        return 0;

    invokeInlineRouter( mode );

    return 0;
}


int EDIT_TOOL::ToggleFootprintAttribute( const TOOL_EVENT& aEvent )
{
    const PCB_SELECTION& selection = m_selectionTool->RequestSelection( EDIT_TOOL::FootprintFilter );

    if( selection.Empty() )
        return 0;

    wxString variantName;

    if( BOARD* board = frame()->GetBoard() )
        variantName = board->GetCurrentVariant();

    bool new_state = false;

    for( const EDA_ITEM* item : selection )
    {
        const FOOTPRINT* fp = static_cast<const FOOTPRINT*>( item );

        if( ( aEvent.IsAction( &PCB_ACTIONS::toggleExcludeFromBOM )
              && !fp->GetExcludedFromBOMForVariant( variantName ) )
            || ( aEvent.IsAction( &PCB_ACTIONS::toggleExcludeFromPosFiles )
                 && !fp->GetExcludedFromPosFilesForVariant( variantName ) ) )
        {
            new_state = true;
            break;
        }
    }

    BOARD_COMMIT commit( this );

    for( EDA_ITEM* item : selection )
    {
        FOOTPRINT* fp = static_cast<FOOTPRINT*>( item );
        commit.Modify( fp );

        if( !variantName.IsEmpty() )
        {
            FOOTPRINT_VARIANT* variant = fp->GetVariant( variantName );

            if( !variant )
                variant = fp->AddVariant( variantName );

            if( variant )
            {
                if( aEvent.IsAction( &PCB_ACTIONS::toggleExcludeFromBOM ) )
                    variant->SetExcludedFromBOM( new_state );
                else if( aEvent.IsAction( &PCB_ACTIONS::toggleExcludeFromPosFiles ) )
                    variant->SetExcludedFromPosFiles( new_state );

                continue;
            }
        }

        if( aEvent.IsAction( &PCB_ACTIONS::toggleExcludeFromBOM ) )
            fp->SetExcludedFromBOM( new_state );
        else if( aEvent.IsAction( &PCB_ACTIONS::toggleExcludeFromPosFiles ) )
            fp->SetExcludedFromPosFiles( new_state );
    }

    if( !commit.Empty() )
        commit.Push( _( "Toggle Attribute" ) );

    if( selection.IsHover() )
        m_toolMgr->RunAction( ACTIONS::selectionClear );

    return 0;
}


int EDIT_TOOL::ChangeTrackWidth( const TOOL_EVENT& aEvent )
{
    const PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                // Iterate from the back so we don't have to worry about removals.
                for( int i = aCollector.GetCount() - 1; i >= 0; --i )
                {
                    BOARD_ITEM* item = aCollector[i];

                    if( !dynamic_cast<PCB_TRACK*>( item ) )
                        aCollector.Remove( item );
                }

                sTool->FilterCollectorForLockedItems( aCollector );
            } );

    m_selectionTool->ReportFilteredLockedItems();

    BOARD_COMMIT commit( this );

    for( EDA_ITEM* item : selection )
    {
        if( item->Type() == PCB_VIA_T )
        {
            PCB_VIA* via = static_cast<PCB_VIA*>( item );

            commit.Modify( via );

            int new_width;
            int new_drill;

            if( via->GetViaType() == VIATYPE::MICROVIA )
            {
                NETCLASS* netClass = via->GetEffectiveNetClass();

                new_width = netClass->GetuViaDiameter();
                new_drill = netClass->GetuViaDrill();
            }
            else
            {
                new_width = board()->GetDesignSettings().GetCurrentViaSize();
                new_drill = board()->GetDesignSettings().GetCurrentViaDrill();
            }

            via->SetDrill( new_drill );
            // TODO(JE) padstacks - is this correct behavior already?  If so, also change stack mode
            via->SetWidth( PADSTACK::ALL_LAYERS, new_width );
        }
        else if( item->Type() == PCB_TRACE_T || item->Type() == PCB_ARC_T )
        {
            PCB_TRACK* track = dynamic_cast<PCB_TRACK*>( item );

            wxCHECK( track, 0 );

            commit.Modify( track );

            int new_width = board()->GetDesignSettings().GetCurrentTrackWidth();
            track->SetWidth( new_width );
        }
    }

    commit.Push( _( "Edit Track Width/Via Size" ) );

    if( selection.IsHover() )
    {
        m_toolMgr->RunAction( ACTIONS::selectionClear );

        // Notify other tools of the changes -- This updates the visual ratsnest
        m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );
    }

    return 0;
}


int EDIT_TOOL::ChangeTrackLayer( const TOOL_EVENT& aEvent )
{
    if( m_toolMgr->GetTool<ROUTER_TOOL>() && m_toolMgr->GetTool<ROUTER_TOOL>()->IsToolActive() )
        return 0;

    bool isNext = aEvent.IsAction( &PCB_ACTIONS::changeTrackLayerNext );

    const PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                // Iterate from the back so we don't have to worry about removals.
                for( int i = aCollector.GetCount() - 1; i >= 0; --i )
                {
                    BOARD_ITEM* item = aCollector[i];

                    if( !dynamic_cast<PCB_TRACK*>( item ) )
                        aCollector.Remove( item );
                }

                sTool->FilterCollectorForLockedItems( aCollector );
            } );

    m_selectionTool->ReportFilteredLockedItems();

    PCB_LAYER_ID origLayer = frame()->GetActiveLayer();

    if( isNext )
        m_toolMgr->RunAction( PCB_ACTIONS::layerNext );
    else
        m_toolMgr->RunAction( PCB_ACTIONS::layerPrev );

    PCB_LAYER_ID newLayer = frame()->GetActiveLayer();

    if( newLayer == origLayer )
        return 0;

    BOARD_COMMIT commit( this );

    for( EDA_ITEM* item : selection )
    {
        if( item->Type() == PCB_TRACE_T || item->Type() == PCB_ARC_T )
        {
            PCB_TRACK* track = dynamic_cast<PCB_TRACK*>( item );

            wxCHECK( track, 0 );

            commit.Modify( track );

            track->SetLayer( newLayer );
        }
    }

    commit.Push( _( "Edit Track Layer" ) );

    if( selection.IsHover() )
    {
        m_toolMgr->RunAction( ACTIONS::selectionClear );

        // Notify other tools of the changes -- This updates the visual ratsnest
        m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );
    }

    return 0;
}


int EDIT_TOOL::FilletTracks( const TOOL_EVENT& aEvent )
{
    // Store last used fillet radius to allow pressing "enter" if repeat fillet is required
    static int filletRadius = 0;

    PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                // Iterate from the back so we don't have to worry about removals.
                for( int i = aCollector.GetCount() - 1; i >= 0; --i )
                {
                    BOARD_ITEM* item = aCollector[i];

                    if( !dynamic_cast<PCB_TRACK*>( item ) )
                        aCollector.Remove( item );
                }

                sTool->FilterCollectorForLockedItems( aCollector );
            } );

    if( m_selectionTool->ReportFilteredLockedItems() )
        return 0;

    if( selection.Size() < 2 )
    {
        frame()->ShowInfoBarMsg( _( "At least two straight track segments must be selected." ) );
        return 0;
    }

    WX_UNIT_ENTRY_DIALOG dlg( frame(), _( "Fillet Tracks" ), _( "Radius:" ), filletRadius );

    if( dlg.ShowModal() == wxID_CANCEL || dlg.GetValue() == 0 )
        return 0;

    filletRadius = dlg.GetValue();

    struct FILLET_OP
    {
        PCB_TRACK* t1;
        PCB_TRACK* t2;
        // Start point of track is modified after PCB_ARC is added, otherwise the end point:
        bool t1Start = true;
        bool t2Start = true;
    };

    std::vector<FILLET_OP> filletOperations;
    bool                   operationPerformedOnAtLeastOne = false;
    bool                   didOneAttemptFail = false;
    std::set<PCB_TRACK*>   processedTracks;

    auto processFilletOp = [&]( PCB_TRACK* aTrack, bool aStartPoint )
    {
        std::shared_ptr<CONNECTIVITY_DATA> c = board()->GetConnectivity();
        VECTOR2I                           anchor = aStartPoint ? aTrack->GetStart() : aTrack->GetEnd();
        std::vector<BOARD_CONNECTED_ITEM*> itemsOnAnchor;

        itemsOnAnchor = c->GetConnectedItemsAtAnchor( aTrack, anchor, baseConnectedTypes );

        if( itemsOnAnchor.size() > 0 && selection.Contains( itemsOnAnchor.at( 0 ) )
            && itemsOnAnchor.at( 0 )->Type() == PCB_TRACE_T )
        {
            PCB_TRACK* trackOther = static_cast<PCB_TRACK*>( itemsOnAnchor.at( 0 ) );

            // Make sure we don't fillet the same pair of tracks twice
            if( processedTracks.find( trackOther ) == processedTracks.end() )
            {
                if( itemsOnAnchor.size() == 1 )
                {
                    FILLET_OP filletOp;
                    filletOp.t1 = aTrack;
                    filletOp.t2 = trackOther;
                    filletOp.t1Start = aStartPoint;
                    filletOp.t2Start = aTrack->IsPointOnEnds( filletOp.t2->GetStart() );
                    filletOperations.push_back( filletOp );
                }
                else
                {
                    // User requested to fillet these two tracks but not possible as
                    // there are other elements connected at that point
                    didOneAttemptFail = true;
                }
            }
        }
    };

    for( EDA_ITEM* item : selection )
    {
        if( item->Type() == PCB_TRACE_T )
        {
            PCB_TRACK* track = static_cast<PCB_TRACK*>( item );

            if( track->GetLength() > 0 )
            {
                processFilletOp( track, true );  // on the start point of track
                processFilletOp( track, false ); // on the end point of track

                processedTracks.insert( track );
            }
        }
    }

    BOARD_COMMIT             commit( this );
    std::vector<BOARD_ITEM*> itemsToAddToSelection;

    for( FILLET_OP filletOp : filletOperations )
    {
        PCB_TRACK* track1 = filletOp.t1;
        PCB_TRACK* track2 = filletOp.t2;

        bool trackOnStart = track1->IsPointOnEnds( track2->GetStart() );
        bool trackOnEnd = track1->IsPointOnEnds( track2->GetEnd() );

        if( trackOnStart && trackOnEnd )
            continue; // Ignore duplicate tracks

        if( ( trackOnStart || trackOnEnd ) && track1->GetLayer() == track2->GetLayer() )
        {
            SEG t1Seg( track1->GetStart(), track1->GetEnd() );
            SEG t2Seg( track2->GetStart(), track2->GetEnd() );

            if( t1Seg.ApproxCollinear( t2Seg ) )
                continue;

            SHAPE_ARC sArc( t1Seg, t2Seg, filletRadius );
            VECTOR2I  t1newPoint, t2newPoint;

            auto setIfPointOnSeg = []( VECTOR2I& aPointToSet, const SEG& aSegment, const VECTOR2I& aVecToTest )
            {
                VECTOR2I segToVec = aSegment.NearestPoint( aVecToTest ) - aVecToTest;

                // Find out if we are on the segment (minimum precision)
                if( segToVec.EuclideanNorm() < SHAPE_ARC::MIN_PRECISION_IU )
                {
                    aPointToSet.x = aVecToTest.x;
                    aPointToSet.y = aVecToTest.y;
                    return true;
                }

                return false;
            };

            //Do not draw a fillet if the end points of the arc are not within the track segments
            if( !setIfPointOnSeg( t1newPoint, t1Seg, sArc.GetP0() )
                && !setIfPointOnSeg( t2newPoint, t2Seg, sArc.GetP0() ) )
            {
                didOneAttemptFail = true;
                continue;
            }

            if( !setIfPointOnSeg( t1newPoint, t1Seg, sArc.GetP1() )
                && !setIfPointOnSeg( t2newPoint, t2Seg, sArc.GetP1() ) )
            {
                didOneAttemptFail = true;
                continue;
            }

            PCB_ARC* tArc = new PCB_ARC( frame()->GetBoard(), &sArc );
            tArc->SetLayer( track1->GetLayer() );
            tArc->SetWidth( track1->GetWidth() );
            tArc->SetNet( track1->GetNet() );
            tArc->SetLocked( track1->IsLocked() );
            tArc->SetHasSolderMask( track1->HasSolderMask() );
            tArc->SetLocalSolderMaskMargin( track1->GetLocalSolderMaskMargin() );
            commit.Add( tArc );
            itemsToAddToSelection.push_back( tArc );

            commit.Modify( track1 );
            commit.Modify( track2 );

            if( filletOp.t1Start )
                track1->SetStart( t1newPoint );
            else
                track1->SetEnd( t1newPoint );

            if( filletOp.t2Start )
                track2->SetStart( t2newPoint );
            else
                track2->SetEnd( t2newPoint );

            operationPerformedOnAtLeastOne = true;
        }
    }

    commit.Push( _( "Fillet Tracks" ) );

    //select the newly created arcs
    for( BOARD_ITEM* item : itemsToAddToSelection )
        m_selectionTool->AddItemToSel( item );

    if( !operationPerformedOnAtLeastOne )
        frame()->ShowInfoBarMsg( _( "Unable to fillet the selected track segments." ) );
    else if( didOneAttemptFail )
        frame()->ShowInfoBarMsg( _( "Some of the track segments could not be filleted." ) );

    return 0;
}


/**
 * Prompt the user for a radius and return it.
 *
 * @param aFrame
 * @param aTitle the title of the dialog
 * @param aPersitentRadius the last used radius
 * @return std::optional<int> the radius or std::nullopt if no
 * valid radius specified
 */
static std::optional<int> GetRadiusParams( PCB_BASE_EDIT_FRAME& aFrame, const wxString& aTitle, int& aPersitentRadius )
{
    WX_UNIT_ENTRY_DIALOG dlg( &aFrame, aTitle, _( "Radius:" ), aPersitentRadius );

    if( dlg.ShowModal() == wxID_CANCEL || dlg.GetValue() == 0 )
        return std::nullopt;

    aPersitentRadius = dlg.GetValue();

    return aPersitentRadius;
}


static std::optional<DOGBONE_CORNER_ROUTINE::PARAMETERS> GetDogboneParams( PCB_BASE_EDIT_FRAME& aFrame )
{
    // Persistent parameters
    static DOGBONE_CORNER_ROUTINE::PARAMETERS s_dogBoneParams{
        pcbIUScale.mmToIU( 1 ),
        true,
    };

    std::vector<WX_MULTI_ENTRY_DIALOG::ENTRY> entries{
        {
                _( "Arc radius:" ),
                WX_MULTI_ENTRY_DIALOG::UNIT_BOUND{ s_dogBoneParams.DogboneRadiusIU },
                wxEmptyString,
        },
        {
                _( "Add slots in acute corners" ),
                WX_MULTI_ENTRY_DIALOG::CHECKBOX{ s_dogBoneParams.AddSlots },
                _( "Add slots in acute corners to allow access to a cutter of the given radius" ),
        },
    };

    WX_MULTI_ENTRY_DIALOG dlg( &aFrame, _( "Dogbone Corner Settings" ), entries );

    if( dlg.ShowModal() == wxID_CANCEL )
        return std::nullopt;

    std::vector<WX_MULTI_ENTRY_DIALOG::RESULT> results = dlg.GetValues();
    wxCHECK( results.size() == 2, std::nullopt );

    try
    {
        s_dogBoneParams.DogboneRadiusIU = std::get<long long int>( results[0] );
        s_dogBoneParams.AddSlots = std::get<bool>( results[1] );
    }
    catch( const std::bad_variant_access& )
    {
        wxASSERT( false );
        return std::nullopt;
    }

    return s_dogBoneParams;
}

/**
 * Prompt the user for chamfer parameters
 *
 * @param aFrame
 * @param aErrorMsg filled with an error message if the parameter is invalid somehow
 * @return std::optional<int> the chamfer parameters or std::nullopt if no
 * valid fillet specified
 */
static std::optional<CHAMFER_PARAMS> GetChamferParams( PCB_BASE_EDIT_FRAME& aFrame )
{
    // Non-zero and the KLC default for Fab layer chamfers
    const int default_setback = pcbIUScale.mmToIU( 1 );
    // Store last used setback to allow pressing "enter" if repeat chamfer is required
    static CHAMFER_PARAMS params{ default_setback, default_setback };

    WX_UNIT_ENTRY_DIALOG dlg( &aFrame, _( "Chamfer Lines" ), _( "Chamfer setback:" ), params.m_chamfer_setback_a );

    if( dlg.ShowModal() == wxID_CANCEL || dlg.GetValue() == 0 )
        return std::nullopt;

    params.m_chamfer_setback_a = dlg.GetValue();
    // It's hard to easily specify an asymmetric chamfer (which line gets the longer setback?),
    // so we just use the same setback for each
    params.m_chamfer_setback_b = params.m_chamfer_setback_a;

    return params;
}


int EDIT_TOOL::ModifyLines( const TOOL_EVENT& aEvent )
{
    PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                std::vector<VECTOR2I> pts;

                // Iterate from the back so we don't have to worry about removals.
                for( int i = aCollector.GetCount() - 1; i >= 0; --i )
                {
                    BOARD_ITEM* item = aCollector[i];

                    // We've converted the polygon and rectangle to segments, so drop everything
                    // that isn't a segment at this point
                    if( !item->IsType(
                                { PCB_SHAPE_LOCATE_SEGMENT_T, PCB_SHAPE_LOCATE_POLY_T, PCB_SHAPE_LOCATE_RECT_T } ) )
                    {
                        aCollector.Remove( item );
                    }
                }

                sTool->FilterCollectorForLockedItems( aCollector );
            } );

    m_selectionTool->ReportFilteredLockedItems();

    std::set<PCB_SHAPE*>    lines_to_add;
    std::vector<PCB_SHAPE*> items_to_remove;

    for( EDA_ITEM* item : selection )
    {
        std::vector<VECTOR2I> pts;
        PCB_SHAPE*            graphic = static_cast<PCB_SHAPE*>( item );
        PCB_LAYER_ID          layer = graphic->GetLayer();
        int                   width = graphic->GetWidth();

        if( graphic->GetShape() == SHAPE_T::RECTANGLE )
        {
            items_to_remove.push_back( graphic );
            VECTOR2I start( graphic->GetStart() );
            VECTOR2I end( graphic->GetEnd() );
            pts.emplace_back( start );
            pts.emplace_back( VECTOR2I( end.x, start.y ) );
            pts.emplace_back( end );
            pts.emplace_back( VECTOR2I( start.x, end.y ) );
        }

        if( graphic->GetShape() == SHAPE_T::POLY )
        {
            items_to_remove.push_back( graphic );

            for( int jj = 0; jj < graphic->GetPolyShape().VertexCount(); ++jj )
                pts.emplace_back( graphic->GetPolyShape().CVertex( jj ) );
        }

        for( size_t jj = 1; jj < pts.size(); ++jj )
        {
            PCB_SHAPE* line = new PCB_SHAPE( frame()->GetModel(), SHAPE_T::SEGMENT );

            line->SetStart( pts[jj - 1] );
            line->SetEnd( pts[jj] );
            line->SetWidth( width );
            line->SetLayer( layer );
            lines_to_add.insert( line );
        }

        if( pts.size() > 1 )
        {
            PCB_SHAPE* line = new PCB_SHAPE( frame()->GetModel(), SHAPE_T::SEGMENT );

            line->SetStart( pts.back() );
            line->SetEnd( pts.front() );
            line->SetWidth( width );
            line->SetLayer( layer );
            lines_to_add.insert( line );
        }
    }

    int segmentCount = selection.CountType( PCB_SHAPE_LOCATE_SEGMENT_T ) + lines_to_add.size();

    if( aEvent.IsAction( &PCB_ACTIONS::extendLines ) && segmentCount != 2 )
    {
        frame()->ShowInfoBarMsg( _( "Exactly two lines must be selected to extend them." ) );

        for( PCB_SHAPE* line : lines_to_add )
            delete line;

        return 0;
    }
    else if( segmentCount < 2 )
    {
        frame()->ShowInfoBarMsg( _( "A shape with at least two lines must be selected." ) );

        for( PCB_SHAPE* line : lines_to_add )
            delete line;

        return 0;
    }

    BOARD_COMMIT commit( this );

    // Items created like lines from a rectangle
    for( PCB_SHAPE* item : lines_to_add )
    {
        commit.Add( item );
        selection.Add( item );
    }

    // Remove items like rectangles that we decomposed into lines
    for( PCB_SHAPE* item : items_to_remove )
    {
        selection.Remove( item );
        commit.Remove( item );
    }

    for( EDA_ITEM* item : selection )
        item->ClearFlags( STRUCT_DELETED );

    // List of thing to select at the end of the operation
    // (doing it as we go will invalidate the iterator)
    std::vector<BOARD_ITEM*> items_to_select_on_success;

    // And same for items to deselect
    std::vector<BOARD_ITEM*> items_to_deselect_on_success;

    // Handle modifications to existing items by the routine
    // How to deal with this depends on whether we're in the footprint editor or not
    // and whether the item was conjured up by decomposing a polygon or rectangle
    auto item_modification_handler = [&]( BOARD_ITEM& aItem )
    {
        // If the item was "conjured up" it will be added later separately
        if( !alg::contains( lines_to_add, &aItem ) )
        {
            commit.Modify( &aItem );
            items_to_select_on_success.push_back( &aItem );
        }
    };

    bool any_items_created = !lines_to_add.empty();
    auto item_creation_handler = [&]( std::unique_ptr<BOARD_ITEM> aItem )
    {
        any_items_created = true;
        items_to_select_on_success.push_back( aItem.get() );
        commit.Add( aItem.release() );
    };

    bool any_items_removed = !items_to_remove.empty();
    auto item_removal_handler = [&]( BOARD_ITEM& aItem )
    {
        aItem.SetFlags( STRUCT_DELETED );
        any_items_removed = true;
        items_to_deselect_on_success.push_back( &aItem );
        commit.Remove( &aItem );
    };

    // Combine these callbacks into a CHANGE_HANDLER to inject in the ROUTINE
    ITEM_MODIFICATION_ROUTINE::CALLABLE_BASED_HANDLER change_handler( item_creation_handler, item_modification_handler,
                                                                      item_removal_handler );

    // Construct an appropriate tool
    std::unique_ptr<PAIRWISE_LINE_ROUTINE> pairwise_line_routine;

    if( aEvent.IsAction( &PCB_ACTIONS::filletLines ) )
    {
        static int         s_filletRadius = pcbIUScale.mmToIU( 1 );
        std::optional<int> filletRadiusIU = GetRadiusParams( *frame(), _( "Fillet Lines" ), s_filletRadius );

        if( filletRadiusIU.has_value() )
        {
            pairwise_line_routine =
                    std::make_unique<LINE_FILLET_ROUTINE>( frame()->GetModel(), change_handler, *filletRadiusIU );
        }
    }
    else if( aEvent.IsAction( &PCB_ACTIONS::dogboneCorners ) )
    {
        std::optional<DOGBONE_CORNER_ROUTINE::PARAMETERS> dogboneParams = GetDogboneParams( *frame() );

        if( dogboneParams.has_value() )
        {
            pairwise_line_routine =
                    std::make_unique<DOGBONE_CORNER_ROUTINE>( frame()->GetModel(), change_handler, *dogboneParams );
        }
    }
    else if( aEvent.IsAction( &PCB_ACTIONS::chamferLines ) )
    {
        std::optional<CHAMFER_PARAMS> chamfer_params = GetChamferParams( *frame() );

        if( chamfer_params.has_value() )
        {
            pairwise_line_routine =
                    std::make_unique<LINE_CHAMFER_ROUTINE>( frame()->GetModel(), change_handler, *chamfer_params );
        }
    }
    else if( aEvent.IsAction( &PCB_ACTIONS::extendLines ) )
    {
        pairwise_line_routine = std::make_unique<LINE_EXTENSION_ROUTINE>( frame()->GetModel(), change_handler );
    }

    if( !pairwise_line_routine )
    {
        // Didn't construct any mofication routine - user must have cancelled
        commit.Revert();
        return 0;
    }

    // Apply the tool to every line pair
    alg::for_all_pairs( selection.begin(), selection.end(),
                        [&]( EDA_ITEM* a, EDA_ITEM* b )
                        {
                            if( ( a->GetFlags() & STRUCT_DELETED ) == 0 && ( b->GetFlags() & STRUCT_DELETED ) == 0 )
                            {
                                PCB_SHAPE* line_a = static_cast<PCB_SHAPE*>( a );
                                PCB_SHAPE* line_b = static_cast<PCB_SHAPE*>( b );

                                pairwise_line_routine->ProcessLinePair( *line_a, *line_b );
                            }
                        } );

    // Select added and modified items
    for( BOARD_ITEM* item : items_to_select_on_success )
        m_selectionTool->AddItemToSel( item, true );

    // Deselect removed items
    for( BOARD_ITEM* item : items_to_deselect_on_success )
        m_selectionTool->RemoveItemFromSel( item, true );

    if( any_items_removed )
        m_toolMgr->ProcessEvent( EVENTS::UnselectedEvent );

    if( any_items_created )
        m_toolMgr->ProcessEvent( EVENTS::SelectedEvent );

    // Notify other tools of the changes
    m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );

    commit.Push( pairwise_line_routine->GetCommitDescription() );

    if( const std::optional<wxString> msg = pairwise_line_routine->GetStatusMessage( segmentCount ) )
        frame()->ShowInfoBarMsg( *msg );

    return 0;
}


int EDIT_TOOL::SimplifyPolygons( const TOOL_EVENT& aEvent )
{
    PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                std::vector<VECTOR2I> pts;

                // Iterate from the back so we don't have to worry about removals.
                for( int i = aCollector.GetCount() - 1; i >= 0; --i )
                {
                    BOARD_ITEM* item = aCollector[i];

                    if( !item->IsType( { PCB_SHAPE_LOCATE_POLY_T, PCB_ZONE_T } ) )
                        aCollector.Remove( item );

                    if( ZONE* zone = dyn_cast<ZONE*>( item ) )
                    {
                        if( zone->IsTeardropArea() )
                            aCollector.Remove( item );
                    }
                }

                sTool->FilterCollectorForLockedItems( aCollector );
            } );

    m_selectionTool->ReportFilteredLockedItems();

    // Store last used value
    static int s_toleranceValue = pcbIUScale.mmToIU( 3 );

    WX_UNIT_ENTRY_DIALOG dlg( frame(), _( "Simplify Shapes" ), _( "Tolerance value:" ), s_toleranceValue );

    if( dlg.ShowModal() == wxID_CANCEL )
        return 0;

    s_toleranceValue = dlg.GetValue();

    if( s_toleranceValue <= 0 )
        return 0;

    BOARD_COMMIT commit{ this };

    std::vector<PCB_SHAPE*> shapeList;

    for( EDA_ITEM* item : selection )
    {
        commit.Modify( item );

        if( PCB_SHAPE* shape = dyn_cast<PCB_SHAPE*>( item ) )
        {
            SHAPE_POLY_SET& poly = shape->GetPolyShape();

            poly.SimplifyOutlines( s_toleranceValue );
        }

        if( ZONE* zone = dyn_cast<ZONE*>( item ) )
        {
            SHAPE_POLY_SET* poly = zone->Outline();

            poly->SimplifyOutlines( s_toleranceValue );
            zone->HatchBorder();
        }
    }

    commit.Push( _( "Simplify Polygons" ) );

    // Notify other tools of the changes
    m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );

    return 0;
}


int EDIT_TOOL::HealShapes( const TOOL_EVENT& aEvent )
{
    PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                std::vector<VECTOR2I> pts;

                // Iterate from the back so we don't have to worry about removals.
                for( int i = aCollector.GetCount() - 1; i >= 0; --i )
                {
                    BOARD_ITEM* item = aCollector[i];

                    // We've converted the polygon and rectangle to segments, so drop everything
                    // that isn't a segment at this point
                    if( !item->IsType(
                                { PCB_SHAPE_LOCATE_SEGMENT_T, PCB_SHAPE_LOCATE_ARC_T, PCB_SHAPE_LOCATE_BEZIER_T } ) )
                    {
                        aCollector.Remove( item );
                    }
                }

                sTool->FilterCollectorForLockedItems( aCollector );
            } );

    m_selectionTool->ReportFilteredLockedItems();

    // Store last used value
    static int s_toleranceValue = pcbIUScale.mmToIU( 3 );

    WX_UNIT_ENTRY_DIALOG dlg( frame(), _( "Heal Shapes" ), _( "Tolerance value:" ), s_toleranceValue );

    if( dlg.ShowModal() == wxID_CANCEL )
        return 0;

    s_toleranceValue = dlg.GetValue();

    if( s_toleranceValue <= 0 )
        return 0;

    BOARD_COMMIT commit{ this };

    std::vector<PCB_SHAPE*> shapeList;

    for( EDA_ITEM* item : selection )
    {
        if( PCB_SHAPE* shape = dynamic_cast<PCB_SHAPE*>( item ) )
        {
            shapeList.push_back( shape );
            commit.Modify( shape );
        }
    }

    ConnectBoardShapes( shapeList, s_toleranceValue );

    commit.Push( _( "Heal Shapes" ) );

    // Notify other tools of the changes
    m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );

    return 0;
}


int EDIT_TOOL::BooleanPolygons( const TOOL_EVENT& aEvent )
{
    PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                // Iterate from the back so we don't have to worry about removals.
                for( int i = aCollector.GetCount() - 1; i >= 0; --i )
                {
                    BOARD_ITEM* item = aCollector[i];

                    static const std::vector<KICAD_T> polygonBooleanTypes = {
                        PCB_SHAPE_LOCATE_POLY_T,
                        PCB_SHAPE_LOCATE_RECT_T,
                        PCB_SHAPE_LOCATE_CIRCLE_T,
                    };

                    if( !item->IsType( polygonBooleanTypes ) )
                        aCollector.Remove( item );
                }

                sTool->FilterCollectorForLockedItems( aCollector );
            } );

    m_selectionTool->ReportFilteredLockedItems();

    const EDA_ITEM* const last_item = selection.GetLastAddedItem();

    // Gather or construct polygon source shapes to merge
    std::vector<PCB_SHAPE*> items_to_process;

    for( EDA_ITEM* item : selection )
    {
        items_to_process.push_back( static_cast<PCB_SHAPE*>( item ) );

        // put the last one in the selection at the front of the vector
        // so it can be used as the property donor and as the basis for the
        // boolean operation
        if( item == last_item )
            std::swap( items_to_process.back(), items_to_process.front() );
    }

    BOARD_COMMIT commit{ this };

    // Handle modifications to existing items by the routine
    auto item_modification_handler = [&]( BOARD_ITEM& aItem )
    {
        commit.Modify( &aItem );
    };

    std::vector<BOARD_ITEM*> items_to_select_on_success;

    auto item_creation_handler = [&]( std::unique_ptr<BOARD_ITEM> aItem )
    {
        items_to_select_on_success.push_back( aItem.get() );
        commit.Add( aItem.release() );
    };

    auto item_removal_handler = [&]( BOARD_ITEM& aItem )
    {
        commit.Remove( &aItem );
    };

    // Combine these callbacks into a CHANGE_HANDLER to inject in the ROUTINE
    ITEM_MODIFICATION_ROUTINE::CALLABLE_BASED_HANDLER change_handler( item_creation_handler, item_modification_handler,
                                                                      item_removal_handler );

    // Construct an appropriate routine
    std::unique_ptr<POLYGON_BOOLEAN_ROUTINE> boolean_routine;

    const auto create_routine = [&]() -> std::unique_ptr<POLYGON_BOOLEAN_ROUTINE>
    {
        // (Re-)construct the boolean routine based on the action
        // This is done here so that we can re-init the routine if we need to
        // go again in the reverse order.

        BOARD_ITEM_CONTAINER* const model = frame()->GetModel();
        wxCHECK( model, nullptr );

        if( aEvent.IsAction( &PCB_ACTIONS::mergePolygons ) )
        {
            return std::make_unique<POLYGON_MERGE_ROUTINE>( model, change_handler );
        }
        else if( aEvent.IsAction( &PCB_ACTIONS::subtractPolygons ) )
        {
            return std::make_unique<POLYGON_SUBTRACT_ROUTINE>( model, change_handler );
        }
        else if( aEvent.IsAction( &PCB_ACTIONS::intersectPolygons ) )
        {
            return std::make_unique<POLYGON_INTERSECT_ROUTINE>( model, change_handler );
        }
        return nullptr;
    };

    const auto run_routine = [&]()
    {
        // Perform the operation on each polygon
        for( PCB_SHAPE* shape : items_to_process )
            boolean_routine->ProcessShape( *shape );

        boolean_routine->Finalize();
    };

    boolean_routine = create_routine();

    wxCHECK_MSG( boolean_routine, 0, "Could not find a polygon routine for this action" );

    // First run the routine and see what we get
    run_routine();

    // If we are doing a non-commutative operation (e.g. subtract), and we just got null,
    // assume the user meant go in a different opposite order
    if( !boolean_routine->IsCommutative() && items_to_select_on_success.empty() )
    {
        // Clear the commit and the selection
        commit.Revert();
        items_to_select_on_success.clear();

        std::map<const PCB_SHAPE*, VECTOR2I::extended_type> items_area;

        for( PCB_SHAPE* shape : items_to_process )
        {
            VECTOR2I::extended_type area = shape->GetBoundingBox().GetArea();
            items_area[shape] = area;
        }

        // Sort the shapes by their bounding box area in descending order
        // This way we will start with the largest shape first and subtract the smaller ones
        // This may not work perfectly in all cases, but it works well when the larger
        // shape completely contains the smaller ones, which is probably the most common case.
        // In other cases, the user will need to select the shapes in the correct order (i.e.
        // the largest shape last), or do the subtractions in multiple steps.
        std::sort( items_to_process.begin(), items_to_process.end(),
                   [&]( const PCB_SHAPE* a, const PCB_SHAPE* b )
                   {
                       return items_area[a] > items_area[b];
                   } );

        // Run the routine again
        boolean_routine = create_routine();
        run_routine();
    }

    // Select new items
    for( BOARD_ITEM* item : items_to_select_on_success )
        m_selectionTool->AddItemToSel( item, true );

    // Notify other tools of the changes
    m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );

    commit.Push( boolean_routine->GetCommitDescription() );

    if( const std::optional<wxString> msg = boolean_routine->GetStatusMessage() )
        frame()->ShowInfoBarMsg( *msg );

    return 0;
}


int EDIT_TOOL::MatchProperties( const TOOL_EVENT& aEvent )
{
    BOARD_ITEM* source = nullptr;

    /*
     * First check whether the user has already selected a valid source.
     *
     * Do not use RequestSelection() here because, when nothing is selected,
     * it immediately tries to use the item currently under the cursor.
     * MATCHPROP must instead wait for an explicit source click.
     */
    PCB_SELECTION& initialSelection = m_selectionTool->GetSelection();

    if( initialSelection.GetSize() == 1 )
    {
        BOARD_ITEM* selectedItem = dynamic_cast<BOARD_ITEM*>( initialSelection.Front() );

        if( selectedItem && selectedItem->IsType( { PCB_TRACE_T, PCB_ARC_T, PCB_VIA_T } ) )
        {
            source = selectedItem;
        }
        else
        {
            frame()->ShowInfoBarMsg( _( "MATCHPROP: The selected source must be a track or via." ) );

            return 0;
        }
    }
    else if( initialSelection.GetSize() > 1 )
    {
        frame()->ShowInfoBarMsg( _( "MATCHPROP: Select exactly one source track or via." ) );

        return 0;
    }

    TOOL_EVENT pushedEvent = aEvent;

    frame()->PushTool( pushedEvent );
    Activate();

    PCB_BASE_EDIT_FRAME* editFrame = getEditFrame<PCB_BASE_EDIT_FRAME>();

    if( !editFrame )
    {
        frame()->PopTool( pushedEvent );
        return 0;
    }

    canvas()->SetCurrentCursor( KICURSOR::PLACE );

    /*
     * Find a selectable track, arc or via at a clicked point.
     *
     * This is used when MATCHPROP is started before selecting a source.
     */
    auto findSourceAtPoint = [&]( const VECTOR2I& aPoint ) -> BOARD_ITEM*
    {
        int hitTolerance = KiROUND( getView()->ToWorld( 8 ) );

        BOX2I searchBox( aPoint - VECTOR2I( hitTolerance, hitTolerance ),
                         VECTOR2I( hitTolerance * 2, hitTolerance * 2 ) );

        std::vector<KIGFX::VIEW::LAYER_ITEM_PAIR> candidates;

        getView()->Query( searchBox, candidates );

        for( const auto& [viewItem, layer] : candidates )
        {
            if( !viewItem->IsBOARD_ITEM() )
                continue;

            BOARD_ITEM* candidate = static_cast<BOARD_ITEM*>( viewItem );

            if( !candidate->IsType( { PCB_TRACE_T, PCB_ARC_T, PCB_VIA_T } ) )
            {
                continue;
            }

            if( !m_selectionTool->Selectable( candidate ) )
                continue;

            if( !candidate->HitTest( aPoint, hitTolerance ) )
                continue;

            return candidate;
        }

        return nullptr;
    };

    /*
     * If no source was preselected, wait for the user to click one.
     */
    if( !source )
    {
        m_statusPopup->SetText( _( "MATCHPROP: Click source track or via - Esc to cancel" ) );

        m_statusPopup->Move( KIPLATFORM::UI::GetMousePosition() + wxPoint( 20, -50 ) );

        m_statusPopup->Popup();

        canvas()->SetStatusPopup( m_statusPopup->GetPanel() );

        bool sourceCancelled = false;

        while( TOOL_EVENT* event = Wait() )
        {
            if( event->IsCancelInteractive() || event->IsActivate() )
            {
                sourceCancelled = true;

                event->SetPassEvent( false );
                break;
            }
            else if( event->IsClick( BUT_LEFT ) || event->IsAction( &ACTIONS::cursorClick ) )
            {
                source = findSourceAtPoint( event->Position() );

                if( source )
                {
                    event->SetPassEvent( false );
                    break;
                }

                frame()->SetStatusText( _( "MATCHPROP: No track or via at the clicked point." ) );

                event->SetPassEvent( false );
            }
            else if( event->IsMotion() )
            {
                m_statusPopup->Move( KIPLATFORM::UI::GetMousePosition() + wxPoint( 20, -50 ) );

                event->SetPassEvent();
            }
            else
            {
                event->SetPassEvent();
            }
        }

        if( sourceCancelled || !source )
        {
            canvas()->SetStatusPopup( nullptr );
            m_statusPopup->Hide();
            canvas()->SetCurrentCursor( KICURSOR::ARROW );

            frame()->PopTool( pushedEvent );

            frame()->SetStatusText( _( "MATCHPROP cancelled" ) );

            return 0;
        }
    }

    /*
     * Store the source properties before clearing any existing selection.
     */
    const KICAD_T sourceType = source->Type();

    int     sourceWidth = 0;
    int     sourceDrill = 0;
    VIATYPE sourceViaType = VIATYPE::THROUGH;

    if( sourceType == PCB_TRACE_T || sourceType == PCB_ARC_T )
    {
        PCB_TRACK* sourceTrack = static_cast<PCB_TRACK*>( source );

        sourceWidth = sourceTrack->GetWidth();
    }
    else if( sourceType == PCB_VIA_T )
    {
        PCB_VIA* sourceVia = static_cast<PCB_VIA*>( source );

        sourceWidth = sourceVia->GetWidth( PADSTACK::ALL_LAYERS );

        sourceDrill = sourceVia->GetDrillValue();

        sourceViaType = sourceVia->GetViaType();
    }

    /*
     * The source properties are now stored, so clear the source selection
     * before choosing destination items.
     */
    m_toolMgr->RunAction( ACTIONS::selectionClear );

    BOARD_COMMIT commit( this );

    int  matchedCount = 0;
    bool cancelled = false;

    wxString prompt;

    if( sourceType == PCB_VIA_T )
    {
        prompt = _( "MATCHPROP: Click or drag-select destination vias - "
                    "Esc to finish" );
    }
    else
    {
        prompt = _( "MATCHPROP: Click or drag-select destination tracks - "
                    "Esc to finish" );
    }

    m_statusPopup->SetText( prompt );
    m_statusPopup->Move( KIPLATFORM::UI::GetMousePosition() + wxPoint( 20, -50 ) );
    m_statusPopup->Popup();
    canvas()->SetStatusPopup( m_statusPopup->GetPanel() );
    canvas()->SetCurrentCursor( KICURSOR::PLACE );

    /*
     * Selection rectangle used for window and crossing selections.
     *
     * It is handled inside this tool's event loop so that another
     * nested Wait() call is not required.
     */
    KIGFX::PREVIEW::SELECTION_AREA selectionArea;
    getView()->Add( &selectionArea );
    getView()->SetVisible( &selectionArea, false );
    bool draggingSelection = false;

    /*
     * Apply the source properties to one compatible destination.
     *
     * Returns true only when the destination was actually changed.
     */
    auto applyToTarget = [&]( BOARD_ITEM* aTarget ) -> bool
    {
        if( !aTarget )
            return false;

        /*
                 * Do not apply properties back to the original source.
                 */
        if( aTarget == source )
            return false;

        if( !m_selectionTool->Selectable( aTarget ) )
            return false;

        bool changed = false;

        /*
                 * VIA source to VIA destination.
                 */
        if( sourceType == PCB_VIA_T && aTarget->Type() == PCB_VIA_T )
        {
            PCB_VIA* targetVia = static_cast<PCB_VIA*>( aTarget );
            int      targetWidth = targetVia->GetWidth( PADSTACK::ALL_LAYERS );
            int      targetDrill = targetVia->GetDrillValue();
            VIATYPE  targetViaType = targetVia->GetViaType();

            if( targetWidth != sourceWidth || targetDrill != sourceDrill || targetViaType != sourceViaType )
            {
                commit.Modify( targetVia );
                targetVia->SetWidth( PADSTACK::ALL_LAYERS, sourceWidth );
                targetVia->SetDrill( sourceDrill );
                targetVia->SetViaType( sourceViaType );
                changed = true;
            }
        }
        /*
                 * Track or track-arc source to track or track-arc
                 * destination.
                 */
        else if( ( sourceType == PCB_TRACE_T || sourceType == PCB_ARC_T )
                 && ( aTarget->Type() == PCB_TRACE_T || aTarget->Type() == PCB_ARC_T ) )
        {
            PCB_TRACK* targetTrack = static_cast<PCB_TRACK*>( aTarget );

            if( targetTrack->GetWidth() != sourceWidth )
            {
                commit.Modify( targetTrack );
                targetTrack->SetWidth( sourceWidth );
                changed = true;
            }
        }

        if( changed )
        {
            matchedCount++;
            getView()->Update( aTarget );
            m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );
        }

        return changed;
    };

    /*
     * Find and apply MATCHPROP to one compatible item at a clicked point.
     */
    auto applyAtPoint = [&]( const VECTOR2I& aPoint ) -> bool
    {
        int   hitTolerance = KiROUND( getView()->ToWorld( 8 ) );
        BOX2I searchBox( aPoint - VECTOR2I( hitTolerance, hitTolerance ),
                         VECTOR2I( hitTolerance * 2, hitTolerance * 2 ) );

        std::vector<KIGFX::VIEW::LAYER_ITEM_PAIR> candidates;
        getView()->Query( searchBox, candidates );

        BOARD_ITEM* target = nullptr;

        for( const auto& [viewItem, layer] : candidates )
        {
            if( !viewItem->IsBOARD_ITEM() )
                continue;

            BOARD_ITEM* candidate = static_cast<BOARD_ITEM*>( viewItem );

            if( candidate == source )
                continue;

            if( !m_selectionTool->Selectable( candidate ) )
                continue;

            bool compatible = false;

            if( sourceType == PCB_VIA_T )
            {
                compatible = candidate->Type() == PCB_VIA_T;
            }
            else
            {
                compatible = candidate->Type() == PCB_TRACE_T || candidate->Type() == PCB_ARC_T;
            }

            if( !compatible )
                continue;

            if( !candidate->HitTest( aPoint, hitTolerance ) )
            {
                continue;
            }

            target = candidate;
            break;
        }

        if( !target )
        {
            frame()->SetStatusText( _( "MATCHPROP: No compatible item at the clicked point." ) );
            return false;
        }

        bool changed = applyToTarget( target );

        if( changed )
        {
            frame()->SetStatusText( wxString::Format( _( "MATCHPROP: %d item(s) changed. "
                                                         "Click or drag-select more targets, "
                                                         "or press Esc." ),
                                                      matchedCount ) );
        }
        else
        {
            frame()->SetStatusText( _( "MATCHPROP: Destination already has "
                                       "matching properties." ) );
        }

        return changed;
    };

    /*
     * Apply MATCHPROP to all compatible objects currently selected
     * by a window or crossing selection.
     */
    auto applyToCurrentSelection = [&]()
    {
        PCB_SELECTION& targets = m_selectionTool->GetSelection();

        /*
         * Copy the item pointers before clearing the selection.
         */
        std::vector<BOARD_ITEM*> selectedTargets;
        selectedTargets.reserve( targets.GetSize() );

        for( EDA_ITEM* item : targets )
        {
            if( !item->IsBOARD_ITEM() )
                continue;

            selectedTargets.push_back( static_cast<BOARD_ITEM*>( item ) );
        }

        int changedByWindow = 0;

        for( BOARD_ITEM* target : selectedTargets )
        {
            if( applyToTarget( target ) )
                changedByWindow++;
        }

        /*
         * Destination selections are temporary. Clear them so the
         * user can immediately click or drag-select again.
         */
        m_toolMgr->RunAction( ACTIONS::selectionClear );

        if( changedByWindow > 0 )
        {
            frame()->SetStatusText( wxString::Format( _( "MATCHPROP: %d item(s) changed; "
                                                         "%d changed by the last selection. "
                                                         "Select more targets or press Esc." ),
                                                      matchedCount, changedByWindow ) );
        }
        else
        {
            frame()->SetStatusText( _( "MATCHPROP: No compatible items in the selection, "
                                       "or their properties already matched." ) );
        }
    };

    /*
     * MATCHPROP interactive destination loop.
     *
     * The user can alternate between individual clicks and
     * window/crossing selections until Esc is pressed.
     */
    while( TOOL_EVENT* event = Wait() )
    {
        if( event->IsCancelInteractive() || event->IsActivate() )
        {
            cancelled = true;

            event->SetPassEvent( false );
            break;
        }
        else if( event->IsDrag( BUT_LEFT ) )
        {
            draggingSelection = true;
            const VECTOR2I origin = event->DragOrigin();
            const VECTOR2I end = event->Position();

            /*
             * Left-to-right:
             *     Only objects fully contained by the rectangle.
             *
             * Right-to-left:
             *     Objects touched by the rectangle.
             */
            bool crossingSelection = end.x < origin.x;

            if( getView()->IsMirroredX() )
                crossingSelection = !crossingSelection;

            selectionArea.SetOrigin( origin );
            selectionArea.SetEnd( end );
            selectionArea.SetAdditive( false );
            selectionArea.SetSubtractive( false );
            selectionArea.SetExclusiveOr( false );
            selectionArea.SetMode( crossingSelection ? SELECTION_MODE::TOUCHING_RECTANGLE
                                                     : SELECTION_MODE::INSIDE_RECTANGLE );

            getView()->SetVisible( &selectionArea, true );
            getView()->Update( &selectionArea );
            getViewControls()->SetAutoPan( true );
            canvas()->SetCurrentCursor( crossingSelection ? KICURSOR::SELECT_LASSO : KICURSOR::SELECT_WINDOW );
            event->SetPassEvent( false );
        }
        else if( event->IsMouseUp( BUT_LEFT ) && draggingSelection )
        {
            getViewControls()->SetAutoPan( false );
            getView()->SetVisible( &selectionArea, false );

            /*
             * Process the completed rectangle without starting
             * another Wait() loop.
             */
            m_selectionTool->SelectMultiple( selectionArea, false, false );
            applyToCurrentSelection();
            draggingSelection = false;
            canvas()->SetCurrentCursor( KICURSOR::PLACE );
            event->SetPassEvent( false );
        }
        else if( event->IsClick( BUT_LEFT ) || event->IsAction( &ACTIONS::cursorClick ) )
        {
            applyAtPoint( event->Position() );
            event->SetPassEvent( false );
        }
        else if( event->IsMotion() )
        {
            m_statusPopup->Move( KIPLATFORM::UI::GetMousePosition() + wxPoint( 20, -50 ) );
            event->SetPassEvent();
        }
        else
        {
            /*
             * Allow zooming, panning and other navigation events
             * to continue working.
             */
            event->SetPassEvent();
        }
    }

    /*
     * Clear any temporary destination selection.
     */
    m_toolMgr->RunAction( ACTIONS::selectionClear );

    /*
     * Clean up selection rectangle and temporary UI state.
     */
    getViewControls()->SetAutoPan( false );
    getView()->SetVisible( &selectionArea, false );
    getView()->Remove( &selectionArea );
    canvas()->SetStatusPopup( nullptr );
    m_statusPopup->Hide();
    canvas()->SetCurrentCursor( KICURSOR::ARROW );

    /*
     * All changes made during this MATCHPROP session are grouped
     * into one Undo operation.
     */
    if( !commit.Empty() )
        commit.Push( _( "Match Properties" ) );

    frame()->PopTool( pushedEvent );

    if( matchedCount > 0 )
    {
        frame()->SetStatusText( wxString::Format( _( "MATCHPROP finished: %d item(s) changed." ), matchedCount ) );
    }
    else if( cancelled )
    {
        frame()->SetStatusText( _( "MATCHPROP cancelled" ) );
    }
    else
    {
        frame()->SetStatusText( _( "MATCHPROP finished: No items changed." ) );
    }

    return 0;
}


int EDIT_TOOL::ExtendTrack( const TOOL_EVENT& aEvent )
{
    TOOL_EVENT pushedEvent = aEvent;

    frame()->PushTool( pushedEvent );
    Activate();

    PCB_BASE_EDIT_FRAME* editFrame = getEditFrame<PCB_BASE_EDIT_FRAME>();

    if( !editFrame )
    {
        frame()->PopTool( pushedEvent );
        return 0;
    }

    BOARD* board = editFrame->GetBoard();

    if( !board )
    {
        frame()->PopTool( pushedEvent );
        return 0;
    }

    /*
     * If EXTEND was invoked from the right-click context menu with
     * exactly one straight track selected, remember that track.
     *
     * It will be extended immediately instead of requiring the user
     * to click it for a second time.
     */
    PCB_TRACK*     initiallySelectedTrack = nullptr;
    PCB_SELECTION& initialSelection = m_selectionTool->GetSelection();

    if( initialSelection.GetSize() == 1 )
    {
        EDA_ITEM* selectedItem = initialSelection.Front();

        if( selectedItem && selectedItem->Type() == PCB_TRACE_T )
        {
            PCB_TRACK* selectedTrack = static_cast<PCB_TRACK*>( selectedItem );

            if( !selectedTrack->IsLocked() && m_selectionTool->Selectable( selectedTrack ) )
            {
                initiallySelectedTrack = selectedTrack;
            }
        }
    }

    /*
     * Remember the current cursor position before clearing the
     * selection. This position determines which endpoint of an
     * initially selected track is extended.
     */
    const VECTOR2I initialCursorPosition = VECTOR2I( getViewControls()->GetCursorPosition( false ) );

    /*
     * Destination selections are not needed while EXTEND is active.
     *
     * Any initially selected source track has already been stored
     * above.
     */
    m_toolMgr->RunAction( ACTIONS::selectionClear );

    BOARD_COMMIT commit( this );
    int          extendedCount = 0;
    bool         cancelled = false;

    m_statusPopup->SetText( _( "EXTEND: Click near the end of a straight track - "
                               "Esc to finish" ) );

    m_statusPopup->Move( KIPLATFORM::UI::GetMousePosition() + wxPoint( 20, -50 ) );
    m_statusPopup->Popup();
    canvas()->SetStatusPopup( m_statusPopup->GetPanel() );
    canvas()->SetCurrentCursor( KICURSOR::PLACE );

    /*
     * Calculate the intersection between an infinite forward ray
     * and a finite line segment.
     *
     * Ray:
     *
     *     aRayOrigin + t * aRayDirection
     *
     * where t must be greater than zero.
     *
     * Segment:
     *
     *     aSegmentStart + u * ( aSegmentEnd - aSegmentStart )
     *
     * where u must be between zero and one.
     */
    auto findRaySegmentIntersection = []( const VECTOR2I& aRayOrigin, const VECTOR2I& aRayDirection,
                                          const VECTOR2I& aSegmentStart, const VECTOR2I& aSegmentEnd,
                                          VECTOR2I& aIntersection, long double& aRayParameter ) -> bool
    {
        const long double rayX = static_cast<long double>( aRayDirection.x );
        const long double rayY = static_cast<long double>( aRayDirection.y );

        const long double segmentX =
                static_cast<long double>( aSegmentEnd.x ) - static_cast<long double>( aSegmentStart.x );

        const long double segmentY =
                static_cast<long double>( aSegmentEnd.y ) - static_cast<long double>( aSegmentStart.y );

        /*
         * Two-dimensional cross product:
         *
         *     rayDirection x segmentDirection
         */
        const long double denominator = rayX * segmentY - rayY * segmentX;

        /*
         * Parallel and collinear objects are ignored.
         */
        constexpr long double parallelTolerance = 1.0e-18L;

        if( std::fabs( denominator ) < parallelTolerance )
        {
            return false;
        }

        const long double deltaX =
                static_cast<long double>( aSegmentStart.x ) - static_cast<long double>( aRayOrigin.x );

        const long double deltaY =
                static_cast<long double>( aSegmentStart.y ) - static_cast<long double>( aRayOrigin.y );

        /*
         * t is the position along the extension ray.
         *
         * u is the position along the target segment.
         */
        const long double t = ( deltaX * segmentY - deltaY * segmentX ) / denominator;
        const long double u = ( deltaX * rayY - deltaY * rayX ) / denominator;

        /*
         * The intersection must be in front of the selected track
         * endpoint. A very small tolerance prevents the current
         * endpoint itself being treated as the extension target.
         */
        constexpr long double forwardTolerance = 1.0e-12L;
        constexpr long double segmentTolerance = 1.0e-12L;

        if( t <= forwardTolerance )
            return false;

        if( u < -segmentTolerance || u > 1.0L + segmentTolerance )
        {
            return false;
        }

        const long double intersectionX = static_cast<long double>( aRayOrigin.x ) + t * rayX;
        const long double intersectionY = static_cast<long double>( aRayOrigin.y ) + t * rayY;
        aIntersection.x = static_cast<int>( std::llround( intersectionX ) );
        aIntersection.y = static_cast<int>( std::llround( intersectionY ) );
        aRayParameter = t;
        return true;
    };

    /*
     * Find one straight track at the clicked position.
     *
     * The clicked point is also retained because it determines
     * which endpoint of the track is extended.
     */
    auto findTrackAtPoint = [&]( const VECTOR2I& aPoint ) -> PCB_TRACK*
    {
        const int                                 hitTolerance = KiROUND( getView()->ToWorld( 8 ) );
        BOX2I                                     searchBox( aPoint - VECTOR2I( hitTolerance, hitTolerance ),
                                                             VECTOR2I( hitTolerance * 2, hitTolerance * 2 ) );
        std::vector<KIGFX::VIEW::LAYER_ITEM_PAIR> candidates;
        getView()->Query( searchBox, candidates );
        PCB_TRACK* selectedTrack = nullptr;

        /*
         * Prefer the track whose centreline is closest to the click.
         *
         * The view query can return several items where tracks
         * overlap or cross.
         */
        long double nearestDistanceSquared = std::numeric_limits<long double>::max();

        for( const auto& [viewItem, layer] : candidates )
        {
            if( !viewItem->IsBOARD_ITEM() )
                continue;

            BOARD_ITEM* candidate = static_cast<BOARD_ITEM*>( viewItem );

            /*
             * EXTEND currently supports straight tracks only.
             */
            if( candidate->Type() != PCB_TRACE_T )
            {
                continue;
            }

            if( !m_selectionTool->Selectable( candidate ) )
            {
                continue;
            }

            if( candidate->IsLocked() )
                continue;

            if( !candidate->HitTest( aPoint, hitTolerance ) )
            {
                continue;
            }

            PCB_TRACK*        candidateTrack = static_cast<PCB_TRACK*>( candidate );
            const VECTOR2I    start = candidateTrack->GetStart();
            const VECTOR2I    end = candidateTrack->GetEnd();
            const long double segmentX = static_cast<long double>( end.x ) - static_cast<long double>( start.x );
            const long double segmentY = static_cast<long double>( end.y ) - static_cast<long double>( start.y );
            const long double segmentLengthSquared = segmentX * segmentX + segmentY * segmentY;

            if( segmentLengthSquared <= 0.0L )
                continue;

            const long double pointX = static_cast<long double>( aPoint.x ) - static_cast<long double>( start.x );
            const long double pointY = static_cast<long double>( aPoint.y ) - static_cast<long double>( start.y );
            long double       projection = ( pointX * segmentX + pointY * segmentY ) / segmentLengthSquared;
            projection = std::max( 0.0L, std::min( 1.0L, projection ) );
            const long double closestX = static_cast<long double>( start.x ) + projection * segmentX;
            const long double closestY = static_cast<long double>( start.y ) + projection * segmentY;
            const long double differenceX = static_cast<long double>( aPoint.x ) - closestX;
            const long double differenceY = static_cast<long double>( aPoint.y ) - closestY;
            const long double distanceSquared = differenceX * differenceX + differenceY * differenceY;

            if( distanceSquared < nearestDistanceSquared )
            {
                nearestDistanceSquared = distanceSquared;
                selectedTrack = candidateTrack;
            }
        }

        return selectedTrack;
    };

    /*
     * Extend an already known straight track.
     *
     * aPoint determines which endpoint is extended.
     */
    auto extendTrack = [&]( PCB_TRACK* sourceTrack, const VECTOR2I& aPoint ) -> bool
    {
        if( !sourceTrack )
            return false;

        if( sourceTrack->Type() != PCB_TRACE_T )
        {
            return false;
        }

        if( sourceTrack->IsLocked() )
        {
            frame()->SetStatusText( _( "EXTEND: The selected track is locked." ) );

            return false;
        }

        const VECTOR2I sourceStart = sourceTrack->GetStart();
        const VECTOR2I sourceEnd = sourceTrack->GetEnd();

        /*
         * Determine which endpoint is nearest the cursor.
         *
         * That is the endpoint which will be extended.
         */
        const long double startDifferenceX =
                static_cast<long double>( aPoint.x ) - static_cast<long double>( sourceStart.x );
        const long double startDifferenceY =
                static_cast<long double>( aPoint.y ) - static_cast<long double>( sourceStart.y );
        const long double endDifferenceX =
                static_cast<long double>( aPoint.x ) - static_cast<long double>( sourceEnd.x );
        const long double endDifferenceY =
                static_cast<long double>( aPoint.y ) - static_cast<long double>( sourceEnd.y );
        const long double startDistanceSquared =
                startDifferenceX * startDifferenceX + startDifferenceY * startDifferenceY;
        const long double endDistanceSquared = endDifferenceX * endDifferenceX + endDifferenceY * endDifferenceY;
        const bool        extendStart = startDistanceSquared < endDistanceSquared;

        VECTOR2I fixedPoint;
        VECTOR2I movingPoint;

        if( extendStart )
        {
            movingPoint = sourceStart;
            fixedPoint = sourceEnd;
        }
        else
        {
            movingPoint = sourceEnd;
            fixedPoint = sourceStart;
        }

        /*
         * The extension continues in the same direction as the
         * existing source track.
         */
        const VECTOR2I rayDirection = movingPoint - fixedPoint;

        if( rayDirection.x == 0 && rayDirection.y == 0 )
        {
            frame()->SetStatusText( _( "EXTEND: The selected track has zero length." ) );
            return false;
        }

        enum class EXTEND_TARGET_TYPE
        {
            NONE,
            SAME_NET_TRACK,
            DIFFERENT_NET_TRACK,
            GRAPHIC_LINE
        };

        EXTEND_TARGET_TYPE nearestTargetType = EXTEND_TARGET_TYPE::NONE;
        VECTOR2I           nearestIntersection;
        long double        nearestRayParameter = std::numeric_limits<long double>::max();

        /*
         * Examine every board track and retain the nearest
         * intersection in front of the selected endpoint.
         */
        for( PCB_TRACK* targetTrack : board->Tracks() )
        {
            if( !targetTrack )
                continue;

            if( targetTrack == sourceTrack )
                continue;

            /*
             * Straight tracks only.
             */
            if( targetTrack->Type() != PCB_TRACE_T )
            {
                continue;
            }

            /*
             * Tracks on other copper layers are not physically
             * connected to the source track.
             */
            if( targetTrack->GetLayer() != sourceTrack->GetLayer() )
            {
                continue;
            }

            if( !m_selectionTool->Selectable( targetTrack ) )
            {
                continue;
            }

            VECTOR2I intersection;

            long double rayParameter = 0.0L;

            if( !findRaySegmentIntersection( movingPoint, rayDirection, targetTrack->GetStart(), targetTrack->GetEnd(),
                                             intersection, rayParameter ) )
            {
                continue;
            }

            if( rayParameter < nearestRayParameter )
            {
                nearestRayParameter = rayParameter;
                nearestIntersection = intersection;

                if( sourceTrack->GetNetCode() == targetTrack->GetNetCode() )
                {
                    nearestTargetType = EXTEND_TARGET_TYPE::SAME_NET_TRACK;
                }
                else
                {
                    nearestTargetType = EXTEND_TARGET_TYPE::DIFFERENT_NET_TRACK;
                }
            }
        }

        /*
         * Examine straight graphic lines.
         *
         * These can be used as temporary AutoCAD-style boundary
         * lines. The graphic may be on Dwgs.User, Cmts.User,
         * Eco layers, Edge.Cuts or another drawing layer.
         */
        for( BOARD_ITEM* drawingItem : board->Drawings() )
        {
            if( !drawingItem )
                continue;

            if( drawingItem->Type() != PCB_SHAPE_T )
            {
                continue;
            }

            PCB_SHAPE* targetLine = static_cast<PCB_SHAPE*>( drawingItem );

            /*
             * Straight graphic segments only.
             */
            if( targetLine->GetShape() != SHAPE_T::SEGMENT )
            {
                continue;
            }

            /*
             * Ignore hidden graphic layers.
             */
            if( !getView()->IsLayerVisible( targetLine->GetLayer() ) )
            {
                continue;
            }

            VECTOR2I    intersection;
            long double rayParameter = 0.0L;

            if( !findRaySegmentIntersection( movingPoint, rayDirection, targetLine->GetStart(), targetLine->GetEnd(),
                                             intersection, rayParameter ) )
            {
                continue;
            }

            if( rayParameter < nearestRayParameter )
            {
                nearestRayParameter = rayParameter;
                nearestIntersection = intersection;
                nearestTargetType = EXTEND_TARGET_TYPE::GRAPHIC_LINE;
            }
        }

        if( nearestTargetType == EXTEND_TARGET_TYPE::NONE )
        {
            frame()->SetStatusText( _( "EXTEND: No intersecting same-layer track "
                                       "or graphic line was found in the selected "
                                       "direction." ) );

            return false;
        }

        /*
         * Do not automatically create a short circuit to another
         * net.
         *
         * A graphic boundary line has no net, so extending to it is
         * always allowed.
         */
        if( nearestTargetType == EXTEND_TARGET_TYPE::DIFFERENT_NET_TRACK )
        {
            frame()->SetStatusText( _( "EXTEND: The nearest intersecting copper "
                                       "track belongs to a different net." ) );

            return false;
        }

        /*
         * Do not create a zero-length track or replace an endpoint
         * with its current position.
         */
        if( nearestIntersection == movingPoint )
        {
            frame()->SetStatusText( _( "EXTEND: The selected endpoint already meets "
                                       "the target object." ) );

            return false;
        }

        commit.Modify( sourceTrack );

        if( extendStart )
        {
            sourceTrack->SetStart( nearestIntersection );
        }
        else
        {
            sourceTrack->SetEnd( nearestIntersection );
        }

        extendedCount++;
        getView()->Update( sourceTrack );
        m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );

        if( nearestTargetType == EXTEND_TARGET_TYPE::GRAPHIC_LINE )
        {
            frame()->SetStatusText( wxString::Format( _( "EXTEND: %d track(s) extended to a "
                                                         "graphic boundary. Click near another "
                                                         "track endpoint or press Esc." ),
                                                      extendedCount ) );
        }
        else
        {
            frame()->SetStatusText( wxString::Format( _( "EXTEND: %d track(s) extended to a "
                                                         "same-net track. Click near another "
                                                         "track endpoint or press Esc." ),
                                                      extendedCount ) );
        }

        return true;
    };

    /*
     * Find a straight track at the clicked point and extend it.
     */
    auto extendAtPoint = [&]( const VECTOR2I& aPoint ) -> bool
    {
        PCB_TRACK* sourceTrack = findTrackAtPoint( aPoint );

        if( !sourceTrack )
        {
            frame()->SetStatusText( _( "EXTEND: No unlocked straight track at the "
                                       "clicked point." ) );

            return false;
        }

        return extendTrack( sourceTrack, aPoint );
    };

    /*
     * When invoked from the right-click context menu with one
     * straight track selected, extend it immediately.
     */
    if( initiallySelectedTrack )
    {
        extendTrack( initiallySelectedTrack, initialCursorPosition );
    }

    /*
     * EXTEND interactive loop.
     *
     * The command remains active so the user can extend several
     * additional tracks before pressing Esc.
     */
    while( TOOL_EVENT* event = Wait() )
    {
        if( event->IsCancelInteractive() || event->IsActivate() )
        {
            cancelled = true;
            event->SetPassEvent( false );
            break;
        }
        else if( event->IsClick( BUT_LEFT ) || event->IsAction( &ACTIONS::cursorClick ) )
        {
            extendAtPoint( event->Position() );
            event->SetPassEvent( false );
        }
        else if( event->IsMotion() )
        {
            m_statusPopup->Move( KIPLATFORM::UI::GetMousePosition() + wxPoint( 20, -50 ) );
            event->SetPassEvent();
        }
        else
        {
            /*
             * Allow zooming, panning and other navigation events
             * while EXTEND remains active.
             */
            event->SetPassEvent();
        }
    }

    /*
     * Clean up temporary user-interface state.
     */
    m_toolMgr->RunAction( ACTIONS::selectionClear );
    canvas()->SetStatusPopup( nullptr );
    m_statusPopup->Hide();
    canvas()->SetCurrentCursor( KICURSOR::ARROW );

    /*
     * Group all tracks extended during this command session into
     * one Undo operation.
     */
    if( !commit.Empty() )
        commit.Push( _( "Extend Track" ) );

    frame()->PopTool( pushedEvent );

    if( extendedCount > 0 )
    {
        frame()->SetStatusText( wxString::Format( _( "EXTEND finished: %d track(s) extended." ), extendedCount ) );
    }
    else if( cancelled )
    {
        frame()->SetStatusText( _( "EXTEND cancelled." ) );
    }
    else
    {
        frame()->SetStatusText( _( "EXTEND finished: No tracks extended." ) );
    }

    return 0;
}


int EDIT_TOOL::TrimTrack( const TOOL_EVENT& aEvent )
{
    TOOL_EVENT pushedEvent = aEvent;
    frame()->PushTool( pushedEvent );
    Activate();

    PCB_BASE_EDIT_FRAME* editFrame = getEditFrame<PCB_BASE_EDIT_FRAME>();

    if( !editFrame )
    {
        frame()->PopTool( pushedEvent );
        return 0;
    }

    BOARD* board = editFrame->GetBoard();

    if( !board )
    {
        frame()->PopTool( pushedEvent );
        return 0;
    }

    /*
     * If TRIM was invoked from the right-click context menu with
     * exactly one straight track selected, remember that track.
     *
     * It will be trimmed immediately without requiring the user
     * to click it again.
     */
    PCB_TRACK* initiallySelectedTrack = nullptr;

    PCB_SELECTION& initialSelection = m_selectionTool->GetSelection();

    if( initialSelection.GetSize() == 1 )
    {
        EDA_ITEM* selectedItem = initialSelection.Front();

        if( selectedItem && selectedItem->Type() == PCB_TRACE_T )
        {
            PCB_TRACK* selectedTrack = static_cast<PCB_TRACK*>( selectedItem );

            if( !selectedTrack->IsLocked() && m_selectionTool->Selectable( selectedTrack ) )
            {
                initiallySelectedTrack = selectedTrack;
            }
        }
    }

    /*
     * This position determines which portion of an initially
     * selected track is trimmed.
     */
    const VECTOR2I initialCursorPosition = VECTOR2I( getViewControls()->GetCursorPosition( false ) );

    m_toolMgr->RunAction( ACTIONS::selectionClear );
    int  trimmedCount = 0;
    bool cancelled = false;
    m_statusPopup->SetText( _( "TRIM: Click the part of a straight track "
                               "to remove - Esc to finish" ) );

    m_statusPopup->Move( KIPLATFORM::UI::GetMousePosition() + wxPoint( 20, -50 ) );
    m_statusPopup->Popup();
    canvas()->SetStatusPopup( m_statusPopup->GetPanel() );
    canvas()->SetCurrentCursor( KICURSOR::PLACE );

    /*
     * Find the intersection between two finite line segments.
     *
     * Source:
     *
     *     aSourceStart + t * sourceDirection
     *
     * Target:
     *
     *     aTargetStart + u * targetDirection
     *
     * Both t and u must be between zero and one.
     *
     * aSourceParameter is the position along the source segment,
     * measured from its start:
     *
     *     start = 0.0
     *     end   = 1.0
     */
    auto findSegmentIntersection = []( const VECTOR2I& aSourceStart, const VECTOR2I& aSourceEnd,
                                       const VECTOR2I& aTargetStart, const VECTOR2I& aTargetEnd,
                                       VECTOR2I& aIntersection, long double& aSourceParameter ) -> bool
    {
        const long double sourceX =
                static_cast<long double>( aSourceEnd.x ) - static_cast<long double>( aSourceStart.x );
        const long double sourceY =
                static_cast<long double>( aSourceEnd.y ) - static_cast<long double>( aSourceStart.y );
        const long double targetX =
                static_cast<long double>( aTargetEnd.x ) - static_cast<long double>( aTargetStart.x );
        const long double targetY =
                static_cast<long double>( aTargetEnd.y ) - static_cast<long double>( aTargetStart.y );
        const long double     denominator = sourceX * targetY - sourceY * targetX;
        constexpr long double parallelTolerance = 1.0e-18L;

        if( std::fabs( denominator ) < parallelTolerance )
        {
            return false;
        }

        const long double deltaX =
                static_cast<long double>( aTargetStart.x ) - static_cast<long double>( aSourceStart.x );
        const long double deltaY =
                static_cast<long double>( aTargetStart.y ) - static_cast<long double>( aSourceStart.y );
        const long double t = ( deltaX * targetY - deltaY * targetX ) / denominator;
        const long double u = ( deltaX * sourceY - deltaY * sourceX ) / denominator;

        /*
         * Ignore intersections at the existing source endpoints.
         *
         * Such intersections would not shorten or split the track.
         */
        constexpr long double endpointTolerance = 1.0e-12L;
        constexpr long double targetTolerance = 1.0e-12L;

        if( t <= endpointTolerance || t >= 1.0L - endpointTolerance )
        {
            return false;
        }

        if( u < -targetTolerance || u > 1.0L + targetTolerance )
        {
            return false;
        }

        const long double intersectionX = static_cast<long double>( aSourceStart.x ) + t * sourceX;
        const long double intersectionY = static_cast<long double>( aSourceStart.y ) + t * sourceY;
        aIntersection.x = static_cast<int>( std::llround( intersectionX ) );
        aIntersection.y = static_cast<int>( std::llround( intersectionY ) );
        aSourceParameter = t;

        return true;
    };

    /*
     * Calculate the position of a point along a finite source track.
     *
     * The returned value is clamped between zero and one:
     *
     *     start = 0.0
     *     end   = 1.0
     */
    auto pointParameterOnTrack = []( const VECTOR2I& aPoint, const VECTOR2I& aStart,
                                     const VECTOR2I& aEnd ) -> long double
    {
        const long double segmentX = static_cast<long double>( aEnd.x ) - static_cast<long double>( aStart.x );
        const long double segmentY = static_cast<long double>( aEnd.y ) - static_cast<long double>( aStart.y );
        const long double lengthSquared = segmentX * segmentX + segmentY * segmentY;

        if( lengthSquared <= 0.0L )
            return 0.0L;

        const long double pointX = static_cast<long double>( aPoint.x ) - static_cast<long double>( aStart.x );
        const long double pointY = static_cast<long double>( aPoint.y ) - static_cast<long double>( aStart.y );
        long double       parameter = ( pointX * segmentX + pointY * segmentY ) / lengthSquared;

        return std::max( 0.0L, std::min( 1.0L, parameter ) );
    };

    /*
     * Find the nearest straight track at a clicked position.
     */
    auto findTrackAtPoint = [&]( const VECTOR2I& aPoint ) -> PCB_TRACK*
    {
        const int                                 hitTolerance = KiROUND( getView()->ToWorld( 8 ) );
        BOX2I                                     searchBox( aPoint - VECTOR2I( hitTolerance, hitTolerance ),
                                                             VECTOR2I( hitTolerance * 2, hitTolerance * 2 ) );
        std::vector<KIGFX::VIEW::LAYER_ITEM_PAIR> candidates;
        getView()->Query( searchBox, candidates );
        PCB_TRACK*  selectedTrack = nullptr;
        long double nearestDistanceSquared = std::numeric_limits<long double>::max();

        for( const auto& [viewItem, layer] : candidates )
        {
            if( !viewItem->IsBOARD_ITEM() )
                continue;

            BOARD_ITEM* candidate = static_cast<BOARD_ITEM*>( viewItem );

            if( candidate->Type() != PCB_TRACE_T )
                continue;

            if( !m_selectionTool->Selectable( candidate ) )
            {
                continue;
            }

            if( candidate->IsLocked() )
                continue;

            if( !candidate->HitTest( aPoint, hitTolerance ) )
            {
                continue;
            }

            PCB_TRACK*        candidateTrack = static_cast<PCB_TRACK*>( candidate );
            const VECTOR2I    start = candidateTrack->GetStart();
            const VECTOR2I    end = candidateTrack->GetEnd();
            const long double segmentX = static_cast<long double>( end.x ) - static_cast<long double>( start.x );
            const long double segmentY = static_cast<long double>( end.y ) - static_cast<long double>( start.y );
            const long double segmentLengthSquared = segmentX * segmentX + segmentY * segmentY;

            if( segmentLengthSquared <= 0.0L )
                continue;

            const long double pointX = static_cast<long double>( aPoint.x ) - static_cast<long double>( start.x );
            const long double pointY = static_cast<long double>( aPoint.y ) - static_cast<long double>( start.y );
            long double       projection = ( pointX * segmentX + pointY * segmentY ) / segmentLengthSquared;
            projection = std::max( 0.0L, std::min( 1.0L, projection ) );
            const long double closestX = static_cast<long double>( start.x ) + projection * segmentX;
            const long double closestY = static_cast<long double>( start.y ) + projection * segmentY;
            const long double differenceX = static_cast<long double>( aPoint.x ) - closestX;
            const long double differenceY = static_cast<long double>( aPoint.y ) - closestY;
            const long double distanceSquared = differenceX * differenceX + differenceY * differenceY;

            if( distanceSquared < nearestDistanceSquared )
            {
                nearestDistanceSquared = distanceSquared;
                selectedTrack = candidateTrack;
            }
        }

        return selectedTrack;
    };

    enum class TRIM_TARGET_TYPE
    {
        SAME_NET_TRACK,
        GRAPHIC_LINE
    };

    struct TRIM_INTERSECTION
    {
        VECTOR2I         point;
        long double      parameter;
        TRIM_TARGET_TYPE type;
    };

    /*
     * Trim an already-known straight track.
     */
    auto trimTrack = [&]( PCB_TRACK* sourceTrack, const VECTOR2I& aPoint ) -> bool
    {
        if( !sourceTrack )
            return false;

        if( sourceTrack->Type() != PCB_TRACE_T )
            return false;

        if( sourceTrack->IsLocked() )
        {
            frame()->SetStatusText( _( "TRIM: The selected track is locked." ) );

            return false;
        }

        const VECTOR2I sourceStart = sourceTrack->GetStart();

        const VECTOR2I sourceEnd = sourceTrack->GetEnd();

        if( sourceStart == sourceEnd )
        {
            frame()->SetStatusText( _( "TRIM: The selected track has zero length." ) );

            return false;
        }

        /*
         * Store every valid intersection along the complete source
         * track rather than retaining only the nearest one.
         */
        std::vector<TRIM_INTERSECTION> intersections;

        /*
         * Add an intersection while suppressing duplicate points.
         *
         * A copper track and graphic boundary may cross the source at
         * exactly the same point.
         */
        auto addIntersection = [&]( const VECTOR2I& aIntersection, long double aParameter, TRIM_TARGET_TYPE aType )
        {
            constexpr long double duplicateTolerance = 1.0e-12L;

            for( TRIM_INTERSECTION& existing : intersections )
            {
                if( existing.point == aIntersection
                    || std::fabs( existing.parameter - aParameter ) < duplicateTolerance )
                {
                    /*
                     * Prefer a graphic line for the status message
                     * when both types exist at the same point.
                     */
                    if( aType == TRIM_TARGET_TYPE::GRAPHIC_LINE )
                    {
                        existing.type = aType;
                    }

                    return;
                }
            }

            intersections.push_back( { aIntersection, aParameter, aType } );
        };

        /*
         * Search same-net straight copper tracks on the same layer.
         *
         * Different-net tracks are deliberately ignored as trim
         * boundaries because trimming onto them would create a short.
         */
        for( PCB_TRACK* targetTrack : board->Tracks() )
        {
            if( !targetTrack )
                continue;

            if( targetTrack == sourceTrack )
                continue;

            if( targetTrack->Type() != PCB_TRACE_T )
                continue;

            if( targetTrack->GetLayer() != sourceTrack->GetLayer() )
            {
                continue;
            }

            if( sourceTrack->GetNetCode() != targetTrack->GetNetCode() )
            {
                continue;
            }

            if( !m_selectionTool->Selectable( targetTrack ) )
            {
                continue;
            }

            VECTOR2I intersection;

            long double sourceParameter = 0.0L;

            if( !findSegmentIntersection( sourceStart, sourceEnd, targetTrack->GetStart(), targetTrack->GetEnd(),
                                          intersection, sourceParameter ) )
            {
                continue;
            }

            addIntersection( intersection, sourceParameter, TRIM_TARGET_TYPE::SAME_NET_TRACK );
        }

        /*
         * Search straight graphic lines.
         *
         * These are valid AutoCAD-style temporary trim boundaries.
         */
        for( BOARD_ITEM* drawingItem : board->Drawings() )
        {
            if( !drawingItem )
                continue;

            PCB_SHAPE* targetLine = dynamic_cast<PCB_SHAPE*>( drawingItem );

            if( !targetLine )
                continue;

            if( targetLine->GetShape() != SHAPE_T::SEGMENT )
            {
                continue;
            }

            VECTOR2I    intersection;
            long double sourceParameter = 0.0L;

            if( !findSegmentIntersection( sourceStart, sourceEnd, targetLine->GetStart(), targetLine->GetEnd(),
                                          intersection, sourceParameter ) )
            {
                continue;
            }

            addIntersection( intersection, sourceParameter, TRIM_TARGET_TYPE::GRAPHIC_LINE );
        }

        if( intersections.empty() )
        {
            frame()->SetStatusText( _( "TRIM: No intersecting same-net track or "
                                       "graphic line was found on the selected track." ) );

            return false;
        }

        /*
         * Sort intersections from the original source start toward
         * the original source end.
         */
        std::sort( intersections.begin(), intersections.end(),
                   []( const TRIM_INTERSECTION& aLeft, const TRIM_INTERSECTION& aRight )
                   {
                       return aLeft.parameter < aRight.parameter;
                   } );

        const long double clickParameter = pointParameterOnTrack( aPoint, sourceStart, sourceEnd );

        /*
         * Locate the nearest intersection before and after the click.
         */
        const TRIM_INTERSECTION* intersectionBefore = nullptr;
        const TRIM_INTERSECTION* intersectionAfter = nullptr;
        constexpr long double    clickTolerance = 1.0e-12L;

        for( const TRIM_INTERSECTION& intersection : intersections )
        {
            if( intersection.parameter < clickParameter - clickTolerance )
            {
                intersectionBefore = &intersection;
            }
            else if( intersection.parameter > clickParameter + clickTolerance )
            {
                intersectionAfter = &intersection;

                break;
            }
        }

        /*
         * Each successful TRIM click is committed immediately.
         *
         * This makes newly-created split segments appear at once and
         * keeps Undo/Redo synchronized with the board and view.
         */
        BOARD_COMMIT commit( this );

        /*
         * CASE 1:
         *
         * The click lies between two boundaries.
         *
         * Keep the original start-side section in sourceTrack and
         * create a duplicate for the end-side section.
         */
        if( intersectionBefore && intersectionAfter )
        {
            //BOARD_ITEM* duplicateItem = sourceTrack->Duplicate( true, &commit );
            BOARD_ITEM* duplicateItem = sourceTrack->Duplicate( false );

            PCB_TRACK* secondTrack = dynamic_cast<PCB_TRACK*>( duplicateItem );

            if( !secondTrack )
            {
                delete duplicateItem;

                frame()->SetStatusText( _( "TRIM: Unable to create the second "
                                           "remaining track segment." ) );

                return false;
            }

            /*
             * The original track becomes:
             *
             *     original start -> boundary before click
             */
            commit.Modify( sourceTrack );
            sourceTrack->SetStart( sourceStart );
            sourceTrack->SetEnd( intersectionBefore->point );

            /*
             * The duplicate track becomes:
             *
             *     boundary after click -> original end
             */
            secondTrack->SetStart( intersectionAfter->point );
            secondTrack->SetEnd( sourceEnd );
            secondTrack->ClearSelected();
            commit.Add( secondTrack );
            commit.Push( _( "Trim Track" ) );
            trimmedCount++;
            m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );

            frame()->SetStatusText( wxString::Format( _( "TRIM: %d section(s) removed between "
                                                         "two boundaries. Click another track "
                                                         "or press Esc." ),
                                                      trimmedCount ) );

            return true;
        }

        /*
         * CASE 2:
         *
         * The click lies before the first intersection.
         *
         * Remove the start-side portion.
         */
        if( !intersectionBefore && intersectionAfter )
        {
            if( intersectionAfter->point == sourceEnd )
            {
                frame()->SetStatusText( _( "TRIM: The selected track cannot be "
                                           "shortened at this intersection." ) );

                return false;
            }

            commit.Modify( sourceTrack );
            sourceTrack->SetStart( intersectionAfter->point );
            commit.Push( _( "Trim Track" ) );
            trimmedCount++;
            m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );

            if( intersectionAfter->type == TRIM_TARGET_TYPE::GRAPHIC_LINE )
            {
                frame()->SetStatusText( wxString::Format( _( "TRIM: %d track section(s) trimmed "
                                                             "to a graphic boundary. Click another "
                                                             "track or press Esc." ),
                                                          trimmedCount ) );
            }
            else
            {
                frame()->SetStatusText( wxString::Format( _( "TRIM: %d track section(s) trimmed "
                                                             "to a same-net track. Click another "
                                                             "track or press Esc." ),
                                                          trimmedCount ) );
            }

            return true;
        }

        /*
         * CASE 3:
         *
         * The click lies after the final intersection.
         *
         * Remove the end-side portion.
         */
        if( intersectionBefore && !intersectionAfter )
        {
            if( intersectionBefore->point == sourceStart )
            {
                frame()->SetStatusText( _( "TRIM: The selected track cannot be "
                                           "shortened at this intersection." ) );

                return false;
            }

            commit.Modify( sourceTrack );
            sourceTrack->SetEnd( intersectionBefore->point );
            commit.Push( _( "Trim Track" ) );
            trimmedCount++;
            m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );

            if( intersectionBefore->type == TRIM_TARGET_TYPE::GRAPHIC_LINE )
            {
                frame()->SetStatusText( wxString::Format( _( "TRIM: %d track section(s) trimmed "
                                                             "to a graphic boundary. Click another "
                                                             "track or press Esc." ),
                                                          trimmedCount ) );
            }
            else
            {
                frame()->SetStatusText( wxString::Format( _( "TRIM: %d track section(s) trimmed "
                                                             "to a same-net track. Click another "
                                                             "track or press Esc." ),
                                                          trimmedCount ) );
            }

            return true;
        }

        /*
         * The click was effectively on an intersection point itself.
         *
         * There is no unambiguous section to remove.
         */
        frame()->SetStatusText( _( "TRIM: Click on the track section to remove, "
                                   "not directly on the boundary." ) );

        return false;
    };

    /*
     * Locate and trim a straight track at a clicked point.
     */
    auto trimAtPoint = [&]( const VECTOR2I& aPoint ) -> bool
    {
        PCB_TRACK* sourceTrack = findTrackAtPoint( aPoint );

        if( !sourceTrack )
        {
            frame()->SetStatusText( _( "TRIM: No unlocked straight track at the "
                                       "clicked point." ) );

            return false;
        }

        return trimTrack( sourceTrack, aPoint );
    };

    /*
     * Right-click menu invocation with one selected track:
     * perform the trim immediately.
     */
    if( initiallySelectedTrack )
    {
        trimTrack( initiallySelectedTrack, initialCursorPosition );
    }

    /*
     * TRIM remains active for additional track selections.
     */
    while( TOOL_EVENT* event = Wait() )
    {
        if( event->IsCancelInteractive() || event->IsActivate() )
        {
            cancelled = true;
            event->SetPassEvent( false );
            break;
        }
        else if( event->IsClick( BUT_LEFT ) || event->IsAction( &ACTIONS::cursorClick ) )
        {
            trimAtPoint( event->Position() );
            event->SetPassEvent( false );
        }
        else if( event->IsMotion() )
        {
            m_statusPopup->Move( KIPLATFORM::UI::GetMousePosition() + wxPoint( 20, -50 ) );
            event->SetPassEvent();
        }
        else
        {
            /*
             * Allow normal zoom and pan events.
             */
            event->SetPassEvent();
        }
    }

    m_toolMgr->RunAction( ACTIONS::selectionClear );
    canvas()->SetStatusPopup( nullptr );
    m_statusPopup->Hide();
    canvas()->SetCurrentCursor( KICURSOR::ARROW );
    frame()->PopTool( pushedEvent );

    if( trimmedCount > 0 )
    {
        frame()->SetStatusText( wxString::Format( _( "TRIM finished: %d track section(s) removed." ), trimmedCount ) );
    }
    else if( cancelled )
    {
        frame()->SetStatusText( _( "TRIM cancelled." ) );
    }
    else
    {
        frame()->SetStatusText( _( "TRIM finished: No track sections removed." ) );
    }

    return 0;
}


int EDIT_TOOL::Properties( const TOOL_EVENT& aEvent )
{
    PCB_BASE_EDIT_FRAME* editFrame = getEditFrame<PCB_BASE_EDIT_FRAME>();

    // A constraint selected by clicking its badge has no board selection, so Edit targets it here
    // before the normal properties path (mirrors the Delete hook for a badge-selected constraint).
    if( CONSTRAINT_EDIT_TOOL* constraintTool = m_toolMgr->GetTool<CONSTRAINT_EDIT_TOOL>();
        constraintTool && constraintTool->TryEditSelectedConstraint() )
    {
        return 0;
    }

    const PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
            } );

    // Snapshot undo depth to detect whether dialog actually committed
    // Cancel leaves it unchanged and must not trigger a constraint resolve
    const int undoBefore = editFrame->GetUndoCommandCount();

    // Tracks & vias are treated in a special way:
    if( ( SELECTION_CONDITIONS::OnlyTypes( { PCB_TRACE_T, PCB_ARC_T, PCB_VIA_T } ) )( selection ) )
    {
        DIALOG_TRACK_VIA_PROPERTIES dlg( editFrame, selection );
        dlg.ShowQuasiModal(); // QuasiModal required for NET_SELECTOR
    }
    else if( ( SELECTION_CONDITIONS::OnlyTypes( { PCB_TABLECELL_T } ) )( selection ) )
    {
        std::vector<PCB_TABLECELL*> cells;

        for( EDA_ITEM* item : selection.Items() )
            cells.push_back( static_cast<PCB_TABLECELL*>( item ) );

        DIALOG_TABLECELL_PROPERTIES dlg( editFrame, cells );

        // QuasiModal required for syntax help and Scintilla auto-complete
        dlg.ShowQuasiModal();

        if( dlg.GetReturnValue() == DIALOG_TABLECELL_PROPERTIES::TABLECELL_PROPS_EDIT_TABLE )
        {
            PCB_TABLE*              table = static_cast<PCB_TABLE*>( cells[0]->GetParent() );
            DIALOG_TABLE_PROPERTIES tableDlg( frame(), table );

            tableDlg.ShowQuasiModal(); // Scintilla's auto-complete requires quasiModal
        }
    }
    else if( selection.Size() == 1 && selection.Front()->IsBOARD_ITEM() )
    {
        // Display properties dialog
        BOARD_ITEM* item = static_cast<BOARD_ITEM*>( selection.Front() );

        // Do not handle undo buffer, it is done by the properties dialogs
        editFrame->OnEditItemRequest( item );

        // Notify other tools of the changes
        m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );
    }
    else if( selection.Size() == 0 && getView()->IsLayerVisible( LAYER_DRAWINGSHEET ) )
    {
        DS_PROXY_VIEW_ITEM* ds = editFrame->GetCanvas()->GetDrawingSheet();
        VECTOR2D            cursorPos = getViewControls()->GetCursorPosition( false );

        if( ds && ds->HitTestDrawingSheetItems( getView(), cursorPos ) )
            m_toolMgr->PostAction( ACTIONS::pageSettings );
        else
            m_toolMgr->RunAction( PCB_ACTIONS::footprintProperties );
    }

    // Position or geometry edit via these dialogs settles constraints as if item were dragged
    // holding edited item and moving neighbors gated on real commit so canceled dialog skips the solve
    if( editFrame->GetUndoCommandCount() > undoBefore )
    {
        if( CONSTRAINT_EDIT_TOOL* constraintTool = m_toolMgr->GetTool<CONSTRAINT_EDIT_TOOL>() )
        {
            std::vector<PCB_SHAPE*> shapes;
            collectConstraintShapes( selection, shapes );
            constraintTool->SolveAfterEdit( shapes );
        }
    }

    if( selection.IsHover() )
    {
        m_toolMgr->RunAction( ACTIONS::selectionClear );
    }
    else
    {
        // Check for items becoming invisible and drop them from the selection.

        PCB_SELECTION selCopy = selection;
        LSET          visible = editFrame->GetBoard()->GetVisibleLayers();

        for( EDA_ITEM* eda_item : selCopy )
        {
            if( !eda_item->IsBOARD_ITEM() )
                continue;

            BOARD_ITEM* item = static_cast<BOARD_ITEM*>( eda_item );

            if( !( item->GetLayerSet() & visible ).any() )
                m_selectionTool->RemoveItemFromSel( item );
        }
    }

    if( m_dragging )
    {
        m_toolMgr->PostAction( PCB_ACTIONS::updateLocalRatsnest, VECTOR2I() );
        m_toolMgr->PostAction( PCB_ACTIONS::refreshPreview );
    }

    return 0;
}


int EDIT_TOOL::EditVertices( const TOOL_EVENT& aEvent )
{
    const PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                sTool->FilterCollectorForMarkers( aCollector );
                sTool->FilterCollectorForHierarchy( aCollector, true );
                sTool->FilterCollectorForTableCells( aCollector );
                sTool->FilterCollectorForLockedItems( aCollector );
            } );

    m_selectionTool->ReportFilteredLockedItems();

    if( !selectionHasEditableCorners( selection ) )
    {
        wxBell();
        return 0;
    }

    PCB_BASE_EDIT_FRAME* editFrame = getEditFrame<PCB_BASE_EDIT_FRAME>();
    BOARD_ITEM*          item = dynamic_cast<BOARD_ITEM*>( selection.Front() );

    if( editFrame && item )
        editFrame->OpenVertexEditor( item );

    return 0;
}


void EDIT_TOOL::collectConstraintShapes( const SELECTION& aSelection, std::vector<PCB_SHAPE*>& aShapes )
{
    // Recurse so constrained shapes carried inside a transformed footprint or group seed their
    // clusters too; a top-level-only walk would leave those constraints silently violated.
    // PCB_GROUP::RunOnChildren only descends into groups and generators, so recurse through every
    // container ourselves; the visited set guards against overlapping ownership paths.
    std::unordered_set<BOARD_ITEM*> visited;

    std::function<void( BOARD_ITEM* )> collect =
            [&]( BOARD_ITEM* aItem )
            {
                if( !aItem || !visited.insert( aItem ).second )
                    return;

                if( aItem->Type() == PCB_SHAPE_T )
                    aShapes.push_back( static_cast<PCB_SHAPE*>( aItem ) );

                aItem->RunOnChildren( collect, RECURSE_MODE::NO_RECURSE );
            };

    for( EDA_ITEM* item : aSelection )
    {
        if( item->IsBOARD_ITEM() )
            collect( static_cast<BOARD_ITEM*>( item ) );
    }
}


void EDIT_TOOL::reSolveConstraintsAfterEdit( const PCB_SELECTION& aSelection )
{
    CONSTRAINT_EDIT_TOOL* constraintTool = m_toolMgr->GetTool<CONSTRAINT_EDIT_TOOL>();

    if( !constraintTool )
        return;

    std::vector<PCB_SHAPE*> shapes;
    collectConstraintShapes( aSelection, shapes );

    constraintTool->SolveAfterMove( shapes );
}


int EDIT_TOOL::Rotate( const TOOL_EVENT& aEvent )
{
    if( isRouterActive() )
    {
        wxBell();
        return 0;
    }

    PCB_BASE_EDIT_FRAME* editFrame = getEditFrame<PCB_BASE_EDIT_FRAME>();
    BOARD_COMMIT         localCommit( this );
    BOARD_COMMIT*        commit = dynamic_cast<BOARD_COMMIT*>( aEvent.Commit() );

    if( !commit )
        commit = &localCommit;

    // Be sure that there is at least one item that we can modify. If nothing was selected before,
    // try looking for the stuff under mouse cursor (i.e. KiCad old-style hover selection)
    PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            [&]( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                sTool->FilterCollectorForMarkers( aCollector );
                sTool->FilterCollectorForHierarchy( aCollector, true );
                sTool->FilterCollectorForFreePads( aCollector, false );
                sTool->FilterCollectorForTableCells( aCollector );

                // Filter locked items if in board editor and in free-pad-mode.  (If we're not in
                // free-pad mode we delay this until the second RequestSelection().)
                if( !m_isFootprintEditor && frame()->GetPcbNewSettings()->m_AllowFreePads )
                    sTool->FilterCollectorForLockedItems( aCollector );
            } );

    m_selectionTool->ReportFilteredLockedItems();

    if( selection.Empty() )
        return 0;

    std::optional<VECTOR2I> oldRefPt;
    bool                    is_hover = selection.IsHover(); // N.B. This must be saved before the second
                                                            // call to RequestSelection() below

    if( selection.HasReferencePoint() )
        oldRefPt = selection.GetReferencePoint();

    // Now filter out pads if not in free pads mode.  We cannot do this in the first
    // RequestSelection() as we need the reference point when a pad is the selection front.
    if( !m_isFootprintEditor && !frame()->GetPcbNewSettings()->m_AllowFreePads )
    {
        selection = m_selectionTool->RequestSelection(
                []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
                {
                    sTool->FilterCollectorForMarkers( aCollector );
                    sTool->FilterCollectorForHierarchy( aCollector, true );
                    sTool->FilterCollectorForFreePads( aCollector );
                    sTool->FilterCollectorForTableCells( aCollector );
                    sTool->FilterCollectorForLockedItems( aCollector );
                } );

        m_selectionTool->ReportFilteredLockedItems();
    }

    // Did we filter everything out?  If so, don't try to operate further
    if( selection.Empty() )
        return 0;

    // Some PCB_SHAPE must be rotated around their center instead of their start point in
    // order to stay to the same place (at least RECT and POLY)
    // Note a RECT shape rotated by a not cardinal angle is a POLY shape
    bool usePcbShapeCenter = false;

    if( selection.Size() == 1 && !m_dragging && dynamic_cast<PCB_SHAPE*>( selection.Front() ) )
    {
        PCB_SHAPE* shape = static_cast<PCB_SHAPE*>( selection.Front() );

        if( shape->GetShape() == SHAPE_T::RECTANGLE || shape->GetShape() == SHAPE_T::POLY )
            usePcbShapeCenter = true;
    }

    if( selection.Size() == 1 && !m_dragging && dynamic_cast<PCB_TABLE*>( selection.Front() ) )
        usePcbShapeCenter = true;

    if( selection.Size() == 1 && !m_dragging && dynamic_cast<PCB_TEXTBOX*>( selection.Front() ) )
    {
        selection.SetReferencePoint( static_cast<PCB_TEXTBOX*>( selection.Front() )->GetCenter() );
    }
    else if( usePcbShapeCenter )
    {
        selection.SetReferencePoint( static_cast<PCB_SHAPE*>( selection.Front() )->GetCenter() );
    }
    else
    {
        updateModificationPoint( selection );
    }

    VECTOR2I  refPt = selection.GetReferencePoint();
    EDA_ANGLE rotateAngle = TOOL_EVT_UTILS::GetEventRotationAngle( *editFrame, aEvent );

    if( frame()->GetCanvas()->GetView()->GetGAL()->IsFlippedX() )
        rotateAngle = -rotateAngle;

    // Calculate view bounding box
    BOX2I viewBBox = selection.Front()->ViewBBox();

    for( EDA_ITEM* item : selection )
        viewBBox.Merge( item->ViewBBox() );

    // Check if the view bounding box will go out of bounds
    VECTOR2D rotPos = viewBBox.GetPosition();
    VECTOR2D rotEnd = viewBBox.GetEnd();

    RotatePoint( &rotPos.x, &rotPos.y, refPt.x, refPt.y, rotateAngle );
    RotatePoint( &rotEnd.x, &rotEnd.y, refPt.x, refPt.y, rotateAngle );

    typedef std::numeric_limits<int> coord_limits;

    int max = coord_limits::max() - COORDS_PADDING;
    int min = -max;

    bool outOfBounds = rotPos.x < min || rotPos.x > max || rotPos.y < min || rotPos.y > max || rotEnd.x < min
                       || rotEnd.x > max || rotEnd.y < min || rotEnd.y > max;

    if( !outOfBounds )
    {
        for( EDA_ITEM* item : selection )
        {
            commit->Modify( item, nullptr, RECURSE_MODE::RECURSE );

            if( item->IsBOARD_ITEM() )
            {
                BOARD_ITEM* board_item = static_cast<BOARD_ITEM*>( item );

                board_item->Rotate( refPt, rotateAngle );
                board_item->Normalize();

                if( board_item->Type() == PCB_FOOTPRINT_T )
                    static_cast<FOOTPRINT*>( board_item )->InvalidateComponentClassCache();
            }
        }

        // Don't push a separate undo entry when we're in the middle of a move operation.
        // The parent move will handle the commit.
        if( !localCommit.Empty() && !m_dragging )
        {
            localCommit.Push( _( "Rotate" ) );
            reSolveConstraintsAfterEdit( selection );
        }

        if( is_hover && !m_dragging )
            m_toolMgr->RunAction( ACTIONS::selectionClear );

        m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );

        if( m_dragging )
        {
            m_toolMgr->PostAction( PCB_ACTIONS::updateLocalRatsnest, VECTOR2I() );
            m_toolMgr->PostAction( PCB_ACTIONS::refreshPreview );
        }
    }

    // Restore the old reference so any mouse dragging that occurs doesn't make the selection jump
    // to this now invalid reference
    if( oldRefPt )
        selection.SetReferencePoint( *oldRefPt );
    else
        selection.ClearReferencePoint();

    return 0;
}


/**
 * Mirror a pad in the H/V axis passing through a point
 */
static void mirrorPad( PAD& aPad, const VECTOR2I& aMirrorPoint, FLIP_DIRECTION aFlipDirection )
{
    // TODO(JE) padstacks
    if( aPad.GetShape( PADSTACK::ALL_LAYERS ) == PAD_SHAPE::CUSTOM )
        aPad.FlipPrimitives( aFlipDirection );

    VECTOR2I tmpPt = aPad.GetPosition();
    MIRROR( tmpPt, aMirrorPoint, aFlipDirection );
    aPad.SetPosition( tmpPt );

    tmpPt = aPad.GetOffset( PADSTACK::ALL_LAYERS );
    MIRROR( tmpPt, VECTOR2I{ 0, 0 }, aFlipDirection );
    aPad.SetOffset( PADSTACK::ALL_LAYERS, tmpPt );

    VECTOR2I tmpz = aPad.GetDelta( PADSTACK::ALL_LAYERS );
    MIRROR( tmpz, VECTOR2I{ 0, 0 }, aFlipDirection );
    aPad.SetDelta( PADSTACK::ALL_LAYERS, tmpz );

    aPad.SetOrientation( -aPad.GetOrientation() );
}


const std::vector<KICAD_T> EDIT_TOOL::MirrorableItems = {
    PCB_SHAPE_T, PCB_FIELD_T, PCB_TEXT_T,  PCB_TEXTBOX_T,   PCB_ZONE_T,  PCB_PAD_T,   PCB_TRACE_T,
    PCB_ARC_T,   PCB_VIA_T,   PCB_GROUP_T, PCB_GENERATOR_T, PCB_POINT_T, PCB_TABLE_T,
};


int EDIT_TOOL::Mirror( const TOOL_EVENT& aEvent )
{
    if( isRouterActive() )
    {
        wxBell();
        return 0;
    }

    BOARD_COMMIT  localCommit( this );
    BOARD_COMMIT* commit = dynamic_cast<BOARD_COMMIT*>( aEvent.Commit() );

    if( !commit )
        commit = &localCommit;

    PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                sTool->FilterCollectorForMarkers( aCollector );
                sTool->FilterCollectorForHierarchy( aCollector, true );
                sTool->FilterCollectorForFreePads( aCollector );
                sTool->FilterCollectorForLockedItems( aCollector );
            } );

    m_selectionTool->ReportFilteredLockedItems();

    if( selection.Empty() )
        return 0;

    updateModificationPoint( selection );
    VECTOR2I mirrorPoint = selection.GetReferencePoint();

    FLIP_DIRECTION flipDirection = aEvent.IsAction( &PCB_ACTIONS::mirrorV ) ? FLIP_DIRECTION::TOP_BOTTOM
                                                                            : FLIP_DIRECTION::LEFT_RIGHT;

    int skippedFootprints = 0;
    int skippedGroups = 0;

    for( EDA_ITEM* item : selection )
    {
        if( !item->IsType( MirrorableItems ) )
        {
            if( item->Type() == PCB_FOOTPRINT_T )
                skippedFootprints++;

            continue;
        }

        // Skip groups that hold non-mirrorable items, else the rest would tear away from them.
        if( item->Type() == PCB_GROUP_T && !groupMirrorable( static_cast<PCB_GROUP*>( item ) ) )
        {
            skippedGroups++;
            continue;
        }

        commit->Modify( item, nullptr, RECURSE_MODE::RECURSE );

        // modify each object as necessary
        switch( item->Type() )
        {
        case PCB_SHAPE_T:
            static_cast<PCB_SHAPE*>( item )->Mirror( mirrorPoint, flipDirection );
            break;

        case PCB_ZONE_T:
            static_cast<ZONE*>( item )->Mirror( mirrorPoint, flipDirection );
            break;

        case PCB_FIELD_T:
        case PCB_TEXT_T:
            static_cast<PCB_TEXT*>( item )->Mirror( mirrorPoint, flipDirection );
            break;

        case PCB_TEXTBOX_T:
            static_cast<PCB_TEXTBOX*>( item )->Mirror( mirrorPoint, flipDirection );
            break;

        case PCB_TABLE_T: static_cast<PCB_TABLE*>( item )->Mirror( mirrorPoint, flipDirection ); break;

        case PCB_PAD_T:
            mirrorPad( *static_cast<PAD*>( item ), mirrorPoint, flipDirection );
            break;

        case PCB_TRACE_T:
        case PCB_ARC_T:
        case PCB_VIA_T:
            static_cast<PCB_TRACK*>( item )->Mirror( mirrorPoint, flipDirection );
            break;

        case PCB_GROUP_T:
            static_cast<PCB_GROUP*>( item )->Mirror( mirrorPoint, flipDirection );
            break;

        case PCB_GENERATOR_T:
            static_cast<PCB_GENERATOR*>( item )->Mirror( mirrorPoint, flipDirection );
            break;

        case PCB_POINT_T:

            static_cast<PCB_POINT*>( item )->Mirror( mirrorPoint, flipDirection ); break;

        default:
            // it's likely the commit object is wrong if you get here
            UNIMPLEMENTED_FOR( item->GetClass() );
        }
    }

    // Don't push a separate undo entry when we're in the middle of a move operation.
    // The parent move will handle the commit.
    if( !localCommit.Empty() && !m_dragging )
    {
        localCommit.Push( _( "Mirror" ) );
        reSolveConstraintsAfterEdit( selection );
    }

    if( skippedFootprints > 0 && !m_dragging )
    {
        frame()->ShowInfoBarMsg( _( "Footprints cannot be mirrored. Use Flip to move them to "
                                    "the other side of the board." ) );
    }
    else if( skippedGroups > 0 && !m_dragging )
    {
        frame()->ShowInfoBarMsg( _( "Groups containing footprints or other items that cannot be "
                                    "mirrored were skipped." ) );
    }

    if( selection.IsHover() && !m_dragging )
        m_toolMgr->RunAction( ACTIONS::selectionClear );

    m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );

    if( m_dragging )
    {
        m_toolMgr->PostAction( PCB_ACTIONS::updateLocalRatsnest, VECTOR2I() );
        m_toolMgr->PostAction( PCB_ACTIONS::refreshPreview );
    }

    return 0;
}


int EDIT_TOOL::JustifyText( const TOOL_EVENT& aEvent )
{
    if( isRouterActive() )
    {
        wxBell();
        return 0;
    }

    BOARD_COMMIT  localCommit( this );
    BOARD_COMMIT* commit = dynamic_cast<BOARD_COMMIT*>( aEvent.Commit() );

    if( !commit )
        commit = &localCommit;

    PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                sTool->FilterCollectorForHierarchy( aCollector, true );
                sTool->FilterCollectorForLockedItems( aCollector );
            } );

    m_selectionTool->ReportFilteredLockedItems();

    if( selection.Empty() )
        return 0;

    auto setJustify = [&]( EDA_TEXT* aTextItem )
    {
        if( aEvent.Matches( ACTIONS::leftJustify.MakeEvent() ) )
            aTextItem->SetHorizJustify( GR_TEXT_H_ALIGN_LEFT );
        else if( aEvent.Matches( ACTIONS::centerJustify.MakeEvent() ) )
            aTextItem->SetHorizJustify( GR_TEXT_H_ALIGN_CENTER );
        else
            aTextItem->SetHorizJustify( GR_TEXT_H_ALIGN_RIGHT );
    };

    for( EDA_ITEM* item : selection )
    {
        if( item->Type() == PCB_FIELD_T || item->Type() == PCB_TEXT_T )
        {
            commit->Modify( item );
            setJustify( static_cast<PCB_TEXT*>( item ) );
        }
        else if( item->Type() == PCB_TEXTBOX_T )
        {
            commit->Modify( item );
            setJustify( static_cast<PCB_TEXTBOX*>( item ) );
        }
    }

    if( !localCommit.Empty() )
    {
        if( aEvent.Matches( ACTIONS::leftJustify.MakeEvent() ) )
            localCommit.Push( _( "Left Justify" ) );
        else if( aEvent.Matches( ACTIONS::centerJustify.MakeEvent() ) )
            localCommit.Push( _( "Center Justify" ) );
        else
            localCommit.Push( _( "Right Justify" ) );
    }

    if( selection.IsHover() && !m_dragging )
        m_toolMgr->RunAction( ACTIONS::selectionClear );

    m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );

    if( m_dragging )
    {
        m_toolMgr->PostAction( PCB_ACTIONS::updateLocalRatsnest, VECTOR2I() );
        m_toolMgr->PostAction( PCB_ACTIONS::refreshPreview );
    }

    return 0;
}


int EDIT_TOOL::Flip( const TOOL_EVENT& aEvent )
{
    if( isRouterActive() )
    {
        wxBell();
        return 0;
    }

    BOARD_COMMIT  localCommit( this );
    BOARD_COMMIT* commit = dynamic_cast<BOARD_COMMIT*>( aEvent.Commit() );

    if( !commit )
        commit = &localCommit;

    PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                sTool->FilterCollectorForMarkers( aCollector );
                sTool->FilterCollectorForHierarchy( aCollector, true );
                sTool->FilterCollectorForFreePads( aCollector );
                sTool->FilterCollectorForTableCells( aCollector );
                sTool->FilterCollectorForLockedItems( aCollector );
            } );

    m_selectionTool->ReportFilteredLockedItems();

    if( selection.Empty() )
        return 0;

    std::optional<VECTOR2I> oldRefPt;

    if( selection.HasReferencePoint() )
        oldRefPt = selection.GetReferencePoint();

    updateModificationPoint( selection );

    // Flip around the anchor for footprints, and the bounding box center for board items
    VECTOR2I refPt = IsFootprintEditor() ? VECTOR2I( 0, 0 ) : selection.GetCenter();

    if( m_dragging && m_inMoveWithReference && oldRefPt )
    {
        refPt = *oldRefPt;
    }
    else if( selection.GetSize() == 1 )
    {
        // If only one item selected, flip around the selection or item anchor point (instead
        // of the bounding box center) to avoid moving the item anchor
        // but only if the item is not a PCB_SHAPE with SHAPE_T::RECTANGLE shape, because
        // for this shape the flip transform swap start and end coordinates and move the shape.
        // So using the center of the shape is better (the shape does not move)
        // (Tables are a bunch of rectangles, so exclude them too)
        PCB_SHAPE* rect = dynamic_cast<PCB_SHAPE*>( selection.GetItem( 0 ) );
        PCB_TABLE* table = dynamic_cast<PCB_TABLE*>( selection.GetItem( 0 ) );

        if( !table && ( !rect || rect->GetShape() != SHAPE_T::RECTANGLE ) )
            refPt = selection.GetReferencePoint();
    }

    const FLIP_DIRECTION flipDirection = frame()->GetPcbNewSettings()->m_FlipDirection;

    for( EDA_ITEM* item : selection )
    {
        if( !item->IsBOARD_ITEM() )
            continue;

        BOARD_ITEM* boardItem = static_cast<BOARD_ITEM*>( item );

        commit->Modify( boardItem, nullptr, RECURSE_MODE::RECURSE );

        boardItem->Flip( refPt, flipDirection );
        boardItem->Normalize();

        if( boardItem->Type() == PCB_FOOTPRINT_T )
            static_cast<FOOTPRINT*>( boardItem )->InvalidateComponentClassCache();
    }

    // Don't push a separate undo entry when we're in the middle of a move operation.
    // The parent move will handle the commit.
    if( !localCommit.Empty() && !m_dragging )
    {
        localCommit.Push( _( "Change Side / Flip" ) );
        reSolveConstraintsAfterEdit( selection );
    }

    if( selection.IsHover() && !m_dragging )
        m_toolMgr->RunAction( ACTIONS::selectionClear );

    m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );

    if( m_dragging )
    {
        m_toolMgr->PostAction( PCB_ACTIONS::updateLocalRatsnest, VECTOR2I() );
        m_toolMgr->PostAction( PCB_ACTIONS::refreshPreview );
    }

    // Restore the old reference so any mouse dragging that occurs doesn't make the selection jump
    // to this now invalid reference
    if( oldRefPt )
        selection.SetReferencePoint( *oldRefPt );
    else
        selection.ClearReferencePoint();

    return 0;
}


void EDIT_TOOL::DeleteItems( const PCB_SELECTION& aItems, bool aIsCut )
{
    PCB_BASE_EDIT_FRAME* editFrame = getEditFrame<PCB_BASE_EDIT_FRAME>();
    BOARD_COMMIT         commit( this );
    int                  commitFlags = 0;

    // As we are about to remove items, they have to be removed from the selection first
    m_toolMgr->RunAction( ACTIONS::selectionClear );

    int itemsDeleted = 0;
    int fieldsHidden = 0;
    int fieldsAlreadyHidden = 0;

    for( EDA_ITEM* item : aItems )
    {
        if( !item->IsBOARD_ITEM() )
            continue;

        BOARD_ITEM* board_item = static_cast<BOARD_ITEM*>( item );
        FOOTPRINT*  parentFP = board_item->GetParentFootprint();

        switch( item->Type() )
        {
        case PCB_FIELD_T:
        {
            PCB_FIELD* field = static_cast<PCB_FIELD*>( board_item );

            wxASSERT( parentFP );
            commit.Modify( parentFP );

            if( field->IsVisible() )
            {
                field->SetVisible( false );
                fieldsHidden++;
            }
            else
            {
                fieldsAlreadyHidden++;
            }

            getView()->Update( parentFP );
            break;
        }

        case PCB_TEXT_T:
        case PCB_SHAPE_T:
        case PCB_TEXTBOX_T:
        case PCB_BARCODE_T:
        case PCB_TABLE_T:
        case PCB_REFERENCE_IMAGE_T:
        case PCB_DIMENSION_T:
        case PCB_DIM_ALIGNED_T:
        case PCB_DIM_LEADER_T:
        case PCB_DIM_CENTER_T:
        case PCB_DIM_RADIAL_T:
        case PCB_DIM_ORTHOGONAL_T:
        case PCB_POINT_T:
            commit.Remove( board_item );
            itemsDeleted++;
            break;

        case PCB_TABLECELL_T:
            // Clear contents of table cell
            commit.Modify( board_item );
            static_cast<PCB_TABLECELL*>( board_item )->SetText( wxEmptyString );
            itemsDeleted++;
            break;

        case PCB_GROUP_T:
            board_item->RunOnChildren(
                    [&commit]( BOARD_ITEM* aItem )
                    {
                        commit.Remove( aItem );
                    },
                    RECURSE_MODE::RECURSE );

            commit.Remove( board_item );
            itemsDeleted++;
            break;

        case PCB_PAD_T:
            if( IsFootprintEditor() || frame()->GetPcbNewSettings()->m_AllowFreePads )
            {
                commit.Remove( board_item );
                itemsDeleted++;
            }

            break;

        case PCB_ZONE_T:
            // We process the zones special so that cutouts can be deleted when the delete
            // tool is called from inside a cutout when the zone is selected.
            // Only interact with cutouts when deleting and a single item is selected
            if( !aIsCut && aItems.GetSize() == 1 )
            {
                VECTOR2I curPos = getViewControls()->GetCursorPosition();
                ZONE*    zone = static_cast<ZONE*>( board_item );

                int outlineIdx, holeIdx;

                if( zone->HitTestCutout( curPos, &outlineIdx, &holeIdx ) )
                {
                    // Remove the cutout
                    commit.Modify( zone );
                    zone->RemoveCutout( outlineIdx, holeIdx );
                    zone->UnFill();

                    // Update the display
                    zone->HatchBorder();
                    canvas()->Refresh();

                    // Restore the selection on the original zone
                    m_toolMgr->RunAction<EDA_ITEM*>( ACTIONS::selectItem, zone );

                    break;
                }
            }

            // Remove the entire zone otherwise
            commit.Remove( board_item );
            itemsDeleted++;
            break;

        case PCB_GENERATOR_T:
        {
            PCB_GENERATOR* generator = static_cast<PCB_GENERATOR*>( board_item );

            if( ( SELECTION_CONDITIONS::OnlyTypes( { PCB_GENERATOR_T } ) )( aItems ) )
            {
                m_toolMgr->RunSynchronousAction<PCB_GENERATOR*>( PCB_ACTIONS::genRemove, &commit, generator );
                commit.Push( _( "Delete" ), commitFlags );
                commitFlags |= APPEND_UNDO;
            }
            else
            {
                for( EDA_ITEM* member : generator->GetItems() )
                    commit.Remove( member );

                commit.Remove( board_item );
            }

            itemsDeleted++;
            break;
        }

        default:
            commit.Remove( board_item );
            itemsDeleted++;
            break;
        }
    }

    // If the entered group has been emptied then leave it.
    PCB_GROUP* enteredGroup = m_selectionTool->GetEnteredGroup();

    if( enteredGroup && enteredGroup->GetItems().empty() )
        m_selectionTool->ExitGroup();

    if( aIsCut )
    {
        commit.Push( _( "Cut" ), commitFlags );
    }
    else if( itemsDeleted == 0 )
    {
        if( fieldsHidden == 1 )
            commit.Push( _( "Hide Field" ), commitFlags );
        else if( fieldsHidden > 1 )
            commit.Push( _( "Hide Fields" ), commitFlags );
        else if( fieldsAlreadyHidden > 0 )
            editFrame->ShowInfoBarError( _( "Use the Footprint Properties dialog to remove fields." ) );
    }
    else
    {
        commit.Push( _( "Delete" ), commitFlags );
    }
}


int EDIT_TOOL::Remove( const TOOL_EVENT& aEvent )
{
    PCB_BASE_EDIT_FRAME* editFrame = getEditFrame<PCB_BASE_EDIT_FRAME>();

    // A geometric constraint selected by clicking its on-canvas badge has no board selection, so a
    // plain Delete targets it here before the normal item-removal path.  Cut is excluded: the
    // constraint is not on the clipboard, so it must not be silently removed (#2329).
    bool isCut = aEvent.Parameter<PCB_ACTIONS::REMOVE_FLAGS>() == PCB_ACTIONS::REMOVE_FLAGS::CUT;

    if( !isCut )
    {
        if( CONSTRAINT_EDIT_TOOL* constraintTool = m_toolMgr->GetTool<CONSTRAINT_EDIT_TOOL>();
            constraintTool && constraintTool->TryDeleteSelectedConstraint() )
        {
            return 0;
        }
    }

    editFrame->PushTool( aEvent );

    std::vector<BOARD_ITEM*> lockedItems;
    Activate();

    // get a copy instead of reference (as we're going to clear the selection before removing items)
    PCB_SELECTION selectionCopy;
    bool          isAlt = aEvent.Parameter<PCB_ACTIONS::REMOVE_FLAGS>() == PCB_ACTIONS::REMOVE_FLAGS::ALT;

    // If we are in a "Cut" operation, then the copied selection exists already and we want to
    // delete exactly that; no more, no fewer.  Any filtering for locked items must be done in
    // the copyToClipboard() routine.
    if( isCut )
    {
        selectionCopy = m_selectionTool->GetSelection();
    }
    else
    {
        // Hover-pick is only a fallback for an empty selection.
        const bool hadInitialSelection = !m_selectionTool->GetSelection().Empty();

        // When not in free-pad mode we normally auto-promote selected pads to their parent
        // footprints.  But this is probably a little too dangerous for a destructive operation,
        // so we just do the promotion but not the deletion (allowing for a second delete to do
        // it if that's what the user wanted).
        selectionCopy = m_selectionTool->RequestSelection(
                []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
                {
                    sTool->FilterCollectorForHierarchy( aCollector, true );
                    sTool->FilterCollectorForLockedItems( aCollector );
                } );

        m_selectionTool->ReportFilteredLockedItems();

        if( hadInitialSelection && selectionCopy.Empty() )
        {
            editFrame->PopTool( aEvent );
            return 0;
        }

        size_t beforeFPCount = selectionCopy.CountType( PCB_FOOTPRINT_T );

        selectionCopy = m_selectionTool->RequestSelection(
                []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
                {
                    sTool->FilterCollectorForHierarchy( aCollector, true );
                    sTool->FilterCollectorForFreePads( aCollector );
                    sTool->FilterCollectorForLockedItems( aCollector );
                } );

        if( !selectionCopy.IsHover() && m_selectionTool->GetSelection().CountType( PCB_FOOTPRINT_T ) > beforeFPCount )
        {
            wxBell();
            canvas()->Refresh();
            editFrame->PopTool( aEvent );
            return 0;
        }

        // In "alternative" mode, we expand selected track items to their full connection.
        if( isAlt && ( selectionCopy.HasType( PCB_TRACE_T ) || selectionCopy.HasType( PCB_VIA_T ) ) )
            m_toolMgr->RunAction( PCB_ACTIONS::selectConnection );

        selectionCopy = m_selectionTool->GetSelection();

        if( selectionCopy.Empty() )
        {
            editFrame->PopTool( aEvent );
            return 0;
        }
    }

    DeleteItems( selectionCopy, isCut );
    canvas()->Refresh();

    editFrame->PopTool( aEvent );
    return 0;
}


int EDIT_TOOL::MoveExact( const TOOL_EVENT& aEvent )
{
    if( isRouterActive() )
    {
        wxBell();
        return 0;
    }

    const PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                sTool->FilterCollectorForMarkers( aCollector );
                sTool->FilterCollectorForHierarchy( aCollector, true );
                sTool->FilterCollectorForFreePads( aCollector, false );
                sTool->FilterCollectorForTableCells( aCollector );
                sTool->FilterCollectorForLockedItems( aCollector );
            } );

    m_selectionTool->ReportFilteredLockedItems();

    if( selection.Empty() )
        return 0;

    VECTOR2I        translation;
    EDA_ANGLE       rotation;
    ROTATION_ANCHOR rotationAnchor = selection.Size() > 1 ? ROTATE_AROUND_SEL_CENTER : ROTATE_AROUND_ITEM_ANCHOR;

    // TODO: Implement a visible bounding border at the edge
    BOX2I sel_box = selection.GetBoundingBox();

    DIALOG_MOVE_EXACT dialog( frame(), translation, rotation, rotationAnchor, sel_box );
    int               ret = dialog.ShowModal();

    if( ret == wxID_OK )
    {
        BOARD_COMMIT commit( this );
        EDA_ANGLE    angle = rotation;
        VECTOR2I     rp = selection.GetCenter();
        VECTOR2I     selCenter( rp.x, rp.y );

        // Make sure the rotation is from the right reference point
        selCenter += translation;

        if( !frame()->GetPcbNewSettings()->m_Display.m_DisplayInvertYAxis )
            rotation = -rotation;

        for( EDA_ITEM* item : selection )
        {
            if( !item->IsBOARD_ITEM() )
                continue;

            BOARD_ITEM* boardItem = static_cast<BOARD_ITEM*>( item );

            commit.Modify( boardItem, nullptr, RECURSE_MODE::RECURSE );

            if( !boardItem->GetParent() || !boardItem->GetParent()->IsSelected() )
                boardItem->Move( translation );

            switch( rotationAnchor )
            {
            case ROTATE_AROUND_ITEM_ANCHOR: boardItem->Rotate( boardItem->GetPosition(), angle ); break;
            case ROTATE_AROUND_SEL_CENTER: boardItem->Rotate( selCenter, angle ); break;
            case ROTATE_AROUND_USER_ORIGIN: boardItem->Rotate( frame()->GetScreen()->m_LocalOrigin, angle ); break;
            case ROTATE_AROUND_AUX_ORIGIN:
                boardItem->Rotate( board()->GetDesignSettings().GetAuxOrigin(), angle );
                break;
            }

            if( !m_dragging )
                getView()->Update( boardItem );
        }

        commit.Push( _( "Move Exactly" ) );
        reSolveConstraintsAfterEdit( selection );

        if( selection.IsHover() )
            m_toolMgr->RunAction( ACTIONS::selectionClear );

        m_toolMgr->ProcessEvent( EVENTS::SelectedItemsModified );

        if( m_dragging )
        {
            m_toolMgr->PostAction( PCB_ACTIONS::updateLocalRatsnest, VECTOR2I() );
            m_toolMgr->PostAction( PCB_ACTIONS::refreshPreview );
        }
    }

    return 0;
}


int EDIT_TOOL::Duplicate( const TOOL_EVENT& aEvent )
{
    if( isRouterActive() )
    {
        wxBell();
        return 0;
    }

    bool increment = aEvent.IsAction( &PCB_ACTIONS::duplicateIncrement );

    // Be sure that there is at least one item that we can modify
    const PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I&, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                sTool->FilterCollectorForMarkers( aCollector );
                sTool->FilterCollectorForHierarchy( aCollector, true );
                sTool->FilterCollectorForFreePads( aCollector, true );
                sTool->FilterCollectorForTableCells( aCollector );
            } );

    if( selection.Empty() )
        return 0;

    // Duplicating tuning patterns alone is not supported
    if( selection.Size() == 1 && selection.CountType( PCB_GENERATOR_T ) )
        return 0;

    // we have a selection to work on now, so start the tool process
    PCB_BASE_EDIT_FRAME* editFrame = getEditFrame<PCB_BASE_EDIT_FRAME>();
    BOARD_COMMIT         commit( this );
    FOOTPRINT*           parentFootprint = nullptr;

    if( m_isFootprintEditor )
        parentFootprint = editFrame->GetBoard()->GetFirstFootprint();

    // If the selection was given a hover, we do not keep the selection after completion
    bool is_hover = selection.IsHover();

    std::vector<BOARD_ITEM*> new_items;
    new_items.reserve( selection.Size() );

    // Maps each duplicated original KIID to its duplicate so constraints between them
    // can be repointed at the copies once duplication finishes
    std::map<KIID, KIID> idMap;

    // Each selected item is duplicated and pushed to new_items list
    // Old selection is cleared, and new items are then selected.
    for( EDA_ITEM* item : selection )
    {
        if( !item->IsBOARD_ITEM() )
            continue;

        BOARD_ITEM* dupe_item = nullptr;
        BOARD_ITEM* orig_item = static_cast<BOARD_ITEM*>( item );

        if( !m_isFootprintEditor && orig_item->GetParentFootprint() )
        {
            // No sub-footprint modifications allowed outside of footprint editor
        }
        else
        {
            switch( orig_item->Type() )
            {
            case PCB_FOOTPRINT_T:
            case PCB_TEXT_T:
            case PCB_TEXTBOX_T:
            case PCB_BARCODE_T:
            case PCB_REFERENCE_IMAGE_T:
            case PCB_SHAPE_T:
            case PCB_TRACE_T:
            case PCB_ARC_T:
            case PCB_VIA_T:
            case PCB_ZONE_T:
            case PCB_TARGET_T:
            case PCB_POINT_T:
            case PCB_DIM_ALIGNED_T:
            case PCB_DIM_CENTER_T:
            case PCB_DIM_RADIAL_T:
            case PCB_DIM_ORTHOGONAL_T:
            case PCB_DIM_LEADER_T:
                if( m_isFootprintEditor )
                    dupe_item = parentFootprint->DuplicateItem( true, &commit, orig_item );
                else
                    dupe_item = orig_item->Duplicate( true, &commit );

                // Clear the selection flag here, otherwise the PCB_SELECTION_TOOL
                // will not properly select it later on
                dupe_item->ClearSelected();

                if( dupe_item->Type() == PCB_SHAPE_T && static_cast<PCB_SHAPE*>( dupe_item )->IsHatchedFill() )
                {
                    dupe_item->SetFlags( IS_NEW );
                }

                idMap[orig_item->m_Uuid] = dupe_item->m_Uuid;
                new_items.push_back( dupe_item );
                commit.Add( dupe_item );
                break;

            case PCB_FIELD_T:
                // PCB_FIELD items are specific items (not only graphic, but are properies)
                // and cannot be duplicated like other footprint items. So skip it:
                orig_item->ClearSelected();
                break;

            case PCB_PAD_T:
                dupe_item = parentFootprint->DuplicateItem( true, &commit, orig_item );

                if( increment && static_cast<PAD*>( dupe_item )->CanHaveNumber() )
                {
                    PAD_TOOL* padTool = m_toolMgr->GetTool<PAD_TOOL>();
                    wxString  padNumber = padTool->GetLastPadNumber();
                    padNumber = parentFootprint->GetNextPadNumber( padNumber );
                    padTool->SetLastPadNumber( padNumber );
                    static_cast<PAD*>( dupe_item )->SetNumber( padNumber );
                }

                // Clear the selection flag here, otherwise the PCB_SELECTION_TOOL
                // will not properly select it later on
                dupe_item->ClearSelected();

                idMap[orig_item->m_Uuid] = dupe_item->m_Uuid;
                new_items.push_back( dupe_item );
                commit.Add( dupe_item );
                break;

            case PCB_TABLE_T:
                // JEY TODO: tables
                break;

            case PCB_GENERATOR_T:
            case PCB_GROUP_T:
            {
                // DeepDuplicate maps original to duplicate KIID per descendant while cloning the group
                // so grouped constraints can be repointed unordered iteration blocks a later pairing
                dupe_item = static_cast<PCB_GROUP*>( orig_item )->DeepDuplicate( true, &commit, &idMap );

                dupe_item->RunOnChildren(
                        [&]( BOARD_ITEM* aItem )
                        {
                            aItem->ClearSelected();
                            new_items.push_back( aItem );
                            commit.Add( aItem );
                        },
                        RECURSE_MODE::RECURSE );

                dupe_item->ClearSelected();
                new_items.push_back( dupe_item );
                commit.Add( dupe_item );
                break;
            }

            default: UNIMPLEMENTED_FOR( orig_item->GetClass() ); break;
            }
        }
    }

    // Carry constraints whose members were all duplicated repointed at the copies
    // constraints are not selectable so add them to commit but keep them out of new selection
    const CONSTRAINTS& sourceConstraints = parentFootprint ? parentFootprint->Constraints()
                                                           : board()->Constraints();

    for( PCB_CONSTRAINT* clone : CloneFullySelectedConstraints( sourceConstraints, idMap ) )
        commit.Add( clone );

    // Clear the old selection first
    m_toolMgr->RunAction( ACTIONS::selectionClear );

    // Select the new items
    EDA_ITEMS nItems( new_items.begin(), new_items.end() );
    m_toolMgr->RunAction<EDA_ITEMS*>( ACTIONS::selectItems, &nItems );

    // record the new items as added
    if( !selection.Empty() )
    {
        editFrame->DisplayToolMsg( wxString::Format( _( "Duplicated %d item(s)" ), (int) new_items.size() ) );

        // If items were duplicated, pick them up
        if( doMoveSelection( aEvent, &commit, true ) )
            commit.Push( _( "Duplicate" ) );
        else
            commit.Revert();

        // Deselect the duplicated item if we originally started as a hover selection
        if( is_hover )
            m_toolMgr->RunAction( ACTIONS::selectionClear );
    }

    return 0;
}


int EDIT_TOOL::Increment( const TOOL_EVENT& aEvent )
{
    PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                for( int i = aCollector.GetCount() - 1; i >= 0; i-- )
                {
                    switch( aCollector[i]->Type() )
                    {
                    case PCB_PAD_T:
                    case PCB_TEXT_T: break;
                    default: aCollector.Remove( i ); break;
                    }
                }

                sTool->FilterCollectorForLockedItems( aCollector );
            } );

    m_selectionTool->ReportFilteredLockedItems();

    if( selection.Empty() )
        return 0;

    ACTIONS::INCREMENT param = { 1, 0 };

    if( aEvent.HasParameter() )
        param = aEvent.Parameter<ACTIONS::INCREMENT>();

    STRING_INCREMENTER incrementer;
    incrementer.SetSkipIOSQXZ( true );

    // If we're coming via another action like 'Move', use that commit
    BOARD_COMMIT  localCommit( m_toolMgr );
    BOARD_COMMIT* commit = dynamic_cast<BOARD_COMMIT*>( aEvent.Commit() );

    if( !commit )
        commit = &localCommit;

    for( EDA_ITEM* item : selection )
    {
        switch( item->Type() )
        {
        case PCB_PAD_T:
        {
            // Only increment pad numbers in the footprint editor
            if( !m_isFootprintEditor )
                break;

            PAD& pad = static_cast<PAD&>( *item );

            if( !pad.CanHaveNumber() )
                continue;

            // Increment on the pad numbers
            std::optional<wxString> newNumber = incrementer.Increment( pad.GetNumber(), param.Delta, param.Index );

            if( newNumber )
            {
                commit->Modify( &pad );
                pad.SetNumber( *newNumber );
            }

            break;
        }
        case PCB_TEXT_T:
        {
            PCB_TEXT& text = static_cast<PCB_TEXT&>( *item );

            std::optional<wxString> newText = incrementer.Increment( text.GetText(), param.Delta, param.Index );

            if( newText )
            {
                commit->Modify( &text );
                text.SetText( *newText );
            }

            break;
        }
        default: break;
        }
    }

    if( selection.Front()->IsMoving() )
        m_toolMgr->PostAction( ACTIONS::refreshPreview );

    commit->Push( _( "Increment" ) );

    return 0;
}


void EDIT_TOOL::PadFilter( const VECTOR2I&, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
{
    for( int i = aCollector.GetCount() - 1; i >= 0; i-- )
    {
        if( aCollector[i]->Type() != PCB_PAD_T )
            aCollector.Remove( i );
    }
}


void EDIT_TOOL::FootprintFilter( const VECTOR2I&, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
{
    for( int i = aCollector.GetCount() - 1; i >= 0; i-- )
    {
        if( aCollector[i]->Type() != PCB_FOOTPRINT_T )
            aCollector.Remove( i );
    }
}


bool EDIT_TOOL::updateModificationPoint( PCB_SELECTION& aSelection )
{
    // Can't modify an empty group
    if( aSelection.Empty() )
        return false;

    if( ( m_dragging || aSelection[0]->IsMoving() ) && aSelection.HasReferencePoint() )
        return false;

    // When there is only one item selected, the reference point is its position...
    if( aSelection.Size() == 1 && aSelection.Front()->Type() != PCB_TABLE_T )
    {
        if( aSelection.Front()->IsBOARD_ITEM() )
        {
            BOARD_ITEM* item = static_cast<BOARD_ITEM*>( aSelection.Front() );
            aSelection.SetReferencePoint( item->GetPosition() );
        }
    }
    // ...otherwise modify items with regard to the grid-snapped center position
    else
    {
        PCB_GRID_HELPER grid( m_toolMgr, frame()->GetMagneticItemsSettings() );
        VECTOR2I        refPt = aSelection.GetCenter();

        // Exclude text in the footprint editor if there's anything else selected
        if( m_isFootprintEditor )
        {
            BOX2I nonFieldsBBox;

            for( EDA_ITEM* item : aSelection.Items() )
            {
                if( !item->IsType( { PCB_TEXT_T, PCB_FIELD_T } ) )
                    nonFieldsBBox.Merge( item->GetBoundingBox() );
            }

            if( nonFieldsBBox.IsValid() )
                refPt = nonFieldsBBox.GetCenter();
        }

        aSelection.SetReferencePoint( grid.BestSnapAnchor( refPt, nullptr ) );
    }

    return true;
}


bool EDIT_TOOL::pickReferencePoint( const wxString& aTooltip, const wxString& aSuccessMessage,
                                    const wxString& aCanceledMessage, VECTOR2I& aReferencePoint )
{
    PCB_PICKER_TOOL*        picker = m_toolMgr->GetTool<PCB_PICKER_TOOL>();
    PCB_BASE_EDIT_FRAME*    editFrame = getEditFrame<PCB_BASE_EDIT_FRAME>();
    std::optional<VECTOR2I> pickedPoint;
    bool                    done = false;

    m_statusPopup->SetText( aTooltip );

    /// This allow the option of snapping in the tool
    picker->SetSnapping( true );
    picker->SetCursor( KICURSOR::PLACE );
    picker->ClearHandlers();

    const auto setPickerLayerSet =
            [&]()
            {
                MAGNETIC_SETTINGS* magSettings = editFrame->GetMagneticItemsSettings();
                LSET               layerFilter;

                if( !magSettings->allLayers )
                    layerFilter = LSET( { editFrame->GetActiveLayer() } );
                else
                    layerFilter = LSET::AllLayersMask();

                picker->SetLayerSet( layerFilter );
            };

    // Initial set
    setPickerLayerSet();

    picker->SetClickHandler(
            [&]( const VECTOR2D& aPoint ) -> bool
            {
                pickedPoint = aPoint;

                if( !aSuccessMessage.empty() )
                {
                    m_statusPopup->SetText( aSuccessMessage );
                    m_statusPopup->Expire( 800 );
                }
                else
                {
                    m_statusPopup->Hide();
                }

                return false; // we don't need any more points
            } );

    picker->SetMotionHandler(
            [&]( const VECTOR2D& aPos )
            {
                m_statusPopup->Move( KIPLATFORM::UI::GetMousePosition() + wxPoint( 20, -50 ) );
            } );

    picker->SetCancelHandler(
            [&]()
            {
                if( !aCanceledMessage.empty() )
                {
                    m_statusPopup->SetText( aCanceledMessage );
                    m_statusPopup->Expire( 800 );
                }
                else
                {
                    m_statusPopup->Hide();
                }
            } );

    picker->SetFinalizeHandler(
            [&]( const int& aFinalState )
            {
                done = true;
            } );

    m_statusPopup->Move( KIPLATFORM::UI::GetMousePosition() + wxPoint( 20, -50 ) );
    m_statusPopup->Popup();
    canvas()->SetStatusPopup( m_statusPopup->GetPanel() );

    m_toolMgr->RunAction( ACTIONS::pickerSubTool );

    while( !done )
    {
        // Pass events unless we receive a null event, then we must shut down
        if( TOOL_EVENT* evt = Wait() )
        {
            if( evt->Matches( PCB_EVENTS::SnappingModeChangedByKeyEvent() ) )
            {
                // Update the layer set when the snapping mode changes
                setPickerLayerSet();
            }

            evt->SetPassEvent();
        }
        else
        {
            break;
        }
    }

    picker->ClearHandlers();

    // Ensure statusPopup is hidden after use and before deleting it:
    canvas()->SetStatusPopup( nullptr );
    m_statusPopup->Hide();

    if( pickedPoint )
        aReferencePoint = *pickedPoint;

    return pickedPoint.has_value();
}


int EDIT_TOOL::copyToClipboard( const TOOL_EVENT& aEvent )
{
    CLIPBOARD_IO    io;
    PCB_GRID_HELPER grid( m_toolMgr, getEditFrame<PCB_BASE_EDIT_FRAME>()->GetMagneticItemsSettings() );
    TOOL_EVENT selectReferencePoint( aEvent.Category(), aEvent.Action(), "pcbnew.InteractiveEdit.selectReferencePoint",
                                     TOOL_ACTION_SCOPE::AS_GLOBAL );

    frame()->PushTool( selectReferencePoint );
    Activate();

    PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            [&]( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                sTool->FilterCollectorForHierarchy( aCollector, true );
                sTool->FilterCollectorForMarkers( aCollector );

                if( aEvent.IsAction( &ACTIONS::cut ) )
                    sTool->FilterCollectorForLockedItems( aCollector );
            } );

    m_selectionTool->ReportFilteredLockedItems();

    if( !selection.Empty() )
    {
        std::vector<BOARD_ITEM*> items;

        for( EDA_ITEM* item : selection )
        {
            if( item->IsBOARD_ITEM() )
                items.push_back( static_cast<BOARD_ITEM*>( item ) );
        }

        VECTOR2I refPoint;

        if( aEvent.IsAction( &PCB_ACTIONS::copyWithReference ) )
        {
            if( !pickReferencePoint( _( "Select reference point for the copy..." ), _( "Selection copied" ),
                                     _( "Copy canceled" ), refPoint ) )
            {
                frame()->PopTool( selectReferencePoint );
                return 0;
            }
        }
        else
        {
            refPoint = grid.BestDragOrigin( getViewControls()->GetCursorPosition(), items );
        }

        selection.SetReferencePoint( refPoint );

        io.SetBoard( board() );
        io.SaveSelection( selection, m_isFootprintEditor );
        frame()->SetStatusText( _( "Selection copied" ) );
    }

    frame()->PopTool( selectReferencePoint );

    if( selection.IsHover() )
        m_selectionTool->ClearSelection();

    return 0;
}


int EDIT_TOOL::copyToClipboardAsText( const TOOL_EVENT& aEvent )
{
    PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                // Anything unsupported will just be ignored
            } );

    if( selection.IsHover() )
        m_selectionTool->ClearSelection();

    const auto getItemText = [&]( const BOARD_ITEM& aItem ) -> wxString
    {
        switch( aItem.Type() )
        {
        case PCB_TEXT_T:
        case PCB_FIELD_T:
        case PCB_DIM_ALIGNED_T:
        case PCB_DIM_LEADER_T:
        case PCB_DIM_CENTER_T:
        case PCB_DIM_RADIAL_T:
        case PCB_DIM_ORTHOGONAL_T:
        {
            // These can all go via the PCB_TEXT class
            const PCB_TEXT& text = static_cast<const PCB_TEXT&>( aItem );
            return text.GetShownText( true );
        }
        case PCB_TEXTBOX_T:
        case PCB_TABLECELL_T:
        {
            // This one goes via EDA_TEXT
            const PCB_TEXTBOX& textBox = static_cast<const PCB_TEXTBOX&>( aItem );
            return textBox.GetShownText( true );
        }
        case PCB_TABLE_T:
        {
            const PCB_TABLE& table = static_cast<const PCB_TABLE&>( aItem );
            wxString         s;

            for( int row = 0; row < table.GetRowCount(); ++row )
            {
                for( int col = 0; col < table.GetColCount(); ++col )
                {
                    const PCB_TABLECELL* cell = table.GetCell( row, col );
                    s << cell->GetShownText( true );

                    if( col < table.GetColCount() - 1 )
                    {
                        s << '\t';
                    }
                }

                if( row < table.GetRowCount() - 1 )
                {
                    s << '\n';
                }
            }
            return s;
        }
        default:
            // No string representation for this item type
            break;
        }
        return wxEmptyString;
    };

    wxArrayString itemTexts;

    for( EDA_ITEM* item : selection )
    {
        if( item->IsBOARD_ITEM() )
        {
            BOARD_ITEM* boardItem = static_cast<BOARD_ITEM*>( item );
            wxString    itemText = getItemText( *boardItem );

            itemText.Trim( false ).Trim( true );

            if( !itemText.IsEmpty() )
            {
                itemTexts.Add( std::move( itemText ) );
            }
        }
    }

    // Send the text to the clipboard
    if( !itemTexts.empty() )
    {
        SaveClipboard( wxJoin( itemTexts, '\n', '\0' ).ToStdString() );
    }

    return 0;
}


int EDIT_TOOL::cutToClipboard( const TOOL_EVENT& aEvent )
{
    if( !copyToClipboard( aEvent ) )
    {
        // N.B. Setting the CUT flag prevents lock filtering as we only want to delete the items
        // that were copied to the clipboard, no more, no fewer.  Filtering for locked item, if
        // any will be done in the copyToClipboard() routine
        TOOL_EVENT evt = aEvent;
        evt.SetParameter( PCB_ACTIONS::REMOVE_FLAGS::CUT );
        Remove( evt );
    }

    return 0;
}


void EDIT_TOOL::rebuildConnectivity()
{
    board()->BuildConnectivity();
    m_toolMgr->PostEvent( EVENTS::ConnectivityChangedEvent );
    canvas()->RedrawRatsnest();
}


// clang-format off
void EDIT_TOOL::setTransitions()
{
    Go( &EDIT_TOOL::GetAndPlace,           PCB_ACTIONS::getAndPlace.MakeEvent() );
    Go( &EDIT_TOOL::Move,                  PCB_ACTIONS::move.MakeEvent() );
    Go( &EDIT_TOOL::Move,                  PCB_ACTIONS::moveIndividually.MakeEvent() );
    Go( &EDIT_TOOL::Drag,                  PCB_ACTIONS::drag45Degree.MakeEvent() );
    Go( &EDIT_TOOL::Drag,                  PCB_ACTIONS::dragFreeAngle.MakeEvent() );
    Go( &EDIT_TOOL::Rotate,                PCB_ACTIONS::rotateCw.MakeEvent() );
    Go( &EDIT_TOOL::Rotate,                PCB_ACTIONS::rotateCcw.MakeEvent() );
    Go( &EDIT_TOOL::Flip,                  PCB_ACTIONS::flip.MakeEvent() );
    Go( &EDIT_TOOL::Remove,                ACTIONS::doDelete.MakeEvent() );
    Go( &EDIT_TOOL::Remove,                PCB_ACTIONS::deleteFull.MakeEvent() );
    Go( &EDIT_TOOL::Properties,            PCB_ACTIONS::properties.MakeEvent() );
    Go( &EDIT_TOOL::MatchProperties,       PCB_ACTIONS::matchProperties.MakeEvent() );
    Go( &EDIT_TOOL::ExtendTrack,           PCB_ACTIONS::extendTrack.MakeEvent() );
    Go( &EDIT_TOOL::TrimTrack,             PCB_ACTIONS::trimTrack.MakeEvent() );
    Go( &EDIT_TOOL::MoveExact,             PCB_ACTIONS::moveExact.MakeEvent() );
    Go( &EDIT_TOOL::Move,                  PCB_ACTIONS::moveWithReference.MakeEvent() );
    Go( &EDIT_TOOL::Duplicate,             ACTIONS::duplicate.MakeEvent() );
    Go( &EDIT_TOOL::Duplicate,             PCB_ACTIONS::duplicateIncrement.MakeEvent() );
    Go( &EDIT_TOOL::Mirror,                PCB_ACTIONS::mirrorH.MakeEvent() );
    Go( &EDIT_TOOL::Mirror,                PCB_ACTIONS::mirrorV.MakeEvent() );
    Go( &EDIT_TOOL::Swap,                  PCB_ACTIONS::swap.MakeEvent() );
    Go( &EDIT_TOOL::SwapPadNets,           PCB_ACTIONS::swapPadNets.MakeEvent() );
    Go( &EDIT_TOOL::SwapGateNets,          PCB_ACTIONS::swapGateNets.MakeEvent() );
    Go( &EDIT_TOOL::PackAndMoveFootprints,    PCB_ACTIONS::packAndMoveFootprints.MakeEvent() );
    Go( &EDIT_TOOL::ToggleFootprintAttribute, PCB_ACTIONS::toggleExcludeFromBOM.MakeEvent() );
    Go( &EDIT_TOOL::ToggleFootprintAttribute, PCB_ACTIONS::toggleExcludeFromPosFiles.MakeEvent() );
    Go( &EDIT_TOOL::ChangeTrackWidth,         PCB_ACTIONS::changeTrackWidth.MakeEvent() );
    Go( &EDIT_TOOL::ChangeTrackLayer,      PCB_ACTIONS::changeTrackLayerNext.MakeEvent() );
    Go( &EDIT_TOOL::ChangeTrackLayer,      PCB_ACTIONS::changeTrackLayerPrev.MakeEvent() );
    Go( &EDIT_TOOL::FilletTracks,          PCB_ACTIONS::filletTracks.MakeEvent() );
    Go( &EDIT_TOOL::ModifyLines,           PCB_ACTIONS::filletLines.MakeEvent() );
    Go( &EDIT_TOOL::ModifyLines,           PCB_ACTIONS::chamferLines.MakeEvent() );
    Go( &EDIT_TOOL::ModifyLines,           PCB_ACTIONS::dogboneCorners.MakeEvent() );
    Go( &EDIT_TOOL::SimplifyPolygons,      PCB_ACTIONS::simplifyPolygons.MakeEvent() );
    Go( &EDIT_TOOL::EditVertices,          PCB_ACTIONS::editVertices.MakeEvent() );
    Go( &EDIT_TOOL::HealShapes,            PCB_ACTIONS::healShapes.MakeEvent() );
    Go( &EDIT_TOOL::ModifyLines,           PCB_ACTIONS::extendLines.MakeEvent() );

    Go( &EDIT_TOOL::Increment,             ACTIONS::increment.MakeEvent() );
    Go( &EDIT_TOOL::Increment,             ACTIONS::incrementPrimary.MakeEvent() );
    Go( &EDIT_TOOL::Increment,             ACTIONS::decrementPrimary.MakeEvent() );
    Go( &EDIT_TOOL::Increment,             ACTIONS::incrementSecondary.MakeEvent() );
    Go( &EDIT_TOOL::Increment,             ACTIONS::decrementSecondary.MakeEvent() );

    Go( &EDIT_TOOL::BooleanPolygons,       PCB_ACTIONS::mergePolygons.MakeEvent() );
    Go( &EDIT_TOOL::BooleanPolygons,       PCB_ACTIONS::subtractPolygons.MakeEvent() );
    Go( &EDIT_TOOL::BooleanPolygons,       PCB_ACTIONS::intersectPolygons.MakeEvent() );

    Go( &EDIT_TOOL::JustifyText,           ACTIONS::leftJustify.MakeEvent() );
    Go( &EDIT_TOOL::JustifyText,           ACTIONS::centerJustify.MakeEvent() );
    Go( &EDIT_TOOL::JustifyText,           ACTIONS::rightJustify.MakeEvent() );

    Go( &EDIT_TOOL::copyToClipboard,       ACTIONS::copy.MakeEvent() );
    Go( &EDIT_TOOL::copyToClipboard,       PCB_ACTIONS::copyWithReference.MakeEvent() );
    Go( &EDIT_TOOL::copyToClipboardAsText, ACTIONS::copyAsText.MakeEvent() );
    Go( &EDIT_TOOL::cutToClipboard,        ACTIONS::cut.MakeEvent() );
}
// clang-format on
