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

#include "dialog_plm_audit.h"

#include <plm/plm_plugin.h>

#include <wx/datetime.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/textfile.h>


DIALOG_PLM_AUDIT::DIALOG_PLM_AUDIT( wxWindow* aParent, PLM_PLUGIN* aPlugin ) :
        DIALOG_PLM_AUDIT_BASE( aParent ),
        m_plugin( aPlugin )
{
    initColumns();
    initFilters();

    // Default date range to last 7 days
    wxDateTime now = wxDateTime::Now();
    wxDateTime weekAgo = now - wxDateSpan( 0, 0, 1, 0 );
    m_fromDatePicker->SetValue( weekAgo );
    m_toDatePicker->SetValue( now );

    updateRecordCount();

    SetMinSize( wxSize( 900, 600 ) );
    Layout();
}


DIALOG_PLM_AUDIT::~DIALOG_PLM_AUDIT()
{
}


void DIALOG_PLM_AUDIT::SetProjectFilter( const wxString& aProjectId )
{
    m_presetProjectId = aProjectId;
}


void DIALOG_PLM_AUDIT::initColumns()
{
    m_eventList->AppendTextColumn( _( "Timestamp" ), wxDATAVIEW_CELL_INERT, 160,
                                   wxALIGN_LEFT,
                                   wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_eventList->AppendTextColumn( _( "User" ), wxDATAVIEW_CELL_INERT, 120,
                                   wxALIGN_LEFT,
                                   wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_eventList->AppendTextColumn( _( "Event Type" ), wxDATAVIEW_CELL_INERT, 140,
                                   wxALIGN_LEFT,
                                   wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_eventList->AppendTextColumn( _( "Project" ), wxDATAVIEW_CELL_INERT, 140,
                                   wxALIGN_LEFT,
                                   wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_eventList->AppendTextColumn( _( "Details" ), wxDATAVIEW_CELL_INERT, 250,
                                   wxALIGN_LEFT,
                                   wxDATAVIEW_COL_RESIZABLE );
}


void DIALOG_PLM_AUDIT::initFilters()
{
    m_userFilter->Append( _( "(All Users)" ) );
    m_userFilter->SetSelection( 0 );

    m_eventTypeFilter->Append( _( "(All Types)" ) );
    m_eventTypeFilter->Append( _( "Authentication" ) );
    m_eventTypeFilter->Append( _( "File Operations" ) );
    m_eventTypeFilter->Append( _( "Lock Operations" ) );
    m_eventTypeFilter->Append( _( "Revisions" ) );
    m_eventTypeFilter->Append( _( "State Transitions" ) );
    m_eventTypeFilter->Append( _( "Reviews" ) );
    m_eventTypeFilter->Append( _( "Errors" ) );
    m_eventTypeFilter->SetSelection( 0 );

    m_projectFilter->Append( _( "(All Projects)" ) );
    m_projectFilter->SetSelection( 0 );
}


void DIALOG_PLM_AUDIT::queryAuditLog()
{
    if( !m_plugin )
        return;

    PLM_AUDIT_FILTER filter = buildFilter();

    PLM_AUDIT_QUERY_RESULT result;

    if( !m_presetProjectId.IsEmpty() )
    {
        result = m_plugin->GetProjectAuditLog( m_presetProjectId, filter.limit );
    }
    else
    {
        result = m_plugin->GetAuditLog( filter );
    }

    if( !result.IsSuccess() )
    {
        wxMessageBox( wxString::Format( _( "Failed to query audit log:\n%s" ),
                                         result.error_message ),
                      _( "Error" ), wxOK | wxICON_ERROR, this );
        return;
    }

    m_events = result.events;
    populateEventList();
    updateRecordCount();
}


void DIALOG_PLM_AUDIT::populateEventList()
{
    m_eventList->DeleteAllItems();

    for( const PLM_AUDIT_EVENT& evt : m_events )
    {
        wxVector<wxVariant> data;

        data.push_back( wxVariant( formatTimestamp( evt.timestamp_ms ) ) );
        data.push_back( wxVariant( evt.user_name.IsEmpty() ? evt.user_id : evt.user_name ) );
        data.push_back( wxVariant( evt.GetEventTypeName() ) );
        data.push_back( wxVariant( evt.project_name.IsEmpty() ? evt.project_id
                                                               : evt.project_name ) );
        data.push_back( wxVariant( evt.result ) );

        m_eventList->AppendItem( data );
    }
}


void DIALOG_PLM_AUDIT::updateRecordCount()
{
    m_recordCountLabel->SetLabel(
            wxString::Format( _( "%zu records" ), m_events.size() ) );
}


void DIALOG_PLM_AUDIT::showEventDetail( int aRow )
{
    if( aRow < 0 || static_cast<size_t>( aRow ) >= m_events.size() )
    {
        m_detailText->SetValue( wxEmptyString );
        return;
    }

    const PLM_AUDIT_EVENT& evt = m_events[aRow];

    wxString detail;
    detail += wxString::Format( _( "Event ID: %s\n" ), evt.event_id );
    detail += wxString::Format( _( "Type: %s\n" ), evt.GetEventTypeName() );
    detail += wxString::Format( _( "Time: %s\n" ), formatTimestamp( evt.timestamp_ms ) );
    detail += wxString::Format( _( "User: %s (%s)\n" ), evt.user_name, evt.user_id );
    detail += wxString::Format( _( "Project: %s (%s)\n" ), evt.project_name, evt.project_id );
    detail += wxString::Format( _( "Success: %s\n" ), evt.success ? _( "Yes" ) : _( "No" ) );

    if( !evt.result.IsEmpty() )
        detail += wxString::Format( _( "Result: %s\n" ), evt.result );

    if( !evt.details_json.IsEmpty() )
    {
        detail += wxS( "\n" );
        detail += _( "Details JSON:\n" );
        detail += evt.details_json;
    }

    m_detailText->SetValue( detail );

    if( m_detailPane->IsCollapsed() )
        m_detailPane->Expand();
}


PLM_AUDIT_FILTER DIALOG_PLM_AUDIT::buildFilter() const
{
    PLM_AUDIT_FILTER filter;

    wxDateTime fromDate = m_fromDatePicker->GetValue();
    wxDateTime toDate = m_toDatePicker->GetValue();

    if( fromDate.IsValid() )
    {
        fromDate.SetHour( 0 );
        fromDate.SetMinute( 0 );
        fromDate.SetSecond( 0 );
        filter.start_time_ms = static_cast<int64_t>( fromDate.GetTicks() ) * 1000;
    }

    if( toDate.IsValid() )
    {
        toDate.SetHour( 23 );
        toDate.SetMinute( 59 );
        toDate.SetSecond( 59 );
        filter.end_time_ms = static_cast<int64_t>( toDate.GetTicks() ) * 1000;
    }

    int eventTypeSelection = m_eventTypeFilter->GetSelection();

    if( eventTypeSelection == 7 )
        filter.errors_only = true;

    filter.limit = 500;

    return filter;
}


wxString DIALOG_PLM_AUDIT::formatTimestamp( int64_t aTimestampMs ) const
{
    if( aTimestampMs == 0 )
        return wxEmptyString;

    wxDateTime dt( static_cast<time_t>( aTimestampMs / 1000 ) );

    if( !dt.IsValid() )
        return wxEmptyString;

    return dt.Format( wxS( "%Y-%m-%d %H:%M:%S" ) );
}


void DIALOG_PLM_AUDIT::onFilterChanged( wxCommandEvent& event )
{
    queryAuditLog();
}


void DIALOG_PLM_AUDIT::onDateChanged( wxDateEvent& event )
{
    queryAuditLog();
}


void DIALOG_PLM_AUDIT::onEventSelected( wxDataViewEvent& event )
{
    int row = m_eventList->GetSelectedRow();
    showEventDetail( row );
}


void DIALOG_PLM_AUDIT::onRefresh( wxCommandEvent& event )
{
    queryAuditLog();
}


void DIALOG_PLM_AUDIT::onExportCsv( wxCommandEvent& event )
{
    wxFileDialog dlg( this, _( "Export Audit Log" ), wxEmptyString,
                      wxS( "audit_log.csv" ),
                      _( "CSV files (*.csv)|*.csv" ),
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT );

    if( dlg.ShowModal() != wxID_OK )
        return;

    wxTextFile file( dlg.GetPath() );

    if( file.Exists() )
        file.Open();
    else
        file.Create();

    file.Clear();

    file.AddLine( wxS( "Timestamp,User,Event Type,Project,Success,Details" ) );

    for( const PLM_AUDIT_EVENT& evt : m_events )
    {
        wxString userName = evt.user_name.IsEmpty() ? evt.user_id : evt.user_name;
        wxString projectName = evt.project_name.IsEmpty() ? evt.project_id : evt.project_name;
        wxString result = evt.result;
        result.Replace( wxS( "," ), wxS( ";" ) );

        wxString line = wxString::Format( wxS( "%s,%s,%s,%s,%s,\"%s\"" ),
                                           formatTimestamp( evt.timestamp_ms ),
                                           userName, evt.GetEventTypeName(),
                                           projectName,
                                           evt.success ? wxS( "Yes" ) : wxS( "No" ),
                                           result );

        file.AddLine( line );
    }

    file.Write();
    file.Close();

    wxMessageBox( wxString::Format( _( "Exported %zu records to %s" ),
                                     m_events.size(), dlg.GetPath() ),
                  _( "Export Complete" ), wxOK | wxICON_INFORMATION, this );
}


void DIALOG_PLM_AUDIT::onClose( wxCommandEvent& event )
{
    EndModal( wxID_CLOSE );
}
