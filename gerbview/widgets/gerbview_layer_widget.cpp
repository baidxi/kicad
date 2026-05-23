/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2004-2010 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 2010 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
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

#include <algorithm>
#include <functional>
#include <vector>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/filedlg.h>
#include <wx/font.h>
#include <wx/menu.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <settings/color_settings.h>
#include <bitmaps.h>
#include <gerbview.h>
#include "gerbview_draw_panel_gal.h"
#include <gerbview_frame.h>
#include <gerbview_settings.h>
#include <gerber_file_image.h>
#include <gerber_file_image_list.h>
#include <core/arraydim.h>
#include <layer_roles.h>
#include <lset.h>
#include <view/view.h>
#include <gerbview_painter.h>
#include <gal/graphics_abstraction_layer.h>
#include <settings/settings_manager.h>
#include <tool/tool_manager.h>
#include <tools/gerbview_actions.h>

#include "layer_widget.h"
#include "gbr_layer_box_selector.h"
#include "gerbview_layer_widget.h"
#include "dcode_selection_box.h"


/// Layer-Manager group-header rows are tagged with a positive decoded id
/// starting here so they can be distinguished from real layer slot ids
/// (0..GERBER_DRAWLAYERS_COUNT-1).  The base must satisfy two wxWidgets
/// invariants checked in wxControlBase::CreateBase():
///   * positive (avoid the negative auto-id range that needs wxRegisterId)
///   * encodeId(col, base+groupIdx) must stay below 32767 (signed 16-bit limit)
/// 6000 sits comfortably above wxID_HIGHEST (5999) and worst-case encoded value
/// (6003 * 5 + 4 = 30019) is well within the 16-bit limit.
static constexpr int HEADER_ROW_ID_BASE = 6000;

static inline bool IsHeaderRowDecodedId( int aId )
{
    return aId >= HEADER_ROW_ID_BASE;
}


namespace
{

/**
 * Per-layer role picker used inside the Layer Manager.  Renders as a small
 * flat button showing the role's three-letter code (e.g. "SPT"); clicking it
 * pops up a wxMenu listing "CODE  Full Role Name" entries (e.g.
 * "SPT  Top Solder Paste").  Selecting an entry fires the supplied callback.
 *
 * We use wxButton+wxMenu instead of wxOwnerDrawnComboBox because the latter's
 * built-in dropdown arrow cannot be hidden on Windows (SetButtonPosition(0)
 * is ignored by the native theme), which would clip the 3-letter code.
 */
class ROLE_DROPDOWN : public wxButton
{
public:
    using RoleChangedHandler = std::function<void( GERBVIEW_LAYER_ROLE )>;

    ROLE_DROPDOWN( wxWindow* aParent, GERBVIEW_LAYER_ROLE aCurrentRole, int aVisibleInnerCount,
                   RoleChangedHandler aOnChanged ) :
            wxButton( aParent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT ),
            m_onChanged( std::move( aOnChanged ) )
    {
        // Populate the role list, respecting the visible-inner-count setting.
        for( GERBVIEW_LAYER_ROLE role : AllRoles() )
        {
            int innerIdx = InnerCopperIndex( role );

            if( innerIdx > aVisibleInnerCount )
                continue;

            m_roles.push_back( role );
        }

        // Set the button label to the current role's short code.
        wxString currentCode = RoleShortCode( aCurrentRole );

        if( currentCode.IsEmpty() )
            currentCode = wxT( "---" );

        SetLabel( currentCode );

        // Size the button so it's just wide enough for the widest code we
        // could ever show.  We deliberately add only a tiny visual padding;
        // wxBU_EXACTFIT keeps the button compact.
        int maxW = 0;

        for( GERBVIEW_LAYER_ROLE r : m_roles )
        {
            wxString code = RoleShortCode( r );

            if( code.IsEmpty() )
                code = wxT( "---" );

            maxW = std::max( maxW, GetTextExtent( code ).GetWidth() );
        }

        const int kPad = 14; // borders + a couple of pixels left/right
        wxSize    desired( maxW + kPad, -1 );

        SetMinSize( desired );
        SetSize( desired );

        Bind( wxEVT_BUTTON, &ROLE_DROPDOWN::onClick, this );
    }

private:
    static constexpr int kMenuIdBase = 8000;

    void onClick( wxCommandEvent& /*aEvt*/ )
    {
        wxMenu menu;

        for( size_t i = 0; i < m_roles.size(); ++i )
        {
            wxString code = RoleShortCode( m_roles[i] );

            if( code.IsEmpty() )
                code = wxT( "---" );

            wxString entry = code + wxT( "  " ) + RoleDisplayName( m_roles[i] );
            menu.Append( kMenuIdBase + (int) i, entry );
        }

        int chosen = GetPopupMenuSelectionFromUser( menu, wxPoint( 0, GetSize().GetHeight() ) );

        if( chosen == wxID_NONE )
            return;

        size_t idx = (size_t) ( chosen - kMenuIdBase );

        if( idx < m_roles.size() && m_onChanged )
            m_onChanged( m_roles[idx] );
    }

