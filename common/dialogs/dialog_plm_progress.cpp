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

#include "dialog_plm_progress.h"
#include <wx/sizer.h>
#include <wx/statline.h>


wxDEFINE_EVENT( wxEVT_PLM_PROGRESS_UPDATE, wxThreadEvent );


enum
{
    ID_CANCEL_BTN = wxID_HIGHEST + 1,
    ID_PULSE_TIMER
};


wxBEGIN_EVENT_TABLE( DIALOG_PLM_PROGRESS, DIALOG_SHIM )
    EVT_BUTTON( ID_CANCEL_BTN, DIALOG_PLM_PROGRESS::OnCancelButton )
    EVT_CLOSE( DIALOG_PLM_PROGRESS::OnCloseWindow )
    EVT_THREAD( wxEVT_PLM_PROGRESS_UPDATE, DIALOG_PLM_PROGRESS::OnProgressUpdate )
    EVT_TIMER( ID_PULSE_TIMER, DIALOG_PLM_PROGRESS::OnTimer )
wxEND_EVENT_TABLE()


DIALOG_PLM_PROGRESS::DIALOG_PLM_PROGRESS( wxWindow* aParent, const wxString& aTitle,
                                          bool aCancellable ) :
        DIALOG_SHIM( aParent, wxID_ANY, aTitle, wxDefaultPosition, wxDefaultSize,
                     wxDEFAULT_DIALOG_STYLE ),
        m_statusLabel( nullptr ),
        m_progressGauge( nullptr ),
        m_cancelBtn( nullptr ),
        m_pulseTimer( this, ID_PULSE_TIMER ),
        m_cancellable( aCancellable ),
        m_cancelled( false ),
        m_complete( false ),
        m_indeterminate( false ),
        m_progressValue( 0 ),
        m_progressMax( 100 ),
        m_pendingUpdate( false )
{
    BuildLayout();
    SetMinSize( wxSize( 400, 150 ) );
    Centre( wxBOTH );
}


DIALOG_PLM_PROGRESS::~DIALOG_PLM_PROGRESS()
{
    m_pulseTimer.Stop();
}


void DIALOG_PLM_PROGRESS::BuildLayout()
{
    wxBoxSizer* mainSizer = new wxBoxSizer( wxVERTICAL );

    // Status label
    m_statusLabel = new wxStaticText( this, wxID_ANY, _( "Please wait..." ) );
    mainSizer->Add( m_statusLabel, 0, wxEXPAND | wxALL, 10 );

    // Progress gauge
    m_progressGauge = new wxGauge( this, wxID_ANY, m_progressMax );
    mainSizer->Add( m_progressGauge, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10 );

    // Separator line
    mainSizer->Add( new wxStaticLine( this ), 0, wxEXPAND | wxLEFT | wxRIGHT, 10 );

    // Button sizer
    wxBoxSizer* buttonSizer = new wxBoxSizer( wxHORIZONTAL );
    buttonSizer->AddStretchSpacer();

    m_cancelBtn = new wxButton( this, ID_CANCEL_BTN, _( "Cancel" ) );
    m_cancelBtn->Enable( m_cancellable );
    buttonSizer->Add( m_cancelBtn, 0, wxALL, 5 );

    mainSizer->Add( buttonSizer, 0, wxEXPAND | wxALL, 5 );

    SetSizer( mainSizer );
    mainSizer->Fit( this );
}


void DIALOG_PLM_PROGRESS::SetStatus( const wxString& aMessage )
{
    if( wxThread::IsMain() )
    {
        m_statusLabel->SetLabel( aMessage );
        Layout();
    }
    else
    {
        m_pendingStatus = aMessage;
        m_pendingUpdate = true;

        wxThreadEvent* event = new wxThreadEvent( wxEVT_PLM_PROGRESS_UPDATE );
        wxQueueEvent( this, event );
    }
}


void DIALOG_PLM_PROGRESS::SetProgress( int aCurrent, int aTotal )
{
    m_progressValue = aCurrent;
    m_progressMax = aTotal;

    if( wxThread::IsMain() )
    {
        UpdateUI();
    }
    else
    {
        m_pendingUpdate = true;

        wxThreadEvent* event = new wxThreadEvent( wxEVT_PLM_PROGRESS_UPDATE );
        wxQueueEvent( this, event );
    }
}


