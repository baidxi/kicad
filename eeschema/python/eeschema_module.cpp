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

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <wx/wx.h>

#include <sch_io/sch_io_mgr.h>
#include <sch_io/sch_io.h>
#include <sch_sheet.h>
#include <sch_screen.h>
#include <schematic.h>
#include <settings/settings_manager.h>
#include <io/io_mgr.h>
#include <kiface_base.h>
#include <pgm_base.h>
#include <eeschema_settings.h>
#include <symbol_editor/symbol_editor_settings.h>

namespace py = pybind11;

// -----------------------------------------------------------------------
// Minimal PGM_BASE for headless Python use.
// -----------------------------------------------------------------------
namespace
{
struct HeadlessPgm : PGM_BASE
{
    void MacOpenFile( const wxString& ) override {}
};

HeadlessPgm s_headlessPgm;
} // namespace

// -----------------------------------------------------------------------
// Stub Kiface() required by eeschema_kiface_objects.
// -----------------------------------------------------------------------
namespace
{
struct DummyKiface : KIFACE_BASE
{
    DummyKiface() : KIFACE_BASE( "eeschema", KIWAY::FACE_SCH ) {}

    bool       OnKifaceStart( PGM_BASE*, int, KIWAY* ) override { return true; }
    wxWindow*  CreateKiWindow( wxWindow*, int, KIWAY*, int ) override
    {
        return nullptr;
    }
    void*      IfaceOrAddress( int ) override { return nullptr; }
};

DummyKiface s_dummyKiface;
} // namespace

KIFACE_BASE& Kiface()
{
    return s_dummyKiface;
}

using SCH_FILE_T = SCH_IO_MGR::SCH_FILE_T;

// -----------------------------------------------------------------------
// SchematicContext - manages the lifecycle of SETTINGS_MANAGER and
// SCHEMATIC objects. The Python user must hold a reference to this
// object as long as any SCH_SHEET obtained from LoadSchematic is in use.
// -----------------------------------------------------------------------
class SchematicContext
{
public:
    SchematicContext() :
            m_schematic( nullptr )
    {
        m_settingsManager.LoadProject( "" );
        m_schematic.SetProject( &m_settingsManager.Prj() );
        m_schematic.Reset();
    }

    ~SchematicContext()
    {
        m_schematic.Reset();
    }

    SCHEMATIC& GetSchematic() { return m_schematic; }

private:
    SETTINGS_MANAGER m_settingsManager;
    SCHEMATIC        m_schematic;
};

// -----------------------------------------------------------------------
// String conversion helpers
// -----------------------------------------------------------------------
static std::string wxToStr( const wxString& aWs )
{
    return std::string( aWs.ToUTF8() );
}

static wxString strToWx( const std::string& aStr )
{
    return wxString( aStr.c_str(), wxConvUTF8 );
}

// -----------------------------------------------------------------------
// Properties conversion: std::map<std::string, std::string> (Python)
//   → std::map<std::string, UTF8> (KiCad)
// -----------------------------------------------------------------------
static void convertProperties(
        const std::map<std::string, std::string>& aSrc,
        std::map<std::string, UTF8>&              aDst )
{
    for( const auto& [key, value] : aSrc )
        aDst.emplace( key, UTF8( value ) );
}

// -----------------------------------------------------------------------
// Schematic I/O wrappers (named functions instead of lambdas)
// -----------------------------------------------------------------------
static SCH_SHEET* doLoadSchematic( SCH_FILE_T aFileType,
                                   const std::string& aFileName,
                                   SchematicContext& aContext,
                                   const std::map<std::string, std::string>& aProperties )
{
    IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( aFileType ) );

    if( !plugin )
    {
        throw std::runtime_error(
                "No plugin found for file type "
                + wxToStr( SCH_IO_MGR::ShowType( aFileType ) ) );
    }

    std::map<std::string, UTF8> props;
    convertProperties( aProperties, props );

    const std::map<std::string, UTF8>* pProps = aProperties.empty() ? nullptr : &props;

    return plugin->LoadSchematicFile( strToWx( aFileName ), &aContext.GetSchematic(),
                                      nullptr, pProps );
}

static void doSaveSchematic( SCH_FILE_T aFileType, const std::string& aFileName,
                             SCH_SHEET* aSheet, SchematicContext& aContext,
                             const std::map<std::string, std::string>& aProperties )
{
    IO_RELEASER<SCH_IO> plugin( SCH_IO_MGR::FindPlugin( aFileType ) );

    if( !plugin )
    {
        throw std::runtime_error(
                "No plugin found for file type "
                + wxToStr( SCH_IO_MGR::ShowType( aFileType ) ) );
    }

    std::map<std::string, UTF8> props;
    convertProperties( aProperties, props );

    const std::map<std::string, UTF8>* pProps = aProperties.empty() ? nullptr : &props;

    plugin->SaveSchematicFile( strToWx( aFileName ), aSheet, &aContext.GetSchematic(),
                               pProps );
}

// clang-format off

