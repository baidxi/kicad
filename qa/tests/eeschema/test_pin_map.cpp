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

/**
 * @file test_pin_map.cpp
 * Test suite for the (pin_map) s-expression on .kicad_sym symbols.
 *
 * Pin_map is per-symbol metadata that maps symbol pin numbers to footprint pad
 * numbers without rewriting the underlying SCH_PIN numbers.  These tests cover
 * round-trip of the file format only; consumer integration (netlist export,
 * cross-probe) is tested elsewhere.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <lib_symbol.h>
#include <sch_io/kicad_sexpr/sch_io_kicad_sexpr_lib_cache.h>

#include <wx/filename.h>
#include <wx/file.h>
#include <wx/stdpaths.h>

#include <fstream>
#include <vector>

#include <boost/test/unit_test.hpp>


class PIN_MAP_FIXTURE
{
public:
    ~PIN_MAP_FIXTURE()
    {
        for( const wxString& f : m_tempFiles )
        {
            if( wxFileExists( f ) )
                wxRemoveFile( f );
        }
    }

    wxString GetTempLibraryPath()
    {
        wxString tempDir = wxStandardPaths::Get().GetTempDir();
        wxString fileName =
                tempDir + wxFileName::GetPathSeparator() + wxString::Format( "test_pin_map_%d.kicad_sym", rand() );
        m_tempFiles.push_back( fileName );
        return fileName;
    }

    void WriteFile( const wxString& aPath, const std::string& aContent )
    {
        std::ofstream f( aPath.ToStdString() );
        f << aContent;
        f.close();
    }

    /// Root "Parent" with pins 1..3 plus a derived "Derived" carrying its own pin_map.
    static std::string MakeLib()
    {
        return "(kicad_symbol_lib (version 20260507) (generator \"qa_test\")\n"
               "  (generator_version \"9.0\")\n"
               "  (symbol \"Parent\"\n"
               "    (property \"Reference\" \"U\" (at 0 0 0)\n"
               "      (effects (font (size 1.27 1.27))))\n"
               "    (property \"Value\" \"Parent\" (at 0 -2.54 0)\n"
               "      (effects (font (size 1.27 1.27))))\n"
               "    (property \"Footprint\" \"\" (at 0 0 0)\n"
               "      (effects (font (size 1.27 1.27)) (hide yes)))\n"
               "    (property \"Datasheet\" \"\" (at 0 0 0)\n"
               "      (effects (font (size 1.27 1.27)) (hide yes)))\n"
               "    (property \"Description\" \"\" (at 0 0 0)\n"
               "      (effects (font (size 1.27 1.27)) (hide yes)))\n"
               "    (pin_map\n"
               "      (entry \"1\" \"A1\")\n"
               "      (entry \"2\" \"B1\" \"B2\")\n"
               "    )\n"
               "    (symbol \"Parent_0_1\"\n"
               "      (pin passive line (at -5.08 2.54 0) (length 2.54)\n"
               "        (name \"P1\" (effects (font (size 1.27 1.27))))\n"
               "        (number \"1\" (effects (font (size 1.27 1.27)))))\n"
               "      (pin passive line (at -5.08 0 0) (length 2.54)\n"
               "        (name \"P2\" (effects (font (size 1.27 1.27))))\n"
               "        (number \"2\" (effects (font (size 1.27 1.27)))))\n"
               "      (pin passive line (at -5.08 -2.54 0) (length 2.54)\n"
               "        (name \"P3\" (effects (font (size 1.27 1.27))))\n"
               "        (number \"3\" (effects (font (size 1.27 1.27)))))\n"
               "    )\n"
               "    (embedded_fonts no)\n"
               "  )\n"
               "  (symbol \"Derived\" (extends \"Parent\")\n"
               "    (property \"Reference\" \"U\" (at 0 0 0)\n"
               "      (effects (font (size 1.27 1.27))))\n"
               "    (property \"Value\" \"Derived\" (at 0 -2.54 0)\n"
               "      (effects (font (size 1.27 1.27))))\n"
               "    (property \"Footprint\" \"\" (at 0 0 0)\n"
               "      (effects (font (size 1.27 1.27)) (hide yes)))\n"
               "    (property \"Datasheet\" \"\" (at 0 0 0)\n"
               "      (effects (font (size 1.27 1.27)) (hide yes)))\n"
               "    (property \"Description\" \"\" (at 0 0 0)\n"
               "      (effects (font (size 1.27 1.27)) (hide yes)))\n"
               "    (pin_map\n"
               "      (entry \"1\" \"X9\")\n"
               "    )\n"
               "    (embedded_fonts no)\n"
               "  )\n"
               ")\n";
    }

private:
    std::vector<wxString> m_tempFiles;
};


BOOST_FIXTURE_TEST_SUITE( PinMap, PIN_MAP_FIXTURE )


BOOST_AUTO_TEST_CASE( ParserStoresMapOnRootAndDerived )
{
    wxString libPath = GetTempLibraryPath();
    WriteFile( libPath, MakeLib() );

    SCH_IO_KICAD_SEXPR_LIB_CACHE cache( libPath );
    BOOST_REQUIRE_NO_THROW( cache.Load() );

    auto* parent = cache.GetSymbolMap().at( wxS( "Parent" ) );
    BOOST_REQUIRE( parent );

    const auto& parentMap = parent->PinMap();
    BOOST_CHECK_EQUAL( parentMap.size(), 2u );
    BOOST_REQUIRE( parentMap.count( wxS( "1" ) ) );
    BOOST_CHECK_EQUAL( parentMap.at( wxS( "1" ) ).at( 0 ), wxS( "A1" ) );
    BOOST_REQUIRE( parentMap.count( wxS( "2" ) ) );
    BOOST_REQUIRE_EQUAL( parentMap.at( wxS( "2" ) ).size(), 2u );
    BOOST_CHECK_EQUAL( parentMap.at( wxS( "2" ) ).at( 0 ), wxS( "B1" ) );
    BOOST_CHECK_EQUAL( parentMap.at( wxS( "2" ) ).at( 1 ), wxS( "B2" ) );

    auto* derived = cache.GetSymbolMap().at( wxS( "Derived" ) );
    BOOST_REQUIRE( derived );
    const auto& derivedMap = derived->PinMap();
    BOOST_CHECK_EQUAL( derivedMap.size(), 1u );
    BOOST_REQUIRE( derivedMap.count( wxS( "1" ) ) );
    BOOST_CHECK_EQUAL( derivedMap.at( wxS( "1" ) ).at( 0 ), wxS( "X9" ) );
}


BOOST_AUTO_TEST_CASE( PinNumbersAreNotMutated )
{
    // The pin_map is purely metadata; the underlying SCH_PIN numbers must remain
    // the symbol-local numbers, never the footprint-pad targets.
    wxString libPath = GetTempLibraryPath();
    WriteFile( libPath, MakeLib() );

    SCH_IO_KICAD_SEXPR_LIB_CACHE cache( libPath );
    BOOST_REQUIRE_NO_THROW( cache.Load() );

    auto* parent = cache.GetSymbolMap().at( wxS( "Parent" ) );
    BOOST_REQUIRE( parent );

    std::set<wxString> numbers;

    for( const SCH_PIN* pin : parent->GetGraphicalPins( 0, 0 ) )
        numbers.insert( pin->GetNumber() );

    BOOST_CHECK( numbers.count( wxS( "1" ) ) );
    BOOST_CHECK( numbers.count( wxS( "2" ) ) );
    BOOST_CHECK( numbers.count( wxS( "3" ) ) );
    BOOST_CHECK( !numbers.count( wxS( "A1" ) ) );
    BOOST_CHECK( !numbers.count( wxS( "B1" ) ) );
}


BOOST_AUTO_TEST_CASE( PinMapRoundTripsThroughSave )
{
    wxString libPath = GetTempLibraryPath();
    WriteFile( libPath, MakeLib() );

    SCH_IO_KICAD_SEXPR_LIB_CACHE cache( libPath );
    BOOST_REQUIRE_NO_THROW( cache.Load() );

    cache.SetModified( true );
    BOOST_REQUIRE_NO_THROW( cache.Save() );

    SCH_IO_KICAD_SEXPR_LIB_CACHE reloaded( libPath );
    BOOST_REQUIRE_NO_THROW( reloaded.Load() );

    auto* parent = reloaded.GetSymbolMap().at( wxS( "Parent" ) );
    BOOST_REQUIRE( parent );
    BOOST_CHECK_EQUAL( parent->PinMap().size(), 2u );
    BOOST_CHECK_EQUAL( parent->PinMap().at( wxS( "1" ) ).at( 0 ), wxS( "A1" ) );
    BOOST_REQUIRE_EQUAL( parent->PinMap().at( wxS( "2" ) ).size(), 2u );
    BOOST_CHECK_EQUAL( parent->PinMap().at( wxS( "2" ) ).at( 0 ), wxS( "B1" ) );
    BOOST_CHECK_EQUAL( parent->PinMap().at( wxS( "2" ) ).at( 1 ), wxS( "B2" ) );

    auto* derived = reloaded.GetSymbolMap().at( wxS( "Derived" ) );
    BOOST_REQUIRE( derived );
    BOOST_CHECK_EQUAL( derived->PinMap().size(), 1u );
    BOOST_CHECK_EQUAL( derived->PinMap().at( wxS( "1" ) ).at( 0 ), wxS( "X9" ) );
}


BOOST_AUTO_TEST_SUITE_END()