void DIALOG_PLM_PROGRESS::SetIndeterminate()
{
    m_indeterminate = true;

    if( wxThread::IsMain() )
    {
        m_progressGauge->Pulse();
        m_pulseTimer.Start( 100 );
    }
    else
    {
        m_pendingUpdate = true;

        wxThreadEvent* event = new wxThreadEvent( wxEVT_PLM_PROGRESS_UPDATE );
        wxQueueEvent( this, event );
    }
}


void DIALOG_PLM_PROGRESS::SetDeterminate( int aTotal )
{
    m_indeterminate = false;
    m_progressMax = aTotal;
    m_progressValue = 0;
    m_pulseTimer.Stop();

    if( wxThread::IsMain() )
    {
        m_progressGauge->SetRange( aTotal );
        m_progressGauge->SetValue( 0 );
    }
    else
    {
        m_pendingUpdate = true;

        wxThreadEvent* event = new wxThreadEvent( wxEVT_PLM_PROGRESS_UPDATE );
        wxQueueEvent( this, event );
    }
}


void DIALOG_PLM_PROGRESS::SetComplete()
{
    m_complete = true;
    m_pulseTimer.Stop();

    if( wxThread::IsMain() )
    {
        EndModal( wxID_OK );
    }
    else
    {
        wxThreadEvent* event = new wxThreadEvent( wxEVT_PLM_PROGRESS_UPDATE );
        wxQueueEvent( this, event );
    }
}


void DIALOG_PLM_PROGRESS::SetError( const wxString& aErrorMessage )
{
    m_complete = true;
    m_pulseTimer.Stop();

    if( wxThread::IsMain() )
    {
        m_statusLabel->SetLabel( aErrorMessage );
        m_statusLabel->SetForegroundColour( *wxRED );
        m_cancelBtn->SetLabel( _( "Close" ) );
        m_cancelBtn->Enable( true );
        m_progressGauge->SetValue( 0 );
        Layout();
    }
    else
    {
        m_pendingStatus = aErrorMessage;
        m_pendingUpdate = true;

        wxThreadEvent* event = new wxThreadEvent( wxEVT_PLM_PROGRESS_UPDATE );
        wxQueueEvent( this, event );
    }
}


void DIALOG_PLM_PROGRESS::OnCancelButton( wxCommandEvent& aEvent )
{
    if( m_complete )
    {
        EndModal( wxID_CANCEL );
    }
    else
    {
        m_cancelled = true;
        m_cancelBtn->Enable( false );
        m_statusLabel->SetLabel( _( "Cancelling..." ) );
    }
}


void DIALOG_PLM_PROGRESS::OnCloseWindow( wxCloseEvent& aEvent )
{
    if( !m_complete && m_cancellable )
    {
        m_cancelled = true;
        aEvent.Veto();
    }
    else
    {
        aEvent.Skip();
    }
}


void DIALOG_PLM_PROGRESS::OnProgressUpdate( wxThreadEvent& aEvent )
{
    if( m_complete )
    {
        EndModal( wxID_OK );
        return;
    }

    if( m_pendingUpdate )
    {
        m_pendingUpdate = false;
        UpdateUI();
    }

    if( !m_pendingStatus.IsEmpty() )
    {
        m_statusLabel->SetLabel( m_pendingStatus );
        m_pendingStatus.Clear();
        Layout();
    }
}


void DIALOG_PLM_PROGRESS::OnTimer( wxTimerEvent& aEvent )
{
    if( m_indeterminate && !m_complete )
        m_progressGauge->Pulse();
}


void DIALOG_PLM_PROGRESS::UpdateUI()
{
    if( m_indeterminate )
    {
        m_progressGauge->Pulse();

        if( !m_pulseTimer.IsRunning() )
            m_pulseTimer.Start( 100 );
    }
    else
    {
        m_pulseTimer.Stop();
        m_progressGauge->SetRange( m_progressMax );
        m_progressGauge->SetValue( m_progressValue );
    }
}
