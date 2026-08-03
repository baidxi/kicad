/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.TXT for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
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

#ifndef EESCHEMA_HELPERS_H_
#define EESCHEMA_HELPERS_H_

#include <vector>

#include <wx/string.h>
#include <sch_io/sch_io_mgr.h>

class SCHEMATIC;
class SCH_EDIT_FRAME;
class SETTINGS_MANAGER;
class PROJECT;

/**
 * Helper functions to do things like load schematics behind the scenes for special functions
 *
 * Similar to the Pcbnew scripting helpers, this is just to have one spot
 * for these types of "hacky" solutions
 */
class EESCHEMA_HELPERS
{
public:
    static void              SetSchEditFrame( SCH_EDIT_FRAME* aSchEditFrame );
    static SCHEMATIC*        LoadSchematic( const wxString& aFileName, bool aSetActive,
                                            bool aForceDefaultProject,
                                            PROJECT* aProject = nullptr,
                                            bool aCalculateConnectivity = true );
    static SCHEMATIC*        LoadSchematic( const wxString& aFileName,
                                            SCH_IO_MGR::SCH_FILE_T aFormat,
                                            bool aSetActive, bool aForceDefaultProject,
                                            PROJECT* aProject = nullptr,
                                            bool aCalculateConnectivity = true );

    /**
     * Collect the schematics that could be the hierarchical root owning \a aFileName.
     *
     * A schematic handed to a headless job may be a sub-sheet, in which case loading it on its
     * own leaves its hierarchical labels without the cross-sheet mates that drive them.  Every
     * project file sitting next to \a aFileName contributes the root sheet named after it as a
     * candidate.  Candidates are only possible owners: the caller must load one and confirm with
     * ContainsSheetFile() that its hierarchy really pulls \a aFileName in.
     *
     * @return absolute root schematic paths, empty when \a aFileName is a project root itself or
     *         has no project file beside it.
     */
    static std::vector<wxString> CollectProjectRootCandidates( const wxString& aFileName );

    /**
     * @return true if \a aFileName is one of the sheet files making up \a aSchematic.
     */
    static bool ContainsSheetFile( const SCHEMATIC& aSchematic, const wxString& aFileName );

private:
    static SCH_EDIT_FRAME*   s_SchEditFrame;
};

#endif
