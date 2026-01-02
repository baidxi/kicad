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
 * @file test_packed_footprint_lib.cpp
 *
 * Test suite for packed footprint library (.kicad_fpl) support.
 * Tests loading, saving, and roundtrip conversion between packed and unpacked formats.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <boost/test/unit_test.hpp>

#include <filesystem>

#include <footprint.h>
#include <pcb_io/kicad_sexpr/pcb_io_kicad_sexpr.h>
#include <pcb_io/pcb_io_mgr.h>
#include <pcbnew_utils/board_file_utils.h>
#include <pcbnew_utils/board_test_utils.h>
#include <wildcards_and_files_ext.h>


namespace
{

/**
 * A temporary directory that will be deleted when it goes out of scope.
 */
class TEMPORARY_DIRECTORY
{
public:
    TEMPORARY_DIRECTORY( const std::string& aNamePrefix, const std::string& aSuffix )
    {
        int i = 0;

        while( true )
        {
            m_path = std::filesystem::temp_directory_path()
                     / ( aNamePrefix + std::to_string( i ) + aSuffix );

            if( !std::filesystem::exists( m_path ) )
                break;

            i++;
        }

        std::filesystem::create_directories( m_path );
    }

    ~TEMPORARY_DIRECTORY() { std::filesystem::remove_all( m_path ); }

    const std::filesystem::path& GetPath() const { return m_path; }

private:
    std::filesystem::path m_path;
};


/**
 * Get the absolute path to the packed test library
 */
wxString GetPackedTestLibPath()
{
    return wxString( KI_TEST::GetPcbnewTestDataDir()
                     + "plugins/kicad_sexpr/packed_fp_lib.kicad_fpl" );
}


/**
 * Get the absolute path to the unpacked test library
 */
wxString GetUnpackedTestLibPath()
{
    return wxString( KI_TEST::GetPcbnewTestDataDir() + "plugins/kicad_sexpr/fp.pretty" );
}

} // namespace


BOOST_AUTO_TEST_SUITE( PackedFootprintLib )


/**
 * Test that the packed library file extension constant is correctly defined.
 */
BOOST_AUTO_TEST_CASE( FileExtensionConstant )
{
    BOOST_CHECK_EQUAL( FILEEXT::KiCadPackedFootprintLibFileExtension, "kicad_fpl" );
}


/**
 * Test that the plugin can detect a packed library format.
 */
BOOST_AUTO_TEST_CASE( CanReadPackedLibrary )
{
    PCB_IO_KICAD_SEXPR plugin;
    wxString           libPath = GetPackedTestLibPath();

    BOOST_TEST_MESSAGE( "Testing CanReadLibrary for: " << libPath );
    BOOST_CHECK( plugin.CanReadLibrary( libPath ) );
}


/**
 * Test that the plugin can detect an unpacked library format.
 */
BOOST_AUTO_TEST_CASE( CanReadUnpackedLibrary )
{
    PCB_IO_KICAD_SEXPR plugin;
    wxString           libPath = GetUnpackedTestLibPath();

    BOOST_TEST_MESSAGE( "Testing CanReadLibrary for: " << libPath );
    BOOST_CHECK( plugin.CanReadLibrary( libPath ) );
}


/**
 * Test that we can enumerate footprints in a packed library.
 */
BOOST_AUTO_TEST_CASE( EnumeratePackedLibrary )
{
    PCB_IO_KICAD_SEXPR plugin;
    wxString           libPath = GetPackedTestLibPath();
    wxArrayString      footprintNames;

    BOOST_TEST_MESSAGE( "Enumerating footprints in: " << libPath );

    plugin.FootprintEnumerate( footprintNames, libPath, false, nullptr );

    BOOST_TEST_MESSAGE( "Found " << footprintNames.size() << " footprints" );

    // Our test library has 2 footprints
    BOOST_CHECK_EQUAL( footprintNames.size(), 2 );

    // Check that expected footprints are present
    bool hasR0402 = std::find( footprintNames.begin(), footprintNames.end(),
                               "R_0402_1005Metric" ) != footprintNames.end();
    bool hasC0402 = std::find( footprintNames.begin(), footprintNames.end(),
                               "C_0402_1005Metric" ) != footprintNames.end();

    BOOST_CHECK( hasR0402 );
    BOOST_CHECK( hasC0402 );
}


/**
 * Test loading a footprint from a packed library.
 */
