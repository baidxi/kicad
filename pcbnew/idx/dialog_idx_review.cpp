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
    NB_COLS
};


DIALOG_IDX_REVIEW::DIALOG_IDX_REVIEW( PCB_BASE_FRAME* aParent, KICAD_IDX* aIDX )
    : DIALOG__IDX_REVIEW_BASE( aParent ),
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
    prepareCanvas();
    m_changeGrid->AutoSizeColumn( IDX_REVIEW_COLS::ACCEPT );

    // Center and fit the dialog
    Layout();
    Centre();
}


DIALOG_IDX_REVIEW::~DIALOG_IDX_REVIEW()
{
}

void DIALOG_IDX_REVIEW::AddChange( KICAD_IDX_CHANGE& aChange)
{
    m_changeGrid->AppendRows(1);

    wxString typeStr = "";

    if ( aChange.m_type == KICAD_IDX_CHANGE::ELEMENT_TYPE::BOARD_OUTLINE )
        typeStr = _( "Outline" );
    else if ( aChange.m_type == KICAD_IDX_CHANGE::ELEMENT_TYPE::COMPONENT )
        typeStr = _( "Component" );
    else if ( aChange.m_type == KICAD_IDX_CHANGE::ELEMENT_TYPE::NPTH )
        typeStr = _( "NPTH" );
    else if ( aChange.m_type == KICAD_IDX_CHANGE::ELEMENT_TYPE::PTH )
        typeStr = _( "PTH" );
    else if ( aChange.m_type == KICAD_IDX_CHANGE::ELEMENT_TYPE::KEEP )
        typeStr = _( "Keepout/Keepin" );
    else if ( aChange.m_type == KICAD_IDX_CHANGE::ELEMENT_TYPE::OTHER )
        typeStr = _( "Other" );

    int row =  m_changeGrid->GetNumberRows()-1;

    m_changeGrid->SetCellValue( row, IDX_REVIEW_COLS::NAME, aChange.m_name );
    m_changeGrid->SetCellValue( row, IDX_REVIEW_COLS::TYPE, typeStr );
    m_changeGrid->SetCellValue( row, IDX_REVIEW_COLS::ACCEPT, "1" );

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

    for ( KICAD_IDX_CHANGE& change: m_idx->GetChanges() )
    {
        AddChange( change );
        for ( BOARD_ITEM* item: change.m_items )
        {
            if ( bb )
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

void DIALOG_IDX_REVIEW::OnGridCellClick(wxGridEvent& event)
{
    // Get the row and column of the clicked cell
    int row = event.GetRow();
    int col = event.GetCol();

    // Only toggle the checkbox when the "ACCEPT" column is clicked
    if (col == IDX_REVIEW_COLS::ACCEPT) {
        // Get the current value of the checkbox cell
        wxString currentValue = m_changeGrid->GetCellValue(row, col);
        
        // Toggle the checkbox value
        wxString newValue = (currentValue == "1") ? "0" : "1";  // Toggle between "1" (checked) and "0" (unchecked)
        
        // Set the new value for the checkbox
        m_changeGrid->SetCellValue(row, col, newValue);
    }

    // If you still want to process the item highlighting logic
    KIGFX::VIEW* view = m_panelPreview->GetView();
    int j = 0;
    BOX2I bb;
    bool atLeastOne = false;
    m_changeGrid->SelectRow(row);

    for (auto& objectGroup : m_objects)
    {
        for (BOARD_ITEM* item : objectGroup)
        {
            if (j == row) {
                item->SetBrightened();
                if (!atLeastOne) {
                    bb = item->ViewBBox();
                    atLeastOne = true;
                } else {
                    bb.Merge(item->ViewBBox());
                }
            } else {
                item->ClearBrightened();
            }
            view->Update(item);
        }
        j++;
    }

    // Adjust the viewport to highlight the relevant area
    if (atLeastOne) {
        view->SetViewport(BOX2D(bb.GetOrigin(), bb.GetSize()));
        view->SetScale(view->GetScale() * 0.1);
    }
    m_panelPreview->Refresh();  // Refresh the preview to show the changes

    event.Skip();  // Allow default event handling to propagate
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
    for ( int i = 0; i < m_changeGrid->GetNumberRows(); i++ )
    {
        m_changeGrid->SetCellValue( i, IDX_REVIEW_COLS::ACCEPT, "1" );
    }
}

void DIALOG_IDX_REVIEW::OnRejectAll( wxCommandEvent& event )
{
    for ( int i = 0; i < m_changeGrid->GetNumberRows(); i++ )
    {
        m_changeGrid->SetCellValue( i, IDX_REVIEW_COLS::ACCEPT, "0" );
    }
}