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

/**
 * @file test_easyedapro_v3_import.cpp
 * Lightweight regression tests for EasyEDA Pro v3 raw index and parser behavior
 */

#include <boost/test/unit_test.hpp>

#include <array>
#include <set>

#include <qa_utils/file_utils.h>
#include <qa_utils/wx_utils/unit_test_utils.h>

#include <io/easyedapro/easyedapro_import_utils.h>
#include <io/easyedapro/easyedapro_v3_parser.h>

#include <sch_io/easyedapro/sch_easyedapro_v3_parser.h>

#include <pgm_base.h>
#include <schematic.h>
#include <sch_io/sch_io.h>
#include <sch_io/sch_io_mgr.h>
#include <sch_label.h>
#include <sch_line.h>
#include <sch_pin.h>
#include <sch_screen.h>
#include <sch_symbol.h>
#include <sch_text.h>

#include <settings/settings_manager.h>

#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>


namespace
{

static wxString getEasyEdaProV3ArchivePath()
{
    return wxString::FromUTF8( KI_TEST::GetTestDataRootDir()
            + "pcbnew/plugins/easyedapro/ProProject_LS2K0300Core_2025-11-14.epro2" );
}


static void writeZipEntry( wxZipOutputStream& aZip, const wxString& aName, const std::string& aContent )
{
    aZip.PutNextEntry( aName );
    aZip.Write( aContent.data(), aContent.size() );
    aZip.CloseEntry();
}


static wxString makeTempElibz2Path( const wxString& aPrefix )
{
    wxString tempPath = wxFileName::CreateTempFileName( aPrefix );
    wxRemoveFile( tempPath );

    wxFileName fn( tempPath );
    fn.SetExt( wxS( "elibz2" ) );

    return fn.GetFullPath();
}


static void writeV3LibraryArchive( const wxString& aPath, const std::string& aSymbolIndex,
                                   const std::string& aFootprintIndex, const std::string& aElibu )
{
    wxFFileOutputStream out( aPath );
    BOOST_REQUIRE( out.IsOk() );

    wxZipOutputStream zip( out, -1, wxConvUTF8 );

    if( !aSymbolIndex.empty() )
        writeZipEntry( zip, wxS( "symbol2.json" ), aSymbolIndex );

    if( !aFootprintIndex.empty() )
        writeZipEntry( zip, wxS( "footprint2.json" ), aFootprintIndex );

    writeZipEntry( zip, wxS( "library.elibu" ), aElibu );
}

} // namespace


BOOST_AUTO_TEST_SUITE( EasyEdaProV3Import )


BOOST_AUTO_TEST_CASE( LibraryIndexMergeKeepsSymbolAndFootprintNames )
{
    wxString archivePath = makeTempElibz2Path( wxS( "easyedapro_v3_mixed" ) );

    const std::string symbolIndex = R"({
        "devices": {},
        "symbols": {
            "sym_uuid": { "uuid": "sym_uuid", "display_title": "SymbolName" }
        },
        "footprints": {},
        "panelLibs": {}
    })";

    const std::string footprintIndex = R"({
        "devices": {},
        "symbols": {},
        "footprints": {
            "fp_uuid": { "uuid": "fp_uuid", "display_title": "FootprintName" }
        },
        "panelLibs": {}
    })";

    const std::string elibu = "{\"type\":\"DOCHEAD\"}||{\"docType\":\"SYMBOL\",\"uuid\":\"sym_uuid\"}|\n"
                              "{\"type\":\"DOCHEAD\"}||{\"docType\":\"FOOTPRINT\",\"uuid\":\"fp_uuid\"}|\n";

    writeV3LibraryArchive( archivePath, symbolIndex, footprintIndex, elibu );

    EASYEDAPRO::V3_DOC_PARSER parser( archivePath );
    BOOST_REQUIRE_NO_THROW( parser.LoadLibrary() );

    std::map<wxString, wxString> symbols = EASYEDAPRO::BuildV3LibraryItemMap( parser, "symbols", wxS( "SYMBOL" ) );
    std::map<wxString, wxString> footprints =
            EASYEDAPRO::BuildV3LibraryItemMap( parser, "footprints", wxS( "FOOTPRINT" ) );

    BOOST_CHECK_EQUAL( symbols.at( wxS( "SymbolName" ) ), wxString( wxS( "sym_uuid" ) ) );
    BOOST_CHECK_EQUAL( footprints.at( wxS( "FootprintName" ) ), wxString( wxS( "fp_uuid" ) ) );

    BOOST_CHECK( wxRemoveFile( archivePath ) );
}


BOOST_AUTO_TEST_CASE( LibraryItemDuplicateNamesGetNumericSuffix )
{
    wxString archivePath = makeTempElibz2Path( wxS( "easyedapro_v3_dupes" ) );

    const std::string symbolIndex = R"({
        "devices": {},
        "symbols": {
            "aaaaaaaa0001": { "uuid": "aaaaaaaa0001", "display_title": "DUP" },
            "aaaaaaaa0002": { "uuid": "aaaaaaaa0002", "display_title": "DUP" },
            "aaaaaaaa0003": { "uuid": "aaaaaaaa0003", "display_title": "DUP" }
        },
        "footprints": {},
        "panelLibs": {}
    })";

    const std::string elibu = "{\"type\":\"DOCHEAD\"}||{\"docType\":\"SYMBOL\",\"uuid\":\"aaaaaaaa0001\"}|\n"
                              "{\"type\":\"DOCHEAD\"}||{\"docType\":\"SYMBOL\",\"uuid\":\"aaaaaaaa0002\"}|\n"
                              "{\"type\":\"DOCHEAD\"}||{\"docType\":\"SYMBOL\",\"uuid\":\"aaaaaaaa0003\"}|\n";

    writeV3LibraryArchive( archivePath, symbolIndex, std::string(), elibu );

    EASYEDAPRO::V3_DOC_PARSER parser( archivePath );
    BOOST_REQUIRE_NO_THROW( parser.LoadLibrary() );

    std::map<wxString, wxString> symbols = EASYEDAPRO::BuildV3LibraryItemMap( parser, "symbols", wxS( "SYMBOL" ) );

    BOOST_REQUIRE_EQUAL( symbols.size(), 3 );
    BOOST_CHECK( symbols.contains( wxS( "DUP" ) ) );
    BOOST_CHECK( symbols.contains( wxS( "DUP_2" ) ) );
    BOOST_CHECK( symbols.contains( wxS( "DUP_3" ) ) );

    BOOST_CHECK( wxRemoveFile( archivePath ) );
}


BOOST_AUTO_TEST_CASE( RawDocNullTombstonesRemovedAfterDeduplication )
{
    wxString archivePath = makeTempElibz2Path( wxS( "easyedapro_v3_tombstone" ) );

    const std::string footprintIndex = R"({
        "devices": {},
        "symbols": {},
        "footprints": {},
        "panelLibs": {}
    })";

    // p1: live row superseded by a newer null tombstone (must disappear).
    // p2: null tombstone superseded by a newer live row (must survive).
    // p3: plain live row (must survive).
    const std::string elibu =
            "{\"type\":\"DOCHEAD\"}||{\"docType\":\"FOOTPRINT\",\"uuid\":\"fp_tombstone\"}|\n"
            "{\"type\":\"POLY\",\"id\":\"p1\",\"ticket\":1}||{\"path\":[[0,0],[10,10]],\"width\":1}|\n"
            "{\"type\":\"POLY\",\"id\":\"p1\",\"ticket\":2}|||\n"
            "{\"type\":\"POLY\",\"id\":\"p2\",\"ticket\":1}|||\n"
            "{\"type\":\"POLY\",\"id\":\"p2\",\"ticket\":2}||{\"path\":[[0,0],[10,10]],\"width\":1}|\n"
            "{\"type\":\"FILL\",\"id\":\"p3\",\"ticket\":1}||{\"path\":[[0,0],[10,10]]}|\n";

    writeV3LibraryArchive( archivePath, std::string(), footprintIndex, elibu );

    EASYEDAPRO::V3_DOC_PARSER parser( archivePath );
    BOOST_REQUIRE_NO_THROW( parser.LoadLibrary() );

    const EASYEDAPRO::V3_DOC_RAW* doc = parser.FindRawDoc( wxS( "FOOTPRINT" ), wxS( "fp_tombstone" ) );
    BOOST_REQUIRE( doc );

    // Every surviving row carries a non-null inner payload: null tombstones are dropped.
    for( const EASYEDAPRO::V3_ROW& row : doc->rows )
        BOOST_CHECK( !row.inner.is_null() );

    // p1 was superseded by a newer null tombstone and must be gone.
    BOOST_CHECK( !doc->rowById.contains( wxS( "p1" ) ) );

    // p2 and p3 must survive.
    BOOST_REQUIRE( doc->rowById.contains( wxS( "p2" ) ) );
    BOOST_REQUIRE( doc->rowById.contains( wxS( "p3" ) ) );
    BOOST_CHECK_EQUAL( doc->rows.size(), 2 );

    // rowById must index every surviving row at its compacted position.
    for( const auto& [id, index] : doc->rowById )
    {
        BOOST_REQUIRE( index < doc->rows.size() );
        BOOST_CHECK_EQUAL( doc->rows[index].id, id );
    }

    BOOST_CHECK( wxRemoveFile( archivePath ) );
}


