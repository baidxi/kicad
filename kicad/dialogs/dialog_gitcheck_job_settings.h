/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2025 Dmitry Rezvanov <dmitry.rezvanov@yandex.ru>
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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <dialog_shim.h>

class JOB_SPECIAL_GIT_CHECK;

class DIALOG_GITCHECK_JOB_SETTINGS : public DIALOG_SHIM
{
public:
    DIALOG_GITCHECK_JOB_SETTINGS( wxWindow* aParent, JOB_SPECIAL_GIT_CHECK* aJob );
    ~DIALOG_GITCHECK_JOB_SETTINGS();

    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;

private:
    void onOK( wxCommandEvent& aEvent );

    JOB_SPECIAL_GIT_CHECK* m_job;
    class wxCheckBox* m_cbRequireCommit;
    class wxCheckBox* m_cbRequirePush;
};
