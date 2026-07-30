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

/**
 * @file
 * Test suite for embedded files in LIB_SYMBOL objects, including derived symbols.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

// Code under test
#include <lib_symbol.h>
#include <sch_io/kicad_sexpr/sch_io_kicad_sexpr.h>
#include <sch_io/sch_io_mgr.h>
#include <embedded_files.h>
#include <mmh3_hash.h>
#include <schematic.h>
#include <sch_screen.h>
#include <sch_sheet.h>
#include <sch_sheet_path.h>
#include <sch_symbol.h>
#include <sch_bitmap.h>
#include <project.h>
#include <lib_id.h>
#include <settings/settings_manager.h>

#include <wx/filename.h>
#include <wx/dir.h>
#include <wx/image.h>
#include <wx/mstream.h>

#include <memory>
#include <vector>

class SYMBOL_EMBEDDED_FILES_TEST_FIXTURE
{
public:
    SYMBOL_EMBEDDED_FILES_TEST_FIXTURE()
    {
        m_tempDir = wxFileName::CreateTempFileName( wxS( "kicad_test_" ) );
        wxRemoveFile( m_tempDir );
        wxFileName::Mkdir( m_tempDir );

        m_libPath = wxFileName( m_tempDir, wxS( "test_lib.kicad_sym" ) ).GetFullPath();
    }

    ~SYMBOL_EMBEDDED_FILES_TEST_FIXTURE()
    {
        // Clean up temporary directory
        if( wxFileName::DirExists( m_tempDir ) )
        {
            wxFileName::Rmdir( m_tempDir, wxPATH_RMDIR_RECURSIVE );
        }
    }

    wxString GetTempDir() const { return m_tempDir; }
    wxString GetLibPath() const { return m_libPath; }

    /**
     * Create a test embedded file with the given name and content.
     */
    EMBEDDED_FILES::EMBEDDED_FILE* CreateTestEmbeddedFile( const wxString& aName,
                                                           const std::string& aContent )
    {
        EMBEDDED_FILES::EMBEDDED_FILE* file = new EMBEDDED_FILES::EMBEDDED_FILE();
        file->name = aName;
        file->decompressedData.assign( aContent.begin(), aContent.end() );

        MMH3_HASH hash( EMBEDDED_FILES::Seed() );
        hash.add( file->decompressedData );
        file->data_hash = hash.digest().ToString();

        EMBEDDED_FILES::RETURN_CODE result = EMBEDDED_FILES::CompressAndEncode( *file );
        BOOST_REQUIRE( result == EMBEDDED_FILES::RETURN_CODE::OK );

        return file;
    }

private:
    wxString m_tempDir;
    wxString m_libPath;
};

// PNG image with metadata chunks.  Loading one copy from the original encoded bytes and another
// from decoded pixels creates equivalent images whose serialized PNG bytes are not identical.
static const std::vector<unsigned char> test_png_4tile = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00,
    0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x08, 0x06, 0x00, 0x00, 0x00, 0xC4, 0x0F, 0xBE, 0x8B, 0x00, 0x00, 0x00,
    0x04, 0x73, 0x42, 0x49, 0x54, 0x08, 0x08, 0x08, 0x08, 0x7C, 0x08, 0x64, 0x88, 0x00, 0x00, 0x00, 0x09, 0x70,
    0x48, 0x59, 0x73, 0x00, 0x00, 0x0E, 0xC4, 0x00, 0x00, 0x0E, 0xC4, 0x01, 0x95, 0x2B, 0x0E, 0x1B, 0x00, 0x00,
    0x00, 0x19, 0x74, 0x45, 0x58, 0x74, 0x53, 0x6F, 0x66, 0x74, 0x77, 0x61, 0x72, 0x65, 0x00, 0x77, 0x77, 0x77,
    0x2E, 0x69, 0x6E, 0x6B, 0x73, 0x63, 0x61, 0x70, 0x65, 0x2E, 0x6F, 0x72, 0x67, 0x9B, 0xEE, 0x3C, 0x1A, 0x00,
    0x00, 0x00, 0x40, 0x49, 0x44, 0x41, 0x54, 0x18, 0x95, 0xA5, 0xCD, 0x4B, 0x0A, 0xC0, 0x20, 0x14, 0x43, 0xD1,
    0x63, 0x77, 0xA7, 0xDD, 0xFF, 0x06, 0xD4, 0x7D, 0x3C, 0x07, 0x95, 0x42, 0x3F, 0xE0, 0xC0, 0x84, 0x0C, 0x02,
    0xE1, 0x86, 0x78, 0x99, 0x13, 0x1D, 0x0D, 0xC5, 0xCF, 0xA0, 0x21, 0x66, 0xEA, 0x61, 0xA9, 0x2F, 0xA1, 0x4C,
    0x4A, 0x45, 0x4E, 0x71, 0xA1, 0x6E, 0xA5, 0x67, 0xB5, 0xBC, 0xD8, 0x1F, 0x0C, 0xA7, 0xFD, 0x23, 0x67, 0x70,
    0xDA, 0x89, 0xA2, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
};


