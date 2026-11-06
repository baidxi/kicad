/**
 * @file dialog_symbol_merge_conflict.h
 */

/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef DIALOG_SYMBOL_MERGE_CONFLICT_H
#define DIALOG_SYMBOL_MERGE_CONFLICT_H

#include <dialog_shim.h>

#include <map>
#include <vector>

#include <wx/string.h>

class SYMBOL_EDIT_FRAME;
class SYMBOL_PREVIEW_WIDGET;
class wxListCtrl;
class wxListEvent;
class wxStaticText;


/**
 * A symbol name that is offered by more than one library taking part in a merge
 * (either by two source libraries, or by a source library and the target library).
 */
struct SYMBOL_MERGE_CONFLICT
{
    wxString              symbolName;
    std::vector<wxString> providers; ///< Nicknames of the libraries offering the symbol
};


/**
 * Dialog shown when merging symbol libraries produces symbol name conflicts.
 *
 * Conflicts are handled one symbol at a time.  All versions of the current
 * symbol (including the one already in the target library) are listed; single
 * click previews a version, double click selects it as the version to keep
 * (marked with a check mark and changeable at any time).  With more than one
 * conflict, the buttons below the list step through the conflicts.
 */
class DIALOG_SYMBOL_MERGE_CONFLICTS : public DIALOG_SHIM
{
public:
    DIALOG_SYMBOL_MERGE_CONFLICTS( wxWindow* aParent, SYMBOL_EDIT_FRAME* aFrame, const wxString& aTargetLibrary,
                                   const std::vector<SYMBOL_MERGE_CONFLICT>& aConflicts );

    /**
     * The conflict decisions: symbol name -> nickname of the library whose
     * version is kept in the target library.
     */
    const std::map<wxString, wxString>& GetChoices() const { return m_choices; }

private:
    // Single click: preview the clicked version
    void onListSelect( wxListEvent& aEvent );

    // Double click: choose the clicked version (check mark)
    void onListItemActivated( wxListEvent& aEvent );

    // Step through the conflicts
    void onPrevConflict( wxCommandEvent& aEvent );
    void onNextConflict( wxCommandEvent& aEvent );
    void onOk( wxCommandEvent& aEvent );

    int currentRow() const;

    /// Show conflict number aIndex (0-based) in the list and preview
    void loadConflict( int aIndex );

    /// Refresh the check marks of the current conflict from #m_choices
    void refreshChecks();

    /// Show the symbol version of the given row in the preview
    void previewRow( int aRow );

    /// Update the navigation buttons for the current conflict index
    void updateNavControls();

    SYMBOL_EDIT_FRAME* m_frame;
    wxString           m_targetLibrary;

    std::vector<SYMBOL_MERGE_CONFLICT> m_conflicts;
    std::map<wxString, wxString>       m_choices; ///< name -> kept library
    int                                m_currentConflict;

    wxStaticText*          m_titleLabel;
    wxListCtrl*            m_conflictList;
    SYMBOL_PREVIEW_WIDGET* m_preview;
    wxStaticText*          m_previewLabel;
    wxButton*              m_prevBtn;
    wxButton*              m_nextBtn;
};

#endif // DIALOG_SYMBOL_MERGE_CONFLICT_H
