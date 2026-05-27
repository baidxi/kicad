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

#include "dialog_plm_bom.h"
#include <plm/plm_plugin.h>

#include <wx/msgdlg.h>
#include <wx/textdlg.h>


DIALOG_PLM_BOM::DIALOG_PLM_BOM( wxWindow* aParent, PLM_PLUGIN* aPlugin,
                                  const wxString& aProductId ) :
        DIALOG_PLM_BOM_BASE( aParent ),
        m_plugin( aPlugin ),
        m_productId( aProductId )
{
    m_bomTypeChoice->Append( _( "Engineering" ) );
    m_bomTypeChoice->Append( _( "Manufacturing" ) );
    m_bomTypeChoice->Append( _( "Spare Parts" ) );
    m_bomTypeChoice->SetSelection( 0 );

    setupColumns();
    loadBom();

    SetupStandardButtons();
    finishDialogSettings();
}


DIALOG_PLM_BOM::~DIALOG_PLM_BOM()
{
}


void DIALOG_PLM_BOM::setupColumns()
{
    m_bomListCtrl->AppendTextColumn( _( "Line #" ), wxDATAVIEW_CELL_INERT, 60 );
    m_bomListCtrl->AppendTextColumn( _( "Reference" ), wxDATAVIEW_CELL_INERT, 100 );
    m_bomListCtrl->AppendTextColumn( _( "Part Number" ), wxDATAVIEW_CELL_INERT, 140 );
    m_bomListCtrl->AppendTextColumn( _( "Description" ), wxDATAVIEW_CELL_INERT, 200 );
    m_bomListCtrl->AppendTextColumn( _( "Qty" ), wxDATAVIEW_CELL_INERT, 60 );
    m_bomListCtrl->AppendTextColumn( _( "UoM" ), wxDATAVIEW_CELL_INERT, 60 );

    m_validationListCtrl->AppendTextColumn( _( "Severity" ), wxDATAVIEW_CELL_INERT, 80 );
    m_validationListCtrl->AppendTextColumn( _( "Reference" ), wxDATAVIEW_CELL_INERT, 80 );
    m_validationListCtrl->AppendTextColumn( _( "Part Number" ), wxDATAVIEW_CELL_INERT, 120 );
    m_validationListCtrl->AppendTextColumn( _( "Message" ), wxDATAVIEW_CELL_INERT, 250 );
    m_validationListCtrl->AppendTextColumn( _( "Suggestion" ), wxDATAVIEW_CELL_INERT, 120 );
}


void DIALOG_PLM_BOM::loadBom()
{
    if( !m_plugin )
        return;

    PLM_BOM_TYPE type = getSelectedBomType();
    m_currentBom = m_plugin->GetBOM( m_productId, type );

    populateBomList( m_currentBom );
    clearValidation();

    m_availableBoms = m_plugin->GetBOMs( m_productId );
}


void DIALOG_PLM_BOM::populateBomList( const PLM_BOM& aBom )
{
    m_bomListCtrl->DeleteAllItems();

    int lineNum = 1;

    for( const auto& line : aBom.lines )
    {
        wxVector<wxVariant> data;
        data.push_back( wxVariant( wxString::Format( wxS( "%d" ), lineNum++ ) ) );
        data.push_back( wxVariant( line.reference ) );
        data.push_back( wxVariant( line.part_number ) );
        data.push_back( wxVariant( line.description ) );
        data.push_back( wxVariant( wxString::Format( wxS( "%.0f" ), line.quantity ) ) );
        data.push_back( wxVariant( line.uom.IsEmpty() ? wxString( wxS( "pcs" ) ) : line.uom ) );

        m_bomListCtrl->AppendItem( data );
    }
}