BOOST_AUTO_TEST_CASE( LoadFootprintFromPackedLibrary )
{
    PCB_IO_KICAD_SEXPR plugin;
    wxString           libPath = GetPackedTestLibPath();

    BOOST_TEST_MESSAGE( "Loading footprint R_0402_1005Metric from: " << libPath );

    FOOTPRINT* fp = plugin.FootprintLoad( libPath, "R_0402_1005Metric", false, nullptr );

    BOOST_REQUIRE( fp != nullptr );

    // Check basic properties
    BOOST_CHECK_EQUAL( fp->GetValue(), "R_0402_1005Metric" );
    BOOST_CHECK_EQUAL( fp->GetLibDescription(), "Resistor SMD 0402 (1005 Metric)" );
    BOOST_CHECK_EQUAL( fp->GetKeywords(), "resistor 0402" );

    // Check that it has expected pads
    BOOST_CHECK_EQUAL( fp->Pads().size(), 2 );

    delete fp;
}


/**
 * Test that we can create a new packed library.
 */
BOOST_AUTO_TEST_CASE( CreatePackedLibrary )
{
    TEMPORARY_DIRECTORY tempDir( "kicad_qa_create_packed_", "" );
    wxString            libPath = wxString( tempDir.GetPath().string() ) + "/test.kicad_fpl";

    PCB_IO_KICAD_SEXPR plugin;

    BOOST_TEST_MESSAGE( "Creating packed library: " << libPath );

    // Create the library
    plugin.CreateLibrary( libPath );

    // Verify it exists
    BOOST_CHECK( wxFileExists( libPath ) );

    // Verify we can read it
    BOOST_CHECK( plugin.CanReadLibrary( libPath ) );

    // Enumerate should return empty
    wxArrayString footprintNames;
    plugin.FootprintEnumerate( footprintNames, libPath, false, nullptr );
    BOOST_CHECK_EQUAL( footprintNames.size(), 0 );
}


/**
 * Test saving a footprint to a packed library.
 */
BOOST_AUTO_TEST_CASE( SaveFootprintToPackedLibrary )
{
    TEMPORARY_DIRECTORY tempDir( "kicad_qa_save_packed_", "" );
    wxString            libPath = wxString( tempDir.GetPath().string() ) + "/test.kicad_fpl";

    PCB_IO_KICAD_SEXPR plugin;

    // Create the library
    plugin.CreateLibrary( libPath );

    // Load a footprint from the unpacked library
    wxString   unpackedPath = GetUnpackedTestLibPath();
    FOOTPRINT* fp = plugin.FootprintLoad( unpackedPath, "R_0201_0603Metric", false, nullptr );

    BOOST_REQUIRE( fp != nullptr );

    BOOST_TEST_MESSAGE( "Saving footprint to packed library: " << libPath );

    // Save to packed library
    plugin.FootprintSave( libPath, fp );

    // Verify we can enumerate it
    wxArrayString footprintNames;
    plugin.FootprintEnumerate( footprintNames, libPath, false, nullptr );
    BOOST_CHECK_EQUAL( footprintNames.size(), 1 );

    // Reload and verify
    FOOTPRINT* reloaded = plugin.FootprintLoad( libPath, "R_0201_0603Metric", false, nullptr );

    BOOST_REQUIRE( reloaded != nullptr );

    // Use CheckFootprint for comprehensive comparison
    KI_TEST::CheckFootprint( fp, reloaded );

    delete fp;
    delete reloaded;
}


/**
 * Test deleting a footprint from a packed library.
 */
BOOST_AUTO_TEST_CASE( DeleteFootprintFromPackedLibrary )
{
    TEMPORARY_DIRECTORY tempDir( "kicad_qa_delete_packed_", "" );
    wxString            libPath = wxString( tempDir.GetPath().string() ) + "/test.kicad_fpl";

    PCB_IO_KICAD_SEXPR plugin;

    // Create the library and add a footprint
    plugin.CreateLibrary( libPath );

    wxString   unpackedPath = GetUnpackedTestLibPath();
    FOOTPRINT* fp = plugin.FootprintLoad( unpackedPath, "R_0201_0603Metric", false, nullptr );

    BOOST_REQUIRE( fp != nullptr );

    plugin.FootprintSave( libPath, fp );

    // Verify it's there
    wxArrayString footprintNames;
    plugin.FootprintEnumerate( footprintNames, libPath, false, nullptr );
    BOOST_CHECK_EQUAL( footprintNames.size(), 1 );

    BOOST_TEST_MESSAGE( "Deleting footprint from packed library" );

    // Delete the footprint
    plugin.FootprintDelete( libPath, "R_0201_0603Metric" );

    // Verify it's gone
    footprintNames.clear();
    plugin.FootprintEnumerate( footprintNames, libPath, false, nullptr );
    BOOST_CHECK_EQUAL( footprintNames.size(), 0 );

    delete fp;
}


