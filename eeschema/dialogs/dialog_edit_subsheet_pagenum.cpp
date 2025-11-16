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

#include <dialogs/dialog_edit_subsheet_pagenum.h>
#include <sch_edit_frame.h>
#include <validators.h>

static wxString g_setToLastName;
static int      g_setToLastOrientation = 0;
static int      g_setToLastPosition = 0;
static int      g_setToLastAction = 0;

class SCH_EDIT_FRAME;

DIALOG_EDIT_SUBSHEET_PAGENUM::DIALOG_EDIT_SUBSHEET_PAGENUM( SCH_EDIT_FRAME* aParent ) :
        DIALOG_EDIT_SUBSHEET_PAGENUM_BASE( aParent ), m_frame( aParent )
{
    if( g_setToLastName.IsEmpty() )
    {
        m_checkDefaultName->SetValue( true );
        m_textName->SetValue( _( "Page" ) );
    }
    else
    {
        m_checkDefaultName->SetValue( false );
        m_textName->SetValue( g_setToLastName );
    }

    m_radioOrientation->SetSelection( g_setToLastOrientation );
    m_radioPosition->SetSelection( g_setToLastPosition );
    m_radioAction->SetSelection( g_setToLastAction );

    // Hide all labels
    for( uint i = 0; i < m_radioOrientation->GetCount(); i++ )
        m_radioOrientation->SetString(i, wxT( "" ) );

    // Create update event
    wxCommandEvent update;
    update.SetInt( m_checkDefaultName->GetValue() );
    OnChangedDefaultName( update );

    SetupStandardButtons();
    finishDialogSettings();
}


DIALOG_EDIT_SUBSHEET_PAGENUM::~DIALOG_EDIT_SUBSHEET_PAGENUM()
{
    if( m_checkDefaultName->GetValue() == wxCHK_CHECKED )
        g_setToLastName.Clear();
    else
        g_setToLastName = m_textName->GetValue();

    g_setToLastOrientation = m_radioOrientation->GetSelection();
    g_setToLastPosition = m_radioPosition->GetSelection();
    g_setToLastAction = m_radioAction->GetSelection();
}


void DIALOG_EDIT_SUBSHEET_PAGENUM::OnChangedDefaultName( wxCommandEvent& event )
{
    m_textName->Enable( !event.IsChecked() );
}


bool DIALOG_EDIT_SUBSHEET_PAGENUM::TransferDataFromWindow()
{
    m_pageFieldName = ( m_checkDefaultName->GetValue() ? _( "Page" ) : m_textName->GetValue() );
    m_orientation = m_radioOrientation->GetSelection();
    m_position = m_radioPosition->GetSelection();
    m_action = m_radioAction->GetSelection();

    return true;
}
