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

#include "dialog_plm_conflict.h"
#include <wx/choice.h>
#include <wx/msgdlg.h>


DIALOG_PLM_CONFLICT::DIALOG_PLM_CONFLICT( wxWindow* parent ) :
        DIALOG_PLM_CONFLICT_BASE( parent )
{
    initConflictList();

    m_sdbSizerOK->SetLabel( _( "Accept Changes" ) );

    SetupStandardButtons();

    finishDialogSettings();
}


DIALOG_PLM_CONFLICT::~DIALOG_PLM_CONFLICT()
{
}


void DIALOG_PLM_CONFLICT::initConflictList()
{
    m_conflictList->AppendTextColumn( _( "Item" ), wxDATAVIEW_CELL_INERT, 200,
                                      wxALIGN_LEFT,
                                      wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE );

    m_conflictList->AppendTextColumn( _( "Local State" ), wxDATAVIEW_CELL_INERT, 200,
                                      wxALIGN_LEFT,
                                      wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE );

    m_conflictList->AppendTextColumn( _( "Server State" ), wxDATAVIEW_CELL_INERT, 200,
                                      wxALIGN_LEFT,
                                      wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE );

    wxArrayString resolutionChoices;
    resolutionChoices.Add( _( "Choose..." ) );
    resolutionChoices.Add( _( "Use Local" ) );
    resolutionChoices.Add( _( "Use Server" ) );

    m_conflictList->AppendTextColumn( _( "Resolution" ), wxDATAVIEW_CELL_EDITABLE, 150,
                                      wxALIGN_LEFT,
                                      wxDATAVIEW_COL_RESIZABLE );
}


void DIALOG_PLM_CONFLICT::AddConflict( const wxString& aItemName, const wxString& aLocalState,
                                       const wxString& aServerState, const wxString& aItemPath )
{
    PLM_CONFLICT_ITEM item( aItemName, aLocalState, aServerState, aItemPath );
    m_conflicts.push_back( item );

    wxVector<wxVariant> data;
    data.push_back( wxVariant( aItemName ) );
    data.push_back( wxVariant( aLocalState ) );
    data.push_back( wxVariant( aServerState ) );
    data.push_back( wxVariant( _( "Choose..." ) ) );

    m_conflictList->AppendItem( data );
}


std::vector<PLM_CONFLICT_ITEM> DIALOG_PLM_CONFLICT::GetResolutions() const
{
    return m_conflicts;
}


void DIALOG_PLM_CONFLICT::AddComponentChanges( int aConflictIndex,
                                                const std::vector<PLM_CONFLICT_COMPONENT_CHANGE>& aChanges )
{
    if( aConflictIndex < 0 || aConflictIndex >= static_cast<int>( m_conflicts.size() ) )
        return;

    m_conflicts[aConflictIndex].component_changes = aChanges;
}


void DIALOG_PLM_CONFLICT::AddComponentChange( int aConflictIndex, const PLM_CONFLICT_COMPONENT_CHANGE& aChange )
{
    if( aConflictIndex < 0 || aConflictIndex >= static_cast<int>( m_conflicts.size() ) )
        return;

    m_conflicts[aConflictIndex].component_changes.push_back( aChange );
}


std::vector<PLM_CONFLICT_COMPONENT_CHANGE> DIALOG_PLM_CONFLICT::GetMergeResult( int aConflictIndex ) const
{
    std::vector<PLM_CONFLICT_COMPONENT_CHANGE> result;

    if( aConflictIndex < 0 || aConflictIndex >= static_cast<int>( m_conflicts.size() ) )
        return result;

    const PLM_CONFLICT_ITEM& item = m_conflicts[aConflictIndex];

    for( const auto& change : item.component_changes )
    {
        if( change.action == PLM_CONFLICT_MERGE_ACTION::INCLUDE )
            result.push_back( change );
    }

    return result;
}


void DIALOG_PLM_CONFLICT::SetComponentMergeAction( int aConflictIndex, int aChangeIndex, bool aInclude )
{
    if( aConflictIndex < 0 || aConflictIndex >= static_cast<int>( m_conflicts.size() ) )
        return;

    auto& changes = m_conflicts[aConflictIndex].component_changes;

    if( aChangeIndex < 0 || aChangeIndex >= static_cast<int>( changes.size() ) )
        return;

    changes[aChangeIndex].action = aInclude ? PLM_CONFLICT_MERGE_ACTION::INCLUDE : PLM_CONFLICT_MERGE_ACTION::EXCLUDE;

    int row = m_conflictList->GetSelectedRow();

    if( row == aConflictIndex )
        updateMergePreview( aConflictIndex );
}


void DIALOG_PLM_CONFLICT::EnableMergeMode( int aConflictIndex )
{
    if( aConflictIndex < 0 || aConflictIndex >= static_cast<int>( m_conflicts.size() ) )
        return;

    m_conflicts[aConflictIndex].resolution = PLM_CONFLICT_RESOLUTION::MERGE;

    m_conflictList->SetValue( wxVariant( _( "Merge" ) ), aConflictIndex, COL_RESOLUTION );

    for( auto& change : m_conflicts[aConflictIndex].component_changes )
    {
        if( change.action == PLM_CONFLICT_MERGE_ACTION::UNDECIDED )
            change.action = PLM_CONFLICT_MERGE_ACTION::INCLUDE;
    }
}


