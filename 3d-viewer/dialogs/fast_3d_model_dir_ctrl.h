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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef FAST_3D_MODEL_DIR_CTRL_H
#define FAST_3D_MODEL_DIR_CTRL_H

#include <wx/dirctrl.h>


class FAST_3D_MODEL_DIR_CTRL : public wxGenericDirCtrl
{
public:
    FAST_3D_MODEL_DIR_CTRL( wxWindow* aParent, wxWindowID aId = wxID_ANY,
                            const wxString& aDir = wxEmptyString,
                            const wxPoint& aPos = wxDefaultPosition,
                            const wxSize& aSize = wxDefaultSize,
                            long aStyle = wxDIRCTRL_DEFAULT_STYLE,
                            const wxString& aFilter = wxEmptyString,
                            int aDefaultFilter = 0,
                            const wxString& aName = wxASCII_STR( wxTreeCtrlNameStr ) );

    void SetupSections() override;
};

#endif