/**
 * Test roundtrip: Load from packed, save to packed, reload and compare.
 */
BOOST_AUTO_TEST_CASE( PackedLibraryRoundtrip )
{
    TEMPORARY_DIRECTORY tempDir( "kicad_qa_packed_roundtrip_", "" );
    wxString            tempLibPath = wxString( tempDir.GetPath().string() ) + "/roundtrip.kicad_fpl";

    PCB_IO_KICAD_SEXPR plugin;

    // Load all footprints from original packed library
    wxString      origPath = GetPackedTestLibPath();
    wxArrayString origNames;
    plugin.FootprintEnumerate( origNames, origPath, false, nullptr );

    BOOST_TEST_MESSAGE( "Loading " << origNames.size() << " footprints from packed library" );

    std::vector<std::unique_ptr<FOOTPRINT>> origFootprints;

    for( const wxString& name : origNames )
    {
        FOOTPRINT* fp = plugin.FootprintLoad( origPath, name, false, nullptr );
        BOOST_REQUIRE( fp != nullptr );
        origFootprints.emplace_back( fp );
    }

    // Create new packed library and save all footprints
    plugin.CreateLibrary( tempLibPath );

    for( auto& fp : origFootprints )
    {
        plugin.FootprintSave( tempLibPath, fp.get() );
    }

    BOOST_TEST_MESSAGE( "Saved footprints to: " << tempLibPath );

    // Reload and compare
    wxArrayString reloadedNames;
    plugin.FootprintEnumerate( reloadedNames, tempLibPath, false, nullptr );
    BOOST_CHECK_EQUAL( reloadedNames.size(), origNames.size() );

    for( size_t i = 0; i < origFootprints.size(); ++i )
    {
        const wxString& name = origNames[i];

        BOOST_TEST_MESSAGE( "Comparing footprint: " << name );

        FOOTPRINT* reloaded = plugin.FootprintLoad( tempLibPath, name, false, nullptr );
        BOOST_REQUIRE( reloaded != nullptr );

        KI_TEST::CheckFootprint( origFootprints[i].get(), reloaded );

        delete reloaded;
    }
}


/**
 * Test conversion: Load from unpacked library, save to packed library.
 */
BOOST_AUTO_TEST_CASE( UnpackedToPackedConversion )
{
    TEMPORARY_DIRECTORY tempDir( "kicad_qa_unpacked_to_packed_", "" );
    wxString            packedPath = wxString( tempDir.GetPath().string() ) + "/converted.kicad_fpl";

    PCB_IO_KICAD_SEXPR plugin;

    // Load from unpacked library
    wxString   unpackedPath = GetUnpackedTestLibPath();
    FOOTPRINT* origFp = plugin.FootprintLoad( unpackedPath, "R_0201_0603Metric", false, nullptr );

    BOOST_REQUIRE( origFp != nullptr );

    BOOST_TEST_MESSAGE( "Converting unpacked footprint to packed library" );

    // Save to packed library
    plugin.CreateLibrary( packedPath );
    plugin.FootprintSave( packedPath, origFp );

    // Reload and compare
    FOOTPRINT* convertedFp = plugin.FootprintLoad( packedPath, "R_0201_0603Metric", false, nullptr );

    BOOST_REQUIRE( convertedFp != nullptr );

    KI_TEST::CheckFootprint( origFp, convertedFp );

    delete origFp;
    delete convertedFp;
}


/**
 * Test conversion: Load from packed library, save to unpacked library.
 */
BOOST_AUTO_TEST_CASE( PackedToUnpackedConversion )
{
    TEMPORARY_DIRECTORY tempDir( "kicad_qa_packed_to_unpacked_", ".pretty" );
    wxString            unpackedPath = wxString( tempDir.GetPath().string() );

    PCB_IO_KICAD_SEXPR plugin;

    // Load from packed library
    wxString   packedPath = GetPackedTestLibPath();
    FOOTPRINT* origFp = plugin.FootprintLoad( packedPath, "R_0402_1005Metric", false, nullptr );

    BOOST_REQUIRE( origFp != nullptr );

    BOOST_TEST_MESSAGE( "Converting packed footprint to unpacked library: " << unpackedPath );

    // The temp directory is already created with .pretty suffix
    // Save to unpacked library
    plugin.FootprintSave( unpackedPath, origFp );

    // Reload and compare
    FOOTPRINT* convertedFp = plugin.FootprintLoad( unpackedPath, "R_0402_1005Metric", false, nullptr );

    BOOST_REQUIRE( convertedFp != nullptr );

    KI_TEST::CheckFootprint( origFp, convertedFp );

    delete origFp;
    delete convertedFp;
}


