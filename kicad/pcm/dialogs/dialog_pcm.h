/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2021 Andrew Lutsenko, anlutsenko at gmail dot com
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef DIALOG_PCM_H_
#define DIALOG_PCM_H_

#include "core/wx_stl_compat.h"
#include "dialog_pcm_base.h"
#include "panel_package.h"
#include "panel_packages_view.h"
#include "pcm.h"
#include "pcm_data.h"

#include <widgets/progress_reporter_base.h>

#include <memory>
#include <thread>
#include <vector>

class EDA_BASE_FRAME;


/**
 * Progress reporter used for repository fetches running in a background thread.
 *
 * It has no UI of its own and only provides thread-safe cancellation, so the
 * fetch can be aborted when the user switches repositories or closes the dialog.
 */
class PCM_FETCH_REPORTER : public PROGRESS_REPORTER_BASE
{
public:
    PCM_FETCH_REPORTER() : PROGRESS_REPORTER_BASE( 1 ) {}

    void Cancel() { m_cancelled.store( true ); }

private:
    bool updateUI() override { return !m_cancelled.load(); }
};


/** Implementing pcm main dialog. */
class DIALOG_PCM : public DIALOG_PCM_BASE
{
public:
    /** Constructor */
    DIALOG_PCM( EDA_BASE_FRAME* parent, std::shared_ptr<PLUGIN_CONTENT_MANAGER> pcm );
    ~DIALOG_PCM();

    EDA_BASE_FRAME* ParentFrame() const { return m_parentFrame; }

    ///< Closes the window, asks user confirmation if there are pending actions
    void OnCloseClicked( wxCommandEvent& event ) override;
    void OnCloseWindow( wxCloseEvent& aEvent );

    ///< Opens repository management dialog, saves changes to PCM
    void OnManageRepositoriesClicked( wxCommandEvent& event ) override;

    ///< Discards current repo cache, fetches it anew and displays
    void OnRefreshClicked( wxCommandEvent& event ) override;

    ///< Opens file selection dialog and installs selected package archive
    void OnInstallFromFileClicked( wxCommandEvent& event ) override;

    ///< Opens local directory where packages are installed in file manager
    void OnOpenPackageDirClicked( wxCommandEvent& event ) override;

    ///< Enqueues current pending actions in PCM_TASK_MANAGER and runs the queue
    void OnApplyChangesClicked( wxCommandEvent& event ) override;

    ///< Discards all pending changes
    void OnDiscardChangesClicked( wxCommandEvent& event ) override;

    ///< Switches to another repository
    void OnRepositoryChoice( wxCommandEvent& event ) override;

    ///< Selects the whole row in the grid if a cell is clicked
    void OnPendingActionsCellClicked( wxGridEvent& event ) override;

    ///< Discards selected pending actions
    void OnDiscardActionClicked( wxCommandEvent& event ) override;

    ///< Handles modification of the buttons' status
    void OnUpdateEventButtons( wxUpdateUIEvent& event );

    ///< Returns types of packages that were installed/uninstalled
    const std::unordered_set<PCM_PACKAGE_TYPE>& GetChangedPackageTypes() const
    {
        return m_changed_package_types;
    };

    void SetActivePackageType( PCM_PACKAGE_TYPE aType );

private:
    /**
     * @brief Gets package data from PCM and displays it on repository tab
     *
     * If the repository is not cached yet, starts a background fetch and returns.
     * The repository tab is populated when the fetch finishes.
     *
     * @param aRepositoryId id of the repository
     */
    void setRepositoryData( const wxString& aRepositoryId );

    /**
     * @brief Fetches repository data in a background thread
     *
     * @param aRepositoryId id of the repository to fetch
     * @param aForDisplay true if the fetch was triggered to display the repository
     *        on the repository tab: the repository choice and refresh button are
     *        disabled and the tab shows a loading indication while it runs. False
     *        for opportunistic background fetches of repositories that installed
     *        packages come from.
     */
    void startRepositoryFetch( const wxString& aRepositoryId, bool aForDisplay );

    ///< Cancels the ongoing repository fetch (if any) and joins the fetch thread
    void cancelRepositoryFetch();

    ///< Handles a finished repository fetch on the main thread
    void onRepositoryFetchCompleted( const wxString& aRepositoryId, bool aSuccess,
                                     PCM_REPOSITORY&& aRepository );

    /**
     * @brief Starts a background fetch of the next uncached repository that installed
     * packages come from, so that available updates show up on the Installed tab
     *
     * Fetches one repository at a time. Each is attempted at most once per dialog
     * lifetime. Does nothing if a fetch is already running.
     */
    void startNextBackgroundFetch();

    ///< Sets repository choice list values
    void setRepositoryListFromPcm();

    ///< Updates pending actions tab caption and content-fits the grid
    void updatePendingActionsTab();

    ///< Gets installed packages list from PCM and displays it on installed tab
    void setInstalledPackages();

    ///< Reflects new state of the package in all panels where it is displayed
    void updatePackageState( const wxString& aPackageId, const PCM_PACKAGE_STATE aState );

    ///< Discards specified pending action
    void discardAction( int aIndex );

private:
    EDA_BASE_FRAME*                                            m_parentFrame;
    std::shared_ptr<PLUGIN_CONTENT_MANAGER>                    m_pcm;
    ActionCallback                                             m_actionCallback;
    PinCallback                                                m_pinCallback;
    PANEL_PACKAGES_VIEW*                                       m_installedPanel;
    std::unordered_map<PCM_PACKAGE_TYPE, PANEL_PACKAGES_VIEW*> m_repositoryContentPanels;
    wxString                                                   m_selectedRepositoryId;
    std::unordered_map<wxString, wxBitmap>                     m_packageBitmaps;
    std::unordered_map<wxString, wxBitmap>                     m_installedBitmaps;
    wxBitmap                                                   m_defaultBitmap;
    std::unordered_set<PCM_PACKAGE_TYPE>                       m_changed_package_types;
    std::thread                                                m_fetchThread;
    std::shared_ptr<PCM_FETCH_REPORTER>                        m_fetchReporter;
    bool                                                       m_fetchInProgress = false;
    std::unordered_set<wxString>                               m_attemptedFetches;

    struct PENDING_ACTION
    {
        PCM_PACKAGE_ACTION action;
        wxString           repository_id;
        PCM_PACKAGE        package;
        wxString           version;
        PENDING_ACTION( const PCM_PACKAGE_ACTION& aAction, const wxString& aRepositoryId,
                        const PCM_PACKAGE& aPackage, const wxString& aVersion ) :
                action( aAction ),
                repository_id( aRepositoryId ), package( aPackage ), version( aVersion )
        {
        }
    };

    std::vector<PENDING_ACTION> m_pendingActions;

    enum PendingActionsGridColumns
    {
        PENDING_COL_ACTION = 0,
        PENDING_COL_NAME,
        PENDING_COL_VERSION,
        PENDING_COL_REPOSITORY
    };
};


#endif // DIALOG_PCM_H_
