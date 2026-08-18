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

#ifndef STEP_SOLID_EXPORT_H
#define STEP_SOLID_EXPORT_H

#include <cstdint>
#include <vector>

#include <math/vector2d.h>

class REPORTER;

/**
 * Helpers that reconstruct simple parametric solids with OpenCASCADE and serialize them to an
 * in-memory STEP file.  These are used to import Altium's generated (non-file) 3D bodies —
 * extruded 2D outlines, cylinders and spheres — as embedded STEP models.
 *
 * The colour argument is packed as Altium stores it: red in bits 0-7, green in bits 8-15 and blue
 * in bits 16-23.  All lengths are in millimetres.  On success the STEP file bytes are written to
 * @p aStepOut and true is returned.
 *
 * OpenCASCADE's STEP writer and its global static settings are not thread-safe, so these functions
 * serialize their OCC access internally.
 */
namespace STEP_SOLID_EXPORT
{
/// Extrude a closed 2D polygon (millimetres) from @p aZMinMM up to @p aZMaxMM.
bool ExtrudedPolygonToStep( const std::vector<VECTOR2D>& aOutlineMM, double aZMinMM, double aZMaxMM, uint32_t aColorBGR,
                            std::vector<char>& aStepOut, REPORTER* aReporter );

/// A cylinder of the given radius and height, its base on Z = 0 and axis along +Z.
bool CylinderToStep( double aRadiusMM, double aHeightMM, uint32_t aColorBGR, std::vector<char>& aStepOut,
                     REPORTER* aReporter );

/// A sphere of the given radius, centred on the origin.
bool SphereToStep( double aRadiusMM, uint32_t aColorBGR, std::vector<char>& aStepOut, REPORTER* aReporter );
} // namespace STEP_SOLID_EXPORT

#endif // STEP_SOLID_EXPORT_H