BOOST_AUTO_TEST_CASE( BuildProjectIndexHasSchematicSheets )
{
    EASYEDAPRO::V3_DOC_PARSER v3( getEasyEdaProV3ArchivePath() );
    BOOST_REQUIRE_NO_THROW( v3.Load() );

    nlohmann::json project = EASYEDAPRO::BuildV3ProjectIndexFromRawDocs( v3 );

    BOOST_REQUIRE( project.contains( "schematics" ) );
    BOOST_REQUIRE( project.at( "schematics" ).is_object() );
    BOOST_REQUIRE( !project.at( "schematics" ).empty() );

    bool hasAtLeastOneSheet = false;

    for( const auto& [schUuid, sch] : project.at( "schematics" ).items() )
    {
        if( sch.contains( "sheets" ) && sch.at( "sheets" ).is_array()
            && !sch.at( "sheets" ).empty() )
        {
            hasAtLeastOneSheet = true;
            break;
        }
    }

    BOOST_CHECK( hasAtLeastOneSheet );
}


BOOST_AUTO_TEST_CASE( RawSchematicPageContainsTextWireAndNetAttr )
{
    EASYEDAPRO::V3_DOC_PARSER v3( getEasyEdaProV3ArchivePath() );
    BOOST_REQUIRE_NO_THROW( v3.Load() );

    const auto& pages = v3.GetRawDocs( wxS( "SCH_PAGE" ) );
    BOOST_REQUIRE( !pages.empty() );

    bool hasTextValue = false;
    bool hasLineStartEnd = false;
    bool hasWireNetAttr = false;

    for( const auto& [pageUuid, pageDoc] : pages )
    {
        for( const EASYEDAPRO::V3_ROW& row : pageDoc.rows )
        {
            if( row.type == wxS( "TEXT" ) && row.inner.contains( "value" ) )
                hasTextValue = true;

            if( row.type == wxS( "LINE" )
                && row.inner.contains( "startX" ) && row.inner.contains( "startY" )
                && row.inner.contains( "endX" ) && row.inner.contains( "endY" ) )
            {
                hasLineStartEnd = true;
            }

            if( row.type == wxS( "ATTR" )
                && EASYEDAPRO::V3GetString( row.inner, "key" ) == wxS( "NET" ) )
            {
                hasWireNetAttr = true;
            }
        }
    }

    BOOST_CHECK( hasTextValue );
    BOOST_CHECK( hasLineStartEnd );
    BOOST_CHECK( hasWireNetAttr );
}


BOOST_AUTO_TEST_CASE( RawSymbolContainsBlobObjectReference )
{
    EASYEDAPRO::V3_DOC_PARSER v3( getEasyEdaProV3ArchivePath() );
    BOOST_REQUIRE_NO_THROW( v3.Load() );

    const auto& symbols = v3.GetRawDocs( wxS( "SYMBOL" ) );
    BOOST_REQUIRE( !symbols.empty() );

    bool hasBlobObject = false;

    for( const auto& [symbolUuid, symbolDoc] : symbols )
    {
        for( const EASYEDAPRO::V3_ROW& row : symbolDoc.rows )
        {
            if( row.type != wxS( "OBJ" ) )
                continue;

            wxString content = EASYEDAPRO::V3GetString( row.inner, "content" );

            if( content.StartsWith( wxS( "blob:" ) ) )
            {
                hasBlobObject = true;
                break;
            }
        }

        if( hasBlobObject )
            break;
    }

    BOOST_CHECK( hasBlobObject );
}


BOOST_AUTO_TEST_CASE( RawSymbolPartUnitsAndPinOrientationAreStable )
{
    EASYEDAPRO::V3_DOC_PARSER v3( getEasyEdaProV3ArchivePath() );
    BOOST_REQUIRE_NO_THROW( v3.Load() );

    nlohmann::json project = EASYEDAPRO::BuildV3ProjectIndexFromRawDocs( v3 );
    std::map<wxString, EASYEDAPRO::PRJ_DEVICE> devices = project.at( "devices" );

    auto findDevice = [&]( const wxString& aTitle ) -> const EASYEDAPRO::PRJ_DEVICE*
    {
        for( const auto& [devUuid, dev] : devices )
        {
            if( dev.title == aTitle )
                return &dev;
        }

        return nullptr;
    };

    const EASYEDAPRO::PRJ_DEVICE* ls2kDevice = findDevice( wxS( "LS2K0300" ) );
    BOOST_REQUIRE( ls2kDevice );

    const EASYEDAPRO::PRJ_DEVICE* sgmDevice = findDevice( wxS( "SGM61032BXKB6G/TR" ) );
    BOOST_REQUIRE( sgmDevice );

    SCH_EASYEDAPRO_V3_PARSER parser( nullptr, nullptr );

    wxString ls2kSymbolUuid = ls2kDevice->attributes.at( wxS( "Symbol" ) );
    const EASYEDAPRO::V3_DOC_RAW* ls2kRaw = v3.FindRawDoc( wxS( "SYMBOL" ), ls2kSymbolUuid );
    BOOST_REQUIRE( ls2kRaw );

    EASYEDAPRO::SYM_INFO ls2kInfo = parser.ParseSymbol( *ls2kRaw, ls2kDevice->attributes );
    BOOST_REQUIRE( ls2kInfo.libSymbol );
    BOOST_CHECK_EQUAL( ls2kInfo.libSymbol->GetUnitCount(), 5 );

    std::array<int, 6> unitPinCount = {};

    for( SCH_PIN* pin : ls2kInfo.libSymbol->GetGraphicalPins( ALL_UNITS, ALL_BODY_STYLES ) )
    {
        int unit = pin->GetUnit();

        if( unit >= 1 && unit <= 5 )
            unitPinCount[unit]++;
    }

    for( int unit = 1; unit <= 5; ++unit )
        BOOST_CHECK_MESSAGE( unitPinCount[unit] > 0, "LS2K0300 should have pins in each unit" );

    wxString sgmSymbolUuid = sgmDevice->attributes.at( wxS( "Symbol" ) );
    const EASYEDAPRO::V3_DOC_RAW* sgmRaw = v3.FindRawDoc( wxS( "SYMBOL" ), sgmSymbolUuid );
    BOOST_REQUIRE( sgmRaw );

    EASYEDAPRO::SYM_INFO sgmInfo = parser.ParseSymbol( *sgmRaw, sgmDevice->attributes );
    BOOST_REQUIRE( sgmInfo.libSymbol );

    bool hasLeftPins = false;
    bool hasRightPins = false;

    for( SCH_PIN* pin : sgmInfo.libSymbol->GetGraphicalPins( ALL_UNITS, ALL_BODY_STYLES ) )
    {
        if( pin->GetPosition().x < 0 )
        {
            hasLeftPins = true;
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetOrientation() ),
                               static_cast<int>( PIN_ORIENTATION::PIN_RIGHT ) );
        }
        else if( pin->GetPosition().x > 0 )
        {
            hasRightPins = true;
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetOrientation() ),
                               static_cast<int>( PIN_ORIENTATION::PIN_LEFT ) );
        }
    }

    BOOST_CHECK( hasLeftPins );
    BOOST_CHECK( hasRightPins );
}


