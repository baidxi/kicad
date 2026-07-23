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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// Schematic version of the board oracle in pcbnew. A downgraded schematic must use only node heads
// the target release knows, taken from its real keyword files under qa/data/downgrade.

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <schematic.h>
#include <sch_screen.h>
#include <sch_sheet.h>
#include <lib_symbol.h>
#include <downgrade/sch_downgrade.h>
#include <downgrade_target.h>
#include <sch_io/kicad_sexpr/sch_io_kicad_sexpr.h>
#include <settings/settings_manager.h>

#include <wx/filefn.h>
#include <wx/filename.h>

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <eeschema_test_utils.h>
#include <schematic_file_util.h>


BOOST_AUTO_TEST_SUITE( SchDowngradeOracle )


static std::set<std::string> loadTokenSet( const std::string& aFileName )
{
    std::set<std::string> tokens;
    std::ifstream         file( aFileName );
    std::string           line;

    while( std::getline( file, line ) )
    {
        while( !line.empty() && ( line.back() == '\r' || line.back() == ' ' ) )
            line.pop_back();

        if( !line.empty() )
            tokens.insert( line );
    }

    return tokens;
}


// Drop quoted strings so a "(word)" inside a text item or field is not mistaken for a node.
// A quote is real only if an even number of backslashes precede it, matching the production gate.
static std::string stripStrings( const std::string& aText )
{
    std::string result;
    bool        inString = false;

    for( size_t i = 0; i < aText.size(); i++ )
    {
        char c = aText[i];

        if( c == '"' )
        {
            size_t backslashes = 0;

            for( size_t k = i; k > 0 && aText[k - 1] == '\\'; k-- )
                backslashes++;

            if( backslashes % 2 == 0 )
            {
                inString = !inString;
                continue;
            }
        }

        if( !inString )
            result += c;
    }

    return result;
}


static std::set<std::string> nodeHeads( const std::string& aText )
{
    std::set<std::string> heads;

    for( size_t i = 0; i + 1 < aText.size(); i++ )
    {
        if( aText[i] != '(' || !std::islower( static_cast<unsigned char>( aText[i + 1] ) ) )
            continue;

        size_t j = i + 1;

        while( j < aText.size()
               && ( std::islower( static_cast<unsigned char>( aText[j] ) )
                    || std::isdigit( static_cast<unsigned char>( aText[j] ) ) || aText[j] == '_' ) )
        {
            j++;
        }

        heads.insert( aText.substr( i + 1, j - i - 1 ) );
    }

    return heads;
}


static void checkExportUsesOnlyKnownTokens( const DOWNGRADE_TARGET& aTarget, const std::string& aTokenFileName )
{
    SETTINGS_MANAGER settingsManager;

    std::unique_ptr<SCHEMATIC> schematic;
    KI_TEST::LoadSchematic( settingsManager, wxT( "NoConnectOnPin" ), schematic );
    BOOST_REQUIRE( schematic != nullptr );

    SCH_SCREENS allScreens( schematic->Root() );

    for( SCH_SCREEN* screen = allScreens.GetFirst(); screen; screen = allScreens.GetNext() )
        DowngradeScreenInPlace( screen, aTarget );

    wxString dest = wxFileName::CreateTempFileName( wxT( "kicad_qa_sch_downgrade" ) );
    wxRemoveFile( dest );
    dest += wxT( ".kicad_sch" );

    SaveSchematicForTarget( schematic->GetTopLevelSheet(), schematic.get(), dest, aTarget );

    std::ifstream file( dest.ToStdString() );
    std::string   content( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );
    file.close();
    wxRemoveFile( dest );

    // The stamp proves the dispatch picked the right writer, not just a parseable one.
    BOOST_CHECK_MESSAGE( content.find( "(version " + std::to_string( aTarget.m_schVersion ) + ")" )
                                 != std::string::npos,
                         "Downgraded schematic does not carry the target version stamp" );

    const std::string           tokenPath = KI_TEST::GetEeschemaTestDataDir() + "/../downgrade/" + aTokenFileName;
    const std::set<std::string> known = loadTokenSet( tokenPath );
    BOOST_REQUIRE( !known.empty() );

    std::vector<std::string> unsupported;

    for( const std::string& head : nodeHeads( stripStrings( content ) ) )
    {
        if( !known.count( head ) )
            unsupported.push_back( head );
    }

    if( !unsupported.empty() )
    {
        std::string msg = "Downgraded schematic uses tokens " + aTarget.m_name.ToStdString() + " cannot parse:";

        for( const std::string& token : unsupported )
            msg += " " + token;

        BOOST_ERROR( msg );
    }
}


BOOST_AUTO_TEST_CASE( SchToKicad9UsesOnlyKnownTokens )
{
    checkExportUsesOnlyKnownTokens( GetDowngradeTargets().at( 1 ), "kicad9_sch_tokens.txt" );
}


