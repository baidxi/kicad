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

#include "dialog_plm_review.h"
#include <plm/plm_plugin.h>

#include <wx/datetime.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textdlg.h>


DIALOG_PLM_REVIEW::DIALOG_PLM_REVIEW( wxWindow* aParent, PLM_PLUGIN* aPlugin,
                                        const wxString& aProjectId ) :
        DIALOG_PLM_REVIEW_BASE( aParent ),
        m_plugin( aPlugin ),
        m_projectId( aProjectId ),
        m_selectedIndex( -1 )
{
    setupColumns();
    loadReviews();

    clearDetails();

    SetupStandardButtons();
    finishDialogSettings();
}


DIALOG_PLM_REVIEW::~DIALOG_PLM_REVIEW()
{
}


void DIALOG_PLM_REVIEW::setupColumns()
{
    m_reviewListCtrl->InsertColumn( 0, _( "ID" ), wxLIST_FORMAT_LEFT, 80 );
    m_reviewListCtrl->InsertColumn( 1, _( "Title" ), wxLIST_FORMAT_LEFT, 140 );
    m_reviewListCtrl->InsertColumn( 2, _( "Status" ), wxLIST_FORMAT_LEFT, 80 );
    m_reviewListCtrl->InsertColumn( 3, _( "Author" ), wxLIST_FORMAT_LEFT, 80 );
}


void DIALOG_PLM_REVIEW::loadReviews()
{
    if( !m_plugin )
        return;

    m_reviews = m_plugin->GetReviews( m_projectId );
    populateReviewList();
}


void DIALOG_PLM_REVIEW::populateReviewList()
{
    m_reviewListCtrl->DeleteAllItems();

    for( size_t i = 0; i < m_reviews.size(); ++i )
    {
        const PLM_REVIEW& review = m_reviews[i];

        long idx = m_reviewListCtrl->InsertItem( static_cast<long>( i ), review.review_id );
        m_reviewListCtrl->SetItem( idx, 1, review.title );
        m_reviewListCtrl->SetItem( idx, 2, review.GetStatusName() );
        m_reviewListCtrl->SetItem( idx, 3, review.submitter_name );
    }
}


void DIALOG_PLM_REVIEW::onReviewSelected( wxListEvent& event )
{
    int selection = event.GetIndex();

    if( selection >= 0 && selection < static_cast<int>( m_reviews.size() ) )
    {
        showReviewDetails( selection );
    }

    event.Skip();
}


void DIALOG_PLM_REVIEW::showReviewDetails( int aIndex )
{
    if( aIndex < 0 || aIndex >= static_cast<int>( m_reviews.size() ) )
        return;

    m_selectedIndex = aIndex;

    // Fetch full review details
    PLM_REVIEW review = m_plugin->GetReview( m_reviews[aIndex].review_id );
    m_reviews[aIndex] = review;

    m_titleLabel->SetLabel( review.title );
    m_statusLabel->SetLabel( review.GetStatusName() );
    m_authorLabel->SetLabel( review.submitter_name );
    m_dateLabel->SetLabel( formatDate( review.created_at ) );

    populateReviewers( review );
    populateComments( review.review_id );
    updateDecisionButtons( review );

    m_rightPanel->Layout();
}


void DIALOG_PLM_REVIEW::populateReviewers( const PLM_REVIEW& aReview )
{
    clearScrolledContent( m_reviewersScrolled, bReviewersSizer );

    for( const auto& reviewer : aReview.reviewers )
    {
        wxString roleName;

        switch( reviewer.role )
        {
        case PLM_REVIEW_ROLE::REVIEWER: roleName = wxS( "Reviewer" ); break;
        case PLM_REVIEW_ROLE::REQUIRED: roleName = wxS( "Required" ); break;
        case PLM_REVIEW_ROLE::APPROVER: roleName = wxS( "Approver" ); break;
        }

        wxString decisionName = PLM_REVIEW::GetDecisionName( reviewer.decision );

        wxString text = wxString::Format( wxS( "%s (%s) - %s" ),
                                          reviewer.display_name,
                                          roleName,
                                          decisionName );

        wxStaticText* label = new wxStaticText( m_reviewersScrolled, wxID_ANY, text );

        if( reviewer.IsApproval() )
        {
            label->SetForegroundColour( wxColour( 0, 128, 0 ) );
        }
        else if( reviewer.decision == PLM_REVIEW_DECISION::REJECT ||
                 reviewer.decision == PLM_REVIEW_DECISION::REQUEST_CHANGES )
        {
            label->SetForegroundColour( wxColour( 192, 0, 0 ) );
        }

        bReviewersSizer->Add( label, 0, wxLEFT | wxTOP, 3 );
    }

    m_reviewersScrolled->Layout();
    m_reviewersScrolled->FitInside();
}