void DIALOG_PLM_BOM::populateValidationResults( const PLM_BOM_VALIDATION_RESULT& aResult )
{
    m_validationListCtrl->DeleteAllItems();

    if( aResult.all_valid )
    {
        wxVector<wxVariant> data;
        data.push_back( wxVariant( wxString( wxS( "Info" ) ) ) );
        data.push_back( wxVariant( wxString( wxS( "-" ) ) ) );
        data.push_back( wxVariant( wxString( wxS( "-" ) ) ) );
        data.push_back( wxVariant( wxString::Format( _( "All %zu parts validated successfully." ),
                                                     aResult.total_parts ) ) );
        data.push_back( wxVariant( wxEmptyString ) );

        m_validationListCtrl->AppendItem( data );
        return;
    }

    for( const auto& issue : aResult.issues )
    {
        wxVector<wxVariant> data;

        wxString severity;

        switch( issue.severity )
        {
        case PLM_BOM_ISSUE_SEVERITY::ERROR:   severity = wxS( "Error" );   break;
        case PLM_BOM_ISSUE_SEVERITY::WARNING: severity = wxS( "Warning" ); break;
        case PLM_BOM_ISSUE_SEVERITY::INFO:    severity = wxS( "Info" );    break;
        }

        data.push_back( wxVariant( severity ) );
        data.push_back( wxVariant( issue.reference ) );
        data.push_back( wxVariant( issue.part_number ) );
        data.push_back( wxVariant( issue.message ) );
        data.push_back( wxVariant( issue.suggested_part ) );

        m_validationListCtrl->AppendItem( data );
    }

    m_validationLabel->SetLabel( wxString::Format( _( "Validation Results: %zu issues found" ),
                                                   aResult.issues.size() ) );
}


void DIALOG_PLM_BOM::clearValidation()
{
    m_validationListCtrl->DeleteAllItems();
    m_validationLabel->SetLabel( _( "Validation Results:" ) );
}


PLM_BOM_TYPE DIALOG_PLM_BOM::getSelectedBomType() const
{
    switch( m_bomTypeChoice->GetSelection() )
    {
    case 0:  return PLM_BOM_TYPE::ENGINEERING;
    case 1:  return PLM_BOM_TYPE::MANUFACTURING;
    case 2:  return PLM_BOM_TYPE::SPARE_PARTS;
    default: return PLM_BOM_TYPE::ENGINEERING;
    }
}


wxString DIALOG_PLM_BOM::formatBomType( PLM_BOM_TYPE aType ) const
{
    switch( aType )
    {
    case PLM_BOM_TYPE::ENGINEERING:    return _( "Engineering" );
    case PLM_BOM_TYPE::MANUFACTURING:  return _( "Manufacturing" );
    case PLM_BOM_TYPE::SPARE_PARTS:    return _( "Spare Parts" );
    case PLM_BOM_TYPE::PHANTOM:        return _( "Phantom" );
    default:                           return _( "Unknown" );
    }
}


void DIALOG_PLM_BOM::onBomTypeChanged( wxCommandEvent& event )
{
    loadBom();
    event.Skip();
}


void DIALOG_PLM_BOM::onCreateBom( wxCommandEvent& event )
{
    if( !m_plugin )
        return;

    PLM_BOM newBom( m_productId, getSelectedBomType() );

    PLM_BOM_RESULT result = m_plugin->CreateBOM( newBom );

    if( !result.success )
    {
        wxMessageBox( wxString::Format( _( "Failed to create BOM: %s" ),
                                        result.error_message ),
                      _( "Error" ), wxOK | wxICON_ERROR, this );
        return;
    }

    wxMessageBox( _( "BOM created successfully." ), _( "BOM Created" ),
                  wxOK | wxICON_INFORMATION, this );

    loadBom();
}


void DIALOG_PLM_BOM::onDeleteBom( wxCommandEvent& event )
{
    if( !m_plugin )
        return;

    if( m_currentBom.bom_id.IsEmpty() )
    {
        wxMessageBox( _( "No BOM loaded to delete." ), _( "Error" ),
                      wxOK | wxICON_WARNING, this );
        return;
    }

    int answer = wxMessageBox( _( "Are you sure you want to delete this BOM?" ),
                               _( "Confirm Delete" ),
                               wxYES_NO | wxICON_QUESTION, this );

    if( answer != wxYES )
        return;

    if( !m_plugin->DeleteBOM( m_currentBom.bom_id ) )
    {
        wxMessageBox( _( "Failed to delete BOM." ), _( "Error" ),
                      wxOK | wxICON_ERROR, this );
        return;
    }

    wxMessageBox( _( "BOM deleted." ), _( "Deleted" ),
                  wxOK | wxICON_INFORMATION, this );

    loadBom();
}


