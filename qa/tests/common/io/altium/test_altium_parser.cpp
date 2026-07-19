/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.TXT for contributors.
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

/**
 * @file test_altium_parser.cpp
 * Test suite for #ALTIUM_BINARY_PARSER
 */

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <boost/test/data/test_case.hpp>

#include <common/io/altium/altium_binary_parser.h>
#include <common/io/altium/altium_props_utils.h>

struct ALTIUM_BINARY_PARSER_FIXTURE
{
    ALTIUM_BINARY_PARSER_FIXTURE() {}
};


/**
 * Declares the struct as the Boost test fixture.
 */
BOOST_FIXTURE_TEST_SUITE( AltiumParser, ALTIUM_BINARY_PARSER_FIXTURE )

/**
 * A list of valid internal unit conversation factors
 * Rem: altium to kicad importer rounds coordinates to the near 10 nm value
 * when converting altium values in 0.01 mil to pcbnew units (1 nm)
 */
static const std::vector<std::tuple<int, int>> altium_to_kicad_unit = {
    // Some simple values
    { 0, 0 },
    { 1, 0 },
    { 2, 10 },
    { 3, 10 },
    { 10, 30 },
    { 20, 50 },
    { 30, 80 },
    // Edge Cases
    { 845466002, 2147483640 },
    { -845466002, -2147483640 },
    // Clamp bigger values
    { 845466003, 2147483640 },
    { -845466003, -2147483640 },
    { 1000000000, 2147483640 },
    { -1000000000, -2147483640 },
    // imperial rounded units as input (rounded to the near 10 nm value)
    { 100, 250 },
    { 200, 510 },
    { 300, 760 },
    { 400, 1020 },
    { 500, 1270 },
    { 600, 1520 },
    { 700, 1780 },
    { 800, 2030 },
    { 900, 2290 },
    { 1000, 2540 },
    // metric rounded units as input
    { 394, 1000 },
    { 787, 2000 },
    { 1181, 3000 },
    { 1575, 4000 },
    { 1969, 5000 },
    { 2362, 6000 },
    { 2756, 7000 },
    { 3150, 8000 },
    { 3543, 9000 },
    { 3937, 10000 },
    { 39370, 100000 },
    { 78740, 200000 },
    { 118110, 300000 },
    { 157480, 400000 },
    { 196850, 500000 },
    { 236220, 600000 },
    { 275591, 700000 },
    { 314961, 800000 },
    { 354331, 900000 },
    { 393701, 1000000 },
    { -394, -1000 },
    { -787, -2000 },
    { -1181, -3000 },
    { -1575, -4000 },
    { -1969, -5000 },
    { -2362, -6000 },
    { -2756, -7000 },
    { -3150, -8000 },
    { -3543, -9000 },
    { -3937, -10000 },
    { -39370, -100000 },
    { -78740, -200000 },
    { -118110, -300000 },
    { -157480, -400000 },
    { -196850, -500000 },
    { -236220, -600000 },
    { -275591, -700000 },
    { -314961, -800000 },
    { -354331, -900000 },
    { -393701, -1000000 },
};

/**
 * Test conversation from Altium internal units into KiCad internal units
 */
BOOST_DATA_TEST_CASE( ConvertToKicadUnit,
                      boost::unit_test::data::make(altium_to_kicad_unit),
                      input_value,
                      expected_result )
{
    int result = ALTIUM_PROPS_UTILS::ConvertToKicadUnit( input_value );

    // These are all valid
    BOOST_CHECK_EQUAL( result, expected_result );
}

/**
 * A list of valid test strings and the expected results
 */
static const std::vector<std::tuple<wxString, int>> read_kicad_unit_property = {
    // Empty (use default)
    { "", 0 },
    // Some simple cases
    { "0mil", 0 },
    { "1mil", 25400 },
    { "+1mil", 25400 },
    { "-1mil", -25400 },
    // Decimal Places
    { "0.1mil", 2540 },
    { "-0.1mil", -2540 },
    { "0.01mil", 250 },
    { "-0.01mil", -250 },
    { "0.001mil", 30 },
    { "-0.001mil", -30 },
    { "0.0001mil", 0 },
    { "-0.0001mil", 0 },
    { "0.00001mil", 0 },
    { "-0.00001mil", -0 },
    // Big Numbers
    { "10mil", 254000 },
    { "-10mil", -254000 },
    { "100mil", 2540000 },
    { "-100mil", -2540000 },
    { "1000mil", 25400000 },
    { "-1000mil", -25400000 },
    { "10000mil", 254000000 },
    { "-10000mil", -254000000 },
    // Edge Cases, values are rounded to the near available 10nm
    { "84546.6002mil", 2147483640 },
    { "-84546.6002mil", -2147483640 },
    // Clamp bigger values, values are rounded to the near available 10nm
    { "84546.6003mil", 2147483640 },
    { "-84546.6003mil", -2147483640 },
    { "100000mil", 2147483640 },
    { "-100000mil", -2147483640 },
    { "1000000mil", 2147483640 },
    { "-1000000mil", -2147483640 },
    { "10000000mil", 2147483640 },
    { "-10000000mil", -2147483640 },
    // Incorrect suffix
    { "100", 0 },
    { "100mils", 0 },
    // Incorrect prefix
    { "a100mil", 0 },
};


