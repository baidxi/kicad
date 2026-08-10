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

// kicad_curl_easy.h **must be** included before any wxWidgets header to avoid conflicts
// at least on Windows/msys2
#include "kicad_curl/kicad_curl_easy.h"

#include "bitmaps.h"
#include "dialog_manage_repositories.h"
#include "dialog_pcm.h"
#include <eda_base_frame.h>
#include "grid_tricks.h"
#include "ki_exception.h"
#include "pcm_task_manager.h"
#include "pgm_base.h"
#include "settings/kicad_settings.h"
#include "settings/settings_manager.h"
#include "thread"
#include "widgets/wx_grid.h"

#include <algorithm>
#include <fstream>
#include <kiplatform/ui.h>
#include <launch_ext.h>
#include <sstream>
#include <vector>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>


#define GRID_CELL_MARGIN 4

// Notes: These strings are static, so wxGetTranslation must be called to display the
// transalted text
static std::vector<std::pair<PCM_PACKAGE_TYPE, wxString>> PACKAGE_TYPE_LIST = {
    { PT_PLUGIN,     _( "Plugins (%d)" ) },
    { PT_FAB,        _( "Fabrication plugins (%d)" ) },
    { PT_LIBRARY,    _( "Libraries (%d)" ) },
    { PT_DATASOURCE, _( "Data sources (%d)" ) },
    { PT_COLORTHEME, _( "Color themes (%d)" ) },
};