static wxMemoryBuffer testPngBuffer()
{
    wxMemoryBuffer buffer;
    buffer.AppendData( test_png_4tile.data(), test_png_4tile.size() );
    return buffer;
}


static std::unique_ptr<SCH_BITMAP> createSymbolBitmap( bool aPreserveEncodedBytes, bool aAlterFirstPixel = false )
{
    auto bitmap = std::make_unique<SCH_BITMAP>();

    if( aPreserveEncodedBytes )
    {
        wxMemoryBuffer buffer = testPngBuffer();
        BOOST_REQUIRE( bitmap->GetReferenceImage().ReadImageFile( buffer ) );
    }
    else
    {
        wxMemoryInputStream stream( test_png_4tile.data(), test_png_4tile.size() );
        wxImage             image;

        BOOST_REQUIRE( image.LoadFile( stream, wxBITMAP_TYPE_PNG ) );

        if( aAlterFirstPixel )
        {
            BOOST_REQUIRE( image.GetData() );
            image.GetData()[0] = static_cast<unsigned char>( image.GetData()[0] ^ 0xff );
        }

        BOOST_REQUIRE( bitmap->GetReferenceImage().SetImage( image ) );
    }

    return bitmap;
}


static std::unique_ptr<LIB_SYMBOL> createImageSymbol( bool aPreserveEncodedBytes, bool aAlterFirstPixel = false )
{
    auto symbol = std::make_unique<LIB_SYMBOL>( wxS( "test_img" ) );
    symbol->SetLibId( LIB_ID( wxS( "test_lib" ), wxS( "test_img" ) ) );
    symbol->GetValueField().SetText( wxS( "test_img" ) );
    symbol->GetReferenceField().SetText( wxS( "U" ) );
    symbol->AddDrawItem( createSymbolBitmap( aPreserveEncodedBytes, aAlterFirstPixel ).release() );
    return symbol;
}


static const SCH_BITMAP* onlySymbolBitmap( const LIB_SYMBOL& aSymbol )
{
    const SCH_BITMAP* bitmap = nullptr;
    int               bitmapCount = 0;

    for( const SCH_ITEM& item : aSymbol.GetDrawItems() )
    {
        if( item.Type() == SCH_BITMAP_T )
        {
            bitmap = static_cast<const SCH_BITMAP*>( &item );
            bitmapCount++;
        }
    }

    BOOST_REQUIRE_EQUAL( bitmapCount, 1 );
    BOOST_REQUIRE( bitmap );

    return bitmap;
}


static void checkSingleCanonicalCachedImage( const SCH_SCREEN& aScreen, const LIB_SYMBOL& aExpectedSymbol )
{
    const wxString cacheName = wxS( "test_lib:test_img" );
    const auto&    libSymbols = aScreen.GetLibSymbols();

    BOOST_REQUIRE_EQUAL( libSymbols.size(), 1 );

    auto cacheIt = libSymbols.find( cacheName );

    BOOST_REQUIRE( cacheIt != libSymbols.end() );
    BOOST_REQUIRE( cacheIt->second );

    const SCH_BITMAP* expectedBitmap = onlySymbolBitmap( aExpectedSymbol );
    const SCH_BITMAP* cachedBitmap = onlySymbolBitmap( *cacheIt->second );

    // Equivalent of the old COMPARE_FLAGS::EQUALITY: unit, position and content, but not UUID.
    BOOST_CHECK_EQUAL(
            cachedBitmap->compare( *expectedBitmap, SCH_ITEM::COMPARE_FLAGS::UNIT | SCH_ITEM::COMPARE_FLAGS::POSITION ),
            0 );
}


