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

#include "dialog_plm_variant.h"

#include <plm/plm_plugin.h>

#include <wx/msgdlg.h>
#include <wx/textdlg.h>


DIALOG_PLM_VARIANT::DIALOG_PLM_VARIANT( wxWindow* aParent, PLM_PLUGIN* aPlugin,
                                         const wxString& aProjectId ) :
        DIALOG_PLM_VARIANT_BASE( aParent ),
        m_plugin( aPlugin ),
        m_projectId( aProjectId ),
        m_selectedIndex( -1 )
{
    m_overridesList->AppendTextColumn( _( "Part Ref" ), wxDATAVIEW_CELL_INERT, 80,
                                       wxALIGN_LEFT,
                                       wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_overridesList->AppendTextColumn( _( "Override Type" ), wxDATAVIEW_CELL_INERT, 100,
                                       wxALIGN_LEFT,
                                       wxDATAVIEW_COL_RESIZABLE );
    m_overridesList->AppendTextColumn( _( "Original Part" ), wxDATAVIEW_CELL_INERT, 120,
                                       wxALIGN_LEFT,
                                       wxDATAVIEW_COL_RESIZABLE );
    m_overridesList->AppendTextColumn( _( "Replacement Part" ), wxDATAVIEW_CELL_INERT, 120,
                                       wxALIGN_LEFT,
                                       wxDATAVIEW_COL_RESIZABLE );
    m_overridesList->AppendTextColumn( _( "New Qty" ), wxDATAVIEW_CELL_INERT, 70,
                                       wxALIGN_RIGHT,
                                       wxDATAVIEW_COL_RESIZABLE );

    clearDetail();
    updateButtonStates();

    SetupStandardButtons();

    SetMinSize( wxSize( 800, 500 ) );
    Layout();
}


DIALOG_PLM_VARIANT::~DIALOG_PLM_VARIANT()
{
}


bool DIALOG_PLM_VARIANT::TransferDataToWindow()
{
    loadVariants();
    populateVariantList();
    return true;
}


bool DIALOG_PLM_VARIANT::TransferDataFromWindow()
{
    saveCurrentVariant();
    return true;
}


void DIALOG_PLM_VARIANT::loadVariants()
{
    if( !m_plugin )
        return;

    m_variantData = m_plugin->GetVariants( m_projectId );
}


void DIALOG_PLM_VARIANT::populateVariantList()
{
    m_variantList->Clear();

    for( const PLM_VARIANT_CONFIG& cfg : m_variantData.configs )
    {
        m_variantList->Append( cfg.name );
    }

    if( !m_variantData.configs.empty() )
    {
        m_variantList->SetSelection( 0 );
        showVariantDetail( 0 );
    }
    else
    {
        clearDetail();
    }

    updateButtonStates();
}


void DIALOG_PLM_VARIANT::showVariantDetail( int aIndex )
{
    if( aIndex < 0 || static_cast<size_t>( aIndex ) >= m_variantData.configs.size() )
    {
        clearDetail();
        return;
    }

    m_selectedIndex = aIndex;
    const PLM_VARIANT_CONFIG& cfg = m_variantData.configs[aIndex];

    m_nameCtrl->SetValue( cfg.name );
    m_descriptionCtrl->SetValue( cfg.description );

    populateOverridesList( cfg );
    updateButtonStates();
}


void DIALOG_PLM_VARIANT::clearDetail()
{
    m_selectedIndex = -1;
    m_nameCtrl->SetValue( wxEmptyString );
    m_descriptionCtrl->SetValue( wxEmptyString );
    m_overridesList->DeleteAllItems();
    updateButtonStates();
}


void DIALOG_PLM_VARIANT::saveCurrentVariant()
{
    if( m_selectedIndex < 0 ||
        static_cast<size_t>( m_selectedIndex ) >= m_variantData.configs.size() )
    {
        return;
    }

    PLM_VARIANT_CONFIG& cfg = m_variantData.configs[m_selectedIndex];
    cfg.name = m_nameCtrl->GetValue();
    cfg.description = m_descriptionCtrl->GetValue();

    if( m_plugin )
        m_plugin->UpdateVariant( cfg );
}


void DIALOG_PLM_VARIANT::populateOverridesList( const PLM_VARIANT_CONFIG& aConfig )
{
    m_overridesList->DeleteAllItems();

    for( const PLM_VARIANT_BOM_OVERRIDE& ovr : aConfig.overrides )
    {
        wxVector<wxVariant> data;

        data.push_back( wxVariant( ovr.reference ) );
        data.push_back( wxVariant( getOverrideTypeName( ovr.override_type ) ) );
        data.push_back( wxVariant( ovr.original_part_id ) );
        data.push_back( wxVariant( ovr.replacement_part_id ) );
        data.push_back( wxVariant( ovr.new_value ) );

        m_overridesList->AppendItem( data );
    }
}


void DIALOG_PLM_VARIANT::updateButtonStates()
{
    bool hasSelection = m_variantList->GetSelection() != wxNOT_FOUND;

    m_deleteVariantBtn->Enable( hasSelection );
    m_nameCtrl->Enable( hasSelection );
    m_descriptionCtrl->Enable( hasSelection );
    m_addOverrideBtn->Enable( hasSelection );
    m_removeOverrideBtn->Enable( hasSelection && m_overridesList->GetSelectedRow() != wxNOT_FOUND );
}


wxString DIALOG_PLM_VARIANT::getOverrideTypeName( PLM_VARIANT_OVERRIDE_TYPE aType ) const
{
    switch( aType )
    {
    case PLM_VARIANT_OVERRIDE_TYPE::SUBSTITUTE: return _( "Substitute" );
    case PLM_VARIANT_OVERRIDE_TYPE::DNP:        return _( "DNP" );
    case PLM_VARIANT_OVERRIDE_TYPE::VALUE:      return _( "Value" );
    case PLM_VARIANT_OVERRIDE_TYPE::CUSTOM:     return _( "Custom" );
    default:                                    return _( "Unknown" );
    }
}


void DIALOG_PLM_VARIANT::onVariantSelected( wxCommandEvent& event )
{
    if( m_selectedIndex >= 0 )
        saveCurrentVariant();

    int selection = m_variantList->GetSelection();
    showVariantDetail( selection );
}


void DIALOG_PLM_VARIANT::onAddVariant( wxCommandEvent& event )
{
    wxTextEntryDialog dlg( this, _( "Enter variant name:" ), _( "New Variant" ) );

    if( dlg.ShowModal() != wxID_OK )
        return;

    wxString name = dlg.GetValue().Trim().Trim( false );

    if( name.IsEmpty() )
        return;

    if( m_selectedIndex >= 0 )
        saveCurrentVariant();

    PLM_VARIANT_CONFIG newConfig;
    newConfig.name = name;

    if( m_plugin )
    {
        PLM_VARIANT_RESULT result = m_plugin->CreateVariant( m_projectId, newConfig );

        if( !result.success )
        {
            wxMessageBox( wxString::Format( _( "Failed to create variant:\n%s" ),
                                             result.error_message ),
                          _( "Error" ), wxOK | wxICON_ERROR, this );
            return;
        }

        newConfig.config_id = result.config_id;
    }

    m_variantData.configs.push_back( newConfig );
    m_variantList->Append( name );

    int newIndex = static_cast<int>( m_variantData.configs.size() ) - 1;
    m_variantList->SetSelection( newIndex );
    showVariantDetail( newIndex );
}


void DIALOG_PLM_VARIANT::onDeleteVariant( wxCommandEvent& event )
{
    int selection = m_variantList->GetSelection();

    if( selection == wxNOT_FOUND )
        return;

    const PLM_VARIANT_CONFIG& cfg = m_variantData.configs[selection];

    int answer = wxMessageBox(
            wxString::Format( _( "Delete variant '%s'?" ), cfg.name ),
            _( "Confirm Delete" ), wxYES_NO | wxICON_QUESTION, this );

    if( answer != wxYES )
        return;

    if( m_plugin && !cfg.config_id.IsEmpty() )
        m_plugin->DeleteVariant( cfg.config_id );

    m_variantData.configs.erase( m_variantData.configs.begin() + selection );
    m_variantList->Delete( selection );

    if( !m_variantData.configs.empty() )
    {
        int newSel = std::min( selection, static_cast<int>( m_variantData.configs.size() ) - 1 );
        m_variantList->SetSelection( newSel );
        showVariantDetail( newSel );
    }
    else
    {
        clearDetail();
    }
}


void DIALOG_PLM_VARIANT::onAddOverride( wxCommandEvent& event )
{
    if( m_selectedIndex < 0 ||
        static_cast<size_t>( m_selectedIndex ) >= m_variantData.configs.size() )
    {
        return;
    }

    wxTextEntryDialog dlg( this, _( "Enter reference designator (e.g., R1):" ),
                           _( "Add Override" ) );

    if( dlg.ShowModal() != wxID_OK )
        return;

    wxString ref = dlg.GetValue().Trim().Trim( false );

    if( ref.IsEmpty() )
        return;

    PLM_VARIANT_BOM_OVERRIDE ovr( ref, PLM_VARIANT_OVERRIDE_TYPE::SUBSTITUTE );

    PLM_VARIANT_CONFIG& cfg = m_variantData.configs[m_selectedIndex];
    cfg.overrides.push_back( ovr );

    wxVector<wxVariant> data;
    data.push_back( wxVariant( ovr.reference ) );
    data.push_back( wxVariant( getOverrideTypeName( ovr.override_type ) ) );
    data.push_back( wxVariant( ovr.original_part_id ) );
    data.push_back( wxVariant( ovr.replacement_part_id ) );
    data.push_back( wxVariant( ovr.new_value ) );

    m_overridesList->AppendItem( data );
}


void DIALOG_PLM_VARIANT::onRemoveOverride( wxCommandEvent& event )
{
    int row = m_overridesList->GetSelectedRow();

    if( row == wxNOT_FOUND )
        return;

    if( m_selectedIndex < 0 ||
        static_cast<size_t>( m_selectedIndex ) >= m_variantData.configs.size() )
    {
        return;
    }

    PLM_VARIANT_CONFIG& cfg = m_variantData.configs[m_selectedIndex];

    if( static_cast<size_t>( row ) < cfg.overrides.size() )
        cfg.overrides.erase( cfg.overrides.begin() + row );

    m_overridesList->DeleteItem( row );
    updateButtonStates();
}
