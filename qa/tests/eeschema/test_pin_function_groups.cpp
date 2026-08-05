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
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <qa_utils/wx_utils/wx_assert.h>

#include <lib_symbol.h>
#include <pin_type.h>
#include <sch_pin.h>
#include <sch_symbol.h>
#include <sch_io/kicad_sexpr/sch_io_kicad_sexpr.h>
#include <richio.h>


/*
 * Pin function groups are modelled per-pin: a pin's assigned alternate (SCH_PIN::m_alt) is the
 * single source of truth.  A group is "active" when one of its member alternates is the selected
 * alternate on its pin (SCH_SYMBOL::GetActivePinFunctionGroups() derives this).  Members may be
 * required or optional; optional members can be left unassigned, freeing the pin for other uses.
 */
class TEST_PIN_FUNCTION_GROUP_FIXTURE
{
public:
    TEST_PIN_FUNCTION_GROUP_FIXTURE()
    {
        m_lib = new LIB_SYMBOL( "TestChip", nullptr );

        // A small MCU-like symbol: 4 GPIOs each with grouped alternates.  SPI0 spans all four
        // pins, but its MISO line (pin 2) is an OPTIONAL member (free in output-only mode).

        // Pin 1: GPIO0 - SPI_MOSI (SPI0, required), UART_TX (UART0), PWM0 (ungrouped)
        m_libPin1 = new SCH_PIN( m_lib );
        m_libPin1->SetNumber( "1" );
        m_libPin1->SetName( "GPIO0" );
        m_libPin1->SetType( ELECTRICAL_PINTYPE::PT_BIDI );
        m_libPin1->SetPosition( VECTOR2I( 0, 0 ) );

        addAlt( m_libPin1, "SPI_MOSI", ELECTRICAL_PINTYPE::PT_OUTPUT, GRAPHIC_PINSHAPE::LINE, "SPI0" );
        addAlt( m_libPin1, "UART_TX", ELECTRICAL_PINTYPE::PT_OUTPUT, GRAPHIC_PINSHAPE::LINE, "UART0" );
        addAlt( m_libPin1, "PWM0", ELECTRICAL_PINTYPE::PT_OUTPUT, GRAPHIC_PINSHAPE::LINE, "" );

        m_lib->AddDrawItem( m_libPin1 );

        // Pin 2: GPIO1 - SPI_MISO (SPI0, OPTIONAL), UART_RX (UART0)
        m_libPin2 = new SCH_PIN( m_lib );
        m_libPin2->SetNumber( "2" );
        m_libPin2->SetName( "GPIO1" );
        m_libPin2->SetType( ELECTRICAL_PINTYPE::PT_BIDI );
        m_libPin2->SetPosition( VECTOR2I( 0, -2540000 ) );

        addAlt( m_libPin2, "SPI_MISO", ELECTRICAL_PINTYPE::PT_INPUT, GRAPHIC_PINSHAPE::LINE, "SPI0",
                /* optional */ true );
        addAlt( m_libPin2, "UART_RX", ELECTRICAL_PINTYPE::PT_INPUT, GRAPHIC_PINSHAPE::LINE, "UART0" );

        m_lib->AddDrawItem( m_libPin2 );

        // Pin 3: GPIO2 - SPI_CLK (SPI0, required), I2C_SDA (I2C0)
        m_libPin3 = new SCH_PIN( m_lib );
        m_libPin3->SetNumber( "3" );
        m_libPin3->SetName( "GPIO2" );
        m_libPin3->SetType( ELECTRICAL_PINTYPE::PT_BIDI );
        m_libPin3->SetPosition( VECTOR2I( 0, -5080000 ) );

        addAlt( m_libPin3, "SPI_CLK", ELECTRICAL_PINTYPE::PT_OUTPUT, GRAPHIC_PINSHAPE::CLOCK, "SPI0" );
        addAlt( m_libPin3, "I2C_SDA", ELECTRICAL_PINTYPE::PT_BIDI, GRAPHIC_PINSHAPE::LINE, "I2C0" );

        m_lib->AddDrawItem( m_libPin3 );

        // Pin 4: GPIO3 - SPI_CS (SPI0, required), I2C_SCL (I2C0)
        m_libPin4 = new SCH_PIN( m_lib );
        m_libPin4->SetNumber( "4" );
        m_libPin4->SetName( "GPIO3" );
        m_libPin4->SetType( ELECTRICAL_PINTYPE::PT_BIDI );
        m_libPin4->SetPosition( VECTOR2I( 0, -7620000 ) );

        addAlt( m_libPin4, "SPI_CS", ELECTRICAL_PINTYPE::PT_OUTPUT, GRAPHIC_PINSHAPE::LINE, "SPI0" );
        addAlt( m_libPin4, "I2C_SCL", ELECTRICAL_PINTYPE::PT_OUTPUT, GRAPHIC_PINSHAPE::CLOCK, "I2C0" );

        m_lib->AddDrawItem( m_libPin4 );

        // Create schematic symbol
        SCH_SHEET_PATH path;
        m_symbol = new SCH_SYMBOL( *m_lib, m_lib->GetLibId(), &path, 0, 0, VECTOR2I( 0, 0 ) );
        m_symbol->SetRef( &path, "U1" );
        m_symbol->UpdatePins();
    }

    ~TEST_PIN_FUNCTION_GROUP_FIXTURE()
    {
        delete m_symbol;
        delete m_lib;
    }

    static void addAlt( SCH_PIN* aPin, const wxString& aName, ELECTRICAL_PINTYPE aType,
                        GRAPHIC_PINSHAPE aShape, const wxString& aGroup, bool aOptional = false )
    {
        SCH_PIN::ALT alt;
        alt.m_Name = aName;
        alt.m_Type = aType;
        alt.m_Shape = aShape;
        alt.m_FunctionGroup = aGroup;
        alt.m_FunctionGroupOptional = aOptional;
        aPin->GetAlternates()[alt.m_Name] = alt;
    }

    SCH_PIN* GetSchPin( const wxString& aNumber ) const { return m_symbol->GetPin( aNumber ); }

