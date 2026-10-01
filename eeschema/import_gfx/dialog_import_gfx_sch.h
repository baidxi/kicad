/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2013 Jean-Pierre Charras, jp.charras at wanadoo.fr
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __DIALOG_IMPORT_GFX_SCH_H__
#define __DIALOG_IMPORT_GFX_SCH_H__

#include "dialog_import_gfx_sch_base.h"

#include <import_gfx/graphics_importer.h>
#include <widgets/unit_binder.h>

class SCH_BASE_FRAME;
class GRAPHICS_IMPORT_MGR;


class DIALOG_IMPORT_GFX_SCH : public DIALOG_IMPORT_GFX_SCH_BASE
{
public:
    DIALOG_IMPORT_GFX_SCH( SCH_BASE_FRAME* aParent );

    /**
     * @return a list of items imported from a vector graphics file.
     */
    std::list<std::unique_ptr<EDA_ITEM>>& GetImportedItems()
    {
        return m_importer->GetItems();
    }

    /**
     * @return true if the placement is interactive, i.e. all imported
     * items must be moved by the mouse cursor to the final position
     * false means the imported items are placed to the final position after import.
     */
    bool IsPlacementInteractive() { return m_placementInteractive; }

    bool ShouldGroupItems() { return m_cbGroupItems->IsChecked(); }

    /**
     * Set the filename override to be applied in TransferDataToWindow.
     */
    void SetFilenameOverride( const wxString& aFilenameOverride );

    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;

private:
    // Virtual event handlers
    void onBrowseFiles( wxCommandEvent& event ) override;
    void onFilenameChanged( wxCommandEvent& event ) override;
    void onThresholdChanged( wxScrollEvent& event ) override;
    void onInvertChanged( wxCommandEvent& event ) override;
    void onScaleChanged( wxCommandEvent& event ) override;
    void onWidthChanged( wxCommandEvent& event ) override;
    void onHeightChanged( wxCommandEvent& event ) override;
    void onLockAspectChanged( wxCommandEvent& event ) override;

    /// Read the chosen file, which is where the preview and the source size come from.
    void loadSourceFile();

    /// Show the image controls only when the chosen file is a raster image.
    void updateImageControls();

    void updatePreview();

    /// Offer the size fields only when the source has a size to scale.
    void updateSizeControls();

    /// Write the scale into the size fields.
    void sizeFromScale();

    /// Work the scale out from a size the user typed.
    void scaleFromSize( bool aFromWidth );
    void originOptionOnUpdateUI( wxUpdateUIEvent& event ) override;

	void onInteractivePlacement( wxCommandEvent& event ) override
    {
        m_placementInteractive = true;
    }

	void onAbsolutePlacement( wxCommandEvent& event ) override
    {
        m_placementInteractive = false;
    }

private:
    SCH_BASE_FRAME*                      m_parent;
    std::unique_ptr<GRAPHICS_IMPORTER>   m_importer;
    std::unique_ptr<GRAPHICS_IMPORT_MGR> m_gfxImportMgr;

    UNIT_BINDER          m_xOrigin;
    UNIT_BINDER          m_yOrigin;
    UNIT_BINDER          m_defaultLineWidth;
    UNIT_BINDER          m_importWidth;
    UNIT_BINDER          m_importHeight;

    wxImage m_previewSource;

    /// The file the source size and the preview were taken from.
    wxString m_sourcePath;

    /// Size of the source file's contents in mm, (0,0) when it has none to report.
    VECTOR2D m_sourceSizeMM;

    double m_xScale;
    double m_yScale;

    static bool          m_placementInteractive;
    static double        m_importScale;         // a scale factor to change the size of imported
                                                // items m_importScale =1.0 means keep original size

    wxString             m_filenameOverride;
};

#endif    //  __DIALOG_IMPORT_GFX_SCH_H__