BOOST_AUTO_TEST_CASE( PluginLoadProducesWireTextAndLabel )
{
    KI_TEST::SCOPED_TEMP_PROJECT tempProject( Pgm().GetSettingsManager(), wxS( "easyedapro_v3_import" ),
                                              wxS( "easyedapro_v3_test" ) );

    wxFileName archiveFile( tempProject.DirStr(), "sample", "epro2" );

    BOOST_REQUIRE( wxCopyFile( getEasyEdaProV3ArchivePath(), archiveFile.GetFullPath() ) );

    SCHEMATIC schematic( nullptr );
    schematic.SetProject( &tempProject.Project() );

    IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_EASYEDAPRO_V3 ) );
    BOOST_REQUIRE( plugin );

    SCH_SHEET* rootSheet = nullptr;
    BOOST_REQUIRE_NO_THROW( rootSheet = plugin->LoadSchematicFile( archiveFile.GetFullPath(),
                                                                   &schematic, nullptr, nullptr ) );
    BOOST_REQUIRE( rootSheet );

    schematic.RefreshHierarchy();

    int wireCount = 0;
    int textCount = 0;
    int labelCount = 0;

    for( const SCH_SHEET_PATH& sheetPath : schematic.Hierarchy() )
    {
        SCH_SCREEN* screen = sheetPath.LastScreen();

        if( !screen )
            continue;

        for( SCH_ITEM* item : screen->Items().OfType( SCH_LINE_T ) )
        {
            SCH_LINE* line = static_cast<SCH_LINE*>( item );

            if( line->GetLayer() == LAYER_WIRE )
                wireCount++;
        }

        for( SCH_ITEM* item : screen->Items().OfType( SCH_TEXT_T ) )
        {
            SCH_TEXT* text = static_cast<SCH_TEXT*>( item );

            if( !text->GetText().IsEmpty() )
                textCount++;
        }

        for( SCH_ITEM* item : screen->Items().OfType( SCH_LABEL_T ) )
        {
            SCH_LABEL* label = static_cast<SCH_LABEL*>( item );

            if( !label->GetText().IsEmpty() )
                labelCount++;
        }
    }

    BOOST_CHECK( wireCount > 0 );
    BOOST_CHECK( textCount > 0 );
    BOOST_CHECK( labelCount > 0 );
}


BOOST_AUTO_TEST_CASE( PluginLoadPreservesWireNetLabelAlignment )
{
    KI_TEST::SCOPED_TEMP_PROJECT tempProject( Pgm().GetSettingsManager(), wxS( "easyedapro_v3_import" ),
                                              wxS( "easyedapro_v3_test" ) );

    wxFileName archiveFile( tempProject.DirStr(), "sample", "epro2" );

    BOOST_REQUIRE( wxCopyFile( getEasyEdaProV3ArchivePath(), archiveFile.GetFullPath() ) );

    // Pick a stable, unique NET label from raw docs where align indicates the text is anchored
    // on the right side of the connection point.
    EASYEDAPRO::V3_DOC_PARSER v3( archiveFile.GetFullPath() );
    BOOST_REQUIRE_NO_THROW( v3.Load() );

    struct NET_ATTR
    {
        wxString value;
        wxString align;
        int      rotation;
    };

    std::vector<NET_ATTR>   candidates;
    std::map<wxString, int> valueCounts;

    for( const auto& [pageUuid, pageDoc] : v3.GetRawDocs( wxS( "SCH_PAGE" ) ) )
    {
        for( const EASYEDAPRO::V3_ROW& row : pageDoc.rows )
        {
            if( row.type != wxS( "ATTR" ) )
                continue;

            if( EASYEDAPRO::V3GetString( row.inner, "key" ) != wxS( "NET" ) )
                continue;

            if( !EASYEDAPRO::V3GetBool( row.inner, "valueVisible" ) )
                continue;

            wxString value =
                    EASYEDAPRO::V3JsonToString( row.inner.value( "value", nlohmann::json() ) );

            if( value.IsEmpty() )
                continue;

            valueCounts[value]++;

            wxString align = EASYEDAPRO::V3GetString( row.inner, "align", wxS( "LEFT_BOTTOM" ) );
            int      rotation = EASYEDAPRO::V3GetInt( row.inner, "rotation", 0 );

            if( align.StartsWith( wxS( "RIGHT" ) ) )
                candidates.push_back( { value, align, rotation } );
        }
    }

    BOOST_REQUIRE( !candidates.empty() );

    NET_ATTR selected = candidates.front();

    for( const NET_ATTR& candidate : candidates )
    {
        if( valueCounts[candidate.value] == 1 )
        {
            selected = candidate;
            break;
        }
    }

    SPIN_STYLE expectedSpinStyle = SPIN_STYLE::RIGHT;

    if( selected.align.StartsWith( wxS( "RIGHT" ) ) )
        expectedSpinStyle = SPIN_STYLE::LEFT;
    else if( selected.align.StartsWith( wxS( "LEFT" ) ) )
        expectedSpinStyle = SPIN_STYLE::RIGHT;

    int rot = selected.rotation % 360;

    if( rot < 0 )
        rot += 360;

    if( rot % 90 == 0 )
    {
        for( int i = 0; i < rot; i += 90 )
            expectedSpinStyle = expectedSpinStyle.RotateCCW();
    }

    // Load via the plugin and validate the resulting schematic keeps label spin style.
    SCHEMATIC schematic( nullptr );
    schematic.SetProject( &tempProject.Project() );

    IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_EASYEDAPRO_V3 ) );
    BOOST_REQUIRE( plugin );

    SCH_SHEET* rootSheet = nullptr;
    BOOST_REQUIRE_NO_THROW( rootSheet =
                                    plugin->LoadSchematicFile( archiveFile.GetFullPath(),
                                                               &schematic, nullptr, nullptr ) );
    BOOST_REQUIRE( rootSheet );

    schematic.RefreshHierarchy();

    int  foundCount = 0;
    bool hasExpectedSpinStyle = false;

    for( const SCH_SHEET_PATH& sheetPath : schematic.Hierarchy() )
    {
        SCH_SCREEN* screen = sheetPath.LastScreen();

        if( !screen )
            continue;

        for( SCH_ITEM* item : screen->Items().OfType( SCH_LABEL_T ) )
        {
            SCH_LABEL* label = static_cast<SCH_LABEL*>( item );

            if( label->GetText() != selected.value )
                continue;

            foundCount++;

            if( static_cast<int>( label->GetSpinStyle() ) == static_cast<int>( expectedSpinStyle ) )
                hasExpectedSpinStyle = true;
        }
    }

    BOOST_CHECK_EQUAL( foundCount, 1 );
    BOOST_CHECK_MESSAGE( hasExpectedSpinStyle,
                         wxString::Format( "Label '%s' spin style mismatch", selected.value ) );
}