    std::vector<GERBVIEW_LAYER_ROLE> m_roles;
    RoleChangedHandler               m_onChanged;
};

} // namespace


GERBER_LAYER_GROUP GroupForRole( GERBVIEW_LAYER_ROLE aRole )
{
    if( IsInnerCopperRole( aRole ) )
        return GERBER_LAYER_GROUP::Inner;

    switch( aRole )
    {
    case GERBVIEW_LAYER_ROLE::TopSolderPaste:
    case GERBVIEW_LAYER_ROLE::TopSolderMask:
    case GERBVIEW_LAYER_ROLE::TopCopper:
    case GERBVIEW_LAYER_ROLE::TopSilkScreen: return GERBER_LAYER_GROUP::Top;

    case GERBVIEW_LAYER_ROLE::BottomCopper:
    case GERBVIEW_LAYER_ROLE::BottomSolderMask:
    case GERBVIEW_LAYER_ROLE::BottomSolderPaste:
    case GERBVIEW_LAYER_ROLE::BottomSilkScreen: return GERBER_LAYER_GROUP::Bottom;

    case GERBVIEW_LAYER_ROLE::DrillPlated:
    case GERBVIEW_LAYER_ROLE::DrillUnplated:
    case GERBVIEW_LAYER_ROLE::ContourPlated:
    case GERBVIEW_LAYER_ROLE::ContourUnplated: return GERBER_LAYER_GROUP::Drill;

    case GERBVIEW_LAYER_ROLE::Unassigned:
    default: return GERBER_LAYER_GROUP::Unmapped;
    }
}


GERBER_LAYER_GROUP EffectiveGroupForImage( const GERBER_FILE_IMAGE* aImg, GERBVIEW_LAYER_ROLE aRole )
{
    if( aRole != GERBVIEW_LAYER_ROLE::Unassigned )
        return GroupForRole( aRole );

    if( !aImg )
        return GERBER_LAYER_GROUP::Unmapped;

    // Excellon drill files are universally identifiable by extension, so we
    // promote them into the Drill section even when the user hasn't picked a
    // specific drill role yet.  This keeps drill data visually on top in the
    // Layer Manager just like it already is on top of the canvas (slot 0 ==
    // top of the Z-stack), avoiding the "I see drills on the canvas but they
    // are hidden under Unmapped" inconsistency.
    GERBER_ORDER_ENUM order = GERBER_ORDER_ENUM::GERBER_LAYER_UNKNOWN;
    wxString          matchedExt;

    GERBER_FILE_IMAGE_LIST::GetGerberLayerFromFilename( aImg->m_FileName, order, matchedExt );

    if( order == GERBER_ORDER_ENUM::GERBER_DRILL )
        return GERBER_LAYER_GROUP::Drill;

    return GERBER_LAYER_GROUP::Unmapped;
}


GERBER_LAYER_GROUP EffectiveGroupForSlot( GERBVIEW_FRAME* aFrame, int aSlot )
{
    if( !aFrame )
        return GERBER_LAYER_GROUP::Unmapped;

    GERBER_FILE_IMAGE_LIST* list = &GERBER_FILE_IMAGE_LIST::GetImagesList();
    GERBER_FILE_IMAGE*      img = list->GetGbrImage( aSlot );

    if( !img )
        return GERBER_LAYER_GROUP::Unmapped;

    return EffectiveGroupForImage( img, LookupRoleForSlot( aFrame, aSlot ) );
}


wxString GroupDisplayName( GERBER_LAYER_GROUP aGroup )
{
    switch( aGroup )
    {
    case GERBER_LAYER_GROUP::Top: return _( "Top" );
    case GERBER_LAYER_GROUP::Inner: return _( "Inner" );
    case GERBER_LAYER_GROUP::Bottom: return _( "Bottom" );
    case GERBER_LAYER_GROUP::Drill: return _( "Drill" );
    case GERBER_LAYER_GROUP::Unmapped: return _( "Unmapped" );
    default: return wxEmptyString;
    }
}


wxString GroupShortCode( GERBER_LAYER_GROUP aGroup )
{
    // Codes match those displayed in the bold group-header row of the
    // layer manager.
    switch( aGroup )
    {
    case GERBER_LAYER_GROUP::Top: return wxT( "TOP" );
    case GERBER_LAYER_GROUP::Inner: return wxT( "INN" );
    case GERBER_LAYER_GROUP::Bottom: return wxT( "BOT" );
    case GERBER_LAYER_GROUP::Drill: return wxT( "DRL" );
    case GERBER_LAYER_GROUP::Unmapped: return wxT( "UMP" );
    default: return wxEmptyString;
    }
}


int EffectiveRoleOrderInGroup( GERBER_LAYER_GROUP aGroup, GERBVIEW_LAYER_ROLE aRole )
{
    // Unmapped slots only end up in the Drill bucket via extension promotion
    // (EffectiveGroupForImage).  We want every such slot to sit below all
    // files that picked an explicit Drill/Contour role, so push them past the
    // highest possible role-order (3 today).
    if( aRole == GERBVIEW_LAYER_ROLE::Unassigned && aGroup == GERBER_LAYER_GROUP::Drill )
    {
        return 100;
    }

    return RoleOrderInGroup( aRole );
}


