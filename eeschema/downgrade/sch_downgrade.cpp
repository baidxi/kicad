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

#include <downgrade/sch_downgrade.h>

#include <cctype>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include <base_units.h>
#include <sch_file_versions.h>
#include <sch_io/kicad_sexpr/writers/sch_writer_v9.h>
#include <sch_io/kicad_sexpr/writers/sch_writer_v10.h>
#include <sch_screen.h>
#include <sch_shape.h>
#include <sch_symbol.h>
#include <sch_group.h>
#include <lib_symbol.h>
#include <pin_map.h>
#include <eda_shape.h>
#include <geometry/shape_poly_set.h>


// Schematic format date each feature landed. A rule fires only when the target predates this.
static constexpr int SCH_VER_SHAPE_HATCH = 20250222;
static constexpr int SCH_VER_JUMPER_PINS = 20250324;
static constexpr int SCH_VER_GROUPS = 20250513;
static constexpr int SCH_VER_LOCKING = 20260326;
static constexpr int SCH_VER_ELLIPSE = 20260508;
static constexpr int SCH_VER_PIN_MAPS = 20260629;
static constexpr int SCH_VER_BODY_STYLES = 20250827;
static constexpr int SCH_VER_VARIANTS = 20250922;
static constexpr int SCH_VER_NET_CHAINS = 20260512;

// The formats this feature was built for. If a version moves past its constant, a new feature
// landed and the rules or denylist may be stale. Review the keywords, then update these.
static constexpr int SCH_DOWNGRADE_COVERED = 20260629;
static constexpr int SYMLIB_DOWNGRADE_COVERED = 20260629;


static bool isEllipse( const SCH_ITEM* aItem )
{
    if( aItem->Type() != SCH_SHAPE_T )
        return false;

    SHAPE_T shape = static_cast<const SCH_SHAPE*>( aItem )->GetShape();
    return shape == SHAPE_T::ELLIPSE || shape == SHAPE_T::ELLIPSE_ARC;
}


static int countLockedItems( const SCH_SCREEN* aScreen )
{
    int count = 0;

    for( SCH_ITEM* item : aScreen->Items() )
    {
        // Group locking predates general item locking, so targets with groups keep those locks.
        if( item->IsLocked() && item->Type() != SCH_GROUP_T )
            count++;
    }

    return count;
}


static void unlockItems( SCH_SCREEN* aScreen )
{
    for( SCH_ITEM* item : aScreen->Items() )
    {
        if( item->IsLocked() && item->Type() != SCH_GROUP_T )
            item->SetLocked( false );
    }
}


static int countEllipses( const SCH_SCREEN* aScreen )
{
    int count = 0;

    for( SCH_ITEM* item : aScreen->Items().OfType( SCH_SHAPE_T ) )
    {
        if( isEllipse( item ) )
            count++;
    }

    // Symbol bodies are written from the screen library cache, so ellipses there count too.
    for( const auto& [name, symbol] : aScreen->GetLibSymbols() )
    {
        if( !symbol )
            continue;

        for( SCH_ITEM& item : symbol->GetDrawItems() )
        {
            if( isEllipse( &item ) )
                count++;
        }
    }

    return count;
}


static void lowerEllipses( SCH_SCREEN* aScreen )
{
    // Collect first, then mutate. Changing items while iterating the R-tree is not safe.
    std::vector<SCH_SHAPE*> ellipses;

    for( SCH_ITEM* item : aScreen->Items().OfType( SCH_SHAPE_T ) )
    {
        if( isEllipse( item ) )
            ellipses.push_back( static_cast<SCH_SHAPE*>( item ) );
    }

    // The writer emits symbol bodies from the screen library cache, so lower those too.
    for( const auto& [name, symbol] : aScreen->GetLibSymbols() )
    {
        if( !symbol )
            continue;

        for( SCH_ITEM& item : symbol->GetDrawItems() )
        {
            if( isEllipse( &item ) )
                ellipses.push_back( static_cast<SCH_SHAPE*>( &item ) );
        }
    }

    int maxError = schIUScale.mmToIU( 0.01 );

    for( SCH_SHAPE* shape : ellipses )
    {
        SHAPE_POLY_SET poly;
        shape->EDA_SHAPE::TransformShapeToPolygon( poly, 0, maxError, ERROR_INSIDE, true );
        shape->SetShape( SHAPE_T::POLY );
        shape->SetPolyShape( poly );
    }
}