bool DIALOG_PLM_CONFLICT::AllConflictsResolved() const
{
    for( const auto& conflict : m_conflicts )
    {
        if( conflict.resolution == PLM_CONFLICT_RESOLUTION::UNRESOLVED )
            return false;
    }

    return true;
}


void DIALOG_PLM_CONFLICT::onConflictSelected( wxDataViewEvent& event )
{
    int row = m_conflictList->GetSelectedRow();

    if( row != wxNOT_FOUND && row >= 0 && row < static_cast<int>( m_conflicts.size() ) )
    {
        updatePreview( row );
    }
}


void DIALOG_PLM_CONFLICT::updatePreview( int aIndex )
{
    if( aIndex < 0 || aIndex >= static_cast<int>( m_conflicts.size() ) )
        return;

    const PLM_CONFLICT_ITEM& item = m_conflicts[aIndex];

    if( item.resolution == PLM_CONFLICT_RESOLUTION::MERGE && !item.component_changes.empty() )
    {
        updateMergePreview( aIndex );
        return;
    }

    wxString previewText = wxString::Format(
            _( "Item: %s\n\n"
               "Local Changes:\n%s\n\n"
               "Server Changes:\n%s\n\n"
               "Diff visualization will be implemented in a future update." ),
            item.item_name,
            item.local_state,
            item.server_state );

    m_previewPlaceholder->SetLabel( previewText );
    m_previewContent->Layout();
}


void DIALOG_PLM_CONFLICT::updateMergePreview( int aIndex )
{
    if( aIndex < 0 || aIndex >= static_cast<int>( m_conflicts.size() ) )
        return;

    const PLM_CONFLICT_ITEM& item = m_conflicts[aIndex];

    wxString previewText = wxString::Format( _( "Item: %s\n\n" ), item.item_name );

    previewText += _( "Component Changes:\n" );
    previewText += formatComponentChanges( item.component_changes, true );

    previewText += _( "\n\nMerge Preview (included changes):\n" );

    std::vector<PLM_CONFLICT_COMPONENT_CHANGE> included;

    for( const auto& change : item.component_changes )
    {
        if( change.action == PLM_CONFLICT_MERGE_ACTION::INCLUDE )
            included.push_back( change );
    }

    if( included.empty() )
    {
        previewText += _( "  (No changes selected)\n" );
    }
    else
    {
        previewText += formatComponentChanges( included, false );
    }

    m_previewPlaceholder->SetLabel( previewText );
    m_previewContent->Layout();
}


wxString DIALOG_PLM_CONFLICT::formatComponentChanges( const std::vector<PLM_CONFLICT_COMPONENT_CHANGE>& aChanges,
                                                       bool aShowSelection ) const
{
    wxString result;

    for( const auto& change : aChanges )
    {
        wxString statusPrefix;

        if( aShowSelection )
        {
            switch( change.action )
            {
            case PLM_CONFLICT_MERGE_ACTION::INCLUDE:   statusPrefix = _( "[+] " ); break;
            case PLM_CONFLICT_MERGE_ACTION::EXCLUDE:   statusPrefix = _( "[-] " ); break;
            case PLM_CONFLICT_MERGE_ACTION::UNDECIDED: statusPrefix = _( "[?] " ); break;
            }
        }

        result += wxString::Format( _( "  %s%s (%s)\n" ),
                                    statusPrefix,
                                    change.component_name,
                                    change.change_type );

        if( !change.local_value.IsEmpty() || !change.server_value.IsEmpty() )
        {
            if( !change.local_value.IsEmpty() )
                result += wxString::Format( _( "    Local: %s\n" ), change.local_value );

            if( !change.server_value.IsEmpty() )
                result += wxString::Format( _( "    Server: %s\n" ), change.server_value );
        }
    }

    return result;
}


bool DIALOG_PLM_CONFLICT::TransferDataFromWindow()
{
    for( size_t i = 0; i < m_conflicts.size(); i++ )
    {
        wxVariant value;
        m_conflictList->GetValue( value, i, COL_RESOLUTION );
        wxString resolutionStr = value.GetString();

        if( resolutionStr == _( "Use Local" ) )
        {
            m_conflicts[i].resolution = PLM_CONFLICT_RESOLUTION::USE_LOCAL;
        }
        else if( resolutionStr == _( "Use Server" ) )
        {
            m_conflicts[i].resolution = PLM_CONFLICT_RESOLUTION::USE_SERVER;
        }
        else if( resolutionStr == _( "Merge" ) )
        {
            m_conflicts[i].resolution = PLM_CONFLICT_RESOLUTION::MERGE;
        }
        else
        {
            m_conflicts[i].resolution = PLM_CONFLICT_RESOLUTION::UNRESOLVED;
        }
    }

    if( !AllConflictsResolved() )
    {
        wxMessageBox( _( "Please resolve all conflicts before continuing." ),
                      _( "Unresolved Conflicts" ),
                      wxOK | wxICON_WARNING,
                      this );
        return false;
    }

    return true;
}
