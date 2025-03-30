#include <ki_exception.h>
#include <pcbnew/pcb_io/allegro/pcb_io_allegro.h>
#include <pcbnew/pcb_io/kicad_sexpr/pcb_io_kicad_sexpr.h>
#include <pcbnew_utils/board_file_utils.h>
#include <pcbnew_utils/board_test_utils.h>
#include <qa_utils/wx_utils/unit_test_utils.h>

#include <board.h>

struct ALLEGRO_BRD_IMPORT_FIXTURE
{
    ALLEGRO_BRD_IMPORT_FIXTURE() = default;

    PCB_IO_ALLEGRO     allegroPlugin;
    PCB_IO_KICAD_SEXPR kicadPlugin;
};

BOOST_FIXTURE_TEST_SUITE( AllegroBrdImport, ALLEGRO_BRD_IMPORT_FIXTURE )

BOOST_AUTO_TEST_CASE( AllegroBrdImport )
{
    std::vector<wxString> tests = {
        "AVALON.brd",
        "BeagleBone-AI.brd",
        "BeagleY-AI_PCB_V1.0_240105.brd",
        "MAINBOARD_02_20181023.brd",
        "OpenCellular_Connect-1_GBC_Life-3_Board_v1.1.brd",
    };

    std::string dataPath = KI_TEST::GetPcbnewTestDataDir() + "plugins/allegro/";

    for( const wxString& brdName : tests )
    {
        wxString allegroBrdPath = dataPath + brdName;
        BOOST_TEST_CONTEXT( "Filename: " << allegroBrdPath )
        {
            // Parsing the files should not result in an exception
            BOOST_CHECK_NO_THROW( do {
                BOARD* brd = allegroPlugin.LoadBoard( allegroBrdPath, nullptr );
                BOOST_CHECK_NE( brd->GetCopperLayerCount(), 0 );
            } while( 0 ) );
        }
    }
};

BOOST_AUTO_TEST_CASE( AllegroInvalidBrdImport )
{
    std::vector<std::pair<std::string, std::string>> tests = {
        { "Invalid_Magic.brd", "Board file magic=0x00149999 not recognized." },
        { "Invalid_Units.brd", "Units 0x04 not recognized." },
        { "AVALON_corrupted.brd", "Do not have parser for t=0xFF000000 available." },
        { "file_missing.brd", "Failed to open file." },
    };

    std::string dataPath = KI_TEST::GetPcbnewTestDataDir() + "plugins/allegro/";

    for( auto& [brdName, expectedMessage] : tests )
    {
        wxString allegroBrdPath = dataPath + brdName;
        BOOST_TEST_CONTEXT( "Filename: " << allegroBrdPath )
        {
            BOOST_CHECK_EXCEPTION( allegroPlugin.LoadBoard( allegroBrdPath, nullptr ), IO_ERROR,
                                   [expectedMessage]( IO_ERROR exception )
                                   {
                                       BOOST_TEST_MESSAGE( "Checking that exception message \""
                                                           << exception.Problem() << "\" matches expected message \""
                                                           << expectedMessage << "\"" );
                                       return exception.Problem() == expectedMessage;
                                   } );
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