BOOST_AUTO_TEST_CASE( PluginLoadConvertsNetportsToGlobalLabels )
{
    KI_TEST::SCOPED_TEMP_PROJECT tempProject( Pgm().GetSettingsManager(), wxS( "easyedapro_v3_import" ),
                                              wxS( "easyedapro_v3_test" ) );

    wxFileName archiveFile( tempProject.DirStr(), "sample", "epro2" );

    BOOST_REQUIRE( wxCopyFile( getEasyEdaProV3ArchivePath(), archiveFile.GetFullPath() ) );

    // Extract expected netport instance names from the raw v3 docs.
    EASYEDAPRO::V3_DOC_PARSER v3( archiveFile.GetFullPath() );
    BOOST_REQUIRE_NO_THROW( v3.Load() );

    std::set<wxString> netportSymbolUuids;

    for( const auto& [uuid, symDoc] : v3.GetRawDocs( wxS( "SYMBOL" ) ) )
    {
        for( const EASYEDAPRO::V3_ROW& row : symDoc.rows )
        {
            if( row.type == wxS( "META" )
                && EASYEDAPRO::V3GetInt( row.inner, "docType", 0 ) == 19 )
            {
                netportSymbolUuids.insert( uuid );
                break;
            }
        }
    }

    BOOST_REQUIRE( !netportSymbolUuids.empty() );

    std::set<wxString> netportComponentIds;

    for( const auto& [pageUuid, pageDoc] : v3.GetRawDocs( wxS( "SCH_PAGE" ) ) )
    {
        for( const EASYEDAPRO::V3_ROW& row : pageDoc.rows )
        {
            if( row.type != wxS( "ATTR" ) )
                continue;

            if( EASYEDAPRO::V3GetString( row.inner, "key" ) != wxS( "Symbol" ) )
                continue;

            wxString symbolUuid = EASYEDAPRO::V3GetString( row.inner, "value" );

            if( netportSymbolUuids.count( symbolUuid ) )
                netportComponentIds.insert( EASYEDAPRO::V3GetString( row.inner, "parentId" ) );
        }
    }

    BOOST_REQUIRE( !netportComponentIds.empty() );

    std::set<wxString> expectedNetportNames;

    for( const auto& [pageUuid, pageDoc] : v3.GetRawDocs( wxS( "SCH_PAGE" ) ) )
    {
        for( const EASYEDAPRO::V3_ROW& row : pageDoc.rows )
        {
            if( row.type != wxS( "ATTR" ) )
                continue;

            wxString parentId = EASYEDAPRO::V3GetString( row.inner, "parentId" );

            if( !netportComponentIds.count( parentId ) )
                continue;

            if( EASYEDAPRO::V3GetString( row.inner, "key" ) != wxS( "Name" ) )
                continue;

            wxString name = EASYEDAPRO::V3JsonToString(
                    row.inner.value( "value", nlohmann::json() ) );

            if( !name.IsEmpty() )
                expectedNetportNames.insert( name );
        }
    }

    BOOST_REQUIRE( !expectedNetportNames.empty() );

    // Load via the plugin and validate the resulting schematic contains global labels (not symbols).
    SCHEMATIC schematic( nullptr );
    schematic.SetProject( &tempProject.Project() );

    IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_EASYEDAPRO_V3 ) );
    BOOST_REQUIRE( plugin );

    SCH_SHEET* rootSheet = nullptr;
    BOOST_REQUIRE_NO_THROW( rootSheet =
                                    plugin->LoadSchematicFile( archiveFile.GetFullPath(),
                                                               &schematic, nullptr, nullptr ) );
    BOOST_REQUIRE( rootSheet );

    schematic.RefreshHierarchy();

    int  netportSymbolCount = 0;
    int  globalLabelCount = 0;
    bool hasInputLabel = false;
    bool hasOutputLabel = false;
    bool hasBidiLabel = false;

    std::set<wxString> importedGlobalLabelNames;

    for( const SCH_SHEET_PATH& sheetPath : schematic.Hierarchy() )
    {
        SCH_SCREEN* screen = sheetPath.LastScreen();

        if( !screen )
            continue;

        for( SCH_ITEM* item : screen->Items().OfType( SCH_SYMBOL_T ) )
        {
            SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );
            wxString    ref = symbol->GetRef( &sheetPath, false );

            if( ref.StartsWith( wxS( "#NET" ) ) )
                netportSymbolCount++;
        }

        for( SCH_ITEM* item : screen->Items().OfType( SCH_GLOBAL_LABEL_T ) )
        {
            SCH_GLOBALLABEL* label = static_cast<SCH_GLOBALLABEL*>( item );

            if( label->GetText().IsEmpty() )
                continue;

            globalLabelCount++;
            importedGlobalLabelNames.insert( label->GetText() );

            switch( label->GetShape() )
            {
            case LABEL_FLAG_SHAPE::L_INPUT:  hasInputLabel = true;  break;
            case LABEL_FLAG_SHAPE::L_OUTPUT: hasOutputLabel = true; break;
            case LABEL_FLAG_SHAPE::L_BIDI:   hasBidiLabel = true;   break;
            default: break;
            }
        }
    }

    BOOST_CHECK_EQUAL( netportSymbolCount, 0 );
    BOOST_CHECK( globalLabelCount > 0 );

    // The test project contains Netport-IN/OUT/BI symbols; ensure we preserve their direction.
    BOOST_CHECK( hasInputLabel );
    BOOST_CHECK( hasOutputLabel );
    BOOST_CHECK( hasBidiLabel );

    // All netport instance names should import as global labels.
    for( const wxString& expected : expectedNetportNames )
    {
        BOOST_CHECK_MESSAGE( importedGlobalLabelNames.count( expected ) > 0,
                             wxString::Format( "Missing global label '%s'", expected ) );
    }
}


BOOST_AUTO_TEST_CASE( PluginLoadKeepsSchematicVerticalOrderAndFootprints )
{
    KI_TEST::SCOPED_TEMP_PROJECT tempProject( Pgm().GetSettingsManager(), wxS( "easyedapro_v3_import" ),
                                              wxS( "easyedapro_v3_test" ) );

    wxFileName archiveFile( tempProject.DirStr(), "sample", "epro2" );

    BOOST_REQUIRE( wxCopyFile( getEasyEdaProV3ArchivePath(), archiveFile.GetFullPath() ) );

    SCHEMATIC schematic( nullptr );
    schematic.SetProject( &tempProject.Project() );

    IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_EASYEDAPRO_V3 ) );
    BOOST_REQUIRE( plugin );

    SCH_SHEET* rootSheet = nullptr;
    BOOST_REQUIRE_NO_THROW( rootSheet =
                                    plugin->LoadSchematicFile( archiveFile.GetFullPath(),
                                                               &schematic, nullptr, nullptr ) );
    BOOST_REQUIRE( rootSheet );

    schematic.RefreshHierarchy();

    SCH_SYMBOL* u6 = nullptr;
    SCH_SYMBOL* u7 = nullptr;
    SCH_SYMBOL* u1 = nullptr;

    for( const SCH_SHEET_PATH& sheetPath : schematic.Hierarchy() )
    {
        SCH_SCREEN* screen = sheetPath.LastScreen();

        if( !screen )
            continue;

        for( SCH_ITEM* item : screen->Items().OfType( SCH_SYMBOL_T ) )
        {
            SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );
            wxString    ref = symbol->GetRef( &sheetPath, false );

            if( ref == wxS( "U6" ) )
                u6 = symbol;
            else if( ref == wxS( "U7" ) )
                u7 = symbol;
            else if( ref == wxS( "U1" ) )
                u1 = symbol;
        }
    }

    BOOST_REQUIRE( u6 );
    BOOST_REQUIRE( u7 );
    BOOST_REQUIRE( u1 );

    BOOST_CHECK_MESSAGE( u7->GetPosition().y < u6->GetPosition().y,
                         "U7 should remain above U6 after EasyEDA Pro v3 import" );

    wxString u6Footprint = u6->GetFootprintFieldText( nullptr, RAW_VALUE );
    wxString u1Footprint = u1->GetFootprintFieldText( nullptr, RAW_VALUE );

    BOOST_CHECK( !u6Footprint.IsEmpty() );
    BOOST_CHECK( u6Footprint.Contains( wxS( "SOT-563-6" ) ) );

    BOOST_CHECK( !u1Footprint.IsEmpty() );
    BOOST_CHECK( u1Footprint.Contains( wxS( "BGA-286_17x17_12.0x12.0mm" ) ) );
}


