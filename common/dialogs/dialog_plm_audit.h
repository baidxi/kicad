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
 * @file dialog_plm_audit.h
 * @brief Dialog for viewing PLM audit log events.
 */

#ifndef DIALOG_PLM_AUDIT_H
#define DIALOG_PLM_AUDIT_H

#include "dialog_plm_audit_base.h"

#include <plm/plm_audit.h>

#include <vector>


class PLM_PLUGIN;


/**
 * Dialog for viewing and filtering PLM audit log events.
 *
 * Provides date range, user, event type, and project filters with
 * CSV export and a collapsible detail panel.
 */
class DIALOG_PLM_AUDIT : public DIALOG_PLM_AUDIT_BASE
{
public:
    DIALOG_PLM_AUDIT( wxWindow* aParent, PLM_PLUGIN* aPlugin );
    ~DIALOG_PLM_AUDIT();

    /**
     * Pre-filter to a specific project.
     *
     * @param aProjectId Project to filter by.
     */
    void SetProjectFilter( const wxString& aProjectId );

protected:
    void onFilterChanged( wxCommandEvent& event ) override;
    void onDateChanged( wxDateEvent& event ) override;
    void onEventSelected( wxDataViewEvent& event ) override;
    void onRefresh( wxCommandEvent& event ) override;
    void onExportCsv( wxCommandEvent& event ) override;
    void onClose( wxCommandEvent& event ) override;

private:
    void initFilters();
    void initColumns();
    void queryAuditLog();
    void populateEventList();
    void updateRecordCount();
    void showEventDetail( int aRow );
    PLM_AUDIT_FILTER buildFilter() const;

    wxString formatTimestamp( int64_t aTimestampMs ) const;

    PLM_PLUGIN*                     m_plugin;
    wxString                        m_presetProjectId;
    std::vector<PLM_AUDIT_EVENT>    m_events;
};


#endif // DIALOG_PLM_AUDIT_H
