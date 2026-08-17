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

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <libraries/symbol_library_adapter.h>
#include <libraries/library_manager.h>
#include <template_fieldnames.h>

#include <fstream>
#include <wx/textfile.h>

#include <filesystem>


namespace
{

/**
 * Exposes a way to seed a LOAD_ERROR sentinel entry into the project-scope map,
 * exactly as the async loader leaves it when a library fails to load (status set
 * to LOAD_ERROR, plugin and row left null).
 */
class TEST_SYMBOL_LIBRARY_ADAPTER : public SYMBOL_LIBRARY_ADAPTER
{
public:
    using SYMBOL_LIBRARY_ADAPTER::SYMBOL_LIBRARY_ADAPTER;

    void SeedLoadError( const wxString& aNickname )
    {
        m_libraries[aNickname].status.load_status = LOAD_STATUS::LOAD_ERROR;
    }

    /// Seed a LOAD_ERROR sentinel carrying a real error message, as the async loader
    /// leaves it when a library plugin fails to enumerate.
    void SeedLoadError( const wxString& aNickname, const wxString& aMessage )
    {
        m_libraries[aNickname].status.load_status = LOAD_STATUS::LOAD_ERROR;
        m_libraries[aNickname].status.error = LIBRARY_ERROR( aMessage );
    }

    /// Mark a table row as successfully loaded so GetLibraryNames() reports it.
    void SeedLoaded( const wxString& aNickname )
    {
        m_libraries[aNickname].status.load_status = LOAD_STATUS::LOADED;
    }
};


/// Writes a project sym-lib-table listing the given nicknames in the order supplied.
class SCOPED_SYM_LIB_TABLE
{
public:
    SCOPED_SYM_LIB_TABLE( const std::vector<wxString>& aNicknamesInTableOrder )
    {
        std::error_code ec;
        m_dir = std::filesystem::temp_directory_path( ec )
                / std::filesystem::path( wxString::Format( wxS( "kicad_qa_symlibtbl_%p" ), (void*) this )
                                                 .ToStdString() );
        std::filesystem::create_directories( m_dir, ec );

        wxTextFile file( wxString( ( m_dir / "sym-lib-table" ).string() ) );
        file.Create();
        file.AddLine( wxS( "(sym_lib_table" ) );
        file.AddLine( wxS( "  (version 7)" ) );

        for( const wxString& nickname : aNicknamesInTableOrder )
        {
            file.AddLine( wxString::Format(
                    wxS( "  (lib (name \"%s\")(type \"KiCad\")(uri \"${KIPRJMOD}/%s.kicad_sym\")(options \"\")(descr \"\"))" ),
                    nickname, nickname ) );
        }

        file.AddLine( wxS( ")" ) );
        file.Write();
        file.Close();
    }

    ~SCOPED_SYM_LIB_TABLE()
    {
        std::error_code ec;
        std::filesystem::remove_all( m_dir, ec );
    }

    wxString Path() const { return wxString( m_dir.string() ); }

private:
    std::filesystem::path m_dir;
};

} // namespace


BOOST_AUTO_TEST_SUITE( SymbolLibraryAdapter )


/**
 * Regression test for a null-plugin dereference crash.
 *
 * A symbol library that fails to load leaves a LOAD_ERROR sentinel with a null
 * plugin in the adapter's map. IsSymbolLibWritable() is called for every table row
 * by the symbol editor and chooser, so it must report such a library as not writable
 * instead of dereferencing the null plugin.
 */
BOOST_AUTO_TEST_CASE( IsSymbolLibWritableHandlesFailedLoad )
{
    LIBRARY_MANAGER             manager;
    TEST_SYMBOL_LIBRARY_ADAPTER adapter( manager );

    adapter.SeedLoadError( wxS( "BadLib" ) );

    BOOST_CHECK_EQUAL( adapter.IsSymbolLibWritable( wxS( "BadLib" ) ), false );

    // A library that was never even attempted must also be safe.
    BOOST_CHECK_EQUAL( adapter.IsSymbolLibWritable( wxS( "NeverSeen" ) ), false );
}


/**
 * Regression test for https://gitlab.com/kicad/code/kicad/-/issues/23066.
 *
 * The library browsers list nicknames straight from GetLibraryNames(). When personal
 * libraries were appended to the table after the default ones, the raw table order left
 * them grouped at the bottom instead of interleaved alphabetically. GetLibraryNames() must
 * return the available nicknames in case-insensitive natural order regardless of table order.
 */
