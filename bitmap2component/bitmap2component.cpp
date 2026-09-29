/*
 * This program source code file is part of KICAD, a free EDA CAD application.
 *
 * Copyright (C) 1992-2019 jean-pierre.charras
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

#include <string>
#include <kiid.h>

#include <bitmap_tracing.h>
#include <build_version.h>
#include <drawing_sheet/ds_data_item.h>
#include <drawing_sheet/ds_data_model.h>
#include <io/kicad/kicad_io_utils.h>
#include <io/kicad/sexpr_file_versions.h>
#include <locale_io.h>
#include <macros.h>
#include <reporter.h>
#include <fmt/format.h>
#include <wx/translation.h>

#include "bitmap2component.h"


BITMAPCONV_INFO::BITMAPCONV_INFO( std::string& aData, REPORTER& aReporter ):
    m_Data( aData ),
    m_reporter( aReporter )
{
    m_Format = POSTSCRIPT_FMT;
    m_PixmapWidth  = 0;
    m_PixmapHeight = 0;
    m_ScaleX  = 1.0;
    m_ScaleY = 1.0;
    m_CmpName = "LOGO";
}


void BITMAPCONV_INFO::ConvertBitmap( const wxImage& aBilevelImage, OUTPUT_FMT_ID aFormat, int aDpi_X, int aDpi_Y,
                                     const wxString& aLayer )
{
    m_PixmapWidth = aBilevelImage.GetWidth();
    m_PixmapHeight = aBilevelImage.GetHeight(); // the bitmap size in pixels
    m_Format       = aFormat;

    switch( aFormat )
    {
    case DRAWING_SHEET_FMT:
        m_ScaleX = PL_IU_PER_MM * 25.4 / aDpi_X;  // the conversion scale from PPI to micron
        m_ScaleY = PL_IU_PER_MM * 25.4 / aDpi_Y;  // Y axis is top to bottom
        break;

    case POSTSCRIPT_FMT:
        m_ScaleX = 1.0;                // the conversion scale
        m_ScaleY = m_ScaleX;
        break;

    case SYMBOL_FMT:
    case SYMBOL_PASTE_FMT:
        m_ScaleX = SCH_IU_PER_MM * 25.4 / aDpi_X;   // the conversion scale from PPI to eeschema iu
        m_ScaleY = -SCH_IU_PER_MM * 25.4 / aDpi_Y;  // Y axis is bottom to Top for components in libs
        break;

    case FOOTPRINT_FMT:
        m_ScaleX = PCB_IU_PER_MM * 25.4 / aDpi_X;   // the conversion scale from PPI to IU
        m_ScaleY = PCB_IU_PER_MM * 25.4 / aDpi_Y;   // Y axis is top to bottom in Footprint Editor
        break;
    }

    SHAPE_POLY_SET polygons = TraceBilevelImage( aBilevelImage, m_ScaleX, m_ScaleY, m_reporter );

    if( aFormat == FOOTPRINT_FMT )
        createOutputData( polygons, aLayer );
    else
        createOutputData( polygons );
}


void BITMAPCONV_INFO::outputDataHeader( const wxString& aBrdLayerName )
{
    double Ypos = ( m_PixmapHeight / 2.0 * m_ScaleY );    // fields Y position in mm
    double fieldSize;             // fields text size in mm

    switch( m_Format )
    {
    case POSTSCRIPT_FMT:
        m_Data += "%!PS-Adobe-3.0 EPSF-3.0\n";
        m_Data += fmt::format( "%%BoundingBox: 0 0 {} {}\n", m_PixmapWidth, m_PixmapHeight );
        m_Data += "gsave\n";
        break;

    case FOOTPRINT_FMT:
        // fields text size = 1.5 mm
        // fields text thickness = 1.5 / 5 = 0.3mm
        m_Data += fmt::format( "(footprint \"{}\" (version {}) (generator \"bitmap2component\") (generator_version \"{}\")\n"
                               "  (layer \"F.Cu\")\n",
                               m_CmpName.c_str(),
                               SEXPR_FOOTPRINT_FILE_VERSION,
                               GetMajorMinorVersion().ToStdString() );

        m_Data += fmt::format( "  (attr board_only exclude_from_pos_files exclude_from_bom)\n" );
        m_Data += fmt::format( "  (fp_text reference \"G***\" (at 0 0) (layer \"{}\")\n"
                               "      (effects (font (size 1.5 1.5) (thickness 0.3)))\n"
                               "    (uuid {})\n  )\n",
                               aBrdLayerName.ToStdString().c_str(),
                               KIID().AsString().ToStdString().c_str() );

        m_Data += fmt::format( "  (fp_text value \"{}\" (at 0.75 0) (layer \"{}\") hide\n"
                               "      (effects (font (size 1.5 1.5) (thickness 0.3)))\n"
                               "    (uuid {})\n  )\n",
                               m_CmpName.c_str(),
                               aBrdLayerName.ToStdString().c_str(),
                               KIID().AsString().ToStdString().c_str() );
        break;

    case DRAWING_SHEET_FMT:
        break;

    case SYMBOL_FMT:
        m_Data += fmt::format( "(kicad_symbol_lib (version {}) (generator \"bitmap2component\") (generator_version \"{}\")\n",
                               SEXPR_SYMBOL_LIB_FILE_VERSION,
                               GetMajorMinorVersion().ToStdString() );
        KI_FALLTHROUGH;

    case SYMBOL_PASTE_FMT:
        fieldSize = 1.27;             // fields text size in mm (= 50 mils)
        Ypos /= SCH_IU_PER_MM;
        Ypos += fieldSize / 2;
        m_Data += fmt::format( "  (symbol \"{}\" (pin_names (offset 1.016)) (in_bom yes) (on_board yes)\n",
                               m_CmpName.c_str() );

        m_Data += fmt::format( "    (property \"Reference\" \"#G\" (at 0 {:g} 0)\n"
                               "      (effects (font (size {:g} {:g})) hide)\n    )\n",
                               -Ypos,
                               fieldSize,
                               fieldSize );

        m_Data += fmt::format( "    (property \"Value\" \"{}\" (at 0 {:g} 0)\n"
                               "      (effects (font (size {:g} {:g})) hide)\n    )\n",
                               m_CmpName.c_str(),
                               Ypos,
                               fieldSize,
                               fieldSize );

        m_Data += fmt::format( "    (property \"Footprint\" \"\" (at 0 0 0)\n"
                               "      (effects (font (size {:g} {:g})) hide)\n    )\n",
                               fieldSize,
                               fieldSize );

        m_Data += fmt::format( "    (property \"Datasheet\" \"\" (at 0 0 0)\n"
                               "      (effects (font (size {:g} {:g})) hide)\n    )\n",
                               fieldSize,
                               fieldSize );

        m_Data += fmt::format( "    (symbol \"{}_0_0\"\n", m_CmpName.c_str() );
        break;
    }
}


void BITMAPCONV_INFO::outputDataEnd()
{
    switch( m_Format )
    {
    case POSTSCRIPT_FMT:
        m_Data += "grestore\n";
        m_Data += "%%EOF\n";
        break;

    case FOOTPRINT_FMT:
        m_Data += ")\n";
        break;

    case DRAWING_SHEET_FMT:
        break;

    case SYMBOL_PASTE_FMT:
        m_Data += "    )\n";        // end symbol_0_0
        m_Data += "  )\n";          // end symbol
        break;

    case SYMBOL_FMT:
        m_Data += "    )\n";        // end symbol_0_0
        m_Data += "  )\n";          // end symbol
        m_Data += ")\n";            // end lib
        break;
    }
}


void BITMAPCONV_INFO::outputOnePolygon( SHAPE_LINE_CHAIN& aPolygon, const wxString& aBrdLayerName )
{
    // write one polygon to output file.
    // coordinates are expected in target unit.
    int      ii, jj;
    VECTOR2I currpoint;
    int      offsetX = KiROUND( m_PixmapWidth / 2.0 * m_ScaleX );
    int      offsetY = KiROUND( m_PixmapHeight / 2.0 * m_ScaleY );

    const VECTOR2I startpoint = aPolygon.CPoint( 0 );

    switch( m_Format )
    {
    case POSTSCRIPT_FMT:
        offsetY = (int)( m_PixmapHeight * m_ScaleY );
        m_Data += fmt::format( "newpath\n{} {} moveto\n", startpoint.x, offsetY - startpoint.y );
        jj = 0;

        for( ii = 1; ii < aPolygon.PointCount(); ii++ )
        {
            currpoint = aPolygon.CPoint( ii );
            m_Data += fmt::format( " {} {} lineto", currpoint.x, offsetY - currpoint.y );

            if( jj++ > 6 )
            {
                jj = 0;
                m_Data += "\n";
            }
        }

        m_Data += "\nclosepath fill\n";
        break;

    case FOOTPRINT_FMT:
        m_Data += "  (fp_poly\n    (pts\n";

        jj = 0;

        for( ii = 0; ii < aPolygon.PointCount(); ii++ )
        {
            currpoint = aPolygon.CPoint( ii );
            m_Data += fmt::format( "      (xy {} {})\n",
                                   ( currpoint.x - offsetX ) / PCB_IU_PER_MM,
                                   ( currpoint.y - offsetY ) / PCB_IU_PER_MM );
        }
        // No need to close polygon

        m_Data += "    )\n\n";
        m_Data += fmt::format( "    (stroke (width {:f}) (type solid)) (fill solid) (layer \"{}\") (uuid {}))\n",
                               0.0,
                               aBrdLayerName.ToStdString().c_str(),
                               KIID().AsString().ToStdString().c_str() );
        break;

    case DRAWING_SHEET_FMT:
        break;

    case SYMBOL_FMT:
    case SYMBOL_PASTE_FMT:
        // The polygon outline thickness is fixed here to 0.01  ( 0.0 is the default thickness)
#define SCH_LINE_THICKNESS_MM 0.01
        //snprintf( strbuf, sizeof(strbuf), "P %d 0 0 %d", (int) aPolygon.PointCount() + 1, EE_LINE_THICKNESS );
        m_Data += "      (polyline\n        (pts\n";

        for( ii = 0; ii < aPolygon.PointCount(); ii++ )
        {
            currpoint = aPolygon.CPoint( ii );
            m_Data += fmt::format( "          (xy {:f} {:f})\n",
                                   ( currpoint.x - offsetX ) / SCH_IU_PER_MM,
                                   ( currpoint.y - offsetY ) / SCH_IU_PER_MM );
        }

        // Close polygon
        m_Data += fmt::format( "          (xy {:f} {:f})\n",
                               ( startpoint.x - offsetX ) / SCH_IU_PER_MM,
                               ( startpoint.y - offsetY ) / SCH_IU_PER_MM );
        m_Data += "        )\n";        // end pts

        m_Data += fmt::format( "        (stroke (width {:g}) (type default))\n"
                               "        (fill (type outline))\n",
                               SCH_LINE_THICKNESS_MM );
        m_Data += "      )\n"; // end polyline
        break;
    }
}


void BITMAPCONV_INFO::createOutputData( const SHAPE_POLY_SET& aPolygons, const wxString& aLayer )
{
    if( aPolygons.IsEmpty() )
    {
        m_reporter.Report( _( "No shape in black and white image to convert: no outline created." ),
                           RPT_SEVERITY_ERROR );
    }

    if( m_Format == DRAWING_SHEET_FMT )
    {
        SHAPE_POLY_SET outputPolygons = aPolygons;
        createDrawingSheetData( outputPolygons );
        return;
    }

    // The layer name has meaning only for .kicad_mod files.
    // For these files the header creates 2 invisible texts: value and ref
    // (needed but not useful) on silk screen layer
    outputDataHeader( "F.SilkS" );

    for( int ii = 0; ii < aPolygons.OutlineCount(); ii++ )
    {
        SHAPE_LINE_CHAIN poly = aPolygons.COutline( ii );
        outputOnePolygon( poly, aLayer );
    }

    outputDataEnd();
}


void BITMAPCONV_INFO::createDrawingSheetData( SHAPE_POLY_SET& aPolyset )
{
    DS_DATA_MODEL model;
    model.SetGenerator( wxT( "bitmap2component" ) );
    model.SetLeftMargin( 10 );
    model.SetRightMargin( 10 );
    model.SetTopMargin( 10 );
    model.SetBottomMargin( 10 );
    model.m_DefaultTextSize = VECTOR2D( 1.5, 1.5 );
    model.m_DefaultLineWidth = 0.15;
    model.m_DefaultTextThickness = 0.15;

    auto polyItem = std::make_unique<DS_DATA_ITEM_POLYGONS>();
    polyItem->m_Name = wxEmptyString;
    polyItem->SetStart( 0, 0, RB_CORNER );
    polyItem->m_LineWidth = 0.01;

    int offsetX = KiROUND( m_PixmapWidth / 2.0 * m_ScaleX );
    int offsetY = KiROUND( m_PixmapHeight / 2.0 * m_ScaleY );

    for( int ii = 0; ii < aPolyset.OutlineCount(); ii++ )
    {
        SHAPE_LINE_CHAIN& poly = aPolyset.Outline( ii );

        if( poly.PointCount() == 0 )
            continue;

        for( int jj = 0; jj < poly.PointCount(); jj++ )
        {
            VECTOR2I pt = poly.CPoint( jj );
            polyItem->AppendCorner( VECTOR2D( ( pt.x - offsetX ) / PL_IU_PER_MM,
                                              ( pt.y - offsetY ) / PL_IU_PER_MM ) );
        }

        // Close polygon by repeating the first point
        VECTOR2I startPt = poly.CPoint( 0 );
        polyItem->AppendCorner( VECTOR2D( ( startPt.x - offsetX ) / PL_IU_PER_MM,
                                          ( startPt.y - offsetY ) / PL_IU_PER_MM ) );
        polyItem->CloseContour();
    }

    if( polyItem->GetPolyCount() > 0 )
        model.Append( polyItem.release() );

    wxString output;
    model.SaveInString( &output );

    std::string result = output.ToStdString();
    KICAD_FORMAT::Prettify( result );
    m_Data += result;
}
