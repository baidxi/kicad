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

// A downgraded board must use only node heads the target release knows, taken from its real
// keyword files under qa/data/downgrade.

#include <boost/test/unit_test.hpp>

#include <cctype>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include <board.h>
#include <downgrade/board_downgrade.h>
#include <downgrade_target.h>
#include <settings/settings_manager.h>
#include <pcbnew_utils/board_file_utils.h>
#include <pcbnew_utils/board_test_utils.h>


BOOST_AUTO_TEST_SUITE( DowngradeOracle )


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


// Drop quoted strings so a "(word)" inside e.g. a footprint description is not mistaken for a node.
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

    const std::string src = KI_TEST::GetPcbnewTestDataDir() + "via_off_center.kicad_pcb";

    KI_TEST::TEMPORARY_DIRECTORY tmp( "kicad_qa_downgrade_oracle", "" );
    const std::string            dest = ( tmp.GetPath() / "out.kicad_pcb" ).string();

    COMPATIBILITY_REPORT report;
    BOOST_REQUIRE( ExportBoardToOlderVersion( src, dest, aTarget, report ) );

    std::ifstream file( dest );
    std::string   content( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );

    const std::set<std::string> known =
            loadTokenSet( KI_TEST::GetPcbnewTestDataDir() + "../downgrade/" + aTokenFileName );
    BOOST_REQUIRE( !known.empty() );

    // Plot settings the target parser skips harmlessly (pcb_plot_params uses a lenient default).
    const std::set<std::string> harmless = { "pngdpi", "pngantialias" };

    std::vector<std::string> unsupported;

    for( const std::string& head : nodeHeads( stripStrings( content ) ) )
    {
        if( !known.count( head ) && !harmless.count( head ) )
            unsupported.push_back( head );
    }

    if( !unsupported.empty() )
    {
        std::string msg = "Downgraded board uses tokens " + aTarget.m_name.ToStdString() + " cannot parse:";

        for( const std::string& token : unsupported )
            msg += " " + token;

        BOOST_ERROR( msg );
    }
}


BOOST_AUTO_TEST_CASE( BoardToKicad9UsesOnlyKnownTokens )
{
    checkExportUsesOnlyKnownTokens( GetDowngradeTargets().at( 1 ), "kicad9_board_tokens.txt" );
}


BOOST_AUTO_TEST_CASE( BoardToKicad10UsesOnlyKnownTokens )
{
    checkExportUsesOnlyKnownTokens( GetDowngradeTargets().at( 0 ), "kicad10_board_tokens.txt" );
}


BOOST_AUTO_TEST_SUITE_END()
