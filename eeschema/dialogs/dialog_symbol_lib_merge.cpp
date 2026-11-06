/**
 * @file dialog_symbol_lib_merge.cpp
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

#include <confirm.h>
#include <common.h>
#include <kiway.h>
#include <lib_id.h>
#include <lib_symbol.h>
#include <pgm_base.h>
#include <project_sch.h>
#include <symbol_edit_frame.h>

#include <libraries/library_manager.h>
#include <libraries/library_table.h>
#include <libraries/symbol_library_adapter.h>

#include <wx/checklst.h>
#include <wx/simplebook.h>
#include <wx/statline.h>

#include "dialog_symbol_merge_conflict.h"
#include "dialog_symbol_lib_merge.h"


DIALOG_SYMBOL_LIB_MERGE::DIALOG_SYMBOL_LIB_MERGE( SYMBOL_EDIT_FRAME* aParent ) :
        DIALOG_SHIM( aParent, wxID_ANY, _( "Merge Symbol Libraries" ), wxDefaultPosition, wxDefaultSize,
                     wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER ),
        m_frame( aParent ),
        m_currentPage( 0 )
{
    SetSizeHints( wxDefaultSize, wxDefaultSize );

    wxBoxSizer* topSizer = new wxBoxSizer( wxVERTICAL );
    m_book = new wxSimplebook( this, wxID_ANY );

    // Page 1: pick the source libraries (multiple selection)
    wxPanel*    sourcePage = new wxPanel( m_book, wxID_ANY );
    wxBoxSizer* sourceSizer = new wxBoxSizer( wxVERTICAL );

    wxStaticText* sourceLabel = new wxStaticText( sourcePage, wxID_ANY,
                                                  _( "Select the libraries to merge (multiple selection).\n"
                                                     "Only user-created libraries are listed; built-in KiCad libraries "
                                                     "cannot be merged:" ) );
    sourceSizer->Add( sourceLabel, 0, wxEXPAND | wxBOTTOM, 5 );

    m_sourceList = new wxCheckListBox( sourcePage, wxID_ANY, wxDefaultPosition, wxSize( 420, 260 ) );

    sourceSizer->Add( m_sourceList, 1, wxEXPAND, 5 );
    sourcePage->SetSizer( sourceSizer );
    m_book->AddPage( sourcePage, wxEmptyString, true );

    // Page 2: pick the target library (single selection)
    wxPanel*    targetPage = new wxPanel( m_book, wxID_ANY );
    wxBoxSizer* targetSizer = new wxBoxSizer( wxVERTICAL );

    wxStaticText* targetLabel =
            new wxStaticText( targetPage, wxID_ANY, _( "Select the target library (a single choice):" ) );
    targetSizer->Add( targetLabel, 0, wxEXPAND | wxBOTTOM, 5 );

    m_targetList =
            new wxListBox( targetPage, wxID_ANY, wxDefaultPosition, wxSize( 420, 260 ), 0, nullptr, wxLB_SINGLE );
    targetSizer->Add( m_targetList, 1, wxEXPAND, 5 );
    targetPage->SetSizer( targetSizer );
    m_book->AddPage( targetPage, wxEmptyString, false );

    // Page 3: summary and final warning
    wxPanel*    warningPage = new wxPanel( m_book, wxID_ANY );
    wxBoxSizer* warningSizer = new wxBoxSizer( wxVERTICAL );

    wxStaticText* warningLabel = new wxStaticText( warningPage, wxID_ANY, _( "Warning:" ) );
    warningLabel->SetFont( warningLabel->GetFont().Bold() );
    warningSizer->Add( warningLabel, 0, wxEXPAND | wxBOTTOM, 5 );

    wxStaticText* warningText =
            new wxStaticText( warningPage, wxID_ANY,
                              _( "Merging deletes the original libraries (including their files) and moves all "
                                 "of their symbols into the target library. This action cannot be undone." ) );
    warningText->Wrap( 420 );
    warningSizer->Add( warningText, 0, wxEXPAND | wxBOTTOM, 10 );

    m_summary = new wxTextCtrl( warningPage, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( 420, 180 ),
                                wxTE_MULTILINE | wxTE_READONLY );
    warningSizer->Add( m_summary, 1, wxEXPAND, 5 );
    warningPage->SetSizer( warningSizer );
    m_book->AddPage( warningPage, wxEmptyString, false );

    topSizer->Add( m_book, 1, wxEXPAND | wxALL, 10 );

    wxStaticLine* separator = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    topSizer->Add( separator, 0, wxEXPAND | wxBOTTOM, 6 );

    wxBoxSizer* buttonSizer = new wxBoxSizer( wxHORIZONTAL );
    m_backBtn = new wxButton( this, wxID_ANY, _( "< Back" ) );
    m_nextBtn = new wxButton( this, wxID_ANY, _( "Next >" ) );
    wxButton* cancelBtn = new wxButton( this, wxID_CANCEL, _( "Cancel" ) );

    buttonSizer->Add( m_backBtn, 0, wxALL, 5 );
    buttonSizer->AddStretchSpacer();
    buttonSizer->Add( cancelBtn, 0, wxALL, 5 );
    buttonSizer->Add( m_nextBtn, 0, wxALL, 5 );

    topSizer->Add( buttonSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6 );

    SetSizer( topSizer );
    Layout();
    topSizer->Fit( this );

    Centre();

    m_backBtn->Bind( wxEVT_BUTTON, &DIALOG_SYMBOL_LIB_MERGE::onBack, this );
    m_nextBtn->Bind( wxEVT_BUTTON, &DIALOG_SYMBOL_LIB_MERGE::onNext, this );
    cancelBtn->Bind( wxEVT_BUTTON, &DIALOG_SYMBOL_LIB_MERGE::onCancel, this );

    // Keep the navigation buttons in sync with the list selections
    m_sourceList->Bind( wxEVT_CHECKLISTBOX,
                        [this]( wxCommandEvent& )
                        {
                            updateNavButtons();
                        } );

    m_targetList->Bind( wxEVT_LISTBOX,
                        [this]( wxCommandEvent& )
                        {
                            updateNavButtons();
                        } );

    // Populate the source library list
    SYMBOL_LIBRARY_ADAPTER* adapter = PROJECT_SCH::SymbolLibAdapter( &m_frame->Prj() );
    adapter->BlockUntilLoaded();

    for( const wxString& libName : adapter->GetLibraryNames() )
    {
        // Only user-created, writable libraries can be merged
        if( isMergeableLibrary( libName ) )
            m_sourceList->Append( libName );
    }

    updateNavButtons();

    finishDialogSettings();
}


void DIALOG_SYMBOL_LIB_MERGE::onBack( wxCommandEvent& aEvent )
{
    if( m_currentPage > 0 )
    {
        m_currentPage--;
        m_book->SetSelection( m_currentPage );
        updateNavButtons();
    }
}


void DIALOG_SYMBOL_LIB_MERGE::onNext( wxCommandEvent& aEvent )
{
    if( m_currentPage == 0 )
    {
        if( selectedSourceLibraries().empty() )
            return;

        m_currentPage = 1;
        buildTargetLibraryList();
        m_book->SetSelection( m_currentPage );
        updateNavButtons();
    }
    else if( m_currentPage == 1 )
    {
        if( selectedTargetLibrary().IsEmpty() )
            return;

        computeConflicts();
        m_conflictChoices.clear();

        // Let the user resolve name conflicts before showing the summary page
        if( !m_conflicts.empty() )
        {
            DIALOG_SYMBOL_MERGE_CONFLICTS dlg( this, m_frame, selectedTargetLibrary(), m_conflicts );

            if( dlg.ShowQuasiModal() != wxID_OK )
                return;

            m_conflictChoices = dlg.GetChoices();
        }

        m_currentPage = 2;
        buildSummary();
        m_book->SetSelection( m_currentPage );
        updateNavButtons();
    }
    else
    {
        if( performMerge() )
            EndQuasiModal( wxID_OK );
        else
            EndQuasiModal( wxID_CANCEL );
    }
}


void DIALOG_SYMBOL_LIB_MERGE::onCancel( wxCommandEvent& aEvent )
{
    EndQuasiModal( wxID_CANCEL );
}


void DIALOG_SYMBOL_LIB_MERGE::updateNavButtons()
{
    m_backBtn->Enable( m_currentPage > 0 );

    if( m_currentPage == 0 )
    {
        m_nextBtn->SetLabel( _( "Next >" ) );
        m_nextBtn->Enable( !selectedSourceLibraries().empty() );
    }
    else if( m_currentPage == 1 )
    {
        m_nextBtn->SetLabel( _( "Next >" ) );
        m_nextBtn->Enable( !selectedTargetLibrary().IsEmpty() );
    }
    else
    {
        m_nextBtn->SetLabel( _( "Merge" ) );
        m_nextBtn->Enable( true );
    }
}


std::vector<wxString> DIALOG_SYMBOL_LIB_MERGE::selectedSourceLibraries() const
{
    std::vector<wxString> sources;

    for( unsigned int ii = 0; ii < m_sourceList->GetCount(); ++ii )
    {
        if( m_sourceList->IsChecked( ii ) )
            sources.emplace_back( m_sourceList->GetString( ii ) );
    }

    return sources;
}


wxString DIALOG_SYMBOL_LIB_MERGE::selectedTargetLibrary() const
{
    int selection = m_targetList->GetSelection();

    if( selection == wxNOT_FOUND )
        return wxEmptyString;

    return m_targetList->GetString( selection );
}


void DIALOG_SYMBOL_LIB_MERGE::buildTargetLibraryList()
{
    std::vector<wxString>   sources = selectedSourceLibraries();
    SYMBOL_LIBRARY_ADAPTER* adapter = PROJECT_SCH::SymbolLibAdapter( &m_frame->Prj() );

    m_targetList->Clear();

    for( const wxString& libName : adapter->GetLibraryNames() )
    {
        // The target library cannot be one of the source libraries
        if( std::find( sources.begin(), sources.end(), libName ) != sources.end() )
            continue;

        // The target library has to accept new symbols
        if( !isMergeableLibrary( libName ) )
            continue;

        m_targetList->Append( libName );
    }
}


void DIALOG_SYMBOL_LIB_MERGE::buildSummary()
{
    SYMBOL_LIBRARY_ADAPTER* adapter = PROJECT_SCH::SymbolLibAdapter( &m_frame->Prj() );
    std::vector<wxString>   sources = selectedSourceLibraries();
    wxString                target = selectedTargetLibrary();

    wxString text = wxString::Format( _( "The following libraries will be merged into '%s':" ), target ) + "\n";

    for( const wxString& source : sources )
    {
        std::vector<wxString> names = adapter->GetSymbolNames( source );
        text += wxString::Format( "  - %s (%d %s)\n", source, (int) names.size(), _( "symbols" ) );
    }

    if( !m_conflicts.empty() )
    {
        text += "\n" + wxString::Format( _( "Conflicting symbol names (%d):" ), (int) m_conflicts.size() );

        for( const SYMBOL_MERGE_CONFLICT& conflict : m_conflicts )
        {
            const wxString& kept = m_conflictChoices[conflict.symbolName];

            if( kept == target )
            {
                text += wxString::Format( wxT( "\n  %s: " ), conflict.symbolName )
                        + _( "keep the target library version (source skipped)" );
            }
            else
            {
                text += wxString::Format( wxT( "\n  %s: " ), conflict.symbolName )
                        + wxString::Format( _( "keep the version from '%s'" ), kept );
            }
        }
    }

    text += "\n\n"
            + _( "The source libraries above will be deleted from the symbol library "
                 "table and their files removed from disk." );

    m_summary->SetValue( text );
}


void DIALOG_SYMBOL_LIB_MERGE::computeConflicts()
{
    SYMBOL_LIBRARY_ADAPTER* adapter = PROJECT_SCH::SymbolLibAdapter( &m_frame->Prj() );
    adapter->BlockUntilLoaded();

    m_conflicts.clear();

    std::vector<wxString> sources = selectedSourceLibraries();
    wxString              target = selectedTargetLibrary();

    std::map<wxString, std::vector<wxString>> providers;

    for( const wxString& source : sources )
    {
        for( const wxString& name : adapter->GetSymbolNames( source ) )
        {
            std::vector<wxString>& libs = providers[name];

            if( std::find( libs.begin(), libs.end(), source ) == libs.end() )
                libs.push_back( source );
        }
    }

    std::vector<wxString> targetSymbolNames = adapter->GetSymbolNames( target );
    std::set<wxString>    targetNames( targetSymbolNames.begin(), targetSymbolNames.end() );

    for( auto& [name, libs] : providers )
    {
        if( targetNames.count( name ) && std::find( libs.begin(), libs.end(), target ) == libs.end() )
            libs.push_back( target );

        if( libs.size() > 1 )
            m_conflicts.push_back( SYMBOL_MERGE_CONFLICT( { name, libs } ) );
    }
}


bool DIALOG_SYMBOL_LIB_MERGE::isMergeableLibrary( const wxString& aNickname )
{
    SYMBOL_LIBRARY_ADAPTER* adapter = PROJECT_SCH::SymbolLibAdapter( &m_frame->Prj() );

    if( !adapter->GetRow( aNickname ).value_or( nullptr ) )
        return false;

    // Only writable libraries can be merged: the bundled KiCad libraries are
    // flagged read-only, which keeps them out of both lists
    try
    {
        return adapter->IsSymbolLibWritable( aNickname );
    }
    catch( const IO_ERROR& )
    {
        return false;
    }
}


bool DIALOG_SYMBOL_LIB_MERGE::performMerge()
{
    SYMBOL_LIBRARY_ADAPTER* adapter = PROJECT_SCH::SymbolLibAdapter( &m_frame->Prj() );
    adapter->BlockUntilLoaded();

    std::vector<wxString> sources = selectedSourceLibraries();
    wxString              target = selectedTargetLibrary();
    int                   mergedCount = 0;
    std::vector<wxString> skipped;
    std::vector<wxString> failed;

    // Names already present in the target library; symbols with these names are skipped
    // rather than overwritten
    std::vector<wxString> targetSymbolNames = adapter->GetSymbolNames( target );
    std::set<wxString>    existingNames( targetSymbolNames.begin(), targetSymbolNames.end() );

    for( const wxString& source : sources )
    {
        for( const wxString& name : adapter->GetSymbolNames( source ) )
        {
            auto decision = m_conflictChoices.find( name );

            if( decision != m_conflictChoices.end() )
            {
                // Conflicting name: only the version from the library chosen in the
                // conflict dialog is copied, overwriting the target library copy
                if( decision->second == source )
                {
                    LIB_SYMBOL* symbol = adapter->LoadSymbol( source, name );

                    if( symbol )
                    {
                        // The adapter hands out borrowed pointers, so work on a copy
                        std::unique_ptr<LIB_SYMBOL> symbolCopy = std::make_unique<LIB_SYMBOL>( *symbol );

                        LIB_ID id = symbolCopy->GetLibId();
                        id.SetLibNickname( target );
                        symbolCopy->SetLibId( id );

                        if( adapter->SaveSymbol( target, std::move( symbolCopy ), true )
                            == SYMBOL_LIBRARY_ADAPTER::SAVE_OK )
                        {
                            mergedCount++;
                        }
                        else
                        {
                            failed.emplace_back( wxString::Format( wxT( "%s (%s)" ), name, source ) );
                        }
                    }
                    else
                    {
                        failed.emplace_back( wxString::Format( wxT( "%s (%s)" ), name, source ) );
                    }
                }

                continue;
            }

            if( existingNames.count( name ) )
            {
                skipped.emplace_back( wxString::Format( wxT( "%s (%s)" ), name, source ) );
                continue;
            }

            LIB_SYMBOL* symbol = adapter->LoadSymbol( source, name );

            if( !symbol )
            {
                failed.emplace_back( wxString::Format( wxT( "%s (%s)" ), name, source ) );
                continue;
            }

            // The adapter hands out borrowed pointers, so work on a copy
            std::unique_ptr<LIB_SYMBOL> symbolCopy = std::make_unique<LIB_SYMBOL>( *symbol );

            LIB_ID id = symbolCopy->GetLibId();
            id.SetLibNickname( target );
            symbolCopy->SetLibId( id );

            if( adapter->SaveSymbol( target, std::move( symbolCopy ), true ) == SYMBOL_LIBRARY_ADAPTER::SAVE_OK )
            {
                existingNames.insert( name );
                mergedCount++;
            }
            else
            {
                failed.emplace_back( wxString::Format( wxT( "%s (%s)" ), name, source ) );
            }
        }
    }

    // Remove the source libraries from the library tables and delete their files
    LIBRARY_MANAGER&      manager = Pgm().GetLibraryManager();
    std::set<wxString>    sourceNames( sources.begin(), sources.end() );
    std::vector<wxString> deletedFiles;
    std::vector<wxString> failedFiles;
    bool                  globalTableChanged = false;
    bool                  projectTableChanged = false;

    for( LIBRARY_TABLE_SCOPE scope : { LIBRARY_TABLE_SCOPE::GLOBAL, LIBRARY_TABLE_SCOPE::PROJECT } )
    {
        std::optional<LIBRARY_TABLE*> optTable = manager.Table( LIBRARY_TABLE_TYPE::SYMBOL, scope );

        if( !optTable.has_value() || !( *optTable ) )
            continue;

        LIBRARY_TABLE* table = *optTable;
        bool           modified = false;

        for( auto it = table->Rows().begin(); it != table->Rows().end(); )
        {
            if( sourceNames.count( it->Nickname() ) )
            {
                wxString uri = ExpandEnvVarSubstitutions( it->URI(), &m_frame->Prj() );
                removeLibraryFile( uri, deletedFiles, failedFiles );
                it = table->Rows().erase( it );
                modified = true;
            }
            else
            {
                ++it;
            }
        }

        if( modified )
        {
            bool saveOk = true;

            table->Save().map_error(
                    [&saveOk]( const LIBRARY_ERROR& aError )
                    {
                        KICAD_MESSAGE_DIALOG dlg( nullptr, _( "Error saving library table." ), _( "File Save Error" ),
                                                  wxOK | wxICON_ERROR );
                        dlg.SetExtendedMessage( aError.message );
                        dlg.ShowModal();
                        saveOk = false;
                    } );

            if( !saveOk )
                return false;

            if( scope == LIBRARY_TABLE_SCOPE::GLOBAL )
                globalTableChanged = true;
            else
                projectTableChanged = true;
        }
    }

    // Reload the library data to pick up the table changes
    if( globalTableChanged )
        manager.LoadGlobalTables( { LIBRARY_TABLE_TYPE::SYMBOL } );

    if( projectTableChanged )
        manager.ProjectChanged();

    if( KIFACE* schface = m_frame->Kiway().KiFACE( KIWAY::FACE_SCH ) )
        schface->PreloadLibraries( &m_frame->Kiway() );

    m_frame->SyncLibraries( false );

    // Report the result
    wxString msg = wxString::Format( _( "Merged %d symbol(s) into '%s'." ), mergedCount, target );

    if( !skipped.empty() )
    {
        msg += "\n\n"
               + wxString::Format( _( "Skipped %d symbol(s) that already exist in the "
                                      "target library:" ),
                                   (int) skipped.size() );

        for( const wxString& name : skipped )
            msg += "\n  " + name;
    }

    if( !failed.empty() )
    {
        msg += "\n\n" + wxString::Format( _( "Failed to copy %d symbol(s):" ), (int) failed.size() );

        for( const wxString& name : failed )
            msg += "\n  " + name;
    }

    if( !failedFiles.empty() )
    {
        msg += "\n\n"
               + _( "Could not delete the following library files (the libraries were "
                    "still removed from the library table):" );

        for( const wxString& file : failedFiles )
            msg += "\n  " + file;
    }

    KICAD_MESSAGE_DIALOG dlg( this, msg, _( "Merge Symbol Libraries" ), wxOK | wxICON_INFORMATION );
    dlg.ShowModal();

    return true;
}


void DIALOG_SYMBOL_LIB_MERGE::removeLibraryFile( const wxString& aUri, std::vector<wxString>& aDeleted,
                                                 std::vector<wxString>& aFailed )
{
    wxString path = aUri;

    if( path.StartsWith( wxT( "file://" ) ) )
        path = path.Mid( 7 );

    wxFileName fn( path );

    // Non local libraries (http, database, ...) have no file to delete
    if( !fn.FileExists() )
        return;

    if( wxRemoveFile( fn.GetFullPath() ) )
        aDeleted.emplace_back( fn.GetFullPath() );
    else
        aFailed.emplace_back( fn.GetFullPath() );
}