// Every fill-bearing schematic item derives from SCH_SHAPE but has its own type id.
static constexpr KICAD_T fillableTypes[] = { SCH_SHAPE_T, SCH_TEXTBOX_T, SCH_RULE_AREA_T };


static bool isHatchedFillItem( const SCH_ITEM& aItem )
{
    if( aItem.Type() != SCH_SHAPE_T && aItem.Type() != SCH_TEXTBOX_T )
        return false;

    return static_cast<const SCH_SHAPE&>( aItem ).IsHatchedFill();
}


static int countHatchedFills( const SCH_SCREEN* aScreen )
{
    int count = 0;

    for( KICAD_T type : fillableTypes )
    {
        for( SCH_ITEM* item : aScreen->Items().OfType( type ) )
        {
            if( static_cast<SCH_SHAPE*>( item )->IsHatchedFill() )
                count++;
        }
    }

    // Symbol bodies are written from the screen library cache, so hatched fills there count too.
    for( const auto& [name, symbol] : aScreen->GetLibSymbols() )
    {
        if( !symbol )
            continue;

        for( SCH_ITEM& item : symbol->GetDrawItems() )
        {
            if( isHatchedFillItem( item ) )
                count++;
        }
    }

    return count;
}


static void lowerHatchedFills( SCH_SCREEN* aScreen )
{
    for( KICAD_T type : fillableTypes )
    {
        for( SCH_ITEM* item : aScreen->Items().OfType( type ) )
        {
            SCH_SHAPE* shape = static_cast<SCH_SHAPE*>( item );

            if( shape->IsHatchedFill() )
                shape->SetFillMode( FILL_T::FILLED_SHAPE );
        }
    }

    // The writer emits symbol bodies from the screen library cache, so lower those too.
    for( const auto& [name, symbol] : aScreen->GetLibSymbols() )
    {
        if( !symbol )
            continue;

        for( SCH_ITEM& item : symbol->GetDrawItems() )
        {
            if( isHatchedFillItem( item ) )
                static_cast<SCH_SHAPE&>( item ).SetFillMode( FILL_T::FILLED_SHAPE );
        }
    }
}


static bool hasPinMaps( const SCH_SYMBOL* aSymbol )
{
    if( !aSymbol->GetPinMapOverride().IsDefault() )
        return true;

    const std::unique_ptr<LIB_SYMBOL>& lib = aSymbol->GetLibSymbolRef();
    return lib && ( lib->GetPinMaps() != PIN_MAP_SET() || !lib->GetAssociatedFootprints().empty() );
}


static int countPinMaps( const SCH_SCREEN* aScreen )
{
    int count = 0;

    for( SCH_ITEM* item : aScreen->Items().OfType( SCH_SYMBOL_T ) )
    {
        if( hasPinMaps( static_cast<const SCH_SYMBOL*>( item ) ) )
            count++;
    }

    return count;
}


static void dropPinMaps( SCH_SCREEN* aScreen )
{
    for( SCH_ITEM* item : aScreen->Items().OfType( SCH_SYMBOL_T ) )
    {
        SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );

        symbol->SetPinMapOverride( PIN_MAP_INSTANCE_OVERRIDE() );

        if( symbol->GetLibSymbolRef() )
            symbol->GetLibSymbolRef()->SetPinMaps( PIN_MAP_SET() );
    }

    // The writer takes (pin_maps) from the screen library cache, so clear it there too.
    for( const auto& [name, symbol] : aScreen->GetLibSymbols() )
    {
        if( symbol )
        {
            symbol->SetPinMaps( PIN_MAP_SET() );
            symbol->SetAssociatedFootprints( {} );
        }
    }
}