/**
 * Test conversation from Unit property into KiCad internal units
 */
BOOST_DATA_TEST_CASE( PropertiesReadKicadUnit,
                      boost::unit_test::data::make(read_kicad_unit_property),
                      input_value,
                      expected_result )
{
    std::map<wxString, wxString> properties = { { "TEST", input_value } };

    int result = ALTIUM_PROPS_UTILS::ReadKicadUnit( properties, "TEST", "0mil" );

    // These are all valid
    BOOST_CHECK_EQUAL( result, expected_result );
}


BOOST_AUTO_TEST_CASE( MalformedTypedPropertiesUseDefaults )
{
    std::map<wxString, wxString> properties = { { "INVALID", "not-a-number" }, { "TRAILING", "12garbage" },
                                                { "INTEGER", "-12" },          { "DOUBLE", "1.25e2" },
                                                { "FALSE", "FALSE" },          { "TRUE", "true" },
                                                { "UNIT", "broken" } };

    BOOST_CHECK_EQUAL( ALTIUM_PROPS_UTILS::ReadInt( properties, "INVALID", 42 ), 42 );
    BOOST_CHECK_EQUAL( ALTIUM_PROPS_UTILS::ReadInt( properties, "TRAILING", 42 ), 42 );
    BOOST_CHECK_EQUAL( ALTIUM_PROPS_UTILS::ReadInt( properties, "INTEGER", 42 ), -12 );

    BOOST_CHECK_CLOSE( ALTIUM_PROPS_UTILS::ReadDouble( properties, "INVALID", 3.5 ), 3.5, 1e-9 );
    BOOST_CHECK_CLOSE( ALTIUM_PROPS_UTILS::ReadDouble( properties, "TRAILING", 3.5 ), 3.5, 1e-9 );
    BOOST_CHECK_CLOSE( ALTIUM_PROPS_UTILS::ReadDouble( properties, "DOUBLE", 3.5 ), 125.0, 1e-9 );

    BOOST_CHECK( !ALTIUM_PROPS_UTILS::ReadBool( properties, "FALSE", true ) );
    BOOST_CHECK( ALTIUM_PROPS_UTILS::ReadBool( properties, "TRUE", false ) );
    BOOST_CHECK( ALTIUM_PROPS_UTILS::ReadBool( properties, "INVALID", true ) );

    BOOST_CHECK_EQUAL( ALTIUM_PROPS_UTILS::ReadKicadUnit( properties, "UNIT", "5mil" ), 127000 );
}


/**
 * A list of valid test strings and the expected result map
 */