    /// Assign the SPI0 required members (output-capable: SCLK, MOSI, CS) leaving MISO unassigned.
    void assignSpiRequired()
    {
        GetSchPin( "1" )->SetAlt( "SPI_MOSI" );
        GetSchPin( "3" )->SetAlt( "SPI_CLK" );
        GetSchPin( "4" )->SetAlt( "SPI_CS" );
    }

    LIB_SYMBOL* m_lib;
    SCH_PIN*    m_libPin1;
    SCH_PIN*    m_libPin2;
    SCH_PIN*    m_libPin3;
    SCH_PIN*    m_libPin4;

    SCH_SYMBOL* m_symbol;
};


BOOST_FIXTURE_TEST_SUITE( PinFunctionGroups, TEST_PIN_FUNCTION_GROUP_FIXTURE )


/**
 * With no alternates assigned, pins show their default names/types and no group is active.
 */
BOOST_AUTO_TEST_CASE( NoAssignedAltsReturnsDefaults )
{
    SCH_PIN* pin1 = GetSchPin( "1" );
    BOOST_REQUIRE( pin1 != nullptr );

    BOOST_CHECK_EQUAL( pin1->GetShownName(), "GPIO0" );
    BOOST_CHECK( pin1->GetType() == ELECTRICAL_PINTYPE::PT_BIDI );
    BOOST_CHECK( pin1->GetShape() == GRAPHIC_PINSHAPE::LINE );

    BOOST_CHECK( m_symbol->GetActivePinFunctionGroups().empty() );
}


/**
 * Assigning a pin's alternate drives its shown name, type and shape.
 */
BOOST_AUTO_TEST_CASE( PerPinAltDeterminesAppearance )
{
    SCH_PIN* pin3 = GetSchPin( "3" );

    pin3->SetAlt( "SPI_CLK" );
    BOOST_CHECK_EQUAL( pin3->GetShownName(), "SPI_CLK" );
    BOOST_CHECK( pin3->GetType() == ELECTRICAL_PINTYPE::PT_OUTPUT );
    BOOST_CHECK( pin3->GetShape() == GRAPHIC_PINSHAPE::CLOCK );
}


/**
 * Active groups are derived from the alternates the pins are assigned to.
 */
BOOST_AUTO_TEST_CASE( ActiveGroupsDerivedFromAssignedAlts )
{
    assignSpiRequired();

    std::set<wxString> active = m_symbol->GetActivePinFunctionGroups();
    BOOST_CHECK( active.count( "SPI0" ) == 1 );
    BOOST_CHECK( active.count( "UART0" ) == 0 );
    BOOST_CHECK( active.count( "I2C0" ) == 0 );
}


/**
 * Clearing a pin's alternate reverts it to the base function and can deactivate the group.
 */
BOOST_AUTO_TEST_CASE( ClearingAltRevertsToDefault )
{
    assignSpiRequired();
    BOOST_CHECK_EQUAL( GetSchPin( "1" )->GetShownName(), "SPI_MOSI" );

    GetSchPin( "1" )->SetAlt( wxEmptyString );
    GetSchPin( "3" )->SetAlt( wxEmptyString );
    GetSchPin( "4" )->SetAlt( wxEmptyString );

    BOOST_CHECK_EQUAL( GetSchPin( "1" )->GetShownName(), "GPIO0" );
    BOOST_CHECK( GetSchPin( "1" )->GetType() == ELECTRICAL_PINTYPE::PT_BIDI );
    BOOST_CHECK( m_symbol->GetActivePinFunctionGroups().empty() );
}


/**
 * A partially-used peripheral leaves its optional members free for other functions.
 *
 * Output-only SPI0 needs SCLK, MOSI and CS but not MISO (pin 2).  Pin 2 stays at its base
 * function and is available, e.g. as UART_RX, while SPI0 is still active.
 */
BOOST_AUTO_TEST_CASE( PartialGroupLeavesOptionalPinFree )
{
    assignSpiRequired();

    // SPI0 is active from its required members...
    BOOST_CHECK( m_symbol->GetActivePinFunctionGroups().count( "SPI0" ) == 1 );

    // ...but pin 2 (optional SPI_MISO) was never assigned, so it is free.
    SCH_PIN* pin2 = GetSchPin( "2" );
    BOOST_CHECK_EQUAL( pin2->GetShownName(), "GPIO1" );

    // The freed pin can serve another function entirely.
    pin2->SetAlt( "UART_RX" );
    BOOST_CHECK_EQUAL( pin2->GetShownName(), "UART_RX" );
    BOOST_CHECK( pin2->GetType() == ELECTRICAL_PINTYPE::PT_INPUT );

    // Both SPI0 (required members) and UART0 (the freed pin) are now active together.
    std::set<wxString> active = m_symbol->GetActivePinFunctionGroups();
    BOOST_CHECK( active.count( "SPI0" ) == 1 );
    BOOST_CHECK( active.count( "UART0" ) == 1 );
}


/**
 * Assigning the optional member too brings it into the group.
 */
BOOST_AUTO_TEST_CASE( OptionalMemberCanBeAssigned )
{
    assignSpiRequired();
    GetSchPin( "2" )->SetAlt( "SPI_MISO" );

    BOOST_CHECK_EQUAL( GetSchPin( "2" )->GetShownName(), "SPI_MISO" );
    BOOST_CHECK( GetSchPin( "2" )->GetType() == ELECTRICAL_PINTYPE::PT_INPUT );

    // Still a single active group (SPI0); the optional member belongs to it.
    std::set<wxString> active = m_symbol->GetActivePinFunctionGroups();
    BOOST_CHECK( active.count( "SPI0" ) == 1 );
    BOOST_CHECK_EQUAL( active.size(), 1u );
}


/**
 * The optional flag is carried on the library pin's alternate.
 */
BOOST_AUTO_TEST_CASE( OptionalFlagStoredOnAlternate )
{
    const std::map<wxString, SCH_PIN::ALT>& alts = m_libPin2->GetAlternates();

    auto miso = alts.find( "SPI_MISO" );
    BOOST_REQUIRE( miso != alts.end() );
    BOOST_CHECK( miso->second.m_FunctionGroupOptional == true );

    const std::map<wxString, SCH_PIN::ALT>& alts1 = m_libPin1->GetAlternates();
    auto mosi = alts1.find( "SPI_MOSI" );
    BOOST_REQUIRE( mosi != alts1.end() );
    BOOST_CHECK( mosi->second.m_FunctionGroupOptional == false );
}


