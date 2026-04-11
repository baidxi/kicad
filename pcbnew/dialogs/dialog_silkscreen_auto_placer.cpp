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

#include "dialog_silkscreen_auto_placer.h"

#include <base_units.h>
#include <board.h>
#include <board_commit.h>
#include <footprint.h>
#include <pcb_edit_frame.h>
#include <tool/tool_manager.h>
#include <tools/global_edit_tool.h>
#include <tools/pcb_actions.h>
#include <tools/pcb_selection_tool.h>
#include <autorouter/silkscreen_auto_placer.h>

#include <wx/busyinfo.h>
#include <wx/tokenzr.h>


DIALOG_SILKSCREEN_AUTO_PLACER::DIALOG_SILKSCREEN_AUTO_PLACER( PCB_EDIT_FRAME* aParent ) :
        DIALOG_SILKSCREEN_AUTO_PLACER_BASE( aParent ),
        m_frame( aParent ),
        m_fixedSize( aParent, m_stFixedSize, m_tcFixedSize, m_stFixedSizeUnits ),
        m_fixedWidth( aParent, m_stFixedWidth, m_tcFixedWidth, m_stFixedWidthUnits ),
        m_positionDelta( aParent, m_stPosDelta, m_tcPosDelta, m_stPosDeltaUnits )
{
    SetupStandardButtons( { { wxID_OK,     _( "Run" ) },
                            { wxID_CANCEL, _( "Close" ) } } );

    finishDialogSettings();
}


bool DIALOG_SILKSCREEN_AUTO_PLACER::TransferDataToWindow()
{
    m_fixedSize.SetValue( pcbIUScale.mmToIU( 0.8 ) );
    m_fixedWidth.SetValue( pcbIUScale.mmToIU( 0.15 ) );
    m_positionDelta.SetValue( pcbIUScale.mmToIU( 0.42 ) );

    return true;
}


bool DIALOG_SILKSCREEN_AUTO_PLACER::TransferDataFromWindow()
{
    BOARD* board = m_frame->GetBoard();

    SILK_PLACER_SETTINGS settings;
    settings.placeSelectedOnly = m_rbPlaceSel->GetValue();

    if( m_rbFailCenter->GetValue() )
        settings.failedPlacement = SILK_FAILED_PLACEMENT::CENTER_OVER_COMPONENT;
    else if( m_rbFailOffBoard->GetValue() )
        settings.failedPlacement = SILK_FAILED_PLACEMENT::PLACE_OFF_BOARD;
    else
        settings.failedPlacement = SILK_FAILED_PLACEMENT::RESTORE_ORIGINAL;

    // Parse the allow silk under text
    wxString allowText = m_allowSilkUnder->GetValue();
    wxStringTokenizer tokenizer( allowText, wxT( "\n\r, " ) );

    while( tokenizer.HasMoreTokens() )
    {
        wxString token = tokenizer.GetNextToken().Trim().Trim( false );

        if( !token.IsEmpty() )
            settings.allowSilkUnder.insert( token );
    }

    settings.avoidVias = m_cbAvoidVias->GetValue();
    settings.rotationStrategy = m_choiceRotation->GetSelection() == 0
                                        ? SILK_ROTATION_STRATEGY::KLC_STYLE
                                        : SILK_ROTATION_STRATEGY::MATCH_COMPONENT;
    settings.tryAlteredRotation = m_cbTryAltRotation->GetValue();

    settings.fixedSize = m_cbFixedSize->GetValue();
    settings.fixedSizeValue = m_fixedSize.GetIntValue();
    settings.fixedWidth = m_cbFixedWidth->GetValue();
    settings.fixedWidthValue = m_fixedWidth.GetIntValue();

    settings.positionFlags = 0;

    if( m_cbTopCenter->GetValue() )   settings.positionFlags |= POS_TOP_CENTER;
    if( m_cbCenterRight->GetValue() ) settings.positionFlags |= POS_CENTER_RIGHT;
    if( m_cbBotCenter->GetValue() )   settings.positionFlags |= POS_BOT_CENTER;
    if( m_cbCenterLeft->GetValue() )  settings.positionFlags |= POS_CENTER_LEFT;
    if( m_cbTopLeft->GetValue() )     settings.positionFlags |= POS_TOP_LEFT;
    if( m_cbTopRight->GetValue() )    settings.positionFlags |= POS_TOP_RIGHT;
    if( m_cbBotLeft->GetValue() )     settings.positionFlags |= POS_BOT_LEFT;
    if( m_cbBotRight->GetValue() )    settings.positionFlags |= POS_BOT_RIGHT;

    if( settings.positionFlags == 0 )
    {
        wxMessageBox( _( "At least one position must be enabled." ), _( "Error" ),
                      wxOK | wxICON_ERROR, this );
        return false;
    }

    settings.positionDelta = m_positionDelta.GetIntValue();

    // Collect footprints
    std::vector<FOOTPRINT*> footprints;

    if( settings.placeSelectedOnly )
    {
        PCB_SELECTION_TOOL* selTool =
                m_frame->GetToolManager()->GetTool<PCB_SELECTION_TOOL>();
        const PCB_SELECTION& selection = selTool->GetSelection();

        for( EDA_ITEM* item : selection )
        {
            if( item->Type() == PCB_FOOTPRINT_T )
                footprints.push_back( static_cast<FOOTPRINT*>( item ) );
        }

        if( footprints.empty() )
        {
            wxMessageBox( _( "No footprints selected." ), _( "Error" ),
                          wxOK | wxICON_ERROR, this );
            return false;
        }
    }
    else
    {
        for( FOOTPRINT* fp : board->Footprints() )
            footprints.push_back( fp );
    }

    BOARD_COMMIT commit( m_frame );
    wxBusyCursor busy;

    SILKSCREEN_AUTO_PLACER placer( board, settings );
    SILK_PLACER_RESULT result = placer.Run( commit, footprints );

    commit.Push( _( "Auto-Place Silkscreen" ) );

    m_frame->GetCanvas()->Refresh();

    wxString msg = wxString::Format( _( "Placed %d of %d reference designators." ),
                                     result.moved, result.attempted );
    wxMessageBox( msg, _( "Auto-Place Silkscreen" ), wxOK | wxICON_INFORMATION, this );

    return true;
}


void DIALOG_SILKSCREEN_AUTO_PLACER::onFixedSizeCheck( wxCommandEvent& event )
{
    m_tcFixedSize->Enable( m_cbFixedSize->GetValue() );
}


void DIALOG_SILKSCREEN_AUTO_PLACER::onFixedWidthCheck( wxCommandEvent& event )
{
    m_tcFixedWidth->Enable( m_cbFixedWidth->GetValue() );
}


int GLOBAL_EDIT_TOOL::SilkscreenAutoPlace( const TOOL_EVENT& aEvent )
{
    PCB_EDIT_FRAME* editFrame = getEditFrame<PCB_EDIT_FRAME>();
    DIALOG_SILKSCREEN_AUTO_PLACER dlg( editFrame );

    dlg.ShowModal();
    return 0;
}
