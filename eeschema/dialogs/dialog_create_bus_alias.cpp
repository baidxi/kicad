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

#include "dialog_create_bus_alias.h"

#include <confirm.h>
#include <sch_edit_frame.h>
#include <schematic.h>
#include <widgets/ui_common.h>

#include <wx/button.h>
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>


DIALOG_CREATE_BUS_ALIAS::DIALOG_CREATE_BUS_ALIAS( SCH_EDIT_FRAME* aParent,
                                                 const std::vector<wxString>& aMemberNets ) :
        DIALOG_SHIM( aParent, wxID_ANY, _( "Create Bus Alias" ), wxDefaultPosition, wxDefaultSize,
                     wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER ),
        m_frame( aParent ),
        m_nameCtrl( nullptr ),
        m_membersList( nullptr ),
        m_members( aMemberNets )
{
    wxBoxSizer* topSizer = new wxBoxSizer( wxVERTICAL );

    wxStaticText* nameLabel = new wxStaticText( this, wxID_ANY, _( "Alias name:" ) );
    topSizer->Add( nameLabel, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP( 10 ) );

    m_nameCtrl = new wxTextCtrl( this, wxID_ANY );
    topSizer->Add( m_nameCtrl, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP( 10 ) );

    wxStaticText* membersLabel = new wxStaticText( this, wxID_ANY, _( "Members:" ) );
    topSizer->Add( membersLabel, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP( 10 ) );

    m_membersList = new wxListBox( this, wxID_ANY, wxDefaultPosition, FromDIP( wxSize( 280, 160 ) ) );
    m_membersList->SetFont( KIUI::GetInfoFont( this ) );

    for( const wxString& member : m_members )
        m_membersList->Append( member );

    topSizer->Add( m_membersList, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP( 10 ) );

    wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer();
    buttons->AddButton( new wxButton( this, wxID_OK ) );
    buttons->AddButton( new wxButton( this, wxID_CANCEL ) );
    buttons->Realize();
    topSizer->Add( buttons, 0, wxEXPAND | wxALL, FromDIP( 10 ) );

    SetSizer( topSizer );
    SetupStandardButtons();
    finishDialogSettings();

    m_nameCtrl->SetFocus();
}


wxString DIALOG_CREATE_BUS_ALIAS::GetAliasName() const
{
    return m_nameCtrl->GetValue().Strip( wxString::both );
}


bool DIALOG_CREATE_BUS_ALIAS::TransferDataFromWindow()
{
    wxString name = GetAliasName();

    if( name.IsEmpty() )
    {
        DisplayErrorMessage( this, _( "Alias name cannot be empty." ) );
        m_nameCtrl->SetFocus();
        return false;
    }

    if( m_frame->Schematic().GetBusAlias( name ) )
    {
        DisplayErrorMessage( this,
                             wxString::Format( _( "Alias name '%s' already in use." ), name ) );
        m_nameCtrl->SetFocus();
        m_nameCtrl->SelectAll();
        return false;
    }

    if( m_members.empty() )
    {
        DisplayErrorMessage( this, _( "Bus alias must have at least one member." ) );
        return false;
    }

    return true;
}
