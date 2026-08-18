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

#include <io/altium/altium_project.h>

#include <boost/uuid/name_generator_sha1.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <wx/filename.h>
#include <wx/textfile.h>

#include <map>
#include <set>
#include <utility>
#include <vector>


namespace
{
/**
 * Read an Altium INI-style file (.PrjPcb) into ordered sections.
 *
 * wxFileConfig proved unreliable on real-world project files (UTF-8 BOM, tens of thousands of
 * entries), silently yielding no groups.  A direct line scan is robust and sufficient because
 * the project format is a flat [Section]/key=value layout.
 *
 * @return a vector of { sectionName, { key -> value } } preserving section order.
 */
std::vector<std::pair<wxString, std::map<wxString, wxString>>> ReadAltiumIniSections( const wxString& aPath )
{
    std::vector<std::pair<wxString, std::map<wxString, wxString>>> sections;

    wxTextFile file( aPath );

    if( !file.Open() )
        return sections;

    std::map<wxString, wxString>* current = nullptr;

    for( size_t i = 0; i < file.GetLineCount(); ++i )
    {
        wxString line = file[i];

        // Strip a leading BOM that may cling to the first line.
        while( !line.empty() && ( line[0] == wxUniChar( 0xFEFF ) || line[0] == wxUniChar( 0xFFFE ) ) )
            line.Remove( 0, 1 );

        line.Trim( false );

        if( line.empty() )
            continue;

        if( line[0] == wxUniChar( '[' ) )
        {
            int close = line.Find( wxUniChar( ']' ) );

            if( close == wxNOT_FOUND )
                continue;

            wxString name = line.Mid( 1, close - 1 );
            sections.emplace_back( name, std::map<wxString, wxString>() );
            current = &sections.back().second;
            continue;
        }

        if( !current )
            continue;

        int eqPos = line.Find( wxUniChar( '=' ) );

        if( eqPos == wxNOT_FOUND )
            continue;

        wxString key = line.Left( eqPos );
        wxString value = line.Mid( eqPos + 1 );

        key.Trim( true ).Trim( false );

        // Values may legitimately contain '=' (e.g. Variation lines); only the first is the
        // key/value separator, so the remainder is kept verbatim (trailing CR already stripped
        // by wxTextFile).
        ( *current )[key] = value;
    }

    file.Close();

    return sections;
}
} // namespace


ALTIUM_PROJECT_FILES ParseAltiumProjectFiles( const wxString& aPrjPcbPath )
{
    ALTIUM_PROJECT_FILES files;
    std::set<wxString>   schematicFiles;
    wxFileName           projectFileName( aPrjPcbPath );

    for( const auto& [groupName, keys] : ReadAltiumIniSections( aPrjPcbPath ) )
    {
        if( !groupName.StartsWith( wxS( "Document" ) ) )
            continue;

        wxString documentNumber = groupName.Mid( 8 );
        long     parsedDocumentNumber;

        if( !documentNumber.ToLong( &parsedDocumentNumber ) )
            continue;

        auto pathIt = keys.find( wxS( "DocumentPath" ) );

        if( pathIt == keys.end() )
            continue;

        wxString path = pathIt->second;
        path.Trim( true ).Trim( false );

        if( path.empty() )
            continue;

        wxFileName fileName( path, wxPATH_WIN );

        if( !fileName.IsAbsolute() )
            fileName.MakeAbsolute( projectFileName.GetPath() );

        if( fileName.GetExt().IsSameAs( wxS( "SchDoc" ), false ) )
        {
            schematicFiles.insert( fileName.GetFullPath() );
        }
        else if( files.pcbFile.empty()
                 && ( fileName.GetExt().IsSameAs( wxS( "PcbDoc" ), false )
                      || fileName.GetExt().IsSameAs( wxS( "CSPcbDoc" ), false )
                      || fileName.GetExt().IsSameAs( wxS( "CMPcbDoc" ), false )
                      || fileName.GetExt().IsSameAs( wxS( "SWPcbDoc" ), false ) ) )
        {
            files.pcbFile = fileName.GetFullPath();
        }
    }

    files.schematicFiles.assign( schematicFiles.begin(), schematicFiles.end() );
    return files;
}