/**
 * Test that multiple footprints in a packed library maintain their distinct properties.
 */
BOOST_AUTO_TEST_CASE( MultipleFootprintsIntegrity )
{
    PCB_IO_KICAD_SEXPR plugin;
    wxString           libPath = GetPackedTestLibPath();

    FOOTPRINT* fpR = plugin.FootprintLoad( libPath, "R_0402_1005Metric", false, nullptr );
    FOOTPRINT* fpC = plugin.FootprintLoad( libPath, "C_0402_1005Metric", false, nullptr );

    BOOST_REQUIRE( fpR != nullptr );
    BOOST_REQUIRE( fpC != nullptr );

    // Verify they have distinct properties
    BOOST_CHECK_EQUAL( fpR->GetValue(), "R_0402_1005Metric" );
    BOOST_CHECK_EQUAL( fpC->GetValue(), "C_0402_1005Metric" );

    BOOST_CHECK( fpR->GetLibDescription().Contains( "Resistor" ) );
    BOOST_CHECK( fpC->GetLibDescription().Contains( "Capacitor" ) );

    BOOST_CHECK( fpR->GetKeywords().Contains( "resistor" ) );
    BOOST_CHECK( fpC->GetKeywords().Contains( "capacitor" ) );

    delete fpR;
    delete fpC;
}


/**
 * Test deleting a packed library.
 */
BOOST_AUTO_TEST_CASE( DeletePackedLibrary )
{
    TEMPORARY_DIRECTORY tempDir( "kicad_qa_delete_lib_", "" );
    wxString            libPath = wxString( tempDir.GetPath().string() ) + "/todelete.kicad_fpl";

    PCB_IO_KICAD_SEXPR plugin;

    // Create the library
    plugin.CreateLibrary( libPath );
    BOOST_CHECK( wxFileExists( libPath ) );

    BOOST_TEST_MESSAGE( "Deleting packed library: " << libPath );

    // Delete the library
    plugin.DeleteLibrary( libPath );

    // Verify it's gone
    BOOST_CHECK( !wxFileExists( libPath ) );
}


/**
 * Test that loading a non-existent footprint from a packed library fails gracefully.
 */
BOOST_AUTO_TEST_CASE( LoadNonExistentFootprint )
{
    PCB_IO_KICAD_SEXPR plugin;
    wxString           libPath = GetPackedTestLibPath();

    FOOTPRINT* fp = plugin.FootprintLoad( libPath, "NonExistentFootprint", false, nullptr );

    BOOST_CHECK( fp == nullptr );
}


/**
 * Test that we can save multiple footprints to a packed library and enumerate them all.
 */
BOOST_AUTO_TEST_CASE( SaveMultipleFootprints )
{
    TEMPORARY_DIRECTORY tempDir( "kicad_qa_multi_save_", "" );
    wxString            libPath = wxString( tempDir.GetPath().string() ) + "/multi.kicad_fpl";

    PCB_IO_KICAD_SEXPR plugin;

    // Create library
    plugin.CreateLibrary( libPath );

    // Load footprints from both packed and unpacked sources
    wxString packedPath = GetPackedTestLibPath();
    wxString unpackedPath = GetUnpackedTestLibPath();

    FOOTPRINT* fp1 = plugin.FootprintLoad( packedPath, "R_0402_1005Metric", false, nullptr );
    FOOTPRINT* fp2 = plugin.FootprintLoad( packedPath, "C_0402_1005Metric", false, nullptr );
    FOOTPRINT* fp3 = plugin.FootprintLoad( unpackedPath, "R_0201_0603Metric", false, nullptr );

    BOOST_REQUIRE( fp1 != nullptr );
    BOOST_REQUIRE( fp2 != nullptr );
    BOOST_REQUIRE( fp3 != nullptr );

    // Save all to the new packed library
    plugin.FootprintSave( libPath, fp1 );
    plugin.FootprintSave( libPath, fp2 );
    plugin.FootprintSave( libPath, fp3 );

    // Enumerate and verify count
    wxArrayString names;
    plugin.FootprintEnumerate( names, libPath, false, nullptr );
    BOOST_CHECK_EQUAL( names.size(), 3 );

    // Verify all footprints are loadable
    for( const wxString& name : names )
    {
        FOOTPRINT* loaded = plugin.FootprintLoad( libPath, name, false, nullptr );
        BOOST_CHECK( loaded != nullptr );
        delete loaded;
    }

    delete fp1;
    delete fp2;
    delete fp3;
}


