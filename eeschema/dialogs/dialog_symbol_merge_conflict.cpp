/**
 * @file dialog_symbol_merge_conflict.cpp
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

#include <lib_id.h>
#include <symbol_edit_frame.h>
#include <widgets/symbol_preview_widget.h>

#include <wx/listctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>

#include "dialog_symbol_merge_conflict.h"

///< Check mark shown next to the version chosen to be kept
static const wxString s_checkMark = wxString::FromUTF8( "\u2713" );


DIALOG_SYMBOL_MERGE_CONFLICTS::DIALOG_SYMBOL_MERGE_CONFLICTS( wxWindow* aParent, SYMBOL_EDIT_FRAME* aFrame,
                                                              const wxString&                           aTargetLibrary,
                                                              const std::vector<SYMBOL_MERGE_CONFLICT>& aConflicts ) :
        DIALOG_SHIM( aParent, wxID_ANY, _( "Resolve Symbol Conflicts" ), wxDefaultPosition, wxDefaultSize,
                     wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER ),
        m_frame( aFrame ),
        m_targetLibrary( aTargetLibrary ),
        m_conflicts( aConflicts ),
        m_currentConflict( 0 )
{
    SetSizeHints( wxDefaultSize, wxDefaultSize );

    // Safe default for every conflict: keep the version already in the target
    // library when there is one, otherwise the first source version
    for( const SYMBOL_MERGE_CONFLICT& conflict : m_conflicts )
    {
        bool targetProvides = std::find( conflict.providers.begin(), conflict.providers.end(), m_targetLibrary )
                              != conflict.providers.end();

        m_choices[conflict.symbolName] = targetProvides ? m_targetLibrary : conflict.providers.front();
    }

    wxBoxSizer* topSizer = new wxBoxSizer( wxVERTICAL );

    m_titleLabel = new wxStaticText( this, wxID_ANY, wxEmptyString );
    m_titleLabel->SetFont( m_titleLabel->GetFont().Bold() );
    topSizer->Add( m_titleLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10 );

    wxStaticText* infoLabel =
            new wxStaticText( this, wxID_ANY,
                              _( "Click a version to preview it; double click to choose it as the version to "
                                 "keep (marked with a check mark). The choice can be changed at any time." ) );
    infoLabel->Wrap( 520 );
    topSizer->Add( infoLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6 );

    wxBoxSizer* mainSizer = new wxBoxSizer( wxHORIZONTAL );

    // One row per version of the current conflict; row number == provider index
    m_conflictList = new wxListCtrl( this, wxID_ANY, wxDefaultPosition, wxSize( 300, 300 ),
                                     wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES );

    m_conflictList->InsertColumn( 0, _( "Library" ), wxLIST_FORMAT_LEFT, 170 );
    m_conflictList->InsertColumn( 1, _( "Keep" ), wxLIST_FORMAT_CENTER, 70 );

    mainSizer->Add( m_conflictList, 0, wxALL, 5 );

    // Preview of the selected version
    wxBoxSizer* previewSizer = new wxBoxSizer( wxVERTICAL );

    m_previewLabel = new wxStaticText( this, wxID_ANY, wxEmptyString );
    previewSizer->Add( m_previewLabel, 0, wxEXPAND | wxBOTTOM, 3 );

    EDA_DRAW_PANEL_GAL::GAL_TYPE backend = m_frame->GetCanvas()->GetBackend();
    m_preview = new SYMBOL_PREVIEW_WIDGET( this, &m_frame->Kiway(), true, backend );
    previewSizer->Add( m_preview, 1, wxEXPAND );

    mainSizer->Add( previewSizer, 1, wxEXPAND | wxALL, 5 );

    topSizer->Add( mainSizer, 1, wxEXPAND | wxLEFT | wxRIGHT, 5 );

    // Conflict navigation below the list
    wxBoxSizer* navSizer = new wxBoxSizer( wxHORIZONTAL );

    m_prevBtn = new wxButton( this, wxID_ANY, _( "< Previous" ) );
    m_nextBtn = new wxButton( this, wxID_ANY, _( "Next >" ) );

    navSizer->Add( m_prevBtn, 0, wxALL, 3 );
    navSizer->Add( m_nextBtn, 0, wxALL, 3 );

    topSizer->Add( navSizer, 0, wxALIGN_CENTER_HORIZONTAL, 3 );

    wxStaticLine* separator = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    topSizer->Add( separator, 0, wxEXPAND | wxALL, 5 );

    wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer();
    buttons->AddButton( new wxButton( this, wxID_OK ) );
    buttons->AddButton( new wxButton( this, wxID_CANCEL ) );
    buttons->Realize();

    topSizer->Add( buttons, 0, wxEXPAND | wxALL, 5 );

    SetSizer( topSizer );
    SetSize( wxSize( 780, 540 ) );
    Layout();
    topSizer->SetSizeHints( this );

    Centre();

    m_conflictList->Bind( wxEVT_LIST_ITEM_SELECTED, &DIALOG_SYMBOL_MERGE_CONFLICTS::onListSelect, this );
    m_conflictList->Bind( wxEVT_LIST_ITEM_ACTIVATED, &DIALOG_SYMBOL_MERGE_CONFLICTS::onListItemActivated, this );
    m_prevBtn->Bind( wxEVT_BUTTON, &DIALOG_SYMBOL_MERGE_CONFLICTS::onPrevConflict, this );
    m_nextBtn->Bind( wxEVT_BUTTON, &DIALOG_SYMBOL_MERGE_CONFLICTS::onNextConflict, this );
    Bind( wxEVT_BUTTON, &DIALOG_SYMBOL_MERGE_CONFLICTS::onOk, this, wxID_OK );

    loadConflict( 0 );

    finishDialogSettings();
}


int DIALOG_SYMBOL_MERGE_CONFLICTS::currentRow() const
{
    long item = -1;
    item = m_conflictList->GetNextItem( item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
    return (int) item;
}


void DIALOG_SYMBOL_MERGE_CONFLICTS::loadConflict( int aIndex )
{
    m_currentConflict = aIndex;

    const SYMBOL_MERGE_CONFLICT& conflict = m_conflicts[m_currentConflict];

    m_titleLabel->SetLabel( wxString::Format( _( "Conflict %d of %d: symbol '%s'" ), m_currentConflict + 1,
                                              (int) m_conflicts.size(), conflict.symbolName ) );

    m_conflictList->DeleteAllItems();

    for( size_t providerIdx = 0; providerIdx < conflict.providers.size(); ++providerIdx )
    {
        const wxString& provider = conflict.providers[providerIdx];

        m_conflictList->InsertItem(
                m_conflictList->GetItemCount(),
                provider == m_targetLibrary ? wxString::Format( _( "%s (target)" ), m_targetLibrary ) : provider );
    }

    refreshChecks();

    // Select and preview the currently chosen version
    const wxString& kept = m_choices[conflict.symbolName];

    for( size_t providerIdx = 0; providerIdx < conflict.providers.size(); ++providerIdx )
    {
        if( conflict.providers[providerIdx] == kept )
        {
            m_conflictList->SetItemState( (long) providerIdx, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                                          wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED );
            previewRow( (int) providerIdx );
            break;
        }
    }

    updateNavControls();
    Layout();
}


void DIALOG_SYMBOL_MERGE_CONFLICTS::refreshChecks()
{
    const SYMBOL_MERGE_CONFLICT& conflict = m_conflicts[m_currentConflict];
    const wxString&              kept = m_choices[conflict.symbolName];

    for( long row = 0; row < (long) conflict.providers.size(); ++row )
    {
        m_conflictList->SetItem( row, 1, conflict.providers[row] == kept ? s_checkMark : wxString() );
    }
}


void DIALOG_SYMBOL_MERGE_CONFLICTS::previewRow( int aRow )
{
    const SYMBOL_MERGE_CONFLICT& conflict = m_conflicts[m_currentConflict];

    if( aRow < 0 || aRow >= (int) conflict.providers.size() )
        return;

    const wxString& provider = conflict.providers[aRow];

    m_previewLabel->SetLabel( wxString::Format( _( "Preview: %s / %s" ), provider, conflict.symbolName ) );
    m_preview->DisplaySymbol( LIB_ID( provider, conflict.symbolName ), 0, 0 );
}


void DIALOG_SYMBOL_MERGE_CONFLICTS::updateNavControls()
{
    m_prevBtn->Enable( m_currentConflict > 0 );

    bool lastConflict = m_currentConflict >= (int) m_conflicts.size() - 1;
    m_nextBtn->SetLabel( lastConflict ? _( "Finish" ) : _( "Next >" ) );
}


void DIALOG_SYMBOL_MERGE_CONFLICTS::onListSelect( wxListEvent& aEvent )
{
    previewRow( aEvent.GetIndex() );
}


void DIALOG_SYMBOL_MERGE_CONFLICTS::onListItemActivated( wxListEvent& aEvent )
{
    const SYMBOL_MERGE_CONFLICT& conflict = m_conflicts[m_currentConflict];
    int                          row = aEvent.GetIndex();

    if( row < 0 || row >= (int) conflict.providers.size() )
        return;

    m_choices[conflict.symbolName] = conflict.providers[row];
    refreshChecks();
    previewRow( row );
}


void DIALOG_SYMBOL_MERGE_CONFLICTS::onPrevConflict( wxCommandEvent& aEvent )
{
    if( m_currentConflict > 0 )
        loadConflict( m_currentConflict - 1 );
}


void DIALOG_SYMBOL_MERGE_CONFLICTS::onNextConflict( wxCommandEvent& aEvent )
{
    if( m_currentConflict < (int) m_conflicts.size() - 1 )
    {
        loadConflict( m_currentConflict + 1 );
    }
    else
    {
        // Already at the last conflict: finish the dialog
        EndQuasiModal( wxID_OK );
    }
}


void DIALOG_SYMBOL_MERGE_CONFLICTS::onOk( wxCommandEvent& aEvent )
{
    EndQuasiModal( wxID_OK );
}
