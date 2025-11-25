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

#ifndef DIALOG_PLM_LOGIN_H
#define DIALOG_PLM_LOGIN_H

#include "dialog_plm_login_base.h"

class PLM_CREDENTIAL_STORE;


/**
 * Dialog for PLM server login.
 *
 * Provides UI for entering PLM server credentials and optionally saving them
 * to the system's secure credential store. Validates inputs before allowing
 * the user to proceed.
 */
class DIALOG_PLM_LOGIN : public DIALOG_PLM_LOGIN_BASE
{
public:
    /**
     * Construct the PLM login dialog.
     *
     * @param aParent Parent window.
     */
    DIALOG_PLM_LOGIN( wxWindow* aParent );

    ~DIALOG_PLM_LOGIN() override;

    /**
     * Get the server URL entered by the user.
     * @return Server URL string.
     */
    wxString GetServerUrl() const { return m_serverUrlCtrl->GetValue().Trim(); }

    /**
     * Get the username entered by the user.
     * @return Username string.
     */
    wxString GetUsername() const { return m_usernameCtrl->GetValue().Trim(); }

    /**
     * Get the password entered by the user.
     * @return Password string.
     */
    wxString GetPassword() const { return m_passwordCtrl->GetValue(); }

    /**
     * Check if the user wants to remember credentials.
     * @return True if "Remember credentials" is checked.
     */
    bool IsRememberChecked() const { return m_rememberCheckbox->GetValue(); }

    /**
     * Set initial server URL value.
     * @param aUrl Server URL to pre-fill.
     */
    void SetServerUrl( const wxString& aUrl ) { m_serverUrlCtrl->SetValue( aUrl ); }

    /**
     * Set initial username value.
     * @param aUsername Username to pre-fill.
     */
    void SetUsername( const wxString& aUsername ) { m_usernameCtrl->SetValue( aUsername ); }

    /**
     * Set the remember credentials checkbox state.
     * @param aRemember Initial checkbox state.
     */
    void SetRemember( bool aRemember ) { m_rememberCheckbox->SetValue( aRemember ); }

protected:
    bool TransferDataFromWindow() override;
};


#endif // DIALOG_PLM_LOGIN_H
