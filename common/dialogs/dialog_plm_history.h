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

/**
 * @file dialog_plm_history.h
 * @brief Dialog for viewing and managing PLM revision history.
 */

#ifndef DIALOG_PLM_HISTORY_H
#define DIALOG_PLM_HISTORY_H

#include "dialog_plm_history_base.h"

#include <plm/plm_lifecycle.h>
#include <plm/plm_release.h>
#include <plm/plm_revision.h>

#include <vector>

class wxButton;
class wxChoice;
class wxStaticText;
class wxTextCtrl;
class wxDataViewListCtrl;
class PLM_PLUGIN;


/**
 * Dialog for displaying revision history of a PLM project.
 *
 * Provides a timeline view of all revisions with details, comparison capabilities,
 * the ability to download specific revisions, lifecycle state transitions,
 * and release information.
 */
class DIALOG_PLM_HISTORY : public DIALOG_PLM_HISTORY_BASE
{
public:
    DIALOG_PLM_HISTORY( wxWindow* aParent, PLM_PLUGIN* aPlugin );
    ~DIALOG_PLM_HISTORY();

    /**
     * Load revision history for a specific project.
     *
     * @param aProjectId The PLM project identifier to load history for.
     * @return True if history was loaded successfully.
     */
    bool LoadHistory( const wxString& aProjectId );

    /**
     * Get the currently selected revisions for comparison.
     *
     * @return Vector of selected revision IDs (up to 2 for comparison).
     */
    std::vector<wxString> GetSelectedRevisions() const;

protected:
    void onRevisionSelected( wxDataViewEvent& event ) override;
    void onCompare( wxCommandEvent& event ) override;
    void onDownload( wxCommandEvent& event ) override;
    void onClose( wxCommandEvent& event ) override;

private:
    void populateRevisionList();
    void updateDetailsPanel( const PLM_REVISION& aRevision );
    void clearDetailsPanel();
    void updateButtonStates();
    wxString formatTimestamp( int64_t aTimestamp ) const;

    // State transition extensions
    void createStateTransitionSection();
    void loadStateInfo();
    void loadValidTransitions();
    void onChangeState( wxCommandEvent& event );

    // Release info extensions
    void createReleaseInfoSection();
    void loadReleases();
    void onCreateRelease( wxCommandEvent& event );

    PLM_PLUGIN*           m_plugin;
    wxString              m_projectId;
    PLM_REVISION_HISTORY  m_history;

    // State transition controls
    wxStaticText*         m_currentStateLabel;
    wxStaticText*         m_currentStateValue;
    wxChoice*             m_transitionChoice;
    wxButton*             m_changeStateBtn;
    wxTextCtrl*           m_transitionCommentCtrl;
    std::vector<PLM_VALID_TRANSITION> m_validTransitions;

    // Release info controls
    wxDataViewListCtrl*   m_releaseList;
    wxButton*             m_createReleaseBtn;
    std::vector<PLM_RELEASE> m_releases;
};


#endif // DIALOG_PLM_HISTORY_H
