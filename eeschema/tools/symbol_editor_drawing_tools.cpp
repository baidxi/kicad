/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2019 CERN
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

#include <sch_actions.h>
#include <optional>
#include <symbol_edit_frame.h>
#include <widgets/wx_infobar.h>
#include <sch_commit.h>
#include <gal/graphics_abstraction_layer.h>
#include <tools/symbol_editor_drawing_tools.h>
#include <tools/symbol_editor_pin_tool.h>
#include <tools/ee_grid_helper.h>
#include <dialogs/dialog_text_properties.h>
#include <sch_bitmap.h>
#include <sch_shape.h>
#include <sch_textbox.h>
#include <pgm_base.h>
#include <view/view_controls.h>
#include <symbol_editor/symbol_editor_settings.h>
#include <settings/settings_manager.h>
#include <string_utils.h>
#include <wx/msgdlg.h>
#include <wildcards_and_files_ext.h>


KIID SYMBOL_EDITOR_DRAWING_TOOLS::g_lastPin;


SYMBOL_EDITOR_DRAWING_TOOLS::SYMBOL_EDITOR_DRAWING_TOOLS() :
        SCH_TOOL_BASE<SYMBOL_EDIT_FRAME>( "eeschema.SymbolDrawing" ),
        m_lastTextBold( false ),
        m_lastTextItalic( false ),
        m_lastTextAngle( ANGLE_HORIZONTAL ),
        m_lastTextJust( GR_TEXT_H_ALIGN_LEFT ),
        m_inDrawShape( false ),
        m_inPlaceAnchor( false ),
        m_inTwoClickPlace( false ),
        m_inPlaceImage( false )
{
}


bool SYMBOL_EDITOR_DRAWING_TOOLS::Init()
{
    SCH_TOOL_BASE::Init();

    auto isDrawingCondition =
            [] ( const SELECTION& aSel )
            {
                SCH_ITEM* item = dynamic_cast<SCH_ITEM*>( aSel.Front() );
                return item && item->IsNew();
            };

    m_menu->GetMenu().AddItem( ACTIONS::finishInteractive, isDrawingCondition, 2 );

    return true;
}


