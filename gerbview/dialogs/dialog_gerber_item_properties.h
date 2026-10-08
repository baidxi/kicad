/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DIALOG_GERBER_ITEM_PROPERTIES_H
#define DIALOG_GERBER_ITEM_PROPERTIES_H

#include <dialog_shim.h>

class GERBVIEW_FRAME;
class GERBER_DRAW_ITEM;
class wxBoxSizer;
class wxCheckBox;
class wxPanel;


/**
 * Modeless read-only properties dialog for a single GERBER_DRAW_ITEM.
 *
 * The dialog rebuilds its content whenever the selection changes in GerbView,
 * so the user can keep it open and explore the design.  A "Highlight" button
 * forces highlight of all items sharing the current D-code.
 */
class DIALOG_GERBER_ITEM_PROPERTIES : public DIALOG_SHIM
{
public:
    DIALOG_GERBER_ITEM_PROPERTIES( GERBVIEW_FRAME* aParent, GERBER_DRAW_ITEM* aItem );
    ~DIALOG_GERBER_ITEM_PROPERTIES() override;

    /// Reload all fields for the given item (may be nullptr to show an empty state).
    void SetItem( GERBER_DRAW_ITEM* aItem );

private:
    void buildContent();
    void onClose( wxCloseEvent& aEvent );
    void onCloseButton( wxCommandEvent& aEvent );
    void onHighlightToggled( wxCommandEvent& aEvent );

    /// Apply or refresh D-code highlight in the GerbView painter for the current item.
    void applyHighlight();
    /// Clear any D-code highlight in the GerbView painter.
    void clearHighlight();

    GERBVIEW_FRAME*   m_frame;
    GERBER_DRAW_ITEM* m_item;

    wxPanel*          m_contentPanel;
    wxBoxSizer*       m_contentSizer;
    wxCheckBox*       m_highlightDCode;
};


#endif // DIALOG_GERBER_ITEM_PROPERTIES_H