int RoleOrderInGroup( GERBVIEW_LAYER_ROLE aRole )
{
    // Top side: paste, mask, copper, silk (silk last so it sits closest to
    // the inner copper rows below it visually).
    switch( aRole )
    {
    case GERBVIEW_LAYER_ROLE::TopSolderPaste: return 0;
    case GERBVIEW_LAYER_ROLE::TopSolderMask: return 1;
    case GERBVIEW_LAYER_ROLE::TopCopper: return 2;
    case GERBVIEW_LAYER_ROLE::TopSilkScreen: return 3;

    // Bottom side: paste, mask, copper, silk -- mirrors the visual stack
    // when looking at the underside of the board.
    case GERBVIEW_LAYER_ROLE::BottomSolderPaste: return 0;
    case GERBVIEW_LAYER_ROLE::BottomSolderMask: return 1;
    case GERBVIEW_LAYER_ROLE::BottomCopper: return 2;
    case GERBVIEW_LAYER_ROLE::BottomSilkScreen: return 3;

    case GERBVIEW_LAYER_ROLE::DrillPlated: return 0;
    case GERBVIEW_LAYER_ROLE::DrillUnplated: return 1;
    case GERBVIEW_LAYER_ROLE::ContourPlated: return 2;
    case GERBVIEW_LAYER_ROLE::ContourUnplated: return 3;

    default:
        // Inner copper: sort by InnerCopperIndex; Unmapped: any constant.
        int idx = InnerCopperIndex( aRole );

        if( idx > 0 )
            return idx - 1;

        return 0;
    }
}


GERBER_LAYER_WIDGET::GERBER_LAYER_WIDGET( GERBVIEW_FRAME* aParent, wxWindow* aFocusOwner ) :
    LAYER_WIDGET( aParent, aFocusOwner ),
    m_frame( aParent )
{
    m_alwaysShowActiveLayer = false;

    ReFillRender();

    // Update default tabs labels for GerbView
    SetLayersManagerTabsText( );

    // handle the popup menu over the layer window.
    m_LayerScrolledWindow->Connect( wxEVT_RIGHT_DOWN,
                                    wxMouseEventHandler( GERBER_LAYER_WIDGET::onRightDownLayers ),
                                    nullptr, this );

    // since Popupmenu() calls this->ProcessEvent() we must call this->Connect()
    // and not m_LayerScrolledWindow->Connect()
    Connect( ID_LAYER_MANAGER_START, ID_LAYER_MANAGER_END, wxEVT_COMMAND_MENU_SELECTED,
             wxCommandEventHandler( GERBER_LAYER_WIDGET::onPopupSelection ), nullptr, this );
}


GERBER_FILE_IMAGE_LIST* GERBER_LAYER_WIDGET::GetImagesList()
{
    return &GERBER_FILE_IMAGE_LIST::GetImagesList();
}


void GERBER_LAYER_WIDGET::SetLayersManagerTabsText()
{
    m_notebook->SetPageText( 0, _( "Layers" ) );
    m_notebook->SetPageText( 1, _( "Items" ) );
}


void GERBER_LAYER_WIDGET::CollectCurrentColorSettings(  COLOR_SETTINGS* aColorSettings )
{
    std::vector<int>render_layers{ LAYER_DCODES, LAYER_NEGATIVE_OBJECTS, LAYER_GERBVIEW_GRID,
                            LAYER_GERBVIEW_DRAWINGSHEET, LAYER_GERBVIEW_PAGE_LIMITS,
                            LAYER_GERBVIEW_BACKGROUND };

    for( int layer: render_layers )
    {
        int row = findRenderRow( layer );

        if( row < 0 )
            continue;

        COLOR4D color = GetRenderColor( row );

        if( color != COLOR4D::UNSPECIFIED )
            aColorSettings->SetColor( layer, color );
    }

    for( int layer = GERBVIEW_LAYER_ID_START; layer < GERBVIEW_LAYER_ID_START + GERBER_DRAWLAYERS_COUNT; layer++ )
    {
        int row = findLayerRow( layer - GERBVIEW_LAYER_ID_START );

        if( row < 0 )   // Not existing in layer list
            continue;

        COLOR4D color = GetLayerColor( row );

        if( color != COLOR4D::UNSPECIFIED )
            aColorSettings->SetColor( layer, color );
    }
}


