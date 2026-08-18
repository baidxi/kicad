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

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <io/altium/altium_project.h>

#include <wx/filename.h>
#include <wx/log.h>


BOOST_AUTO_TEST_SUITE( AltiumProject )


BOOST_AUTO_TEST_CASE( ParseProjectFiles )
{
    wxFileName projectFile( wxString::FromUTF8( KI_TEST::GetTestDataRootDir() )
                            + wxS( "pcbnew/plugins/altium/eDP_adapter_dvt1_source/"
                                   "eDP_adapter_dvt1.PrjPcb" ) );

    ALTIUM_PROJECT_FILES files = ParseAltiumProjectFiles( projectFile.GetFullPath() );

    BOOST_REQUIRE_EQUAL( files.schematicFiles.size(), 2 );

    wxFileName firstSchematic( projectFile.GetPath(), wxS( "it6251_core" ), wxS( "SchDoc" ) );
    wxFileName secondSchematic( projectFile.GetPath(), wxS( "power" ), wxS( "SchDoc" ) );
    wxFileName board( projectFile.GetPath(), wxS( "eDP_adapter_dvt1" ), wxS( "PcbDoc" ) );

    BOOST_CHECK_EQUAL( files.schematicFiles[0], firstSchematic.GetFullPath() );
    BOOST_CHECK_EQUAL( files.schematicFiles[1], secondSchematic.GetFullPath() );
    BOOST_CHECK_EQUAL( files.pcbFile, board.GetFullPath() );
}


BOOST_AUTO_TEST_CASE( MissingProjectHasNoFiles )
{
    wxLogNull            suppressExpectedError;
    ALTIUM_PROJECT_FILES files = ParseAltiumProjectFiles( wxS( "missing.PrjPcb" ) );

    BOOST_CHECK( files.schematicFiles.empty() );
    BOOST_CHECK( files.pcbFile.empty() );
}


BOOST_AUTO_TEST_CASE( ParseVariationFields )
{
    ALTIUM_VARIANT_ENTRY entry =
            ParseVariationString( wxS( "Designator=R1|UniqueId=ROOT\\SHEET\\ABCDEFGH|Kind=0|AlternatePart=RES_ALT|"
                                       "Comment=10k" ) );

    BOOST_CHECK_EQUAL( entry.designator, wxS( "R1" ) );
    // Only the final backslash-delimited path segment is the component's own unique id.
    BOOST_CHECK_EQUAL( entry.uniqueId, wxS( "ABCDEFGH" ) );
    BOOST_CHECK_EQUAL( entry.kind, 0 );
    BOOST_CHECK_EQUAL( entry.alternateFields[wxS( "LibReference" )], wxS( "RES_ALT" ) );
    BOOST_CHECK_EQUAL( entry.alternateFields[wxS( "Comment" )], wxS( "10k" ) );
}


BOOST_AUTO_TEST_CASE( ParseVariationBackslashBeforeSeparator )
{
    // A UniqueId path segment ending in a backslash immediately before the '|' separator must not
    // merge the following field: wxSplit's default '\' escape is disabled.
    ALTIUM_VARIANT_ENTRY entry = ParseVariationString( wxS( "UniqueId=ROOT\\WEIRD\\|Kind=1" ) );

    // The trailing '\' leaves an empty final segment; the important part is that Kind was not
    // swallowed into the UniqueId field.
    BOOST_CHECK_EQUAL( entry.kind, 1 );
}


BOOST_AUTO_TEST_CASE( ParseParamVariationFields )
{
    ALTIUM_PARAM_VARIATION variation =
            ParseParamVariationString( wxS( "ParameterName=VALUE|VariantValue=39pF" ) );

    BOOST_CHECK_EQUAL( variation.parameterName, wxS( "VALUE" ) );
    BOOST_CHECK_EQUAL( variation.variantValue, wxS( "39pF" ) );
}


BOOST_AUTO_TEST_CASE( ParseParamVariationKeepsEqualsInValue )
{
    // Only the first '=' of each pipe-delimited token separates key from value, so a value that
    // itself contains '=' must survive intact.
    ALTIUM_PARAM_VARIATION variation =
            ParseParamVariationString( wxS( "ParameterName=Formula|VariantValue=R=1k" ) );

    BOOST_CHECK_EQUAL( variation.parameterName, wxS( "Formula" ) );
    BOOST_CHECK_EQUAL( variation.variantValue, wxS( "R=1k" ) );
}