static void checkPlacedSymbolsUseLibraryIdLookup( const SCH_SCREEN& aScreen, int aExpectedCount )
{
    int symbolCount = 0;

    for( const SCH_ITEM* item : aScreen.Items().OfType( SCH_SYMBOL_T ) )
    {
        const SCH_SYMBOL* symbol = static_cast<const SCH_SYMBOL*>( item );

        BOOST_CHECK( symbol->UseLibIdLookup() );
        symbolCount++;
    }

    BOOST_CHECK_EQUAL( symbolCount, aExpectedCount );
}


BOOST_FIXTURE_TEST_SUITE( SymbolEmbeddedFiles, SYMBOL_EMBEDDED_FILES_TEST_FIXTURE )


/**
 * Test embedded files in parent and derived symbols:
 * 1) Create a symbol and add an embedded file and save.
 * 2) Close and reload the symbol to verify that the embedded file still exists.
 * 3) Create a derived symbol based on the first symbol and save.
 * 4) Close and re-open the derived symbol. Ensure that the embedded file does not exist
 *    in the derived symbol.
 * 5) Embed a new file in the derived symbol.
 * 6) Close and reopen the derived symbol. Ensure that the new embedded file exists.
 * 7) Add only the derived symbol to a new schematic sheet. Ensure that both embedded
 *    files are present in the schematic.
 */
