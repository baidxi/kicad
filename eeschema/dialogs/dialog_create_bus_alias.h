/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
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

#ifndef DIALOG_CREATE_BUS_ALIAS_H
#define DIALOG_CREATE_BUS_ALIAS_H

#include <dialog_shim.h>
#include <vector>
#include <wx/string.h>

class SCH_EDIT_FRAME;
class wxListBox;
class wxTextCtrl;


class DIALOG_CREATE_BUS_ALIAS : public DIALOG_SHIM
{
public:
    DIALOG_CREATE_BUS_ALIAS( SCH_EDIT_FRAME* aParent, const std::vector<wxString>& aMemberNets );

    wxString GetAliasName() const;

    const std::vector<wxString>& GetMembers() const { return m_members; }

    bool TransferDataFromWindow() override;

private:
    SCH_EDIT_FRAME*       m_frame;
    wxTextCtrl*           m_nameCtrl;
    wxListBox*            m_membersList;
    std::vector<wxString> m_members;
};

#endif // DIALOG_CREATE_BUS_ALIAS_H
