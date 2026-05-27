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
 * @file dialog_plm_browser.h
 * @brief PLM Browser dialog for browsing and managing PLM files.
 */

#ifndef DIALOG_PLM_BROWSER_H
#define DIALOG_PLM_BROWSER_H

#include "dialog_plm_browser_base.h"

#include <plm/plm_file.h>
#include <plm/plm_navigation.h>
#include <plm/plm_parts.h>
#include <plm/plm_plugin.h>
#include <plm/plm_preview_cache.h>
#include <plm/plm_schema.h>

#include <wx/collpane.h>
#include <wx/dataview.h>
#include <wx/notebook.h>
#include <wx/propgrid/propgrid.h>
#include <wx/srchctrl.h>
#include <wx/splitter.h>
#include <wx/statbmp.h>
#include <wx/treectrl.h>

#include <map>
#include <memory>
#include <vector>


class PLM_PLUGIN;


/**
 * Tree item data for PLM folder nodes.
 *
 * Associates tree items with PLM folder information for lazy loading.
 */
class PLM_TREE_ITEM_DATA : public wxTreeItemData
{
public:
    PLM_TREE_ITEM_DATA( const PLM_FOLDER& aFolder ) :
            m_folder( aFolder ),
            m_childrenLoaded( false )
    {
    }

    const PLM_FOLDER& GetFolder() const { return m_folder; }
    bool ChildrenLoaded() const { return m_childrenLoaded; }
    void SetChildrenLoaded() { m_childrenLoaded = true; }

private:
    PLM_FOLDER m_folder;
    bool       m_childrenLoaded;
};


/**
 * PLM Browser dialog implementation.
 *
 * Provides a browser interface for navigating PLM folder structures and
 * managing PLM-controlled files. The dialog has a four-panel layout:
 * - Left: Folder tree view
 * - Center: File list for selected folder
 * - Right: Preview/properties panel
 * - Bottom: Activity/status panel
 */
class DIALOG_PLM_BROWSER : public DIALOG_PLM_BROWSER_BASE
{
public:
    DIALOG_PLM_BROWSER( wxWindow* aParent, PLM_PLUGIN* aPlugin );
    ~DIALOG_PLM_BROWSER();

    /**
     * Connect to the PLM server and load the root folders.
     *
     * @return True if connection and initial load succeeded.
     */
    bool Connect();

    /**
     * Disconnect from the PLM server.
     */
    void Disconnect();

    /**
     * Refresh the currently selected folder.
     */
    void RefreshCurrentFolder();

    /**
     * Get the currently selected folder ID.
     *
     * @return Selected folder ID, or empty if none selected.
     */
    wxString GetSelectedFolderId() const;

protected:
    /**
     * Build the four-panel UI layout.
     */
    void BuildLayout();

    /**
     * Load root folders into the tree.
     */
    void LoadRootFolders();

    /**
     * Load children for a tree node (lazy loading).
     *
     * @param aItem Tree item to load children for.
     */
    void LoadChildFolders( wxTreeItemId aItem );

    /**
     * Prefetch sibling folders for smooth scrolling.
     *
     * @param aItem Reference item to prefetch around.
     */
    void PrefetchSiblings( wxTreeItemId aItem );

    /**
     * Load files for the specified folder into the file list.
     *
     * @param aFolderId Folder ID to load files from.
     */
    void LoadFilesForFolder( const wxString& aFolderId );

    /**
     * Update the file list with a result set.
     *
     * @param aResult Paginated result from ListFiles.
     */
    void PopulateFileList( const PLM_LIST_RESULT<PLM_FILE_INFO>& aResult );

    /**
     * Update pagination controls based on current result.
     */
    void UpdatePaginationControls();

    /**
     * Format a timestamp for display.
     *
     * @param aTimestamp Unix epoch timestamp.
     * @return Formatted date/time string.
     */
    wxString FormatTimestamp( int64_t aTimestamp ) const;

    /**
     * Get a display string for the state category.
     *
     * @param aState State category to format.
     * @return Human-readable state string.
     */
    wxString FormatStateCategory( PLM_STATE_CATEGORY aState ) const;

    /**
     * Load and display preview for the specified file.
     *
     * @param aFileId PLM file identifier.
     */
    void LoadPreview( const wxString& aFileId );

    /**
     * Clear the preview display.
     */
    void ClearPreview();

    /**
     * Show the loading indicator in the preview panel.
     */
    void ShowPreviewLoading();

    /**
     * Show the "no preview" fallback message.
     */
    void ShowNoPreview();

    /**
     * Scale a bitmap to fit within the preview area while maintaining aspect ratio.
     *
     * @param aBitmap Source bitmap.
     * @param aMaxWidth Maximum width.
     * @param aMaxHeight Maximum height.
     * @return Scaled bitmap.
     */
    wxBitmap ScalePreviewBitmap( const wxBitmap& aBitmap, int aMaxWidth, int aMaxHeight ) const;

    /**
     * Load properties for the specified file into the property grid.
     *
     * @param aFileId PLM file identifier.
     */
    void LoadProperties( const wxString& aFileId );

    /**
     * Clear the property grid.
     */
    void ClearProperties();

    /**
     * Populate the property grid from schema and attribute values.
     *
     * @param aSchema The PLM schema.
     * @param aAttributes Current attribute values.
     */
    void PopulatePropertyGrid( const PLM_SCHEMA& aSchema, const PLM_ATTRIBUTE_SET& aAttributes );

    /**
     * Enter edit mode for properties.
     */
    void EnterEditMode();

    /**
     * Exit edit mode, optionally saving changes.
     *
     * @param aSave True to save changes, false to discard.
     */
    void ExitEditMode( bool aSave );