KIID AltiumUniqueIdToKiid( const wxString& aUniqueId )
{
    // Fixed namespace so a given id always maps to the same KIID. Must never change or
    // previously stamped footprint paths would stop matching.
    static const boost::uuids::uuid s_namespace =
            boost::uuids::string_generator()( "6f9619ff-8b86-d011-b42d-00cf4fc964ff" );

    boost::uuids::name_generator_sha1 generator( s_namespace );
    boost::uuids::uuid                uuid = generator( aUniqueId.utf8_string() );

    return KIID( wxString::FromUTF8( boost::uuids::to_string( uuid ) ) );
}


ALTIUM_VARIANT_ENTRY ParseVariationString( const wxString& aValue )
{
    ALTIUM_VARIANT_ENTRY entry;

    // Altium delimits fields with '|'.  Backslashes are literal (UniqueId targets are
    // backslash-delimited paths), so disable wxSplit's default '\' escape character.
    for( const wxString& token : wxSplit( aValue, '|', '\0' ) )
    {
        int eqPos = token.Find( '=' );

        if( eqPos == wxNOT_FOUND )
            continue;

        wxString key = token.Left( eqPos ).Trim().Trim( false );
        wxString val = token.Mid( eqPos + 1 ).Trim().Trim( false );

        if( key.CmpNoCase( wxS( "Designator" ) ) == 0 )
        {
            entry.designator = val;
        }
        else if( key.CmpNoCase( wxS( "UniqueId" ) ) == 0 )
        {
            // The target is a backslash-delimited path; only the final segment is the
            // component's own unique id, which is what symbol and footprint records store.
            int sep = val.Find( '\\', true );

            if( sep != wxNOT_FOUND )
                entry.uniqueId = val.Mid( sep + 1 );
            else
                entry.uniqueId = val;
        }
        else if( key.CmpNoCase( wxS( "Kind" ) ) == 0 )
        {
            long k = 0;
            val.ToLong( &k );
            entry.kind = static_cast<int>( k );
        }

        // AlternatePart and its trailing sub-fields are collected in the second pass below.
    }

    // Parse AlternatePart sub-fields. In the raw line, AlternatePart= is followed by
    // pipe-separated key=value pairs that are part of the alternate part definition.
    // Since we already split on '|', find the AlternatePart token and treat everything
    // after it as sub-fields.
    bool inAlternatePart = false;

    for( const wxString& token : wxSplit( aValue, '|', '\0' ) )
    {
        int eqPos = token.Find( '=' );

        if( eqPos == wxNOT_FOUND )
            continue;

        wxString key = token.Left( eqPos ).Trim().Trim( false );
        wxString val = token.Mid( eqPos + 1 ).Trim().Trim( false );

        if( key.CmpNoCase( wxS( "AlternatePart" ) ) == 0 )
        {
            inAlternatePart = true;

            // The value after AlternatePart= might itself be the first sub-field value
            // In practice it's typically empty for Kind=1, or a lib reference for Kind=0
            if( !val.empty() )
                entry.alternateFields[wxS( "LibReference" )] = val;

            continue;
        }

        if( inAlternatePart && !val.empty() )
            entry.alternateFields[key] = val;
    }

    return entry;
}


ALTIUM_PARAM_VARIATION ParseParamVariationString( const wxString& aValue )
{
    ALTIUM_PARAM_VARIATION variation;

    for( const wxString& token : wxSplit( aValue, '|', '\0' ) )
    {
        int eqPos = token.Find( '=' );

        if( eqPos == wxNOT_FOUND )
            continue;

        wxString key = token.Left( eqPos ).Trim().Trim( false );
        wxString val = token.Mid( eqPos + 1 ).Trim().Trim( false );

        if( key.CmpNoCase( wxS( "ParameterName" ) ) == 0 )
            variation.parameterName = val;
        else if( key.CmpNoCase( wxS( "VariantValue" ) ) == 0 )
            variation.variantValue = val;
    }

    return variation;
}


wxString AltiumParameterNameToKiCadField( const wxString& aName )
{
    wxString name = aName.Upper();

    if( name == wxS( "COMMENT" ) )
        return wxS( "Value" );

    if( name == wxS( "VALUE" ) )
        return wxS( "ALTIUM_VALUE" );

    return name;
}


