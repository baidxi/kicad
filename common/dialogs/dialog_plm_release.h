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
 * @file dialog_plm_release.h
 * @brief Wizard dialog for creating PLM release packages.
 */

#ifndef DIALOG_PLM_RELEASE_H
#define DIALOG_PLM_RELEASE_H

#include "dialog_plm_release_base.h"

#include <plm/plm_release.h>

#include <vector>


class PLM_PLUGIN;


/**
 * Wizard dialog for creating a formal release package.
 *
 * Guides the user through version selection, pre-release validation,
 * manufacturing output selection, and final release creation.
 */
class DIALOG_PLM_RELEASE : public DIALOG_PLM_RELEASE_BASE
{
public:
    DIALOG_PLM_RELEASE( wxWindow* aParent, PLM_PLUGIN* aPlugin, const wxString& aProjectId );
    ~DIALOG_PLM_RELEASE();

    /**
     * Get the created release after the wizard completes.
     *
     * @return The release record, or empty release if wizard was cancelled.
     */
    PLM_RELEASE GetRelease() const { return m_release; }

protected:
    void onBack( wxCommandEvent& event ) override;
    void onNext( wxCommandEvent& event ) override;
    void onCancel( wxCommandEvent& event ) override;

private:
    enum WizardPage
    {
        PAGE_INFO = 0,
        PAGE_CHECKS = 1,
        PAGE_OUTPUTS = 2,
        PAGE_SUMMARY = 3
    };

    void initVersionSchemes();
    void navigateToPage( int aPage );
    void updateNavigationButtons();
    void runValidationChecks();
    void buildSummary();
    PLM_RELEASE_OPTIONS buildReleaseOptions() const;
    bool createRelease();

    wxString getVersionSchemeName( PLM_VERSION_SCHEME aScheme ) const;
    wxString getArtifactTypeName( PLM_ARTIFACT_TYPE aType ) const;

    PLM_PLUGIN*                     m_plugin;
    wxString                        m_projectId;
    int                             m_currentPage;
    std::vector<PLM_RELEASE_CHECK>  m_checks;
    PLM_RELEASE                     m_release;
};


#endif // DIALOG_PLM_RELEASE_H