int SYMBOL_EDITOR_DRAWING_TOOLS::TwoClickPlace( const TOOL_EVENT& aEvent )
{
    KICAD_T                 type = aEvent.Parameter<KICAD_T>();
    SYMBOL_EDITOR_SETTINGS* cfg = GetAppSettings<SYMBOL_EDITOR_SETTINGS>( "symbol_editor" );
    SYMBOL_EDITOR_PIN_TOOL* pinTool = type == SCH_PIN_T ? m_toolMgr->GetTool<SYMBOL_EDITOR_PIN_TOOL>()
                                                        : nullptr;

    if( m_inTwoClickPlace )
        return 0;

    REENTRANCY_GUARD guard( &m_inTwoClickPlace );

    KIGFX::VIEW_CONTROLS* controls = getViewControls();
    EE_GRID_HELPER        grid( m_toolMgr );
    VECTOR2I              cursorPos;
    bool                  ignorePrimePosition = false;
    SCH_ITEM*             item   = nullptr;
    bool                  isText = aEvent.IsAction( &SCH_ACTIONS::placeSymbolText );
    COMMON_SETTINGS*      common_settings = Pgm().GetCommonSettings();

    m_toolMgr->RunAction( ACTIONS::selectionClear );

    m_frame->PushTool( aEvent );

    auto setCursor =
            [&]()
            {
                if( item )
                    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::PLACE );
                else if( isText )
                    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::TEXT );
                else
                    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::PENCIL );
            };

    auto cleanup =
            [&] ()
            {
                m_toolMgr->RunAction( ACTIONS::selectionClear );
                m_view->ClearPreview();
                delete item;
                item = nullptr;
            };

    Activate();
    // Must be done after Activate() so that it gets set into the correct context
    controls->ShowCursor( true );
    // Set initial cursor
    setCursor();

    if( aEvent.HasPosition() )
    {
        m_toolMgr->PrimeTool( aEvent.Position() );
    }
    else if( common_settings->m_Input.immediate_actions && !aEvent.IsReactivate() )
    {
        m_toolMgr->PrimeTool( { 0, 0 } );
        ignorePrimePosition = true;
    }

    SCH_COMMIT commit( m_toolMgr );

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        setCursor();
        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( getView()->GetGAL()->GetGridSnapping() && !evt->DisableGridSnapping() );

        cursorPos = grid.Align( controls->GetMousePosition(), grid.GetItemGrid( item ) );
        controls->ForceCursorPosition( true, cursorPos );

        // The tool hotkey is interpreted as a click when drawing
        bool isSyntheticClick = item && evt->IsActivate() && evt->HasPosition()
                                && evt->Matches( aEvent );

        if( evt->IsCancelInteractive() )
        {
            m_frame->GetInfoBar()->Dismiss();

            if( item )
            {
                cleanup();
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsActivate() && !isSyntheticClick )
        {
            if( item && evt->IsMoveTool() )
            {
                // we're already moving our own item; ignore the move tool
                evt->SetPassEvent( false );
                continue;
            }

            if( item )
            {
                m_frame->ShowInfoBarMsg( _( "Press <ESC> to cancel item creation." ) );
                evt->SetPassEvent( false );
                continue;
            }

            if( evt->IsPointEditor() )
            {
                // don't exit (the point editor runs in the background)
            }
            else if( evt->IsMoveTool() )
            {
                break;
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsClick( BUT_LEFT ) || evt->IsDblClick( BUT_LEFT ) || isSyntheticClick )
        {
            LIB_SYMBOL* symbol = m_frame->GetCurSymbol();

            if( !symbol )
                continue;

            // First click creates...
            if( !item )
            {
                m_toolMgr->RunAction( ACTIONS::selectionClear );

                switch( type )
                {
                case SCH_PIN_T:
                    item = pinTool->CreatePin( cursorPos, symbol );

                    if( item )
                        g_lastPin = item->m_Uuid;

                    break;

                case SCH_TEXT_T:
                {
                    SCH_TEXT* text = new SCH_TEXT( cursorPos, wxEmptyString, LAYER_DEVICE );

                    text->SetParent( symbol );

                    if( m_frame->GetDrawSpecificUnit() )
                        text->SetUnit( m_frame->GetUnit() );

                    if( m_frame->GetDrawSpecificBodyStyle() )
                        text->SetBodyStyle( m_frame->GetBodyStyle() );

                    if( cfg )
                    {
                        text->SetTextSize( VECTOR2I( schIUScale.MilsToIU( cfg->m_Defaults.text_size ),
                                                     schIUScale.MilsToIU( cfg->m_Defaults.text_size ) ) );
                    }

                    text->SetTextAngle( m_lastTextAngle );

                    DIALOG_TEXT_PROPERTIES dlg( m_frame, text );

                    if( dlg.ShowModal() != wxID_OK || NoPrintableChars( text->GetText() ) )
                        delete text;
                    else
                        item = text;

                    break;
                }

                default:
                    wxFAIL_MSG( "TwoClickPlace(): unknown type" );
                }

                // If we started with a hotkey which has a position then warp back to that.
                // Otherwise update to the current mouse position pinned inside the autoscroll
                // boundaries.
                if( evt->IsPrime() && !ignorePrimePosition )
                {
                    cursorPos = grid.Align( evt->Position(), grid.GetItemGrid( item ) );
                    getViewControls()->WarpMouseCursor( cursorPos, true );
                }
                else
                {
                    getViewControls()->PinCursorInsideNonAutoscrollArea( true );
                    cursorPos = grid.Align( getViewControls()->GetMousePosition(), grid.GetItemGrid( item ) );
                }

                if( item )
                {
                    item->SetPosition( VECTOR2I( cursorPos.x, -cursorPos.y ) );

                    item->SetFlags( IS_NEW | IS_MOVING );
                    m_view->ClearPreview();
                    m_view->AddToPreview( item, false );
                    m_selectionTool->AddItemToSel( item );

                    // update the cursor so it looks correct before another event
                    setCursor();
                }

                if( m_frame->GetMoveWarpsCursor() )
                    controls->SetCursorPosition( cursorPos, false );

                m_toolMgr->PostAction( ACTIONS::refreshPreview );
            }
            // ... and second click places:
            else
            {
                commit.Modify( symbol, m_frame->GetScreen() );

                switch( item->Type() )
                {
                case SCH_PIN_T:
                    pinTool->PlacePin( &commit, static_cast<SCH_PIN*>( item ) );
                    item->ClearEditFlags();
                    commit.Push( _( "Place Pin" ) );
                    break;

                case SCH_TEXT_T:
                    symbol->AddDrawItem( static_cast<SCH_TEXT*>( item ) );
                    item->ClearEditFlags();
                    commit.Push( _( "Draw Text" ) );
                    break;

                default:
                    wxFAIL_MSG( "TwoClickPlace(): unknown type" );
                }

                item = nullptr;
                m_view->ClearPreview();
                m_frame->RebuildView();
            }
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            // Warp after context menu only if dragging...
            if( !item )
                m_toolMgr->VetoContextMenuMouseWarp();

            m_menu->ShowContextMenu( m_selectionTool->GetSelection() );
        }
        else if( evt->IsAction( &ACTIONS::increment ) )
        {
            if( evt->HasParameter() )
                m_toolMgr->RunSynchronousAction( ACTIONS::increment, &commit, evt->Parameter<ACTIONS::INCREMENT>() );
            else
                m_toolMgr->RunSynchronousAction( ACTIONS::increment, &commit, ACTIONS::INCREMENT { 1, 0 } );
        }
        else if( item && ( evt->IsAction( &ACTIONS::refreshPreview ) || evt->IsMotion() ) )
        {
            item->SetPosition( VECTOR2I( cursorPos.x, cursorPos.y ) );
            m_view->ClearPreview();
            m_view->AddToPreview( item, false );
        }
        else
        {
            evt->SetPassEvent();
        }

        // Enable autopanning and cursor capture only when there is an item to be placed
        controls->SetAutoPan( item != nullptr );
        controls->CaptureCursor( item != nullptr );
    }

    controls->SetAutoPan( false );
    controls->CaptureCursor( false );
    controls->ForceCursorPosition( false );
    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::ARROW );
    return 0;
}