/**
 * Per-pin assignments (and therefore derived active groups) survive a symbol copy.
 */
BOOST_AUTO_TEST_CASE( ActiveGroupsDerivedAfterCopy )
{
    assignSpiRequired();
    GetSchPin( "2" )->SetAlt( "UART_RX" );

    SCH_SYMBOL copied( *m_symbol );

    std::set<wxString> active = copied.GetActivePinFunctionGroups();
    BOOST_CHECK( active.count( "SPI0" ) == 1 );
    BOOST_CHECK( active.count( "UART0" ) == 1 );
    BOOST_CHECK( active.count( "I2C0" ) == 0 );

    BOOST_CHECK_EQUAL( copied.GetPin( "1" )->GetShownName(), "SPI_MOSI" );
}


/**
 * The optional flag participates in alternate comparison (so library diffs detect a change).
 */
BOOST_AUTO_TEST_CASE( OptionalFlagIncludedInComparison )
{
    SCH_PIN::ALT a;
    a.m_Name = wxS( "TEST" );
    a.m_Shape = GRAPHIC_PINSHAPE::LINE;
    a.m_Type = ELECTRICAL_PINTYPE::PT_OUTPUT;
    a.m_FunctionGroup = wxS( "GRP_A" );
    a.m_FunctionGroupOptional = false;

    SCH_PIN::ALT b = a;
    b.m_FunctionGroupOptional = true;

    BOOST_CHECK( a.m_FunctionGroupOptional != b.m_FunctionGroupOptional );
}


/**
 * Group descriptions are compared by LIB_SYMBOL::Compare().
 */
BOOST_AUTO_TEST_CASE( GroupDescriptionsIncludedInLibCompare )
{
    LIB_SYMBOL other( *m_lib );

    // Identical symbols compare equal.
    BOOST_CHECK_EQUAL( m_lib->Compare( other, SCH_ITEM::COMPARE_FLAGS::EQUALITY ), 0 );

    // Adding a group description on one side makes them differ.
    other.PinFunctionGroupDescriptions()[wxS( "SPI0" )] =
            PIN_FUNCTION_GROUP_DESC{ wxS( "Serial Peripheral Interface 0" ), wxEmptyString };

    BOOST_CHECK( m_lib->Compare( other, SCH_ITEM::COMPARE_FLAGS::EQUALITY ) != 0 );
}


/**
 * The optional group-member marker survives a .kicad_sym write/read round-trip.
 */
BOOST_AUTO_TEST_CASE( OptionalFlagSurvivesSerializationRoundTrip )
{
    STRING_FORMATTER formatter;
    SCH_IO_KICAD_SEXPR::FormatLibSymbol( m_lib, formatter );

    std::string text = formatter.GetString();

    // The optional member is written with the trailing marker; required members are not.
    BOOST_CHECK( text.find( "(group \"SPI0\" optional)" ) != std::string::npos );

    std::vector<LIB_SYMBOL*> parsed = SCH_IO_KICAD_SEXPR::ParseLibSymbols( text, "round-trip" );
    BOOST_REQUIRE_EQUAL( parsed.size(), 1u );

    LIB_SYMBOL* rt = parsed[0];

    const SCH_PIN* pin2 = rt->GetPin( wxS( "2" ), 0, 0 );
    BOOST_REQUIRE( pin2 != nullptr );
    auto miso = pin2->GetAlternates().find( wxS( "SPI_MISO" ) );
    BOOST_REQUIRE( miso != pin2->GetAlternates().end() );
    BOOST_CHECK_EQUAL( miso->second.m_FunctionGroup, wxS( "SPI0" ) );
    BOOST_CHECK( miso->second.m_FunctionGroupOptional == true );

    const SCH_PIN* pin1 = rt->GetPin( wxS( "1" ), 0, 0 );
    BOOST_REQUIRE( pin1 != nullptr );
    auto mosi = pin1->GetAlternates().find( wxS( "SPI_MOSI" ) );
    BOOST_REQUIRE( mosi != pin1->GetAlternates().end() );
    BOOST_CHECK( mosi->second.m_FunctionGroupOptional == false );

    for( LIB_SYMBOL* sym : parsed )
        delete sym;
}


BOOST_AUTO_TEST_SUITE_END()


/*
 * Subgroups extend the required/optional split with a cardinality rule: a set of member
 * alternates tagged with the same subgroup name, of which at least `min` must be assigned for the
 * group to be validly configured.  This is what expresses "an SPI bus needs at least one chip
 * select, but not all four of them".
 *
 * A separate fixture from TEST_PIN_FUNCTION_GROUP_FIXTURE on purpose: this one pre-populates an
 * SPI0 group description, which would silently invert GroupDescriptionsIncludedInLibCompare above
 * (that case asserts adding a description makes two symbols differ).
 */
