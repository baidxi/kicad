/**
 * @file dialog_symbol_lib_merge.h
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

#ifndef DIALOG_SYMBOL_LIB_MERGE_H
#define DIALOG_SYMBOL_LIB_MERGE_H

#include "dialog_symbol_merge_conflict.h"

#include <dialog_shim.h>

#include <map>
#include <set>
#include <vector>

class SYMBOL_EDIT_FRAME;
class wxButton;
class wxCheckListBox;
class wxListBox;
class wxSimplebook;
class wxStaticText;
class wxTextCtrl;


/**
 * A three page wizard that merges the symbols of one or more source libraries
 * into a single target library.
 *
 * Page 1: pick the source libraries (multiple selection).
 * Page 2: pick the target library (single selection, writable libraries only).
 * Page 3: summary and warning that the source libraries will be deleted and
 *         the merge cannot be undone.
 */
class DIALOG_SYMBOL_LIB_MERGE : public DIALOG_SHIM
{
public:
    DIALOG_SYMBOL_LIB_MERGE( SYMBOL_EDIT_FRAME* aParent );

private:
    // Wizard navigation
    void onBack( wxCommandEvent& aEvent );
    void onNext( wxCommandEvent& aEvent );
    void onCancel( wxCommandEvent& aEvent );

    void updateNavButtons();
    void buildTargetLibraryList();
    void buildSummary();

    /**
     * Test whether a library can take part in a merge: it must be writable.
     *
     * The bundled KiCad libraries are flagged read-only, so this check keeps
     * them out of both the source and the target lists.
     */
    bool isMergeableLibrary( const wxString& aNickname );

    /**
     * Fill #m_conflicts with every symbol name offered by more than one library
     * taking part in the merge (source vs source, or source vs target).
     */
    void computeConflicts();

    std::vector<wxString> selectedSourceLibraries() const;
    wxString              selectedTargetLibrary() const;

    /**
     * Copy all symbols from the selected source libraries into the target library.
     *
     * Symbols that already exist in the target library are skipped.  On success the
     * source libraries are removed from the symbol library tables and their files
     * are deleted from disk.
     *
     * @return true if the merge was performed (even if some symbols were skipped).
     */
    bool performMerge();

    /// Delete the library file backing a source library, if it is a local file
    void removeLibraryFile( const wxString& aUri, std::vector<wxString>& aDeleted, std::vector<wxString>& aFailed );

    SYMBOL_EDIT_FRAME* m_frame;

    wxSimplebook*   m_book;
    wxCheckListBox* m_sourceList;
    wxListBox*      m_targetList;
    wxTextCtrl*     m_summary;
    wxButton*       m_backBtn;
    wxButton*       m_nextBtn;

    int m_currentPage;

    /// Conflicting symbol names detected for the current source/target selection
    std::vector<SYMBOL_MERGE_CONFLICT> m_conflicts;

    /// Conflict decisions from the conflict dialog: symbol name -> kept library
    std::map<wxString, wxString> m_conflictChoices;
};

#endif // DIALOG_SYMBOL_LIB_MERGE_H