void GERBER_LAYER_WIDGET::ReFillRender()
{
    ClearRenderRows();

    // Fixed "Rendering" tab rows within the LAYER_WIDGET, only the initial color
    // is changed before appending to the LAYER_WIDGET.  This is an automatic variable
    // not a static variable, change the color & state after copying from code to renderRows
    // on the stack.
    LAYER_WIDGET::ROW renderRows[7] = {

#define RR  LAYER_WIDGET::ROW   // Render Row abbreviation to reduce source width

        RR( _( "DCodes" ),           LAYER_DCODES,                WHITE,
            _( "Show DCodes identification" ) ),
        RR( _( "Negative Objects" ), LAYER_NEGATIVE_OBJECTS,      DARKGRAY,
            _( "Show negative objects in this color" ) ),
        RR(),
        RR( _( "Grid" ),             LAYER_GERBVIEW_GRID,         WHITE,
            _( "Show the (x,y) grid dots" ) ),
        RR( _( "Drawing Sheet" ),    LAYER_GERBVIEW_DRAWINGSHEET, DARKRED,
            _( "Show drawing sheet border and title block") ),
        RR( _( "Page Limits" ),      LAYER_GERBVIEW_PAGE_LIMITS,  WHITE,
            _( "Show drawing sheet page limits" ) ),
        RR( _( "Background" ),       LAYER_GERBVIEW_BACKGROUND,   BLACK,
            _( "PCB Background" ), true, false )
    };

    for( unsigned row = 0; row < arrayDim( renderRows ); ++row )
    {
        if( renderRows[row].color != COLOR4D::UNSPECIFIED )       // does this row show a color?
            renderRows[row].color = m_frame->GetVisibleElementColor( renderRows[row].id );

        if( renderRows[row].id )    // if not the separator
            renderRows[row].state = m_frame->IsElementVisible( renderRows[row].id );
    }

    AppendRenderRows( renderRows, arrayDim(renderRows) );
}


void GERBER_LAYER_WIDGET::AddRightClickMenuItems( wxMenu* aMenu )
{
    // Remember: menu text is capitalized (see our rules_for_capitalization_in_Kicad_UI.txt)
    KIUI::AddMenuItem( aMenu, ID_SHOW_ALL_LAYERS, _( "Show All Layers" ),
                       KiBitmap( BITMAPS::show_all_layers ) );

    KIUI::AddMenuItem( aMenu, ID_SHOW_NO_LAYERS_BUT_ACTIVE, _( "Hide All Layers But Active" ),
                       KiBitmap( BITMAPS::select_w_layer ) );

    KIUI::AddMenuItem( aMenu, ID_ALWAYS_SHOW_NO_LAYERS_BUT_ACTIVE,
                       _( "Always Hide All Layers But Active" ),
                       KiBitmap( BITMAPS::select_w_layer ) );

    KIUI::AddMenuItem( aMenu, ID_SHOW_NO_LAYERS, _( "Hide All Layers" ),
                       KiBitmap( BITMAPS::show_no_layers ) );

    aMenu->AppendSeparator();

    KIUI::AddMenuItem( aMenu, ID_SORT_GBR_LAYERS_X2, _( "Sort Layers if X2 Mode" ),
                       KiBitmap( BITMAPS::reload ) );

    KIUI::AddMenuItem( aMenu, ID_SORT_GBR_LAYERS_FILE_EXT, _( "Sort Layers by File Extension" ),
                       KiBitmap( BITMAPS::reload ) );

    aMenu->AppendSeparator();

    KIUI::AddMenuItem( aMenu, ID_SET_GBR_LAYERS_DRAW_PRMS,
                       _( "Layers Display Parameters: Offset and Rotation" ),
                       KiBitmap( BITMAPS::tools ) );

    aMenu->AppendSeparator();

    KIUI::AddMenuItem( aMenu, ID_LAYER_MOVE_UP, _( "Move Current Layer Up" ),
                       KiBitmap( BITMAPS::up ) );

    KIUI::AddMenuItem( aMenu, ID_LAYER_MOVE_DOWN, _( "Move Current Layer Down" ),
                       KiBitmap( BITMAPS::down ) );

    KIUI::AddMenuItem( aMenu, ID_LAYER_DELETE, _( "Clear Current Layer..." ),
                       KiBitmap( BITMAPS::delete_gerber ) );
}


void GERBER_LAYER_WIDGET::onRightDownLayers( wxMouseEvent& event )
{
    wxMenu          menu;

    AddRightClickMenuItems( &menu );
    PopupMenu( &menu );

    passOnFocus();
}


