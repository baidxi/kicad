/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2024 KiCad Developers, see AUTHORS.txt for contributors.
 * @author Fabien Corona <f.corona<at>laposte.net>
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

#include "dialog_idx_review_base.h"
#include <board.h>
#include <pcb_view.h>
#include <pcb_base_frame.h>
#include <view/view.h>
#include <gal/graphics_abstraction_layer.h>
#include <tool/tool_manager.h>
#include <board_commit.h>
#include <pcbnew_settings.h>
#include <settings/color_settings.h>
#include <math/box2.h>
#include <idx/kicad_idx.h>

#include "dialog_idx_review.h"


enum IDX_REVIEW_COLS
{
    ACCEPT = 0,
    NAME,
    TYPE,
    COMMENT,
    STATUS,
    REJECT_COMMENT,
    NB_COLS
};


DIALOG_IDX_REVIEW::DIALOG_IDX_REVIEW( PCB_BASE_FRAME* aParent, KICAD_IDX* aIDX ) :
        DIALOG__IDX_REVIEW_BASE( aParent ),
        m_parent( aParent ),
        m_board( aParent->GetBoard() ),
        m_dummyBoard( nullptr )
{
    m_idx = aIDX;
    // Create a dummy board for preview (copy or minimal subset)
    m_dummyBoard = m_board;

    m_changeGrid->AppendCols( IDX_REVIEW_COLS::NB_COLS );
    m_changeGrid->SetColLabelValue( IDX_REVIEW_COLS::ACCEPT, _( "" ) );
    m_changeGrid->SetColFormatBool( IDX_REVIEW_COLS::ACCEPT );
    m_changeGrid->SetColLabelValue( IDX_REVIEW_COLS::NAME, _( "Name" ) );
    m_changeGrid->SetColLabelValue( IDX_REVIEW_COLS::TYPE, _( "Type" ) );
    m_changeGrid->SetColLabelValue( IDX_REVIEW_COLS::STATUS, _( "Status" ) );
    m_changeGrid->SetColLabelValue( IDX_REVIEW_COLS::COMMENT, _( "Comment" ) );
    m_changeGrid->SetColLabelValue( IDX_REVIEW_COLS::REJECT_COMMENT, _( "Reject Comment" ) );
    prepareCanvas();
    m_changeGrid->AutoSizeColumns();

    // Center and fit the dialog
    Layout();
    Centre();
}


DIALOG_IDX_REVIEW::~DIALOG_IDX_REVIEW()
{
}

void DIALOG_IDX_REVIEW::AddChange( KICAD_IDX_CHANGE& aChange )
{
    m_changeGrid->AppendRows( 1 );

    wxString typeStr = "";

    if( aChange.m_type == KICAD_IDX_CHANGE::ELEMENT_TYPE::BOARD_OUTLINE )
        typeStr = _( "Outline" );
    else if( aChange.m_type == KICAD_IDX_CHANGE::ELEMENT_TYPE::COMPONENT )
        typeStr = _( "Component" );
    else if( aChange.m_type == KICAD_IDX_CHANGE::ELEMENT_TYPE::NPTH )
        typeStr = _( "NPTH" );
    else if( aChange.m_type == KICAD_IDX_CHANGE::ELEMENT_TYPE::PTH )
        typeStr = _( "PTH" );
    else if( aChange.m_type == KICAD_IDX_CHANGE::ELEMENT_TYPE::KEEP )
        typeStr = _( "Keepout/Keepin" );
    else if( aChange.m_type == KICAD_IDX_CHANGE::ELEMENT_TYPE::OTHER )
        typeStr = _( "Other" );

    int row = m_changeGrid->GetNumberRows() - 1;

    m_changeGrid->SetCellValue( row, IDX_REVIEW_COLS::NAME, aChange.m_name );
    m_changeGrid->SetCellValue( row, IDX_REVIEW_COLS::TYPE, typeStr );
    m_changeGrid->SetCellValue( row, IDX_REVIEW_COLS::ACCEPT, "1" );
    m_changeGrid->SetCellValue( row, IDX_REVIEW_COLS::STATUS, aChange.baseline ? "Baseline" : "Change" );
    m_changeGrid->SetCellValue( row, IDX_REVIEW_COLS::COMMENT, "" );

    m_changeGrid->SetReadOnly( row, IDX_REVIEW_COLS::NAME );
    m_changeGrid->SetReadOnly( row, IDX_REVIEW_COLS::TYPE );
    m_changeGrid->SetReadOnly( row, IDX_REVIEW_COLS::ACCEPT );
    m_changeGrid->SetReadOnly( row, IDX_REVIEW_COLS::STATUS );
    m_changeGrid->SetReadOnly( row, IDX_REVIEW_COLS::COMMENT );

    m_objects.push_back( aChange.m_items );
}