BOOST_AUTO_TEST_CASE( PluginLoadPowerSymbolsKeepValueAndVisibility )
{
    KI_TEST::SCOPED_TEMP_PROJECT tempProject( Pgm().GetSettingsManager(), wxS( "easyedapro_v3_import" ),
                                              wxS( "easyedapro_v3_test" ) );

    wxFileName archiveFile( tempProject.DirStr(), "sample", "epro2" );

    BOOST_REQUIRE( wxCopyFile( getEasyEdaProV3ArchivePath(), archiveFile.GetFullPath() ) );

    SCHEMATIC schematic( nullptr );
    schematic.SetProject( &tempProject.Project() );

    IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_EASYEDAPRO_V3 ) );
    BOOST_REQUIRE( plugin );

    SCH_SHEET* rootSheet = nullptr;
    BOOST_REQUIRE_NO_THROW( rootSheet =
                                    plugin->LoadSchematicFile( archiveFile.GetFullPath(),
                                                               &schematic, nullptr, nullptr ) );
    BOOST_REQUIRE( rootSheet );

    schematic.RefreshHierarchy();

    int plus5vCount = 0;
    int plus5vShownCount = 0;
    int plus5vOffsetCount = 0;
    int plus5vUprightCount = 0;
    int plus5vUprightAtSymbolXCount = 0;
    int powerValueOnPinBodySideCheckedCount = 0;
    int powerValueOnWrongPinSideCount = 0;
    int otherPowerValueCount = 0;
    int otherPowerShownCount = 0;

    for( const SCH_SHEET_PATH& sheetPath : schematic.Hierarchy() )
    {
        SCH_SCREEN* screen = sheetPath.LastScreen();

        if( !screen )
            continue;

        for( SCH_ITEM* item : screen->Items().OfType( SCH_SYMBOL_T ) )
        {
            SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );

            if( !symbol->IsGlobalPower() )
                continue;

            SCH_FIELD* valueField = symbol->GetField( FIELD_T::VALUE );

            if( !valueField )
                continue;

            wxString valueText = valueField->GetShownText( &sheetPath, INTERNAL );

            if( valueField->IsVisible() && !valueText.IsEmpty() )
            {
                VECTOR2I delta = valueField->GetPosition() - symbol->GetPosition();

                if( delta != VECTOR2I() )
                {
                    std::vector<SCH_PIN*> pins = symbol->GetPins( &sheetPath );

                    if( !pins.empty() )
                    {
                        VECTOR2I dir;

                        switch( pins[0]->PinDrawOrient( symbol->GetTransform() ) )
                        {
                        case PIN_ORIENTATION::PIN_LEFT:  dir = VECTOR2I( -1, 0 ); break;
                        case PIN_ORIENTATION::PIN_RIGHT: dir = VECTOR2I( 1, 0 ); break;
                        case PIN_ORIENTATION::PIN_UP:    dir = VECTOR2I( 0, -1 ); break;
                        case PIN_ORIENTATION::PIN_DOWN:  dir = VECTOR2I( 0, 1 ); break;
                        default:                         dir = VECTOR2I(); break;
                        }

                        if( dir != VECTOR2I() )
                        {
                            long long dot = static_cast<long long>( delta.x ) * dir.x
                                            + static_cast<long long>( delta.y ) * dir.y;

                            powerValueOnPinBodySideCheckedCount++;

                            if( dot < 0 )
                                powerValueOnWrongPinSideCount++;
                        }
                    }
                }
            }

            if( valueText == wxS( "+5V" ) )
            {
                plus5vCount++;

                if( valueField->IsVisible() )
                    plus5vShownCount++;

                if( valueField->GetPosition().y != symbol->GetPosition().y )
                    plus5vOffsetCount++;

                if( symbol->GetOrientation() == SYM_ORIENT_0 )
                {
                    plus5vUprightCount++;

                    if( valueField->GetPosition().x == symbol->GetPosition().x )
                        plus5vUprightAtSymbolXCount++;

                    BOOST_CHECK_EQUAL( valueField->GetHorizJustify(), GR_TEXT_H_ALIGN_CENTER );
                    BOOST_CHECK_EQUAL( valueField->GetVertJustify(), GR_TEXT_V_ALIGN_CENTER );
                }
            }
            else if( valueText.StartsWith( wxS( "+" ) ) )
            {
                otherPowerValueCount++;

                if( valueField->IsVisible() )
                    otherPowerShownCount++;
            }
        }
    }

    BOOST_CHECK( plus5vCount > 0 );
    BOOST_CHECK_EQUAL( plus5vShownCount, plus5vCount );
    BOOST_CHECK( plus5vOffsetCount > 0 );
    BOOST_CHECK( plus5vUprightCount > 0 );
    BOOST_CHECK_EQUAL( plus5vUprightAtSymbolXCount, plus5vUprightCount );

    BOOST_CHECK( powerValueOnPinBodySideCheckedCount > 0 );
    BOOST_CHECK_EQUAL( powerValueOnWrongPinSideCount, 0 );

    BOOST_CHECK( otherPowerValueCount > 0 );
    BOOST_CHECK_EQUAL( otherPowerShownCount, otherPowerValueCount );
}


/**
 * The first EasyEDA page must fill the root sheet instead of leaving an empty
 * index sheet above the real pages.
 */
BOOST_AUTO_TEST_CASE( PluginLoadFillsRootSheetWithFirstPage )
{
    KI_TEST::SCOPED_TEMP_PROJECT tempProject( Pgm().GetSettingsManager(), wxS( "easyedapro_v3_import" ),
                                              wxS( "easyedapro_v3_test" ) );

    wxFileName archiveFile( tempProject.DirStr(), "sample", "epro2" );
    BOOST_REQUIRE( wxCopyFile( getEasyEdaProV3ArchivePath(), archiveFile.GetFullPath() ) );

    SCHEMATIC schematic( nullptr );
    schematic.SetProject( &tempProject.Project() );

    IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_EASYEDAPRO_V3 ) );
    SCH_SHEET* rootSheet = plugin->LoadSchematicFile( archiveFile.GetFullPath(), &schematic );
    BOOST_REQUIRE( rootSheet );
    schematic.RefreshHierarchy();

    int rootSymbols = 0;

    for( SCH_ITEM* item : rootSheet->GetScreen()->Items().OfType( SCH_SYMBOL_T ) )
        (void) item, rootSymbols++;

    // The root screen must carry real schematic content, not just sub-sheet links.
    BOOST_CHECK( rootSymbols > 0 );
    BOOST_CHECK( !rootSheet->GetScreen()->GetPageNumber().IsEmpty() );
}


/**
 * The vertical alignment mapping must read only the vertical token. CENTER is the
 * horizontal token, so a bare CENTER match used to turn e.g. CENTER_BOTTOM into
 * center-justified text.
 */
/**
 * Regression test: an LCSC-style symbol (every pin exported with
 * "Pin Type" = "Undefined") must keep its canvas layout when imported:
 * - EasyEDA Pro canvas Y grows downward, so pins at negative Y appear at the
 *   TOP of the body and pins at positive Y at the BOTTOM; positions must be
 *   scaled without mirroring.
 * - A pin body that grew toward the body edge in EasyEDA (rotation 90 at the
 *   top edge, 270 at the bottom edge) must still point toward the body edge
 *   in KiCad, i.e. keep the same numeric orientation.
 * - Unknown pin types fall back to name-based inference.
 */
