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

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <memory>

#include <google/protobuf/any.pb.h>

#include <font/font.h>
#include <font/font_metrics.h>
#include <lib_symbol.h>
#include <pin_layout_cache.h>
#include <richio.h>
#include <sch_io/kicad_sexpr/sch_io_kicad_sexpr.h>
#include <sch_pin.h>
#include <sch_symbol.h>
#include <settings/settings_manager.h>

namespace
{
/// Long enough that a bracketed block fits on one line, so the multi-line fallback only fires
/// where a case deliberately shortens the pin.
constexpr int LONG_PIN = 5000000;

constexpr int TEXT_SIZE = 50000;


wxString format( const wxString& aNumber, PIN_NUMBER_DISPLAY aDisplay, int aPinLength = LONG_PIN )
{
    KIFONT::FONT* font = KIFONT::FONT::GetFont( wxEmptyString );

    return FormatStackedPinForDisplay( aNumber, aPinLength, TEXT_SIZE, font,
                                       KIFONT::METRICS::Default(), aDisplay );
}


SCH_PIN* addPin( LIB_SYMBOL& aSymbol, const wxString& aNumber )
{
    SCH_PIN* pin = new SCH_PIN( &aSymbol );
    pin->SetName( wxS( "GND" ) );
    pin->SetNumber( aNumber );
    aSymbol.AddDrawItem( pin );

    return pin;
}


/// A pin laid out well clear of the symbol body, so the indicator geometry is unconstrained.
SCH_PIN* addLaidOutPin( LIB_SYMBOL& aSymbol, const wxString& aNumber )
{
    aSymbol.SetPinNameOffset( 0 );

    SCH_PIN* pin = addPin( aSymbol, aNumber );
    pin->SetPosition( VECTOR2I( 0, 0 ) );
    pin->SetOrientation( PIN_ORIENTATION::PIN_RIGHT );
    pin->SetLength( schIUScale.MilsToIU( 200 ) );

    return pin;
}


/// SCH_PIN reads the symbol editor's pin defaults on construction.
struct PIN_NUMBER_DISPLAY_FIXTURE
{
    SETTINGS_MANAGER m_settingsManager;
};
} // namespace


BOOST_FIXTURE_TEST_SUITE( PinNumberDisplay, PIN_NUMBER_DISPLAY_FIXTURE )


BOOST_AUTO_TEST_CASE( ModesRenderTheirShortForm )
{
    // Ordering is by pad number, not by the order the author typed, and a range contributes every
    // pad it covers rather than the one item it is written as
    BOOST_CHECK_EQUAL( format( wxS( "[7,3,5]" ), PIN_NUMBER_DISPLAY::FIRST ), wxS( "3" ) );
    BOOST_CHECK_EQUAL( format( wxS( "[5-9]" ), PIN_NUMBER_DISPLAY::FIRST_AND_COUNT ), wxS( "5(5)" ) );

    BOOST_CHECK_EQUAL( format( wxS( "[1,2,3]" ), PIN_NUMBER_DISPLAY::ALL ), wxS( "[1,2,3]" ) );

    // A block too wide for the pin breaks onto one line per pad
    BOOST_CHECK_EQUAL( format( wxS( "[1,2,3]" ), PIN_NUMBER_DISPLAY::ALL, 1 ), wxS( "[1\n2\n3]" ) );
}


BOOST_AUTO_TEST_CASE( SmallestPadIgnoresAuthoringOrder )
{
    // This is the pad auto-generated net names key on, so it has to be the lowest one rather than
    // whichever the author happened to list first
    LIB_SYMBOL sym( wxS( "FPGA" ) );

    BOOST_CHECK_EQUAL( addPin( sym, wxS( "[7,3,5]" ) )->GetSmallestStackedPadNumber(), wxS( "3" ) );
    BOOST_CHECK_EQUAL( addPin( sym, wxS( "[A12,A2]" ) )->GetSmallestStackedPadNumber(), wxS( "A2" ) );
    BOOST_CHECK_EQUAL( addPin( sym, wxS( "9" ) )->GetSmallestStackedPadNumber(), wxS( "9" ) );
}


BOOST_AUTO_TEST_CASE( SchematicPinInheritsFromItsLibraryPin )
{
    LIB_SYMBOL libSymbol( wxS( "FPGA" ) );
    SCH_PIN*   libPin = addPin( libSymbol, wxS( "[1,2,3]" ) );

    SCH_SYMBOL schSymbol;
    SCH_PIN    schPin( &schSymbol, libPin );

    libPin->SetNumberDisplay( PIN_NUMBER_DISPLAY::FIRST_AND_COUNT );
    BOOST_CHECK( schPin.GetNumberDisplay() == PIN_NUMBER_DISPLAY::FIRST_AND_COUNT );

    libPin->SetNumberDisplay( PIN_NUMBER_DISPLAY::FIRST );
    BOOST_CHECK( schPin.GetNumberDisplay() == PIN_NUMBER_DISPLAY::FIRST );
}