static const std::vector<std::tuple<std::string, std::map<wxString, wxString>>> read_properties = {
    // Empty
    { "", {} },
    { "\0", {} },
    { "|", {} },
    { "|\0", {} },
    { "||", {} },
    { "||\0", {} },
    // Empty key-value pair
    { "|=", { { "", "" } } },
    { "|=\0", { { "", "" } } },
    { "|  =  ", { { "", "" } } },
    { "|  =  \0", { { "", "" } } },
    // Single key-value pair
    { "|A=\0", { { "A", "" } } },
    { "|A=B", { { "A", "B" } } },
    { "|A=B\0", { { "A", "B" } } },
    { "|A=B|", { { "A", "B" } } },
    { "|A=B|\0", { { "A", "B" } } },
    { "A=\0", { { "A", "" } } },
    { "A=B", { { "A", "B" } } },
    { "A=B\0", { { "A", "B" } } },
    { "A=B|", { { "A", "B" } } },
    { "A=B|\0", { { "A", "B" } } },
    // Multiple key-value pairs
    { "|A=B|C=D|\0", { { "A", "B" }, { "C", "D" } } },
    { "A=B|C=D|\0", { { "A", "B" }, { "C", "D" } } },
    // Same key multiple times
    { "|A=B|A=C\0", { { "A", "B" } } },
    { "|A=B|A=C|A=D|A=E|A=F\0", { { "A", "B" } } },
    // Always upper case key
    { "|a=b\0", { { "A", "b" } } },
    { "|abc123=b\0", { { "ABC123", "b" } } },
    // Trim whitespaces, TODO: correct?
    { "|A=  B\0", { { "A", "  B" } } },
    { "|A=B  \0", { { "A", "B" } } },
    { "|  A=B\0", { { "A", "B" } } },
    { "|A  =B\0", { { "A", "B" } } },
    { "|A=\nB\n\0", { { "A", "\nB" } } },
    { "A=  B\0", { { "A", "  B" } } },
    { "A=B  \0", { { "A", "B" } } },
    { "  A=B\0", { { "A", "B" } } },
    { "A  =B\0", { { "A", "B" } } },
    { "A=\nB\n\0", { { "A", "\nB" } } },
    // Escaping and other special cases, TODO: extend
    //{ "|A=||\0", {{"A", "|"}} },
    { "|A==\0", { { "A", "=" } } },
    { "|A=a\na\0", { { "A", "a\na" } } },
    { "|A=a\ta\0", { { "A", "a\ta" } } },
    // Encoding, TODO: extend
    { "|%UTF8%A=abc\0", { { "%UTF8%A", "abc" } } },
    { "|%UTF8%A=\xc2\xa6\0", { { "%UTF8%A", { "\xc2\xa6", wxConvUTF8 } } } }, // Convert to '|' ?
    // Correct reading errors
    { "|A|B=C\0", { { "B", "C" } } },
    { "|A=B|C\0", { { "A", "B" } } },
    // DESIGNATOR/NAME/TEXT overbar conversion is exempt for RECORD=4 (LABEL) and applied for
    // any other record type, independent of whether RECORD precedes or trails the text key.
    { "|RECORD=4|TEXT=\\A\\B\0", { { "RECORD", "4" }, { "TEXT", "\\A\\B" } } },
    { "|TEXT=\\A\\B|RECORD=4\0", { { "RECORD", "4" }, { "TEXT", "\\A\\B" } } },
    { "|RECORD=25|TEXT=\\A\\B\0", { { "RECORD", "25" }, { "TEXT", "~{A}B" } } },
    { "|TEXT=\\A\\B|RECORD=25\0", { { "RECORD", "25" }, { "TEXT", "~{A}B" } } },
};

/**
 * Test conversation from binary to properties
 */
BOOST_DATA_TEST_CASE( ReadProperties,
                      boost::unit_test::data::make(read_properties),
                      input_value,
                      expected_result )
{
    size_t                  size = 4 + input_value.size();
    std::unique_ptr<char[]> content = std::make_unique<char[]>( size );

    *content.get() = input_value.size();
    std::memcpy( content.get() + 4, input_value.c_str(), input_value.size() );

    ALTIUM_BINARY_PARSER parser( content, size );

    std::map<wxString, wxString> result = parser.ReadProperties();

    BOOST_CHECK_EQUAL( parser.HasParsingError(), false );
    BOOST_CHECK_EQUAL( parser.GetRemainingBytes(), 0 );

    BOOST_CHECK_EQUAL_COLLECTIONS( result.begin(), result.end(),
                                   expected_result.begin(), expected_result.end() );
}


/**
 * Verify that ReadProperties does not strip a trailing 0x00 byte from binary records.
 * Binary records use the MSB of the length field as a flag; their payload is raw compressed
 * data where 0x00 is a legitimate final byte. Stripping it corrupts the zlib stream.
 * Regression test for https://gitlab.com/kicad/code/kicad/-/issues/23013
 */
BOOST_AUTO_TEST_CASE( ReadPropertiesBinaryNullBytePreserved )
{
    // Build a binary record whose payload ends with 0x00.
    // The MSB of the 4-byte length field signals "binary" to ReadProperties.
    const std::string payload = { '\x01', '\x02', '\x03', '\x00' };
    uint32_t          rawLength = static_cast<uint32_t>( payload.size() ) | 0x01000000;

    size_t                  totalSize = 4 + payload.size();
    std::unique_ptr<char[]> content = std::make_unique<char[]>( totalSize );

    std::memcpy( content.get(), &rawLength, 4 );
    std::memcpy( content.get() + 4, payload.data(), payload.size() );

    // Capture the raw string that handleBinaryData receives so we can check its length.
    std::string captured;

    auto captureBinaryData = [&]( const std::string& aData ) -> std::map<wxString, wxString>
    {
        captured = aData;
        return {};
    };

    ALTIUM_BINARY_PARSER parser( content, totalSize );

    parser.ReadProperties( captureBinaryData );

    BOOST_CHECK_EQUAL( parser.HasParsingError(), false );
    BOOST_CHECK_EQUAL( captured.size(), payload.size() );
    BOOST_CHECK_EQUAL( captured, payload );
}