void GERBER_LAYER_WIDGET::onPopupSelection( wxCommandEvent& event )
{
    int  layer;
    int  rowCount;
    int  menuId = event.GetId();
    bool visible = (menuId == ID_SHOW_ALL_LAYERS) ? true : false;
    LSET visibleLayers;
    bool force_active_layer_visible;

    switch( menuId )
    {
    case ID_SHOW_ALL_LAYERS:
    case ID_SHOW_NO_LAYERS:
    case ID_ALWAYS_SHOW_NO_LAYERS_BUT_ACTIVE:
    case ID_SHOW_NO_LAYERS_BUT_ACTIVE:
        // Set the display layers options. Sorting layers has no effect to these options
        m_alwaysShowActiveLayer = ( menuId == ID_ALWAYS_SHOW_NO_LAYERS_BUT_ACTIVE );
        force_active_layer_visible = ( menuId == ID_SHOW_NO_LAYERS_BUT_ACTIVE ||
                                       menuId == ID_ALWAYS_SHOW_NO_LAYERS_BUT_ACTIVE );

        // Update icons and check boxes
        rowCount = GetLayerRowCount();

        for( int row = 0; row < rowCount; ++row )
        {
            wxCheckBox* cb = (wxCheckBox*) getLayerComp( row, COLUMN_COLOR_LYR_CB );
            layer = getDecodedId( cb->GetId() );

            // Group-header rows carry a decoded id outside the slot range;
            // skip them so we don't index the LSET out of range or accidentally
            // toggle them here (their tri-state is reconciled below).
            if( layer < 0 || IsHeaderRowDecodedId( layer ) )
                continue;

            bool loc_visible = visible;

            if( force_active_layer_visible && (layer == m_frame->GetActiveLayer() ) )
                loc_visible = true;

            cb->SetValue( loc_visible );
            visibleLayers[ layer ] = loc_visible;
        }

        m_frame->SetVisibleLayers( visibleLayers );
        syncGroupMasterCheckboxes();
        m_frame->GetCanvas()->Refresh();
        break;

    case ID_SORT_GBR_LAYERS_X2:
        m_frame->SortLayersByX2Attributes();
        break;

    case ID_SORT_GBR_LAYERS_FILE_EXT:
        m_frame->SortLayersByFileExtension();
        break;

    case ID_SET_GBR_LAYERS_DRAW_PRMS:
        m_frame->SetLayerDrawPrms();
        break;

    case ID_LAYER_MOVE_UP:
        m_frame->GetToolManager()->RunAction( GERBVIEW_ACTIONS::moveLayerUp );
        break;

    case ID_LAYER_MOVE_DOWN:
        m_frame->GetToolManager()->RunAction( GERBVIEW_ACTIONS::moveLayerDown );
        break;

    case ID_LAYER_DELETE:
        m_frame->Erase_Current_DrawLayer( false );

        break;
    }
}

bool  GERBER_LAYER_WIDGET::OnLayerSelected()
{
    if( !m_alwaysShowActiveLayer )
        return false;

    // postprocess after active layer selection ensure active layer visible
    wxCommandEvent event;
    event.SetId( ID_ALWAYS_SHOW_NO_LAYERS_BUT_ACTIVE );
    onPopupSelection( event );
    return true;
}


void GERBER_LAYER_WIDGET::ReFill()
{
    ClearLayerRows();
    m_groupCheckboxes.clear();

    Freeze();

    struct LoadedSlot
    {
        int                 slot;
        GERBVIEW_LAYER_ROLE role;
        GERBER_LAYER_GROUP  group;
        int                 roleOrder;
    };

    // Bucket every loaded slot by its group so we can emit empty group
    // headers too (the layer manager always shows the full set of sections).
    std::map<GERBER_LAYER_GROUP, std::vector<LoadedSlot>> bucketed;

    for( int slot = 0; slot < GERBER_DRAWLAYERS_COUNT; ++slot )
    {
        if( GetImagesList()->GetGbrImage( slot ) == nullptr )
            continue;

        GERBVIEW_LAYER_ROLE role = LookupRoleForSlot( m_frame, slot );
        GERBER_LAYER_GROUP  group = EffectiveGroupForSlot( m_frame, slot );

        bucketed[group].push_back( { slot, role, group, EffectiveRoleOrderInGroup( group, role ) } );
    }

    // Stable-sort each bucket by within-group role order; ties broken by
    // original slot index so file order is preserved among same-role files.
    for( auto& [group, list] : bucketed )
    {
        std::stable_sort( list.begin(), list.end(),
                          []( const LoadedSlot& a, const LoadedSlot& b )
                          {
                              if( a.roleOrder != b.roleOrder )
                                  return a.roleOrder < b.roleOrder;

                              return a.slot < b.slot;
                          } );
    }

    // Section display order is independent of the enum order: Drill at the
    // very top (user request), then Top -> Inner -> Bottom, with Unmapped
    // catching everything without an assigned role at the bottom.  All
    // sections are emitted even when their bucket is empty.
    constexpr GERBER_LAYER_GROUP kDisplayOrder[] = {
        GERBER_LAYER_GROUP::Drill,  GERBER_LAYER_GROUP::Top,      GERBER_LAYER_GROUP::Inner,
        GERBER_LAYER_GROUP::Bottom, GERBER_LAYER_GROUP::Unmapped,
    };

    bool firstGroupHeader = true;

    for( GERBER_LAYER_GROUP group : kDisplayOrder )
    {
        wxString label = GroupDisplayName( group );

        if( label.IsEmpty() )
            continue;

        // Add a small gap above every header except the first one.
        appendGroupHeader( label, group, /* aWithTopGap */ !firstGroupHeader );
        firstGroupHeader = false;

        auto it = bucketed.find( group );

        if( it == bucketed.end() )
            continue; // group exists but has no loaded files

        for( const LoadedSlot& ls : it->second )
        {
            bool     visible = true;
            COLOR4D  color = m_frame->GetLayerColor( GERBER_DRAW_LAYER( ls.slot ) );
            wxString msg = GetImagesList()->GetDisplayName( ls.slot,
                                                            /* include layer number */ false,
                                                            /* Get the full name */ true );

            if( m_frame->GetCanvas() )
            {
                visible = m_frame->GetCanvas()->GetView()->IsLayerVisible( GERBER_DRAW_LAYER( ls.slot ) );
            }
            else
            {
                visible = m_frame->IsLayerVisible( ls.slot );
            }

            AppendLayerRow( LAYER_WIDGET::ROW( msg, ls.slot, color, wxEmptyString, visible, true ) );

            // Replace the freshly-inserted col-3 file-name label with a
            // panel containing [role dropdown, original file-name label],
            // so each row carries a compact "prefix" picker that lets the
            // user re-map the layer in place.
            decorateLayerRowWithRoleDropdown( ls.slot, ls.role );
        }
    }

    syncGroupMasterCheckboxes();

    UpdateLayouts();
    Thaw();
}


