/**
 * @file dialog_migrate_symbols.h
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

#ifndef DIALOG_MIGRATE_SYMBOLS_H
#define DIALOG_MIGRATE_SYMBOLS_H

#include <dialog_shim.h>

#include <kiid.h>
#include <utility>
#include <vector>

class SCH_EDIT_FRAME;
class SCH_SYMBOL;
class wxListCtrl;
class wxListEvent;
class wxStaticText;


/**
 * Modeless dialog listing every symbol definition whose lib_id no longer
 * resolves to a library (legacy cached copies and dangling references),
 * grouped by lib_id with their instance counts.
 *
 * Double-clicking a row selects every instance of that definition and opens
 * the Change Symbol dialog (a true modal child); the list is rescanned and
 * refreshed afterwards so the remaining rows can be migrated one after
 * another.
 */
class DIALOG_MIGRATE_SYMBOLS : public DIALOG_SHIM
{
public:
    DIALOG_MIGRATE_SYMBOLS( SCH_EDIT_FRAME* aFrame );

private:
    void onItemActivated( wxListEvent& aEvent );
    void onClose( wxCommandEvent& aEvent );

    /// Rebuild the list from the current screen contents
    void rescan();

    /// The screen symbols for a row (looked up by id; rows are rebuilt on rescan)
    std::vector<SCH_SYMBOL*> symbolsFromRow( int aRow ) const;

    SCH_EDIT_FRAME* m_frame;
    wxListCtrl*     m_list;
    wxStaticText*   m_statusLabel;

    ///< row -> (lib_id text, instance ids)
    std::vector<std::pair<wxString, std::vector<KIID>>> m_rowGroups;
};

#endif // DIALOG_MIGRATE_SYMBOLS_H