DIALOG_PCM::DIALOG_PCM( EDA_BASE_FRAME* parent, std::shared_ptr<PLUGIN_CONTENT_MANAGER> pcm ) :
        DIALOG_PCM_BASE( parent ),
        m_parentFrame( parent ),
        m_pcm( pcm )
{
    // correct the min size from wxfb with fromdip
    SetMinSize( FromDIP( GetMinSize() ) );

    SetDoubleBuffered( true );

    m_defaultBitmap = KiBitmap( BITMAPS::icon_pcm );

    m_pcm->SetDialogWindow( this );
    m_pcm->StopBackgroundUpdate();

    m_gridPendingActions->PushEventHandler( new GRID_TRICKS( m_gridPendingActions ) );

    m_discardActionButton->SetBitmap( KiBitmapBundle( BITMAPS::small_trash ) );
    m_panelPending->Layout();

    m_actionCallback = [this]( const PACKAGE_VIEW_DATA& aData, PCM_PACKAGE_ACTION aAction,
                               const wxString& aVersion )
    {
        if( aAction == PPA_UPDATE && m_pcm->IsPackagePinned( aData.package.identifier ) )
        {
            if( wxMessageBox( wxString::Format( _( "Are you sure you want to update pinned package "
                                                   "from version %s to %s?" ),
                                                aData.current_version, aVersion ),
                              _( "Confirm update" ), wxICON_QUESTION | wxYES_NO, this )
                    == wxNO )
            {
                return;
            }
        }

        m_gridPendingActions->Freeze();

        PCM_PACKAGE_STATE new_state;

        m_gridPendingActions->AppendRows();
        int row = m_gridPendingActions->GetNumberRows() - 1;

        m_gridPendingActions->SetCellValue( row, PENDING_COL_NAME, aData.package.name );
        m_gridPendingActions->SetCellValue( row, PENDING_COL_REPOSITORY, aData.repository_name );

        switch( aAction )
        {
        default:
        case PPA_INSTALL:
            m_gridPendingActions->SetCellValue( row, PENDING_COL_ACTION, _( "Install" ) );
            m_gridPendingActions->SetCellValue( row, PENDING_COL_VERSION, aVersion );
            new_state = PPS_PENDING_INSTALL;
            break;

        case PPA_UPDATE:
            m_gridPendingActions->SetCellValue( row, PENDING_COL_ACTION, _( "Update" ) );
            m_gridPendingActions->SetCellValue( row, PENDING_COL_VERSION,
                                                wxString::Format( wxT( "%s \u279C %s" ),
                                                                  aData.current_version,
                                                                  aVersion ) );
            new_state = PPS_PENDING_UPDATE;
            break;

        case PPA_UNINSTALL:
            m_gridPendingActions->SetCellValue( row, PENDING_COL_ACTION, _( "Uninstall" ) );
            m_gridPendingActions->SetCellValue( row, PENDING_COL_VERSION,
                                                m_pcm->GetInstalledPackageVersion( aData.package.identifier ) );
            new_state = PPS_PENDING_UNINSTALL;
            break;
        }

        m_pendingActions.emplace_back( aAction, aData.repository_id, aData.package, aVersion );

        m_gridPendingActions->Thaw();

        updatePendingActionsTab();

        updatePackageState( aData.package.identifier, new_state );
    };

    m_pinCallback =
            [this]( const wxString& aPackageId, const PCM_PACKAGE_STATE aState, const bool aPinned )
            {
                m_pcm->SetPinned( aPackageId, aPinned );

                updatePackageState( aPackageId, aState );
            };

    m_installedPanel = new PANEL_PACKAGES_VIEW( m_panelInstalledHolder, m_pcm, m_actionCallback, m_pinCallback );
    m_panelInstalledHolder->GetSizer()->Add( m_installedPanel, 1, wxEXPAND );
    m_panelInstalledHolder->Layout();

    for( const std::pair<PCM_PACKAGE_TYPE, wxString>& entry : PACKAGE_TYPE_LIST )
    {
        PANEL_PACKAGES_VIEW* panel = new PANEL_PACKAGES_VIEW( m_contentNotebook, m_pcm, m_actionCallback,
                                                              m_pinCallback );
        wxString             label = wxGetTranslation( entry.second );
        m_contentNotebook->AddPage( panel, wxString::Format( label, 0 ) );
        m_repositoryContentPanels.insert( { entry.first, panel } );
    }

    m_dialogNotebook->SetPageText( 0, wxString::Format( _( "Repository (%d)" ), 0 ) );

    setInstalledPackages();
    updatePendingActionsTab();

    m_dialogNotebook->SetSelection( 0 );

    SetupStandardButtons( { { wxID_OK,     _( "Close" ) },
                            { wxID_APPLY,  _( "Apply Pending Changes" ) },
                            { wxID_CANCEL, _( "Discard Pending Changes" ) } } );

    Bind( wxEVT_CLOSE_WINDOW, &DIALOG_PCM::OnCloseWindow, this );
    m_sdbSizer1Cancel->Bind( wxEVT_UPDATE_UI, &DIALOG_PCM::OnUpdateEventButtons, this );
    m_sdbSizer1Apply->Bind( wxEVT_UPDATE_UI, &DIALOG_PCM::OnUpdateEventButtons, this );

    // Has to be called after DIALOG_SHIM reads cached values
    CallAfter(
            [this]()
            {
                setRepositoryListFromPcm();
            } );

    for( int col = 0; col < m_gridPendingActions->GetNumberCols(); col++ )
    {
        const wxString& heading = m_gridPendingActions->GetColLabelValue( col );
        int             headingWidth = GetTextExtent( heading ).x + 2 * GRID_CELL_MARGIN;

        // Set the minimal width to the column label size.
        m_gridPendingActions->SetColMinimalWidth( col, headingWidth );
    }

    // fix sizers now widgets are set.
    finishDialogSettings();
}


DIALOG_PCM::~DIALOG_PCM()
{
    cancelRepositoryFetch();

    Unbind( wxEVT_CLOSE_WINDOW, &DIALOG_PCM::OnCloseWindow, this );
    m_sdbSizer1Cancel->Unbind( wxEVT_UPDATE_UI, &DIALOG_PCM::OnUpdateEventButtons, this );
    m_sdbSizer1Apply->Unbind( wxEVT_UPDATE_UI, &DIALOG_PCM::OnUpdateEventButtons, this );

    m_pcm->SaveInstalledPackages();
    m_pcm->SetDialogWindow( nullptr );
    m_pcm->RunBackgroundUpdate();

    m_gridPendingActions->PopEventHandler( true );
}