// ============================================================================
// PCB_IO_MGR Conversion Utility Tests
// ============================================================================


/**
 * Test the IsPackedLibrary() helper function.
 */
BOOST_AUTO_TEST_CASE( IsPackedLibraryHelper )
{
    // Packed library paths
    BOOST_CHECK( PCB_IO_MGR::IsPackedLibrary( "/path/to/library.kicad_fpl" ) );
    BOOST_CHECK( PCB_IO_MGR::IsPackedLibrary( "library.kicad_fpl" ) );
    BOOST_CHECK( PCB_IO_MGR::IsPackedLibrary( "/path/to/my.lib.kicad_fpl" ) );

    // Case insensitivity
    BOOST_CHECK( PCB_IO_MGR::IsPackedLibrary( "library.KICAD_FPL" ) );
    BOOST_CHECK( PCB_IO_MGR::IsPackedLibrary( "library.Kicad_Fpl" ) );

    // Unpacked library paths
    BOOST_CHECK( !PCB_IO_MGR::IsPackedLibrary( "/path/to/library.pretty" ) );
    BOOST_CHECK( !PCB_IO_MGR::IsPackedLibrary( "library.pretty" ) );
    BOOST_CHECK( !PCB_IO_MGR::IsPackedLibrary( "/path/to/footprint.kicad_mod" ) );

    // Edge cases
    BOOST_CHECK( !PCB_IO_MGR::IsPackedLibrary( "" ) );
    BOOST_CHECK( !PCB_IO_MGR::IsPackedLibrary( "/path/to/kicad_fpl" ) );  // No dot before ext
    BOOST_CHECK( !PCB_IO_MGR::IsPackedLibrary( "kicad_fpl" ) );  // Just the extension word
}


/**
 * Test converting an unpacked library to packed format using ConvertLibraryFormat().
 */
BOOST_AUTO_TEST_CASE( ConvertUnpackedToPackedUtility )
{
    TEMPORARY_DIRECTORY tempDir( "kicad_qa_convert_to_packed_", "" );
    wxString            packedPath = wxString( tempDir.GetPath().string() ) + "/converted.kicad_fpl";

    wxString unpackedPath = GetUnpackedTestLibPath();

    BOOST_TEST_MESSAGE( "Converting unpacked library to packed: " << unpackedPath << " -> " << packedPath );

    // Convert using the utility function
    bool result = PCB_IO_MGR::ConvertLibraryFormat( unpackedPath, packedPath, nullptr );

    BOOST_CHECK( result );
    BOOST_CHECK( wxFileExists( packedPath ) );

    // Verify the converted library
    PCB_IO_KICAD_SEXPR plugin;

    BOOST_CHECK( plugin.CanReadLibrary( packedPath ) );

    // Enumerate and verify
    wxArrayString origNames;
    wxArrayString convertedNames;

    plugin.FootprintEnumerate( origNames, unpackedPath, false, nullptr );
    plugin.FootprintEnumerate( convertedNames, packedPath, false, nullptr );

    BOOST_CHECK_EQUAL( convertedNames.size(), origNames.size() );

    // Compare each footprint
    for( const wxString& name : origNames )
    {
        FOOTPRINT* origFp = plugin.FootprintLoad( unpackedPath, name, false, nullptr );
        FOOTPRINT* convertedFp = plugin.FootprintLoad( packedPath, name, false, nullptr );

        BOOST_REQUIRE( origFp != nullptr );
        BOOST_REQUIRE( convertedFp != nullptr );

        KI_TEST::CheckFootprint( origFp, convertedFp );

        delete origFp;
        delete convertedFp;
    }
}


/**
 * Test converting a packed library to unpacked format using ConvertLibraryFormat().
 */