class TEST_PIN_FUNCTION_SUBGROUP_FIXTURE
{
public:
    TEST_PIN_FUNCTION_SUBGROUP_FIXTURE()
    {
        m_lib = new LIB_SYMBOL( "MiniCM5", nullptr );

        // Modelled on the CM5's SPI0: two required lines, four interchangeable chip-selects in a
        // "CS" subgroup, and one optional line (SIO1 is free when the bus is output-only).
        addPin( "40", "GPIO_11", "SPI0_SCLK", ELECTRICAL_PINTYPE::PT_OUTPUT,
                GRAPHIC_PINSHAPE::CLOCK, "SPI0" );
        addPin( "41", "GPIO_10", "SPI0_SIO0", ELECTRICAL_PINTYPE::PT_BIDI,
                GRAPHIC_PINSHAPE::LINE, "SPI0" );

        addPin( "39", "GPIO_08", "SPI0_CSn0", ELECTRICAL_PINTYPE::PT_OUTPUT,
                GRAPHIC_PINSHAPE::LINE, "SPI0", false, "CS" );
        addPin( "37", "GPIO_07", "SPI0_CSn1", ELECTRICAL_PINTYPE::PT_OUTPUT,
                GRAPHIC_PINSHAPE::LINE, "SPI0", false, "CS" );
        addPin( "56", "GPIO_03", "SPI0_CSn2", ELECTRICAL_PINTYPE::PT_OUTPUT,
                GRAPHIC_PINSHAPE::LINE, "SPI0", false, "CS" );
        addPin( "58", "GPIO_02", "SPI0_CSn3", ELECTRICAL_PINTYPE::PT_OUTPUT,
                GRAPHIC_PINSHAPE::LINE, "SPI0", false, "CS" );

        addPin( "42", "GPIO_09", "SPI0_SIO1", ELECTRICAL_PINTYPE::PT_BIDI,
                GRAPHIC_PINSHAPE::LINE, "SPI0", true );

        // Description carries no explicit rule, so the CS subgroup relies on the min-1 default.
        m_lib->PinFunctionGroupDescriptions()[wxS( "SPI0" )].description =
                wxS( "Serial Peripheral Interface 0" );

        SCH_SHEET_PATH path;
        m_symbol = new SCH_SYMBOL( *m_lib, m_lib->GetLibId(), &path, 0, 0, VECTOR2I( 0, 0 ) );
        m_symbol->SetRef( &path, "U1" );
        m_symbol->UpdatePins();
    }

    ~TEST_PIN_FUNCTION_SUBGROUP_FIXTURE()
    {
        delete m_symbol;
        delete m_lib;
    }

    /// Add a pin carrying a single grouped alternate.
    void addPin( const wxString& aNumber, const wxString& aName, const wxString& aAltName,
                 ELECTRICAL_PINTYPE aType, GRAPHIC_PINSHAPE aShape, const wxString& aGroup,
                 bool aOptional = false, const wxString& aSubgroup = wxEmptyString )
    {
        SCH_PIN* pin = new SCH_PIN( m_lib );
        pin->SetNumber( aNumber );
        pin->SetName( aName );
        pin->SetType( ELECTRICAL_PINTYPE::PT_BIDI );
        pin->SetPosition( VECTOR2I( 0, -2540000 * static_cast<int>( m_pinCount++ ) ) );

        SCH_PIN::ALT alt;
        alt.m_Name = aAltName;
        alt.m_Type = aType;
        alt.m_Shape = aShape;
        alt.m_FunctionGroup = aGroup;
        alt.m_FunctionGroupOptional = aOptional;
        alt.m_FunctionGroupSubgroup = aSubgroup;
        pin->GetAlternates()[alt.m_Name] = alt;

        m_lib->AddDrawItem( pin );
    }

    SCH_PIN* GetSchPin( const wxString& aNumber ) const { return m_symbol->GetPin( aNumber ); }

    /// Assign the two required SPI0 members, leaving every chip-select unassigned.
    void assignSpiRequired()
    {
        GetSchPin( "40" )->SetAlt( "SPI0_SCLK" );
        GetSchPin( "41" )->SetAlt( "SPI0_SIO0" );
    }

    size_t      m_pinCount = 0;
    LIB_SYMBOL* m_lib;
    SCH_SYMBOL* m_symbol;
};


BOOST_FIXTURE_TEST_SUITE( PinFunctionSubgroups, TEST_PIN_FUNCTION_SUBGROUP_FIXTURE )


/**
 * The subgroup name is stored on the alternate, alongside (not instead of) the group tag.
 */
BOOST_AUTO_TEST_CASE( SubgroupTagStoredOnAlternate )
{
    const SCH_PIN* csPin = m_lib->GetPin( wxS( "39" ), 0, 0 );
    BOOST_REQUIRE( csPin != nullptr );

    auto cs = csPin->GetAlternates().find( wxS( "SPI0_CSn0" ) );
    BOOST_REQUIRE( cs != csPin->GetAlternates().end() );
    BOOST_CHECK_EQUAL( cs->second.m_FunctionGroup, wxS( "SPI0" ) );
    BOOST_CHECK_EQUAL( cs->second.m_FunctionGroupSubgroup, wxS( "CS" ) );

    const SCH_PIN* sclkPin = m_lib->GetPin( wxS( "40" ), 0, 0 );
    BOOST_REQUIRE( sclkPin != nullptr );

    auto sclk = sclkPin->GetAlternates().find( wxS( "SPI0_SCLK" ) );
    BOOST_REQUIRE( sclk != sclkPin->GetAlternates().end() );
    BOOST_CHECK( sclk->second.m_FunctionGroupSubgroup.IsEmpty() );
}


/**
 * A subgrouped member is never individually required, even without the `optional` marker: its
 * obligation is discharged by the subgroup's cardinality rule.  Without this, tagging four chip
 * selects as a min-1 subgroup would ALSO demand all four -- the contradiction the feature exists
 * to remove.
 */
BOOST_AUTO_TEST_CASE( SubgroupMemberIsNotIndividuallyRequired )
{
    const SCH_PIN* csPin = m_lib->GetPin( wxS( "39" ), 0, 0 );
    const SCH_PIN::ALT& cs = csPin->GetAlternates().at( wxS( "SPI0_CSn0" ) );

    BOOST_CHECK( cs.m_FunctionGroupOptional == false );   // not flagged optional on disk...
    BOOST_CHECK( cs.IsOptionalGroupMember() == true );    // ...but still not individually required

    const SCH_PIN* sclkPin = m_lib->GetPin( wxS( "40" ), 0, 0 );
    BOOST_CHECK( sclkPin->GetAlternates().at( wxS( "SPI0_SCLK" ) ).IsOptionalGroupMember()
                 == false );

    const SCH_PIN* sio1Pin = m_lib->GetPin( wxS( "42" ), 0, 0 );
    BOOST_CHECK( sio1Pin->GetAlternates().at( wxS( "SPI0_SIO1" ) ).IsOptionalGroupMember()
                 == true );
}


/**
 * An explicit cardinality rule is stored on the group description, not on the members.
 */