BOOST_AUTO_TEST_CASE( ParameterNameToKiCadFieldMapping )
{
    // Altium's own "Value" parameter is kept apart from KiCad's Value field, which the importers
    // fill from the component's Comment.
    BOOST_CHECK_EQUAL( AltiumParameterNameToKiCadField( wxS( "Value" ) ), wxS( "ALTIUM_VALUE" ) );
    BOOST_CHECK_EQUAL( AltiumParameterNameToKiCadField( wxS( "VALUE" ) ), wxS( "ALTIUM_VALUE" ) );
    BOOST_CHECK_EQUAL( AltiumParameterNameToKiCadField( wxS( "Comment" ) ), wxS( "Value" ) );
    BOOST_CHECK_EQUAL( AltiumParameterNameToKiCadField( wxS( "COMMENT" ) ), wxS( "Value" ) );

    // Anything else is upper-cased, matching how the base parameters are imported.
    BOOST_CHECK_EQUAL( AltiumParameterNameToKiCadField( wxS( "Tolerance" ) ), wxS( "TOLERANCE" ) );
}


BOOST_AUTO_TEST_CASE( ParseProjectVariantParameterOverrides )
{
    wxFileName projectFile( wxString::FromUTF8( KI_TEST::GetTestDataRootDir() )
                            + wxS( "pcbnew/plugins/altium/variant_parameters/"
                                   "variant_parameters.PrjPcb" ) );

    std::vector<ALTIUM_PROJECT_VARIANT> variants =
            ParseAltiumProjectVariants( projectFile.GetFullPath() );

    BOOST_REQUIRE_EQUAL( variants.size(), 2 );
    BOOST_CHECK_EQUAL( variants[0].name, wxS( "VariantA" ) );

    auto entryFor =
            [&]( const ALTIUM_PROJECT_VARIANT& aVariant, const wxString& aRef ) -> const ALTIUM_VARIANT_ENTRY*
            {
                for( const ALTIUM_VARIANT_ENTRY& e : aVariant.variations )
                {
                    if( e.designator == aRef )
                        return &e;
                }

                return nullptr;
            };

    // Three Variation entries plus one override whose designator has no Variation of its own.
    BOOST_REQUIRE_EQUAL( variants[0].variations.size(), 4 );

    // ParamVariation indices are numbered independently of the Variation entries they sit
    // between, so pairing must use ParamDesignator of the same index.
    const ALTIUM_VARIANT_ENTRY* c1 = entryFor( variants[0], wxS( "C1" ) );
    BOOST_REQUIRE( c1 );
    BOOST_CHECK_EQUAL( c1->parameterOverrides.at( wxS( "VALUE" ) ), wxS( "39pF" ) );

    // The same file uses both "VALUE" and "Value" spellings.
    const ALTIUM_VARIANT_ENTRY* r2 = entryFor( variants[0], wxS( "R2" ) );
    BOOST_REQUIRE( r2 );
    BOOST_CHECK_EQUAL( r2->parameterOverrides.at( wxS( "Value" ) ), wxS( "910R" ) );

    // A not-fitted component carries no overrides.
    const ALTIUM_VARIANT_ENTRY* r1 = entryFor( variants[0], wxS( "R1" ) );
    BOOST_REQUIRE( r1 );
    BOOST_CHECK_EQUAL( r1->kind, 1 );
    BOOST_CHECK( r1->parameterOverrides.empty() );

    // An override without a matching Variation is carried on a synthesised fitted entry rather
    // than being dropped.
    const ALTIUM_VARIANT_ENTRY* r99 = entryFor( variants[0], wxS( "R99" ) );
    BOOST_REQUIRE( r99 );
    BOOST_CHECK_EQUAL( r99->kind, 0 );
    BOOST_CHECK_EQUAL( r99->parameterOverrides.at( wxS( "Comment" ) ), wxS( "orphaned override" ) );

    // ParamVariationCount=0 leaves the plain variations untouched.
    BOOST_REQUIRE_EQUAL( variants[1].variations.size(), 1 );
    BOOST_CHECK( variants[1].variations[0].parameterOverrides.empty() );
}


BOOST_AUTO_TEST_SUITE_END()
