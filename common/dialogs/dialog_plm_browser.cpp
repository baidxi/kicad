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

#include "dialog_plm_browser.h"

#include <wx/artprov.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/collpane.h>
#include <wx/datetime.h>
#include <wx/imaglist.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/propgrid/props.h>
#include <wx/statbox.h>

#include <algorithm>


enum
{
    ID_PREV_PAGE = wxID_HIGHEST + 1,
    ID_NEXT_PAGE,
    ID_ZOOM_IN,
    ID_ZOOM_OUT,
    ID_ZOOM_FIT,
    ID_PROP_EDIT,
    ID_PROP_SAVE,
    ID_PROP_CANCEL,
    ID_SEARCH,
    ID_CLEAR_SEARCH,
    ID_ADV_SEARCH,
    ID_ADV_CLEAR,
    ID_WHERE_USED,
    ID_RESULTS_NOTEBOOK
};


wxBEGIN_EVENT_TABLE( DIALOG_PLM_BROWSER, DIALOG_PLM_BROWSER_BASE )
    EVT_TREE_ITEM_EXPANDING( wxID_ANY, DIALOG_PLM_BROWSER::OnTreeItemExpanding )
    EVT_TREE_SEL_CHANGED( wxID_ANY, DIALOG_PLM_BROWSER::OnTreeSelChanged )
    EVT_DATAVIEW_COLUMN_HEADER_CLICK( wxID_ANY, DIALOG_PLM_BROWSER::OnFileListColumnHeaderClick )
    EVT_DATAVIEW_SELECTION_CHANGED( wxID_ANY, DIALOG_PLM_BROWSER::OnFileListSelChanged )
    EVT_BUTTON( ID_PREV_PAGE, DIALOG_PLM_BROWSER::OnPrevPage )
    EVT_BUTTON( ID_NEXT_PAGE, DIALOG_PLM_BROWSER::OnNextPage )
    EVT_BUTTON( ID_ZOOM_IN, DIALOG_PLM_BROWSER::OnZoomIn )
    EVT_BUTTON( ID_ZOOM_OUT, DIALOG_PLM_BROWSER::OnZoomOut )
    EVT_BUTTON( ID_ZOOM_FIT, DIALOG_PLM_BROWSER::OnZoomFit )
    EVT_BUTTON( ID_PROP_EDIT, DIALOG_PLM_BROWSER::OnPropertyEdit )
    EVT_BUTTON( ID_PROP_SAVE, DIALOG_PLM_BROWSER::OnPropertySave )
    EVT_BUTTON( ID_PROP_CANCEL, DIALOG_PLM_BROWSER::OnPropertyCancel )
    EVT_PG_CHANGED( wxID_ANY, DIALOG_PLM_BROWSER::OnPropertyChanged )
    EVT_SEARCHCTRL_SEARCH_BTN( ID_SEARCH, DIALOG_PLM_BROWSER::OnSearchButton )
    EVT_TEXT_ENTER( ID_SEARCH, DIALOG_PLM_BROWSER::OnSearchText )
    EVT_BUTTON( ID_CLEAR_SEARCH, DIALOG_PLM_BROWSER::OnClearSearch )
    EVT_BUTTON( ID_ADV_SEARCH, DIALOG_PLM_BROWSER::OnAdvancedSearch )
    EVT_BUTTON( ID_ADV_CLEAR, DIALOG_PLM_BROWSER::OnClearAdvancedSearch )
    EVT_BUTTON( ID_WHERE_USED, DIALOG_PLM_BROWSER::OnWhereUsed )
    EVT_NOTEBOOK_PAGE_CHANGED( ID_RESULTS_NOTEBOOK, DIALOG_PLM_BROWSER::OnResultsTabChanged )
    EVT_CLOSE( DIALOG_PLM_BROWSER::OnClose )
wxEND_EVENT_TABLE()


DIALOG_PLM_BROWSER::DIALOG_PLM_BROWSER( wxWindow* aParent, PLM_PLUGIN* aPlugin ) :
        DIALOG_PLM_BROWSER_BASE( aParent ),
        m_plugin( aPlugin ),
        m_connected( false ),
        m_mainSplitter( nullptr ),
        m_leftSplitter( nullptr ),
        m_rightSplitter( nullptr ),
        m_folderTree( nullptr ),
        m_fileListPanel( nullptr ),
        m_previewPanel( nullptr ),
        m_activityPanel( nullptr ),
        m_fileListCtrl( nullptr ),
        m_prevPageBtn( nullptr ),
        m_nextPageBtn( nullptr ),
        m_pageLabel( nullptr ),
        m_previewImage( nullptr ),
        m_previewLabel( nullptr ),
        m_zoomInBtn( nullptr ),
        m_zoomOutBtn( nullptr ),
        m_zoomFitBtn( nullptr ),
        m_previewZoomLevel( 100 ),
        m_propertyGrid( nullptr ),
        m_propertiesPanel( nullptr ),
        m_editBtn( nullptr ),
        m_saveBtn( nullptr ),
        m_cancelBtn( nullptr ),
        m_editMode( false ),
        m_currentListOptions( PLM_LIST_OPTIONS::FirstPage() ),
        m_searchPanel( nullptr ),
        m_searchCtrl( nullptr ),
        m_clearSearchBtn( nullptr ),
        m_searchMode( false ),
        m_advSearchPane( nullptr ),
        m_advSearchContent( nullptr ),
        m_advSearchBtn( nullptr ),
        m_advClearBtn( nullptr ),
        m_resultsNotebook( nullptr ),
        m_fileListTabPanel( nullptr ),
        m_whereUsedTabPanel( nullptr ),
        m_whereUsedTree( nullptr ),
        m_whereUsedBtn( nullptr )
{
    BuildLayout();
}


DIALOG_PLM_BROWSER::~DIALOG_PLM_BROWSER()
{
    Disconnect();
}


