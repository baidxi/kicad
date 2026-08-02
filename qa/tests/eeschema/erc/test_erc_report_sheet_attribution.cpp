/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
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
#include <schematic_utils/schematic_file_util.h>

#include <fstream>
#include <memory>
#include <vector>

#include <erc/erc_item.h>
#include <erc/erc_report.h>
#include <erc/erc_settings.h>
#include <json_common.h>
#include <locale_io.h>
#include <sch_sheet_path.h>
#include <schematic.h>
#include <settings/settings_manager.h>

#include <wx/filefn.h>
#include <wx/filename.h>


/**
 * Feeds the report writers a fixed set of ERC items so that the sheet attribution can be
 * checked without depending on which violations a given test schematic happens to produce.
 */
class SCRIPTED_ERC_ITEMS_PROVIDER : public RC_ITEMS_PROVIDER
{
public:
    void Add( const std::shared_ptr<RC_ITEM>& aItem ) { m_items.push_back( aItem ); }

    void SetSeverities( int aSeverities ) override { m_severities = aSeverities; }

    int GetSeverities() const override { return m_severities; }

    int GetCount( int aSeverity = -1 ) const override { return static_cast<int>( m_items.size() ); }

    std::shared_ptr<RC_ITEM> GetItem( int aIndex ) const override { return m_items.at( aIndex ); }

    void DeleteItem( int aIndex, bool aDeep ) override { m_items.erase( m_items.begin() + aIndex ); }

private:
    std::vector<std::shared_ptr<RC_ITEM>> m_items;
    int                                   m_severities = RPT_SEVERITY_ERROR | RPT_SEVERITY_WARNING;
};


struct ERC_REPORT_ATTRIBUTION_FIXTURE
{
    ERC_REPORT_ATTRIBUTION_FIXTURE()
    { }

    SETTINGS_MANAGER           m_settingsManager;
    std::unique_ptr<SCHEMATIC> m_schematic;
};


// Most ERC checks bind a violation to its sheet with SetSheetSpecificPath() only; they never
// call SetItemsSheetPaths().  Both report writers used to fall straight back to the first sheet
// of the hierarchy for those, so on a hierarchical design every such violation was listed under
// one sheet regardless of where the offending item actually lives.
BOOST_FIXTURE_TEST_CASE( ERCReportSubSheetAttribution, ERC_REPORT_ATTRIBUTION_FIXTURE )
{
    LOCALE_IO dummy;

    KI_TEST::LoadSchematic( m_settingsManager, wxS( "issue10926_1" ), m_schematic );
    m_schematic->RefreshHierarchy();

    SCH_SHEET_LIST sheets = m_schematic->Hierarchy();

    BOOST_REQUIRE( sheets.size() > 1 );

    // sheets[0] is the sheet the writers fall back to when a violation carries no sheet at all.
    const SCH_SHEET_PATH& fallbackSheet = sheets[0];
    const SCH_SHEET_PATH& subSheet = sheets[1];
    const std::string     subSheetUuidPath = subSheet.Path().AsString().ToStdString();

    std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_LABEL_NOT_CONNECTED );
    ercItem->SetSheetSpecificPath( subSheet );

    BOOST_REQUIRE( !ercItem->MainItemHasSheetPath() );

    std::shared_ptr<SCRIPTED_ERC_ITEMS_PROVIDER> provider =
            std::make_shared<SCRIPTED_ERC_ITEMS_PROVIDER>();
    provider->Add( ercItem );
    provider->SetSeverities( RPT_SEVERITY_ERROR | RPT_SEVERITY_WARNING );

    ERC_REPORT reportWriter( m_schematic.get(), EDA_UNITS::MM, provider );

    wxString jsonPath = wxFileName::CreateTempFileName( wxS( "kicad_erc_attribution" ) );

    BOOST_REQUIRE( reportWriter.WriteJsonReport( jsonPath ) );

    nlohmann::json report;
    std::ifstream  jsonStream( jsonPath.fn_str() );

    BOOST_REQUIRE( jsonStream.is_open() );
    jsonStream >> report;
    jsonStream.close();
    wxRemoveFile( jsonPath );

    bool foundSubSheet = false;

    for( const nlohmann::json& sheet : report.at( "sheets" ) )
    {
        std::string uuidPath = sheet.at( "uuid_path" ).get<std::string>();
        size_t      violationCount = sheet.at( "violations" ).size();

        if( uuidPath == subSheetUuidPath )
        {
            foundSubSheet = true;
            BOOST_CHECK_EQUAL( violationCount, static_cast<size_t>( 1 ) );
        }
        else
        {
            BOOST_CHECK_MESSAGE( violationCount == 0,
                                 "JSON report attributed the violation to " << uuidPath
                                         << " instead of " << subSheetUuidPath );
        }
    }

    BOOST_CHECK( foundSubSheet );

    // The text report groups violations the same way.
    wxString textReport = reportWriter.GetTextReport();

    auto sheetSection =
            [&textReport]( const SCH_SHEET_PATH& aSheet ) -> wxString
            {
                wxString header = wxString::Format( wxS( "***** Sheet %s\n" ),
                                                    aSheet.PathHumanReadable() );
                int      pos = textReport.Find( header );

                if( pos == wxNOT_FOUND )
                    return wxEmptyString;

                wxString section = textReport.Mid( pos + header.length() );
                int      next = section.Find( wxS( "***** Sheet " ) );

                if( next != wxNOT_FOUND )
                    section = section.Left( next );

                return section;
            };

    wxString violationTag = wxS( "[" ) + ercItem->GetSettingsKey() + wxS( "]" );

    BOOST_CHECK_MESSAGE( sheetSection( subSheet ).Contains( violationTag ),
                         "Text report is missing the violation under its own sheet\n"
                                 << textReport );

    BOOST_CHECK_MESSAGE( !sheetSection( fallbackSheet ).Contains( violationTag ),
                         "Text report listed the sub-sheet violation under the fallback sheet\n"
                                 << textReport );
}
