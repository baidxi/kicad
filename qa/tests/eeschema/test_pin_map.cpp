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


BOOST_AUTO_TEST_CASE( ValidateRejectsTargetCollision )
{
    wxString libPath = GetTempLibraryPath();
    WriteFile( libPath, MakeLib() );

    SCH_IO_KICAD_SEXPR_LIB_CACHE cache( libPath );
    BOOST_REQUIRE_NO_THROW( cache.Load() );

    auto* parent = cache.GetSymbolMap().at( wxS( "Parent" ) );
    BOOST_REQUIRE( parent );

    std::map<wxString, std::vector<wxString>> colliding = {
        { wxS( "1" ), { wxS( "A1" ) } },
        { wxS( "2" ), { wxS( "A1" ) } }, // collision: same target as "1"
    };

    std::vector<wxString> errors;
    BOOST_CHECK( !parent->ValidatePinMap( colliding, &errors ) );

    bool mentionsCollision = false;

    for( const wxString& e : errors )
        if( e.Contains( wxS( "A1" ) ) )
            mentionsCollision = true;

    BOOST_CHECK( mentionsCollision );
}


BOOST_AUTO_TEST_CASE( ValidateAllowsWithinEntryDuplicates )
{
    // One symbol pin bonded to the same pad multiple times is harmless and must not
    // count as a collision against itself.
    wxString libPath = GetTempLibraryPath();
    WriteFile( libPath, MakeLib() );

    SCH_IO_KICAD_SEXPR_LIB_CACHE cache( libPath );
    BOOST_REQUIRE_NO_THROW( cache.Load() );

    auto* parent = cache.GetSymbolMap().at( wxS( "Parent" ) );
    BOOST_REQUIRE( parent );

    std::map<wxString, std::vector<wxString>> ok = {
        { wxS( "1" ), { wxS( "A1" ), wxS( "A1" ) } },
        { wxS( "2" ), { wxS( "B1" ), wxS( "B2" ) } },
    };

    std::vector<wxString> errors;
    BOOST_CHECK( parent->ValidatePinMap( ok, &errors ) );
    BOOST_CHECK( errors.empty() );
}


BOOST_AUTO_TEST_CASE( LoadDropsCollidingMap )
{
    // Library with a colliding pin_map.  Load succeeds but the map is dropped.
    wxString    libPath = GetTempLibraryPath();
    std::string lib = "(kicad_symbol_lib (version 20260507) (generator \"qa_test\")\n"
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
                      "      (entry \"2\" \"A1\")\n" // collision
                      "    )\n"
                      "    (symbol \"Parent_0_1\"\n"
                      "      (pin passive line (at -5.08 2.54 0) (length 2.54)\n"
                      "        (name \"P1\" (effects (font (size 1.27 1.27))))\n"
                      "        (number \"1\" (effects (font (size 1.27 1.27)))))\n"
                      "      (pin passive line (at -5.08 0 0) (length 2.54)\n"
                      "        (name \"P2\" (effects (font (size 1.27 1.27))))\n"
                      "        (number \"2\" (effects (font (size 1.27 1.27)))))\n"
                      "    )\n"
                      "    (embedded_fonts no)\n"
                      "  )\n"
                      ")\n";

    WriteFile( libPath, lib );

    SCH_IO_KICAD_SEXPR_LIB_CACHE cache( libPath );
    BOOST_REQUIRE_NO_THROW( cache.Load() );

    auto* parent = cache.GetSymbolMap().at( wxS( "Parent" ) );
    BOOST_REQUIRE( parent );
    BOOST_CHECK( parent->PinMap().empty() );
}


BOOST_AUTO_TEST_CASE( MapPinToPadsForward )
{
    wxString libPath = GetTempLibraryPath();
    WriteFile( libPath, MakeLib() );

    SCH_IO_KICAD_SEXPR_LIB_CACHE cache( libPath );
    BOOST_REQUIRE_NO_THROW( cache.Load() );

    auto* parent = cache.GetSymbolMap().at( wxS( "Parent" ) );
    BOOST_REQUIRE( parent );

    // Pin 1 mapped to single pad.
    auto v1 = parent->MapPinToPads( wxS( "1" ) );
    BOOST_REQUIRE_EQUAL( v1.size(), 1u );
    BOOST_CHECK_EQUAL( v1[0], wxS( "A1" ) );

    // Pin 2 mapped to multiple pads.
    auto v2 = parent->MapPinToPads( wxS( "2" ) );
    BOOST_REQUIRE_EQUAL( v2.size(), 2u );
    BOOST_CHECK_EQUAL( v2[0], wxS( "B1" ) );
    BOOST_CHECK_EQUAL( v2[1], wxS( "B2" ) );

    // Pin 3 not in map: identity.
    auto v3 = parent->MapPinToPads( wxS( "3" ) );
    BOOST_REQUIRE_EQUAL( v3.size(), 1u );
    BOOST_CHECK_EQUAL( v3[0], wxS( "3" ) );
}


