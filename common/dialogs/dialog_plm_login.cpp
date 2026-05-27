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

#include "dialog_plm_login.h"
#include <plm/plm_credential_store.h>
#include <wx/msgdlg.h>


DIALOG_PLM_LOGIN::DIALOG_PLM_LOGIN( wxWindow* aParent ) :
        DIALOG_PLM_LOGIN_BASE( aParent )
{
    m_sdbSizerOK->SetLabel( _( "Connect" ) );

    m_serverUrlCtrl->SetFocus();

    SetupStandardButtons();
    finishDialogSettings();
}


DIALOG_PLM_LOGIN::~DIALOG_PLM_LOGIN()
{
}


bool DIALOG_PLM_LOGIN::TransferDataFromWindow()
{
    wxString serverUrl = GetServerUrl();
    wxString username = GetUsername();
    wxString password = GetPassword();

    if( serverUrl.IsEmpty() )
    {
        wxMessageBox( _( "Server URL cannot be empty" ), _( "Validation Error" ),
                      wxOK | wxICON_ERROR, this );
        m_serverUrlCtrl->SetFocus();
        return false;
    }

    if( username.IsEmpty() )
    {
        wxMessageBox( _( "Username cannot be empty" ), _( "Validation Error" ),
                      wxOK | wxICON_ERROR, this );
        m_usernameCtrl->SetFocus();
        return false;
    }

    if( IsRememberChecked() )
    {
        auto credStore = PLM_CREDENTIAL_STORE::Create();

        if( credStore && credStore->IsAvailable() )
        {
            PLM_CREDENTIALS creds( serverUrl, username, password );
            auto result = credStore->StoreCredentials( creds );

            if( !result.success )
            {
                wxString msg = wxString::Format(
                        _( "Failed to save credentials: %s\n\nContinue without saving?" ),
                        result.error_message );

                if( wxMessageBox( msg, _( "Credential Storage Error" ),
                                  wxYES_NO | wxICON_WARNING, this ) != wxYES )
                {
                    return false;
                }
            }
        }
        else
        {
            wxString backend = credStore ? credStore->GetBackendName() : _( "Unknown" );
            wxString msg = wxString::Format(
                    _( "Secure credential storage (%s) is not available.\n\nContinue without saving credentials?" ),
                    backend );

            if( wxMessageBox( msg, _( "Credential Storage Unavailable" ),
                              wxYES_NO | wxICON_WARNING, this ) != wxYES )
            {
                return false;
            }
        }
    }

    return true;
}