int SYMBOL_EDITOR_DRAWING_TOOLS::PlaceAnchor( const TOOL_EVENT& aEvent )
{
    if( m_inPlaceAnchor )
        return 0;

    REENTRANCY_GUARD guard( &m_inPlaceAnchor );

    m_frame->PushTool( aEvent );

    auto setCursor =
            [&]()
            {
                m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::BULLSEYE );
            };

    Activate();
    // Must be done after Activate() so that it gets set into the correct context
    getViewControls()->ShowCursor( true );
    // Set initial cursor
    setCursor();

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        setCursor();

        if( evt->IsCancelInteractive() )
        {
            m_frame->PopTool( aEvent );
            break;
        }
        else if( evt->IsActivate() )
        {
            m_frame->PopTool( aEvent );
            break;
        }
        else if( evt->IsClick( BUT_LEFT ) || evt->IsDblClick( BUT_LEFT ) )
        {
            LIB_SYMBOL* symbol = m_frame->GetCurSymbol();

            if( !symbol )
                continue;

            VECTOR2I cursorPos = getViewControls()->GetCursorPosition( !evt->DisableGridSnapping() );

            symbol->Move( -cursorPos );

            // Refresh the view without changing the viewport
            m_view->SetCenter( m_view->GetCenter() + cursorPos );
            m_view->RecacheAllItems();
            m_frame->OnModify();
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            m_menu->ShowContextMenu( m_selectionTool->GetSelection() );
        }
        else
        {
            evt->SetPassEvent();
        }
    }

    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::ARROW );
    return 0;
}