BOOST_AUTO_TEST_CASE( SubgroupRuleStoredOnGroupDescription )
{
    PIN_FUNCTION_GROUP_DESC& desc = m_lib->PinFunctionGroupDescriptions()[wxS( "SPI0" )];

    BOOST_CHECK( desc.subgroups.empty() );                // default min-1 applies implicitly

    desc.subgroups[wxS( "CS" )] = PIN_FUNCTION_SUBGROUP_RULE{ 2 };
    BOOST_CHECK_EQUAL( m_lib->PinFunctionGroupDescriptions()
                               .at( wxS( "SPI0" ) ).subgroups.at( wxS( "CS" ) ).min,
                       2 );

    // An undeclared rule defaults to 1 rather than 0, so `(subgroup "CS")` alone is meaningful.
    BOOST_CHECK_EQUAL( PIN_FUNCTION_SUBGROUP_RULE{}.min, 1 );
}


/**
 * Subgroup tags and explicit rules survive a .kicad_sym write/read round-trip.
 */
BOOST_AUTO_TEST_CASE( SubgroupSurvivesSerializationRoundTrip )
{
    m_lib->PinFunctionGroupDescriptions()[wxS( "SPI0" )].subgroups[wxS( "CS" )] =
            PIN_FUNCTION_SUBGROUP_RULE{ 2 };

    STRING_FORMATTER formatter;
    SCH_IO_KICAD_SEXPR::FormatLibSymbol( m_lib, formatter );

    std::string text = formatter.GetString();

    BOOST_CHECK( text.find( "(group \"SPI0\" (subgroup \"CS\"))" ) != std::string::npos );
    BOOST_CHECK( text.find( "(subgroup \"CS\" (min 2))" ) != std::string::npos );
    BOOST_CHECK( text.find( "(group \"SPI0\" optional)" ) != std::string::npos );

    std::vector<LIB_SYMBOL*> parsed = SCH_IO_KICAD_SEXPR::ParseLibSymbols( text, "round-trip" );
    BOOST_REQUIRE_EQUAL( parsed.size(), 1u );

    LIB_SYMBOL* rt = parsed[0];

    const SCH_PIN* csPin = rt->GetPin( wxS( "39" ), 0, 0 );
    BOOST_REQUIRE( csPin != nullptr );
    BOOST_CHECK_EQUAL( csPin->GetAlternates().at( wxS( "SPI0_CSn0" ) ).m_FunctionGroupSubgroup,
                       wxS( "CS" ) );

    const SCH_PIN* sclkPin = rt->GetPin( wxS( "40" ), 0, 0 );
    BOOST_REQUIRE( sclkPin != nullptr );
    BOOST_CHECK( sclkPin->GetAlternates().at( wxS( "SPI0_SCLK" ) )
                         .m_FunctionGroupSubgroup.IsEmpty() );

    BOOST_CHECK_EQUAL(
            rt->PinFunctionGroupDescriptions().at( wxS( "SPI0" ) ).subgroups.at( wxS( "CS" ) ).min,
            2 );

    for( LIB_SYMBOL* sym : parsed )
        delete sym;
}


/**
 * Files written before subgroups existed still parse: a bare (group "X"), the `optional` marker
 * and a (bus "...") sub-element must all keep working, with empty subgroup state.
 */
BOOST_AUTO_TEST_CASE( LegacyGroupSyntaxStillParses )
{
    std::string legacy =
            "(symbol \"LegacyChip\"\n"
            "  (pin_function_group_descriptions\n"
            "    (group \"SPI0\" \"Serial Peripheral Interface 0\" (bus \"spi\"))\n"
            "  )\n"
            "  (symbol \"LegacyChip_1_1\"\n"
            "    (pin bidirectional line\n"
            "      (at 0 0 0)\n"
            "      (length 2.54)\n"
            "      (name \"GPIO0\" (effects (font (size 1.27 1.27))))\n"
            "      (number \"1\" (effects (font (size 1.27 1.27))))\n"
            "      (alternate \"SPI0_SCLK\" output clock (group \"SPI0\"))\n"
            "      (alternate \"SPI0_MISO\" input line (group \"SPI0\" optional))\n"
            "    )\n"
            "  )\n"
            ")\n";

    std::vector<LIB_SYMBOL*> parsed = SCH_IO_KICAD_SEXPR::ParseLibSymbols( legacy, "legacy" );
    BOOST_REQUIRE_EQUAL( parsed.size(), 1u );

    LIB_SYMBOL* sym = parsed[0];

    const SCH_PIN* pin = sym->GetPin( wxS( "1" ), 0, 0 );
    BOOST_REQUIRE( pin != nullptr );

    const SCH_PIN::ALT& sclk = pin->GetAlternates().at( wxS( "SPI0_SCLK" ) );
    BOOST_CHECK_EQUAL( sclk.m_FunctionGroup, wxS( "SPI0" ) );
    BOOST_CHECK( sclk.m_FunctionGroupOptional == false );
    BOOST_CHECK( sclk.m_FunctionGroupSubgroup.IsEmpty() );
    BOOST_CHECK( sclk.IsOptionalGroupMember() == false );

    const SCH_PIN::ALT& miso = pin->GetAlternates().at( wxS( "SPI0_MISO" ) );
    BOOST_CHECK( miso.m_FunctionGroupOptional == true );
    BOOST_CHECK( miso.m_FunctionGroupSubgroup.IsEmpty() );

    const PIN_FUNCTION_GROUP_DESC& desc =
            sym->PinFunctionGroupDescriptions().at( wxS( "SPI0" ) );
    BOOST_CHECK_EQUAL( desc.bus, wxS( "spi" ) );
    BOOST_CHECK( desc.subgroups.empty() );

    for( LIB_SYMBOL* s : parsed )
        delete s;
}


/**
 * Subgroup rules participate in LIB_SYMBOL::Compare(), so a library-vs-schematic diff notices a
 * changed minimum.  The failure mode without this is silent, surfacing much later as a missed
 * ERCE_LIB_SYMBOL_MISMATCH.
 */
BOOST_AUTO_TEST_CASE( SubgroupRulesIncludedInLibCompare )
{
    LIB_SYMBOL other( *m_lib );

    BOOST_CHECK_EQUAL( m_lib->Compare( other, SCH_ITEM::COMPARE_FLAGS::EQUALITY ), 0 );

    other.PinFunctionGroupDescriptions()[wxS( "SPI0" )].subgroups[wxS( "CS" )] =
            PIN_FUNCTION_SUBGROUP_RULE{ 2 };

    BOOST_CHECK( m_lib->Compare( other, SCH_ITEM::COMPARE_FLAGS::EQUALITY ) != 0 );
}


