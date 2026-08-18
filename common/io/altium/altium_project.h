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

#ifndef ALTIUM_PROJECT_H
#define ALTIUM_PROJECT_H

#include <map>
#include <vector>
#include <wx/string.h>

#include <kiid.h>

/**
 * Importable design documents listed by an Altium .PrjPcb file.
 */
struct ALTIUM_PROJECT_FILES
{
    std::vector<wxString> schematicFiles;
    wxString              pcbFile;
};


/**
 * Parse the importable design documents from an Altium .PrjPcb project file.
 *
 * Schematic paths are de-duplicated and returned in a stable order.  The first supported board
 * document is returned because a KiCad project can contain only one board.
 *
 * @param aPrjPcbPath Full path to the .PrjPcb file.
 * @return The schematic and board document paths resolved relative to the project file.
 */
ALTIUM_PROJECT_FILES ParseAltiumProjectFiles( const wxString& aPrjPcbPath );


/**
 * A single component variation within an Altium project variant.
 *
 * In Altium, each variant can override individual components. Kind=1 means
 * "Not Fitted" (DNP), Kind=0 means "Alternate" with optional field overrides
 * supplied via the AlternatePart string.
 */
struct ALTIUM_VARIANT_ENTRY
{
    wxString designator;

    // Component's own unique id (final segment of the Altium path); the value used for matching.
    wxString uniqueId;

    int                          kind = 0;
    std::map<wxString, wxString> alternateFields;

    // Per-variant overrides of the component's own parameters, keyed on the raw Altium parameter
    // name (ParamVariation entries).  Unlike alternateFields, which describes a substituted part,
    // these only change parameter values on the component that is already there.
    std::map<wxString, wxString> parameterOverrides;
};


/**
 * A single Altium "ParamVariation" entry: one component parameter overridden by a variant.
 *
 * The owning designator is not part of the string; it is carried by the matching
 * ParamDesignator key.
 */
struct ALTIUM_PARAM_VARIATION
{
    wxString parameterName;
    wxString variantValue;
};


/**
 * A project-level assembly variant parsed from an Altium .PrjPcb file.
 *
 * Each [ProjectVariantN] section in the .PrjPcb becomes one of these.
 */
struct ALTIUM_PROJECT_VARIANT
{
    wxString                          name;
    wxString                          description;
    std::vector<ALTIUM_VARIANT_ENTRY> variations;
};


/**
 * Parse a single pipe-delimited Altium "Variation" string into its component entry.
 *
 * Exposed for unit testing; consumers normally obtain entries via ParseAltiumProjectVariants().
 */
ALTIUM_VARIANT_ENTRY ParseVariationString( const wxString& aValue );


/**
 * Parse a single pipe-delimited Altium "ParamVariation" string.
 *
 * Exposed for unit testing; consumers normally obtain overrides via
 * ALTIUM_VARIANT_ENTRY::parameterOverrides.
 */
ALTIUM_PARAM_VARIATION ParseParamVariationString( const wxString& aValue );


/**
 * Canonicalize an Altium component parameter name to the KiCad field name the importers use.
 *
 * Altium parameter references are case-insensitive and its special string conversion emits
 * upper-case names, so the name is upper-cased.  Two names are remapped: "Comment" is what
 * Altium displays as the component value and becomes KiCad's "Value" field, while Altium's own
 * "Value" parameter is kept apart as "ALTIUM_VALUE" so the two cannot collide.
 *
 * This mirrors the mapping the schematic and board importers apply to a component's base
 * parameters, so a variant override lands on the same field the base value was imported into.
 */
wxString AltiumParameterNameToKiCadField( const wxString& aName );


/**
 * Parse all [ProjectVariantN] sections from an Altium .PrjPcb project file.
 *
 * @param aPrjPcbPath Full path to the .PrjPcb file.
 * @return Vector of project variants with their per-component entries.
 */
std::vector<ALTIUM_PROJECT_VARIANT> ParseAltiumProjectVariants( const wxString& aPrjPcbPath );


/**
 * Parse all [ParameterN] sections from an Altium .PrjPcb project file.
 *
 * Altium stores project-wide special strings (PCB_Revision, Company_Name, ...) here as
 * Name/Value pairs. They are what board text such as ".PCB_Revision" resolves against, so
 * they map to KiCad project text variables.
 *
 * @param aPrjPcbPath Full path to the .PrjPcb file.
 * @return Map of upper-cased parameter name to value, matching the case-insensitive variable
 *         references emitted by AltiumPcbSpecialStringsToKiCadStrings.
 */
std::map<wxString, wxString> ParseAltiumProjectParameters( const wxString& aPrjPcbPath );


/**
 * Derive a stable KIID from an Altium component unique id.
 *
 * Altium unique ids are not hexadecimal, so KIID's string constructor cannot parse them and
 * returns a fresh random uuid each call. This maps equal id strings to equal KIIDs.
 *
 * @param aUniqueId Altium component unique id.
 * @return A KIID that is identical for identical input strings.
 */
KIID AltiumUniqueIdToKiid( const wxString& aUniqueId );

#endif // ALTIUM_PROJECT_H
