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
#include <wx/dirdlg.h>

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


DIALOG_IDX_REVIEW::DIALOG_IDX_REVIEW( PCB_BASE_FRAME* aParent, BOARD_COMMIT* aCommit ) :
        DIALOG__IDX_REVIEW_BASE( aParent ),
        m_parent( aParent ),
        m_board( aParent->GetBoard() ),
        m_dummyBoard( nullptr )
{
    if( !m_changeGrid )
        return;


    // Create a dummy board for preview (copy or minimal subset)
    m_dummyBoard = m_board;
    m_commit = aCommit;

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

    m_reg = std::make_shared<KIDX_REGISTRY_KICAD>( "/home/fabien/Documents/kicad-dev/kicad-git/test.kicad_idx" );

    if( !m_reg->GetPath().empty() )
    {
        m_collaborationFolderInput->SetValue( m_reg->GetPath() );
        m_collaborationFolderInput->SetModified( true );
    }
}


DIALOG_IDX_REVIEW::~DIALOG_IDX_REVIEW()
{
}

void DIALOG_IDX_REVIEW::AddChange( KICAD_IDX_CHANGE& aChange )
{
    if( !m_changeGrid )
        return;

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
}

void DIALOG_IDX_REVIEW::OnGridCellClick( wxGridEvent& event )
{
    // Get the row and column of the clicked cell
    int row = event.GetRow();
    int col = event.GetCol();

    if( !m_changeGrid || row < 0 || row >= (int) m_changes.size() )
    {
        event.Skip();
        return;
    }


    // Only toggle the checkbox when the "ACCEPT" column is clicked
    if( col == IDX_REVIEW_COLS::ACCEPT )
    {
        wxString currentValue = m_changeGrid->GetCellValue( row, col );
        wxString newValue = ( currentValue == "1" ) ? "0" : "1";

        m_changeGrid->SetCellValue( row, col, newValue );
        m_changes[row].first = ( newValue == "1" );
    }

    if( !m_panelPreview )
    {
        event.Skip();
        return;
    }

    KIGFX::VIEW* view = m_panelPreview->GetView();

    if( !view )
    {
        event.Skip();
        return;
    }

    int   j = 0;
    BOX2I bb;
    bool  atLeastOne = false;
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
    if( !m_changeGrid )
        return;

    for( int i = 0; i < m_changeGrid->GetNumberRows(); i++ )
    {
        m_changes[i].first = true;
        m_changeGrid->SetCellValue( i, IDX_REVIEW_COLS::ACCEPT, "1" );
    }
}


void DIALOG_IDX_REVIEW::OnRejectAll( wxCommandEvent& event )
{
    if( !m_changeGrid )
        return;

    for( int i = 0; i < m_changeGrid->GetNumberRows(); i++ )
    {
        m_changes[i].first = false;
        m_changeGrid->SetCellValue( i, IDX_REVIEW_COLS::ACCEPT, "0" );
    }
}

