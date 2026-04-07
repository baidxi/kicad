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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef JOB_GERBER_EXPORT_PNG_H
#define JOB_GERBER_EXPORT_PNG_H

#include <kicommon.h>
#include "job.h"

/**
 * Job to convert Gerber/Excellon files to PNG images.
 *
 * Uses the PNG_PLOTTER backend with Cairo for rasterization.
 */
class KICOMMON_API JOB_GERBER_EXPORT_PNG : public JOB
{
public:
    JOB_GERBER_EXPORT_PNG();

    wxString GetDefaultDescription() const override;
    wxString GetSettingsDialogTitle() const override;

    wxString m_inputFile;
    int      m_dpi = 300;
    int      m_width = 0;
    int      m_height = 0;
    bool     m_antialias = true;
    bool     m_transparentBackground = true;
    bool     m_strict = false;

    // When width/height are 0, calculate from DPI and Gerber bounds
    // When specified, override the automatic calculation
};

#endif