void DIALOG_PCM::SetActivePackageType( PCM_PACKAGE_TYPE aType )
{
    for( size_t i = 0; i < PACKAGE_TYPE_LIST.size(); ++i )
    {
        if( PACKAGE_TYPE_LIST[i].first == aType )
        {
            m_contentNotebook->SetSelection( i );
            break;
        }
    }
}


void DIALOG_PCM::OnUpdateEventButtons( wxUpdateUIEvent& event )
{
    event.Enable( !m_pendingActions.empty() );
}


void DIALOG_PCM::OnCloseClicked( wxCommandEvent& event )
{
    if( m_pendingActions.size() == 0
        || wxMessageBox( _( "Are you sure you want to close the package manager "
                            "and discard pending changes?" ),
                         _( "Plugin and Content Manager" ), wxICON_QUESTION | wxYES_NO, this )
                   == wxYES )
    {
        EndModal( wxID_OK );
    }
}


void DIALOG_PCM::OnCloseWindow( wxCloseEvent& aEvent )
{
    wxCommandEvent dummy;

    OnCloseClicked( dummy );
}


void DIALOG_PCM::OnManageRepositoriesClicked( wxCommandEvent& event )
{
    // The repository management dialog can modify the repository list and fetch
    // repositories on the main thread. Make sure no background fetch is running
    // while it is open.
    cancelRepositoryFetch();

    DIALOG_MANAGE_REPOSITORIES* dialog = new DIALOG_MANAGE_REPOSITORIES( this, m_pcm );

    std::vector<std::pair<wxString, wxString>>            dialog_data;
    std::vector<std::tuple<wxString, wxString, wxString>> repo_list = m_pcm->GetRepositoryList();

    for( const auto& [id, url, name] : repo_list )
        dialog_data.push_back( std::make_pair( url, name ) );

    dialog->SetData( dialog_data );

    if( dialog->ShowModal() == wxID_SAVE )
    {
        dialog_data = dialog->GetData();
        m_pcm->SetRepositoryList( dialog_data );

        if( KICAD_SETTINGS* cfg = GetAppSettings<KICAD_SETTINGS>( "kicad" ) )
            cfg->m_PcmRepositories = std::move( dialog_data );

        setRepositoryListFromPcm();
    }
    else if( !m_selectedRepositoryId.IsEmpty() )
    {
        // A fetch of the selected repository may have been cancelled above. Restart it
        // so the repository tab is not left in the loading state.
        setRepositoryData( m_selectedRepositoryId );
    }

    dialog->Destroy();
}


void DIALOG_PCM::setRepositoryListFromPcm()
{
    std::vector<std::tuple<wxString, wxString, wxString>> repositories = m_pcm->GetRepositoryList();
    KICAD_SETTINGS*                                       cfg = GetAppSettings<KICAD_SETTINGS>( "kicad" );

    m_choiceRepository->Clear();

    for( const auto& [id, url, name] : repositories )
        m_choiceRepository->Append( url, new wxStringClientData( id ) );

    if( repositories.size() > 0 )
    {
        int idx = 0;

        if( cfg && !cfg->m_PcmLastSelectedRepoId.IsEmpty() )
        {
            auto it = std::find_if( repositories.begin(), repositories.end(),
                                    [&cfg]( const auto& repo )
                                    {
                                        return std::get<0>( repo ) == cfg->m_PcmLastSelectedRepoId;
                                    } );

            if( it != repositories.end() )
                idx = std::distance( repositories.begin(), it );
        }

        m_choiceRepository->SetSelection( idx );
        m_selectedRepositoryId = std::get<0>( repositories[idx] );
        setRepositoryData( m_selectedRepositoryId );
    }
    else
    {
        m_selectedRepositoryId = "";
    }

    if( cfg )
        cfg->m_PcmLastSelectedRepoId = m_selectedRepositoryId;
}


void DIALOG_PCM::OnRefreshClicked( wxCommandEvent& event )
{
    m_pcm->DiscardRepositoryCache( m_selectedRepositoryId );
    setRepositoryData( m_selectedRepositoryId );
}


