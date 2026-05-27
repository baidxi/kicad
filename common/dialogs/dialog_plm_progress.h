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

/**
 * @file dialog_plm_progress.h
 * @brief Progress dialog for long-running PLM operations.
 */

#ifndef DIALOG_PLM_PROGRESS_H
#define DIALOG_PLM_PROGRESS_H

#include <dialog_shim.h>
#include <wx/gauge.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/timer.h>


/**
 * Progress dialog for PLM operations.
 *
 * Provides a modal dialog that displays progress for long-running PLM operations
 * such as file uploads, downloads, or synchronization. Supports both determinate
 * (percentage-based) and indeterminate (pulse) progress modes.
 *
 * Thread safety: The Update methods use wxThreadEvent to safely post updates
 * from background threads to the UI thread.
 */
class DIALOG_PLM_PROGRESS : public DIALOG_SHIM
{
public:
    /**
     * Construct the progress dialog.
     *
     * @param aParent Parent window.
     * @param aTitle Dialog title.
     * @param aCancellable True if operation can be cancelled.
     */
    DIALOG_PLM_PROGRESS( wxWindow* aParent, const wxString& aTitle, bool aCancellable = true );

    ~DIALOG_PLM_PROGRESS() override;

    /**
     * Set the status message.
     *
     * Thread-safe: Can be called from background threads.
     *
     * @param aMessage Status message to display.
     */
    void SetStatus( const wxString& aMessage );

    /**
     * Set the progress value (determinate mode).
     *
     * Thread-safe: Can be called from background threads.
     *
     * @param aCurrent Current progress value.
     * @param aTotal Total progress value (100 for percentage).
     */
    void SetProgress( int aCurrent, int aTotal = 100 );

    /**
     * Switch to indeterminate progress mode.
     *
     * Shows a pulsing progress bar for operations of unknown duration.
     */
    void SetIndeterminate();

    /**
     * Switch to determinate progress mode.
     *
     * @param aTotal Total progress value for the range.
     */
    void SetDeterminate( int aTotal = 100 );

    /**
     * Check if the user has requested cancellation.
     *
     * Thread-safe: Can be called from background threads.
     *
     * @return True if cancel was requested.
     */
    bool IsCancelled() const { return m_cancelled; }

    /**
     * Signal that the operation is complete.
     *
     * Thread-safe: Can be called from background threads.
     * The dialog will close automatically when this is called.
     */
    void SetComplete();

    /**
     * Signal that the operation failed.
     *
     * Thread-safe: Can be called from background threads.
     * Shows an error message and enables the close button.
     *
     * @param aErrorMessage Error message to display.
     */
    void SetError( const wxString& aErrorMessage );

    /**
     * Check if the operation is complete.
     *
     * @return True if SetComplete() or SetError() was called.
     */
    bool IsComplete() const { return m_complete; }

protected:
    void OnCancelButton( wxCommandEvent& aEvent );
    void OnCloseWindow( wxCloseEvent& aEvent );
    void OnProgressUpdate( wxThreadEvent& aEvent );
    void OnTimer( wxTimerEvent& aEvent );

private:
    void BuildLayout();
    void UpdateUI();

    wxStaticText* m_statusLabel;
    wxGauge*      m_progressGauge;
    wxButton*     m_cancelBtn;
    wxTimer       m_pulseTimer;

    bool          m_cancellable;
    bool          m_cancelled;
    bool          m_complete;
    bool          m_indeterminate;
    int           m_progressValue;
    int           m_progressMax;
    wxString      m_pendingStatus;
    bool          m_pendingUpdate;

    wxDECLARE_EVENT_TABLE();
};


#endif // DIALOG_PLM_PROGRESS_H