BOOST_AUTO_TEST_CASE( DerivedSymbolEmbeddedFiles )
{
    // Step 1: Create a parent symbol with an embedded file and save
    std::unique_ptr<LIB_SYMBOL> parentSymbol = std::make_unique<LIB_SYMBOL>( wxS( "ParentSymbol" ) );
    parentSymbol->GetValueField().SetText( wxS( "ParentSymbol" ) );
    parentSymbol->GetReferenceField().SetText( wxS( "U" ) );

    // Add an embedded file to the parent
    EMBEDDED_FILES::EMBEDDED_FILE* parentFile =
        CreateTestEmbeddedFile( wxS( "parent_datasheet.pdf" ), "Parent datasheet content" );
    parentSymbol->AddFile( parentFile );

    BOOST_CHECK( parentSymbol->HasFile( wxS( "parent_datasheet.pdf" ) ) );
    BOOST_CHECK_EQUAL( parentSymbol->EmbeddedFileMap().size(), 1 );

    // Save the parent symbol to the library
    {
        IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_KICAD ) );
        plugin->CreateLibrary( GetLibPath() );
        plugin->SaveSymbol( GetLibPath(), new LIB_SYMBOL( *parentSymbol ) );
        plugin->SaveLibrary( GetLibPath() );
    }

    // Step 2: Close and reload the symbol to verify that the embedded file still exists.
    {
        IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_KICAD ) );
        LIB_SYMBOL* loadedParent = plugin->LoadSymbol( GetLibPath(), wxS( "ParentSymbol" ) );
        BOOST_REQUIRE( loadedParent );
        BOOST_CHECK( loadedParent->HasFile( wxS( "parent_datasheet.pdf" ) ) );
    }

    // Step 3: Create a derived symbol based on the first symbol and save
    {
        IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_KICAD ) );
        LIB_SYMBOL* loadedParent = plugin->LoadSymbol( GetLibPath(), wxS( "ParentSymbol" ) );
        BOOST_REQUIRE( loadedParent );

        std::unique_ptr<LIB_SYMBOL> derivedSymbol = std::make_unique<LIB_SYMBOL>( wxS( "DerivedSymbol" ) );
        derivedSymbol->GetValueField().SetText( wxS( "DerivedSymbol" ) );
        derivedSymbol->SetParent( loadedParent );

        plugin->SaveSymbol( GetLibPath(), new LIB_SYMBOL( *derivedSymbol ) );
        plugin->SaveLibrary( GetLibPath() );
    }

    // Step 4: Close and re-open the derived symbol. Ensure that the embedded file does not exist in the derived symbol.
    {
        IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_KICAD ) );
        LIB_SYMBOL* loadedDerived = plugin->LoadSymbol( GetLibPath(), wxS( "DerivedSymbol" ) );
        BOOST_REQUIRE( loadedDerived );

        // The derived symbol itself should not have the file in its map
        BOOST_CHECK( !loadedDerived->HasFile( wxS( "parent_datasheet.pdf" ) ) );
        BOOST_CHECK_EQUAL( loadedDerived->EmbeddedFileMap().size(), 0 );
    }

    // Step 5: Embed a new file in the derived symbol
    {
        IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_KICAD ) );
        // We need to load parent to set it again, because LoadSymbol doesn't automatically link parents from disk without a library table
        LIB_SYMBOL* loadedParent = plugin->LoadSymbol( GetLibPath(), wxS( "ParentSymbol" ) );
        LIB_SYMBOL* loadedDerived = plugin->LoadSymbol( GetLibPath(), wxS( "DerivedSymbol" ) );
        BOOST_REQUIRE( loadedDerived );
        loadedDerived->SetParent( loadedParent );

        EMBEDDED_FILES::EMBEDDED_FILE* derivedFile =
            CreateTestEmbeddedFile( wxS( "derived_datasheet.pdf" ), "Derived datasheet content" );
        loadedDerived->AddFile( derivedFile );

        BOOST_CHECK( loadedDerived->HasFile( wxS( "derived_datasheet.pdf" ) ) );

        plugin->SaveSymbol( GetLibPath(), new LIB_SYMBOL( *loadedDerived ) );
        plugin->SaveLibrary( GetLibPath() );
    }

    // Step 6: Close and reopen the derived symbol. Ensure that the new embedded file exists
    {
        IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_KICAD ) );
        LIB_SYMBOL* loadedDerived = plugin->LoadSymbol( GetLibPath(), wxS( "DerivedSymbol" ) );
        BOOST_REQUIRE( loadedDerived );
        BOOST_CHECK( loadedDerived->HasFile( wxS( "derived_datasheet.pdf" ) ) );
    }

    // Step 7: Add only the derived symbol to a new schematic sheet. Ensure that both embedded files are present in the schematic.
    {
        SCHEMATIC schematic( nullptr );

        schematic.Reset();
        SCH_SHEET* defaultSheet = schematic.GetTopLevelSheet( 0 );

        SCH_SHEET* rootSheet = new SCH_SHEET( &schematic );
        SCH_SCREEN* screen = new SCH_SCREEN( &schematic );
        rootSheet->SetScreen( screen );

        schematic.AddTopLevelSheet( rootSheet );
        schematic.RemoveTopLevelSheet( defaultSheet );
        delete defaultSheet;

        IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_KICAD ) );
        LIB_SYMBOL* loadedParent = plugin->LoadSymbol( GetLibPath(), wxS( "ParentSymbol" ) );
        LIB_SYMBOL* loadedDerived = plugin->LoadSymbol( GetLibPath(), wxS( "DerivedSymbol" ) );

        loadedDerived->SetParent( loadedParent );

        SCH_SHEET_PATH rootPath;
        rootPath.push_back( &schematic.Root() );
        rootPath.push_back( rootSheet );

        SCH_SYMBOL* schSymbol = new SCH_SYMBOL( *loadedDerived, LIB_ID( "test_lib", "DerivedSymbol" ),
                                                &rootPath, 0 );
        screen->Append( schSymbol );

        // Verify derived file
        BOOST_CHECK( schSymbol->GetLibSymbolRef()->HasFile( wxS( "derived_datasheet.pdf" ) ) );

        // Verify parent file (should be flattened into the symbol)
        BOOST_CHECK( schSymbol->GetLibSymbolRef()->HasFile( wxS( "parent_datasheet.pdf" ) ) );
    }
}


/**
 * Test that copying a symbol with embedded files preserves the files.
 */
BOOST_AUTO_TEST_CASE( CopySymbolPreservesEmbeddedFiles )
{
    // Create a symbol with an embedded file
    std::unique_ptr<LIB_SYMBOL> original = std::make_unique<LIB_SYMBOL>( wxS( "Original" ) );
    original->GetValueField().SetText( wxS( "Original" ) );

    EMBEDDED_FILES::EMBEDDED_FILE* file =
        CreateTestEmbeddedFile( wxS( "test.pdf" ), "Test content" );
    original->AddFile( file );

    BOOST_CHECK_EQUAL( original->EmbeddedFileMap().size(), 1 );

    // Copy the symbol using copy constructor
    std::unique_ptr<LIB_SYMBOL> copy = std::make_unique<LIB_SYMBOL>( *original );

    // Verify the copy has the embedded file
    BOOST_CHECK_EQUAL( copy->EmbeddedFileMap().size(), 1 );
    BOOST_CHECK( copy->HasFile( wxS( "test.pdf" ) ) );

    EMBEDDED_FILES::EMBEDDED_FILE* copiedFile = copy->GetEmbeddedFile( wxS( "test.pdf" ) );
    BOOST_REQUIRE( copiedFile );
    BOOST_CHECK( copiedFile->Validate() );
}


