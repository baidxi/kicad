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

#include <pcb_edit_frame.h>
#include <board.h>
#include <board_commit.h>
#include <footprint.h>
#include <dialog_relink_footprints.h>
#include <widgets/wx_html_report_panel.h>
#include <tool/tool_manager.h>
#include <tools/pcb_selection_tool.h>
#include <kiface_base.h>
#include <project.h>
#include <string_utils.h>

#include <map>
#include <set>


DIALOG_RELINK_FOOTPRINTS::DIALOG_RELINK_FOOTPRINTS( PCB_EDIT_FRAME* aParent,
                                                     const PCB_SELECTION& aSelection ) :
    DIALOG_RELINK_FOOTPRINTS_BASE( aParent ),
    m_frame( aParent ),
    m_initialized( false )
{
    // Snapshot the selected footprints at dialog creation time
    for( EDA_ITEM* item : aSelection )
    {
        if( item->Type() == PCB_FOOTPRINT_T )
            m_selectedFootprints.push_back( static_cast<FOOTPRINT*>( item ) );
    }

    // Set up the report panel
    m_messagePanel->SetLabel( _( "Changes to be Applied" ) );
    m_messagePanel->SetFileName( Prj().GetProjectPath() + wxT( "relink_report.txt" ) );
    m_messagePanel->SetLazyUpdate( true );

    // Configure scope radio buttons based on selection
    if( m_selectedFootprints.empty() )
    {
        m_rbSelected->Enable( false );
        m_rbAll->SetValue( true );
    }
    else
    {
        m_rbSelected->Enable( true );
        m_rbSelected->SetValue( true );
    }

    m_messagePanel->GetSizer()->SetSizeHints( this );
    m_messagePanel->Layout();

    SetupStandardButtons( { { wxID_OK,     _( "Relink" ) },
                            { wxID_CANCEL, _( "Close" )  } } );

    finishDialogSettings();

    m_initialized = true;

    // Try to fetch netlist from schematic
    if( !m_frame->FetchNetlistFromSchematic( m_netlist,
                                              _( "Relinking footprints requires a schematic." ) ) )
    {
        m_sdbSizerOK->Enable( false );
        m_messagePanel->Reporter().Report(
                _( "Could not fetch netlist from schematic. Is a schematic open?" ),
                RPT_SEVERITY_ERROR );
        m_messagePanel->Flush( true );
        return;
    }

    PerformRelink( true );
}


DIALOG_RELINK_FOOTPRINTS::~DIALOG_RELINK_FOOTPRINTS()
{
}


