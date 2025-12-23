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

#pragma once

#include <dialog_relink_footprints_base.h>
#include <netlist_reader/pcb_netlist.h>

class PCB_EDIT_FRAME;
class PCB_SELECTION;

/**
 * Dialog to relink footprints to schematic symbols by matching reference designators.
 *
 * This tool re-establishes UUID links between PCB footprints and schematic symbols when
 * they've become out of sync (e.g., after board import, manual footprint placement, or
 * schematic rework).
 */
class DIALOG_RELINK_FOOTPRINTS : public DIALOG_RELINK_FOOTPRINTS_BASE
{
public:
    /**
     * Create the relink dialog.
     *
     * @param aParent is the PCB editor frame.
     * @param aSelection is the current selection (snapshot taken at dialog creation).
     */
    DIALOG_RELINK_FOOTPRINTS( PCB_EDIT_FRAME* aParent, const PCB_SELECTION& aSelection );

    ~DIALOG_RELINK_FOOTPRINTS();

private:
    /**
     * Perform the relinking operation.
     *
     * @param aDryRun if true, only report what would be changed without actually changing anything.
     */
    void PerformRelink( bool aDryRun );

    void OnScopeChanged( wxCommandEvent& event ) override;
    void OnRelinkClick( wxCommandEvent& event ) override;

    PCB_EDIT_FRAME*           m_frame;
    NETLIST                   m_netlist;
    std::vector<FOOTPRINT*>   m_selectedFootprints;
    bool                      m_initialized;
};
