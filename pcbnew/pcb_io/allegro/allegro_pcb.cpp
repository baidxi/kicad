/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2025 Jeff Wheeler <jeffwheeler@gmail.com>
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

#include "allegro_pcb.h"

#include <wx/log.h>

#include "allegro_parser.h"

ALLEGRO_PCB::ALLEGRO_PCB( BOARD* aBoard ) : m_board( aBoard )
{
}

void ALLEGRO_PCB::Parse( const ALLEGRO_FILE& aAllegroBrdFile )
{
    uint32_t magic = *(uint32_t*) ( aAllegroBrdFile.region.get_address() );
    switch( magic )
    {
    case 0x00130000:
    case 0x00130200:
    {
        ALLEGRO_PARSER<ALLEGRO::A_160>( m_board, aAllegroBrdFile ).Parse();
        return;
    }
    case 0x00130402:
    {
        ALLEGRO_PARSER<ALLEGRO::A_162>( m_board, aAllegroBrdFile ).Parse();
        return;
    }
    case 0x00130C03:
    {
        ALLEGRO_PARSER<ALLEGRO::A_164>( m_board, aAllegroBrdFile ).Parse();
        return;
    }
    case 0x00131003:
    {
        ALLEGRO_PARSER<ALLEGRO::A_165>( m_board, aAllegroBrdFile ).Parse();
        return;
    }
    case 0x00131503:
    case 0x00131504:
    {
        ALLEGRO_PARSER<ALLEGRO::A_166>( m_board, aAllegroBrdFile ).Parse();
        return;
    }
    case 0x00140400:
    case 0x00140500:
    case 0x00140501:
    case 0x00140502:
    case 0x00140600:
    case 0x00140700:
    {
        ALLEGRO_PARSER<ALLEGRO::A_172>( m_board, aAllegroBrdFile ).Parse();
        return;
    }
    case 0x00140900:
    case 0x00140901:
    case 0x00140902:
    case 0x00140E00:
    {
        ALLEGRO_PARSER<ALLEGRO::A_174>( m_board, aAllegroBrdFile ).Parse();
        return;
    }
    case 0x00141502:
    {
        ALLEGRO_PARSER<ALLEGRO::A_175>( m_board, aAllegroBrdFile ).Parse();
        return;
    }
    default:
        THROW_IO_ERROR( wxString::Format( _( "Board file magic=0x%08X not recognized." ),
                                          local_ntohl( magic ) ) );
    }
}