/**
 * Some Altium files store text that is UTF-8 which has already been misread as Latin-1 and then
 * re-encoded as UTF-8 (classic "mojibake": "µ" becomes "Âµ"), including in the '%UTF8%' twins that
 * are otherwise assumed to be correct UTF-8.  RepairDoubleEncodedUtf8() must recover the real text
 * while leaving correctly-decoded values untouched.
 */
BOOST_AUTO_TEST_CASE( RepairDoubleEncodedUtf8 )
{
    const wxString correct = wxString::FromUTF8( "3.30 \xc2\xb5H" );          // "3.30 µH" (µ = U+00B5)
    const wxString mojibake = wxString::FromUTF8( "3.30 \xc3\x82\xc2\xb5H" ); // "3.30 ÂµH"

    // The double-encoded value is repaired ...
    BOOST_CHECK_EQUAL( ALTIUM_PROPS_UTILS::RepairDoubleEncodedUtf8( mojibake ), correct );

    // ... while an already-correct value (whose Latin-1 byte 0xB5 is not valid stand-alone UTF-8)
    // is returned unchanged.
    BOOST_CHECK_EQUAL( ALTIUM_PROPS_UTILS::RepairDoubleEncodedUtf8( correct ), correct );

    // Plain ASCII and genuine Unicode (codepoints beyond Latin-1) are never altered.
    BOOST_CHECK_EQUAL( ALTIUM_PROPS_UTILS::RepairDoubleEncodedUtf8( wxT( "3.30 uH" ) ), wxT( "3.30 uH" ) );

    const wxString ohm = wxString::FromUTF8( "10 \xce\xa9" ); // "10 Ω" (Ω = U+03A9)
    BOOST_CHECK_EQUAL( ALTIUM_PROPS_UTILS::RepairDoubleEncodedUtf8( ohm ), ohm );

    BOOST_CHECK_EQUAL( ALTIUM_PROPS_UTILS::RepairDoubleEncodedUtf8( wxEmptyString ), wxEmptyString );

    // Genuine Latin-1 text whose high bytes do not form valid UTF-8 must be left untouched.  These
    // guard against the repair over-reaching onto real accented content.
    const wxString german = wxString::FromUTF8( "Gr\xc3\xb6\xc3\x9f" "e" ); // "Größe" (ö U+00F6, ß U+00DF)
    BOOST_CHECK_EQUAL( ALTIUM_PROPS_UTILS::RepairDoubleEncodedUtf8( german ), german );

    const wxString plusMinus = wxString::FromUTF8( "\xc2\xb1"
                                                   "5%" ); // "±5%" (± U+00B1)
    BOOST_CHECK_EQUAL( ALTIUM_PROPS_UTILS::RepairDoubleEncodedUtf8( plusMinus ), plusMinus );

    const wxString degrees = wxString::FromUTF8( "50 \xc2\xb0"
                                                 "C" ); // "50 °C" (° U+00B0)
    BOOST_CHECK_EQUAL( ALTIUM_PROPS_UTILS::RepairDoubleEncodedUtf8( degrees ), degrees );
}


/**
 * End-to-end: a property block whose '%UTF8%' twin is double-encoded (as seen in real Altium
 * ComponentDescription fields) must be recovered by ReadString().
 */
BOOST_AUTO_TEST_CASE( ReadStringRepairsDoubleEncodedUtf8 )
{
    // "3.30 µH": real bytes 0xC2 0xB5; the double-encoded twin is 0xC3 0x82 0xC2 0xB5.
    std::string block = "|%UTF8%TEXT=3.30 \xc3\x82\xc2\xb5H|TEXT=3.30 \xc2\xb5H";
    block.push_back( '\0' );

    size_t                  size = 4 + block.size();
    std::unique_ptr<char[]> content = std::make_unique<char[]>( size );

    *content.get() = static_cast<char>( block.size() );
    std::memcpy( content.get() + 4, block.data(), block.size() );

    ALTIUM_BINARY_PARSER         parser( content, size );
    std::map<wxString, wxString> props = parser.ReadProperties();

    BOOST_CHECK_EQUAL( parser.HasParsingError(), false );

    const wxString expected = wxString::FromUTF8( "3.30 \xc2\xb5H" );
    BOOST_CHECK_EQUAL( ALTIUM_PROPS_UTILS::ReadString( props, "TEXT", "" ), expected );
}


BOOST_AUTO_TEST_SUITE_END()