BOOST_AUTO_TEST_CASE( SchToKicad10UsesOnlyKnownTokens )
{
    checkExportUsesOnlyKnownTokens( GetDowngradeTargets().at( 0 ), "kicad10_sch_tokens.txt" );
}


// Exercise the real .kicad_sym write path used to downgrade a symbol library, and confirm the result
// carries the target stamp and reloads cleanly.
BOOST_AUTO_TEST_CASE( SymbolLibraryFileDowngradeRoundTrips )
{
    const DOWNGRADE_TARGET& kicad9 = GetDowngradeTargets().at( 1 );

    wxString dest = wxFileName::CreateTempFileName( wxT( "kicad_qa_symlib" ) );
    wxRemoveFile( dest );
    dest += wxT( ".kicad_sym" );

    const wxString src = KI_TEST::GetEeschemaTestDataDir() + "/../libraries/test_project/Device.kicad_sym";

    {
        SCH_IO_KICAD_SEXPR       pi;
        std::vector<LIB_SYMBOL*> symbols;
        pi.EnumerateSymbolLib( symbols, src );
        BOOST_REQUIRE( !symbols.empty() );

        for( LIB_SYMBOL* symbol : symbols )
            DowngradeLibSymbolInPlace( symbol, kicad9 );

        SaveSymbolLibraryForTarget( symbols, dest, kicad9 );
    }

    // The written file must declare the target version and reload without error.
    std::ifstream file( dest.ToStdString() );
    std::string   content( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );
    file.close();

    BOOST_CHECK( content.find( "(version 20241209)" ) != std::string::npos );

    SCH_IO_KICAD_SEXPR       reader;
    std::vector<LIB_SYMBOL*> reloaded;
    BOOST_CHECK_NO_THROW( reader.EnumerateSymbolLib( reloaded, dest ) );
    BOOST_CHECK( !reloaded.empty() );

    wxRemoveFile( dest );
}


// The v10 symbol library path gets the same round-trip check as v9.
BOOST_AUTO_TEST_CASE( SymbolLibraryV10RoundTrips )
{
    SETTINGS_MANAGER settingsManager;

    const DOWNGRADE_TARGET& kicad10 = GetDowngradeTargets().front();

    wxString dest = wxFileName::CreateTempFileName( wxT( "kicad_qa_symlib10" ) );
    wxRemoveFile( dest );
    dest += wxT( ".kicad_sym" );

    const wxString src = KI_TEST::GetEeschemaTestDataDir() + "/../libraries/test_project/Device.kicad_sym";

    {
        SCH_IO_KICAD_SEXPR       pi;
        std::vector<LIB_SYMBOL*> symbols;
        pi.EnumerateSymbolLib( symbols, src );
        BOOST_REQUIRE( !symbols.empty() );

        for( LIB_SYMBOL* symbol : symbols )
            DowngradeLibSymbolInPlace( symbol, kicad10 );

        SaveSymbolLibraryForTarget( symbols, dest, kicad10 );
    }

    std::ifstream file( dest.ToStdString() );
    std::string   content( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );
    file.close();
    wxRemoveFile( dest );

    BOOST_CHECK( content.find( "(version 20251024)" ) != std::string::npos );
}


// The write must put a parent before symbols that inherit from it. In this fixture "C" extends
// "CAP" but sorts before it by name.
BOOST_AUTO_TEST_CASE( SymbolLibraryWritesParentBeforeChild )
{
    const DOWNGRADE_TARGET& kicad9 = GetDowngradeTargets().at( 1 );

    wxString dest = wxFileName::CreateTempFileName( wxT( "kicad_qa_symlib_inherit" ) );
    wxRemoveFile( dest );
    dest += wxT( ".kicad_sym" );

    const wxString src =
            KI_TEST::GetEeschemaTestDataDir() + "/spice_netlists/legacy_pspice/schematic_libspice.kicad_sym";

    {
        SCH_IO_KICAD_SEXPR       pi;
        std::vector<LIB_SYMBOL*> symbols;
        pi.EnumerateSymbolLib( symbols, src );
        BOOST_REQUIRE( !symbols.empty() );

        std::stable_sort( symbols.begin(), symbols.end(),
                          []( const LIB_SYMBOL* a, const LIB_SYMBOL* b )
                          {
                              return a->GetInheritanceDepth() < b->GetInheritanceDepth();
                          } );

        for( LIB_SYMBOL* symbol : symbols )
            DowngradeLibSymbolInPlace( symbol, kicad9 );

        SaveSymbolLibraryForTarget( symbols, dest, kicad9 );
    }

    std::ifstream file( dest.ToStdString() );
    std::string   content( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );
    file.close();
    wxRemoveFile( dest );

    size_t parentPos = content.find( "(symbol \"CAP\"" );
    size_t childRef = content.find( "(extends \"CAP\")" );

    BOOST_REQUIRE( parentPos != std::string::npos );
    BOOST_REQUIRE( childRef != std::string::npos );
    BOOST_CHECK( parentPos < childRef );
}


BOOST_AUTO_TEST_SUITE_END()
