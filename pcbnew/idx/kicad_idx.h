/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2024 KiCad Developers, see AUTHORS.txt for contributors.
 * @author Fabien Corona <f.corona<at>laposte.net>
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


#ifndef KICAD_IDX_H
#define KICAD_IDX_H

#include <wx/xml/xml.h>
#include <board_commit.h>
#include <idx/idx.h>
#include <board.h>

class LSET;
enum PCB_LAYER_ID : int;

//bool read_IDX( wxString aFilename, BOARD_COMMIT* aCommit );
class IDX_HOLDER;

class KICAD_IDX
{
public:
    KICAD_IDX( wxString aFilename, BOARD* aBoard, BOARD_COMMIT* aCommit );


    void        AddAll();
    BOARD_ITEM* AddElement( idx_geometry::EDMD_CURVESET2D&, LSET );
    BOARD_ITEM* AddElement( idx_geometry::EDMD_POLYLINE&, LSET );
    BOARD_ITEM* AddElement( idx_geometry::EDMD_CIRCLE&, LSET );
    BOARD_ITEM* AddElement( idx_geometry::EDMD_ARC&, LSET );

    VECTOR2I  ArcMidPoint( idx_geometry::EDMD_ARC& );
    PCB_SHAPE ConvertToKiCadShape( idx_geometry::EDMD_ARC& );
    SHAPE_ARC ConvertToKiCadArc( idx_geometry::EDMD_ARC& );

    void AddItem( BOARD_ITEM*, LSET );

    std::unique_ptr<IDX_HOLDER> m_holder;
    BOARD_COMMIT*               m_commit;
    BOARD*                      m_board;
};

#endif