/**
 * An unused peripheral is never an error, however many members it has.
 */
BOOST_AUTO_TEST_CASE( InactiveGroupIsNotIncomplete )
{
    auto status = m_symbol->ValidatePinFunctionGroups();

    BOOST_REQUIRE( status.count( wxS( "SPI0" ) ) == 1 );
    BOOST_CHECK( status.at( wxS( "SPI0" ) ).state == PIN_FUNCTION_GROUP_STATE::INACTIVE );
    BOOST_CHECK( status.at( wxS( "SPI0" ) ).IsValid() );
}


/**
 * THE HEADLINE CASE.  SPI0 with its clock and data assigned but no chip select is incomplete:
 * the CS subgroup needs at least one member.  This is what the old binary required/optional
 * model could not express.
 */
BOOST_AUTO_TEST_CASE( GroupIncompleteWhenNoSubgroupMemberAssigned )
{
    assignSpiRequired();

    auto status = m_symbol->ValidatePinFunctionGroups();
    const PIN_FUNCTION_GROUP_STATUS& spi0 = status.at( wxS( "SPI0" ) );

    BOOST_CHECK( spi0.state == PIN_FUNCTION_GROUP_STATE::INCOMPLETE );
    BOOST_CHECK( spi0.missingRequiredPins.empty() );   // SCLK and SIO0 are both assigned
    BOOST_REQUIRE( spi0.unmetSubgroups.count( wxS( "CS" ) ) == 1 );
    BOOST_CHECK_EQUAL( spi0.unmetSubgroups.at( wxS( "CS" ) ).first, 0 );   // assigned
    BOOST_CHECK_EQUAL( spi0.unmetSubgroups.at( wxS( "CS" ) ).second, 1 );  // required
    BOOST_CHECK( !spi0.message.IsEmpty() );
}


/**
 * Any ONE chip select satisfies the minimum -- not merely the first one.  This is the difference
 * between "pick one of these" and "use this specific pin".
 */
BOOST_AUTO_TEST_CASE( AnySingleSubgroupMemberSatisfiesMinimum )
{
    const std::vector<std::pair<wxString, wxString>> chipSelects = {
        { wxS( "39" ), wxS( "SPI0_CSn0" ) },
        { wxS( "37" ), wxS( "SPI0_CSn1" ) },
        { wxS( "56" ), wxS( "SPI0_CSn2" ) },
        { wxS( "58" ), wxS( "SPI0_CSn3" ) },
    };

    for( const auto& [pinNumber, altName] : chipSelects )
    {
        assignSpiRequired();

        for( const auto& [otherPin, otherAlt] : chipSelects )
            GetSchPin( otherPin )->SetAlt( wxEmptyString );

        GetSchPin( pinNumber )->SetAlt( altName );

        auto status = m_symbol->ValidatePinFunctionGroups();

        BOOST_CHECK_MESSAGE( status.at( wxS( "SPI0" ) ).state
                                     == PIN_FUNCTION_GROUP_STATE::COMPLETE,
                             "SPI0 should be complete with only " + altName.ToStdString() );
    }
}


/**
 * The minimum is a floor, not an equality: using several chip selects is fine.
 */
BOOST_AUTO_TEST_CASE( MultipleSubgroupMembersStillComplete )
{
    assignSpiRequired();
    GetSchPin( "39" )->SetAlt( "SPI0_CSn0" );
    GetSchPin( "56" )->SetAlt( "SPI0_CSn2" );

    auto status = m_symbol->ValidatePinFunctionGroups();
    BOOST_CHECK( status.at( wxS( "SPI0" ) ).state == PIN_FUNCTION_GROUP_STATE::COMPLETE );
}


/**
 * An undeclared subgroup defaults to min 1, so `(subgroup "CS")` alone is meaningful and the
 * CM5 case needs no pin_function_group_descriptions block at all.
 */
BOOST_AUTO_TEST_CASE( SubgroupMinimumDefaultsToOne )
{
    BOOST_REQUIRE( m_lib->PinFunctionGroupDescriptions().at( wxS( "SPI0" ) ).subgroups.empty() );

    assignSpiRequired();

    auto status = m_symbol->ValidatePinFunctionGroups();
    BOOST_CHECK( status.at( wxS( "SPI0" ) ).state == PIN_FUNCTION_GROUP_STATE::INCOMPLETE );
    BOOST_CHECK_EQUAL( status.at( wxS( "SPI0" ) ).unmetSubgroups.at( wxS( "CS" ) ).second, 1 );
}


/**
 * An explicit minimum overrides the default, so "this part needs two chip selects" is sayable.
 */
BOOST_AUTO_TEST_CASE( ExplicitSubgroupMinimumIsHonoured )
{
    m_lib->PinFunctionGroupDescriptions()[wxS( "SPI0" )].subgroups[wxS( "CS" )] =
            PIN_FUNCTION_SUBGROUP_RULE{ 2 };
    m_symbol->SetLibSymbol( new LIB_SYMBOL( *m_lib ) );
    m_symbol->UpdatePins();

    assignSpiRequired();
    GetSchPin( "39" )->SetAlt( "SPI0_CSn0" );

    auto status = m_symbol->ValidatePinFunctionGroups();
    BOOST_REQUIRE( status.at( wxS( "SPI0" ) ).unmetSubgroups.count( wxS( "CS" ) ) == 1 );
    BOOST_CHECK_EQUAL( status.at( wxS( "SPI0" ) ).unmetSubgroups.at( wxS( "CS" ) ).second, 2 );

    GetSchPin( "56" )->SetAlt( "SPI0_CSn2" );

    status = m_symbol->ValidatePinFunctionGroups();
    BOOST_CHECK( status.at( wxS( "SPI0" ) ).state == PIN_FUNCTION_GROUP_STATE::COMPLETE );
}


/**
 * A missing required member and an unmet subgroup are independent failure modes and are
 * reported separately, so the UI can say which one actually needs attention.
 */
