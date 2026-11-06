/**
 * @file dialog_migrate_symbols.cpp
 */

/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <algorithm>

#include <confirm.h>
#include <lib_id.h>
#include <project_sch.h>
#include <sch_field.h>
#include <sch_symbol.h>
#include <symbol_edit_frame.h>
#include <libraries/symbol_library_adapter.h>
#include <template_fieldnames.h>

#include <wx/listctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>

#include "dialog_change_symbols.h"
#include "dialog_migrate_symbols.h"


/// The lib_id text of a symbol, as shown in the list
static wxString libIdText( const LIB_ID& aLibId )
{
    return wxString( aLibId.GetLibNickname().c_str() ) + wxT( ":" ) + wxString( aLibId.GetLibItemName().c_str() );
}


/// A definition needs migration when its lib_id has no library nickname (a legacy
/// cached copy stored in the schematic file), when the library tables no longer
/// contain its library, or when the library does not contain the symbol.
static bool symbolNeedsMigration( SCH_SYMBOL* aSymbol, SYMBOL_LIBRARY_ADAPTER* aAdapter )
{
    LIB_ID libId = aSymbol->GetLibId();

    if( libId.GetLibNickname().empty() )
        return true; // cached copy with no library

    wxString nickname( libId.GetLibNickname().c_str() );

    // The library must exist in the symbol library tables
    if( !aAdapter->HasLibrary( nickname ) )
        return true;

    // The symbol must exist in that library (loading the library if needed)
    try
    {
        if( !aAdapter->IsLibraryLoaded( nickname ) )
            aAdapter->LoadOne( nickname );

        std::vector<wxString> names = aAdapter->GetSymbolNames( nickname );

        return std::find( names.begin(), names.end(), wxString( libId.GetLibItemName().c_str() ) ) == names.end();
    }
    catch( const IO_ERROR& )
    {
        return true;
    }
}


DIALOG_MIGRATE_SYMBOLS::DIALOG_MIGRATE_SYMBOLS( SCH_EDIT_FRAME* aFrame ) :
        DIALOG_SHIM( aFrame, wxID_ANY, _( "Migrate Symbols" ), wxDefaultPosition, wxDefaultSize,
                     wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER ),
        m_frame( aFrame )
{
    SetSizeHints( wxDefaultSize, wxDefaultSize );

    wxBoxSizer* topSizer = new wxBoxSizer( wxVERTICAL );

    m_statusLabel = new wxStaticText( this, wxID_ANY, wxEmptyString );
    topSizer->Add( m_statusLabel, 0, wxEXPAND | wxALL, 10 );

    wxStaticText* infoLabel =
            new wxStaticText( this, wxID_ANY,
                              _( "Double-click a row to migrate every instance of that symbol. In the dialog "
                                 "that opens, fill in the New Symbol ID (use the browse button), then click "
                                 "Change. The list refreshes automatically after each replacement." ) );
    infoLabel->Wrap( 480 );
    topSizer->Add( infoLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6 );

    m_list = new wxListCtrl( this, wxID_ANY, wxDefaultPosition, wxSize( 520, 320 ),
                             wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES );

    m_list->InsertColumn( 0, _( "Reference" ), wxLIST_FORMAT_LEFT, 130 );
    m_list->InsertColumn( 1, _( "Lib ID" ), wxLIST_FORMAT_LEFT, 240 );
    m_list->InsertColumn( 2, _( "Status" ), wxLIST_FORMAT_LEFT, 130 );

    topSizer->Add( m_list, 1, wxEXPAND | wxLEFT | wxRIGHT, 5 );

    wxStaticLine* separator = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    topSizer->Add( separator, 0, wxEXPAND | wxALL, 5 );

    wxBoxSizer* buttonSizer = new wxBoxSizer( wxHORIZONTAL );
    wxButton*   closeBtn = new wxButton( this, wxID_CANCEL, _( "Close" ) );
    buttonSizer->Add( closeBtn, 0, wxALL, 5 );
    topSizer->Add( buttonSizer, 0, wxALIGN_CENTER_HORIZONTAL, 3 );

    SetSizer( topSizer );
    Layout();
    topSizer->Fit( this );
    topSizer->SetSizeHints( this );

    Centre();

    m_list->Bind( wxEVT_LIST_ITEM_ACTIVATED, &DIALOG_MIGRATE_SYMBOLS::onItemActivated, this );
    closeBtn->Bind( wxEVT_BUTTON, &DIALOG_MIGRATE_SYMBOLS::onClose, this );

    rescan();

    finishDialogSettings();
}