void DIALOG_IDX_REVIEW::onItemActivated( wxDataViewEvent& event )
{
    wxDataViewItem item = event.GetItem();

    if( !item.IsOk() )
        return;
    if( !m_dir )
        return;
    if( !m_history )
        return;


    unsigned int row = m_history->ItemToRow( item );

    if( row >= m_historyReady.size() )
        return;

    wxString filename = m_history->GetTextValue( row, 0 );

    if( !m_historyReady[row] )
    {
        wxLogMessage( wxT( "Can't apply file %s on top of history." ), filename );
        return;
    }

    clearIDX();

    wxString fullpath = m_dir->GetNameWithSep() + filename;


    wxXmlDocument doc;

    if( !doc.Load( fullpath ) )
        return;

    std::shared_ptr<IDX_XML_NODE> child = std::make_shared<IDX_XML_NODE_KICAD>( doc.GetRoot() );

    m_loadedDataset = std::make_shared<idx_foundation::EDMD_DATASET>( *child );
    m_loadedDataset->Resolve();
    m_loadedDataset->Prepare();

    m_idx = std::make_shared<KICAD_IDX>( m_loadedDataset, m_board, m_commit );

    m_currentFile = m_reg->GetFiles()[row];
    loadIDX( m_idx );
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

bool DIALOG_IDX_REVIEW::loadIDX( std::shared_ptr<KICAD_IDX> aIDX )
{
    if( !aIDX || !m_panelPreview )
        return false;

    m_panelPreview->StopDrawing();

    KIGFX::VIEW* view = m_panelPreview->GetView();


    if( !view )
        return false;

    m_idx = aIDX;
    std::unique_ptr<BOX2I> bb;

    for( KICAD_IDX_CHANGE& change : m_idx->Process() )
    {
        m_changes.emplace_back( true, std::make_shared<KICAD_IDX_CHANGE>( change ) );
        auto last_change = m_changes.back().second;

        m_changes.back().first = true; // accept by default
        AddChange( *last_change );

        for( BOARD_ITEM* item : last_change->m_items )
        {
            if( !item )
                continue;

            if( bb )
                bb->Merge( item->ViewBBox() );
            else
                bb = std::make_unique<BOX2I>( item->ViewBBox() );

            view->Add( item );
        }
    }
    if( bb )
    {
        view->SetBoundary( BOX2D( bb->GetOrigin(), bb->GetSize() * 1.2 ) );
        view->SetViewport( BOX2D( bb->GetOrigin(), bb->GetSize() ) );
        view->SetScale( view->GetScale() * 0.7 );
    }
    m_panelPreview->StartDrawing();
    return true;
}


void DIALOG_IDX_REVIEW::clearIDX()
{
    m_objects.clear();
    m_changes.clear();


    if( m_changeGrid )
        m_changeGrid->DeleteRows( 0, m_changeGrid->GetNumberRows() );

    if( m_panelPreview )
    {
        m_panelPreview->StopDrawing();
        KIGFX::VIEW* view = m_panelPreview->GetView();
        if( view )
            view->Clear();

        m_panelPreview->StartDrawing();
        m_panelPreview->Refresh();
    }
}


wxString StrikeText( const wxString& aTxt )
{
    return "<span strikethrough='true'>" + aTxt + "</span>";
}
wxString BoldText( const wxString& aTxt )
{
    return "<span weight='bold'>" + aTxt + "</span>";
}

wxString GreyText( const wxString& aTxt )
{
    return "<span color='grey'>" + aTxt + "</span>";
}

wxString Format( std::shared_ptr<IDX_FILE_REGISTRY> aFile, const wxString& aTxt )
{
    wxString result = aTxt;

    if( aFile->GetStatus() == IDX_FILE_REGISTRY::STATUS::IDX_PROCESSED )
        result = GreyText( result );
    else if( aFile->GetStatus() == IDX_FILE_REGISTRY::STATUS::IDX_PROCESSED_DELETED )
        result = GreyText( StrikeText( result ) );
    if( aFile->IsProbablyNext() )
    {
        result = BoldText( result );
    }
    return result;
}

void DIALOG_IDX_REVIEW::loadDir( const wxString& aDir )
{
    if( !m_history )
        return;

    // Add a column if it has none
    if( m_history->GetColumnCount() == 0 )
    {
        m_history->AppendTextColumn( "Raw Name" ); // Don't translate, it is not user-facing
        m_history->GetColumn( 0 )->SetHidden( true );

        auto* r1 = new wxDataViewTextRenderer( "string", wxDATAVIEW_CELL_INERT );
        r1->EnableMarkup( true );
        m_history->AppendColumn( new wxDataViewColumn( _( "File Name" ), r1, 1, 200, wxALIGN_LEFT ) );

        auto* r2 = new wxDataViewTextRenderer( "string", wxDATAVIEW_CELL_INERT );
        r2->EnableMarkup( true );
        m_history->AppendColumn( new wxDataViewColumn( _( "Software" ), r2, 2, 200, wxALIGN_LEFT ) );

        auto* r3 = new wxDataViewTextRenderer( "string", wxDATAVIEW_CELL_INERT );
        r3->EnableMarkup( true );
        m_history->AppendColumn( new wxDataViewColumn( _( "Date" ), r3, 3, 200, wxALIGN_LEFT ) );
    }

    m_dir = std::make_shared<wxDir>( aDir );

    if( !m_reg->LoadFolder( aDir ) )
    {
        wxLogError( "Cannot open directory: %s", aDir );
        return;
    }

    m_reg->ComputeNext();
    m_historyReady.clear();
    m_history->DeleteAllItems();


    for( std::shared_ptr<IDX_FILE_REGISTRY> file : m_reg->GetFiles() )
    {
        wxVector<wxVariant> row;

        wxString rawname;
        rawname = wxString( file->GetName() );

        wxString name = rawname;

        m_historyReady.push_back( file->IsProbablyNext() );


        row.push_back( wxVariant( rawname ) );
        row.push_back( wxVariant( Format( file, name ) ) );
        row.push_back( wxVariant( Format( file, file->GetSoft() ) ) );
        row.push_back( wxVariant( Format( file, file->GetDate() ) ) );

        m_history->AppendItem( row );
    }
}


void DIALOG_IDX_REVIEW::OnCollaborationFolderText( wxCommandEvent& event )
{
    loadDir( m_collaborationFolderInput->GetValue() );
}


void DIALOG_IDX_REVIEW::onBrowseButton( wxCommandEvent& event )
{
    wxDirDialog dirDialog( this, _( "Select collaboration folder" ), wxEmptyString,
                           wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST );

    if( dirDialog.ShowModal() == wxID_OK )
    {
        wxString folder = dirDialog.GetPath();
        m_collaborationFolderInput->SetValue( folder );
        m_collaborationFolderInput->SetModified( true );
    }
}

void DIALOG_IDX_REVIEW::OnApply( wxCommandEvent& event )
{
    if( m_reg && m_idx && m_currentFile )
    {
        m_idx->Apply( GetAccepted() );
        m_currentFile->SetStatus( IDX_FILE_REGISTRY::STATUS::IDX_PROCESSED );
        m_reg->Save();
    }

    EndModal( wxID_OK );
}