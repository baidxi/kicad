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

#ifndef DIALOG_SILKSCREEN_AUTO_PLACER_H
#define DIALOG_SILKSCREEN_AUTO_PLACER_H

#include "dialog_silkscreen_auto_placer_base.h"
#include <widgets/unit_binder.h>

class PCB_EDIT_FRAME;


class DIALOG_SILKSCREEN_AUTO_PLACER : public DIALOG_SILKSCREEN_AUTO_PLACER_BASE
{
public:
    DIALOG_SILKSCREEN_AUTO_PLACER( PCB_EDIT_FRAME* aParent );

private:
    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;

    void onFixedSizeCheck( wxCommandEvent& event ) override;
    void onFixedWidthCheck( wxCommandEvent& event ) override;

    PCB_EDIT_FRAME* m_frame;
    UNIT_BINDER     m_fixedSize;
    UNIT_BINDER     m_fixedWidth;
    UNIT_BINDER     m_positionDelta;
};

#endif // DIALOG_SILKSCREEN_AUTO_PLACER_H
