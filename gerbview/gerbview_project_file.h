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

#ifndef GERBVIEW_PROJECT_FILE_H
#define GERBVIEW_PROJECT_FILE_H

#include <map>
#include <string>
#include <vector>

#include <gal/color4d.h>
#include <wx/string.h>

class GERBVIEW_FRAME;
class REPORTER;


/**
 * In-memory representation of a GerbView project file (`.kicad_gvp`).
 *
 * A project file is a small JSON document that bundles the information needed
 * to restore a GerbView session:
 *
 *  - which Gerber/Excellon files are part of the project (with type and the
 *    role each file should map to in the Layer Manager),
 *  - the color palette (per-role overrides),
 *  - selected per-session settings (e.g. how many inner-copper layers are
 *    exposed by the Layer Roles dialog, the name of the last imported color
 *    scheme).
 *
 * The format is intentionally future-proof: it carries `schema_version` and
 * `kind` markers, and unknown fields are ignored on load so existing files
 * remain readable as we add functionality.
 *
 * Storage convention for file paths:
 *  - On save, paths are made relative to the project file's directory; this
 *    keeps projects portable when the containing folder is moved.
 *  - On load, paths are resolved against the project file's directory, with
 *    absolute paths preserved as-is.
 */
class GERBVIEW_PROJECT_FILE
{
public:
    static constexpr int         CURRENT_SCHEMA_VERSION = 1;
    static constexpr const char* PROJECT_KIND = "kicad_gerbview_project";

    enum class FILE_KIND
    {
        Gerber = 0,
        Drill = 1
    };

    struct FILE_ENTRY
    {
        /// Path stored in / read from the project file (relative or absolute).
        wxString path;

        FILE_KIND kind = FILE_KIND::Gerber;

        /// Stable role key (see RoleSerializedKey).  Empty == no role mapping.
        std::string roleKey;

        /// Layer visibility when the project was saved.
        bool visible = true;
    };

    GERBVIEW_PROJECT_FILE() = default;

    /// Reset every field to its default-constructed state.
    void Reset();

    /// Capture the frame's currently loaded files + settings + colors into this object.
    /// After this call the object is ready to be passed to SaveToFile().
    void CaptureFromFrame( GERBVIEW_FRAME* aFrame );

    /**
     * Apply the project to @p aFrame: clears the existing layers, loads each
     * referenced file (resolving paths against @p aProjectDir), then applies
     * the role mappings + color overrides + settings.
     *
     * Returns true if all files loaded successfully.  Non-fatal issues are
     * reported through @p aReporter.
     */
    bool ApplyToFrame( GERBVIEW_FRAME* aFrame, const wxString& aProjectDir, REPORTER* aReporter );

    /// Parse a .kicad_gvp file into this object.
    bool LoadFromFile( const wxString& aFullFileName, REPORTER* aReporter );

    /// Serialize this object to a .kicad_gvp file (overwriting if present).
    bool SaveToFile( const wxString& aFullFileName, REPORTER* aReporter );

    // ----------------------------------------------------------------------
    // Public data (kept simple on purpose so consumers can read/edit easily).
    // ----------------------------------------------------------------------
    std::vector<FILE_ENTRY>               m_Files;
    std::map<std::string, KIGFX::COLOR4D> m_Colors;
    int                                   m_VisibleInnerCount = 2;
    wxString                              m_ColorSchemeName;
};


#endif // GERBVIEW_PROJECT_FILE_H