BOOST_AUTO_TEST_CASE( SymbolPinOrientationAndInferredTypes )
{
    wxString archivePath = makeTempElibz2Path( wxS( "easyedapro_v3_bq" ) );

    const std::string symbolIndex = R"({
        "devices": {},
        "symbols": {
            "bq_uuid": { "uuid": "bq_uuid", "display_title": "BQ25883RGER" }
        },
        "footprints": {},
        "panelLibs": {}
    })";

    // Mirrors the LCSC BQ25883RGER.elibz2 layout:
    //   pin 7  TS   bottom edge in canvas (y=-70), body rotation 90 (toward body)
    //   pin 19 GND  top edge in canvas    (y=+70), body rotation 270 (toward body)
    //   pin 24 D+   left edge             (x=-60), body rotation 0   (toward body)
    //   pin 5  SCL  left edge             (x=-60), body rotation 0   (toward body)
    const std::string elibu =
        "{\"type\":\"DOCHEAD\"}||{\"docType\":\"SYMBOL\",\"uuid\":\"bq_uuid\","
            "\"editVersion\":\"4.1.60\",\"user\":{}}|\n"
        "{\"type\":\"META\",\"ticket\":1,\"id\":\"META\"}||{\"title\":\"BQ25883RGER\"}|\n"
        "{\"type\":\"PART\",\"ticket\":2,\"id\":\"BQ.1\"}||"
            "{\"BBOX\":[-50,-60,50,60],\"title\":\"BQ.1\"}|\n"
        "{\"type\":\"PIN\",\"ticket\":4,\"id\":\"p7\"}||"
            "{\"partId\":\"BQ.1\",\"x\":-25,\"y\":-70,\"length\":10,\"rotation\":90,"
            "\"pinShape\":\"NONE\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":5,\"id\":\"a7n\"}||"
            "{\"partId\":\"p7\",\"parentId\":\"p7\",\"key\":\"NAME\",\"value\":\"TS\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":6,\"id\":\"a7u\"}||"
            "{\"partId\":\"p7\",\"parentId\":\"p7\",\"key\":\"NUMBER\",\"value\":\"7\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":7,\"id\":\"a7t\"}||"
            "{\"partId\":\"p7\",\"parentId\":\"p7\",\"key\":\"Pin Type\",\"value\":\"Undefined\"}|\n"
        "{\"type\":\"PIN\",\"ticket\":8,\"id\":\"p19\"}||"
            "{\"partId\":\"BQ.1\",\"x\":25,\"y\":70,\"length\":10,\"rotation\":270,"
            "\"pinShape\":\"NONE\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":9,\"id\":\"a19n\"}||"
            "{\"partId\":\"p19\",\"parentId\":\"p19\",\"key\":\"NAME\",\"value\":\"GND\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":10,\"id\":\"a19u\"}||"
            "{\"partId\":\"p19\",\"parentId\":\"p19\",\"key\":\"NUMBER\",\"value\":\"19\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":11,\"id\":\"a19t\"}||"
            "{\"partId\":\"p19\",\"parentId\":\"p19\",\"key\":\"Pin Type\",\"value\":\"Undefined\"}|\n"
        "{\"type\":\"PIN\",\"ticket\":12,\"id\":\"p24\"}||"
            "{\"partId\":\"BQ.1\",\"x\":-60,\"y\":25,\"length\":10,\"rotation\":0,"
            "\"pinShape\":\"NONE\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":13,\"id\":\"a24n\"}||"
            "{\"partId\":\"p24\",\"parentId\":\"p24\",\"key\":\"NAME\",\"value\":\"D+\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":14,\"id\":\"a24u\"}||"
            "{\"partId\":\"p24\",\"parentId\":\"p24\",\"key\":\"NUMBER\",\"value\":\"24\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":15,\"id\":\"a24t\"}||"
            "{\"partId\":\"p24\",\"parentId\":\"p24\",\"key\":\"Pin Type\",\"value\":\"Undefined\"}|\n"
        "{\"type\":\"PIN\",\"ticket\":16,\"id\":\"p5\"}||"
            "{\"partId\":\"BQ.1\",\"x\":-60,\"y\":-25,\"length\":10,\"rotation\":0,"
            "\"pinShape\":\"NONE\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":17,\"id\":\"a5n\"}||"
            "{\"partId\":\"p5\",\"parentId\":\"p5\",\"key\":\"NAME\",\"value\":\"SCL\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":18,\"id\":\"a5u\"}||"
            "{\"partId\":\"p5\",\"parentId\":\"p5\",\"key\":\"NUMBER\",\"value\":\"5\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":19,\"id\":\"a5t\"}||"
            "{\"partId\":\"p5\",\"parentId\":\"p5\",\"key\":\"Pin Type\",\"value\":\"Undefined\"}|\n"
        // Paralleled BAT outputs: only the first may stay a power output.
        "{\"type\":\"PIN\",\"ticket\":20,\"id\":\"p13\"}||"
            "{\"partId\":\"BQ.1\",\"x\":60,\"y\":25,\"length\":10,\"rotation\":180,"
            "\"pinShape\":\"NONE\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":21,\"id\":\"a13n\"}||"
            "{\"partId\":\"p13\",\"parentId\":\"p13\",\"key\":\"NAME\",\"value\":\"BAT\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":22,\"id\":\"a13u\"}||"
            "{\"partId\":\"p13\",\"parentId\":\"p13\",\"key\":\"NUMBER\",\"value\":\"13\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":23,\"id\":\"a13t\"}||"
            "{\"partId\":\"p13\",\"parentId\":\"p13\",\"key\":\"Pin Type\",\"value\":\"Undefined\"}|\n"
        "{\"type\":\"PIN\",\"ticket\":24,\"id\":\"p14\"}||"
            "{\"partId\":\"BQ.1\",\"x\":60,\"y\":15,\"length\":10,\"rotation\":180,"
            "\"pinShape\":\"NONE\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":25,\"id\":\"a14n\"}||"
            "{\"partId\":\"p14\",\"parentId\":\"p14\",\"key\":\"NAME\",\"value\":\"BAT\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":26,\"id\":\"a14u\"}||"
            "{\"partId\":\"p14\",\"parentId\":\"p14\",\"key\":\"NUMBER\",\"value\":\"14\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":27,\"id\":\"a14t\"}||"
            "{\"partId\":\"p14\",\"parentId\":\"p14\",\"key\":\"Pin Type\",\"value\":\"Undefined\"}|\n"
        // Open-drain status output
        "{\"type\":\"PIN\",\"ticket\":28,\"id\":\"p2\"}||"
            "{\"partId\":\"BQ.1\",\"x\":-60,\"y\":5,\"length\":10,\"rotation\":0,"
            "\"pinShape\":\"NONE\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":29,\"id\":\"a2n\"}||"
            "{\"partId\":\"p2\",\"parentId\":\"p2\",\"key\":\"NAME\",\"value\":\"STAT\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":30,\"id\":\"a2u\"}||"
            "{\"partId\":\"p2\",\"parentId\":\"p2\",\"key\":\"NUMBER\",\"value\":\"2\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":31,\"id\":\"a2t\"}||"
            "{\"partId\":\"p2\",\"parentId\":\"p2\",\"key\":\"Pin Type\",\"value\":\"Undefined\"}|\n"
        // Explicit NC pin
        "{\"type\":\"PIN\",\"ticket\":32,\"id\":\"p10\"}||"
            "{\"partId\":\"BQ.1\",\"x\":25,\"y\":-70,\"length\":10,\"rotation\":90,"
            "\"pinShape\":\"NONE\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":33,\"id\":\"a10n\"}||"
            "{\"partId\":\"p10\",\"parentId\":\"p10\",\"key\":\"NAME\",\"value\":\"NC\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":34,\"id\":\"a10u\"}||"
            "{\"partId\":\"p10\",\"parentId\":\"p10\",\"key\":\"NUMBER\",\"value\":\"10\"}|\n"
        "{\"type\":\"ATTR\",\"ticket\":35,\"id\":\"a10t\"}||"
            "{\"partId\":\"p10\",\"parentId\":\"p10\",\"key\":\"Pin Type\",\"value\":\"Undefined\"}|\n";

    writeV3LibraryArchive( archivePath, symbolIndex, std::string(), elibu );

    EASYEDAPRO::V3_DOC_PARSER v3( archivePath );
    BOOST_REQUIRE_NO_THROW( v3.LoadLibrary() );

    const EASYEDAPRO::V3_DOC_RAW* raw = v3.FindRawDoc( wxS( "SYMBOL" ), wxS( "bq_uuid" ) );
    BOOST_REQUIRE( raw );

    SCH_EASYEDAPRO_V3_PARSER parser( nullptr, nullptr );
    EASYEDAPRO::SYM_INFO     info = parser.ParseSymbol( *raw, {} );
    BOOST_REQUIRE( info.libSymbol );

    bool checkedBottom = false;
    bool checkedTop = false;
    bool checkedLeftBidi = false;
    bool checkedLeftInput = false;

    std::vector<SCH_PIN*> pins = info.libSymbol->GetGraphicalPins( ALL_UNITS, ALL_BODY_STYLES );
    BOOST_TEST_MESSAGE( "parsed pin count: " << pins.size() );

    for( SCH_PIN* pin : pins )
    {
        const wxString& num = pin->GetNumber();
        BOOST_TEST_MESSAGE( "pin number '" << num << "' name '" << pin->GetName()
                << "' at (" << pin->GetPosition().x << ","
                << pin->GetPosition().y << ") orient " << static_cast<int>( pin->GetOrientation() )
                << " et " << static_cast<int>( pin->GetType() ) );

        if( num == wxS( "7" ) )
        {
            // Canvas y=-70 is the BOTTOM edge in EasyEDA; the Y mirror must keep
            // it at the bottom in KiCad (y>0), with the body pointing up (toward
            // the body).
            BOOST_CHECK_GT( pin->GetPosition().y, 0 );
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetOrientation() ),
                               static_cast<int>( PIN_ORIENTATION::PIN_UP ) );
            // Analog pin wired to an RC network infers passive.
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetType() ),
                               static_cast<int>( ELECTRICAL_PINTYPE::PT_PASSIVE ) );
            checkedBottom = true;
        }
        else if( num == wxS( "19" ) )
        {
            // Canvas y=+70 is the TOP edge in EasyEDA; the Y mirror must keep it
            // at the top in KiCad (y<0), with the body pointing down (toward the
            // body).
            BOOST_CHECK_LT( pin->GetPosition().y, 0 );
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetOrientation() ),
                               static_cast<int>( PIN_ORIENTATION::PIN_DOWN ) );
            // Ground name infers power input.
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetType() ),
                               static_cast<int>( ELECTRICAL_PINTYPE::PT_POWER_IN ) );
            checkedTop = true;
        }
        else if( num == wxS( "24" ) )
        {
            BOOST_CHECK_LT( pin->GetPosition().x, 0 );
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetOrientation() ),
                               static_cast<int>( PIN_ORIENTATION::PIN_RIGHT ) );
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetType() ),
                               static_cast<int>( ELECTRICAL_PINTYPE::PT_BIDI ) );
            checkedLeftBidi = true;
        }
        else if( num == wxS( "5" ) )
        {
            BOOST_CHECK_LT( pin->GetPosition().x, 0 );
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetOrientation() ),
                               static_cast<int>( PIN_ORIENTATION::PIN_RIGHT ) );
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetType() ),
                               static_cast<int>( ELECTRICAL_PINTYPE::PT_INPUT ) );
            checkedLeftInput = true;
        }
        else if( num == wxS( "13" ) )
        {
            BOOST_CHECK_GT( pin->GetPosition().x, 0 );
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetOrientation() ),
                               static_cast<int>( PIN_ORIENTATION::PIN_LEFT ) );
            // First of two paralleled BAT pins stays a power output.
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetType() ),
                               static_cast<int>( ELECTRICAL_PINTYPE::PT_POWER_OUT ) );
            checkedTop = true;
        }
        else if( num == wxS( "14" ) )
        {
            BOOST_CHECK_GT( pin->GetPosition().x, 0 );
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetOrientation() ),
                               static_cast<int>( PIN_ORIENTATION::PIN_LEFT ) );
            // Second BAT pin of the pair is demoted to passive so that two
            // power outputs never meet in the same net.
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetType() ),
                               static_cast<int>( ELECTRICAL_PINTYPE::PT_PASSIVE ) );
        }
        else if( num == wxS( "2" ) )
        {
            BOOST_CHECK_LT( pin->GetPosition().x, 0 );
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetOrientation() ),
                               static_cast<int>( PIN_ORIENTATION::PIN_RIGHT ) );
            // Open-drain status output infers open collector.
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetType() ),
                               static_cast<int>( ELECTRICAL_PINTYPE::PT_OPENCOLLECTOR ) );
        }
        else if( num == wxS( "10" ) )
        {
            // NC pins infer passive so a stub wire never trips the NC checks.
            BOOST_CHECK_EQUAL( static_cast<int>( pin->GetType() ),
                               static_cast<int>( ELECTRICAL_PINTYPE::PT_PASSIVE ) );
        }
    }

    BOOST_CHECK( checkedBottom );
    BOOST_CHECK( checkedTop );
    BOOST_CHECK( checkedLeftBidi );
    BOOST_CHECK( checkedLeftInput );

    BOOST_CHECK( wxRemoveFile( archivePath ) );
}