static int countGroups( const SCH_SCREEN* aScreen )
{
    auto groups = aScreen->Items().OfType( SCH_GROUP_T );
    return static_cast<int>( std::distance( groups.begin(), groups.end() ) );
}


static void ungroupAll( SCH_SCREEN* aScreen )
{
    std::vector<SCH_GROUP*> groups;

    for( SCH_ITEM* item : aScreen->Items().OfType( SCH_GROUP_T ) )
        groups.push_back( static_cast<SCH_GROUP*>( item ) );

    for( SCH_GROUP* group : groups )
    {
        group->RemoveAll(); // detach members, keeping them on the screen
        aScreen->Remove( group );
    }
}


static int countJumperPinGroups( const SCH_SCREEN* aScreen )
{
    int count = 0;

    for( const auto& [name, symbol] : aScreen->GetLibSymbols() )
    {
        if( symbol && !symbol->JumperPinGroups().empty() )
            count++;
    }

    return count;
}


static void dropJumperPinGroups( SCH_SCREEN* aScreen )
{
    for( const auto& [name, symbol] : aScreen->GetLibSymbols() )
    {
        if( symbol )
        {
            symbol->JumperPinGroups().clear();
            symbol->SetDuplicatePinNumbersAreJumpers( false );
        }
    }
}


// Same pattern as board_downgrade.cpp. One list drives both the report and the transform.
struct SCREEN_RULE
{
    int                                     m_introducedIn;
    DOWNGRADE_BUCKET                        m_bucket;
    wxString                                m_feature;
    wxString                                m_detail;
    std::function<int( const SCH_SCREEN* )> m_count;
    std::function<void( SCH_SCREEN* )>      m_apply;
};


static std::vector<SCREEN_RULE> screenRules()
{
    return {
        { SCH_VER_LOCKING, DOWNGRADE_BUCKET::LOWER, _( "Locked items" ), _( "The lock is not kept." ), countLockedItems,
          unlockItems },
        { SCH_VER_ELLIPSE, DOWNGRADE_BUCKET::LOWER, _( "Ellipse graphics" ), _( "Approximated by a polygon." ),
          countEllipses, lowerEllipses },
        { SCH_VER_SHAPE_HATCH, DOWNGRADE_BUCKET::LOWER, _( "Hatched shape fills" ), _( "Converted to a solid fill." ),
          countHatchedFills, lowerHatchedFills },
        { SCH_VER_PIN_MAPS, DOWNGRADE_BUCKET::DROP, _( "Pin-to-pad maps" ),
          _( "No equivalent in the target. The map is removed." ), countPinMaps, dropPinMaps },
        { SCH_VER_GROUPS, DOWNGRADE_BUCKET::LOWER, _( "Groups" ), _( "Grouping is removed; the items are kept." ),
          countGroups, ungroupAll },
        { SCH_VER_JUMPER_PINS, DOWNGRADE_BUCKET::DROP, _( "Jumper pin groups" ),
          _( "Jumper pin groupings are removed." ), countJumperPinGroups, dropJumperPinGroups },
    };
}


COMPATIBILITY_REPORT ClassifyScreenForDowngrade( const SCH_SCREEN* aScreen, const DOWNGRADE_TARGET& aTarget )
{
    // The rules were written against a specific format. Warn developers when the format has
    // moved on, since a frozen writer silently omits features that have no rule yet.
    wxASSERT_MSG( SEXPR_SCHEMATIC_FILE_VERSION == SCH_DOWNGRADE_COVERED,
                  wxT( "Schematic format changed. Review the downgrade rules and denylist, then "
                       "update SCH_DOWNGRADE_COVERED." ) );

    COMPATIBILITY_REPORT report;

    for( const SCREEN_RULE& rule : screenRules() )
    {
        if( aTarget.m_schVersion >= rule.m_introducedIn )
            continue;

        if( int count = rule.m_count( aScreen ); count > 0 )
            report.Add( rule.m_bucket, rule.m_feature, rule.m_detail, count );
    }

    return report;
}