void DIALOG_PCM::OnInstallFromFileClicked( wxCommandEvent& event )
{
    wxFileDialog open_file_dialog( this, _( "Install Package" ), wxEmptyString, wxEmptyString,
                                   wxT( "Zip files (*.zip)|*.zip" ), wxFD_OPEN | wxFD_FILE_MUST_EXIST );

    KIPLATFORM::UI::AllowNetworkFileSystems( &open_file_dialog );

    if( open_file_dialog.ShowModal() == wxID_CANCEL )
        return;

    // The task manager runs worker threads that modify the installed package list.
    // The fetch completion handler must not run in parallel with them (an event
    // yield could dispatch it), so make sure no background fetch is in flight.
    cancelRepositoryFetch();

    PCM_TASK_MANAGER task_manager( m_pcm );
    task_manager.InstallFromFile( this, open_file_dialog.GetPath() );

    m_changed_package_types.merge( task_manager.GetChangedPackageTypes() );

    setInstalledPackages();

    if( !m_selectedRepositoryId.IsEmpty() )
        setRepositoryData( m_selectedRepositoryId );
}


void DIALOG_PCM::OnRepositoryChoice( wxCommandEvent& event )
{
    wxStringClientData* data = static_cast<wxStringClientData*>(
            m_choiceRepository->GetClientObject( m_choiceRepository->GetSelection() ) );

    m_selectedRepositoryId = data->GetData();

    setRepositoryData( m_selectedRepositoryId );

    if( KICAD_SETTINGS* cfg = GetAppSettings<KICAD_SETTINGS>( "kicad" ) )
        cfg->m_PcmLastSelectedRepoId = m_selectedRepositoryId;
}


void DIALOG_PCM::setRepositoryData( const wxString& aRepositoryId )
{
    // If the repository is not cached yet, fetch it in a background thread so the
    // dialog stays responsive. This method is called again once the fetch finishes.
    if( !m_pcm->IsRepositoryCached( aRepositoryId ) )
    {
        startRepositoryFetch( aRepositoryId, true );
        return;
    }

    m_dialogNotebook->Freeze();

    for( const auto& [ packageType, packagesView ] : m_repositoryContentPanels )
        packagesView->ClearData();

    m_packageBitmaps = m_pcm->GetRepositoryPackageBitmaps( aRepositoryId );

    const std::vector<PCM_PACKAGE> packages = m_pcm->GetRepositoryPackages( aRepositoryId );

    std::unordered_map<PCM_PACKAGE_TYPE, std::vector<PACKAGE_VIEW_DATA>> data;

    for( const PCM_PACKAGE& pkg : packages )
    {
        if( pkg.type == PT_INVALID )
            continue;

        PACKAGE_VIEW_DATA package_data( pkg );

        if( m_packageBitmaps.count( package_data.package.identifier ) > 0 )
            package_data.bitmap = &m_packageBitmaps.at( package_data.package.identifier );
        else
            package_data.bitmap = &m_defaultBitmap;

        package_data.state = m_pcm->GetPackageState( aRepositoryId, pkg.identifier );

        if( package_data.state == PPS_INSTALLED || package_data.state == PPS_UPDATE_AVAILABLE )
        {
            package_data.current_version = m_pcm->GetInstalledPackageVersion( pkg.identifier );
            package_data.pinned = m_pcm->IsPackagePinned( pkg.identifier );
            package_data.swig_warning = m_pcm->UsesSWIGRuntime( pkg, package_data.current_version );
        }
        else if( !pkg.versions.empty() )
        {
            package_data.swig_warning = m_pcm->UsesSWIGRuntime( pkg, pkg.versions[0].version );
        }

        if( package_data.state == PPS_UPDATE_AVAILABLE )
            package_data.update_version = m_pcm->GetPackageUpdateVersion( pkg );

        for( const PENDING_ACTION& action : m_pendingActions )
        {
            if( action.package.identifier != pkg.identifier )
                continue;

            switch( action.action )
            {
            case PPA_INSTALL: package_data.state = PPS_PENDING_INSTALL; break;
            case PPA_UPDATE: package_data.state = PPS_PENDING_UPDATE; break;
            case PPA_UNINSTALL: package_data.state = PPS_PENDING_UNINSTALL; break;
            }

            break;
        }

        package_data.repository_id = aRepositoryId;
        package_data.repository_name = m_choiceRepository->GetStringSelection();

        // Fabrication plugins are displayed in a different tab although they are still plugins
        PCM_PACKAGE_TYPE type = pkg.category == PC_FAB ? PT_FAB : pkg.type;

        data[type].emplace_back( package_data );
    }

    for( size_t i = 0; i < PACKAGE_TYPE_LIST.size(); i++ )
    {
        PCM_PACKAGE_TYPE type = PACKAGE_TYPE_LIST[i].first;
        const wxString&  label = PACKAGE_TYPE_LIST[i].second;
        m_repositoryContentPanels[type]->SetData( data[type] );
        m_contentNotebook->SetPageText( i, wxString::Format( wxGetTranslation( label ),
                                                             (int) data[type].size() ) );
    }

    m_dialogNotebook->SetPageText( 0, wxString::Format( _( "Repository (%d)" ),
                                                        (int) packages.size() ) );

    m_dialogNotebook->Thaw();

    // Continue prefetching remaining repositories in the background, if any.
    startNextBackgroundFetch();
}