// -----------------------------------------------------------------------
// Pybind11 module definition
// -----------------------------------------------------------------------
PYBIND11_MODULE( _eeschema, m )
{
    // Initialize wxWidgets and KiCad program context for headless operation
    if( !wxTheApp )
        wxInitialize();

    SetPgm( &s_headlessPgm );
    s_headlessPgm.InitPgm( true );

    // Register eeschema settings (required by InitPgm/SCHEMATIC)
    s_headlessPgm.GetSettingsManager().RegisterSettings( new EESCHEMA_SETTINGS );
    s_headlessPgm.GetSettingsManager().RegisterSettings( new SYMBOL_EDITOR_SETTINGS );

    m.doc() = "KiCad Eeschema - Schematic editor Python bindings";

    // -- SchematicContext --------------------------------------------------
    py::class_<SchematicContext>( m, "SchematicContext" )
            .def( py::init<>(),
                  "Create a new schematic context for loading/saving schematics." );

    // -- SCH_SHEET ---------------------------------------------------------
    py::class_<SCH_SHEET>( m, "SCH_SHEET" )
            .def( "GetFileName",
                  []( SCH_SHEET& aSheet ) { return wxToStr( aSheet.GetFileName() ); },
                  "Return the filename associated with this sheet." )
            .def( "GetName",
                  []( SCH_SHEET& aSheet ) { return wxToStr( aSheet.GetName() ); },
                  "Return the sheet name." )
            .def( "GetScreen",
                  []( SCH_SHEET& aSheet ) -> SCH_SCREEN* { return aSheet.GetScreen(); },
                  py::return_value_policy::reference,
                  "Return the screen (drawing data) associated with this sheet." );

    // -- SCH_SCREEN --------------------------------------------------------
    py::class_<SCH_SCREEN>( m, "SCH_SCREEN" )
            .def( "GetFileName",
                  []( SCH_SCREEN& aScreen ) { return wxToStr( aScreen.GetFileName() ); },
                  "Return the file name associated with this screen." );

    // -- SCH_IO_Mgr --------------------------------------------------------
    py::class_<SCH_IO_MGR> schIoMgr( m, "SCH_IO_Mgr" );

    schIoMgr
            .def_static( "ShowType",
                         []( SCH_FILE_T aType ) {
                             return wxToStr( SCH_IO_MGR::ShowType( aType ) );
                         },
                         py::arg( "file_type" ),
                         "Return a human-readable name for a plugin type." )

            .def_static( "GuessPluginTypeFromSchPath",
                         []( const std::string& aPath ) {
                             return SCH_IO_MGR::GuessPluginTypeFromSchPath(
                                     strToWx( aPath ) );
                         },
                         py::arg( "path" ),
                         "Auto-detect the schematic plugin type from a file path." )

            .def_static( "LoadSchematic", &doLoadSchematic,
                         py::arg( "file_type" ),
                         py::arg( "file_name" ),
                         py::arg( "ctx" ),
                         py::arg( "properties" ) = std::map<std::string, std::string>{},
                         py::return_value_policy::reference,
                         "Load a schematic file using the specified plugin type.\n"
                         "Returns the root SCH_SHEET of the loaded design.\n"
                         "The returned sheet is owned by ctx; keep ctx alive." )

            .def_static( "SaveSchematic", &doSaveSchematic,
                         py::arg( "file_type" ),
                         py::arg( "file_name" ),
                         py::arg( "sheet" ),
                         py::arg( "ctx" ),
                         py::arg( "properties" ) = std::map<std::string, std::string>{},
                         "Save a schematic to a file using the specified plugin type." );

    // -- SCH_FILE_T enum ------------------------------------------------
    py::enum_<SCH_IO_MGR::SCH_FILE_T>( m, "SCH_FILE_T" )
            .value( "SCH_FILE_UNKNOWN", SCH_IO_MGR::SCH_FILE_UNKNOWN )
            .value( "SCH_KICAD", SCH_IO_MGR::SCH_KICAD )
            .value( "SCH_LEGACY", SCH_IO_MGR::SCH_LEGACY )
            .value( "SCH_ALTIUM", SCH_IO_MGR::SCH_ALTIUM )
            .value( "SCH_CADSTAR_ARCHIVE", SCH_IO_MGR::SCH_CADSTAR_ARCHIVE )
            .value( "SCH_DATABASE", SCH_IO_MGR::SCH_DATABASE )
            .value( "SCH_EAGLE", SCH_IO_MGR::SCH_EAGLE )
            .value( "SCH_EASYEDA", SCH_IO_MGR::SCH_EASYEDA )
            .value( "SCH_EASYEDAPRO", SCH_IO_MGR::SCH_EASYEDAPRO )
            .value( "SCH_EASYEDAPRO_V3", SCH_IO_MGR::SCH_EASYEDAPRO_V3 )
            .value( "SCH_GEDA", SCH_IO_MGR::SCH_GEDA )
            .value( "SCH_LTSPICE", SCH_IO_MGR::SCH_LTSPICE )
            .value( "SCH_HTTP", SCH_IO_MGR::SCH_HTTP )
            .value( "SCH_PADS", SCH_IO_MGR::SCH_PADS )
            .value( "SCH_DIPTRACE", SCH_IO_MGR::SCH_DIPTRACE )
            .value( "SCH_PCAD", SCH_IO_MGR::SCH_PCAD )
            .value( "SCH_ORCAD", SCH_IO_MGR::SCH_ORCAD )
            .export_values();
}