BOOST_AUTO_TEST_CASE( MapPadToPinReverse )
{
    wxString libPath = GetTempLibraryPath();
    WriteFile( libPath, MakeLib() );

    SCH_IO_KICAD_SEXPR_LIB_CACHE cache( libPath );
    BOOST_REQUIRE_NO_THROW( cache.Load() );

    auto* parent = cache.GetSymbolMap().at( wxS( "Parent" ) );
    BOOST_REQUIRE( parent );

    BOOST_CHECK_EQUAL( parent->MapPadToPin( wxS( "A1" ) ), wxS( "1" ) );
    BOOST_CHECK_EQUAL( parent->MapPadToPin( wxS( "B1" ) ), wxS( "2" ) );
    BOOST_CHECK_EQUAL( parent->MapPadToPin( wxS( "B2" ) ), wxS( "2" ) );

    // Pad not used as a target anywhere: identity.
    BOOST_CHECK_EQUAL( parent->MapPadToPin( wxS( "Z9" ) ), wxS( "Z9" ) );
}


BOOST_AUTO_TEST_CASE( MapHelpersIdentityWhenNoMap )
{
    LIB_SYMBOL bare( wxS( "Bare" ) );

    BOOST_REQUIRE( bare.PinMap().empty() );
    BOOST_CHECK_EQUAL( bare.MapPinToPads( wxS( "7" ) ).size(), 1u );
    BOOST_CHECK_EQUAL( bare.MapPinToPads( wxS( "7" ) )[0], wxS( "7" ) );
    BOOST_CHECK_EQUAL( bare.MapPadToPin( wxS( "Z9" ) ), wxS( "Z9" ) );
}


BOOST_AUTO_TEST_SUITE_END()


// ---------------------------------------------------------------------------
// XML netlist exporter integration: confirms pin_map translation reaches the
// emitted <node pin="..."> entries that pcbnew consumes.
// ---------------------------------------------------------------------------

#include <schematic.h>
#include <settings/settings_manager.h>
#include <netlist_exporter_xml.h>
#include <schematic_utils/schematic_file_util.h>

#include <wx/xml/xml.h>


struct PIN_MAP_NETLIST_FIXTURE
{
    SETTINGS_MANAGER           m_settingsManager;
    std::unique_ptr<SCHEMATIC> m_schematic;
};


static std::vector<wxXmlNode*> netlist_find_children( wxXmlNode* aParent, const wxString& aName )
{
    std::vector<wxXmlNode*> out;

    for( wxXmlNode* child = aParent->GetChildren(); child; child = child->GetNext() )
    {
        if( child->GetName() == aName )
            out.push_back( child );
    }

    return out;
}


static wxXmlNode* netlist_find_child( wxXmlNode* aParent, const wxString& aName )
{
    for( wxXmlNode* child = aParent->GetChildren(); child; child = child->GetNext() )
    {
        if( child->GetName() == aName )
            return child;
    }

    return nullptr;
}


