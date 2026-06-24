/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2018-2024 KiCad Developers, see AUTHORS.txt for contributors.
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

#ifndef DIALOG_EDIT_SUBSHEET_PAGENUM_H
#define DIALOG_EDIT_SUBSHEET_PAGENUM_H

#include <dialogs/dialog_edit_subsheet_pagenum_base.h>
#include <sch_item.h>

class DIALOG_EDIT_SUBSHEET_PAGENUM : public DIALOG_EDIT_SUBSHEET_PAGENUM_BASE
{
public:
    DIALOG_EDIT_SUBSHEET_PAGENUM( SCH_EDIT_FRAME* aParent );
    ~DIALOG_EDIT_SUBSHEET_PAGENUM() override;

    wxString GetFieldName() { return m_pageFieldName; };
    void     SetFieldName( const wxString& aFieldName ) { m_pageFieldName = aFieldName; };

    int  GetOrientation() { return m_orientation; };
    void SetOrientation( const int aOrientation ) { m_orientation = aOrientation; };

    int  GetPosition() { return m_position; };
    void SetPosition( const int aPosition ) { m_position = aPosition; };

    int  GetAction() { return m_action; };
    void SetAction( const int aAction ) { m_action = aAction; };

    virtual void OnChangedDefaultName( wxCommandEvent& event ) override;

private:
    bool TransferDataFromWindow() override;

private:
    SCH_EDIT_FRAME* m_frame;

    wxString m_pageFieldName;
    int      m_orientation = 0;
    int      m_position = 0;
    int      m_action = 0;
};

#endif