// Loads the real-world LCSC archive and verifies every one of its 25 pins is
// imported with a sane position, orientation and electrical type.
BOOST_AUTO_TEST_CASE( RealBQ25883ArchiveAll25Pins )
{
    wxString realArchive = wxS( "/home/juno/tools/kicad/BQ25883RGER.elibz2" );

    if( !wxFileExists( realArchive ) )
    {
        BOOST_TEST_MESSAGE( "real BQ25883RGER.elibz2 not present, skipping" );
        return; // environment-dependent, don't fail machines without the file
    }

    EASYEDAPRO::V3_DOC_PARSER v3( realArchive );
    BOOST_REQUIRE_NO_THROW( v3.LoadLibrary() );

    // find the SYMBOL doc
    const EASYEDAPRO::V3_DOC_RAW* raw = nullptr;
    const auto& symbolDocs = v3.GetRawDocs( wxS( "SYMBOL" ) );

    for( const auto& [uuid, doc] : symbolDocs )
        raw = &doc;

    BOOST_REQUIRE_MESSAGE( raw, "no SYMBOL document found in archive" );

    SCH_EASYEDAPRO_V3_PARSER parser( nullptr, nullptr );
    EASYEDAPRO::SYM_INFO     info = parser.ParseSymbol( *raw, {} );
    BOOST_REQUIRE( info.libSymbol );

    std::vector<SCH_PIN*> pins = info.libSymbol->GetGraphicalPins( ALL_UNITS, ALL_BODY_STYLES );
    BOOST_TEST_MESSAGE( "real BQ25883RGER imported pin count: " << pins.size() );
    BOOST_CHECK_EQUAL( pins.size(), 25 );

    int unspecified = 0;

    for( SCH_PIN* pin : pins )
    {
        int et = static_cast<int>( pin->GetType() );
        int orient = static_cast<int>( pin->GetOrientation() );

        BOOST_TEST_MESSAGE( "  pin " << pin->GetNumber() << " '" << pin->GetName()
                            << "' at (" << pin->GetPosition().x << "," << pin->GetPosition().y
                            << ") orient=" << orient << " et=" << et );

        if( et == static_cast<int>( ELECTRICAL_PINTYPE::PT_UNSPECIFIED ) )
            unspecified++;
    }

    BOOST_CHECK_EQUAL( unspecified, 0 );
}


BOOST_AUTO_TEST_CASE( AlignTokenMapsToJustify )
{
    BOOST_CHECK_EQUAL( EASYEDAPRO::AlignToFontV( wxS( "CENTER_TOP" ) ), 0 );
    BOOST_CHECK_EQUAL( EASYEDAPRO::AlignToFontV( wxS( "CENTER_MIDDLE" ) ), 1 );
    BOOST_CHECK_EQUAL( EASYEDAPRO::AlignToFontV( wxS( "CENTER_BOTTOM" ) ), 2 );
    BOOST_CHECK_EQUAL( EASYEDAPRO::AlignToFontV( wxS( "LEFT_BOTTOM" ) ), 2 );
    BOOST_CHECK_EQUAL( EASYEDAPRO::AlignToFontV( wxS( "RIGHT_MIDDLE" ) ), 1 );

    BOOST_CHECK_EQUAL( EASYEDAPRO::AlignToFontH( wxS( "CENTER_BOTTOM" ) ), 1 );
    BOOST_CHECK_EQUAL( EASYEDAPRO::AlignToFontH( wxS( "LEFT_BOTTOM" ) ), 0 );
    BOOST_CHECK_EQUAL( EASYEDAPRO::AlignToFontH( wxS( "RIGHT_TOP" ) ), 2 );
}