void DowngradeScreenInPlace( SCH_SCREEN* aScreen, const DOWNGRADE_TARGET& aTarget )
{
    for( const SCREEN_RULE& rule : screenRules() )
    {
        if( aTarget.m_schVersion >= rule.m_introducedIn )
            continue;

        if( rule.m_apply )
            rule.m_apply( aScreen );
    }
}


// Symbol-scoped subset for standalone .kicad_sym files the screen pass never sees.
COMPATIBILITY_REPORT ClassifyLibSymbolForDowngrade( const LIB_SYMBOL* aSymbol, const DOWNGRADE_TARGET& aTarget )
{
    wxASSERT_MSG( SEXPR_SYMBOL_LIB_FILE_VERSION == SYMLIB_DOWNGRADE_COVERED,
                  wxT( "Symbol library format changed. Review the symbol downgrade path, then "
                       "update SYMLIB_DOWNGRADE_COVERED." ) );

    COMPATIBILITY_REPORT report;
    const int            ver = aTarget.m_schVersion;

    int ellipses = 0;
    int hatched = 0;

    for( const SCH_ITEM& item : aSymbol->GetDrawItems() )
    {
        if( item.Type() == SCH_SHAPE_T || item.Type() == SCH_TEXTBOX_T )
        {
            const SCH_SHAPE& shape = static_cast<const SCH_SHAPE&>( item );

            if( shape.GetShape() == SHAPE_T::ELLIPSE || shape.GetShape() == SHAPE_T::ELLIPSE_ARC )
                ellipses++;

            if( shape.IsHatchedFill() )
                hatched++;
        }
    }

    if( ver < SCH_VER_ELLIPSE && ellipses > 0 )
        report.Add( DOWNGRADE_BUCKET::LOWER, _( "Ellipse graphics" ), _( "Approximated by a polygon." ), ellipses );

    if( ver < SCH_VER_SHAPE_HATCH && hatched > 0 )
        report.Add( DOWNGRADE_BUCKET::LOWER, _( "Hatched shape fills" ), _( "Converted to a solid fill." ), hatched );

    if( ver < SCH_VER_PIN_MAPS
        && ( aSymbol->GetPinMaps() != PIN_MAP_SET() || !aSymbol->GetAssociatedFootprints().empty() ) )
    {
        report.Add( DOWNGRADE_BUCKET::DROP, _( "Pin-to-pad maps" ),
                    _( "No equivalent in the target. The map is removed." ) );
    }

    if( ver < SCH_VER_JUMPER_PINS
        && ( !aSymbol->JumperPinGroups().empty() || aSymbol->GetDuplicatePinNumbersAreJumpers() ) )
    {
        report.Add( DOWNGRADE_BUCKET::DROP, _( "Jumper pin groups" ), _( "Jumper pin groupings are removed." ) );
    }

    return report;
}


