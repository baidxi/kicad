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

#include <dialogs/git/git_remote_prompts.h>

#include <vector>

#include <wx/richmsgdlg.h>

#include <dialogs/git/dialog_git_credentials.h>
#include <git/git_push_handler.h>
#include <git/kicad_git_common.h>
#include <kiplatform/secrets.h>
#include <widgets/wx_progress_reporters.h>


bool PromptForGitCredentials( wxWindow* aParent, KIGIT_COMMON* aCommon )
{
    DIALOG_GIT_CREDENTIALS dlg( aParent, aCommon->GetRemote(), aCommon->GetConnType(),
                                aCommon->GetUsername(), wxEmptyString );

    if( dlg.ShowModal() != wxID_OK )
        return false;

    aCommon->SetUsername( dlg.GetUsername() );
    aCommon->SetPassword( dlg.GetPassword() );

    if( dlg.GetConnType() == KIGIT_COMMON::GIT_CONN_TYPE::GIT_CONN_SSH && !dlg.GetSSHKey().IsEmpty() )
        aCommon->SetSSHKey( dlg.GetSSHKey() );

    if( dlg.SaveCredentials() )
    {
        KIPLATFORM::SECRETS::StoreSecret( aCommon->GetRemote(), aCommon->GetUsername(),
                                          dlg.GetPassword() );
    }

    aCommon->ClearAuthFailure();
    aCommon->TestedTypes() = 0;
    aCommon->ResetNextKey();
    return true;
}


bool PushWithPrompts( wxWindow* aParent, KIGIT_COMMON* aCommon,
                      const std::function<std::unique_ptr<WX_PROGRESS_REPORTER>()>& aProgressFactory,
                      wxString* aError )
{
    bool force = false;

    while( true )
    {
        aCommon->ClearAuthFailure();

        GIT_PUSH_HANDLER handler( aCommon );

        if( aProgressFactory )
            handler.SetProgressReporter( aProgressFactory() );

        PushResult pushResult = handler.PerformPush( force );

        if( pushResult == PushResult::Success )
            return true;

        if( aCommon->WasAuthFailure() && PromptForGitCredentials( aParent, aCommon ) )
            continue;

        if( pushResult == PushResult::NonFastForward && !force )
        {
            wxString upstream = aCommon->GetUpstreamShorthand();

            wxRichMessageDialog dlg(
                    aParent,
                    wxString::Format( _( "Push rejected: your local branch and %s have diverged.\n\n"
                                         "This usually happens after amending or rewriting a commit "
                                         "that was already pushed.  Force push to overwrite the remote?\n\n"
                                         "Anyone who already pulled the previous version will need to "
                                         "reset their copy." ),
                                      upstream.empty() ? wxString( _( "the remote" ) ) : upstream ),
                    _( "Force Push?" ), wxYES_NO | wxNO_DEFAULT | wxICON_WARNING | wxCENTER );

            dlg.SetYesNoLabels( _( "&Force Push" ), _( "Cancel" ) );

            if( dlg.ShowModal() == wxID_YES )
            {
                force = true;
                continue;
            }
        }

        if( aError )
        {
            // A fresh handler is built for every attempt, so these are the errors of the
            // attempt that just failed rather than an accumulation over the retries.
            aError->clear();

            for( const wxString& error : handler.GetErrorStrings() )
            {
                if( !aError->IsEmpty() )
                    *aError += wxT( "; " );

                *aError += error;
            }
        }

        return false;
    }
}
