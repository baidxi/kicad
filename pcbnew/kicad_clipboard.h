/**
 * @file kicad_clipboard.h
 * @brief
 * @author Kristoffer Ödmark
 * @version 1.0
 * @date 2017-05-03
 *
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.TXT for contributors.
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef KICAD_CLIPBOARD_H
#define KICAD_CLIPBOARD_H

#include <board_item.h>
#include <kiid.h>
#include <pcb_io/kicad_sexpr/pcb_io_kicad_sexpr.h>
#include <pcb_io/kicad_sexpr/pcb_io_kicad_sexpr_parser.h>
#include <memory.h>
#include <tools/pcb_selection.h>
#include <vector>


class CLIPBOARD_IO : public PCB_IO_KICAD_SEXPR
{
public:
    CLIPBOARD_IO();
    ~CLIPBOARD_IO();

    void SetWriter( std::function<void(const wxString&)> aWriter ) { m_writer = aWriter; }
    void SetReader( std::function<wxString()> aReader ) { m_reader = aReader; }

    /*
     * Saves the entire board to the clipboard formatted using the PCB_IO_KICAD_SEXPR formatting
     */
    void SaveBoard( const wxString& aFileName, BOARD* aBoard,
                    const std::map<std::string, UTF8>* aProperties = nullptr ) override;

    /*
     * Write all the settings of the BOARD* set by setBoard() and then adds all the
     * BOARD_ITEMs found in selection formatted by PCB_IO_KICAD_SEXPR to clipboard as sexpr text
     *
     * If savedIds is given, it receives the KIID each selected item was written under, in output
     * order.  These are not always the KIIDs of the selected items: a selected field is written as
     * a text inside a synthesized footprint, selected table cells are written as their promoted
     * parent table (once per table), and items that are not serialized at all are omitted.  Items
     * written as dependents of a selected item (group children, carried constraints) are not
     * listed separately.
     */
    void SaveSelection( const PCB_SELECTION& selected, bool isFootprintEditor, std::vector<KIID>* savedIds = nullptr );

    BOARD_ITEM* Parse();

    BOARD* LoadBoard( const wxString& aFileName, BOARD* aAppendToMe,
                      const std::map<std::string, UTF8>* aProperties = nullptr, PROJECT* aProject = nullptr ) override;

    void SetBoard( BOARD* aBoard );

private:
    static void clipboardWriter( const wxString& aData );
    static wxString clipboardReader();

    STRING_FORMATTER m_formatter;
    std::function<void(const wxString&)> m_writer;
    std::function<wxString()> m_reader;
};


#endif /* KICAD_CLIPBOARD_H */