BOOST_AUTO_TEST_CASE( GetLibraryNamesSortedRegardlessOfTableOrder )
{
    const std::vector<wxString> tableOrder = { wxS( "Zebra" ), wxS( "amplifier" ), wxS( "Device" ),
                                               wxS( "Board_2" ), wxS( "Board_10" ) };

    SCOPED_SYM_LIB_TABLE tableFile( tableOrder );

    LIBRARY_MANAGER manager;
    manager.LoadProjectTables( tableFile.Path(), { LIBRARY_TABLE_TYPE::SYMBOL } );

    TEST_SYMBOL_LIBRARY_ADAPTER adapter( manager );

    for( const wxString& nickname : tableOrder )
        adapter.SeedLoaded( nickname );

    const std::vector<wxString> names = adapter.GetLibraryNames();

    const std::vector<wxString> expected = { wxS( "amplifier" ), wxS( "Board_2" ), wxS( "Board_10" ),
                                             wxS( "Device" ), wxS( "Zebra" ) };

    BOOST_CHECK_EQUAL_COLLECTIONS( names.begin(), names.end(), expected.begin(), expected.end() );
}


/**
 * Regression test for https://gitlab.com/kicad/code/kicad/-/issues/22764.
 *
 * With "remember open files" enabled the schematic auto-opens and loads symbols on demand
 * while the background preload is still enumerating the library table. At any instant some
 * enabled table rows have no loaded entry yet. GetLibraryLoadErrors() collects the status of
 * every enabled row, and a row present in the table but not yet loaded must not be reported
 * as a load error, otherwise a spurious error indicator appears and its count varies with the
 * load timing from one launch to the next. Genuine failures still carry a real error message
 * and must still be reported.
 */
BOOST_AUTO_TEST_CASE( PartiallyLoadedTableReportsNoSpuriousErrors )
{
    const std::vector<wxString> tableOrder = { wxS( "Device" ), wxS( "Connector" ), wxS( "Amplifier" ),
                                               wxS( "Regulator" ), wxS( "BadLib" ) };

    SCOPED_SYM_LIB_TABLE tableFile( tableOrder );

    LIBRARY_MANAGER manager;
    manager.LoadProjectTables( tableFile.Path(), { LIBRARY_TABLE_TYPE::SYMBOL } );

    TEST_SYMBOL_LIBRARY_ADAPTER adapter( manager );

    // Simulate a preload caught mid-flight: two libraries resolved, one genuinely failed,
    // and the remaining table rows have no entry yet because loading has not reached them.
    adapter.SeedLoaded( wxS( "Device" ) );
    adapter.SeedLoaded( wxS( "Connector" ) );
    adapter.SeedLoadError( wxS( "BadLib" ), wxS( "File is corrupt" ) );

    const wxString errors = adapter.GetLibraryLoadErrors();

    // The unloaded-but-present rows must not masquerade as missing-from-table errors.
    BOOST_CHECK( !errors.Contains( wxS( "not found in library table" ) ) );
    BOOST_CHECK( !errors.Contains( wxS( "Amplifier" ) ) );
    BOOST_CHECK( !errors.Contains( wxS( "Regulator" ) ) );

    // The genuine failure must still be reported.
    BOOST_CHECK( errors.Contains( wxS( "BadLib" ) ) );
    BOOST_CHECK( errors.Contains( wxS( "File is corrupt" ) ) );
}