BOOST_AUTO_TEST_CASE( MissingRequiredMemberReportedSeparatelyFromSubgroup )
{
    // A chip select but no clock.
    GetSchPin( "41" )->SetAlt( "SPI0_SIO0" );
    GetSchPin( "39" )->SetAlt( "SPI0_CSn0" );

    auto status = m_symbol->ValidatePinFunctionGroups();
    const PIN_FUNCTION_GROUP_STATUS& spi0 = status.at( wxS( "SPI0" ) );

    BOOST_CHECK( spi0.state == PIN_FUNCTION_GROUP_STATE::INCOMPLETE );
    BOOST_CHECK( spi0.unmetSubgroups.empty() );                       // the CS minimum IS met
    BOOST_CHECK( spi0.missingRequiredPins.count( wxS( "40" ) ) == 1 );  // but SCLK is not assigned
}


/**
 * A rule naming a subgroup that has no members is ignored, not violated: a typo in the library
 * must not permanently invalidate every instance of the symbol.
 */
BOOST_AUTO_TEST_CASE( RuleForUnknownSubgroupIsIgnored )
{
    m_lib->PinFunctionGroupDescriptions()[wxS( "SPI0" )].subgroups[wxS( "NOSUCH" )] =
            PIN_FUNCTION_SUBGROUP_RULE{ 1 };
    m_symbol->SetLibSymbol( new LIB_SYMBOL( *m_lib ) );
    m_symbol->UpdatePins();

    assignSpiRequired();
    GetSchPin( "39" )->SetAlt( "SPI0_CSn0" );

    auto status = m_symbol->ValidatePinFunctionGroups();
    BOOST_CHECK( status.at( wxS( "SPI0" ) ).state == PIN_FUNCTION_GROUP_STATE::COMPLETE );
}


/**
 * The optional member (SIO1) stays free while SPI0 is otherwise complete -- the original
 * partial-group guarantee, still holding alongside subgroups.
 */
BOOST_AUTO_TEST_CASE( OptionalMemberDoesNotBlockCompletion )
{
    assignSpiRequired();
    GetSchPin( "39" )->SetAlt( "SPI0_CSn0" );

    auto status = m_symbol->ValidatePinFunctionGroups();
    BOOST_CHECK( status.at( wxS( "SPI0" ) ).state == PIN_FUNCTION_GROUP_STATE::COMPLETE );
    BOOST_CHECK( GetSchPin( "42" )->GetAlt().IsEmpty() );
}


/**
 * Subgroup cardinality rules survive symbol inheritance even when the derived symbol overrides
 * only the description.  A whole-value overwrite would silently drop the parent's rules.
 */
BOOST_AUTO_TEST_CASE( SubgroupRulesInheritedByFlatten )
{
    m_lib->PinFunctionGroupDescriptions()[wxS( "SPI0" )].subgroups[wxS( "CS" )] =
            PIN_FUNCTION_SUBGROUP_RULE{ 2 };

    LIB_SYMBOL derived( "MiniCM5_Variant", nullptr );
    derived.SetParent( m_lib );
    derived.PinFunctionGroupDescriptions()[wxS( "SPI0" )].description = wxS( "Overridden" );

    std::unique_ptr<LIB_SYMBOL> flat = derived.Flatten();
    BOOST_REQUIRE( flat != nullptr );

    const PIN_FUNCTION_GROUP_DESC& desc =
            flat->PinFunctionGroupDescriptions().at( wxS( "SPI0" ) );

    BOOST_CHECK_EQUAL( desc.description, wxS( "Overridden" ) );        // child wins...
    BOOST_REQUIRE( desc.subgroups.count( wxS( "CS" ) ) == 1 );         // ...parent rule survives
    BOOST_CHECK_EQUAL( desc.subgroups.at( wxS( "CS" ) ).min, 2 );
}


/**
 * A collision that happens only through an ASSIGNED optional/subgroup member is detected.  The
 * previous required-members-only check missed exactly this case.
 */
BOOST_AUTO_TEST_CASE( ConflictDetectedThroughAssignedSubgroupMember )
{
    // Give pin 58 a SECOND alternate belonging to another group, so the two groups contend for
    // that one pin.  (This mirrors the real CM5, where pin 58 carries both SPI0_CSn3 and
    // UART1_CTS.)
    SCH_PIN* libPin58 = nullptr;

    for( SCH_PIN* pin : m_lib->GetGraphicalPins( 0, 0 ) )
    {
        if( pin->GetNumber() == wxS( "58" ) )
            libPin58 = pin;
    }

    BOOST_REQUIRE( libPin58 != nullptr );

    SCH_PIN::ALT cts;
    cts.m_Name = wxS( "UART1_CTS" );
    cts.m_Type = ELECTRICAL_PINTYPE::PT_INPUT;
    cts.m_Shape = GRAPHIC_PINSHAPE::LINE;
    cts.m_FunctionGroup = wxS( "UART1" );   // required member of UART1
    libPin58->GetAlternates()[cts.m_Name] = cts;

    // Rebuild the instance so it sees the added alternate.
    m_symbol->SetLibSymbol( new LIB_SYMBOL( *m_lib ) );
    m_symbol->UpdatePins();

    assignSpiRequired();
    GetSchPin( "58" )->SetAlt( "SPI0_CSn3" );   // an optional-by-subgroup member, now assigned

    std::map<wxString, wxString> assignments = m_symbol->GetPinAlternateAssignments();
    std::set<wxString>           active = m_symbol->GetActivePinFunctionGroups();

    auto conflicts = m_lib->GetConflictingPinFunctionGroups( active, assignments );

    // UART1's required member sits on pin 58, which SPI0 is holding via its subgroup member.
    BOOST_REQUIRE( conflicts.count( wxS( "UART1" ) ) == 1 );
    BOOST_CHECK( conflicts.at( wxS( "UART1" ) ).count( wxS( "SPI0" ) ) == 1 );

    // ...and the contended pin is reported, so the UI can name it.
    BOOST_CHECK( conflicts.at( wxS( "UART1" ) ).at( wxS( "SPI0" ) ).count( wxS( "58" ) ) == 1 );
}


