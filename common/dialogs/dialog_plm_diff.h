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

#ifndef DIALOG_PLM_DIFF_H
#define DIALOG_PLM_DIFF_H

#include <dialogs/dialog_plm_diff_base.h>


class DIALOG_PLM_DIFF : public DIALOG_PLM_DIFF_BASE
{
public:
    DIALOG_PLM_DIFF( wxWindow* aParent );
    ~DIALOG_PLM_DIFF() override;

    void SetRevisions( const wxString& aLeftRev, const wxString& aRightRev );
    void SetChangeCount( int aCurrent, int aTotal );
    int  GetViewMode() const;
    void NavigateToChange( int aIndex );

protected:
    void OnViewModeChanged( wxCommandEvent& aEvent ) override;
    void OnPrevChange( wxCommandEvent& aEvent ) override;
    void OnNextChange( wxCommandEvent& aEvent ) override;

private:
    int m_currentChange;
    int m_totalChanges;

    void updateNavigationButtons();
};

#endif  // DIALOG_PLM_DIFF_H