void DIALOG_MIGRATE_SYMBOLS::rescan()
{
    m_list->DeleteAllItems();
    m_rowGroups.clear();

    SYMBOL_LIBRARY_ADAPTER* adapter = PROJECT_SCH::SymbolLibAdapter( &m_frame->Prj() );

    // Group the broken instances by lib_id; instances of one definition are
    // migrated together, so they belong on one row
    std::map<wxString, std::vector<KIID>> groups;
    std::map<wxString, wxString>          firstReference;

    for( SCH_ITEM* item : m_frame->GetScreen()->Items() )
    {
        if( item->Type() != SCH_SYMBOL_T )
            continue;

        SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );

        if( !symbolNeedsMigration( symbol, adapter ) )
            continue;

        wxString text = libIdText( symbol->GetLibId() );
        groups[text].push_back( symbol->m_Uuid );

        if( !firstReference.count( text ) )
            firstReference[text] = symbol->GetField( FIELD_T::REFERENCE )->GetText();
    }

    for( auto& [text, ids] : groups )
        m_rowGroups.emplace_back( text, std::move( ids ) );

    for( size_t row = 0; row < m_rowGroups.size(); ++row )
    {
        const wxString& text = m_rowGroups[row].first;
        size_t          count = m_rowGroups[row].second.size();

        long item = m_list->InsertItem( m_list->GetItemCount(), firstReference[text] );

        if( count > 1 )
            m_list->SetItem( item, 0, firstReference[text] + wxString::Format( wxT( " (+%d)" ), (int) count - 1 ) );

        m_list->SetItem( item, 1, text );
        m_list->SetItem( item, 2, _( "Not in any library" ) );
    }

    if( m_rowGroups.empty() )
        m_statusLabel->SetLabel( _( "All symbols resolve to their libraries; "
                                    "no migration needed." ) );
    else
        m_statusLabel->SetLabel( wxString::Format( _( "%d symbol definition(s) need "
                                                      "migration:" ),
                                                   (int) m_rowGroups.size() ) );
}


std::vector<SCH_SYMBOL*> DIALOG_MIGRATE_SYMBOLS::symbolsFromRow( int aRow ) const
{
    std::vector<SCH_SYMBOL*> symbols;

    if( aRow < 0 || aRow >= (int) m_rowGroups.size() )
        return symbols;

    for( SCH_ITEM* item : m_frame->GetScreen()->Items() )
    {
        if( item->Type() != SCH_SYMBOL_T )
            continue;

        if( std::find( m_rowGroups[aRow].second.begin(), m_rowGroups[aRow].second.end(), item->m_Uuid )
            != m_rowGroups[aRow].second.end() )
        {
            symbols.push_back( static_cast<SCH_SYMBOL*>( item ) );
        }
    }

    return symbols;
}


void DIALOG_MIGRATE_SYMBOLS::onItemActivated( wxListEvent& aEvent )
{
    std::vector<SCH_SYMBOL*> symbols = symbolsFromRow( aEvent.GetIndex() );

    if( symbols.empty() )
        return;

    // Select every instance of the definition so the Change Symbol dialog matches
    // them all by selection
    for( SCH_SYMBOL* symbol : symbols )
        symbol->SetSelected();

    {
        DIALOG_CHANGE_SYMBOLS dlg( m_frame, symbols.front(), DIALOG_CHANGE_SYMBOLS::MODE::CHANGE );

        // True modal child: it returns when closed and only the inner event loop is
        // affected, so the list window stays open
        dlg.ShowModal();
    }

    for( SCH_SYMBOL* symbol : symbols )
        symbol->ClearSelected();

    // Refresh the list so migrated rows drop out and the rest stay actionable
    rescan();
}


void DIALOG_MIGRATE_SYMBOLS::onClose( wxCommandEvent& aEvent )
{
    Destroy();
}
