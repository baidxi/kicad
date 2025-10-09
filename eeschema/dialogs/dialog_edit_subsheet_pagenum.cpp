/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2018-2024 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <dialogs/dialog_edit_subsheet_pagenum.h>
#include <sch_edit_frame.h>
#include <validators.h>

static wxString g_setToLastName;
static int      g_setToLastOrientation = 0;
static int      g_setToLastPosition = 0;
static int      g_setToLastAction = 0;

class SCH_EDIT_FRAME;

DIALOG_EDIT_SUBSHEET_PAGENUM::DIALOG_EDIT_SUBSHEET_PAGENUM( SCH_EDIT_FRAME* aParent ) :
        DIALOG_EDIT_SUBSHEET_PAGENUM_BASE( aParent ), m_frame( aParent )
{
    if( g_setToLastName.IsEmpty() )
    {
        m_checkDefaultName->SetValue( true );
        m_textName->SetValue( _( "Page" ) );
    }
    else
    {
        m_checkDefaultName->SetValue( false );
        m_textName->SetValue( g_setToLastName );
    }

    m_radioOrientation->SetSelection( g_setToLastOrientation );
    m_radioPosition->SetSelection( g_setToLastPosition );
    m_radioAction->SetSelection( g_setToLastAction );

    // Hide all labels
    for( uint i = 0; i < m_radioOrientation->GetCount(); i++ )
        m_radioOrientation->SetString(i, wxT( "" ) );

    // Create update event
    wxCommandEvent update;
    update.SetInt( m_checkDefaultName->GetValue() );
    OnChangedDefaultName( update );

    SetupStandardButtons();
    finishDialogSettings();
}


DIALOG_EDIT_SUBSHEET_PAGENUM::~DIALOG_EDIT_SUBSHEET_PAGENUM()
{
    if( m_checkDefaultName->GetValue() == wxCHK_CHECKED )
        g_setToLastName.Clear();
    else
        g_setToLastName = m_textName->GetValue();

    g_setToLastOrientation = m_radioOrientation->GetSelection();
    g_setToLastPosition = m_radioPosition->GetSelection();
    g_setToLastAction = m_radioAction->GetSelection();
}


void DIALOG_EDIT_SUBSHEET_PAGENUM::OnChangedDefaultName( wxCommandEvent& event )
{
    m_textName->Enable( !event.IsChecked() );
}


