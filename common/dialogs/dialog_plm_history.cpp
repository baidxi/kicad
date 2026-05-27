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

#include "dialog_plm_history.h"

#include <plm/plm_plugin.h>

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/dataview.h>
#include <wx/datetime.h>
#include <wx/msgdlg.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>


DIALOG_PLM_HISTORY::DIALOG_PLM_HISTORY( wxWindow* aParent, PLM_PLUGIN* aPlugin ) :
        DIALOG_PLM_HISTORY_BASE( aParent ),
        m_plugin( aPlugin ),
        m_currentStateLabel( nullptr ),
        m_currentStateValue( nullptr ),
        m_transitionChoice( nullptr ),
        m_changeStateBtn( nullptr ),
        m_transitionCommentCtrl( nullptr ),
        m_releaseList( nullptr ),
        m_createReleaseBtn( nullptr )
{
    m_revisionList->AppendTextColumn( _( "Revision" ), wxDATAVIEW_CELL_INERT, 100,
                                       wxALIGN_LEFT,
                                       wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_revisionList->AppendTextColumn( _( "Date/Time" ), wxDATAVIEW_CELL_INERT, 150,
                                       wxALIGN_LEFT,
                                       wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_revisionList->AppendTextColumn( _( "Author" ), wxDATAVIEW_CELL_INERT, 150,
                                       wxALIGN_LEFT,
                                       wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_revisionList->AppendTextColumn( _( "Comment" ), wxDATAVIEW_CELL_INERT, 300,
                                       wxALIGN_LEFT,
                                       wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );

    createStateTransitionSection();
    createReleaseInfoSection();

    clearDetailsPanel();
    updateButtonStates();

    SetMinSize( wxSize( 800, 600 ) );
    Layout();
}


DIALOG_PLM_HISTORY::~DIALOG_PLM_HISTORY()
{
}


void DIALOG_PLM_HISTORY::createStateTransitionSection()
{
    wxBoxSizer* bMainSizer = dynamic_cast<wxBoxSizer*>( GetSizer() );

    if( !bMainSizer )
        return;

    // Insert before the button sizer (last item)
    int insertPos = static_cast<int>( bMainSizer->GetItemCount() ) - 1;

    if( insertPos < 0 )
        insertPos = 0;

    wxStaticBoxSizer* sbStateSizer = new wxStaticBoxSizer(
            new wxStaticBox( this, wxID_ANY, _( "Lifecycle State" ) ), wxVERTICAL );

    wxFlexGridSizer* fgStateSizer = new wxFlexGridSizer( 0, 2, 5, 0 );
    fgStateSizer->AddGrowableCol( 1 );
    fgStateSizer->SetFlexibleDirection( wxBOTH );
    fgStateSizer->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_currentStateLabel = new wxStaticText( sbStateSizer->GetStaticBox(), wxID_ANY,
                                             _( "Current State:" ) );
    m_currentStateLabel->Wrap( -1 );
    fgStateSizer->Add( m_currentStateLabel, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

    m_currentStateValue = new wxStaticText( sbStateSizer->GetStaticBox(), wxID_ANY,
                                             _( "Unknown" ) );
    m_currentStateValue->Wrap( -1 );
    wxFont boldFont = m_currentStateValue->GetFont();
    boldFont.SetWeight( wxFONTWEIGHT_BOLD );
    m_currentStateValue->SetFont( boldFont );
    fgStateSizer->Add( m_currentStateValue, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

    wxStaticText* transitionLabel = new wxStaticText( sbStateSizer->GetStaticBox(), wxID_ANY,
                                                       _( "Transition to:" ) );
    transitionLabel->Wrap( -1 );
    fgStateSizer->Add( transitionLabel, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

    wxBoxSizer* bTransitionSizer = new wxBoxSizer( wxHORIZONTAL );

    wxArrayString transitionChoices;
    m_transitionChoice = new wxChoice( sbStateSizer->GetStaticBox(), wxID_ANY, wxDefaultPosition,
                                        wxDefaultSize, transitionChoices, 0 );
    bTransitionSizer->Add( m_transitionChoice, 1, wxALL | wxEXPAND, 5 );

    m_changeStateBtn = new wxButton( sbStateSizer->GetStaticBox(), wxID_ANY,
                                      _( "Change State" ) );
    m_changeStateBtn->Enable( false );
    m_changeStateBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                                wxCommandEventHandler( DIALOG_PLM_HISTORY::onChangeState ),
                                NULL, this );
    bTransitionSizer->Add( m_changeStateBtn, 0, wxALL, 5 );

    fgStateSizer->Add( bTransitionSizer, 0, wxEXPAND, 5 );

    wxStaticText* commentLabel = new wxStaticText( sbStateSizer->GetStaticBox(), wxID_ANY,
                                                    _( "Comment:" ) );
    commentLabel->Wrap( -1 );
    fgStateSizer->Add( commentLabel, 0, wxALIGN_TOP | wxALL, 5 );

    m_transitionCommentCtrl = new wxTextCtrl( sbStateSizer->GetStaticBox(), wxID_ANY,
                                               wxEmptyString, wxDefaultPosition,
                                               wxSize( -1, 40 ), wxTE_MULTILINE );
    fgStateSizer->Add( m_transitionCommentCtrl, 0, wxALL | wxEXPAND, 5 );

    sbStateSizer->Add( fgStateSizer, 0, wxEXPAND, 5 );

    bMainSizer->Insert( insertPos, sbStateSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 5 );
}


void DIALOG_PLM_HISTORY::createReleaseInfoSection()
{
    wxBoxSizer* bMainSizer = dynamic_cast<wxBoxSizer*>( GetSizer() );

    if( !bMainSizer )
        return;

    // Insert before the button sizer (last item)
    int insertPos = static_cast<int>( bMainSizer->GetItemCount() ) - 1;

    if( insertPos < 0 )
        insertPos = 0;

    wxStaticBoxSizer* sbReleaseSizer = new wxStaticBoxSizer(
            new wxStaticBox( this, wxID_ANY, _( "Releases" ) ), wxVERTICAL );

    m_releaseList = new wxDataViewListCtrl( sbReleaseSizer->GetStaticBox(), wxID_ANY,
                                             wxDefaultPosition, wxSize( -1, 100 ),
                                             wxDV_ROW_LINES | wxDV_SINGLE );

    m_releaseList->AppendTextColumn( _( "Version" ), wxDATAVIEW_CELL_INERT, 100,
                                      wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE );
    m_releaseList->AppendTextColumn( _( "Title" ), wxDATAVIEW_CELL_INERT, 200,
                                      wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE );
    m_releaseList->AppendTextColumn( _( "Status" ), wxDATAVIEW_CELL_INERT, 100,
                                      wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE );
    m_releaseList->AppendTextColumn( _( "Released By" ), wxDATAVIEW_CELL_INERT, 120,
                                      wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE );

    sbReleaseSizer->Add( m_releaseList, 1, wxEXPAND | wxALL, 5 );

    m_createReleaseBtn = new wxButton( sbReleaseSizer->GetStaticBox(), wxID_ANY,
                                        _( "Create Release..." ) );
    m_createReleaseBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                                  wxCommandEventHandler( DIALOG_PLM_HISTORY::onCreateRelease ),
                                  NULL, this );
    sbReleaseSizer->Add( m_createReleaseBtn, 0, wxALL, 5 );

    bMainSizer->Insert( insertPos, sbReleaseSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 5 );
}


bool DIALOG_PLM_HISTORY::LoadHistory( const wxString& aProjectId )
{
    if( !m_plugin )
        return false;

    m_projectId = aProjectId;

    try
    {
        m_history = m_plugin->GetRevisionHistory( aProjectId );
    }
    catch( const std::exception& e )
    {
        wxMessageBox( wxString::Format( _( "Failed to load revision history:\n%s" ), e.what() ),
                      _( "Error" ), wxOK | wxICON_ERROR, this );
        return false;
    }

    if( m_history.IsEmpty() )
    {
        wxMessageBox( _( "No revision history available for this project." ), _( "Information" ),
                      wxOK | wxICON_INFORMATION, this );
        return false;
    }

    populateRevisionList();
    loadStateInfo();
    loadValidTransitions();
    loadReleases();
    return true;
}


std::vector<wxString> DIALOG_PLM_HISTORY::GetSelectedRevisions() const
{
    std::vector<wxString> selected;
    wxDataViewItemArray   selections;

    int count = m_revisionList->GetSelections( selections );

    for( int i = 0; i < count && i < 2; ++i )
    {
        int row = m_revisionList->ItemToRow( selections[i] );

        if( row >= 0 && static_cast<size_t>( row ) < m_history.revisions.size() )
            selected.push_back( m_history.revisions[row].revision_id );
    }

    return selected;
}


void DIALOG_PLM_HISTORY::populateRevisionList()
{
    m_revisionList->DeleteAllItems();

    for( const PLM_REVISION& revision : m_history.revisions )
    {
        wxVector<wxVariant> data;

        data.push_back( wxVariant( revision.revision_number ) );
        data.push_back( wxVariant( formatTimestamp( revision.timestamp ) ) );
        data.push_back( wxVariant( revision.author ) );
        data.push_back( wxVariant( revision.comment ) );

        m_revisionList->AppendItem( data );
    }
}


void DIALOG_PLM_HISTORY::updateDetailsPanel( const PLM_REVISION& aRevision )
{
    m_revisionIdValue->SetLabel( aRevision.revision_number );
    m_authorValue->SetLabel( aRevision.author );
    m_dateValue->SetLabel( formatTimestamp( aRevision.timestamp ) );
    m_commentText->SetValue( aRevision.comment );

    m_detailsPanel->Layout();
}


void DIALOG_PLM_HISTORY::clearDetailsPanel()
{
    m_revisionIdValue->SetLabel( wxEmptyString );
    m_authorValue->SetLabel( wxEmptyString );
    m_dateValue->SetLabel( wxEmptyString );
    m_commentText->SetValue( wxEmptyString );

    m_detailsPanel->Layout();
}


void DIALOG_PLM_HISTORY::updateButtonStates()
{
    wxDataViewItemArray selections;
    int                 count = m_revisionList->GetSelections( selections );

    m_compareBtn->Enable( count == 2 );
    m_downloadBtn->Enable( count == 1 );

    if( m_changeStateBtn )
        m_changeStateBtn->Enable( m_transitionChoice && m_transitionChoice->GetCount() > 0 );
}


wxString DIALOG_PLM_HISTORY::formatTimestamp( int64_t aTimestamp ) const
{
    if( aTimestamp == 0 )
        return wxEmptyString;

    wxDateTime dt( static_cast<time_t>( aTimestamp ) );

    if( !dt.IsValid() )
        return wxEmptyString;

    return dt.Format( wxS( "%Y-%m-%d %H:%M:%S" ) );
}


void DIALOG_PLM_HISTORY::loadStateInfo()
{
    if( !m_plugin || !m_currentStateValue )
        return;

    PLM_STATE_INFO stateInfo;

    if( m_plugin->GetCurrentState( m_projectId, stateInfo ) )
    {
        m_currentStateValue->SetLabel( stateInfo.display_name );

        switch( stateInfo.category )
        {
        case PLM_STATE_CATEGORY::EDITABLE:
            m_currentStateValue->SetForegroundColour( wxColour( 0, 128, 0 ) );
            break;
        case PLM_STATE_CATEGORY::LOCKED:
            m_currentStateValue->SetForegroundColour( wxColour( 200, 150, 0 ) );
            break;
        case PLM_STATE_CATEGORY::RELEASED:
            m_currentStateValue->SetForegroundColour( wxColour( 0, 0, 200 ) );
            break;
        case PLM_STATE_CATEGORY::PENDING_REVIEW:
            m_currentStateValue->SetForegroundColour( wxColour( 128, 128, 128 ) );
            break;
        default:
            m_currentStateValue->SetForegroundColour( *wxBLACK );
            break;
        }
    }
    else
    {
        m_currentStateValue->SetLabel( _( "Unknown" ) );
        m_currentStateValue->SetForegroundColour( *wxBLACK );
    }
}


void DIALOG_PLM_HISTORY::loadValidTransitions()
{
    if( !m_plugin || !m_transitionChoice )
        return;

    m_transitionChoice->Clear();
    m_validTransitions = m_plugin->GetValidTransitions( m_projectId );

    for( const PLM_VALID_TRANSITION& transition : m_validTransitions )
    {
        m_transitionChoice->Append( transition.display_name );
    }

    if( !m_validTransitions.empty() )
        m_transitionChoice->SetSelection( 0 );

    updateButtonStates();
}


void DIALOG_PLM_HISTORY::loadReleases()
{
    if( !m_plugin || !m_releaseList )
        return;

    m_releaseList->DeleteAllItems();
    m_releases = m_plugin->ListReleases( m_projectId );

    for( const PLM_RELEASE& release : m_releases )
    {
        wxVector<wxVariant> data;

        data.push_back( wxVariant( release.version ) );
        data.push_back( wxVariant( release.title ) );
        data.push_back( wxVariant( release.GetStatusName() ) );
        data.push_back( wxVariant( release.released_by ) );

        m_releaseList->AppendItem( data );
    }
}


void DIALOG_PLM_HISTORY::onChangeState( wxCommandEvent& event )
{
    if( !m_plugin || !m_transitionChoice )
        return;

    int selection = m_transitionChoice->GetSelection();

    if( selection == wxNOT_FOUND || static_cast<size_t>( selection ) >= m_validTransitions.size() )
        return;

    const PLM_VALID_TRANSITION& transition = m_validTransitions[selection];

    if( !transition.CanTransition() )
    {
        wxString blockerMsg = _( "Cannot transition due to unsatisfied preconditions:\n" );

        for( const PLM_TRANSITION_PRECONDITION& pre : transition.GetBlockers() )
        {
            blockerMsg += wxString::Format( wxS( "  - %s\n" ), pre.description );
        }

        wxMessageBox( blockerMsg, _( "Transition Blocked" ), wxOK | wxICON_WARNING, this );
        return;
    }

    wxString comment = m_transitionCommentCtrl ? m_transitionCommentCtrl->GetValue()
                                               : wxString( wxEmptyString );

    if( transition.requires_comment && comment.IsEmpty() )
    {
        wxMessageBox( _( "A comment is required for this state transition." ),
                      _( "Comment Required" ), wxOK | wxICON_WARNING, this );
        return;
    }

    PLM_STATE_TRANSITION_RESULT result = m_plugin->ChangeState( m_projectId,
                                                                  transition.target_state,
                                                                  comment );

    if( result.success )
    {
        if( !result.pending_approval_id.IsEmpty() )
        {
            wxMessageBox( _( "State change submitted for approval." ),
                          _( "Pending Approval" ), wxOK | wxICON_INFORMATION, this );
        }

        loadStateInfo();
        loadValidTransitions();

        if( m_transitionCommentCtrl )
            m_transitionCommentCtrl->SetValue( wxEmptyString );
    }
    else
    {
        wxMessageBox( wxString::Format( _( "State change failed:\n%s" ), result.error_message ),
                      _( "Error" ), wxOK | wxICON_ERROR, this );
    }
}


void DIALOG_PLM_HISTORY::onCreateRelease( wxCommandEvent& event )
{
    wxMessageBox( _( "Release wizard would be opened here." ),
                  _( "Not Implemented" ), wxOK | wxICON_INFORMATION, this );
}


void DIALOG_PLM_HISTORY::onRevisionSelected( wxDataViewEvent& event )
{
    int row = m_revisionList->GetSelectedRow();

    if( row >= 0 && static_cast<size_t>( row ) < m_history.revisions.size() )
    {
        updateDetailsPanel( m_history.revisions[row] );
    }
    else
    {
        clearDetailsPanel();
    }

    updateButtonStates();
}


void DIALOG_PLM_HISTORY::onCompare( wxCommandEvent& event )
{
    std::vector<wxString> selected = GetSelectedRevisions();

    if( selected.size() != 2 )
    {
        wxMessageBox( _( "Please select exactly two revisions to compare." ), _( "Information" ),
                      wxOK | wxICON_INFORMATION, this );
        return;
    }

    wxMessageBox( wxString::Format( _( "Compare functionality not yet implemented.\n\n"
                                        "Would compare revisions:\n%s\nvs\n%s" ),
                                     selected[0], selected[1] ),
                  _( "Not Implemented" ), wxOK | wxICON_INFORMATION, this );
}


void DIALOG_PLM_HISTORY::onDownload( wxCommandEvent& event )
{
    std::vector<wxString> selected = GetSelectedRevisions();

    if( selected.empty() )
    {
        wxMessageBox( _( "Please select a revision to download." ), _( "Information" ),
                      wxOK | wxICON_INFORMATION, this );
        return;
    }

    wxMessageBox( wxString::Format( _( "Download functionality not yet implemented.\n\n"
                                        "Would download revision: %s" ),
                                     selected[0] ),
                  _( "Not Implemented" ), wxOK | wxICON_INFORMATION, this );
}


void DIALOG_PLM_HISTORY::onClose( wxCommandEvent& event )
{
    EndModal( wxID_CLOSE );
}
