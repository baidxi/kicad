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
 */

/**
 * @file plm_kicad_pro_metadata.h
 * @brief Lightweight reader for .kicad_pro project metadata.
 *
 * Used by PLM plugins to enrich search results, project listings, and
 * reuse-from-template flows without depending on the full KiCad project
 * machinery. Only the fields relevant to PLM are extracted; the rest of
 * the .kicad_pro JSON tree is ignored.
 */

#ifndef PLM_KICAD_PRO_METADATA_H
#define PLM_KICAD_PRO_METADATA_H

#include <kicommon.h>

#include <wx/string.h>
#include <map>
#include <optional>


/**
 * Project metadata extracted from a .kicad_pro file.
 *
 * Every field is best-effort: a missing or malformed entry leaves the
 * corresponding member at its default. The presence of the struct in an
 * std::optional return value indicates the file was successfully opened
 * and parsed as JSON; individual fields may still be empty.
 */
struct PLM_KICAD_PRO_METADATA
{
    wxString filename;          ///< meta.filename — usually matches the file's basename
    int      version = 0;       ///< meta.version — project format version
    int      sheet_count = 0;   ///< number of root-level sheets in the project

    /// Project-defined text variables (project setup → text variables).
    std::map<wxString, wxString> text_variables;
};


/**
 * Read PLM-relevant fields from a .kicad_pro file on disk.
 *
 * @param aPath Absolute path to a .kicad_pro file.
 * @return Populated struct on success, or std::nullopt if the file does
 *         not exist, cannot be opened, or fails to parse as JSON.
 */
KICOMMON_API std::optional<PLM_KICAD_PRO_METADATA>
ReadKicadProMetadata( const wxString& aPath );


#endif // PLM_KICAD_PRO_METADATA_H
