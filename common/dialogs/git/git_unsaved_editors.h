/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.TXT for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef GIT_UNSAVED_EDITORS_H
#define GIT_UNSAVED_EDITORS_H

#include <wx/string.h>

class KIWAY;
class wxWindow;

/**
 * Offer to save the editors holding unsaved changes before a Git operation snapshots the
 * working tree.
 *
 * The schematic and board editors keep the user's work in memory until it is saved, so an
 * operation built from the working tree would otherwise capture a stale design, or find
 * nothing to capture at all.  Every entry point that writes the working tree to the
 * repository (commit, amend, the Git check job) asks this so that the answer is always the
 * same three choices, whichever one the user came in through.
 *
 * @param aKiway Kiway used to reach the editor frames.
 * @param aParent Parent window for the prompt.
 * @param aMessageFormat Question to put to the user; it must hold a single %s where the list
 *                       of modified editors is inserted.
 * @param aSaveLabel Label of the button that saves the editors and continues.
 * @param aContinueLabel Label of the button that continues without saving.
 * @return True to go ahead with the operation, false if the user cancelled or an editor
 *         could not be saved.
 */
bool HandleUnsavedEditors( KIWAY& aKiway, wxWindow* aParent, const wxString& aMessageFormat,
                           const wxString& aSaveLabel, const wxString& aContinueLabel );

#endif // GIT_UNSAVED_EDITORS_H
