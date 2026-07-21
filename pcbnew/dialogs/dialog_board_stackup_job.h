/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2026 Krishna Swaroop <krishna.swaroop@pixxel.co.in>
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

#pragma once

#include <dialog_shim.h>
#include <jobs/job_export_pcb_stackup.h>

#include <vector>

class wxCheckBox;
class wxChoice;
class wxTextCtrl;

class DIALOG_BOARD_STACKUP_JOB : public DIALOG_SHIM
{
public:
    DIALOG_BOARD_STACKUP_JOB( wxWindow* aParent, JOB_EXPORT_PCB_STACKUP* aJob );

    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;

protected:
    void OnFormatChoice( wxCommandEvent& aEvent );

    JOB_EXPORT_PCB_STACKUP::OUTPUT_FORMAT getSelectedFormat();
    void                                  setSelectedFormat( JOB_EXPORT_PCB_STACKUP::OUTPUT_FORMAT aFormat );

    struct FIELD_TOGGLE
    {
        wxString    label;
        bool*       jobFlag;
        wxCheckBox* checkbox = nullptr;
    };

    JOB_EXPORT_PCB_STACKUP*   m_job;
    wxTextCtrl*               m_textCtrlOutputPath;
    wxChoice*                 m_choiceFormat;
    wxChoice*                 m_choiceUnits;
    std::vector<FIELD_TOGGLE> m_fieldToggles;
};
