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
 * along with this program; if not, see
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 */

#ifndef IDX_READER_H
#define IDX_READER_H

#include <idx/kidx/kidx.h>
#include <iostream>
#include <board_commit.h>
#include <math/vector2d.h>
#include <pcb_shape.h>
#include <pcb_group.h>
#include <idx/CAD_software.h>
#include <memory>
#include <vector>

class IDX_HOLDER
{
public:
    IDX_HOLDER( wxXmlDocument& aDoc, bool aHeaderOnly = false );

    void Read( wxXmlDocument& aDoc, bool aHeaderOnly );

    std::shared_ptr<idx_foundation::EDMD_DATA_SET> m_dataset;

private:
};

#endif // IDX_READER_H