void GERBER_LAYER_WIDGET::decorateLayerRowWithRoleDropdown( int aSlot, GERBVIEW_LAYER_ROLE aRole )
{
    // Locate the row that was just appended by AppendLayerRow.  The label
    // we want to swap in sits at column 3 of the last row.
    int row = GetLayerRowCount() - 1;

    if( row < 0 )
        return;

    int           col3Index = row * LYR_COLUMN_COUNT + COLUMN_COLOR_LYRNAME;
    wxSizerItem*  col3Item = m_LayersFlexGridSizer->GetItem( (size_t) col3Index );
    wxStaticText* origLabel = dynamic_cast<wxStaticText*>( col3Item ? col3Item->GetWindow() : nullptr );

    if( !origLabel )
        return;

    // Detach the original label from the FlexGridSizer without destroying it
    // so it can be reparented into the wrapper panel.
    m_LayersFlexGridSizer->Detach( col3Index );

    // Build the wrapper: [dropdown][original label].  The wrapper is sized
    // so the FlexGridSizer happily stretches it across the layer-name
    // column.
    wxPanel*    cell = new wxPanel( m_LayerScrolledWindow, wxID_ANY );
    wxBoxSizer* hbox = new wxBoxSizer( wxHORIZONTAL );

    int visibleInnerCount = 2;

    if( m_frame->gvconfig() )
        visibleInnerCount = m_frame->gvconfig()->m_LayerRoles.visibleInnerCount;

    GERBVIEW_FRAME* frame = m_frame;

    ROLE_DROPDOWN* dropdown = new ROLE_DROPDOWN( cell, aRole, visibleInnerCount,
                                                 [frame, aSlot]( GERBVIEW_LAYER_ROLE aNewRole )
                                                 {
                                                     if( !frame )
                                                         return;

                                                     SetRoleForSlot( frame, aSlot, aNewRole );

                                                     // Reorder slots and rebuild the layer manager so the row
                                                     // jumps into its new group (and the canvas Z-stack follows).
                                                     // Done asynchronously because the dropdown itself is about
                                                     // to be destroyed when the rows are rebuilt.
                                                     frame->CallAfter(
                                                             [frame]()
                                                             {
                                                                 frame->SortLayersByLayerRoles();
                                                             } );
                                                 } );

    hbox->Add( dropdown, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4 );

    origLabel->Reparent( cell );
    hbox->Add( origLabel, 1, wxALIGN_CENTER_VERTICAL );

    cell->SetSizer( hbox );

    const int flags = wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT | wxEXPAND;
    m_LayersFlexGridSizer->wxSizer::Insert( col3Index, cell, 0, flags );
}