void DIALOG_PLM_REVIEW::populateComments( const wxString& aReviewId )
{
    clearScrolledContent( m_commentsScrolled, bCommentsSizer );

    if( !m_plugin )
        return;

    std::vector<PLM_REVIEW_COMMENT> comments = m_plugin->GetReviewComments( aReviewId );

    for( const auto& comment : comments )
    {
        wxPanel* commentPanel = new wxPanel( m_commentsScrolled );
        wxBoxSizer* cSizer = new wxBoxSizer( wxVERTICAL );

        wxString header = wxString::Format( wxS( "%s  %s" ),
                                            comment.author_name,
                                            formatDate( comment.created_at ) );
        wxStaticText* headerLabel = new wxStaticText( commentPanel, wxID_ANY, header );
        headerLabel->SetFont( headerLabel->GetFont().Bold() );
        cSizer->Add( headerLabel, 0, wxBOTTOM, 2 );

        wxStaticText* bodyLabel = new wxStaticText( commentPanel, wxID_ANY, comment.body );
        bodyLabel->Wrap( 400 );
        cSizer->Add( bodyLabel, 0, wxBOTTOM, 5 );

        if( comment.resolved )
        {
            wxStaticText* resolvedLabel = new wxStaticText( commentPanel, wxID_ANY,
                                                            _( "[Resolved]" ) );
            resolvedLabel->SetForegroundColour( wxColour( 0, 128, 0 ) );
            cSizer->Add( resolvedLabel, 0, wxBOTTOM, 2 );
        }

        commentPanel->SetSizer( cSizer );
        bCommentsSizer->Add( commentPanel, 0, wxEXPAND | wxALL, 5 );
    }

    if( comments.empty() )
    {
        wxStaticText* noComments = new wxStaticText( m_commentsScrolled, wxID_ANY,
                                                     _( "No comments yet." ) );
        noComments->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
        bCommentsSizer->Add( noComments, 0, wxALL, 5 );
    }

    m_commentsScrolled->Layout();
    m_commentsScrolled->FitInside();
}


void DIALOG_PLM_REVIEW::clearDetails()
{
    m_titleLabel->SetLabel( wxEmptyString );
    m_statusLabel->SetLabel( wxEmptyString );
    m_authorLabel->SetLabel( wxEmptyString );
    m_dateLabel->SetLabel( wxEmptyString );

    clearScrolledContent( m_reviewersScrolled, bReviewersSizer );
    clearScrolledContent( m_commentsScrolled, bCommentsSizer );

    m_decisionPanel->Hide();
    m_selectedIndex = -1;
}


void DIALOG_PLM_REVIEW::clearScrolledContent( wxScrolledWindow* aScrolled, wxBoxSizer* aSizer )
{
    aScrolled->DestroyChildren();
    aSizer->Clear();
    aScrolled->Layout();
}


void DIALOG_PLM_REVIEW::updateDecisionButtons( const PLM_REVIEW& aReview )
{
    bool isOpen = aReview.IsOpen();

    m_approveBtn->Show( isOpen );
    m_requestChangesBtn->Show( isOpen );
    m_rejectBtn->Show( isOpen );
    m_mergeBtn->Show( aReview.status == PLM_REVIEW_STATUS::APPROVED );
    m_decisionPanel->Show( isOpen || aReview.status == PLM_REVIEW_STATUS::APPROVED );

    m_decisionPanel->Layout();
}