/**
 * Test that assignment operator preserves embedded files.
 */
BOOST_AUTO_TEST_CASE( AssignmentPreservesEmbeddedFiles )
{
    // Create a symbol with an embedded file
    std::unique_ptr<LIB_SYMBOL> source = std::make_unique<LIB_SYMBOL>( wxS( "Source" ) );
    source->GetValueField().SetText( wxS( "Source" ) );

    EMBEDDED_FILES::EMBEDDED_FILE* file =
        CreateTestEmbeddedFile( wxS( "source.pdf" ), "Source content" );
    source->AddFile( file );

    // Create a destination symbol
    std::unique_ptr<LIB_SYMBOL> dest = std::make_unique<LIB_SYMBOL>( wxS( "Dest" ) );
    dest->GetValueField().SetText( wxS( "Dest" ) );

    // Assign source to destination
    *dest = *source;

    // Verify destination has the embedded file
    BOOST_CHECK_EQUAL( dest->EmbeddedFileMap().size(), 1 );
    BOOST_CHECK( dest->HasFile( wxS( "source.pdf" ) ) );

    EMBEDDED_FILES::EMBEDDED_FILE* destFile = dest->GetEmbeddedFile( wxS( "source.pdf" ) );
    BOOST_REQUIRE( destFile );
    BOOST_CHECK( destFile->Validate() );
}


/**
 * Adding a new embedded file to a placed schematic symbol must not drop the existing files.
 * The schematic serializes the screen's cached library symbols, while the symbol properties
 * dialog edits the placed instance's library symbol.  SCH_SCREEN::Append must reconcile the
 * cache with the instance even when only the embedded files changed (LIB_SYMBOL::Compare
 * ignores them); without it the edits are lost on save.
 */
