/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2024 Mark Roszko <mark.roszko@gmail.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <kicommon.h>
#include <vector>
#include <optional>
#include <jobs/job.h>

class PROJECT;

class KICOMMON_API JOBS_OUTPUT_HANDLER
{
public:
    JOBS_OUTPUT_HANDLER()
    {
    }

    virtual ~JOBS_OUTPUT_HANDLER() {}

    /**
     * @brief Checks if the output process can proceed before doing anything else
     * This can include user prompts
     *
     * @return true if the output process can proceed
     */
    virtual bool OutputPrecheck() { return true; }
    virtual bool HandleOutputs( const wxString&                aBaseTempPath,
                                PROJECT*                       aProject,
                                const std::vector<wxString>&   aPathsWithOverwriteDisallowed,
                                const std::vector<JOB_OUTPUT>& aOutputsToHandle,
                                std::optional<wxString>&       aResolvedOutputPath,
                                const wxString&                aJobsetFilePath ) = 0;

    virtual void FromJson( const nlohmann::json& j ) = 0;
    virtual void ToJson( nlohmann::json& j ) const = 0;


    void SetOutputPath( const wxString& aPath ) { m_outputPath = aPath; }
    wxString GetOutputPath() const { return m_outputPath; }

    /**
     * Expand text/environment variables and a leading '~' in @a aPath, then resolve it to an
     * absolute path against the directory of @a aJobsetFilePath when the result is relative.
     *
     * @param aPath            output path as stored on the destination (may be relative).
     * @param aJobsetFilePath  full path to the jobset file; used as the base when @a aPath is
     *                         relative.  May be empty, in which case relative paths are returned
     *                         unchanged (after expansion).
     * @param aProject         project used to resolve text variables and env-var substitutions.
     *                         May be null, in which case no variable expansion is performed.
     */
    static wxString ResolveOutputPath( const wxString& aPath, const wxString& aJobsetFilePath,
                                       PROJECT* aProject );

    virtual wxString GetDefaultDescription() const { return wxEmptyString; }

protected:
    wxString m_outputPath;
};