BOOST_FIXTURE_TEST_CASE( NetlistExporterXML_PinMapTranslatesNodes, PIN_MAP_NETLIST_FIXTURE )
{
    KI_TEST::LoadSchematic( m_settingsManager, wxT( "pin_map_netlist" ), m_schematic );

    wxFileName netFile = m_schematic->Project().GetProjectFullName();
    netFile.SetName( netFile.GetName() + wxT( "_pin_map_netlist_test" ) );
    netFile.SetExt( wxT( "xml" ) );

    if( wxFileExists( netFile.GetFullPath() ) )
        wxRemoveFile( netFile.GetFullPath() );

    WX_STRING_REPORTER                    reporter;
    std::unique_ptr<NETLIST_EXPORTER_XML> exporter = std::make_unique<NETLIST_EXPORTER_XML>( m_schematic.get() );

    bool success = exporter->WriteNetlist( netFile.GetFullPath(), 0, reporter );
    BOOST_REQUIRE( success && reporter.GetMessages().IsEmpty() );

    wxXmlDocument xdoc;
    BOOST_REQUIRE( xdoc.Load( netFile.GetFullPath() ) );

    wxXmlNode* root = xdoc.GetRoot();
    BOOST_REQUIRE( root );

    wxXmlNode* nets = netlist_find_child( root, wxT( "nets" ) );
    BOOST_REQUIRE( nets );

    // Collect U1's emitted pad numbers per net.
    std::map<wxString, std::set<wxString>> u1ByNet;

    for( wxXmlNode* net : netlist_find_children( nets, wxT( "net" ) ) )
    {
        wxString netName = net->GetAttribute( wxT( "name" ), wxEmptyString );

        for( wxXmlNode* node : netlist_find_children( net, wxT( "node" ) ) )
        {
            if( node->GetAttribute( wxT( "ref" ), wxEmptyString ) == wxT( "U1" ) )
            {
                u1ByNet[netName].insert( node->GetAttribute( wxT( "pin" ), wxEmptyString ) );
            }
        }
    }

    // Net names from connection_graph carry the prefix /; locate by suffix match.
    auto netByLabel = [&]( const wxString& aLabel ) -> std::set<wxString>
    {
        for( const auto& [name, pads] : u1ByNet )
        {
            if( name.EndsWith( aLabel ) )
                return pads;
        }

        return {};
    };

    // The schematic places three labels on three wires.  Schematic-side Y is inverted
    // relative to the lib symbol's Y, so the bottom-most label (NET_A in the fixture)
    // catches pin 1, NET_B catches pin 2, and NET_C catches pin 3.
    //
    // Pin 1 -> A1 (single mapped pad).
    BOOST_CHECK( netByLabel( wxT( "NET_A" ) ) == std::set<wxString>{ wxS( "A1" ) } );

    // Pin 2 -> [B1, B2] (multi-pad: emits two <node> entries).
    BOOST_CHECK( netByLabel( wxT( "NET_B" ) ) == ( std::set<wxString>{ wxS( "B1" ), wxS( "B2" ) } ) );

    // Pin 3 not in pin_map -> identity ("3").
    BOOST_CHECK( netByLabel( wxT( "NET_C" ) ) == std::set<wxString>{ wxS( "3" ) } );

    wxRemoveFile( netFile.GetFullPath() );
}


// ---------------------------------------------------------------------------
// Schematic API: SchematicSymbol carries pin_map across Serialize/Deserialize.
// ---------------------------------------------------------------------------

#include <sch_symbol.h>
#include <api/schematic/schematic_types.pb.h>


BOOST_AUTO_TEST_CASE( ApiSchSymbolPinMapRoundTrip )
{
    // Build a LIB_SYMBOL with a pin_map; wrap it in a SCH_SYMBOL.
    LIB_SYMBOL libSym( wxS( "Mapped" ) );

    std::map<wxString, std::vector<wxString>> map;
    map[wxS( "1" )] = { wxS( "A1" ) };
    map[wxS( "2" )] = { wxS( "B1" ), wxS( "B2" ) };
    libSym.SetPinMap( map );

    SCH_SYMBOL src;
    src.SetLibSymbol( libSym.Flatten().release() );

    google::protobuf::Any any;
    src.Serialize( any );

    SCH_SYMBOL dst;
    BOOST_REQUIRE( dst.Deserialize( any ) );

    BOOST_REQUIRE( dst.GetLibSymbolRef() );
    const auto& got = dst.GetLibSymbolRef()->PinMap();

    BOOST_CHECK_EQUAL( got.size(), 2u );
    BOOST_REQUIRE( got.count( wxS( "1" ) ) );
    BOOST_CHECK_EQUAL( got.at( wxS( "1" ) ).at( 0 ), wxS( "A1" ) );
    BOOST_REQUIRE( got.count( wxS( "2" ) ) );
    BOOST_REQUIRE_EQUAL( got.at( wxS( "2" ) ).size(), 2u );
    BOOST_CHECK_EQUAL( got.at( wxS( "2" ) ).at( 0 ), wxS( "B1" ) );
    BOOST_CHECK_EQUAL( got.at( wxS( "2" ) ).at( 1 ), wxS( "B2" ) );
}