/**
 * A group holding a pin through an assigned *plain optional* member (not a subgroup one) also
 * blocks another group that requires it.
 *
 * This is the reported case: SPI0's optional SIO1 sits on a pin that I2C0 requires, and I2C0
 * kept showing as available -- then stole the pin when enabled.
 */
BOOST_AUTO_TEST_CASE( ConflictDetectedThroughAssignedOptionalMember )
{
    // Give pin 42 (SPI0_SIO1, the optional member) a second alternate required by I2C0.
    SCH_PIN* libPin42 = nullptr;

    for( SCH_PIN* pin : m_lib->GetGraphicalPins( 0, 0 ) )
    {
        if( pin->GetNumber() == wxS( "42" ) )
            libPin42 = pin;
    }

    BOOST_REQUIRE( libPin42 != nullptr );

    SCH_PIN::ALT scl;
    scl.m_Name = wxS( "I2C0_SCL" );
    scl.m_Type = ELECTRICAL_PINTYPE::PT_BIDI;
    scl.m_Shape = GRAPHIC_PINSHAPE::LINE;
    scl.m_FunctionGroup = wxS( "I2C0" );   // required member of I2C0
    libPin42->GetAlternates()[scl.m_Name] = scl;

    m_symbol->SetLibSymbol( new LIB_SYMBOL( *m_lib ) );
    m_symbol->UpdatePins();

    assignSpiRequired();
    GetSchPin( "39" )->SetAlt( "SPI0_CSn0" );

    // Before the optional member is assigned, I2C0 is genuinely free to enable.
    auto before = m_lib->GetConflictingPinFunctionGroups(
            m_symbol->GetActivePinFunctionGroups(), m_symbol->GetPinAlternateAssignments() );
    BOOST_CHECK( before.count( wxS( "I2C0" ) ) == 0 );

    // Assigning SPI0's optional SIO1 takes pin 42, which I2C0 requires.
    GetSchPin( "42" )->SetAlt( "SPI0_SIO1" );

    auto after = m_lib->GetConflictingPinFunctionGroups(
            m_symbol->GetActivePinFunctionGroups(), m_symbol->GetPinAlternateAssignments() );

    BOOST_REQUIRE( after.count( wxS( "I2C0" ) ) == 1 );
    BOOST_CHECK( after.at( wxS( "I2C0" ) ).count( wxS( "SPI0" ) ) == 1 );
    BOOST_CHECK( after.at( wxS( "I2C0" ) ).at( wxS( "SPI0" ) ).count( wxS( "42" ) ) == 1 );
}


/**
 * Alternate-level group data participates in LIB_SYMBOL::Compare(), so a library-vs-schematic
 * diff notices a changed subgroup tag.
 *
 * Compare() diffs pins through the base SCH_ITEM::compare(), which cannot see
 * SCH_PIN::m_alternates, so this is only true because Compare() also compares the alternates
 * explicitly.  Without that, every alternate-level change -- including a renamed alternate --
 * is invisible to a library diff.
 */
BOOST_AUTO_TEST_CASE( SubgroupTagIncludedInLibCompare )
{
    LIB_SYMBOL other( *m_lib );

    BOOST_CHECK_EQUAL( m_lib->Compare( other, SCH_ITEM::COMPARE_FLAGS::EQUALITY ), 0 );

    SCH_PIN* csPin = nullptr;

    for( SCH_PIN* pin : other.GetGraphicalPins( 0, 0 ) )
    {
        if( pin->GetNumber() == wxS( "39" ) )
            csPin = pin;
    }

    BOOST_REQUIRE( csPin != nullptr );
    csPin->GetAlternates().at( wxS( "SPI0_CSn0" ) ).m_FunctionGroupSubgroup = wxS( "CS_ALT" );

    BOOST_CHECK( m_lib->Compare( other, SCH_ITEM::COMPARE_FLAGS::EQUALITY ) != 0 );
}


/**
 * The optional flag likewise participates in LIB_SYMBOL::Compare().
 */
BOOST_AUTO_TEST_CASE( OptionalFlagIncludedInLibCompare )
{
    LIB_SYMBOL other( *m_lib );

    BOOST_CHECK_EQUAL( m_lib->Compare( other, SCH_ITEM::COMPARE_FLAGS::EQUALITY ), 0 );

    SCH_PIN* sclkPin = nullptr;

    for( SCH_PIN* pin : other.GetGraphicalPins( 0, 0 ) )
    {
        if( pin->GetNumber() == wxS( "40" ) )
            sclkPin = pin;
    }

    BOOST_REQUIRE( sclkPin != nullptr );
    sclkPin->GetAlternates().at( wxS( "SPI0_SCLK" ) ).m_FunctionGroupOptional = true;

    BOOST_CHECK( m_lib->Compare( other, SCH_ITEM::COMPARE_FLAGS::EQUALITY ) != 0 );
}


/**
 * A changed subgroup tag also changes the symbol's serialized form, so an on-disk library diff
 * sees it independently of Compare().
 */
BOOST_AUTO_TEST_CASE( SubgroupTagChangesSerializedForm )
{
    STRING_FORMATTER before;
    SCH_IO_KICAD_SEXPR::FormatLibSymbol( m_lib, before );

    LIB_SYMBOL other( *m_lib );

    // Only the const GetPin() overload exists; reach the mutable pin via GetGraphicalPins().
    SCH_PIN* csPin = nullptr;

    for( SCH_PIN* pin : other.GetGraphicalPins( 0, 0 ) )
    {
        if( pin->GetNumber() == wxS( "39" ) )
            csPin = pin;
    }

    BOOST_REQUIRE( csPin != nullptr );
    csPin->GetAlternates().at( wxS( "SPI0_CSn0" ) ).m_FunctionGroupSubgroup = wxS( "CS_ALT" );

    STRING_FORMATTER after;
    SCH_IO_KICAD_SEXPR::FormatLibSymbol( &other, after );

    BOOST_CHECK( before.GetString() != after.GetString() );
    BOOST_CHECK( after.GetString().find( "(subgroup \"CS_ALT\")" ) != std::string::npos );
}


BOOST_AUTO_TEST_SUITE_END()