void DowngradeLibSymbolInPlace( LIB_SYMBOL* aSymbol, const DOWNGRADE_TARGET& aTarget )
{
    const int ver = aTarget.m_schVersion;

    if( ver < SCH_VER_ELLIPSE )
    {
        int                     maxError = schIUScale.mmToIU( 0.01 );
        std::vector<SCH_SHAPE*> ellipses;

        for( SCH_ITEM& item : aSymbol->GetDrawItems() )
        {
            if( item.Type() == SCH_SHAPE_T )
            {
                SCH_SHAPE& shape = static_cast<SCH_SHAPE&>( item );

                if( shape.GetShape() == SHAPE_T::ELLIPSE || shape.GetShape() == SHAPE_T::ELLIPSE_ARC )
                    ellipses.push_back( &shape );
            }
        }

        for( SCH_SHAPE* shape : ellipses )
        {
            SHAPE_POLY_SET poly;
            shape->EDA_SHAPE::TransformShapeToPolygon( poly, 0, maxError, ERROR_INSIDE, true );
            shape->SetShape( SHAPE_T::POLY );
            shape->SetPolyShape( poly );
        }
    }

    if( ver < SCH_VER_SHAPE_HATCH )
    {
        for( SCH_ITEM& item : aSymbol->GetDrawItems() )
        {
            if( item.Type() == SCH_SHAPE_T || item.Type() == SCH_TEXTBOX_T )
            {
                SCH_SHAPE& shape = static_cast<SCH_SHAPE&>( item );

                if( shape.IsHatchedFill() )
                    shape.SetFillMode( FILL_T::FILLED_SHAPE );
            }
        }
    }

    if( ver < SCH_VER_PIN_MAPS )
    {
        aSymbol->SetPinMaps( PIN_MAP_SET() );
        aSymbol->SetAssociatedFootprints( {} );
    }

    if( ver < SCH_VER_JUMPER_PINS )
    {
        aSymbol->JumperPinGroups().clear();
        aSymbol->SetDuplicatePinNumbersAreJumpers( false );
    }
}


// Node heads dated by the format that introduced them. The versioned writers cannot emit these,
// so this gate is a safety net against a wrong or miswired writer. To add a token, add a row
// with its format version.
struct DATED_TOKEN
{
    const char* m_token;
    int         m_introducedIn;
};

static constexpr DATED_TOKEN SCH_TOKENS[] = {
    { "jumper_pin_groups", SCH_VER_JUMPER_PINS },
    { "group", SCH_VER_GROUPS },
    { "body_style", SCH_VER_BODY_STYLES },
    { "body_styles", SCH_VER_BODY_STYLES },
    { "variant", SCH_VER_VARIANTS },
    { "field", SCH_VER_VARIANTS },
    { "locked", SCH_VER_LOCKING },
    { "ellipse", SCH_VER_ELLIPSE },
    { "ellipse_arc", SCH_VER_ELLIPSE },
    { "rotation_angle", SCH_VER_ELLIPSE },
    { "net_chain", SCH_VER_NET_CHAINS },
    { "net_class", SCH_VER_NET_CHAINS },
    { "nets", SCH_VER_NET_CHAINS },
    { "passthrough", SCH_VER_NET_CHAINS },
    { "pin_map", SCH_VER_PIN_MAPS },
    { "pin_maps", SCH_VER_PIN_MAPS },
    { "pin_map_override", SCH_VER_PIN_MAPS },
    { "named_map", SCH_VER_PIN_MAPS },
    { "associated_footprints", SCH_VER_PIN_MAPS },
    { "entry", SCH_VER_PIN_MAPS },
    { "map", SCH_VER_PIN_MAPS },
    { "edit", SCH_VER_PIN_MAPS },
};

// Bareword values the target cannot parse. The node scan cannot see these.
static constexpr DATED_TOKEN SCH_VALUES[] = {
    { "hatch", SCH_VER_SHAPE_HATCH },
    { "reverse_hatch", SCH_VER_SHAPE_HATCH },
    { "cross_hatch", SCH_VER_SHAPE_HATCH },
};


static bool containsSexprNode( const std::string& aText, const std::string& aToken )
{
    const std::string needle = "(" + aToken;

    for( size_t pos = aText.find( needle ); pos != std::string::npos; pos = aText.find( needle, pos + 1 ) )
    {
        size_t after = pos + needle.size();
        char   next = ( after < aText.size() ) ? aText[after] : ' ';

        if( !std::isalnum( static_cast<unsigned char>( next ) ) && next != '_' )
            return true;
    }

    return false;
}