BOOST_AUTO_TEST_CASE( StackIndicatorTracksTheNumber )
{
    LIB_SYMBOL sym( wxS( "FPGA" ) );
    SCH_PIN*   plain = addLaidOutPin( sym, wxS( "4" ) );
    SCH_PIN*   stacked = addLaidOutPin( sym, wxS( "[1,2,3]" ) );

    // Nothing is hidden at the default, and a plain pin has no stack to advertise
    BOOST_CHECK( !stacked->GetLayoutCache().GetStackIconBBox().has_value() );

    plain->SetNumberDisplay( PIN_NUMBER_DISPLAY::FIRST );
    BOOST_CHECK( !plain->GetLayoutCache().GetStackIconBBox().has_value() );

    for( PIN_NUMBER_DISPLAY display : { PIN_NUMBER_DISPLAY::FIRST, PIN_NUMBER_DISPLAY::FIRST_AND_COUNT } )
    {
        stacked->SetNumberDisplay( display );

        OPT_BOX2I icon = stacked->GetLayoutCache().GetStackIconBBox();
        OPT_BOX2I numBox = stacked->GetLayoutCache().GetPinNumberBBox();
        BOOST_REQUIRE( icon.has_value() );
        BOOST_REQUIRE( numBox.has_value() );

        // Outboard of the number, never on top of the digits.  Which side the number sits on
        // depends on whether the name shares the space, so follow it rather than assume.
        BOOST_CHECK( !icon->Intersects( *numBox ) );

        if( numBox->GetCenter().y > stacked->GetPosition().y )
            BOOST_CHECK_GT( icon->GetTop(), numBox->GetBottom() );
        else
            BOOST_CHECK_LT( icon->GetBottom(), numBox->GetTop() );
    }

    // Zeroing the number text size is how a symbol hides one pin's number; the indicator has to
    // outlive that or the stack disappears with nothing standing in for its pads
    stacked->SetNumberTextSize( 0 );
    BOOST_CHECK_GT( StackedPinIconSize( *stacked ), 0 );

    OPT_BOX2I hidden = stacked->GetLayoutCache().GetStackIconBBox();
    BOOST_REQUIRE( hidden.has_value() );
    BOOST_CHECK_GT( hidden->GetWidth(), 0 );
    BOOST_CHECK_GT( hidden->GetHeight(), 0 );
}


BOOST_AUTO_TEST_CASE( AbbreviatedNumberDoesNotInflateTheBoundingBox )
{
    LIB_SYMBOL sym( wxS( "FPGA" ) );
    SCH_PIN*   pin = addLaidOutPin( sym, wxS( "[1000000,2000000,3000000]" ) );

    const BOX2I full = pin->GetLayoutCache().GetPinBoundingBox( false, true, false );

    pin->SetNumberDisplay( PIN_NUMBER_DISPLAY::FIRST );

    // The undrawn pads must stop reserving room
    BOOST_CHECK_LT( pin->GetLayoutCache().GetPinBoundingBox( false, true, false ).GetWidth(),
                    full.GetWidth() );
}


BOOST_AUTO_TEST_CASE( DisplayModeSurvivesTheFileFormat )
{
    LIB_SYMBOL sym( wxS( "FPGA" ) );
    addPin( sym, wxS( "[1,2,3]" ) )->SetNumberDisplay( PIN_NUMBER_DISPLAY::FIRST_AND_COUNT );
    addPin( sym, wxS( "[6,7]" ) )->SetNumberDisplay( PIN_NUMBER_DISPLAY::FIRST );
    addPin( sym, wxS( "8" ) );

    STRING_FORMATTER formatter;
    SCH_IO_KICAD_SEXPR::FormatLibSymbol( &sym, formatter );

    // Every symbol in the wild is at the default; emitting the token unconditionally would rewrite
    // whole libraries on the next save
    std::string text = formatter.GetString();
    BOOST_CHECK( text.find( "number_display all" ) == std::string::npos );

    std::vector<LIB_SYMBOL*> loaded = SCH_IO_KICAD_SEXPR::ParseLibSymbols( text, "pin display test" );
    BOOST_REQUIRE_EQUAL( loaded.size(), 1u );

    std::unique_ptr<LIB_SYMBOL>            reloaded( loaded.front() );
    std::map<wxString, PIN_NUMBER_DISPLAY> byNumber;

    for( SCH_PIN* pin : reloaded->GetPins() )
        byNumber[pin->GetNumber()] = pin->GetNumberDisplay();

    BOOST_CHECK( byNumber[wxS( "[1,2,3]" )] == PIN_NUMBER_DISPLAY::FIRST_AND_COUNT );
    BOOST_CHECK( byNumber[wxS( "[6,7]" )] == PIN_NUMBER_DISPLAY::FIRST );
    BOOST_CHECK( byNumber[wxS( "8" )] == PIN_NUMBER_DISPLAY::ALL );
}


BOOST_AUTO_TEST_CASE( ApiRoundTripKeepsTheDisplayMode )
{
    LIB_SYMBOL sym( wxS( "FPGA" ) );

    for( PIN_NUMBER_DISPLAY display : { PIN_NUMBER_DISPLAY::ALL, PIN_NUMBER_DISPLAY::FIRST,
                                        PIN_NUMBER_DISPLAY::FIRST_AND_COUNT } )
    {
        SCH_PIN* pin = addPin( sym, wxS( "[1,2,3]" ) );
        pin->SetNumberDisplay( display );

        google::protobuf::Any container;
        pin->Serialize( container );

        SCH_PIN restored( &sym );
        BOOST_REQUIRE( restored.Deserialize( container ) );
        BOOST_CHECK( restored.GetNumberDisplay() == display );
    }
}


BOOST_AUTO_TEST_SUITE_END()