BOOST_AUTO_TEST_CASE( AddEmbeddedFileToPlacedSymbolSurvivesSave )
{
    SETTINGS_MANAGER settingsManager;
    wxString         projectPath = wxFileName( GetTempDir(), wxS( "test_project.kicad_pro" ) ).GetFullPath();
    settingsManager.LoadProject( projectPath.ToStdString() );

    auto schematic = std::make_unique<SCHEMATIC>( nullptr );
    schematic->SetProject( &settingsManager.Prj() );
    schematic->Reset();

    SCH_SHEET*  defaultSheet = schematic->GetTopLevelSheet( 0 );
    SCH_SHEET*  rootSheet = new SCH_SHEET( schematic.get() );
    SCH_SCREEN* screen = new SCH_SCREEN( schematic.get() );
    rootSheet->SetScreen( screen );

    schematic->AddTopLevelSheet( rootSheet );
    schematic->RemoveTopLevelSheet( defaultSheet );
    delete defaultSheet;

    // Build a library symbol carrying one embedded file and place it on the sheet.
    std::unique_ptr<LIB_SYMBOL> libSymbol = std::make_unique<LIB_SYMBOL>( wxS( "EmbeddedSymbol" ) );
    libSymbol->GetValueField().SetText( wxS( "EmbeddedSymbol" ) );
    libSymbol->GetReferenceField().SetText( wxS( "U" ) );
    libSymbol->AddFile( CreateTestEmbeddedFile( wxS( "datasheet.pdf" ), "Original datasheet" ) );

    SCH_SHEET_PATH rootPath;
    rootPath.push_back( &schematic->Root() );
    rootPath.push_back( rootSheet );

    // Two instances share one cached library symbol.  This is the case that exposes the bug:
    // removing one symbol keeps the cache entry alive, so Append must reconcile the cache
    // through its "unchanged" branch (which is what LIB_SYMBOL::Compare reports for an
    // embedded-file-only edit) rather than rebuilding it from scratch.
    SCH_SYMBOL* schSymbol =
            new SCH_SYMBOL( *libSymbol, LIB_ID( "test_lib", "EmbeddedSymbol" ), &rootPath, 0 );
    schSymbol->SetRef( &rootPath, wxS( "U1" ) );
    screen->Append( schSymbol );

    SCH_SYMBOL* otherSymbol =
            new SCH_SYMBOL( *libSymbol, LIB_ID( "test_lib", "EmbeddedSymbol" ), &rootPath, 0 );
    otherSymbol->SetRef( &rootPath, wxS( "U2" ) );
    otherSymbol->Move( VECTOR2I( 2540000, 0 ) );
    screen->Append( otherSymbol );

    // Adding a second file through the properties dialog only touches the placed instance's
    // library symbol, mirroring what PANEL_EMBEDDED_FILES does.
    schSymbol->GetEmbeddedFiles()->AddFile( CreateTestEmbeddedFile( wxS( "extra.pdf" ), "Newly added" ) );

    BOOST_CHECK( schSymbol->GetEmbeddedFiles()->HasFile( wxS( "datasheet.pdf" ) ) );
    BOOST_CHECK( schSymbol->GetEmbeddedFiles()->HasFile( wxS( "extra.pdf" ) ) );

    // Re-add the symbol the same way the properties dialog does on OK; Append must push the
    // instance's embedded files into the cache that is serialized.
    BOOST_REQUIRE( screen->Remove( schSymbol ) );
    screen->Append( schSymbol );

    wxString fileName = wxFileName( GetTempDir(), wxS( "embedded_roundtrip.kicad_sch" ) ).GetFullPath();

    SCH_IO_KICAD_SEXPR io;
    io.SaveSchematicFile( fileName, rootSheet, schematic.get() );
    BOOST_REQUIRE( wxFileExists( fileName ) );

    schematic->Reset();
    SCH_SHEET* newDefaultSheet = schematic->GetTopLevelSheet( 0 );
    SCH_SHEET* loadedSheet = io.LoadSchematicFile( fileName, schematic.get() );
    BOOST_REQUIRE( loadedSheet );

    schematic->AddTopLevelSheet( loadedSheet );
    schematic->RemoveTopLevelSheet( newDefaultSheet );
    delete newDefaultSheet;

    SCH_SCREEN* loadedScreen = loadedSheet->GetScreen();
    BOOST_REQUIRE( loadedScreen );

    bool foundSymbol = false;

    for( const auto& [name, cachedSymbol] : loadedScreen->GetLibSymbols() )
    {
        if( cachedSymbol->GetName() == wxS( "EmbeddedSymbol" ) )
        {
            foundSymbol = true;
            BOOST_CHECK( cachedSymbol->HasFile( wxS( "datasheet.pdf" ) ) );
            BOOST_CHECK( cachedSymbol->HasFile( wxS( "extra.pdf" ) ) );
        }
    }

    BOOST_CHECK( foundSymbol );
}


/**
 * Placing the same symbol image more than once must not create duplicate schematic-cache
 * lib_symbols just because one in-memory image still has original PNG bytes and another has
 * the same decoded pixels that would be re-encoded on save.
 */
BOOST_AUTO_TEST_CASE( EquivalentSymbolImagesDoNotCreateCacheVariants )
{
    SCHEMATIC schematic( nullptr );

    schematic.Reset();
    SCH_SHEET* defaultSheet = schematic.GetTopLevelSheet( 0 );

    SCH_SHEET*  rootSheet = new SCH_SHEET( &schematic );
    SCH_SCREEN* screen = new SCH_SCREEN( &schematic );
    rootSheet->SetScreen( screen );

    schematic.AddTopLevelSheet( rootSheet );
    schematic.RemoveTopLevelSheet( defaultSheet );
    delete defaultSheet;

    SCH_SHEET_PATH rootPath;
    rootPath.push_back( &schematic.Root() );
    rootPath.push_back( rootSheet );

    std::unique_ptr<LIB_SYMBOL> originalEncodedSymbol = createImageSymbol( true );
    std::unique_ptr<LIB_SYMBOL> decodedOnlySymbol = createImageSymbol( false );

    struct PLACEMENT
    {
        LIB_SYMBOL* m_source;
        int         m_orientation;
    };

    std::vector<PLACEMENT> placements = { { originalEncodedSymbol.get(), SYM_ORIENT_0 },
                                          { decodedOnlySymbol.get(), SYM_ORIENT_90 },
                                          { decodedOnlySymbol.get(), SYM_ORIENT_0 + SYM_MIRROR_X },
                                          { decodedOnlySymbol.get(), SYM_ORIENT_90 + SYM_MIRROR_X },
                                          { decodedOnlySymbol.get(), SYM_ORIENT_270 + SYM_MIRROR_X },
                                          { decodedOnlySymbol.get(), SYM_ORIENT_0 + SYM_MIRROR_Y } };

    for( size_t i = 0; i < placements.size(); ++i )
    {
        SCH_SYMBOL* symbol = new SCH_SYMBOL( *placements[i].m_source, LIB_ID( "test_lib", "test_img" ), &rootPath, 0 );
        symbol->SetRef( &rootPath, wxString::Format( wxS( "U%d" ), static_cast<int>( i + 1 ) ) );
        symbol->SetOrientation( placements[i].m_orientation );
        symbol->Move( VECTOR2I( schIUScale.MilsToIU( static_cast<int>( i ) * 1000 ), 0 ) );
        screen->Append( symbol );
    }

    BOOST_CHECK_EQUAL( screen->GetLibSymbols().size(), 1 );
    BOOST_CHECK( screen->GetLibSymbols().find( wxS( "test_lib:test_img" ) ) != screen->GetLibSymbols().end() );
    BOOST_CHECK( screen->GetLibSymbols().find( wxS( "test_img_1" ) ) == screen->GetLibSymbols().end() );
}