// Some features are written as a bareword value, not a node head, so the node scan cannot see
// them. Match the token as a whole word instead.
static bool containsSexprValue( const std::string& aText, const std::string& aToken )
{
    for( size_t pos = aText.find( aToken ); pos != std::string::npos; pos = aText.find( aToken, pos + 1 ) )
    {
        char   prev = pos > 0 ? aText[pos - 1] : ' ';
        size_t after = pos + aToken.size();
        char   next = after < aText.size() ? aText[after] : ' ';

        bool prevOk = !std::isalnum( static_cast<unsigned char>( prev ) ) && prev != '_';
        bool nextOk = !std::isalnum( static_cast<unsigned char>( next ) ) && next != '_';

        if( prevOk && nextOk )
            return true;
    }

    return false;
}


// Drop quoted strings so a token inside a text value cannot cause a false refusal.
static std::string stripSexprStrings( const std::string& aText )
{
    std::string result;
    bool        inString = false;

    for( size_t i = 0; i < aText.size(); i++ )
    {
        char c = aText[i];

        // A quote is real only if an even number of backslashes precede it. This stops a value that
        // ends in a backslash from swallowing the rest of the file.
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


// Each target has its own writer, extracted from that release. A target without a writer writes
// nothing, and the fail-closed check downstream refuses the export.
void SaveSchematicForTarget( SCH_SHEET* aSheet, SCHEMATIC* aSchematic, const wxString& aPath,
                             const DOWNGRADE_TARGET& aTarget )
{
    if( aTarget.m_schVersion == SCH_WRITER_V10::FORMAT_VERSION )
        SCH_WRITER_V10().SaveSchematicFile( aPath, aSheet, aSchematic );
    else if( aTarget.m_schVersion == SCH_WRITER_V9::FORMAT_VERSION )
        SCH_WRITER_V9().SaveSchematicFile( aPath, aSheet, aSchematic );
    else
        wxFAIL_MSG( wxT( "No writer for this downgrade target." ) );
}


void SaveSymbolLibraryForTarget( const std::vector<LIB_SYMBOL*>& aSymbols, const wxString& aPath,
                                 const DOWNGRADE_TARGET& aTarget )
{
    if( aTarget.m_symLibVersion == SCH_WRITER_V10::SYMBOL_LIB_VERSION )
        SCH_WRITER_V10().SaveSymbolLibrary( aPath, aSymbols );
    else if( aTarget.m_symLibVersion == SCH_WRITER_V9::SYMBOL_LIB_VERSION )
        SCH_WRITER_V9().SaveSymbolLibrary( aPath, aSymbols );
    else
        wxFAIL_MSG( wxT( "No writer for this downgrade target." ) );
}


int SchDowngradeCoveredVersion()
{
    return SCH_DOWNGRADE_COVERED;
}


wxString FindUnsupportedSchToken( const wxString& aSerialized, const DOWNGRADE_TARGET& aTarget )
{
    if( aTarget.m_schVersion >= SEXPR_SCHEMATIC_FILE_VERSION )
        return wxEmptyString;

    // Explicit UTF-8: the default locale conversion returns an empty string on unrepresentable
    // characters, which would silently pass the scan.
    std::string text = stripSexprStrings( aSerialized.ToStdString( wxConvUTF8 ) );

    for( const DATED_TOKEN& token : SCH_TOKENS )
    {
        if( aTarget.m_schVersion < token.m_introducedIn && containsSexprNode( text, token.m_token ) )
            return wxString::FromUTF8( token.m_token );
    }

    for( const DATED_TOKEN& value : SCH_VALUES )
    {
        if( aTarget.m_schVersion < value.m_introducedIn && containsSexprValue( text, value.m_token ) )
            return wxString::FromUTF8( value.m_token );
    }

    return wxEmptyString;
}
