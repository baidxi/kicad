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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <pcb_plot_params.h>
#include <map>
#include <vector>

#include <board_outline_splitter.h>

class BOARD;
class FOOTPRINT;
class PROJECT;
class REPORTER;
class wxFileName;
class JOB_EXPORT_PCB_PLOT;

class PCB_PLOTTER
{
public:
    PCB_PLOTTER( BOARD* aBoard, REPORTER* aReporter, PCB_PLOT_PARAMS& aParams );

    bool Plot( const wxString& aOutputPath, const LSEQ& aLayersToPlot, const LSEQ& aCommonLayers,
               bool aUseGerberFileExtensions,
                bool aOutputPathIsSingle = false,
                std::optional<wxString> aLayerName = std::nullopt,
                std::optional<wxString> aSheetName = std::nullopt,
                std::optional<wxString> aSheetPath = std::nullopt,
                std::vector<wxString>* aOutputFiles = nullptr );

    /**
     * Multi-board-outline split export: the independent Edge.Cuts outlines (see
     * SplitBoardOutlines()) are exported one by one, each into its own board<N>
     * sub-directory of the output path.  Items spanning several outlines or sitting
     * outside all of them abort the export with an error report.
     *
     * Call after constructing the plotter, before Plot().
     */
    void SetOutlineSplit( BOARD_OUTLINE_SPLIT& aSplit )
    {
        m_outlineSplit = &aSplit;
    }

    /**
     * Run the pre-flight check for a multi-outline split export.  Returns false and
     * fills @a aErrors when an item spans several outlines or sits outside all of them.
     */
    bool CheckOutlineSplit( wxString& aErrors ) const;

    /**
     * Multi-outline split entry: exports every outline into its own board<N> sub-directory
     * of @a aOutputPath.  Only runs when SetOutlineSplit() was called.
     */
    bool PlotOutlineSplit( const wxString& aOutputPath, const LSEQ& aLayersToPlot,
                           const LSEQ& aCommonLayers, bool aUseGerberFileExtensions,
                           std::vector<wxString>* aOutputFiles );

    /**
     * Pre-filter for a single-outline pass: see PlotOneOutline().
     */
    void SetPlotOutlineFilter( const SHAPE_POLY_SET* aRegion,
                               const std::map<const PCB_SHAPE*, int>* aShapeMap, int aIndex )
    {
        m_filterRegion   = aRegion;
        m_filterShapeMap = aShapeMap;
        m_filterIndex    = aIndex;
    }

    /**
     * All copper layers that are disabled are actually selected
     * This is due to wonkyness in automatically selecting copper layers
     * for plotting when adding more than two layers to a board.
     * If plot options become accessible to the layers setup dialog
     * please move this functionality there!
     * This skips a copper layer if it is actually disabled on the board.
     */
    bool copperLayerShouldBeSkipped( PCB_LAYER_ID aLayerToPlot );

    /**
     * Return the layers a plot dialog stores when \a aChecked are ticked.
     *
     * The dialog shows only the board's \a aEnabledLayers.  Board plot settings also keep every
     * unused copper layer so it starts ticked once the board enables it.  A job keeps only what
     * the dialog showed, since it plots on other boards and a layer nobody can see cannot be
     * removed.
     */
    static LSET DialogLayerSelection( const LSET& aChecked, const LSET& aEnabledLayers, bool aForJob );

    /**
     * Complete a plot filename.
     *
     * It forces the output directory, adds a suffix to the name, and sets the specified extension.
     * The suffix is usually the layer name and replaces illegal file name character in the suffix
     * with an underscore character.
     *
     * @param aFilename is the file name to initialize that contains the base filename.
     * @param aOutputDir is the path.
     * @param aSuffix is the suffix to add to the base filename.
     * @param aExtension is the file extension.
     */
    static void BuildPlotFileName( wxFileName* aFilename, const wxString& aOutputDir, const wxString& aSuffix,
                                   const wxString& aExtension );

    /**
     * Translate a JOB to PCB_PLOT_PARAMS
     */
    static void PlotJobToPlotOpts( PCB_PLOT_PARAMS& aOpts, JOB_EXPORT_PCB_PLOT* aJob,
                                   REPORTER& aReporter );

protected:
    BOARD*          m_board;
    PCB_PLOT_PARAMS m_plotOpts;
    REPORTER*       m_reporter;

private:
    /**
     * Generates a final LSEQ for plotting by removing duplicates
     */
    LSEQ getPlotSequence( PCB_LAYER_ID aLayerToPlot, LSEQ aPlotWithAllLayersSeq );

    /**
     * Multi-outline split pass: export one outline into its board<N> sub-directory.
     */
    bool PlotOneOutline( size_t aOutlineIndex, const wxString& aOutputPath, const LSEQ& aLayersToPlot,
                         const LSEQ& aCommonLayers, bool aUseGerberFileExtensions,
                         std::vector<wxString>* aOutputFiles );

    BOARD_OUTLINE_SPLIT* m_outlineSplit = nullptr;

    /// Single-pass pre-filter (see SetPlotOutlineFilter)
    const SHAPE_POLY_SET*                   m_filterRegion = nullptr;
    const std::map<const PCB_SHAPE*, int>*  m_filterShapeMap = nullptr;
    int                                     m_filterIndex = 0;
};


/**
 * Plot a footprint to an SVG file, with the footprint origin at the SVG origin and the
 * page/viewBox sized to the footprint's bounding box.
 *
 * The footprint is plotted through a temporary board with the footprint placed at its
 * origin. Fit-to-board is assumed.
 */
bool PlotFootprintToSVG( const FOOTPRINT& aFootprint, PROJECT& aProject,
                         const std::map<wxString, wxString>* aVarOverrides, PCB_PLOT_PARAMS& aPlotOpts,
                         const LSEQ& aLayersToPlot, const LSEQ& aLayersOnAll, const wxString& aFileName,
                         REPORTER* aReporter = nullptr );