BOOST_AUTO_TEST_CASE( TransformedSymbolImagesReuseCache )
{
    SETTINGS_MANAGER settingsManager;
    wxString         projectPath = wxFileName( GetTempDir(), wxS( "test_project.kicad_pro" ) ).GetFullPath();
    settingsManager.LoadProject( projectPath.ToStdString() );

    auto schematic = std::make_unique<SCHEMATIC>( nullptr );
    schematic->SetProject( &settingsManager.Prj() );
    schematic->Reset();

    SCH_SHEET*  defaultSheet = schematic->GetTopLevelSheet( 0 );
    SCH_SHEET*  rootSheet = new SCH_SHEET( schematic.get() );
    SCH_SCREEN* screen = new SCH_SCREEN( schematic.get() );
    rootSheet->SetScreen( screen );

    schematic->AddTopLevelSheet( rootSheet );
    schematic->RemoveTopLevelSheet( defaultSheet );
    delete defaultSheet;

    SCH_SHEET_PATH rootPath;
    rootPath.push_back( &schematic->Root() );
    rootPath.push_back( rootSheet );

    std::unique_ptr<LIB_SYMBOL> originalEncodedSymbol = createImageSymbol( true );
    std::unique_ptr<LIB_SYMBOL> decodedOnlySymbol = createImageSymbol( false );

    std::vector<int> orientations = { SYM_ORIENT_0,
                                      SYM_ORIENT_90,
                                      SYM_ORIENT_180,
                                      SYM_ORIENT_270,
                                      SYM_ORIENT_0 + SYM_MIRROR_X,
                                      SYM_ORIENT_0 + SYM_MIRROR_Y,
                                      SYM_ORIENT_90 + SYM_MIRROR_X,
                                      SYM_ORIENT_90 + SYM_MIRROR_Y,
                                      SYM_ORIENT_180 + SYM_MIRROR_X,
                                      SYM_ORIENT_180 + SYM_MIRROR_Y,
                                      SYM_ORIENT_270 + SYM_MIRROR_X,
                                      SYM_ORIENT_270 + SYM_MIRROR_Y };

    SCH_SYMBOL* sourceSymbol = nullptr;

    for( size_t i = 0; i < orientations.size(); ++i )
    {
        SCH_SYMBOL* symbol = nullptr;

        if( i == 0 )
        {
            symbol = new SCH_SYMBOL( *originalEncodedSymbol, LIB_ID( "test_lib", "test_img" ), &rootPath, 0 );
            sourceSymbol = symbol;
        }
        else if( i % 3 == 0 )
        {
            symbol = static_cast<SCH_SYMBOL*>( sourceSymbol->Duplicate( IGNORE_PARENT_GROUP ) );
        }
        else
        {
            LIB_SYMBOL* source = ( i % 2 == 0 ) ? decodedOnlySymbol.get() : originalEncodedSymbol.get();

            symbol = new SCH_SYMBOL( *source, LIB_ID( "test_lib", "test_img" ), &rootPath, 0 );
        }

        symbol->SetRef( &rootPath, wxString::Format( wxS( "U%d" ), static_cast<int>( i + 1 ) ) );
        symbol->SetOrientation( orientations[i] );
        symbol->Move( VECTOR2I( schIUScale.MilsToIU( static_cast<int>( i % 4 ) * 1000 ),
                                schIUScale.MilsToIU( static_cast<int>( i / 4 ) * 1000 ) ) );
        screen->Append( symbol );
    }

    checkSingleCanonicalCachedImage( *screen, *originalEncodedSymbol );
    checkPlacedSymbolsUseLibraryIdLookup( *screen, static_cast<int>( orientations.size() ) );

    wxString fileName = wxFileName( GetTempDir(), wxS( "transformed_images.kicad_sch" ) ).GetFullPath();

    SCH_IO_KICAD_SEXPR io;
    io.SaveSchematicFile( fileName, rootSheet, schematic.get() );
    BOOST_REQUIRE( wxFileExists( fileName ) );

    auto reloaded = std::make_unique<SCHEMATIC>( nullptr );
    reloaded->SetProject( &settingsManager.Prj() );
    reloaded->Reset();

    SCH_SHEET* reloadedDefaultSheet = reloaded->GetTopLevelSheet( 0 );
    SCH_SHEET* loadedSheet = io.LoadSchematicFile( fileName, reloaded.get() );
    BOOST_REQUIRE( loadedSheet );

    reloaded->AddTopLevelSheet( loadedSheet );
    reloaded->RemoveTopLevelSheet( reloadedDefaultSheet );
    delete reloadedDefaultSheet;

    SCH_SCREEN* loadedScreen = loadedSheet->GetScreen();
    BOOST_REQUIRE( loadedScreen );

    checkSingleCanonicalCachedImage( *loadedScreen, *originalEncodedSymbol );
    checkPlacedSymbolsUseLibraryIdLookup( *loadedScreen, static_cast<int>( orientations.size() ) );
}