void DIALOG_PLM_BROWSER::BuildLayout()
{
    m_placeholderText->Hide();

    // Create main horizontal splitter (left tree vs right content)
    m_mainSplitter = new wxSplitterWindow( m_mainPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                           wxSP_3D | wxSP_LIVE_UPDATE );
    m_mainSplitter->SetMinimumPaneSize( 200 );

    // Left panel contains tree and activity panels
    m_leftSplitter = new wxSplitterWindow( m_mainSplitter, wxID_ANY, wxDefaultPosition,
                                           wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE );
    m_leftSplitter->SetMinimumPaneSize( 100 );

    // Create folder tree
    m_folderTree = new wxTreeCtrl( m_leftSplitter, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE );

    // Set up folder icons
    wxImageList* imageList = new wxImageList( 16, 16, true, 2 );
    imageList->Add( wxArtProvider::GetBitmap( wxART_FOLDER, wxART_OTHER, wxSize( 16, 16 ) ) );
    imageList->Add( wxArtProvider::GetBitmap( wxART_FOLDER_OPEN, wxART_OTHER, wxSize( 16, 16 ) ) );
    m_folderTree->AssignImageList( imageList );

    // Activity panel at bottom of left side
    m_activityPanel = new wxPanel( m_leftSplitter, wxID_ANY );
    wxStaticText* activityLabel = new wxStaticText( m_activityPanel, wxID_ANY, _( "Activity" ) );
    wxBoxSizer* activitySizer = new wxBoxSizer( wxVERTICAL );
    activitySizer->Add( activityLabel, 0, wxALL, 5 );
    m_activityPanel->SetSizer( activitySizer );

    m_leftSplitter->SplitHorizontally( m_folderTree, m_activityPanel, -150 );

    // Right panel contains file list and preview
    m_rightSplitter = new wxSplitterWindow( m_mainSplitter, wxID_ANY, wxDefaultPosition,
                                            wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE );
    m_rightSplitter->SetMinimumPaneSize( 150 );

    // File list panel with wxDataViewListCtrl
    m_fileListPanel = new wxPanel( m_rightSplitter, wxID_ANY );
    wxBoxSizer* fileListSizer = new wxBoxSizer( wxVERTICAL );

    // Search panel (only visible if search is supported)
    m_searchPanel = new wxPanel( m_fileListPanel, wxID_ANY );
    wxBoxSizer* searchSizer = new wxBoxSizer( wxHORIZONTAL );

    m_searchCtrl = new wxSearchCtrl( m_searchPanel, ID_SEARCH, wxEmptyString, wxDefaultPosition,
                                      wxDefaultSize, wxTE_PROCESS_ENTER );
    m_searchCtrl->SetDescriptiveText( _( "Search files..." ) );
    m_searchCtrl->ShowSearchButton( true );
    m_searchCtrl->ShowCancelButton( false );
    searchSizer->Add( m_searchCtrl, 1, wxEXPAND | wxALL, 2 );

    m_clearSearchBtn = new wxButton( m_searchPanel, ID_CLEAR_SEARCH, _( "Clear" ) );
    m_clearSearchBtn->Hide();
    searchSizer->Add( m_clearSearchBtn, 0, wxALL, 2 );

    m_searchPanel->SetSizer( searchSizer );
    fileListSizer->Add( m_searchPanel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 5 );

    // Create the data view list control
    m_fileListCtrl = new wxDataViewListCtrl( m_fileListPanel, wxID_ANY, wxDefaultPosition,
                                              wxDefaultSize, wxDV_ROW_LINES | wxDV_SINGLE );

    // Add columns
    m_fileListCtrl->AppendTextColumn( wxEmptyString, wxDATAVIEW_CELL_INERT, 30 );
    m_fileListCtrl->AppendTextColumn( _( "Name" ), wxDATAVIEW_CELL_INERT, 200,
                                       wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_fileListCtrl->AppendTextColumn( _( "Revision" ), wxDATAVIEW_CELL_INERT, 80,
                                       wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_fileListCtrl->AppendTextColumn( _( "State" ), wxDATAVIEW_CELL_INERT, 100,
                                       wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_fileListCtrl->AppendTextColumn( _( "Lock Owner" ), wxDATAVIEW_CELL_INERT, 120,
                                       wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_fileListCtrl->AppendTextColumn( _( "Modified" ), wxDATAVIEW_CELL_INERT, 150,
                                       wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );

    fileListSizer->Add( m_fileListCtrl, 1, wxEXPAND | wxALL, 5 );

    // Pagination controls
    wxBoxSizer* paginationSizer = new wxBoxSizer( wxHORIZONTAL );

    m_prevPageBtn = new wxButton( m_fileListPanel, ID_PREV_PAGE, _( "< Prev" ) );
    m_prevPageBtn->Enable( false );
    paginationSizer->Add( m_prevPageBtn, 0, wxALL, 2 );

    m_pageLabel = new wxStaticText( m_fileListPanel, wxID_ANY, _( "Page 1 of 1" ) );
    paginationSizer->Add( m_pageLabel, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

    m_nextPageBtn = new wxButton( m_fileListPanel, ID_NEXT_PAGE, _( "Next >" ) );
    m_nextPageBtn->Enable( false );
    paginationSizer->Add( m_nextPageBtn, 0, wxALL, 2 );

    paginationSizer->AddStretchSpacer();
    fileListSizer->Add( paginationSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5 );

    m_fileListPanel->SetSizer( fileListSizer );

    // Create a container panel for preview and properties
    wxPanel* previewPropertiesContainer = new wxPanel( m_rightSplitter, wxID_ANY );
    wxBoxSizer* containerSizer = new wxBoxSizer( wxVERTICAL );

    // Preview panel with image display and zoom controls
    m_previewPanel = new wxPanel( previewPropertiesContainer, wxID_ANY );
    wxBoxSizer* previewSizer = new wxBoxSizer( wxVERTICAL );

    // Preview image (centered)
    m_previewImage = new wxStaticBitmap( m_previewPanel, wxID_ANY, wxNullBitmap,
                                          wxDefaultPosition, wxDefaultSize, wxBORDER_SUNKEN );
    previewSizer->Add( m_previewImage, 1, wxEXPAND | wxALL, 5 );

    // Label for "Loading..." or "No preview" messages
    m_previewLabel = new wxStaticText( m_previewPanel, wxID_ANY, _( "Select a file to preview" ),
                                        wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL );
    m_previewLabel->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
    previewSizer->Add( m_previewLabel, 0, wxEXPAND | wxALL, 5 );

    // Zoom controls
    wxBoxSizer* zoomSizer = new wxBoxSizer( wxHORIZONTAL );

    m_zoomOutBtn = new wxButton( m_previewPanel, ID_ZOOM_OUT, wxS( "-" ), wxDefaultPosition,
                                  wxSize( 30, -1 ) );
    m_zoomOutBtn->SetToolTip( _( "Zoom out" ) );
    zoomSizer->Add( m_zoomOutBtn, 0, wxALL, 2 );

    m_zoomFitBtn = new wxButton( m_previewPanel, ID_ZOOM_FIT, _( "Fit" ) );
    m_zoomFitBtn->SetToolTip( _( "Fit to panel" ) );
    zoomSizer->Add( m_zoomFitBtn, 0, wxALL, 2 );

    m_zoomInBtn = new wxButton( m_previewPanel, ID_ZOOM_IN, wxS( "+" ), wxDefaultPosition,
                                 wxSize( 30, -1 ) );
    m_zoomInBtn->SetToolTip( _( "Zoom in" ) );
    zoomSizer->Add( m_zoomInBtn, 0, wxALL, 2 );

    previewSizer->Add( zoomSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 5 );
    m_previewPanel->SetSizer( previewSizer );

    containerSizer->Add( m_previewPanel, 1, wxEXPAND, 0 );

    // Properties panel with property grid
    m_propertiesPanel = new wxPanel( previewPropertiesContainer, wxID_ANY );
    wxBoxSizer* propertiesSizer = new wxBoxSizer( wxVERTICAL );

    // Properties header with edit/save/cancel buttons
    wxBoxSizer* propHeaderSizer = new wxBoxSizer( wxHORIZONTAL );
    wxStaticText* propLabel = new wxStaticText( m_propertiesPanel, wxID_ANY, _( "Properties" ) );
    propHeaderSizer->Add( propLabel, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    propHeaderSizer->AddStretchSpacer();

    m_editBtn = new wxButton( m_propertiesPanel, ID_PROP_EDIT, _( "Edit" ) );
    m_editBtn->Enable( false );
    propHeaderSizer->Add( m_editBtn, 0, wxALL, 2 );

    m_saveBtn = new wxButton( m_propertiesPanel, ID_PROP_SAVE, _( "Save" ) );
    m_saveBtn->Hide();
    propHeaderSizer->Add( m_saveBtn, 0, wxALL, 2 );

    m_cancelBtn = new wxButton( m_propertiesPanel, ID_PROP_CANCEL, _( "Cancel" ) );
    m_cancelBtn->Hide();
    propHeaderSizer->Add( m_cancelBtn, 0, wxALL, 2 );

    propertiesSizer->Add( propHeaderSizer, 0, wxEXPAND, 0 );

    // Property grid
    m_propertyGrid = new wxPropertyGrid( m_propertiesPanel, wxID_ANY, wxDefaultPosition,
                                          wxDefaultSize, wxPG_SPLITTER_AUTO_CENTER );
    m_propertyGrid->SetExtraStyle( wxPG_EX_HELP_AS_TOOLTIPS );
    propertiesSizer->Add( m_propertyGrid, 1, wxEXPAND | wxALL, 5 );

    m_propertiesPanel->SetSizer( propertiesSizer );
    containerSizer->Add( m_propertiesPanel, 1, wxEXPAND, 0 );

    previewPropertiesContainer->SetSizer( containerSizer );

    m_rightSplitter->SplitHorizontally( m_fileListPanel, previewPropertiesContainer, -300 );

    m_mainSplitter->SplitVertically( m_leftSplitter, m_rightSplitter, 300 );

    // Add main splitter to panel sizer
    wxBoxSizer* panelSizer = new wxBoxSizer( wxVERTICAL );
    panelSizer->Add( m_mainSplitter, 1, wxEXPAND, 0 );
    m_mainPanel->SetSizer( panelSizer );
    m_mainPanel->Layout();

    SetMinSize( wxSize( 1200, 800 ) );
    Layout();
}


bool DIALOG_PLM_BROWSER::Connect()
{
    if( !m_plugin )
        return false;

    if( m_connected )
        return true;

    m_folderTree->DeleteAllItems();
    m_folderCache.clear();

    // Show search panel only if the plugin supports search
    bool supportsSearch = m_plugin->SupportsSearch();
    m_searchPanel->Show( supportsSearch );
    m_fileListPanel->Layout();

    // Build advanced search and where-used panels if supported
    if( supportsSearch && !m_advSearchPane )
    {
        BuildAdvancedSearchPanel();
    }

    if( !m_whereUsedTree )
    {
        BuildWhereUsedTab();
    }

    LoadRootFolders();
    m_connected = true;

    m_statusBar->SetStatusText( _( "Connected" ), 0 );
    return true;
}


void DIALOG_PLM_BROWSER::Disconnect()
{
    if( !m_connected )
        return;

    m_folderTree->DeleteAllItems();
    m_folderCache.clear();
    m_fileListCtrl->DeleteAllItems();
    m_currentFolderId.Clear();
    m_currentFileResult = PLM_LIST_RESULT<PLM_FILE_INFO>();
    m_currentListOptions = PLM_LIST_OPTIONS::FirstPage();
    UpdatePaginationControls();
    ClearPreview();
    m_connected = false;

    m_statusBar->SetStatusText( _( "Disconnected" ), 0 );
}


void DIALOG_PLM_BROWSER::RefreshCurrentFolder()
{
    wxTreeItemId selection = m_folderTree->GetSelection();

    if( !selection.IsOk() )
        return;

    PLM_TREE_ITEM_DATA* data = static_cast<PLM_TREE_ITEM_DATA*>(
            m_folderTree->GetItemData( selection ) );

    if( !data )
        return;

    // Clear and reload children
    m_folderTree->DeleteChildren( selection );
    LoadChildFolders( selection );
}


wxString DIALOG_PLM_BROWSER::GetSelectedFolderId() const
{
    wxTreeItemId selection = m_folderTree->GetSelection();

    if( !selection.IsOk() )
        return wxEmptyString;

    PLM_TREE_ITEM_DATA* data = static_cast<PLM_TREE_ITEM_DATA*>(
            m_folderTree->GetItemData( selection ) );

    if( !data )
        return wxEmptyString;

    return data->GetFolder().id;
}


void DIALOG_PLM_BROWSER::LoadRootFolders()
{
    if( !m_plugin )
        return;

    std::vector<PLM_FOLDER> rootFolders = m_plugin->GetRootFolders();

    if( rootFolders.empty() )
    {
        m_statusBar->SetStatusText( _( "No root folders found" ), 0 );
        return;
    }

    wxTreeItemId root = m_folderTree->AddRoot( _( "PLM" ), 0, 0 );

    for( const PLM_FOLDER& folder : rootFolders )
    {
        m_folderCache[folder.id] = folder;

        wxTreeItemId item = m_folderTree->AppendItem( root, folder.name, 0, 1,
                                                      new PLM_TREE_ITEM_DATA( folder ) );

        // Add a dummy child so the expand button shows
        if( folder.has_children )
            m_folderTree->AppendItem( item, _( "Loading..." ) );
    }

    m_folderTree->Expand( root );
    m_statusBar->SetStatusText( wxString::Format( _( "Loaded %zu folders" ), rootFolders.size() ),
                                0 );
}


void DIALOG_PLM_BROWSER::LoadChildFolders( wxTreeItemId aItem )
{
    if( !aItem.IsOk() || !m_plugin )
        return;

    PLM_TREE_ITEM_DATA* data = static_cast<PLM_TREE_ITEM_DATA*>(
            m_folderTree->GetItemData( aItem ) );

    if( !data )
        return;

    if( data->ChildrenLoaded() )
        return;

    // Delete the placeholder "Loading..." item
    m_folderTree->DeleteChildren( aItem );

    std::vector<PLM_FOLDER> children = m_plugin->GetChildFolders( data->GetFolder().id );

    for( const PLM_FOLDER& folder : children )
    {
        m_folderCache[folder.id] = folder;

        wxTreeItemId childItem = m_folderTree->AppendItem( aItem, folder.name, 0, 1,
                                                           new PLM_TREE_ITEM_DATA( folder ) );

        // Add a dummy child so the expand button shows
        if( folder.has_children )
            m_folderTree->AppendItem( childItem, _( "Loading..." ) );
    }

    data->SetChildrenLoaded();
}


void DIALOG_PLM_BROWSER::PrefetchSiblings( wxTreeItemId aItem )
{
    if( !aItem.IsOk() )
        return;

    wxTreeItemId parent = m_folderTree->GetItemParent( aItem );

    if( !parent.IsOk() )
        return;

    // Prefetch previous and next siblings
    wxTreeItemId sibling = m_folderTree->GetPrevSibling( aItem );

    if( sibling.IsOk() )
    {
        PLM_TREE_ITEM_DATA* sibData = static_cast<PLM_TREE_ITEM_DATA*>(
                m_folderTree->GetItemData( sibling ) );

        if( sibData && !sibData->ChildrenLoaded() )
            LoadChildFolders( sibling );
    }

    sibling = m_folderTree->GetNextSibling( aItem );

    if( sibling.IsOk() )
    {
        PLM_TREE_ITEM_DATA* sibData = static_cast<PLM_TREE_ITEM_DATA*>(
                m_folderTree->GetItemData( sibling ) );

        if( sibData && !sibData->ChildrenLoaded() )
            LoadChildFolders( sibling );
    }
}


void DIALOG_PLM_BROWSER::OnTreeItemExpanding( wxTreeEvent& aEvent )
{
    wxTreeItemId item = aEvent.GetItem();

    if( !item.IsOk() )
        return;

    LoadChildFolders( item );
    PrefetchSiblings( item );
}


void DIALOG_PLM_BROWSER::OnTreeSelChanged( wxTreeEvent& aEvent )
{
    wxTreeItemId item = aEvent.GetItem();

    if( !item.IsOk() )
        return;

    PLM_TREE_ITEM_DATA* data = static_cast<PLM_TREE_ITEM_DATA*>(
            m_folderTree->GetItemData( item ) );

    if( !data )
        return;

    // Exit search mode when selecting a folder
    if( m_searchMode )
    {
        m_searchMode = false;
        m_lastSearchQuery.Clear();
        m_searchCtrl->Clear();
        m_clearSearchBtn->Hide();
        m_searchPanel->Layout();
    }

    m_statusBar->SetStatusText( data->GetFolder().name, 1 );

    // Load files for the selected folder
    LoadFilesForFolder( data->GetFolder().id );
}


void DIALOG_PLM_BROWSER::OnFileListColumnHeaderClick( wxDataViewEvent& aEvent )
{
    int col = aEvent.GetColumn();

    if( col < 0 )
        return;

    // Map column index to sort field
    wxString sortField;

    switch( col )
    {
    case COL_NAME:       sortField = wxS( "name" );           break;
    case COL_REVISION:   sortField = wxS( "revision" );       break;
    case COL_STATE:      sortField = wxS( "state" );          break;
    case COL_LOCK_OWNER: sortField = wxS( "lock_owner" );     break;
    case COL_MODIFIED:   sortField = wxS( "modified" );       break;
    default:             return;  // Icon column not sortable
    }

    // Toggle sort direction if clicking same column
    if( m_currentListOptions.sort_field == sortField )
        m_currentListOptions.sort_ascending = !m_currentListOptions.sort_ascending;
    else
        m_currentListOptions.sort_ascending = true;

    m_currentListOptions.sort_field = sortField;
    m_currentListOptions.offset = 0;  // Reset to first page

    LoadFilesForFolder( m_currentFolderId );
}


void DIALOG_PLM_BROWSER::OnFileListSelChanged( wxDataViewEvent& aEvent )
{
    int row = m_fileListCtrl->GetSelectedRow();

    if( row < 0 || static_cast<size_t>( row ) >= m_currentFileResult.items.size() )
    {
        ClearPreview();
        ClearProperties();
        return;
    }

    const PLM_FILE_INFO& file = m_currentFileResult.items[row];
    m_statusBar->SetStatusText( file.name, 1 );

    // Load preview and properties for the selected file
    LoadPreview( file.id );
    LoadProperties( file.id );
}


void DIALOG_PLM_BROWSER::OnPrevPage( wxCommandEvent& aEvent )
{
    if( !m_currentFileResult.HasPreviousPage() )
        return;

    m_currentListOptions = m_currentListOptions.PreviousPage();

    if( m_searchMode && !m_lastSearchQuery.IsEmpty() )
    {
        m_currentFileResult = m_plugin->SearchFiles( m_lastSearchQuery, m_currentListOptions );
        PopulateFileList( m_currentFileResult );
        UpdatePaginationControls();
    }
    else
    {
        LoadFilesForFolder( m_currentFolderId );
    }
}


void DIALOG_PLM_BROWSER::OnNextPage( wxCommandEvent& aEvent )
{
    if( !m_currentFileResult.HasNextPage() )
        return;

    m_currentListOptions = m_currentListOptions.NextPage( m_currentFileResult.items.size() );

    if( m_searchMode && !m_lastSearchQuery.IsEmpty() )
    {
        m_currentFileResult = m_plugin->SearchFiles( m_lastSearchQuery, m_currentListOptions );
        PopulateFileList( m_currentFileResult );
        UpdatePaginationControls();
    }
    else
    {
        LoadFilesForFolder( m_currentFolderId );
    }
}


void DIALOG_PLM_BROWSER::OnClose( wxCloseEvent& aEvent )
{
    Disconnect();
    aEvent.Skip();
}


void DIALOG_PLM_BROWSER::LoadFilesForFolder( const wxString& aFolderId )
{
    if( aFolderId.IsEmpty() || !m_plugin )
        return;

    m_currentFolderId = aFolderId;
    m_currentFileResult = m_plugin->ListFiles( aFolderId, m_currentListOptions );

    PopulateFileList( m_currentFileResult );
    UpdatePaginationControls();

    m_statusBar->SetStatusText(
            wxString::Format( _( "%zu files" ), m_currentFileResult.items.size() ), 0 );
}


void DIALOG_PLM_BROWSER::PopulateFileList( const PLM_LIST_RESULT<PLM_FILE_INFO>& aResult )
{
    m_fileListCtrl->DeleteAllItems();

    for( const PLM_FILE_INFO& file : aResult.items )
    {
        wxVector<wxVariant> data;

        // Icon column (placeholder for now)
        data.push_back( wxVariant( wxEmptyString ) );
        data.push_back( wxVariant( file.name ) );
        data.push_back( wxVariant( file.revision ) );
        data.push_back( wxVariant( FormatStateCategory( file.state_category ) ) );
        data.push_back( wxVariant( file.lock_owner ) );
        data.push_back( wxVariant( FormatTimestamp( file.modified_timestamp ) ) );

        m_fileListCtrl->AppendItem( data );
    }
}


void DIALOG_PLM_BROWSER::UpdatePaginationControls()
{
    m_prevPageBtn->Enable( m_currentFileResult.HasPreviousPage() );
    m_nextPageBtn->Enable( m_currentFileResult.HasNextPage() );

    wxString pageText = wxString::Format( _( "Page %d of %d" ),
                                           m_currentFileResult.CurrentPage(),
                                           m_currentFileResult.TotalPages() );
    m_pageLabel->SetLabel( pageText );
}


wxString DIALOG_PLM_BROWSER::FormatTimestamp( int64_t aTimestamp ) const
{
    if( aTimestamp == 0 )
        return wxEmptyString;

    wxDateTime dt( static_cast<time_t>( aTimestamp ) );

    if( !dt.IsValid() )
        return wxEmptyString;

    return dt.Format( wxS( "%Y-%m-%d %H:%M" ) );
}


wxString DIALOG_PLM_BROWSER::FormatStateCategory( PLM_STATE_CATEGORY aState ) const
{
    switch( aState )
    {
    case PLM_STATE_CATEGORY::EDITABLE:       return _( "Editable" );
    case PLM_STATE_CATEGORY::LOCKED:         return _( "Locked" );
    case PLM_STATE_CATEGORY::RELEASED:       return _( "Released" );
    case PLM_STATE_CATEGORY::PENDING_REVIEW: return _( "Pending Review" );
    default:                                 return _( "Unknown" );
    }
}


void DIALOG_PLM_BROWSER::LoadPreview( const wxString& aFileId )
{
    if( aFileId.IsEmpty() || !m_plugin )
    {
        ShowNoPreview();
        return;
    }

    if( aFileId == m_currentPreviewFileId && m_currentPreviewBitmap.IsOk() )
        return;

    ShowPreviewLoading();

    // Try to get preview from the plugin's preview cache
    PLM_PREVIEW_CACHE* previewCache = m_plugin->GetPreviewCache();

    if( !previewCache )
    {
        ShowNoPreview();
        return;
    }

    wxBitmap preview;

    if( previewCache->GetPreview( aFileId, preview ) && preview.IsOk() )
    {
        m_currentPreviewBitmap = preview;
        m_currentPreviewFileId = aFileId;
        m_previewZoomLevel = 100;

        // Scale to fit
        wxSize panelSize = m_previewImage->GetSize();

        if( panelSize.GetWidth() > 0 && panelSize.GetHeight() > 0 )
        {
            wxBitmap scaled = ScalePreviewBitmap( preview, panelSize.GetWidth(),
                                                   panelSize.GetHeight() );
            m_previewImage->SetBitmap( scaled );
        }
        else
        {
            m_previewImage->SetBitmap( preview );
        }

        m_previewLabel->SetLabel( wxEmptyString );
        m_previewPanel->Layout();
    }
    else
    {
        ShowNoPreview();
    }
}


void DIALOG_PLM_BROWSER::ClearPreview()
{
    m_currentPreviewBitmap = wxNullBitmap;
    m_currentPreviewFileId.Clear();
    m_previewZoomLevel = 100;
    m_previewImage->SetBitmap( wxNullBitmap );
    m_previewLabel->SetLabel( _( "Select a file to preview" ) );
    m_previewPanel->Layout();
}


void DIALOG_PLM_BROWSER::ShowPreviewLoading()
{
    m_previewImage->SetBitmap( wxNullBitmap );
    m_previewLabel->SetLabel( _( "Loading preview..." ) );
    m_previewPanel->Layout();
}


void DIALOG_PLM_BROWSER::ShowNoPreview()
{
    m_currentPreviewBitmap = wxNullBitmap;
    m_currentPreviewFileId.Clear();
    m_previewImage->SetBitmap( wxNullBitmap );
    m_previewLabel->SetLabel( _( "No preview available" ) );
    m_previewPanel->Layout();
}


wxBitmap DIALOG_PLM_BROWSER::ScalePreviewBitmap( const wxBitmap& aBitmap, int aMaxWidth,
                                                  int aMaxHeight ) const
{
    if( !aBitmap.IsOk() || aMaxWidth <= 0 || aMaxHeight <= 0 )
        return aBitmap;

    int srcWidth = aBitmap.GetWidth();
    int srcHeight = aBitmap.GetHeight();

    if( srcWidth <= aMaxWidth && srcHeight <= aMaxHeight )
        return aBitmap;

    // Calculate scale factor maintaining aspect ratio
    double scaleX = static_cast<double>( aMaxWidth ) / srcWidth;
    double scaleY = static_cast<double>( aMaxHeight ) / srcHeight;
    double scale = std::min( scaleX, scaleY );

    int newWidth = static_cast<int>( srcWidth * scale );
    int newHeight = static_cast<int>( srcHeight * scale );

    if( newWidth <= 0 || newHeight <= 0 )
        return aBitmap;

    wxImage image = aBitmap.ConvertToImage();
    image.Rescale( newWidth, newHeight, wxIMAGE_QUALITY_HIGH );
    return wxBitmap( image );
}


void DIALOG_PLM_BROWSER::OnZoomIn( wxCommandEvent& aEvent )
{
    if( !m_currentPreviewBitmap.IsOk() )
        return;

    m_previewZoomLevel = std::min( m_previewZoomLevel + 25, 400 );

    int newWidth = ( m_currentPreviewBitmap.GetWidth() * m_previewZoomLevel ) / 100;
    int newHeight = ( m_currentPreviewBitmap.GetHeight() * m_previewZoomLevel ) / 100;

    if( newWidth > 0 && newHeight > 0 )
    {
        wxImage image = m_currentPreviewBitmap.ConvertToImage();
        image.Rescale( newWidth, newHeight, wxIMAGE_QUALITY_HIGH );
        m_previewImage->SetBitmap( wxBitmap( image ) );
        m_previewPanel->Layout();
    }
}


void DIALOG_PLM_BROWSER::OnZoomOut( wxCommandEvent& aEvent )
{
    if( !m_currentPreviewBitmap.IsOk() )
        return;

    m_previewZoomLevel = std::max( m_previewZoomLevel - 25, 25 );

    int newWidth = ( m_currentPreviewBitmap.GetWidth() * m_previewZoomLevel ) / 100;
    int newHeight = ( m_currentPreviewBitmap.GetHeight() * m_previewZoomLevel ) / 100;

    if( newWidth > 0 && newHeight > 0 )
    {
        wxImage image = m_currentPreviewBitmap.ConvertToImage();
        image.Rescale( newWidth, newHeight, wxIMAGE_QUALITY_HIGH );
        m_previewImage->SetBitmap( wxBitmap( image ) );
        m_previewPanel->Layout();
    }
}


void DIALOG_PLM_BROWSER::OnZoomFit( wxCommandEvent& aEvent )
{
    if( !m_currentPreviewBitmap.IsOk() )
        return;

    m_previewZoomLevel = 100;

    wxSize panelSize = m_previewImage->GetSize();

    if( panelSize.GetWidth() > 0 && panelSize.GetHeight() > 0 )
    {
        wxBitmap scaled = ScalePreviewBitmap( m_currentPreviewBitmap, panelSize.GetWidth(),
                                               panelSize.GetHeight() );
        m_previewImage->SetBitmap( scaled );
        m_previewPanel->Layout();
    }
}


void DIALOG_PLM_BROWSER::LoadProperties( const wxString& aFileId )
{
    if( aFileId.IsEmpty() || !m_plugin )
    {
        ClearProperties();
        return;
    }

    m_currentPropertiesFileId = aFileId;

    // Fetch the schema and attributes from the plugin
    m_cachedSchema = m_plugin->GetSchema();
    PLM_ATTRIBUTE_SET attributes = m_plugin->GetAttributes( aFileId );

    PopulatePropertyGrid( m_cachedSchema, attributes );

    // Enable edit button if there are editable attributes
    bool hasEditable = !m_cachedSchema.GetEditableAttributes().empty();
    m_editBtn->Enable( hasEditable && !m_editMode );
}


void DIALOG_PLM_BROWSER::ClearProperties()
{
    m_propertyGrid->Clear();
    m_currentPropertiesFileId.Clear();
    m_editedAttributes = PLM_ATTRIBUTE_SET();
    m_editBtn->Enable( false );

    if( m_editMode )
        ExitEditMode( false );
}


void DIALOG_PLM_BROWSER::PopulatePropertyGrid( const PLM_SCHEMA& aSchema,
                                                const PLM_ATTRIBUTE_SET& aAttributes )
{
    m_propertyGrid->Clear();

    // Sort attributes by display order
    std::vector<const PLM_ATTRIBUTE_SCHEMA*> sortedAttrs;

    for( const auto& attr : aSchema.attributes )
        sortedAttrs.push_back( &attr );

    std::sort( sortedAttrs.begin(), sortedAttrs.end(),
               []( const PLM_ATTRIBUTE_SCHEMA* a, const PLM_ATTRIBUTE_SCHEMA* b )
               {
                   return a->display_order < b->display_order;
               } );

    for( const PLM_ATTRIBUTE_SCHEMA* attrSchema : sortedAttrs )
    {
        // Get the current value
        const PLM_ATTRIBUTE_VALUE* val = aAttributes.FindValue( attrSchema->attr_id );
        wxString currentValue = val ? val->value : attrSchema->default_value;

        wxPGProperty* prop = nullptr;

        switch( attrSchema->type )
        {
        case PLM_ATTR_TYPE::STRING:
            prop = new wxStringProperty( attrSchema->display_name, attrSchema->attr_id,
                                          currentValue );
            break;

        case PLM_ATTR_TYPE::INTEGER:
        {
            long intVal = 0;
            currentValue.ToLong( &intVal );
            prop = new wxIntProperty( attrSchema->display_name, attrSchema->attr_id, intVal );
            break;
        }

        case PLM_ATTR_TYPE::FLOAT:
        {
            double floatVal = 0.0;
            currentValue.ToDouble( &floatVal );
            prop = new wxFloatProperty( attrSchema->display_name, attrSchema->attr_id, floatVal );
            break;
        }

        case PLM_ATTR_TYPE::BOOLEAN:
        {
            bool boolVal = ( currentValue.CmpNoCase( wxS( "true" ) ) == 0 ||
                             currentValue == wxS( "1" ) );
            prop = new wxBoolProperty( attrSchema->display_name, attrSchema->attr_id, boolVal );
            prop->SetAttribute( wxPG_BOOL_USE_CHECKBOX, true );
            break;
        }

        case PLM_ATTR_TYPE::DATE:
            prop = new wxStringProperty( attrSchema->display_name, attrSchema->attr_id,
                                          currentValue );
            break;

        case PLM_ATTR_TYPE::ENUM:
        {
            wxArrayString choices;

            for( const wxString& enumVal : attrSchema->enum_values )
                choices.Add( enumVal );

            int selection = choices.Index( currentValue );

            if( selection == wxNOT_FOUND )
                selection = 0;

            prop = new wxEnumProperty( attrSchema->display_name, attrSchema->attr_id, choices,
                                        wxArrayInt(), selection );
            break;
        }

        case PLM_ATTR_TYPE::DATETIME:
            prop = new wxStringProperty( attrSchema->display_name, attrSchema->attr_id,
                                          currentValue );
            break;

        case PLM_ATTR_TYPE::USER_REFERENCE:
            prop = new wxStringProperty( attrSchema->display_name, attrSchema->attr_id,
                                          currentValue );
            break;
        }

        if( prop )
        {
            // Set help text as tooltip
            if( !attrSchema->description.IsEmpty() )
                prop->SetHelpString( attrSchema->description );

            // Mark required properties
            if( attrSchema->required )
            {
                wxString label = prop->GetLabel();
                prop->SetLabel( label + wxS( " *" ) );
            }

            // Disable non-editable properties or all if not in edit mode
            if( !attrSchema->editable_in_kicad || !m_editMode )
                prop->Enable( false );

            m_propertyGrid->Append( prop );
        }
    }

    m_propertyGrid->Refresh();
}


void DIALOG_PLM_BROWSER::EnterEditMode()
{
    if( m_currentPropertiesFileId.IsEmpty() )
        return;

    m_editMode = true;

    // Store current values for potential cancel
    m_editedAttributes = m_plugin->GetAttributes( m_currentPropertiesFileId );

    // Enable editable properties
    for( wxPropertyGridIterator it = m_propertyGrid->GetIterator(); !it.AtEnd(); it++ )
    {
        wxPGProperty* prop = *it;
        const PLM_ATTRIBUTE_SCHEMA* attrSchema = m_cachedSchema.FindAttribute( prop->GetName() );

        if( attrSchema && attrSchema->editable_in_kicad )
            prop->Enable( true );
    }

    // Update button visibility
    m_editBtn->Hide();
    m_saveBtn->Show();
    m_cancelBtn->Show();
    m_propertiesPanel->Layout();
}


void DIALOG_PLM_BROWSER::ExitEditMode( bool aSave )
{
    if( aSave && !m_currentPropertiesFileId.IsEmpty() && m_editedAttributes.HasModifications() )
    {
        // Validate before saving
        std::vector<wxString> errors = m_plugin->ValidateAttributes( m_editedAttributes );

        if( !errors.empty() )
        {
            wxString errorMsg = _( "Cannot save properties:\n" );

            for( const wxString& err : errors )
                errorMsg += wxS( "\n- " ) + err;

            wxMessageBox( errorMsg, _( "Validation Error" ), wxOK | wxICON_WARNING, this );
            return;
        }

        // Save to the plugin
        if( !m_plugin->SetAttributes( m_currentPropertiesFileId, m_editedAttributes ) )
        {
            wxMessageBox( _( "Failed to save properties." ), _( "Error" ), wxOK | wxICON_ERROR,
                          this );
            return;
        }

        m_statusBar->SetStatusText( _( "Properties saved" ), 0 );
    }

    m_editMode = false;
    m_editedAttributes = PLM_ATTRIBUTE_SET();

    // Disable all properties
    for( wxPropertyGridIterator it = m_propertyGrid->GetIterator(); !it.AtEnd(); it++ )
    {
        wxPGProperty* prop = *it;
        prop->Enable( false );
    }

    // Update button visibility
    m_editBtn->Show();
    m_editBtn->Enable( !m_cachedSchema.GetEditableAttributes().empty() );
    m_saveBtn->Hide();
    m_cancelBtn->Hide();
    m_propertiesPanel->Layout();

    // Reload properties to discard any unsaved changes
    if( !aSave && !m_currentPropertiesFileId.IsEmpty() )
        LoadProperties( m_currentPropertiesFileId );
}


void DIALOG_PLM_BROWSER::OnPropertyEdit( wxCommandEvent& aEvent )
{
    EnterEditMode();
}


void DIALOG_PLM_BROWSER::OnPropertySave( wxCommandEvent& aEvent )
{
    ExitEditMode( true );
}


void DIALOG_PLM_BROWSER::OnPropertyCancel( wxCommandEvent& aEvent )
{
    ExitEditMode( false );
}


void DIALOG_PLM_BROWSER::OnPropertyChanged( wxPropertyGridEvent& aEvent )
{
    if( !m_editMode )
        return;

    wxPGProperty* prop = aEvent.GetProperty();

    if( !prop )
        return;

    wxString attrId = prop->GetName();
    wxString newValue;

    // Convert property value to string based on type
    const PLM_ATTRIBUTE_SCHEMA* attrSchema = m_cachedSchema.FindAttribute( attrId );

    if( !attrSchema )
        return;

    switch( attrSchema->type )
    {
    case PLM_ATTR_TYPE::INTEGER:
        newValue = wxString::Format( wxS( "%ld" ), prop->GetValue().GetLong() );
        break;

    case PLM_ATTR_TYPE::FLOAT:
        newValue = wxString::Format( wxS( "%g" ), prop->GetValue().GetDouble() );
        break;

    case PLM_ATTR_TYPE::BOOLEAN:
        newValue = prop->GetValue().GetBool() ? wxS( "true" ) : wxS( "false" );
        break;

    case PLM_ATTR_TYPE::ENUM:
    {
        int selection = prop->GetValue().GetInteger();
        wxEnumProperty* enumProp = dynamic_cast<wxEnumProperty*>( prop );

        if( enumProp && selection >= 0 &&
            static_cast<size_t>( selection ) < attrSchema->enum_values.size() )
        {
            newValue = attrSchema->enum_values[selection];
        }

        break;
    }

    default:
        newValue = prop->GetValueAsString();
        break;
    }

    m_editedAttributes.SetValue( attrId, newValue );
}


void DIALOG_PLM_BROWSER::OnSearchText( wxCommandEvent& aEvent )
{
    wxString query = m_searchCtrl->GetValue();

    if( !query.IsEmpty() )
        PerformSearch( query );
}


void DIALOG_PLM_BROWSER::OnSearchButton( wxCommandEvent& aEvent )
{
    wxString query = m_searchCtrl->GetValue();

    if( !query.IsEmpty() )
        PerformSearch( query );
}


void DIALOG_PLM_BROWSER::OnClearSearch( wxCommandEvent& aEvent )
{
    ClearSearch();
}


void DIALOG_PLM_BROWSER::PerformSearch( const wxString& aQuery )
{
    if( aQuery.IsEmpty() || !m_plugin || !m_plugin->SupportsSearch() )
        return;

    m_searchMode = true;
    m_lastSearchQuery = aQuery;

    // Deselect any folder in the tree since we're searching globally
    m_folderTree->UnselectAll();

    // Clear current folder context
    m_currentFolderId.Clear();

    // Reset pagination for search
    m_currentListOptions = PLM_LIST_OPTIONS::FirstPage();

    // Perform the search
    m_currentFileResult = m_plugin->SearchFiles( aQuery, m_currentListOptions );

    PopulateFileList( m_currentFileResult );
    UpdatePaginationControls();

    // Show clear button
    m_clearSearchBtn->Show();
    m_searchPanel->Layout();

    m_statusBar->SetStatusText( wxString::Format( _( "Search: %zu results for '%s'" ),
                                                   m_currentFileResult.items.size(), aQuery ), 0 );
}


void DIALOG_PLM_BROWSER::ClearSearch()
{
    m_searchMode = false;
    m_lastSearchQuery.Clear();
    m_searchCtrl->Clear();

    // Hide clear button
    m_clearSearchBtn->Hide();
    m_searchPanel->Layout();

    // Clear file list
    m_fileListCtrl->DeleteAllItems();
    m_currentFileResult = PLM_LIST_RESULT<PLM_FILE_INFO>();
    m_currentListOptions = PLM_LIST_OPTIONS::FirstPage();
    UpdatePaginationControls();

    ClearPreview();
    ClearProperties();

    m_statusBar->SetStatusText( _( "Select a folder to browse" ), 0 );
}


void DIALOG_PLM_BROWSER::BuildAdvancedSearchPanel()
{
    if( !m_plugin || !m_plugin->SupportsSearch() )
        return;

    m_searchFields = m_plugin->GetSearchFields();

    if( m_searchFields.empty() )
        return;

    wxBoxSizer* fileListSizer = dynamic_cast<wxBoxSizer*>( m_fileListPanel->GetSizer() );

    if( !fileListSizer )
        return;

    m_advSearchPane = new wxCollapsiblePane( m_fileListPanel, wxID_ANY,
                                             _( "Advanced Search Filters" ) );
    m_advSearchContent = new wxPanel( m_advSearchPane->GetPane() );

    wxFlexGridSizer* filterSizer = new wxFlexGridSizer( 2, 10, 5 );
    filterSizer->AddGrowableCol( 1 );

    for( const auto& field : m_searchFields )
    {
        wxStaticText* label = new wxStaticText( m_advSearchContent, wxID_ANY,
                                                field.display_name + wxS( ":" ) );
        filterSizer->Add( label, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );

        wxWindow* control = nullptr;

        switch( field.field_type )
        {
        case PLM_SEARCH_FIELD_TYPE::TEXT:
        case PLM_SEARCH_FIELD_TYPE::NUMBER:
        case PLM_SEARCH_FIELD_TYPE::USER:
        {
            wxTextCtrl* textCtrl = new wxTextCtrl( m_advSearchContent, wxID_ANY );
            control = textCtrl;
            break;
        }

        case PLM_SEARCH_FIELD_TYPE::DATE:
        case PLM_SEARCH_FIELD_TYPE::DATE_RANGE:
        {
            wxTextCtrl* dateCtrl = new wxTextCtrl( m_advSearchContent, wxID_ANY );
            dateCtrl->SetHint( _( "YYYY-MM-DD" ) );
            control = dateCtrl;
            break;
        }

        case PLM_SEARCH_FIELD_TYPE::DROPDOWN:
        {
            wxChoice* choice = new wxChoice( m_advSearchContent, wxID_ANY );
            choice->Append( wxEmptyString );

            for( const auto& opt : field.options )
            {
                choice->Append( opt );
            }

            choice->SetSelection( 0 );
            control = choice;
            break;
        }

        case PLM_SEARCH_FIELD_TYPE::CHECKBOX:
        {
            wxCheckBox* checkbox = new wxCheckBox( m_advSearchContent, wxID_ANY, wxEmptyString );
            control = checkbox;
            break;
        }

        default:
        {
            wxTextCtrl* textCtrl = new wxTextCtrl( m_advSearchContent, wxID_ANY );
            control = textCtrl;
            break;
        }
        }

        filterSizer->Add( control, 1, wxEXPAND );
        m_filterControls.push_back( { label, control, field } );
    }

    m_advSearchContent->SetSizer( filterSizer );

    wxBoxSizer* paneSizer = new wxBoxSizer( wxVERTICAL );
    paneSizer->Add( m_advSearchContent, 1, wxEXPAND | wxALL, 5 );

    wxBoxSizer* advBtnSizer = new wxBoxSizer( wxHORIZONTAL );
    advBtnSizer->AddStretchSpacer();
    m_advClearBtn = new wxButton( m_advSearchPane->GetPane(), ID_ADV_CLEAR, _( "Clear" ) );
    m_advSearchBtn = new wxButton( m_advSearchPane->GetPane(), ID_ADV_SEARCH, _( "Search" ) );
    advBtnSizer->Add( m_advClearBtn, 0, wxRIGHT, 5 );
    advBtnSizer->Add( m_advSearchBtn, 0 );

    paneSizer->Add( advBtnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5 );

    m_advSearchPane->GetPane()->SetSizer( paneSizer );

    // Insert after the search panel
    fileListSizer->Insert( 1, m_advSearchPane, 0, wxEXPAND | wxLEFT | wxRIGHT, 5 );

    m_fileListPanel->Layout();
}


void DIALOG_PLM_BROWSER::BuildWhereUsedTab()
{
    if( !m_plugin )
        return;

    // We replace the simple file list panel with a notebook containing
    // a "Files" tab and a "Where Used" tab. Since we already built the
    // file list, we just wrap it in a notebook structure.

    wxBoxSizer* fileListSizer = dynamic_cast<wxBoxSizer*>( m_fileListPanel->GetSizer() );

    if( !fileListSizer )
        return;

    // Add a "Where Used" button next to the search clear button
    m_whereUsedBtn = new wxButton( m_searchPanel, ID_WHERE_USED, _( "Where Used" ) );
    m_whereUsedBtn->SetToolTip( _( "Show which projects and BOMs reference the selected part" ) );

    wxBoxSizer* searchSizer = dynamic_cast<wxBoxSizer*>( m_searchPanel->GetSizer() );

    if( searchSizer )
    {
        searchSizer->Add( m_whereUsedBtn, 0, wxALL, 2 );
        m_searchPanel->Layout();
    }

    // Create the where-used panel below the file list (as a separate panel)
    m_whereUsedTabPanel = new wxPanel( m_fileListPanel );
    wxBoxSizer* wuSizer = new wxBoxSizer( wxVERTICAL );

    wxStaticText* wuLabel = new wxStaticText( m_whereUsedTabPanel, wxID_ANY,
                                              _( "Where Used Results:" ) );
    wuLabel->SetFont( wuLabel->GetFont().Bold() );
    wuSizer->Add( wuLabel, 0, wxALL, 5 );

    m_whereUsedTree = new wxTreeCtrl( m_whereUsedTabPanel, wxID_ANY, wxDefaultPosition,
                                      wxDefaultSize,
                                      wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_HIDE_ROOT );
    wuSizer->Add( m_whereUsedTree, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5 );

    m_whereUsedTabPanel->SetSizer( wuSizer );
    m_whereUsedTabPanel->Hide();

    fileListSizer->Add( m_whereUsedTabPanel, 1, wxEXPAND );
    m_fileListPanel->Layout();
}


void DIALOG_PLM_BROWSER::OnAdvancedSearch( wxCommandEvent& aEvent )
{
    PerformAdvancedSearch();
}


void DIALOG_PLM_BROWSER::OnClearAdvancedSearch( wxCommandEvent& aEvent )
{
    for( auto& fc : m_filterControls )
    {
        if( wxTextCtrl* textCtrl = dynamic_cast<wxTextCtrl*>( fc.control ) )
        {
            textCtrl->Clear();
        }
        else if( wxChoice* choice = dynamic_cast<wxChoice*>( fc.control ) )
        {
            choice->SetSelection( 0 );
        }
        else if( wxCheckBox* checkbox = dynamic_cast<wxCheckBox*>( fc.control ) )
        {
            checkbox->SetValue( false );
        }
    }
}


void DIALOG_PLM_BROWSER::PerformAdvancedSearch()
{
    if( !m_plugin || !m_plugin->SupportsSearch() )
        return;

    std::map<wxString, wxString> filters;

    for( const auto& fc : m_filterControls )
    {
        wxString value;

        if( wxTextCtrl* textCtrl = dynamic_cast<wxTextCtrl*>( fc.control ) )
        {
            value = textCtrl->GetValue().Trim();
        }
        else if( wxChoice* choice = dynamic_cast<wxChoice*>( fc.control ) )
        {
            if( choice->GetSelection() > 0 )
                value = choice->GetStringSelection();
        }
        else if( wxCheckBox* checkbox = dynamic_cast<wxCheckBox*>( fc.control ) )
        {
            if( checkbox->GetValue() )
                value = wxS( "true" );
        }

        if( !value.IsEmpty() )
        {
            filters[fc.field.field_id] = value;
        }
    }

    if( filters.empty() )
    {
        m_statusBar->SetStatusText( _( "Enter at least one search filter" ), 0 );
        return;
    }

    m_searchMode = true;
    m_folderTree->UnselectAll();
    m_currentFolderId.Clear();
    m_currentListOptions = PLM_LIST_OPTIONS::FirstPage();

    m_currentFileResult = m_plugin->SearchWithFilters( filters, m_currentListOptions );

    PopulateFileList( m_currentFileResult );
    UpdatePaginationControls();

    m_clearSearchBtn->Show();
    m_searchPanel->Layout();

    m_statusBar->SetStatusText( wxString::Format( _( "Advanced search: %zu results" ),
                                                   m_currentFileResult.items.size() ), 0 );
}


void DIALOG_PLM_BROWSER::OnWhereUsed( wxCommandEvent& aEvent )
{
    wxString partId = GetSelectedFilePartId();

    if( partId.IsEmpty() )
    {
        m_statusBar->SetStatusText( _( "Select a file to view where-used references" ), 0 );
        return;
    }

    LoadWhereUsed( partId );
}


void DIALOG_PLM_BROWSER::OnResultsTabChanged( wxBookCtrlEvent& aEvent )
{
    aEvent.Skip();
}


void DIALOG_PLM_BROWSER::LoadWhereUsed( const wxString& aPartId )
{
    if( !m_plugin || aPartId.IsEmpty() )
        return;

    std::vector<PLM_WHERE_USED_ENTRY> entries = m_plugin->GetWhereUsed( aPartId );

    if( m_whereUsedTabPanel )
    {
        m_whereUsedTabPanel->Show();
        PopulateWhereUsedTree( entries );
        m_fileListPanel->Layout();
    }

    m_statusBar->SetStatusText( wxString::Format( _( "Where used: %zu references found" ),
                                                   entries.size() ), 0 );
}


void DIALOG_PLM_BROWSER::PopulateWhereUsedTree( const std::vector<PLM_WHERE_USED_ENTRY>& aEntries )
{
    if( !m_whereUsedTree )
        return;

    m_whereUsedTree->DeleteAllItems();

    wxTreeItemId root = m_whereUsedTree->AddRoot( wxS( "References" ) );

    // Group entries by parent type
    std::map<wxString, std::vector<const PLM_WHERE_USED_ENTRY*>> grouped;

    for( const auto& entry : aEntries )
    {
        wxString type = entry.parent_type.IsEmpty() ? wxString( wxS( "Other" ) )
                                                    : entry.parent_type;
        grouped[type].push_back( &entry );
    }

    for( const auto& [type, entries] : grouped )
    {
        wxTreeItemId typeNode = m_whereUsedTree->AppendItem( root,
                wxString::Format( wxS( "%s (%zu)" ), type, entries.size() ) );

        for( const auto* entry : entries )
        {
            wxString label = entry->parent_name;

            if( entry->quantity > 0.0 )
            {
                label += wxString::Format( wxS( " (qty: %.0f)" ), entry->quantity );
            }

            if( !entry->reference.IsEmpty() )
            {
                label += wxString::Format( wxS( " [%s]" ), entry->reference );
            }

            m_whereUsedTree->AppendItem( typeNode, label );
        }

        m_whereUsedTree->Expand( typeNode );
    }

    if( aEntries.empty() )
    {
        m_whereUsedTree->AppendItem( root, _( "No references found" ) );
    }
}


wxString DIALOG_PLM_BROWSER::GetSelectedFilePartId() const
{
    int row = m_fileListCtrl->GetSelectedRow();

    if( row == wxNOT_FOUND || row >= static_cast<int>( m_currentFileResult.items.size() ) )
        return wxEmptyString;

    const PLM_FILE_INFO& fileInfo = m_currentFileResult.items[row];
    return fileInfo.id;
}
