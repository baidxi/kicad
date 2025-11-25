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

#include "dialog_plm_bom_validation.h"

#include <wx/msgdlg.h>


DIALOG_PLM_BOM_VALIDATION::DIALOG_PLM_BOM_VALIDATION( wxWindow* aParent ) :
        DIALOG_PLM_BOM_VALIDATION_BASE( aParent )
{
    m_bomList->AppendTextColumn( _( "Reference" ), wxDATAVIEW_CELL_INERT, 120,
                                  wxALIGN_LEFT,
                                  wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_bomList->AppendTextColumn( _( "Value" ), wxDATAVIEW_CELL_INERT, 150,
                                  wxALIGN_LEFT,
                                  wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_bomList->AppendTextColumn( _( "Footprint" ), wxDATAVIEW_CELL_INERT, 200,
                                  wxALIGN_LEFT,
                                  wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_bomList->AppendTextColumn( _( "PLM Status" ), wxDATAVIEW_CELL_INERT, 120,
                                  wxALIGN_LEFT,
                                  wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_bomList->AppendTextColumn( _( "PLM Part Number" ), wxDATAVIEW_CELL_INERT, 180,
                                  wxALIGN_LEFT,
                                  wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_bomList->AppendTextColumn( _( "Suggested Replacement" ), wxDATAVIEW_CELL_INERT, 180,
                                  wxALIGN_LEFT,
                                  wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );

    updateButtonStates();

    SetMinSize( wxSize( 900, 600 ) );
    finishDialogSettings();
}


DIALOG_PLM_BOM_VALIDATION::~DIALOG_PLM_BOM_VALIDATION()
{
}


void DIALOG_PLM_BOM_VALIDATION::AddBomItem( const PLM_BOM_ITEM& aItem )
{
    m_items.push_back( aItem );

    wxVector<wxVariant> data;

    data.push_back( wxVariant( aItem.reference ) );
    data.push_back( wxVariant( aItem.value ) );
    data.push_back( wxVariant( aItem.footprint ) );
    data.push_back( wxVariant( GetStatusText( aItem.status ) ) );
    data.push_back( wxVariant( aItem.plm_part_number ) );
    data.push_back( wxVariant( aItem.suggested_replacement ) );

    m_bomList->AppendItem( data );

    UpdateCounts();
}


std::vector<PLM_BOM_ITEM> DIALOG_PLM_BOM_VALIDATION::GetAcceptedSuggestions() const
{
    std::vector<PLM_BOM_ITEM> accepted;

    for( const PLM_BOM_ITEM& item : m_items )
    {
        if( item.accept_suggestion )
            accepted.push_back( item );
    }

    return accepted;
}


void DIALOG_PLM_BOM_VALIDATION::UpdateCounts()
{
    int total = m_items.size();
    int valid = 0;
    int issues = 0;

    for( const PLM_BOM_ITEM& item : m_items )
    {
        if( item.status == PLM_BOM_STATUS::VALID )
            valid++;
        else
            issues++;
    }

    m_totalCountLabel->SetLabel( wxString::Format( _( "Total: %d" ), total ) );
    m_validCountLabel->SetLabel( wxString::Format( _( "Valid: %d" ), valid ) );
    m_issueCountLabel->SetLabel( wxString::Format( _( "Issues: %d" ), issues ) );

    Layout();
}


wxString DIALOG_PLM_BOM_VALIDATION::GetStatusText( PLM_BOM_STATUS aStatus ) const
{
    switch( aStatus )
    {
    case PLM_BOM_STATUS::VALID:
        return _( "Valid" );

    case PLM_BOM_STATUS::MISSING:
        return _( "Missing" );

    case PLM_BOM_STATUS::OBSOLETE:
        return _( "Obsolete" );

    case PLM_BOM_STATUS::MISMATCH:
        return _( "Mismatch" );

    default:
        return _( "Unknown" );
    }
}


void DIALOG_PLM_BOM_VALIDATION::updateButtonStates()
{
    wxDataViewItem sel = m_bomList->GetSelection();
    bool hasSelection = sel.IsOk();
    bool hasSuggestion = false;

    if( hasSelection )
    {
        int storeRow = m_bomList->ItemToRow( sel );

        if( storeRow >= 0 && static_cast<size_t>( storeRow ) < m_items.size() )
        {
            const PLM_BOM_ITEM& item = m_items[storeRow];
            hasSuggestion = !item.suggested_replacement.IsEmpty();
        }
    }

    bool hasAnySuggestions = false;

    for( const PLM_BOM_ITEM& item : m_items )
    {
        if( !item.suggested_replacement.IsEmpty() )
        {
            hasAnySuggestions = true;
            break;
        }
    }

    m_acceptSelectedBtn->Enable( hasSelection && hasSuggestion );
    m_acceptAllBtn->Enable( hasAnySuggestions );
}


void DIALOG_PLM_BOM_VALIDATION::onBomItemSelected( wxDataViewEvent& event )
{
    updateButtonStates();
}


void DIALOG_PLM_BOM_VALIDATION::onAcceptAll( wxCommandEvent& event )
{
    int acceptedCount = 0;

    for( PLM_BOM_ITEM& item : m_items )
    {
        if( !item.suggested_replacement.IsEmpty() && !item.accept_suggestion )
        {
            item.accept_suggestion = true;
            acceptedCount++;
        }
    }

    if( acceptedCount > 0 )
    {
        wxMessageBox( wxString::Format( _( "Accepted %d replacement suggestions." ), acceptedCount ),
                      _( "Suggestions Accepted" ), wxOK | wxICON_INFORMATION, this );
    }

    updateButtonStates();
}


void DIALOG_PLM_BOM_VALIDATION::onAcceptSelected( wxCommandEvent& event )
{
    wxDataViewItem sel = m_bomList->GetSelection();
    int row = sel.IsOk() ? m_bomList->ItemToRow( sel ) : -1;

    if( row >= 0 && static_cast<size_t>( row ) < m_items.size() )
    {
        PLM_BOM_ITEM& item = m_items[row];

        if( !item.suggested_replacement.IsEmpty() )
        {
            item.accept_suggestion = true;

            wxMessageBox( wxString::Format( _( "Accepted replacement for %s:\n%s -> %s" ),
                                             item.reference,
                                             item.plm_part_number,
                                             item.suggested_replacement ),
                          _( "Suggestion Accepted" ), wxOK | wxICON_INFORMATION, this );
        }
    }

    updateButtonStates();
}