    // Event handlers
    void OnTreeItemExpanding( wxTreeEvent& aEvent );
    void OnTreeSelChanged( wxTreeEvent& aEvent );
    void OnFileListColumnHeaderClick( wxDataViewEvent& aEvent );
    void OnFileListSelChanged( wxDataViewEvent& aEvent );
    void OnPrevPage( wxCommandEvent& aEvent );
    void OnNextPage( wxCommandEvent& aEvent );
    void OnZoomIn( wxCommandEvent& aEvent );
    void OnZoomOut( wxCommandEvent& aEvent );
    void OnZoomFit( wxCommandEvent& aEvent );
    void OnPropertyEdit( wxCommandEvent& aEvent );
    void OnPropertySave( wxCommandEvent& aEvent );
    void OnPropertyCancel( wxCommandEvent& aEvent );
    void OnPropertyChanged( wxPropertyGridEvent& aEvent );
    void OnSearchText( wxCommandEvent& aEvent );
    void OnSearchButton( wxCommandEvent& aEvent );
    void OnClearSearch( wxCommandEvent& aEvent );
    void OnClose( wxCloseEvent& aEvent );

    // Advanced search and where-used handlers
    void OnAdvancedSearch( wxCommandEvent& aEvent );
    void OnClearAdvancedSearch( wxCommandEvent& aEvent );
    void OnWhereUsed( wxCommandEvent& aEvent );
    void OnResultsTabChanged( wxBookCtrlEvent& aEvent );

    /**
     * Enter search mode and perform a search.
     *
     * @param aQuery Search query string.
     */
    void PerformSearch( const wxString& aQuery );

    /**
     * Exit search mode and return to folder browsing.
     */
    void ClearSearch();

    /**
     * Build the advanced search filter panel.
     */
    void BuildAdvancedSearchPanel();

    /**
     * Build the where-used results tab.
     */
    void BuildWhereUsedTab();

    /**
     * Perform an advanced search using structured field filters.
     */
    void PerformAdvancedSearch();

    /**
     * Load where-used results for the currently selected file.
     *
     * @param aPartId Part ID to search where-used references.
     */
    void LoadWhereUsed( const wxString& aPartId );

    /**
     * Populate the where-used tree with results.
     */
    void PopulateWhereUsedTree( const std::vector<PLM_WHERE_USED_ENTRY>& aEntries );

    /**
     * Get the selected file's part ID from the file list.
     */
    wxString GetSelectedFilePartId() const;

private:
    PLM_PLUGIN*       m_plugin;
    bool              m_connected;

    // UI panels
    wxSplitterWindow* m_mainSplitter;
    wxSplitterWindow* m_leftSplitter;
    wxSplitterWindow* m_rightSplitter;
    wxTreeCtrl*       m_folderTree;
    wxPanel*          m_fileListPanel;
    wxPanel*          m_previewPanel;
    wxPanel*          m_activityPanel;

    // File list controls
    wxDataViewListCtrl* m_fileListCtrl;
    wxButton*           m_prevPageBtn;
    wxButton*           m_nextPageBtn;
    wxStaticText*       m_pageLabel;

    // Preview controls
    wxStaticBitmap*   m_previewImage;
    wxStaticText*     m_previewLabel;
    wxButton*         m_zoomInBtn;
    wxButton*         m_zoomOutBtn;
    wxButton*         m_zoomFitBtn;
    wxBitmap          m_currentPreviewBitmap;
    wxString          m_currentPreviewFileId;
    int               m_previewZoomLevel;

    // Properties panel controls
    wxPropertyGrid*   m_propertyGrid;
    wxPanel*          m_propertiesPanel;
    wxButton*         m_editBtn;
    wxButton*         m_saveBtn;
    wxButton*         m_cancelBtn;
    bool              m_editMode;
    wxString          m_currentPropertiesFileId;
    PLM_SCHEMA        m_cachedSchema;
    PLM_ATTRIBUTE_SET m_editedAttributes;

    // Pagination state
    PLM_LIST_OPTIONS                  m_currentListOptions;
    PLM_LIST_RESULT<PLM_FILE_INFO>    m_currentFileResult;
    wxString                          m_currentFolderId;

    // Search controls
    wxPanel*          m_searchPanel;
    wxSearchCtrl*     m_searchCtrl;
    wxButton*         m_clearSearchBtn;
    bool              m_searchMode;
    wxString          m_lastSearchQuery;

    // Folder cache
    std::map<wxString, PLM_FOLDER> m_folderCache;

    // Advanced search controls
    wxCollapsiblePane*  m_advSearchPane;
    wxPanel*            m_advSearchContent;
    wxButton*           m_advSearchBtn;
    wxButton*           m_advClearBtn;

    struct FilterControl
    {
        wxStaticText*      label;
        wxWindow*          control;
        PLM_SEARCH_FIELD   field;
    };

    std::vector<FilterControl>      m_filterControls;
    std::vector<PLM_SEARCH_FIELD>   m_searchFields;

    // Where-used controls
    wxNotebook*         m_resultsNotebook;
    wxPanel*            m_fileListTabPanel;
    wxPanel*            m_whereUsedTabPanel;
    wxTreeCtrl*         m_whereUsedTree;
    wxButton*           m_whereUsedBtn;

    // File list column indices
    enum FileListColumns
    {
        COL_ICON = 0,
        COL_NAME,
        COL_REVISION,
        COL_STATE,
        COL_LOCK_OWNER,
        COL_MODIFIED
    };

    wxDECLARE_EVENT_TABLE();
};


#endif // DIALOG_PLM_BROWSER_H