BOOST_AUTO_TEST_CASE( KiCadSymbolIndexCarriesChooserMetadata )
{
    std::error_code       ec;
    std::filesystem::path dir =
            std::filesystem::temp_directory_path( ec )
            / std::filesystem::path( wxString::Format( wxS( "kicad_qa_symindex_%p" ), (void*) &ec ).ToStdString() );

    std::filesystem::remove_all( dir, ec );
    std::filesystem::create_directories( dir, ec );

    const wxString        nickname = wxS( "IndexMetadata" );
    std::filesystem::path libPath = dir / "IndexMetadata.kicad_sym";
    std::filesystem::path tablePath = dir / "sym-lib-table";

    {
        std::ofstream lib( libPath );
        lib << R"(
(kicad_symbol_lib
  (version 20241209)
  (generator "qa")
  (generator_version "10.0")
  (symbol "ParentPower"
    (power global)
    (property "Reference" "#PWR" (at 0 0 0) (effects (font (size 1.27 1.27))))
    (property "Value" "ParentPower" (at 0 0 0) (effects (font (size 1.27 1.27))))
    (property "Description" "Parent description" (at 0 0 0) (effects (font (size 1.27 1.27))))
    (property "ki_keywords" "parent supply rail" (at 0 0 0) (effects (font (size 1.27 1.27))))
    (symbol "ParentPower_1_1"
      (unit_name "Power")
    )
    (symbol "ParentPower_2_1"
      (unit_name "Sense")
    )
  )
  (symbol "DerivedPower"
    (extends "ParentPower")
    (property "Reference" "#PWR" (at 0 0 0) (effects (font (size 1.27 1.27))))
    (property "Value" "DerivedPower" (at 0 0 0) (effects (font (size 1.27 1.27))))
    (property "Footprint" "Package_TO_SOT_SMD:SOT-23" (at 0 0 0) (effects (font (size 1.27 1.27))))
    (property "Manufacturer" "Acme" (at 0 0 0) (effects (font (size 1.27 1.27))))
  )
)
)";
    }

    wxString uri = wxString::FromUTF8( libPath.generic_string() );

    wxTextFile table( wxString::FromUTF8( tablePath.string() ) );
    table.Create();
    table.AddLine( wxS( "(sym_lib_table" ) );
    table.AddLine( wxS( "  (version 7)" ) );
    table.AddLine( wxString::Format(
            wxS( "  (lib (name \"%s\")(type \"KiCad\")(uri \"%s\")(options \"\")(descr \"\"))" ), nickname, uri ) );
    table.AddLine( wxS( ")" ) );
    table.Write();
    table.Close();

    LIBRARY_MANAGER manager;
    manager.LoadProjectTables( wxString::FromUTF8( dir.string() ), { LIBRARY_TABLE_TYPE::SYMBOL } );

    TEST_SYMBOL_LIBRARY_ADAPTER adapter( manager );
    std::optional<LIB_STATUS>   status = adapter.LoadOne( nickname );

    BOOST_REQUIRE( status.has_value() );
    BOOST_REQUIRE( status->load_status == LOAD_STATUS::LOADED );

    std::vector<SYMBOL_LIBRARY_ADAPTER::SYMBOL_INDEX> index = adapter.GetSymbolIndex( nickname );

    BOOST_REQUIRE_EQUAL( index.size(), 2 );

    auto derived = std::ranges::find_if( index,
                                         []( const SYMBOL_LIBRARY_ADAPTER::SYMBOL_INDEX& aSymbol )
                                         {
                                             return aSymbol.m_Name == wxS( "DerivedPower" );
                                         } );

    BOOST_REQUIRE( derived != index.end() );
    BOOST_CHECK( derived->m_IsPower );
    BOOST_CHECK_EQUAL( derived->m_Description, wxS( "Parent description" ) );
    BOOST_CHECK_EQUAL( derived->m_KeyWords, wxS( "parent supply rail" ) );
    BOOST_CHECK_EQUAL( derived->m_Footprint, wxS( "Package_TO_SOT_SMD:SOT-23" ) );
    BOOST_REQUIRE_EQUAL( derived->m_UnitCount, 2 );
    BOOST_REQUIRE( derived->m_UnitDisplayNames.contains( 1 ) );
    BOOST_REQUIRE( derived->m_UnitDisplayNames.contains( 2 ) );
    BOOST_CHECK_EQUAL( derived->m_UnitDisplayNames.at( 1 ), wxS( "Power" ) );
    BOOST_CHECK_EQUAL( derived->m_UnitDisplayNames.at( 2 ), wxS( "Sense" ) );

    BOOST_REQUIRE( derived->m_ChooserFields.contains( GetDefaultFieldName( FIELD_T::VALUE, false ) ) );
    BOOST_REQUIRE( derived->m_ChooserFields.contains( GetDefaultFieldName( FIELD_T::FOOTPRINT, false ) ) );
    BOOST_REQUIRE( derived->m_ChooserFields.contains( wxS( "Manufacturer" ) ) );
    BOOST_REQUIRE( derived->m_ChooserFields.contains( _( "Keywords" ) ) );
    BOOST_CHECK_EQUAL( derived->m_ChooserFields.at( GetDefaultFieldName( FIELD_T::VALUE, false ) ),
                       wxS( "DerivedPower" ) );
    BOOST_CHECK_EQUAL( derived->m_ChooserFields.at( GetDefaultFieldName( FIELD_T::FOOTPRINT, false ) ),
                       wxS( "Package_TO_SOT_SMD:SOT-23" ) );
    BOOST_CHECK_EQUAL( derived->m_ChooserFields.at( wxS( "Manufacturer" ) ), wxS( "Acme" ) );
    BOOST_CHECK_EQUAL( derived->m_ChooserFields.at( _( "Keywords" ) ), wxS( "parent supply rail" ) );

    std::vector<wxString> powerNames =
            adapter.GetSymbolNames( nickname, SYMBOL_LIBRARY_ADAPTER::SYMBOL_TYPE::POWER_ONLY );
    std::vector<wxString> expectedPowerNames = { wxS( "DerivedPower" ), wxS( "ParentPower" ) };

    BOOST_CHECK_EQUAL_COLLECTIONS( powerNames.begin(), powerNames.end(), expectedPowerNames.begin(),
                                   expectedPowerNames.end() );

    std::filesystem::remove_all( dir, ec );
}


BOOST_AUTO_TEST_SUITE_END()