// Real dual-role USB-C receptacle: its VBUS pins are fed from outside the
// sheet (DRP), so the connector heuristic must demote them to passive while
// GND stays a power input. Verifies every pin of the real LCSC archive.
BOOST_AUTO_TEST_CASE( RealLailTypeCArchiveConnectorHeuristic )
{
    wxString realArchive = wxS( "/home/juno/tools/kicad/LAIL-TYPE-C-16PIN-08-WT.elibz2" );

    if( !wxFileExists( realArchive ) )
    {
        BOOST_TEST_MESSAGE( "real LAIL-TYPE-C-16PIN-08-WT.elibz2 not present, skipping" );
        return;
    }

    EASYEDAPRO::V3_DOC_PARSER v3( realArchive );
    BOOST_REQUIRE_NO_THROW( v3.LoadLibrary() );

    const EASYEDAPRO::V3_DOC_RAW* raw = nullptr;
    const auto& symbolDocs = v3.GetRawDocs( wxS( "SYMBOL" ) );

    for( const auto& [uuid, doc] : symbolDocs )
        raw = &doc;

    BOOST_REQUIRE_MESSAGE( raw, "no SYMBOL document found in archive" );

    SCH_EASYEDAPRO_V3_PARSER parser( nullptr, nullptr );
    EASYEDAPRO::SYM_INFO     info = parser.ParseSymbol( *raw, {} );
    BOOST_REQUIRE( info.libSymbol );

    std::vector<SCH_PIN*> pins = info.libSymbol->GetGraphicalPins( ALL_UNITS, ALL_BODY_STYLES );
    BOOST_TEST_MESSAGE( "real LAIL-TYPE-C imported pin count: " << pins.size() );
    BOOST_CHECK_EQUAL( pins.size(), 16 );

    int unspecified = 0;
    int vbusCount = 0;

    for( SCH_PIN* pin : pins )
    {
        int et = static_cast<int>( pin->GetType() );

        BOOST_TEST_MESSAGE( "  pin " << pin->GetNumber() << " '" << pin->GetName()
                            << "' at (" << pin->GetPosition().x << "," << pin->GetPosition().y
                            << ") orient=" << static_cast<int>( pin->GetOrientation() )
                            << " et=" << et );

        if( et == static_cast<int>( ELECTRICAL_PINTYPE::PT_UNSPECIFIED ) )
            unspecified++;

        if( pin->GetName() == wxS( "VBUS" ) )
        {
            vbusCount++;
            // Dual-role power pins of a connector must not claim power input.
            BOOST_CHECK_EQUAL( et, static_cast<int>( ELECTRICAL_PINTYPE::PT_PASSIVE ) );
        }
        else if( pin->GetName() == wxS( "GND" ) )
        {
            BOOST_CHECK_EQUAL( et, static_cast<int>( ELECTRICAL_PINTYPE::PT_POWER_IN ) );
        }
    }

    BOOST_CHECK_EQUAL( vbusCount, 2 );
    BOOST_CHECK_EQUAL( unspecified, 0 );
}


// Real LCSC regulator/charger MP28167: IN is a power input (not a plain
// input), OC is open-collector, OUT is a power output, switching/bootstrap
// pins are passive, and the paralleled GND/AGND pins keep power input.
BOOST_AUTO_TEST_CASE( RealMP28167ArchivePinTypes )
{
    wxString realArchive = wxS( "/home/juno/tools/kicad/MP28167GQ-A-Z.elibz2" );

    if( !wxFileExists( realArchive ) )
    {
        BOOST_TEST_MESSAGE( "real MP28167GQ-A-Z.elibz2 not present, skipping" );
        return;
    }

    EASYEDAPRO::V3_DOC_PARSER v3( realArchive );
    BOOST_REQUIRE_NO_THROW( v3.LoadLibrary() );

    const EASYEDAPRO::V3_DOC_RAW* raw = nullptr;
    const auto& symbolDocs = v3.GetRawDocs( wxS( "SYMBOL" ) );

    for( const auto& [uuid, doc] : symbolDocs )
        raw = &doc;

    BOOST_REQUIRE_MESSAGE( raw, "no SYMBOL document found in archive" );

    SCH_EASYEDAPRO_V3_PARSER parser( nullptr, nullptr );
    EASYEDAPRO::SYM_INFO     info = parser.ParseSymbol( *raw, {} );
    BOOST_REQUIRE( info.libSymbol );

    std::vector<SCH_PIN*> pins = info.libSymbol->GetGraphicalPins( ALL_UNITS, ALL_BODY_STYLES );
    BOOST_TEST_MESSAGE( "real MP28167 imported pin count: " << pins.size() );
    BOOST_CHECK_EQUAL( pins.size(), 16 );

    std::map<wxString, std::pair<wxString, int>> byNumber; // number -> (name, et)

    for( SCH_PIN* pin : pins )
    {
        int et = static_cast<int>( pin->GetType() );

        BOOST_TEST_MESSAGE( "  pin " << pin->GetNumber() << " '" << pin->GetName()
                            << "' at (" << pin->GetPosition().x << "," << pin->GetPosition().y
                            << ") orient=" << static_cast<int>( pin->GetOrientation() )
                            << " et=" << et );

        byNumber[pin->GetNumber()] = { pin->GetName(), et };
    }

    auto checkEt = [&]( const wxString& num, ELECTRICAL_PINTYPE expected )
    {
        auto it = byNumber.find( num );
        BOOST_REQUIRE_MESSAGE( it != byNumber.end(), "missing pin " << num );
        BOOST_CHECK_EQUAL( static_cast<int>( it->second.second ),
                           static_cast<int>( expected ) );
    };

    // internal ELECTRICAL_PINTYPE order:
    // 0 INPUT 1 OUTPUT 2 BIDI 3 TRISTATE 4 PASSIVE 5 NIC 6 UNSPEC
    // 7 POWER_IN 8 POWER_OUT 9 OPENCOLLECTOR 10 OPENEMITTER 11 NC
    checkEt( wxString( wxS( "1" ) ),  ELECTRICAL_PINTYPE::PT_POWER_IN );      // IN
    checkEt( wxString( wxS( "2" ) ),  ELECTRICAL_PINTYPE::PT_POWER_IN );      // GND
    checkEt( wxString( wxS( "3" ) ),  ELECTRICAL_PINTYPE::PT_INPUT );         // EN
    checkEt( wxString( wxS( "4" ) ),  ELECTRICAL_PINTYPE::PT_PASSIVE );       // ALT
    checkEt( wxString( wxS( "5" ) ),  ELECTRICAL_PINTYPE::PT_INPUT );         // SCL
    checkEt( wxString( wxS( "6" ) ),  ELECTRICAL_PINTYPE::PT_BIDI );          // SDA
    checkEt( wxString( wxS( "7" ) ),  ELECTRICAL_PINTYPE::PT_OPENCOLLECTOR ); // OC
    checkEt( wxString( wxS( "8" ) ),  ELECTRICAL_PINTYPE::PT_INPUT );         // FB
    checkEt( wxString( wxS( "9" ) ),  ELECTRICAL_PINTYPE::PT_POWER_OUT );     // VCC (internal LDO out)
    checkEt( wxString( wxS( "10" ) ), ELECTRICAL_PINTYPE::PT_POWER_IN );      // AGND
    checkEt( wxString( wxS( "11" ) ), ELECTRICAL_PINTYPE::PT_POWER_IN );      // GND
    checkEt( wxString( wxS( "12" ) ), ELECTRICAL_PINTYPE::PT_POWER_OUT );     // OUT
    checkEt( wxString( wxS( "13" ) ), ELECTRICAL_PINTYPE::PT_PASSIVE );       // BST2
    checkEt( wxString( wxS( "14" ) ), ELECTRICAL_PINTYPE::PT_PASSIVE );       // SW2
    checkEt( wxString( wxS( "15" ) ), ELECTRICAL_PINTYPE::PT_PASSIVE );       // SW1
    checkEt( wxString( wxS( "16" ) ), ELECTRICAL_PINTYPE::PT_PASSIVE );       // BST1
}


BOOST_AUTO_TEST_SUITE_END()
