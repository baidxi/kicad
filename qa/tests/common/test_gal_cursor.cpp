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

#include <boost/test/unit_test.hpp>

#include <gal/gal_display_options.h>
#include <gal/graphics_abstraction_layer.h>


class TEST_GAL : public KIGFX::GAL
{
public:
    TEST_GAL( KIGFX::GAL_DISPLAY_OPTIONS& aOptions ) : GAL( aOptions ) {}

    void ForceDisplayCursor( bool aEnabled ) { m_forceDisplayCursor = aEnabled; }
};


BOOST_AUTO_TEST_SUITE( GalCursor )


BOOST_AUTO_TEST_CASE( HiddenOutsideCanvas )
{
    KIGFX::GAL_DISPLAY_OPTIONS options;
    TEST_GAL                   gal( options );

    gal.SetCursorEnabled( true );
    BOOST_CHECK( !gal.IsCursorEnabled() );

    gal.SetCursorInCanvas( true );
    BOOST_CHECK( gal.IsCursorEnabled() );

    gal.SetCursorInCanvas( false );
    BOOST_CHECK( !gal.IsCursorEnabled() );

    gal.SetCursorInCanvas( true );
    BOOST_CHECK( gal.IsCursorEnabled() );
}


BOOST_AUTO_TEST_CASE( ForcedCursorHiddenOutsideCanvas )
{
    KIGFX::GAL_DISPLAY_OPTIONS options;
    TEST_GAL                   gal( options );

    gal.ForceDisplayCursor( true );
    BOOST_CHECK( !gal.IsCursorEnabled() );

    gal.SetCursorInCanvas( true );
    BOOST_CHECK( gal.IsCursorEnabled() );

    gal.SetCursorInCanvas( false );
    BOOST_CHECK( !gal.IsCursorEnabled() );
}


BOOST_AUTO_TEST_SUITE_END()