void DIALOG_PCM::startRepositoryFetch( const wxString& aRepositoryId, bool aForDisplay )
{
    cancelRepositoryFetch();

    if( aForDisplay )
    {
        // Clear the previously displayed repository's packages right away and show a
        // loading message so the stale list is not mistaken for the new repository.
        for( const auto& [ packageType, packagesView ] : m_repositoryContentPanels )
            packagesView->ShowMessage( _( "Loading repository..." ) );

        for( size_t i = 0; i < PACKAGE_TYPE_LIST.size(); i++ )
        {
            m_contentNotebook->SetPageText(
                    i, wxString::Format( wxGetTranslation( PACKAGE_TYPE_LIST[i].second ), 0 ) );
        }

        // The repository choice stays enabled so the user can switch away from a slow
        // repository without waiting for it to finish. Selecting another repository
        // cancels this fetch and starts a new one. Refresh would just restart the same
        // fetch, so it stays disabled until this one completes.
        m_refreshButton->Disable();
        m_dialogNotebook->SetPageText( 0, _( "Repository (loading...)" ) );
    }

    m_fetchInProgress = true;
    m_attemptedFetches.insert( aRepositoryId );
    m_fetchReporter = std::make_shared<PCM_FETCH_REPORTER>();

    m_fetchThread = std::thread(
            [this, aRepositoryId, reporter = m_fetchReporter]()
            {
                auto repository = std::make_shared<PCM_REPOSITORY>();
                bool success = m_pcm->DownloadRepository( aRepositoryId, *repository, reporter );

                // If the fetch was cancelled the dialog may be shutting down. Do not
                // touch it from here.
                if( reporter->IsCancelled() )
                    return;

                CallAfter(
                        [this, aRepositoryId, success, repository, reporter]()
                        {
                            // Re-check: the fetch may have been cancelled (and possibly
                            // superseded by another one) after the event was queued.
                            if( reporter->IsCancelled() )
                                return;

                            onRepositoryFetchCompleted( aRepositoryId, success,
                                                        std::move( *repository ) );
                        } );
            } );
}