void DIALOG_RELINK_FOOTPRINTS::PerformRelink( bool aDryRun )
{
    m_messagePanel->Clear();

    REPORTER& reporter = m_messagePanel->Reporter();
    BOARD*    board = m_frame->GetBoard();

    // Determine which footprints to process
    std::vector<FOOTPRINT*> footprintsToProcess;

    if( m_rbSelected->GetValue() && !m_selectedFootprints.empty() )
    {
        footprintsToProcess = m_selectedFootprints;
    }
    else
    {
        for( FOOTPRINT* fp : board->Footprints() )
            footprintsToProcess.push_back( fp );
    }

    // Build a map of reference -> footprints to detect duplicates on the board. This must cover
    // the whole board, even when only relinking a selection, so a duplicate reference outside
    // the selection still blocks an ambiguous relink.
    std::map<wxString, std::vector<FOOTPRINT*>> refToFootprints;

    for( FOOTPRINT* fp : board->Footprints() )
        refToFootprints[fp->GetReference()].push_back( fp );

    // Track statistics
    int relinkedCount = 0;
    int unchangedCount = 0;
    int warningCount = 0;
    int errorCount = 0;

    // Set of references we've already reported errors for (to avoid duplicates)
    std::set<wxString> erroredRefs;

    BOARD_COMMIT commit( m_frame );

    for( FOOTPRINT* footprint : footprintsToProcess )
    {
        wxString ref = footprint->GetReference();

        // Check for duplicate footprints with same reference on board
        if( refToFootprints[ref].size() > 1 )
        {
            if( erroredRefs.find( ref ) == erroredRefs.end() )
            {
                reporter.Report( wxString::Format(
                        _( "Error: Multiple footprints found with reference %s; skipping." ),
                        ref ),
                        RPT_SEVERITY_ERROR );
                errorCount++;
                erroredRefs.insert( ref );
            }

            continue;
        }

        // Look up component by reference in netlist
        COMPONENT* component = m_netlist.GetComponentByReference( ref );

        if( !component )
        {
            reporter.Report( wxString::Format(
                    _( "Warning: %s not found in schematic; link unchanged." ),
                    ref ),
                    RPT_SEVERITY_WARNING );
            warningCount++;
            continue;
        }

        // Build the new path from netlist component
        KIID_PATH newPath = component->GetPath();
        const std::vector<KIID>& kiids = component->GetKIIDs();

        if( !kiids.empty() )
        {
            // For multi-unit symbols, try to match existing unit info
            const auto& unitInfo = footprint->GetUnitInfo();

            if( kiids.size() > 1 && !unitInfo.empty() )
            {
                // Try to find a matching unit KIID based on unit name/index
                // For now, we'll use a simple approach: if the footprint has unit info,
                // try to match the first unit. In the future, this could be smarter.

                // Report warning for multi-unit symbols
                reporter.Report( wxString::Format(
                        _( "Warning: %s matched (symbol has %zu units); verify correct unit." ),
                        ref, kiids.size() ),
                        RPT_SEVERITY_WARNING );
                warningCount++;
            }

            // Use the first KIID (or the matched one if we implement smarter matching)
            newPath.push_back( kiids.front() );
        }

        // Compare paths
        KIID_PATH currentPath = footprint->GetPath();

        if( currentPath == newPath )
        {
            reporter.Report( wxString::Format(
                    _( "Info: %s already linked correctly." ),
                    ref ),
                    RPT_SEVERITY_INFO );
            unchangedCount++;
            continue;
        }

        // Report the change
        if( aDryRun )
        {
            reporter.Report( wxString::Format(
                    _( "Relink %s: path will be updated from '%s' to '%s'." ),
                    ref,
                    currentPath.AsString(),
                    newPath.AsString() ),
                    RPT_SEVERITY_ACTION );
        }
        else
        {
            commit.Modify( footprint );
            footprint->SetPath( newPath );

            reporter.Report( wxString::Format(
                    _( "Relinked %s: path updated from '%s' to '%s'." ),
                    ref,
                    currentPath.AsString(),
                    newPath.AsString() ),
                    RPT_SEVERITY_ACTION );
        }

        relinkedCount++;
    }

    // Report summary
    reporter.Report( wxEmptyString, RPT_SEVERITY_INFO );

    if( aDryRun )
    {
        reporter.Report( wxString::Format(
                _( "Summary: %d to relink, %d unchanged, %d warnings, %d errors." ),
                relinkedCount, unchangedCount, warningCount, errorCount ),
                RPT_SEVERITY_INFO );
    }
    else
    {
        reporter.Report( wxString::Format(
                _( "Summary: %d relinked, %d unchanged, %d warnings, %d errors." ),
                relinkedCount, unchangedCount, warningCount, errorCount ),
                RPT_SEVERITY_INFO );

        if( relinkedCount > 0 )
            commit.Push( _( "Relink footprints to symbols" ) );
    }

    m_messagePanel->Flush( true );
}


void DIALOG_RELINK_FOOTPRINTS::OnScopeChanged( wxCommandEvent& event )
{
    if( m_initialized )
    {
        PerformRelink( true );
        m_sdbSizerOK->Enable( true );
        m_sdbSizerOK->SetDefault();
    }
}


void DIALOG_RELINK_FOOTPRINTS::OnRelinkClick( wxCommandEvent& event )
{
    m_messagePanel->SetLabel( _( "Changes Applied" ) );
    PerformRelink( false );

    m_sdbSizerCancel->SetDefault();
    m_sdbSizerOK->Enable( false );
}