BOOST_AUTO_TEST_CASE( ConvertPackedToUnpackedUtility )
{
    TEMPORARY_DIRECTORY tempDir( "kicad_qa_convert_to_unpacked_", ".pretty" );
    wxString            unpackedPath = wxString( tempDir.GetPath().string() );

    wxString packedPath = GetPackedTestLibPath();

    BOOST_TEST_MESSAGE( "Converting packed library to unpacked: " << packedPath << " -> " << unpackedPath );

    // Convert using the utility function
    bool result = PCB_IO_MGR::ConvertLibraryFormat( packedPath, unpackedPath, nullptr );

    BOOST_CHECK( result );
    BOOST_CHECK( wxDirExists( unpackedPath ) );

    // Verify the converted library
    PCB_IO_KICAD_SEXPR plugin;

    BOOST_CHECK( plugin.CanReadLibrary( unpackedPath ) );

    // Enumerate and verify
    wxArrayString origNames;
    wxArrayString convertedNames;

    plugin.FootprintEnumerate( origNames, packedPath, false, nullptr );
    plugin.FootprintEnumerate( convertedNames, unpackedPath, false, nullptr );

    BOOST_CHECK_EQUAL( convertedNames.size(), origNames.size() );

    // Compare each footprint
    for( const wxString& name : origNames )
    {
        FOOTPRINT* origFp = plugin.FootprintLoad( packedPath, name, false, nullptr );
        FOOTPRINT* convertedFp = plugin.FootprintLoad( unpackedPath, name, false, nullptr );

        BOOST_REQUIRE( origFp != nullptr );
        BOOST_REQUIRE( convertedFp != nullptr );

        KI_TEST::CheckFootprint( origFp, convertedFp );

        delete origFp;
        delete convertedFp;
    }
}


/**
 * Test double conversion roundtrip: unpacked -> packed -> unpacked.
 */
BOOST_AUTO_TEST_CASE( DoubleConversionRoundtrip )
{
    TEMPORARY_DIRECTORY tempDir1( "kicad_qa_roundtrip1_", "" );
    TEMPORARY_DIRECTORY tempDir2( "kicad_qa_roundtrip2_", ".pretty" );

    wxString packedPath = wxString( tempDir1.GetPath().string() ) + "/intermediate.kicad_fpl";
    wxString finalUnpackedPath = wxString( tempDir2.GetPath().string() );

    wxString origUnpackedPath = GetUnpackedTestLibPath();

    BOOST_TEST_MESSAGE( "Double conversion roundtrip: unpacked -> packed -> unpacked" );

    // First conversion: unpacked -> packed
    bool result1 = PCB_IO_MGR::ConvertLibraryFormat( origUnpackedPath, packedPath, nullptr );

    BOOST_CHECK( result1 );
    BOOST_CHECK( wxFileExists( packedPath ) );

    // Second conversion: packed -> unpacked
    bool result2 = PCB_IO_MGR::ConvertLibraryFormat( packedPath, finalUnpackedPath, nullptr );

    BOOST_CHECK( result2 );
    BOOST_CHECK( wxDirExists( finalUnpackedPath ) );

    // Verify final library matches original
    PCB_IO_KICAD_SEXPR plugin;

    wxArrayString origNames;
    wxArrayString finalNames;

    plugin.FootprintEnumerate( origNames, origUnpackedPath, false, nullptr );
    plugin.FootprintEnumerate( finalNames, finalUnpackedPath, false, nullptr );

    BOOST_CHECK_EQUAL( finalNames.size(), origNames.size() );

    // Compare each footprint
    for( const wxString& name : origNames )
    {
        FOOTPRINT* origFp = plugin.FootprintLoad( origUnpackedPath, name, false, nullptr );
        FOOTPRINT* finalFp = plugin.FootprintLoad( finalUnpackedPath, name, false, nullptr );

        BOOST_REQUIRE( origFp != nullptr );
        BOOST_REQUIRE( finalFp != nullptr );

        BOOST_TEST_MESSAGE( "Verifying footprint after double conversion: " << name );
        KI_TEST::CheckFootprint( origFp, finalFp );

        delete origFp;
        delete finalFp;
    }
}


/**
 * Test that ConvertLibraryFormat() fails gracefully with invalid source.
 */
BOOST_AUTO_TEST_CASE( ConvertInvalidSource )
{
    TEMPORARY_DIRECTORY tempDir( "kicad_qa_convert_invalid_", "" );
    wxString            dstPath = wxString( tempDir.GetPath().string() ) + "/dst.kicad_fpl";

    // Try to convert a non-existent library
    bool result = PCB_IO_MGR::ConvertLibraryFormat( "/nonexistent/path.pretty", dstPath, nullptr );

    BOOST_CHECK( !result );
    BOOST_CHECK( !wxFileExists( dstPath ) );
}


BOOST_AUTO_TEST_SUITE_END()
