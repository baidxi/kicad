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
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

/**
 * @file plm_ci.h
 * @brief CI/CD pipeline structures for the PLM client interface.
 *
 * This header defines structures for interacting with CI/CD pipeline
 * runs, jobs, and artifacts. Uses PLM_ARTIFACT_TYPE from plm_release.h
 * for artifact classification.
 */

#ifndef PLM_CI_H
#define PLM_CI_H

#include "plm_types.h"
#include "plm_release.h"

#include <wx/string.h>
#include <cstdint>
#include <vector>


/**
 * Status of a CI/CD pipeline run or individual job.
 *
 * Distinct from PLM_PIPELINE_STATE (forge-level) to allow CI-specific
 * states like TIMED_OUT.
 */
enum class PLM_CI_RUN_STATUS
{
    UNKNOWN = 0,
    QUEUED = 1,
    RUNNING = 2,
    SUCCEEDED = 3,
    FAILED = 4,
    CANCELLED = 5,
    SKIPPED = 6,
    TIMED_OUT = 7
};


/**
 * A single CI job within a pipeline run.
 */
struct PLM_CI_JOB
{
    wxString            job_id;
    wxString            name;
    PLM_CI_RUN_STATUS   status = PLM_CI_RUN_STATUS::UNKNOWN;
    int64_t             started_at = 0;
    int64_t             completed_at = 0;
    wxString            log_url;        ///< URL to job log
    wxString            runner_name;    ///< Name of the CI runner

    PLM_CI_JOB() = default;

    PLM_CI_JOB( const wxString& aId, const wxString& aName ) :
            job_id( aId ),
            name( aName )
    {
    }

    bool IsTerminal() const
    {
        return status == PLM_CI_RUN_STATUS::SUCCEEDED ||
               status == PLM_CI_RUN_STATUS::FAILED ||
               status == PLM_CI_RUN_STATUS::CANCELLED ||
               status == PLM_CI_RUN_STATUS::SKIPPED ||
               status == PLM_CI_RUN_STATUS::TIMED_OUT;
    }
};


/**
 * A downloadable CI artifact.
 */
struct PLM_CI_ARTIFACT
{
    wxString            artifact_id;
    wxString            name;           ///< Display name
    wxString            file_name;      ///< File name for download
    PLM_ARTIFACT_TYPE   type = PLM_ARTIFACT_TYPE::CUSTOM;
    size_t              size_bytes = 0;
    wxString            download_url;
    int64_t             expires_at = 0; ///< Expiry timestamp (0 = never)

    PLM_CI_ARTIFACT() = default;

    PLM_CI_ARTIFACT( const wxString& aId, const wxString& aName,
                     PLM_ARTIFACT_TYPE aType ) :
            artifact_id( aId ),
            name( aName ),
            type( aType )
    {
    }

    bool IsExpired( int64_t aNow ) const
    {
        return expires_at > 0 && aNow > expires_at;
    }
};


/**
 * A complete CI pipeline execution.
 */
struct PLM_PIPELINE_RUN
{
    wxString                        run_id;
    wxString                        project_id;
    wxString                        commit_sha;
    wxString                        branch;
    PLM_CI_RUN_STATUS               status = PLM_CI_RUN_STATUS::UNKNOWN;
    std::vector<PLM_CI_JOB>         jobs;
    std::vector<PLM_CI_ARTIFACT>    artifacts;
    int64_t                         started_at = 0;
    int64_t                         completed_at = 0;
    wxString                        web_url;
    wxString                        trigger_reason; ///< What triggered the run (push, PR, manual)

    PLM_PIPELINE_RUN() = default;

    bool IsComplete() const
    {
        return status == PLM_CI_RUN_STATUS::SUCCEEDED ||
               status == PLM_CI_RUN_STATUS::FAILED ||
               status == PLM_CI_RUN_STATUS::CANCELLED ||
               status == PLM_CI_RUN_STATUS::TIMED_OUT;
    }

    /**
     * Get human-readable status name.
     */
    wxString GetStatusName() const
    {
        switch( status )
        {
        case PLM_CI_RUN_STATUS::UNKNOWN:    return wxS( "Unknown" );
        case PLM_CI_RUN_STATUS::QUEUED:     return wxS( "Queued" );
        case PLM_CI_RUN_STATUS::RUNNING:    return wxS( "Running" );
        case PLM_CI_RUN_STATUS::SUCCEEDED:  return wxS( "Succeeded" );
        case PLM_CI_RUN_STATUS::FAILED:     return wxS( "Failed" );
        case PLM_CI_RUN_STATUS::CANCELLED:  return wxS( "Cancelled" );
        case PLM_CI_RUN_STATUS::SKIPPED:    return wxS( "Skipped" );
        case PLM_CI_RUN_STATUS::TIMED_OUT:  return wxS( "Timed Out" );
        default:                            return wxS( "Unknown" );
        }
    }
};


/**
 * Filter for pipeline queries.
 */
struct PLM_PIPELINE_FILTER
{
    wxString    branch;         ///< Filter by branch (empty = all)
    wxString    commit_sha;     ///< Filter by commit (empty = all)
    size_t      limit = 20;     ///< Maximum results
    size_t      offset = 0;     ///< Pagination offset

    PLM_PIPELINE_FILTER() = default;

    static PLM_PIPELINE_FILTER ForBranch( const wxString& aBranch, size_t aLimit = 20 )
    {
        PLM_PIPELINE_FILTER f;
        f.branch = aBranch;
        f.limit = aLimit;
        return f;
    }
};


#endif // PLM_CI_H
