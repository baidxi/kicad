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

#ifndef DIALOG_PLM_REVIEW_H
#define DIALOG_PLM_REVIEW_H

#include "dialog_plm_review_base.h"
#include <plm/plm_review.h>
#include <vector>

class PLM_PLUGIN;


/**
 * Design review management dialog.
 *
 * Shows a list of reviews for a project with full detail view including
 * reviewers, comments thread, and decision actions. Supports creating
 * new reviews and performing review decisions.
 */
class DIALOG_PLM_REVIEW : public DIALOG_PLM_REVIEW_BASE
{
public:
    DIALOG_PLM_REVIEW( wxWindow* aParent, PLM_PLUGIN* aPlugin, const wxString& aProjectId );
    ~DIALOG_PLM_REVIEW();

protected:
    void onReviewSelected( wxListEvent& event ) override;
    void onNewReview( wxCommandEvent& event ) override;
    void onPostComment( wxCommandEvent& event ) override;
    void onApprove( wxCommandEvent& event ) override;
    void onRequestChanges( wxCommandEvent& event ) override;
    void onReject( wxCommandEvent& event ) override;
    void onMerge( wxCommandEvent& event ) override;

private:
    /**
     * Set up the review list columns.
     */
    void setupColumns();

    /**
     * Load reviews for the current project.
     */
    void loadReviews();

    /**
     * Populate the review list control.
     */
    void populateReviewList();

    /**
     * Show details for a specific review.
     */
    void showReviewDetails( int aIndex );

    /**
     * Populate the reviewers panel for the current review.
     */
    void populateReviewers( const PLM_REVIEW& aReview );

    /**
     * Populate the comments thread for the current review.
     */
    void populateComments( const wxString& aReviewId );

    /**
     * Clear the details panel.
     */
    void clearDetails();

    /**
     * Clear dynamically created controls in a scrolled window.
     */
    void clearScrolledContent( wxScrolledWindow* aScrolled, wxBoxSizer* aSizer );

    /**
     * Update decision button visibility based on review state.
     */
    void updateDecisionButtons( const PLM_REVIEW& aReview );

    /**
     * Submit a review decision and refresh.
     */
    void submitDecision( PLM_REVIEW_DECISION aDecision );

    /**
     * Format a timestamp for display.
     */
    wxString formatDate( int64_t aTimestamp ) const;

    PLM_PLUGIN*             m_plugin;
    wxString                m_projectId;
    std::vector<PLM_REVIEW> m_reviews;
    int                     m_selectedIndex;
};


#endif // DIALOG_PLM_REVIEW_H