void GERBER_LAYER_WIDGET::appendGroupHeader( const wxString& aLabel, GERBER_LAYER_GROUP aGroup, bool aWithTopGap )
{
    // Mirror insertLayerRow's 5-cell row pattern so that the FlexGridSizer
    // keeps its 5-column geometry: an inert INDICATOR_ICON in column 0 (to
    // keep base-class iterators that C-cast column 0 to INDICATOR_ICON*
    // happy -- in particular UpdateLayerIcons()), empty placeholders in
    // columns 1 and 4, and the master checkbox + bold label in columns 2
    // and 3.
    //
    // The header controls are tagged with a POSITIVE decoded layer id
    // (HEADER_ROW_ID_BASE + groupIndex) that lives outside the slot range
    // (0..GERBER_DRAWLAYERS_COUNT-1) and outside both wxWidgets invariants:
    // the negative auto-id range (which would need wxRegisterId) and the
    // 32767 16-bit signed limit enforced by wxControlBase::CreateBase().
    // Base-class iterators recognise these rows as "not a real layer" via
    // IsHeaderRowDecodedId() and skip them.
    int row = m_LayersFlexGridSizer->GetItemCount() / LYR_COLUMN_COUNT;
    int index = row * LYR_COLUMN_COUNT;
    int headerMarker = HEADER_ROW_ID_BASE + int( aGroup );

    // A top border on every cell of this row creates a visual gap between
    // this group and the previous one.  The first header in the list gets
    // no extra gap.
    const int border = aWithTopGap ? 8 : 0;
    const int flags = wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT | ( aWithTopGap ? wxTOP : 0 );

    auto emptyCell = [this, headerMarker]( int aCol )
    {
        return new wxStaticText( m_LayerScrolledWindow, encodeId( aCol, headerMarker ), wxEmptyString );
    };

    // Col 0 - inert INDICATOR_ICON placeholder.  Using INDICATOR_ICON here
    // (instead of wxStaticText) makes the (INDICATOR_ICON*) C-cast performed
    // by LAYER_WIDGET::UpdateLayerIcons() safe; SetIndicatorState() called on
    // it is effectively a no-op for OFF -> OFF transitions on header rows.
    auto* indicator = new INDICATOR_ICON( m_LayerScrolledWindow, *m_IconProvider, ROW_ICON_PROVIDER::STATE::OFF,
                                          encodeId( 0, headerMarker ) );
    m_LayersFlexGridSizer->wxSizer::Insert( index + 0, indicator, 0, flags, border );

    // Col 1 - empty placeholder.
    m_LayersFlexGridSizer->wxSizer::Insert( index + 1, emptyCell( 1 ), 0, flags, border );

    // Col 2 - master checkbox.
    wxCheckBox* master = new wxCheckBox( m_LayerScrolledWindow, encodeId( 2, headerMarker ), wxEmptyString,
                                         wxDefaultPosition, wxDefaultSize, wxCHK_3STATE );
    master->Set3StateValue( wxCHK_CHECKED );
    master->SetToolTip( _( "Toggle visibility of every layer in this group" ) );
    master->Bind( wxEVT_COMMAND_CHECKBOX_CLICKED,
                  [this, aGroup, master]( wxCommandEvent& )
                  {
                      // wxCheckBox in 3-state mode cycles unchecked -> checked
                      // -> indeterminate when clicked; we only honour the two
                      // determinate states for the toggle action.
                      wxCheckBoxState s = master->Get3StateValue();
                      bool            wantVisible = ( s == wxCHK_CHECKED );

                      // Snap indeterminate -> checked so a single click always
                      // moves to a deterministic state.
                      if( s == wxCHK_UNDETERMINED )
                      {
                          wantVisible = true;
                          master->Set3StateValue( wxCHK_CHECKED );
                      }

                      toggleGroupVisibility( aGroup, wantVisible );
                  } );
    m_LayersFlexGridSizer->wxSizer::Insert( index + 2, master, 0, flags, border );

    // Col 3 - bold, slightly larger group label so the header stands out
    // visually from the per-layer rows.  Prefix with the group's compact
    // three-letter code so headers align visually with the per-layer
    // abbreviations below them.
    wxString      fullLabel = GroupShortCode( aGroup ) + wxT( "  " ) + aLabel;
    wxStaticText* label = new wxStaticText( m_LayerScrolledWindow, encodeId( 3, headerMarker ), fullLabel );
    wxFont        font = label->GetFont();
    font.SetWeight( wxFONTWEIGHT_BOLD );
    font.SetPointSize( font.GetPointSize() + 1 );
    label->SetFont( font );
    m_LayersFlexGridSizer->wxSizer::Insert( index + 3, label, 0, flags | wxEXPAND, border );

    // Col 4 - empty placeholder.
    m_LayersFlexGridSizer->wxSizer::Insert( index + 4, emptyCell( 4 ), 0, flags, border );

    m_groupCheckboxes[aGroup] = master;
}


void GERBER_LAYER_WIDGET::toggleGroupVisibility( GERBER_LAYER_GROUP aGroup, bool aVisible )
{
    LSET visibleLayers = m_frame->GetVisibleLayers();

    for( int slot = 0; slot < GERBER_DRAWLAYERS_COUNT; ++slot )
    {
        if( GetImagesList()->GetGbrImage( slot ) == nullptr )
            continue;

        GERBVIEW_LAYER_ROLE role = LookupRoleForSlot( m_frame, slot );

        if( GroupForRole( role ) != aGroup )
            continue;

        visibleLayers[slot] = aVisible;

        // Keep the per-layer checkbox UI in sync without re-triggering events.
        if( findLayerRow( slot ) >= 0 )
            setLayerCheckbox( slot, aVisible );
    }

    m_frame->SetVisibleLayers( visibleLayers );

    // Now that the per-layer checkboxes are coherent, re-evaluate every
    // group master to keep them in sync (e.g. the Unassigned group).
    syncGroupMasterCheckboxes();

    if( m_frame->GetCanvas() )
        m_frame->GetCanvas()->Refresh();
}