int SYMBOL_EDITOR_DRAWING_TOOLS::RepeatDrawItem( const TOOL_EVENT& aEvent )
{
    SYMBOL_EDITOR_PIN_TOOL* pinTool = m_toolMgr->GetTool<SYMBOL_EDITOR_PIN_TOOL>();
    LIB_SYMBOL*   symbol = m_frame->GetCurSymbol();
    SCH_PIN*      sourcePin = nullptr;

    if( !symbol )
        return 0;

    for( SCH_PIN* test : symbol->GetPins() )
    {
        if( test->m_Uuid == g_lastPin )
        {
            sourcePin = test;
            break;
        }
    }

    if( sourcePin )
    {
        SCH_PIN* pin = pinTool->RepeatPin( sourcePin );

        if( pin )
            g_lastPin = pin->m_Uuid;

        m_toolMgr->RunAction( ACTIONS::selectionClear );

        if( pin )
            m_toolMgr->RunAction<EDA_ITEM*>( ACTIONS::selectItem, pin );
    }

    return 0;
}


int SYMBOL_EDITOR_DRAWING_TOOLS::PlaceImage( const TOOL_EVENT& aEvent )
{
    if( m_inPlaceImage )
        return 0;

    REENTRANCY_GUARD guard( &m_inPlaceImage );

    KIGFX::VIEW_CONTROLS* controls = getViewControls();
    EE_GRID_HELPER        grid( m_toolMgr );
    SCH_BITMAP*           image = nullptr;
    bool                  ignorePrimePosition = false;
    COMMON_SETTINGS*      common_settings = Pgm().GetCommonSettings();

    m_toolMgr->RunAction( ACTIONS::selectionClear );

    m_frame->PushTool( aEvent );

    auto setCursor = [&]()
    {
        if( image )
            m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::MOVING );
        else
            m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::ARROW );
    };

    auto cleanup = [&]()
    {
        m_toolMgr->RunAction( ACTIONS::selectionClear );
        m_view->ClearPreview();
        delete image;
        image = nullptr;
    };

    Activate();
    getViewControls()->ShowCursor( true );
    setCursor();

    if( aEvent.HasPosition() )
    {
        m_toolMgr->PrimeTool( aEvent.Position() );
    }
    else if( common_settings->m_Input.immediate_actions && !aEvent.IsReactivate() )
    {
        m_toolMgr->PrimeTool( { 0, 0 } );
        ignorePrimePosition = true;
    }

    while( TOOL_EVENT* evt = Wait() )
    {
        setCursor();
        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( getView()->GetGAL()->GetGridSnapping() && !evt->DisableGridSnapping() );

        VECTOR2I cursorPos = grid.Align( controls->GetMousePosition(), GRID_HELPER_GRIDS::GRID_GRAPHICS );
        controls->ForceCursorPosition( true, cursorPos );

        // The tool hotkey is interpreted as a click when placing the preview image.
        bool isSyntheticClick = image && evt->IsActivate() && evt->HasPosition() && evt->Matches( aEvent );

        if( evt->IsCancelInteractive() || ( image && evt->IsAction( &ACTIONS::undo ) ) )
        {
            m_frame->GetInfoBar()->Dismiss();

            if( image )
            {
                cleanup();
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsActivate() && !isSyntheticClick )
        {
            if( image && evt->IsMoveTool() )
            {
                evt->SetPassEvent( false );
                continue;
            }

            if( image )
            {
                m_frame->ShowInfoBarMsg( _( "Press <ESC> to cancel image creation." ) );
                evt->SetPassEvent( false );
                continue;
            }

            if( evt->IsMoveTool() )
            {
                break;
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsClick( BUT_LEFT ) || evt->IsDblClick( BUT_LEFT ) || isSyntheticClick
                 || evt->IsAction( &ACTIONS::cursorClick ) || evt->IsAction( &ACTIONS::cursorDblClick ) )
        {
            LIB_SYMBOL* symbol = m_frame->GetCurSymbol();

            if( !symbol )
                continue;

            if( !image )
            {
                m_toolMgr->RunAction( ACTIONS::selectionClear );

                wxFileDialog dlg( m_frame, _( "Choose Image" ), wxEmptyString, wxEmptyString,
                                  FILEEXT::ImageFileWildcard(), wxFD_OPEN );

                bool cancelled = false;

                RunMainStack(
                        [&]()
                        {
                            cancelled = dlg.ShowModal() != wxID_OK;
                        } );

                if( cancelled )
                    continue;

                if( evt->IsPrime() && !ignorePrimePosition )
                {
                    cursorPos = grid.Align( evt->Position() );
                    getViewControls()->WarpMouseCursor( cursorPos, true );
                }
                else
                {
                    getViewControls()->PinCursorInsideNonAutoscrollArea( true );
                    cursorPos = getViewControls()->GetMousePosition();
                }

                wxString fullFilename = dlg.GetPath();

                image = new SCH_BITMAP( cursorPos );

                if( !image->GetReferenceImage().ReadImageFile( fullFilename ) )
                {
                    wxMessageBox( wxString::Format( _( "Could not load image from '%s'." ), fullFilename ) );
                    delete image;
                    image = nullptr;
                    continue;
                }

                image->SetPosition( cursorPos );
                image->SetFlags( IS_NEW | IS_MOVING );

                if( m_frame->GetDrawSpecificUnit() )
                    image->SetUnit( m_frame->GetUnit() );

                if( m_frame->GetDrawSpecificBodyStyle() )
                    image->SetBodyStyle( m_frame->GetBodyStyle() );

                m_view->ClearPreview();
                m_view->AddToPreview( image, false );

                m_selectionTool->AddItemToSel( image );
                controls->SetCursorPosition( cursorPos, false );
                setCursor();
            }
            else
            {
                SCH_COMMIT commit( m_toolMgr );
                commit.Modify( symbol, m_frame->GetScreen() );

                image->SetPosition( cursorPos );
                symbol->AddDrawItem( image );
                image->ClearEditFlags();

                commit.Push( _( "Place Image" ) );

                image = nullptr;
                m_view->ClearPreview();
                m_frame->RebuildView();
                m_toolMgr->PostAction( ACTIONS::activatePointEditor );
            }
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            if( !image )
                m_toolMgr->VetoContextMenuMouseWarp();

            m_menu->ShowContextMenu( m_selectionTool->GetSelection() );
        }
        else if( evt->IsAction( &ACTIONS::duplicate ) || evt->IsAction( &SCH_ACTIONS::repeatDrawItem )
                 || evt->IsAction( &ACTIONS::paste ) )
        {
            if( image )
            {
                wxBell();
                continue;
            }

            m_frame->PopTool( aEvent );
            evt->SetPassEvent();
            break;
        }
        else if( image && ( evt->IsAction( &ACTIONS::refreshPreview ) || evt->IsMotion() ) )
        {
            image->SetPosition( cursorPos );
            m_view->ClearPreview();
            m_view->AddToPreview( image, false );
        }
        else if( image && evt->IsAction( &ACTIONS::doDelete ) )
        {
            cleanup();
        }
        else if( image && evt->IsAction( &ACTIONS::redo ) )
        {
            wxBell();
        }
        else
        {
            evt->SetPassEvent();
        }

        controls->SetAutoPan( image != nullptr );
        controls->CaptureCursor( image != nullptr );
    }

    controls->SetAutoPan( false );
    controls->CaptureCursor( false );
    controls->ForceCursorPosition( false );
    m_view->ClearPreview();
    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::ARROW );

    return 0;
}


void SYMBOL_EDITOR_DRAWING_TOOLS::setTransitions()
{
    // clang-format off
    Go( &SYMBOL_EDITOR_DRAWING_TOOLS::TwoClickPlace,     SCH_ACTIONS::placeSymbolPin.MakeEvent() );
    Go( &SYMBOL_EDITOR_DRAWING_TOOLS::TwoClickPlace,     SCH_ACTIONS::placeSymbolText.MakeEvent() );
    Go( &SYMBOL_EDITOR_DRAWING_TOOLS::PlaceAnchor,       SCH_ACTIONS::placeSymbolAnchor.MakeEvent() );
    Go( &SYMBOL_EDITOR_DRAWING_TOOLS::PlaceImage,        SCH_ACTIONS::placeSymbolImage.MakeEvent() );
    Go( &SYMBOL_EDITOR_DRAWING_TOOLS::RepeatDrawItem,    SCH_ACTIONS::repeatDrawItem.MakeEvent() );
    // clang-format on
}