void DIALOG_PCM::onRepositoryFetchCompleted( const wxString& aRepositoryId, bool aSuccess,
                                             PCM_REPOSITORY&& aRepository )
{
    m_fetchInProgress = false;
    m_choiceRepository->Enable();
    m_refreshButton->Enable();

    if( aSuccess )
    {
        m_pcm->StoreRepositoryCache( aRepositoryId, std::move( aRepository ) );

        // Refresh the states of installed packages now that this repository's metadata
        // is known, e.g. to show available updates, then re-apply the states of
        // packages with queued actions which the rebuild does not know about.
        setInstalledPackages();

        for( const PENDING_ACTION& action : m_pendingActions )
        {
            PCM_PACKAGE_STATE state = PPS_PENDING_INSTALL;

            switch( action.action )
            {
            case PPA_INSTALL: state = PPS_PENDING_INSTALL; break;
            case PPA_UPDATE: state = PPS_PENDING_UPDATE; break;
            case PPA_UNINSTALL: state = PPS_PENDING_UNINSTALL; break;
            }

            updatePackageState( action.package.identifier, state );
        }

        if( aRepositoryId == m_selectedRepositoryId )
        {
            setRepositoryData( aRepositoryId );
            return; // setRepositoryData() already continues the background fetches
        }
    }
    else if( aRepositoryId == m_selectedRepositoryId )
    {
        // Don't leave the previously displayed repository's packages on screen for
        // the failed one.
        for( const auto& [ packageType, packagesView ] : m_repositoryContentPanels )
            packagesView->ClearData();

        for( size_t i = 0; i < PACKAGE_TYPE_LIST.size(); i++ )
        {
            m_contentNotebook->SetPageText(
                    i, wxString::Format( wxGetTranslation( PACKAGE_TYPE_LIST[i].second ), 0 ) );
        }

        m_dialogNotebook->SetPageText( 0, wxString::Format( _( "Repository (%d)" ), 0 ) );
    }

    startNextBackgroundFetch();
}


void DIALOG_PCM::startNextBackgroundFetch()
{
    if( m_fetchInProgress )
        return;

    const std::vector<std::tuple<wxString, wxString, wxString>>& repositories =
            m_pcm->GetRepositoryList();

    for( const PCM_INSTALLATION_ENTRY& entry : m_pcm->GetInstalledPackages() )
    {
        const wxString& repoId = entry.repository_id;

        if( repoId.IsEmpty() || m_pcm->IsRepositoryCached( repoId )
            || m_attemptedFetches.count( repoId ) )
        {
            continue;
        }

        // Only fetch repositories that are still configured
        bool known = std::any_of( repositories.begin(), repositories.end(),
                                  [&repoId]( const auto& repo )
                                  {
                                      return std::get<0>( repo ) == repoId;
                                  } );

        if( !known )
        {
            m_attemptedFetches.insert( repoId );
            continue;
        }

        startRepositoryFetch( repoId, false );
        return;
    }
}


void DIALOG_PCM::cancelRepositoryFetch()
{
    if( m_fetchReporter )
        m_fetchReporter->Cancel();

    if( m_fetchThread.joinable() )
        m_fetchThread.join();

    m_fetchInProgress = false;

    // The completion handler of a cancelled fetch never runs, so re-enable the
    // controls it would have re-enabled.
    m_choiceRepository->Enable();
    m_refreshButton->Enable();
}


void DIALOG_PCM::OnPendingActionsCellClicked( wxGridEvent& event )
{
    m_gridPendingActions->ClearSelection();
    m_gridPendingActions->SelectRow( event.GetRow() );
}


void DIALOG_PCM::updatePendingActionsTab()
{
    m_dialogNotebook->SetPageText( 2, wxString::Format( _( "Pending (%d)" ),
                                                        (int) m_pendingActions.size() ) );

    for( int col = 0; col < m_gridPendingActions->GetNumberCols(); col++ )
    {
        // Set the width to see the full contents
        m_gridPendingActions->SetColSize( col, m_gridPendingActions->GetVisibleWidth( col ) );
    }
}


