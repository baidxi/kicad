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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "step_solid_export.h"

#include <mutex>
#include <sstream>

#include <reporter.h>

#include <Standard_Version.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <TopoDS_Shape.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <Quantity_Color.hxx>
#include <Message.hxx>
#include <Message_PrinterOStream.hxx>

#if OCC_VERSION_HEX < 0x070700
#include <fstream>
#include <cstdio>
#include <wx/filename.h>
#endif


namespace
{
// OpenCASCADE's STEP writer relies on process-global Interface_Static settings and is not
// re-entrant, so all serialization is funnelled through this mutex.
std::mutex g_stepWriterMutex;


/// Permanently remove OpenCASCADE's default stderr message printers so reconstructing bodies does
/// not spam the console.  This mirrors the STEP exporter (exporter_step.cpp), which also strips the
/// default printers process-wide; OCC re-installs them where it needs them.
void disableOccDefaultPrinters()
{
    Message::DefaultMessenger()->RemovePrinters( STANDARD_TYPE( Message_PrinterOStream ) );
}


Quantity_Color makeColor( uint32_t aColorBGR )
{
    return Quantity_Color( ( aColorBGR & 0xFF ) / 255.0,           // red
                           ( ( aColorBGR >> 8 ) & 0xFF ) / 255.0,  // green
                           ( ( aColorBGR >> 16 ) & 0xFF ) / 255.0, // blue
                           Quantity_TOC_RGB );
}


bool shapeToStep( const TopoDS_Shape& aShape, uint32_t aColorBGR, std::vector<char>& aStepOut, REPORTER* aReporter )
{
    auto report = [&]( const wxString& aMsg )
    {
        if( aReporter )
            aReporter->Report( aMsg, RPT_SEVERITY_ERROR );
    };

    if( aShape.IsNull() )
    {
        report( wxT( "3D body reconstruction produced an empty shape" ) );
        return false;
    }

    Handle( XCAFApp_Application ) app = XCAFApp_Application::GetApplication();
    Handle( TDocStd_Document ) doc;
    app->NewDocument( "XCAF", doc );

    if( doc.IsNull() )
    {
        report( wxT( "Failed to create OpenCASCADE document for 3D body" ) );
        return false;
    }

    Handle( XCAFDoc_ShapeTool ) shapeTool = XCAFDoc_DocumentTool::ShapeTool( doc->Main() );
    Handle( XCAFDoc_ColorTool ) colorTool = XCAFDoc_DocumentTool::ColorTool( doc->Main() );

    if( shapeTool.IsNull() || colorTool.IsNull() )
    {
        report( wxT( "Failed to initialize OpenCASCADE XCAF tools for 3D body" ) );
        return false;
    }

    TDF_Label label = shapeTool->AddShape( aShape );
    colorTool->SetColor( label, makeColor( aColorBGR ), XCAFDoc_ColorSurf );

    // Serialize.  The writer's global state is not thread-safe, so guard it.
    std::lock_guard<std::mutex> lock( g_stepWriterMutex );

    disableOccDefaultPrinters();
    STEPCAFControl_Writer writer;

    if( !writer.Transfer( doc ) )
    {
        report( wxT( "Failed to transfer 3D body to STEP model" ) );
        return false;
    }

#if OCC_VERSION_HEX >= 0x070700
    // OpenCASCADE 7.7.0+ can write STEP straight to a stream, avoiding a temporary file.
    std::ostringstream oss;

    if( writer.WriteStream( oss ) != IFSelect_RetDone )
    {
        report( wxT( "Failed to write STEP model for 3D body" ) );
        return false;
    }

    const std::string& str = oss.str();
    aStepOut.assign( str.begin(), str.end() );
#else
    // Older OpenCASCADE cannot stream STEP; fall back to a unique temporary file.
    wxString tmpName = wxFileName::CreateTempFileName( wxT( "kicad_step" ) );

    if( tmpName.IsEmpty() || writer.Write( tmpName.ToUTF8() ) != IFSelect_RetDone )
    {
        report( wxT( "Failed to write STEP model for 3D body" ) );

        if( !tmpName.IsEmpty() )
            std::remove( tmpName.ToUTF8() );

        return false;
    }

    std::ifstream in( tmpName.ToUTF8(), std::ios::binary );
    aStepOut.assign( std::istreambuf_iterator<char>( in ), std::istreambuf_iterator<char>() );
    in.close();
    std::remove( tmpName.ToUTF8() );
#endif

    return !aStepOut.empty();
}
} // namespace


bool STEP_SOLID_EXPORT::ExtrudedPolygonToStep( const std::vector<VECTOR2D>& aOutlineMM, double aZMinMM, double aZMaxMM,
                                               uint32_t aColorBGR, std::vector<char>& aStepOut, REPORTER* aReporter )
{
    if( aOutlineMM.size() < 3 )
    {
        if( aReporter )
            aReporter->Report( wxT( "Too few outline points for extruded 3D body" ), RPT_SEVERITY_ERROR );

        return false;
    }

    const double height = aZMaxMM - aZMinMM;

    if( height <= 0.0 )
    {
        if( aReporter )
            aReporter->Report( wxT( "Extruded 3D body has a non-positive height" ), RPT_SEVERITY_ERROR );

        return false;
    }

    BRepBuilderAPI_MakePolygon polygon;

    for( const VECTOR2D& pt : aOutlineMM )
        polygon.Add( gp_Pnt( pt.x, pt.y, aZMinMM ) );

    polygon.Close();

    if( !polygon.IsDone() )
    {
        if( aReporter )
            aReporter->Report( wxT( "Failed to build outline for extruded 3D body" ), RPT_SEVERITY_ERROR );

        return false;
    }

    BRepBuilderAPI_MakeFace face( polygon.Wire() );

    if( !face.IsDone() )
    {
        if( aReporter )
            aReporter->Report( wxT( "Failed to build face for extruded 3D body" ), RPT_SEVERITY_ERROR );

        return false;
    }

    BRepPrimAPI_MakePrism prism( face.Face(), gp_Vec( 0.0, 0.0, height ) );

    if( !prism.IsDone() )
    {
        if( aReporter )
            aReporter->Report( wxT( "Failed to extrude 3D body" ), RPT_SEVERITY_ERROR );

        return false;
    }

    return shapeToStep( prism.Shape(), aColorBGR, aStepOut, aReporter );
}


bool STEP_SOLID_EXPORT::CylinderToStep( double aRadiusMM, double aHeightMM, uint32_t aColorBGR,
                                        std::vector<char>& aStepOut, REPORTER* aReporter )
{
    if( aRadiusMM <= 0.0 || aHeightMM <= 0.0 )
    {
        if( aReporter )
            aReporter->Report( wxT( "Invalid cylinder 3D body dimensions" ), RPT_SEVERITY_ERROR );

        return false;
    }

    BRepPrimAPI_MakeCylinder cylinder( aRadiusMM, aHeightMM );
    return shapeToStep( cylinder.Shape(), aColorBGR, aStepOut, aReporter );
}


bool STEP_SOLID_EXPORT::SphereToStep( double aRadiusMM, uint32_t aColorBGR, std::vector<char>& aStepOut,
                                      REPORTER* aReporter )
{
    if( aRadiusMM <= 0.0 )
    {
        if( aReporter )
            aReporter->Report( wxT( "Invalid sphere 3D body radius" ), RPT_SEVERITY_ERROR );

        return false;
    }

    BRepPrimAPI_MakeSphere sphere( aRadiusMM );
    return shapeToStep( sphere.Shape(), aColorBGR, aStepOut, aReporter );
}