void GERBER_LAYER_WIDGET::syncGroupMasterCheckboxes()
{
    if( m_groupCheckboxes.empty() )
        return;

    // For every group with a master checkbox, count visible / hidden loaded
    // slots and set tri-state value accordingly:
    //   all visible -> checked
    //   none visible -> unchecked
    //   mixed       -> indeterminate
    std::map<GERBER_LAYER_GROUP, int> visibleCount;
    std::map<GERBER_LAYER_GROUP, int> totalCount;

    for( int slot = 0; slot < GERBER_DRAWLAYERS_COUNT; ++slot )
    {
        if( GetImagesList()->GetGbrImage( slot ) == nullptr )
            continue;

        GERBVIEW_LAYER_ROLE role = LookupRoleForSlot( m_frame, slot );
        GERBER_LAYER_GROUP  group = GroupForRole( role );

        totalCount[group]++;

        bool visible = true;

        if( m_frame->GetCanvas() )
        {
            visible = m_frame->GetCanvas()->GetView()->IsLayerVisible( GERBER_DRAW_LAYER( slot ) );
        }
        else
        {
            visible = m_frame->IsLayerVisible( slot );
        }

        if( visible )
            visibleCount[group]++;
    }

    for( auto& [group, cb] : m_groupCheckboxes )
    {
        int total = totalCount[group];
        int vis = visibleCount[group];

        // Disable master checkbox for empty sections so users don't get
        // confused when clicking it has no effect.
        cb->Enable( total > 0 );

        if( total == 0 )
            cb->Set3StateValue( wxCHK_UNCHECKED );
        else if( vis == total )
            cb->Set3StateValue( wxCHK_CHECKED );
        else if( vis == 0 )
            cb->Set3StateValue( wxCHK_UNCHECKED );
        else
            cb->Set3StateValue( wxCHK_UNDETERMINED );
    }
}


void GERBER_LAYER_WIDGET::OnLayerRightClick( wxMenu& aMenu )
{
    AddRightClickMenuItems( &aMenu );
}


void GERBER_LAYER_WIDGET::OnLayerColorChange( int aLayer, const COLOR4D& aColor )
{
    // NOTE: Active layer in GerbView is stored as 0-indexed, but layer color is
    //       stored according to the GERBER_DRAW_LAYER() offset.
    m_frame->SetLayerColor( GERBER_DRAW_LAYER( aLayer ), aColor );

    KIGFX::VIEW* view = m_frame->GetCanvas()->GetView();
    COLOR_SETTINGS* color_settings = m_frame->GetColorSettings();
    color_settings->SetColor( aLayer, aColor );

    view->GetPainter()->GetSettings()->LoadColors( color_settings );
    view->UpdateLayerColor( GERBER_DRAW_LAYER( aLayer ) );

    m_frame->GetCanvas()->Refresh();
}


bool GERBER_LAYER_WIDGET::OnLayerSelect( int aLayer )
{
    // Group-header rows are tagged with a positive decoded id at or above
    // HEADER_ROW_ID_BASE and must never be treated as a real layer selection.
    // The negative branch is kept as a defensive guard.
    if( aLayer < 0 || IsHeaderRowDecodedId( aLayer ) )
        return false;

    // the layer change from the GERBER_LAYER_WIDGET can be denied by returning
    // false from this function.
    int layer = m_frame->GetActiveLayer();

    m_frame->SetActiveLayer( aLayer, false );
    m_frame->syncLayerBox();

    if( layer != m_frame->GetActiveLayer() )
    {
        if( ! OnLayerSelected() )
        {
            auto settings = static_cast<KIGFX::GERBVIEW_PAINTER*>
                                ( m_frame->GetCanvas()->GetView()->GetPainter() )->GetSettings();
            int dcodeSelected = m_frame->m_DCodeSelector->GetSelectedDCodeId();
            settings->m_dcodeHighlightValue = dcodeSelected;
            m_frame->GetCanvas()->GetView()->UpdateAllItems( KIGFX::COLOR );
            m_frame->GetCanvas()->Refresh();
        }
    }

    return true;
}


void GERBER_LAYER_WIDGET::OnLayerVisible( int aLayer, bool isVisible, bool isFinal )
{
    LSET visibleLayers = m_frame->GetVisibleLayers();

    visibleLayers[ aLayer ] = isVisible;

    m_frame->SetVisibleLayers( visibleLayers );

    // Keep group master checkboxes in tri-state sync with the per-layer state.
    syncGroupMasterCheckboxes();

    if( isFinal )
        m_frame->GetCanvas()->Refresh();
}


void GERBER_LAYER_WIDGET::OnRenderColorChange( int aId, const COLOR4D& aColor )
{
    m_frame->SetVisibleElementColor( aId, aColor );

    auto view = m_frame->GetCanvas()->GetView();

    COLOR_SETTINGS* color_settings = m_frame->GetColorSettings();
    color_settings->SetColor( aId, aColor );

    view->GetPainter()->GetSettings()->LoadColors( color_settings );
    view->UpdateLayerColor( aId );
    view->MarkTargetDirty( KIGFX::TARGET_NONCACHED );
    view->UpdateAllItems( KIGFX::COLOR );
    m_frame->GetCanvas()->Refresh();
}


void GERBER_LAYER_WIDGET::OnRenderEnable( int aId, bool isEnabled )
{
    m_frame->SetElementVisibility( aId, isEnabled );

    if( m_frame->GetCanvas() )
    {
        if( aId == LAYER_GERBVIEW_GRID )
        {
            m_frame->GetCanvas()->GetGAL()->SetGridVisibility( m_frame->IsGridVisible() );
            m_frame->GetCanvas()->GetView()->MarkTargetDirty( KIGFX::TARGET_NONCACHED );
        }
        else
        {
            m_frame->GetCanvas()->GetView()->SetLayerVisible( aId, isEnabled );
        }
    }

    m_frame->GetCanvas()->Refresh();
}