void DIALOG_PCM::setInstalledPackages()
{
    m_installedPanel->ClearData();

    const std::vector<PCM_INSTALLATION_ENTRY> installed = m_pcm->GetInstalledPackages();
    std::vector<PACKAGE_VIEW_DATA>            package_list;

    m_installedBitmaps = m_pcm->GetInstalledPackageBitmaps();

    for( const PCM_INSTALLATION_ENTRY& entry : installed )
    {
        PACKAGE_VIEW_DATA package_data( entry );

        if( m_installedBitmaps.count( package_data.package.identifier ) > 0 )
            package_data.bitmap = &m_installedBitmaps.at( package_data.package.identifier );
        else
            package_data.bitmap = &m_defaultBitmap;

        package_data.state = m_pcm->GetPackageState( entry.repository_id,
                                                     entry.package.identifier );

        package_data.swig_warning = m_pcm->UsesSWIGRuntime( entry.package, entry.current_version );

        if( package_data.state == PPS_UPDATE_AVAILABLE )
            package_data.update_version = m_pcm->GetPackageUpdateVersion( entry.package );

        package_list.emplace_back( package_data );
    }

    m_installedPanel->SetData( package_list );

    m_dialogNotebook->SetPageText( 1, wxString::Format( _( "Installed (%d)" ),
                                                        (int) package_list.size() ) );
}


void DIALOG_PCM::OnApplyChangesClicked( wxCommandEvent& event )
{
    if( m_pendingActions.size() == 0 )
        return;

    // The task manager runs worker threads that modify the installed package list.
    // The fetch completion handler must not run in parallel with them (an event
    // yield could dispatch it), so make sure no background fetch is in flight.
    cancelRepositoryFetch();

    m_sdbSizer1OK->Disable();
    m_sdbSizer1Apply->Disable();
    m_sdbSizer1Cancel->Disable();

    PCM_TASK_MANAGER task_manager( m_pcm );

    for( const PENDING_ACTION& action : m_pendingActions )
    {
        if( action.action == PPA_UNINSTALL )
        {
            task_manager.Uninstall( action.package );
        }
        else
        {
            bool isUpdate = action.action == PPA_UPDATE;
            task_manager.DownloadAndInstall( action.package, action.version, action.repository_id,
                                             isUpdate );
        }
    }

    task_manager.RunQueue( this );

    m_changed_package_types.merge( task_manager.GetChangedPackageTypes() );

    m_sdbSizer1OK->Enable();
    m_sdbSizer1Apply->Enable();
    m_sdbSizer1Cancel->Enable();

    setInstalledPackages();
    wxCommandEvent dummy;
    OnDiscardChangesClicked( dummy );

    if( !m_selectedRepositoryId.IsEmpty() )
        setRepositoryData( m_selectedRepositoryId );
}


void DIALOG_PCM::OnDiscardChangesClicked( wxCommandEvent& event )
{
    m_gridPendingActions->Freeze();

    for( int i = m_pendingActions.size() - 1; i >= 0; i-- )
        discardAction( i );

    updatePendingActionsTab();
    m_gridPendingActions->Thaw();
}


void DIALOG_PCM::OnDiscardActionClicked( wxCommandEvent& event )
{
    wxArrayInt rows = m_gridPendingActions->GetSelectedRows();

    std::sort( rows.begin(), rows.end(),
               []( const int& a, const int& b )
               {
                   return a > b;
               } );

    m_gridPendingActions->Freeze();

    for( int row : rows )
        discardAction( row );

    updatePendingActionsTab();
    m_gridPendingActions->Thaw();
}


void DIALOG_PCM::discardAction( int aIndex )
{
    m_gridPendingActions->DeleteRows( aIndex );

    PENDING_ACTION action = m_pendingActions[aIndex];

    PCM_PACKAGE_STATE state = m_pcm->GetPackageState( action.repository_id,
                                                      action.package.identifier );

    updatePackageState( action.package.identifier, state );

    m_pendingActions.erase( m_pendingActions.begin() + aIndex );
}


void DIALOG_PCM::updatePackageState( const wxString& aPackageId, const PCM_PACKAGE_STATE aState )
{
    bool pinned = m_pcm->IsPackagePinned( aPackageId );

    m_installedPanel->SetPackageState( aPackageId, aState, pinned );

    for( const auto& [ packageType, packagesView ] : m_repositoryContentPanels )
        packagesView->SetPackageState( aPackageId, aState, pinned );
}


void DIALOG_PCM::OnOpenPackageDirClicked( wxCommandEvent& event )
{
    LaunchExternal( m_pcm->Get3rdPartyPath() );
}
