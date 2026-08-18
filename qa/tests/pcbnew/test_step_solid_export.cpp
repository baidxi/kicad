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

#include <string>
#include <vector>

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <exporters/step/step_solid_export.h>


BOOST_AUTO_TEST_SUITE( StepSolidExport )


static bool isStepFile( const std::vector<char>& aData )
{
    // Every ISO 10303-21 (STEP) file begins with this token.
    static const std::string header = "ISO-10303-21;";

    return aData.size() > header.size() && std::string( aData.begin(), aData.begin() + header.size() ) == header;
}


BOOST_AUTO_TEST_CASE( ExtrudedPolygonProducesStep )
{
    // A 10 mm square extruded 1 mm high.
    std::vector<VECTOR2D> square = { { 0, 0 }, { 10, 0 }, { 10, 10 }, { 0, 10 } };
    std::vector<char>     step;

    BOOST_REQUIRE( STEP_SOLID_EXPORT::ExtrudedPolygonToStep( square, 0.0, 1.0, 0x00204080, step, nullptr ) );
    BOOST_CHECK( isStepFile( step ) );
}


BOOST_AUTO_TEST_CASE( CylinderAndSphereProduceStep )
{
    std::vector<char> cyl;
    BOOST_REQUIRE( STEP_SOLID_EXPORT::CylinderToStep( 2.5, 4.0, 0x00FFFFFF, cyl, nullptr ) );
    BOOST_CHECK( isStepFile( cyl ) );

    std::vector<char> sph;
    BOOST_REQUIRE( STEP_SOLID_EXPORT::SphereToStep( 3.0, 0x00FF0000, sph, nullptr ) );
    BOOST_CHECK( isStepFile( sph ) );
}


BOOST_AUTO_TEST_CASE( InvalidGeometryIsRejected )
{
    std::vector<char> step;

    // Fewer than three outline points cannot form a face.
    std::vector<VECTOR2D> line = { { 0, 0 }, { 1, 0 } };
    BOOST_CHECK( !STEP_SOLID_EXPORT::ExtrudedPolygonToStep( line, 0.0, 1.0, 0, step, nullptr ) );

    // Non-positive extrusion height.
    std::vector<VECTOR2D> square = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
    BOOST_CHECK( !STEP_SOLID_EXPORT::ExtrudedPolygonToStep( square, 1.0, 1.0, 0, step, nullptr ) );

    // Degenerate primitives.
    BOOST_CHECK( !STEP_SOLID_EXPORT::CylinderToStep( 0.0, 1.0, 0, step, nullptr ) );
    BOOST_CHECK( !STEP_SOLID_EXPORT::SphereToStep( -1.0, 0, step, nullptr ) );
}


BOOST_AUTO_TEST_SUITE_END()
