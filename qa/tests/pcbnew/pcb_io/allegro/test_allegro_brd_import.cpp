#include <pcbnew_utils/board_test_utils.h>
#include <pcbnew_utils/board_file_utils.h>
#include <qa_utils/wx_utils/unit_test_utils.h>

#include <pcbnew/pcb_io/allegro/pcb_io_allegro.h>
#include <pcbnew/pcb_io/kicad_sexpr/pcb_io_kicad_sexpr.h>

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
            BOARD* brd = allegroPlugin.LoadBoard( allegroBrdPath, nullptr );
            BOOST_CHECK_NE( brd->GetCopperLayerCount(), 0 );
        }
    }
};

BOOST_AUTO_TEST_SUITE_END()