bool DIALOG_EDIT_SUBSHEET_PAGENUM::TransferDataFromWindow()
{
    int item_count = 0;

    enum class ORIANTATIONS
    {
        TOP_LEFT = 0,
        TOP_CENTER = 1,
        TOP_RIGHT = 2,
        MID_LEFT = 3,
        MID_CENTER = 4,
        MID_RIGHT = 5,
        BOTTOM_LEFT = 6,
        BOTTOM_CENTER = 7,
        BOTTOM_RIGHT = 8
    };

    enum class POSITIONS
    {
        INSIDE = 0,
        OUTSIDE = 1
    };

    enum class ACTIONS
    {
        ADD = 0,
        REMOVE = 1
    };

    const wxString pageFieldName( wxT( "PageNum" ) );
    const wxString pageFieldDesc( ( m_checkDefaultName->GetValue() ? _( "Page" ) : m_textName->GetValue() ) );

    SCH_SHEET_LIST m_sheetList = m_frame->Schematic().Hierarchy();

    for( SCH_ITEM* item : m_frame->GetScreen()->Items().OfType( SCH_SHEET_T ) )
    {
        SCH_SHEET* sheet = static_cast<SCH_SHEET*>( item );

        auto getFieldPage = [&]() -> SCH_FIELD*
        {
            SCH_FIELD* result = nullptr;

            for( SCH_FIELD& field : sheet->GetFields() )
            {
                if( field.GetName().Contains( pageFieldName ) )
                    result = &field;
            }

            return result;
        };

        auto setTextPosition = [&]( SCH_FIELD* item, int oriantion, int position )
        {
            const VECTOR2I    sheet_pos = sheet->GetPosition();
            const VECTOR2I    sheet_size = sheet->GetSize();
            const int         grid_size = m_frame->Schematic().Settings().m_PinSymbolSize;
            
            GR_TEXT_H_ALIGN_T hAlign = GR_TEXT_H_ALIGN_INDETERMINATE;
            GR_TEXT_V_ALIGN_T vAlign = GR_TEXT_V_ALIGN_INDETERMINATE;
            VECTOR2I          pos_offset(0,0);

            switch( oriantion )
            {
            case(int) ORIANTATIONS::TOP_LEFT:
                hAlign = GR_TEXT_H_ALIGN_LEFT;
                vAlign = GR_TEXT_V_ALIGN_TOP;
                pos_offset.x = grid_size;
                pos_offset.y = grid_size;
                break;

            case(int) ORIANTATIONS::TOP_CENTER:
                hAlign = GR_TEXT_H_ALIGN_CENTER;
                vAlign = GR_TEXT_V_ALIGN_TOP;
                pos_offset.x = sheet_size.x / 2;
                pos_offset.y = grid_size;
                break;

            case(int) ORIANTATIONS::TOP_RIGHT:
                hAlign = GR_TEXT_H_ALIGN_RIGHT;
                vAlign = GR_TEXT_V_ALIGN_TOP;
                pos_offset.x = sheet_size.x - grid_size;
                pos_offset.y = grid_size;
                break;

            case(int) ORIANTATIONS::MID_LEFT:
                hAlign = GR_TEXT_H_ALIGN_LEFT;
                vAlign = GR_TEXT_V_ALIGN_CENTER;
                pos_offset.x = grid_size;
                pos_offset.y = sheet_size.y / 2;
                break;

            case(int) ORIANTATIONS::MID_CENTER:
                hAlign = GR_TEXT_H_ALIGN_CENTER;
                vAlign = GR_TEXT_V_ALIGN_CENTER;
                pos_offset.x = sheet_size.x / 2;
                pos_offset.y = sheet_size.y / 2;
                break;

            case(int) ORIANTATIONS::MID_RIGHT:
                hAlign = GR_TEXT_H_ALIGN_RIGHT;
                vAlign = GR_TEXT_V_ALIGN_CENTER;
                pos_offset.x = sheet_size.x - grid_size;
                pos_offset.y = sheet_size.y / 2;
                break;

            case(int) ORIANTATIONS::BOTTOM_LEFT:
                hAlign = GR_TEXT_H_ALIGN_LEFT;
                vAlign = GR_TEXT_V_ALIGN_BOTTOM;
                pos_offset.x = grid_size;
                pos_offset.y = sheet_size.y - grid_size;
                break;

            case(int) ORIANTATIONS::BOTTOM_CENTER:
                hAlign = GR_TEXT_H_ALIGN_CENTER;
                vAlign = GR_TEXT_V_ALIGN_BOTTOM;
                pos_offset.x = sheet_size.x / 2;
                pos_offset.y = sheet_size.y - grid_size;
                break;

            case(int) ORIANTATIONS::BOTTOM_RIGHT:
                hAlign = GR_TEXT_H_ALIGN_RIGHT;
                vAlign = GR_TEXT_V_ALIGN_BOTTOM;
                pos_offset.x = sheet_size.x - grid_size;
                pos_offset.y = sheet_size.y - grid_size;
                break;

                default: break;
            }

            if( position == (int)POSITIONS::OUTSIDE )
            {
                if( vAlign == GR_TEXT_V_ALIGN_TOP )
                {
                    vAlign = GR_TEXT_V_ALIGN_BOTTOM;
                    pos_offset.y -= 2 * grid_size;
                }
                else if( vAlign == GR_TEXT_V_ALIGN_BOTTOM )
                {
                    vAlign = GR_TEXT_V_ALIGN_TOP;
                    pos_offset.y += 2 * grid_size;
                }
                else if (vAlign == GR_TEXT_V_ALIGN_CENTER)
                {
                    if (hAlign == GR_TEXT_H_ALIGN_LEFT)
                    {
                        hAlign = GR_TEXT_H_ALIGN_RIGHT;
                        pos_offset -= 2 * grid_size;
                    }
                    else if(hAlign == GR_TEXT_H_ALIGN_RIGHT)
                    {
                        hAlign = GR_TEXT_H_ALIGN_LEFT;
                        pos_offset += 2 * grid_size;
                    }
                }
            }

            item->SetHorizJustify( hAlign );
            item->SetVertJustify( vAlign );
            item->SetPosition( sheet_pos + pos_offset );
        };

        if( sheet->IsRootSheet() )
            break;

        SCH_FIELD* pagefield = nullptr;
        pagefield = getFieldPage();

        if( m_radioAction->GetSelection() == (int)ACTIONS::ADD )
        {
            if( pagefield == nullptr )
                pagefield = &sheet->GetFields().emplace_back(
                        std::move( SCH_FIELD( item, FIELD_T::SHEET_USER, pageFieldName ) ) );

            pagefield->SetText( pageFieldDesc + ": ${#}" );
            setTextPosition( pagefield, m_radioOrientation->GetSelection(),
                          m_radioPosition->GetSelection() );
            pagefield->SetVisible( true );
            item_count++;
        }
        else
        {
            if( pagefield != nullptr)
            {
                for( size_t i = 0; i < sheet->GetFields().size(); i++ )
                {
                    if (sheet->GetFields().at(i).GetName().Contains(pageFieldName))
                    {
                        sheet->GetFields().erase( sheet->GetFields().begin() + i );
                        item_count++;
                        break;
                    }
                }
            }
        }
    }

    if( item_count == 0 )
    {
        wxMessageBox( wxT( "No items have been updated." ) );
    }

    m_frame->HardRedraw();

    return true;
}