void DIALOG_PLM_BOM::onDiffBom( wxCommandEvent& event )
{
    if( !m_plugin )
        return;

    if( m_availableBoms.size() < 2 )
    {
        wxMessageBox( _( "At least two BOMs are required for comparison." ),
                      _( "Cannot Compare" ), wxOK | wxICON_WARNING, this );
        return;
    }

    // Compare current BOM with the first available BOM of a different type
    PLM_BOM compareBom;

    for( const auto& bom : m_availableBoms )
    {
        if( bom.bom_id != m_currentBom.bom_id )
        {
            compareBom = bom;
            break;
        }
    }

    if( compareBom.bom_id.IsEmpty() )
    {
        wxMessageBox( _( "No other BOM available for comparison." ),
                      _( "Cannot Compare" ), wxOK | wxICON_WARNING, this );
        return;
    }

    PLM_BOM_DIFF_OPTIONS diffOpts;
    PLM_BOM_DIFF_RESULT diffResult = m_plugin->CompareBOMs( m_currentBom, compareBom, diffOpts );

    // Display diff results in the validation panel
    m_validationListCtrl->DeleteAllItems();

    m_validationLabel->SetLabel( wxString::Format(
            _( "BOM Comparison: %zu added, %zu removed, %zu changed" ),
            diffResult.added_count, diffResult.removed_count, diffResult.changed_count ) );

    for( const auto& entry : diffResult.entries )
    {
        if( entry.change_type == PLM_BOM_DIFF_TYPE::UNCHANGED )
            continue;

        wxVector<wxVariant> data;

        wxString changeType;

        switch( entry.change_type )
        {
        case PLM_BOM_DIFF_TYPE::ADDED:   changeType = wxS( "Added" );   break;
        case PLM_BOM_DIFF_TYPE::REMOVED: changeType = wxS( "Removed" ); break;
        case PLM_BOM_DIFF_TYPE::CHANGED: changeType = wxS( "Changed" ); break;
        default:                         changeType = wxS( "?" );        break;
        }

        const PLM_BOM_LINE& line = ( entry.change_type == PLM_BOM_DIFF_TYPE::REMOVED )
                                           ? entry.old_line
                                           : entry.new_line;

        data.push_back( wxVariant( changeType ) );
        data.push_back( wxVariant( line.reference ) );
        data.push_back( wxVariant( line.part_number ) );
        data.push_back( wxVariant( entry.change_detail ) );
        data.push_back( wxVariant( wxEmptyString ) );

        m_validationListCtrl->AppendItem( data );
    }
}


void DIALOG_PLM_BOM::onValidateBom( wxCommandEvent& event )
{
    if( !m_plugin )
        return;

    if( m_currentBom.lines.empty() )
    {
        wxMessageBox( _( "No BOM lines to validate." ), _( "Validation" ),
                      wxOK | wxICON_WARNING, this );
        return;
    }

    std::vector<PLM_BOM_ENTRY> entries;
    entries.reserve( m_currentBom.lines.size() );

    for( const auto& line : m_currentBom.lines )
    {
        PLM_BOM_ENTRY entry;
        entry.reference = line.reference;
        entry.part_number = line.part_number;
        entry.value = line.value;
        entry.footprint = line.footprint;
        entry.quantity = line.quantity;
        entry.dnp = line.dnp;
        entries.push_back( entry );
    }

    PLM_BOM_VALIDATION_RESULT result = m_plugin->ValidateBOMEntries( entries );
    result.total_parts = entries.size();

    populateValidationResults( result );
}


void DIALOG_PLM_BOM::onBomItemSelected( wxDataViewEvent& event )
{
    event.Skip();
}
