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

#ifndef DIALOG_IDX_REVIEW_H
#define DIALOG_IDX_REVIEW_H

#include "dialog_idx_review_base.h"
#include <wx/dir.h>

class PCB_BASE_FRAME;
class BOARD;
class KICAD_IDX;

/**
 * Derived dialog implementing a functional GAL preview panel.
 */
class DIALOG_IDX_REVIEW : public DIALOG__IDX_REVIEW_BASE
{
public:
    DIALOG_IDX_REVIEW( PCB_BASE_FRAME* aParent, std::shared_ptr<KICAD_IDX> aIDX );
    ~DIALOG_IDX_REVIEW() override;

    bool                                           TransferDataToWindow();
    std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> GetAccepted();
    std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> GetRejected();


protected:
    void OnGridCellClick( wxGridEvent& event ) override;
    void OnAcceptAll( wxCommandEvent& event ) override;
    void OnRejectAll( wxCommandEvent& event ) override;
    void onItemActivated( wxDataViewEvent& event ) override;
    void OnCollaborationFolderText( wxCommandEvent& event ) override;
    void onBrowseButton( wxCommandEvent& event ) override;

private:
    void loadDir( const wxString& aDir );
    bool loadIDX( std::shared_ptr<KICAD_IDX> aIDX );
    void clearIDX();
    void prepareCanvas();
    void refreshPreview();
    void OnResize( wxSizeEvent& event );
    void OnCancel( wxCommandEvent& event );
    void AddChange( KICAD_IDX_CHANGE& aChange );


private:
    std::shared_ptr<wxDir>     m_dir;
    PCB_BASE_FRAME*            m_parent;
    BOARD*                     m_board;
    BOARD*                     m_dummyBoard;
    std::shared_ptr<KICAD_IDX> m_idx;

    std::vector<std::vector<BOARD_ITEM*>>                           m_objects;
    std::vector<std::pair<bool, std::shared_ptr<KICAD_IDX_CHANGE>>> m_changes;
    ;
};

#endif // DIALOG_IDX_REVIEW_H