void DIALOG_PLM_REVIEW::onNewReview( wxCommandEvent& event )
{
    if( !m_plugin )
        return;

    wxTextEntryDialog titleDlg( this, _( "Enter review title:" ), _( "New Review" ) );

    if( titleDlg.ShowModal() != wxID_OK )
        return;

    wxString title = titleDlg.GetValue().Trim();

    if( title.IsEmpty() )
        return;

    PLM_REVIEW_SUBMIT_OPTIONS opts( title, wxEmptyString );
    PLM_REVIEW_RESULT result = m_plugin->SubmitForReview( m_projectId, opts );

    if( !result.success )
    {
        wxMessageBox( wxString::Format( _( "Failed to create review: %s" ),
                                        result.error_message ),
                      _( "Error" ), wxOK | wxICON_ERROR, this );
        return;
    }

    loadReviews();

    wxMessageBox( _( "Review created successfully." ), _( "Review Created" ),
                  wxOK | wxICON_INFORMATION, this );
}


void DIALOG_PLM_REVIEW::onPostComment( wxCommandEvent& event )
{
    if( !m_plugin || m_selectedIndex < 0 )
        return;

    wxString body = m_commentInput->GetValue().Trim();

    if( body.IsEmpty() )
        return;

    PLM_REVIEW_COMMENT comment = PLM_REVIEW_COMMENT::General(
            m_reviews[m_selectedIndex].review_id, body );

    if( !m_plugin->AddReviewComment( comment ) )
    {
        wxMessageBox( _( "Failed to post comment." ), _( "Error" ),
                      wxOK | wxICON_ERROR, this );
        return;
    }

    m_commentInput->Clear();
    populateComments( m_reviews[m_selectedIndex].review_id );
}


void DIALOG_PLM_REVIEW::onApprove( wxCommandEvent& event )
{
    submitDecision( PLM_REVIEW_DECISION::APPROVE );
}


void DIALOG_PLM_REVIEW::onRequestChanges( wxCommandEvent& event )
{
    submitDecision( PLM_REVIEW_DECISION::REQUEST_CHANGES );
}


void DIALOG_PLM_REVIEW::onReject( wxCommandEvent& event )
{
    submitDecision( PLM_REVIEW_DECISION::REJECT );
}


void DIALOG_PLM_REVIEW::submitDecision( PLM_REVIEW_DECISION aDecision )
{
    if( !m_plugin || m_selectedIndex < 0 )
        return;

    wxString decisionName = PLM_REVIEW::GetDecisionName( aDecision );

    wxTextEntryDialog commentDlg( this,
                                  wxString::Format( _( "Comment for %s decision:" ),
                                                    decisionName ),
                                  _( "Review Decision" ) );

    if( commentDlg.ShowModal() != wxID_OK )
        return;

    PLM_REVIEW_RESULT result = m_plugin->SubmitReviewDecision(
            m_reviews[m_selectedIndex].review_id, aDecision, commentDlg.GetValue() );

    if( !result.success )
    {
        wxMessageBox( wxString::Format( _( "Failed to submit decision: %s" ),
                                        result.error_message ),
                      _( "Error" ), wxOK | wxICON_ERROR, this );
        return;
    }

    m_reviews[m_selectedIndex] = result.review;
    showReviewDetails( m_selectedIndex );

    wxMessageBox( wxString::Format( _( "Decision '%s' submitted." ), decisionName ),
                  _( "Decision Submitted" ), wxOK | wxICON_INFORMATION, this );
}


void DIALOG_PLM_REVIEW::onMerge( wxCommandEvent& event )
{
    if( !m_plugin || m_selectedIndex < 0 )
        return;

    int answer = wxMessageBox( _( "Merge this review? This cannot be undone." ),
                               _( "Confirm Merge" ),
                               wxYES_NO | wxICON_QUESTION, this );

    if( answer != wxYES )
        return;

    PLM_REVIEW_RESULT result = m_plugin->MergeReview(
            m_reviews[m_selectedIndex].review_id, wxS( "merge" ) );

    if( !result.success )
    {
        wxMessageBox( wxString::Format( _( "Failed to merge: %s" ), result.error_message ),
                      _( "Error" ), wxOK | wxICON_ERROR, this );
        return;
    }

    m_reviews[m_selectedIndex] = result.review;
    showReviewDetails( m_selectedIndex );
    populateReviewList();

    wxMessageBox( _( "Review merged successfully." ), _( "Merged" ),
                  wxOK | wxICON_INFORMATION, this );
}


wxString DIALOG_PLM_REVIEW::formatDate( int64_t aTimestamp ) const
{
    if( aTimestamp == 0 )
        return _( "Not set" );

    wxDateTime dt;
    dt.Set( static_cast<time_t>( aTimestamp ) );

    return dt.FormatISOCombined( ' ' );
}