std::map<wxString, wxString> ParseAltiumProjectParameters( const wxString& aPrjPcbPath )
{
    std::map<wxString, wxString> parameters;

    for( const auto& [groupname, keys] : ReadAltiumIniSections( aPrjPcbPath ) )
    {
        if( !groupname.StartsWith( wxS( "Parameter" ) ) )
            continue;

        // Only numbered [ParameterN] sections hold the project parameters; reject look-alikes
        // such as [ParameterEngine] by requiring a trailing integer.
        wxString numStr = groupname.Mid( 9 );
        long     num;

        if( !numStr.ToLong( &num ) )
            continue;

        auto nameIt = keys.find( wxS( "Name" ) );

        if( nameIt == keys.end() || nameIt->second.empty() )
            continue;

        wxString name = nameIt->second;

        auto     valueIt = keys.find( wxS( "Value" ) );
        wxString value = valueIt != keys.end() ? valueIt->second : wxString();

        // KiCad variable references are matched case-insensitively by resolving to upper case,
        // which is what AltiumPcbSpecialStringsToKiCadStrings emits, so key on the upper-cased
        // name to keep ${PCB_REVISION} and friends resolvable.
        name.UpperCase();

        parameters[name] = value;
    }

    return parameters;
}


std::vector<ALTIUM_PROJECT_VARIANT> ParseAltiumProjectVariants( const wxString& aPrjPcbPath )
{
    std::vector<ALTIUM_PROJECT_VARIANT> variants;

    for( const auto& [groupname, keys] : ReadAltiumIniSections( aPrjPcbPath ) )
    {
        if( !groupname.StartsWith( wxS( "ProjectVariant" ) ) )
            continue;

        wxString numStr = groupname.Mid( 14 );
        long     num;

        if( !numStr.ToLong( &num ) )
            continue;

        ALTIUM_PROJECT_VARIANT pv;

        auto descIt = keys.find( wxS( "Description" ) );
        pv.description = descIt != keys.end() ? descIt->second : wxString();
        pv.name = pv.description;

        if( pv.name.empty() )
            pv.name = groupname;

        long variationCount = 0;

        if( auto countIt = keys.find( wxS( "VariationCount" ) ); countIt != keys.end() )
            countIt->second.ToLong( &variationCount );

        for( long vi = 1; vi <= variationCount; ++vi )
        {
            auto varIt = keys.find( wxString::Format( wxS( "Variation%ld" ), vi ) );

            if( varIt == keys.end() || varIt->second.empty() )
                continue;

            ALTIUM_VARIANT_ENTRY entry = ParseVariationString( varIt->second );

            if( !entry.designator.empty() )
                pv.variations.push_back( std::move( entry ) );
        }

        long paramVariationCount = 0;

        if( auto countIt = keys.find( wxS( "ParamVariationCount" ) ); countIt != keys.end() )
            countIt->second.ToLong( &paramVariationCount );

        // ParamVariation entries are numbered independently of the Variation entries they sit
        // between, and name their component through the ParamDesignator key of the same index,
        // so the two are paired on that index rather than on a variation.
        std::map<wxString, std::map<wxString, wxString>> overridesByDesignator;

        for( long pi = 1; pi <= paramVariationCount; ++pi )
        {
            auto paramIt = keys.find( wxString::Format( wxS( "ParamVariation%ld" ), pi ) );
            auto desigIt = keys.find( wxString::Format( wxS( "ParamDesignator%ld" ), pi ) );

            if( paramIt == keys.end() || desigIt == keys.end() )
                continue;

            wxString designator = desigIt->second;
            designator.Trim( true ).Trim( false );

            ALTIUM_PARAM_VARIATION paramVar = ParseParamVariationString( paramIt->second );

            if( designator.empty() || paramVar.parameterName.empty() )
                continue;

            overridesByDesignator[designator][paramVar.parameterName] = paramVar.variantValue;
        }

        for( ALTIUM_VARIANT_ENTRY& entry : pv.variations )
        {
            if( auto it = overridesByDesignator.find( entry.designator );
                it != overridesByDesignator.end() )
            {
                entry.parameterOverrides = std::move( it->second );
                overridesByDesignator.erase( it );
            }
        }

        // Altium normally writes a Kind=0 Variation alongside every override, but an override
        // without one would otherwise be dropped, so carry it on a synthesised fitted entry.
        for( auto& [designator, overrides] : overridesByDesignator )
        {
            ALTIUM_VARIANT_ENTRY entry;

            entry.designator = designator;
            entry.kind = 0;
            entry.parameterOverrides = std::move( overrides );

            pv.variations.push_back( std::move( entry ) );
        }

        if( !pv.variations.empty() )
            variants.push_back( std::move( pv ) );
    }

    return variants;
}
