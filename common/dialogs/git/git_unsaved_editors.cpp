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

#include <dialogs/git/git_unsaved_editors.h>

#include <vector>

#include <wx/richmsgdlg.h>

#include <confirm.h>
#include <frame_type.h>
#include <kiway.h>
#include <kiway_player.h>
#include <mail_type.h>


bool HandleUnsavedEditors( KIWAY& aKiway, wxWindow* aParent, const wxString& aMessageFormat,
                           const wxString& aSaveLabel, const wxString& aContinueLabel )
{
    struct DIRTY_EDITOR
    {
        FRAME_T  frameType;
        MAIL_T   saveMail;
        wxString label;
    };

    const DIRTY_EDITOR candidates[] = {
        { FRAME_SCH, MAIL_SCH_SAVE, _( "Schematic Editor" ) },
        { FRAME_PCB_EDITOR, MAIL_PCB_SAVE, _( "PCB Editor" ) },
    };

    std::vector<const DIRTY_EDITOR*> dirty;

    for( const DIRTY_EDITOR& d : candidates )
    {
        KIWAY_PLAYER* frame = aKiway.Player( d.frameType, false );

        if( frame && frame->IsContentModified() )
            dirty.push_back( &d );
    }

    if( dirty.empty() )
        return true;

    wxString listed;

    for( const DIRTY_EDITOR* d : dirty )
        listed += wxString::Format( wxS( "\n    • %s" ), d->label );

    wxRichMessageDialog dlg( aParent, wxString::Format( aMessageFormat, listed ),
                             _( "Unsaved Changes" ),
                             wxYES_NO | wxCANCEL | wxYES_DEFAULT | wxICON_WARNING | wxCENTER );

    dlg.SetYesNoCancelLabels( aSaveLabel, aContinueLabel, _( "Cancel" ) );

    int answer = dlg.ShowModal();

    if( answer == wxID_CANCEL )
        return false;

    if( answer != wxID_YES )
        return true;

    for( const DIRTY_EDITOR* d : dirty )
    {
        std::string payload;
        aKiway.ExpressMail( d->frameType, d->saveMail, payload );

        if( payload != "success" )
        {
            DisplayErrorMessage( aParent, wxString::Format( _( "Could not save %s." ), d->label ) );
            return false;
        }
    }

    return true;
}