BOOST_AUTO_TEST_CASE( DifferentOverlaidSymbolImagesCompareDifferent )
{
    std::unique_ptr<LIB_SYMBOL> baseline = createImageSymbol( false );
    std::unique_ptr<LIB_SYMBOL> withExtraImage = createImageSymbol( false );

    withExtraImage->AddDrawItem( createSymbolBitmap( false, true ).release() );

    BOOST_CHECK( withExtraImage->Compare( *baseline ) != 0 );
    BOOST_CHECK( baseline->Compare( *withExtraImage ) != 0 );
}


BOOST_AUTO_TEST_CASE( SymbolImageUnitBodyStyleRoundTrip )
{
    std::unique_ptr<LIB_SYMBOL> symbol = std::make_unique<LIB_SYMBOL>( wxS( "ScopedImage" ) );
    symbol->GetValueField().SetText( wxS( "ScopedImage" ) );
    symbol->GetReferenceField().SetText( wxS( "U" ) );
    symbol->SetUnitCount( 2, false );
    symbol->SetHasDeMorganBodyStyles( true );

    std::unique_ptr<SCH_BITMAP> bitmap = createSymbolBitmap( false );
    bitmap->SetUnit( 2 );
    bitmap->SetBodyStyle( 2 );
    bitmap->SetPosition( VECTOR2I( schIUScale.MilsToIU( 100 ), schIUScale.MilsToIU( 200 ) ) );
    symbol->AddDrawItem( bitmap.release() );

    {
        IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_KICAD ) );
        plugin->CreateLibrary( GetLibPath() );
        plugin->SaveSymbol( GetLibPath(), new LIB_SYMBOL( *symbol ) );
        plugin->SaveLibrary( GetLibPath() );
    }

    IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_KICAD ) );
    LIB_SYMBOL*         loadedSymbol = plugin->LoadSymbol( GetLibPath(), wxS( "ScopedImage" ) );

    BOOST_REQUIRE( loadedSymbol );

    const SCH_BITMAP* loadedBitmap = nullptr;
    int               bitmapCount = 0;

    for( const SCH_ITEM& item : loadedSymbol->GetDrawItems() )
    {
        if( item.Type() == SCH_BITMAP_T )
        {
            loadedBitmap = static_cast<const SCH_BITMAP*>( &item );
            bitmapCount++;
        }
    }

    BOOST_REQUIRE_EQUAL( bitmapCount, 1 );
    BOOST_REQUIRE( loadedBitmap );
    BOOST_CHECK_EQUAL( loadedBitmap->GetUnit(), 2 );
    BOOST_CHECK_EQUAL( loadedBitmap->GetBodyStyle(), 2 );
}


BOOST_AUTO_TEST_SUITE_END()
