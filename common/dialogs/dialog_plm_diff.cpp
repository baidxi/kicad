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

#include <dialogs/dialog_plm_diff.h>


DIALOG_PLM_DIFF::DIALOG_PLM_DIFF( wxWindow* aParent ) :
        DIALOG_PLM_DIFF_BASE( aParent ),
        m_currentChange( 0 ),
        m_totalChanges( 0 )
{
    SetupStandardButtons();

    finishDialogSettings();

    updateNavigationButtons();
}


DIALOG_PLM_DIFF::~DIALOG_PLM_DIFF()
{
}


void DIALOG_PLM_DIFF::SetRevisions( const wxString& aLeftRev, const wxString& aRightRev )
{
    wxString label = wxString::Format( _( "Comparing: %s vs %s" ), aLeftRev, aRightRev );
    m_comparisonLabel->SetLabel( label );
}


void DIALOG_PLM_DIFF::SetChangeCount( int aCurrent, int aTotal )
{
    m_currentChange = aCurrent;
    m_totalChanges = aTotal;

    wxString label = wxString::Format( _( "Change %d of %d" ), aCurrent, aTotal );
    m_changeCountLabel->SetLabel( label );

    updateNavigationButtons();
}


int DIALOG_PLM_DIFF::GetViewMode() const
{
    return m_viewModeChoice->GetSelection();
}


void DIALOG_PLM_DIFF::NavigateToChange( int aIndex )
{
    if( aIndex < 0 || aIndex >= m_totalChanges )
        return;

    m_currentChange = aIndex;
    SetChangeCount( aIndex, m_totalChanges );
}


void DIALOG_PLM_DIFF::OnViewModeChanged( wxCommandEvent& aEvent )
{
    aEvent.Skip();
}


void DIALOG_PLM_DIFF::OnPrevChange( wxCommandEvent& aEvent )
{
    if( m_currentChange > 1 )
    {
        NavigateToChange( m_currentChange - 1 );
    }

    aEvent.Skip();
}


void DIALOG_PLM_DIFF::OnNextChange( wxCommandEvent& aEvent )
{
    if( m_currentChange < m_totalChanges )
    {
        NavigateToChange( m_currentChange + 1 );
    }

    aEvent.Skip();
}


void DIALOG_PLM_DIFF::updateNavigationButtons()
{
    m_prevChangeBtn->Enable( m_currentChange > 1 );
    m_nextChangeBtn->Enable( m_currentChange < m_totalChanges && m_totalChanges > 0 );
}
