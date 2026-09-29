/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.TXT for contributors.
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

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <bitmap_tracing.h>
#include <reporter.h>

BOOST_AUTO_TEST_SUITE( BitmapTracing )


static wxImage makeImage( int aWidth, int aHeight, unsigned char aFill )
{
    wxImage image( aWidth, aHeight );
    image.SetRGB( wxRect( 0, 0, aWidth, aHeight ), aFill, aFill, aFill );
    return image;
}


static void fillRect( wxImage& aImage, const wxRect& aRect, unsigned char aValue )
{
    aImage.SetRGB( aRect, aValue, aValue, aValue );
}


BOOST_AUTO_TEST_CASE( ThresholdSplitsLightFromDark )
{
    wxImage image = makeImage( 16, 16, 200 );
    fillRect( image, wxRect( 8, 0, 8, 16 ), 20 );

    wxImage bilevel = ConvertImageToBilevel( image, 0.5, false );

    BOOST_CHECK_EQUAL( (int) bilevel.GetGreen( 2, 8 ), 255 );
    BOOST_CHECK_EQUAL( (int) bilevel.GetGreen( 12, 8 ), 0 );
}


BOOST_AUTO_TEST_CASE( InvertSwapsBlackAndWhite )
{
    wxImage image = makeImage( 16, 16, 200 );
    fillRect( image, wxRect( 8, 0, 8, 16 ), 20 );

    wxImage bilevel = ConvertImageToBilevel( image, 0.5, true );

    BOOST_CHECK_EQUAL( (int) bilevel.GetGreen( 2, 8 ), 0 );
    BOOST_CHECK_EQUAL( (int) bilevel.GetGreen( 12, 8 ), 255 );
}


BOOST_AUTO_TEST_CASE( TransparentPixelsDoNotBecomeBlack )
{
    wxImage image = makeImage( 16, 16, 0 );
    image.InitAlpha();

    for( int y = 0; y < 16; y++ )
    {
        for( int x = 0; x < 16; x++ )
            image.SetAlpha( x, y, x < 8 ? wxIMAGE_ALPHA_TRANSPARENT : wxIMAGE_ALPHA_OPAQUE );
    }

    wxImage bilevel = ConvertImageToBilevel( image, 0.5, false );

    BOOST_CHECK_MESSAGE( bilevel.GetGreen( 2, 8 ) == 255, "a transparent dark pixel must not be traced" );
    BOOST_CHECK_MESSAGE( bilevel.GetGreen( 12, 8 ) == 0, "an opaque dark pixel must be traced" );
}


BOOST_AUTO_TEST_CASE( RingTracesToOneOutlineWithItsHoleRemoved )
{
    wxImage image = makeImage( 64, 64, 255 );
    fillRect( image, wxRect( 12, 12, 40, 40 ), 0 );
    fillRect( image, wxRect( 22, 22, 20, 20 ), 255 );

    wxImage bilevel = ConvertImageToBilevel( image, 0.5, false );

    NULL_REPORTER  reporter;
    SHAPE_POLY_SET traced = TraceBilevelImage( bilevel, 1.0, 1.0, reporter );

    BOOST_REQUIRE( !traced.IsEmpty() );

    // Fracturing turns the hole into a slit, so the ring is a single outline.
    BOOST_CHECK_EQUAL( traced.OutlineCount(), 1 );

    const double expectedArea = ( 40.0 * 40.0 ) - ( 20.0 * 20.0 );
    BOOST_CHECK_CLOSE( std::abs( traced.Area() ), expectedArea, 5.0 );

    BOX2I bbox = traced.BBox();
    BOOST_CHECK_LE( std::abs( bbox.GetWidth() - 40 ), 2 );
    BOOST_CHECK_LE( std::abs( bbox.GetHeight() - 40 ), 2 );
}


BOOST_AUTO_TEST_SUITE_END()