void DIALOG_IDX_REVIEW::prepareCanvas()
{
    // Initialize the GAL preview panel
    m_panelPreview->UpdateColors();
    m_panelPreview->SwitchBackend( m_parent->GetCanvas()->GetBackend() );
    m_panelPreview->SetStealsFocus( false );

    m_panelPreview->Show();

    KIGFX::VIEW* view = m_panelPreview->GetView();

    // Provide a small grid to prevent 0-size errors in GAL
    double gridsize = pcbIUScale.mmToIU( 0.001 );
    view->GetGAL()->SetGridSize( VECTOR2D( gridsize, gridsize ) );
    view->GetGAL()->SetGridVisibility( false );


    // Connect resize event
    Bind( wxEVT_SIZE, &DIALOG_IDX_REVIEW::OnResize, this );


    m_objects = {};

    BOX2I* bb = nullptr;

    for( KICAD_IDX_CHANGE& change : m_idx->Process() )
    {
        m_changes.emplace_back( true, std::make_shared<KICAD_IDX_CHANGE>( change ) );
        auto last_change = m_changes.back().second;
        m_changes.back().first = true; // accept by default
        AddChange( *last_change );

        for( BOARD_ITEM* item : last_change->m_items )
        {
            if( bb )
                bb->Merge( item->ViewBBox() );
            else
                bb = new BOX2I( item->ViewBBox() );
            view->Add( item );
        }
    }

    view->SetBoundary( BOX2D( bb->GetOrigin(), bb->GetSize() * 1.2 ) );
    view->SetViewport( BOX2D( bb->GetOrigin(), bb->GetSize() ) );
    view->SetScale( view->GetScale() * 0.7 );

    // Start rendering
    m_panelPreview->StartDrawing();
}

void DIALOG_IDX_REVIEW::OnGridCellClick( wxGridEvent& event )
{
    // Get the row and column of the clicked cell
    int row = event.GetRow();
    int col = event.GetCol();

    // Only toggle the checkbox when the "ACCEPT" column is clicked
    if( col == IDX_REVIEW_COLS::ACCEPT )
    {
        // Get the current value of the checkbox cell
        wxString currentValue = m_changeGrid->GetCellValue( row, col );

        // Toggle the checkbox value
        wxString newValue = ( currentValue == "1" ) ? "0" : "1"; // Toggle between "1" (checked) and "0" (unchecked)

        // Set the new value for the checkbox
        m_changeGrid->SetCellValue( row, col, newValue );
        m_changes[row].first = ( newValue == "1" );
    }

    KIGFX::VIEW* view = m_panelPreview->GetView();
    int          j = 0;
    BOX2I        bb;
    bool         atLeastOne = false;
    m_changeGrid->SelectRow( row );

    for( auto& objectGroup : m_objects )
    {
        for( BOARD_ITEM* item : objectGroup )
        {
            if( j == row )
            {
                item->SetBrightened();
                if( !atLeastOne )
                {
                    bb = item->ViewBBox();
                    atLeastOne = true;
                }
                else
                {
                    bb.Merge( item->ViewBBox() );
                }
            }
            else
            {
                item->ClearBrightened();
            }
            view->Update( item );
        }
        j++;
    }

    // Adjust the viewport to highlight the relevant area
    if( atLeastOne )
    {
        view->SetViewport( BOX2D( bb.GetOrigin(), bb.GetSize() ) );
        view->SetScale( view->GetScale() * 0.1 );
    }
    m_panelPreview->Refresh(); // Refresh the preview to show the changes

    event.Skip(); // Allow default event handling to propagate
}


void DIALOG_IDX_REVIEW::OnResize( wxSizeEvent& event )
{
    if( m_panelPreview )
        m_panelPreview->Refresh();
    event.Skip();
}


void DIALOG_IDX_REVIEW::refreshPreview()
{
    if( !m_dummyBoard || !m_panelPreview )
        return;
    /*
    KIGFX::VIEW* view = m_panelPreview->GetView();

    // Compute bounding box for current board (all items)
    BOX2I bbox = m_dummyBoard->ComputeBoundingBox();

    if( !bbox.GetSize().x || !bbox.GetSize().y )
        bbox = BOX2I( VECTOR2I(0,0), VECTOR2I(1000000,1000000) ); // fallback

    // Fit view to bounding box
    view->SetViewport( BOX2D( bbox.GetOrigin(), bbox.GetSize() ) );
    view->SetScale( view->GetScale() * 0.9 ); // small margin
    view->SetCenter( VECTOR2D( bbox.Centre() ) );

    view->Update( m_dummyBoard );*/
}


bool DIALOG_IDX_REVIEW::TransferDataToWindow()
{
    // Here you would populate text fields or data
    // For now, just refresh the board preview
    refreshPreview();
    return true;
}


void DIALOG_IDX_REVIEW::OnCancel( wxCommandEvent& event )
{
    if( m_panelPreview )
        m_panelPreview->StopDrawing();

    event.Skip(); // default wxWidgets handling
}


void DIALOG_IDX_REVIEW::OnAcceptAll( wxCommandEvent& event )
{
    for( int i = 0; i < m_changeGrid->GetNumberRows(); i++ )
    {
        m_changes[i].first = true;
        m_changeGrid->SetCellValue( i, IDX_REVIEW_COLS::ACCEPT, "1" );
    }
}

void DIALOG_IDX_REVIEW::OnRejectAll( wxCommandEvent& event )
{
    for( int i = 0; i < m_changeGrid->GetNumberRows(); i++ )
    {
        m_changes[i].first = false;
        m_changeGrid->SetCellValue( i, IDX_REVIEW_COLS::ACCEPT, "0" );
    }
}

std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> DIALOG_IDX_REVIEW::GetAccepted()
{
    std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> result;

    for( auto& change : m_changes )
    {
        if( change.first )
            result.push_back( change.second );
    }
    return result;
}


std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> DIALOG_IDX_REVIEW::GetRejected()
{
    std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> result;

    for( auto& change : m_changes )
    {
        if( !change.first )
            result.push_back( change.second );
    }
    return result;
}