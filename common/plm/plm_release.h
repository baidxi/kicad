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
 * @file plm_release.h
 * @brief Release packaging, versioning, and deviation branch structures.
 *
 * This header defines structures for managing formal design releases
 * including manufacturing artifacts, version schemes, release checks,
 * and deviation branches for field issue resolution.
 */

#ifndef PLM_RELEASE_H
#define PLM_RELEASE_H

#include "plm_types.h"

#include <wx/string.h>
#include <cstdint>
#include <vector>


/**
 * Release lifecycle states.
 */
enum class PLM_RELEASE_STATUS
{
    DRAFT = 0,          ///< Release is being assembled
    VALIDATING = 1,     ///< Pre-release checks are running
    READY = 2,          ///< All checks passed, awaiting finalization
    RELEASED = 3,       ///< Release is finalized and immutable
    SUPERSEDED = 4,     ///< A newer release has replaced this one
    RECALLED = 5        ///< Release was recalled due to issues
};


/**
 * Unified manufacturing artifact type.
 *
 * Used by both release packaging (Group 6) and CI/CD pipeline
 * artifacts (Group 7).
 */
enum class PLM_ARTIFACT_TYPE
{
    GERBER = 0,             ///< Gerber fabrication files (RS-274X)
    DRILL = 1,              ///< Excellon drill files
    PICK_AND_PLACE = 2,     ///< Component placement data (CSV/IPC-D-356)
    BOM_EXPORT = 3,         ///< Bill of materials export (XLSX, CSV)
    ASSEMBLY_DRAWING = 4,   ///< Assembly drawing (PDF)
    SCHEMATIC_PDF = 5,      ///< Schematic printout (PDF)
    RENDER_3D = 6,          ///< 3D render (PNG/STEP)
    IPC2581 = 7,            ///< IPC-2581 XML package
    ODB_PLUS_PLUS = 8,      ///< ODB++ archive
    STEP_MODEL = 9,         ///< STEP 3D model export
    PCB_PDF = 10,           ///< PCB layout PDF printout
    DRC_REPORT = 11,        ///< Design Rule Check report
    ERC_REPORT = 12,        ///< Electrical Rule Check report
    VISUAL_DIFF = 13,       ///< Visual diff images
    ARCHIVE = 14,           ///< Combined archive (zip of multiple outputs)
    CUSTOM = 100            ///< Plugin-specific artifact type
};


/**
 * Version numbering scheme for releases.
 */
enum class PLM_VERSION_SCHEME
{
    SEMVER = 0,         ///< Semantic versioning (major.minor.patch)
    CALVER = 1,         ///< Calendar versioning (YYYY.MM.DD)
    SEQUENTIAL = 2,     ///< Simple sequential numbering (Rev A, Rev B, ...)
    CUSTOM = 3          ///< Plugin-defined scheme
};


/**
 * A manufacturing artifact within a release package.
 */
struct PLM_RELEASE_ARTIFACT
{
    wxString            artifact_id;
    PLM_ARTIFACT_TYPE   type = PLM_ARTIFACT_TYPE::CUSTOM;
    wxString            file_name;      ///< Original file name
    wxString            file_path;      ///< Local file path (for upload)
    wxString            download_url;   ///< URL to download (after upload)
    size_t              size_bytes = 0;
    wxString            checksum;       ///< SHA-256 hash

    PLM_RELEASE_ARTIFACT() = default;

    PLM_RELEASE_ARTIFACT( PLM_ARTIFACT_TYPE aType, const wxString& aFileName ) :
            type( aType ),
            file_name( aFileName )
    {
    }
};


/**
 * A manufacturing release package.
 *
 * Groups related artifacts (Gerbers, BOM, assembly drawings) into
 * a single release unit that can be sent to a manufacturer.
 */
struct PLM_RELEASE_PACKAGE
{
    wxString                            package_id;
    wxString                            name;
    wxString                            description;
    std::vector<PLM_RELEASE_ARTIFACT>   artifacts;

    PLM_RELEASE_PACKAGE() = default;

    PLM_RELEASE_PACKAGE( const wxString& aId, const wxString& aName ) :
            package_id( aId ),
            name( aName )
    {
    }
};


/**
 * A pre-release validation check result.
 */
struct PLM_RELEASE_CHECK
{
    wxString    check_id;
    wxString    name;           ///< Check name (e.g., "DRC", "BOM Validation")
    bool        passed = false;
    wxString    message;        ///< Result message
    wxString    details_url;    ///< URL to detailed report

    PLM_RELEASE_CHECK() = default;

    PLM_RELEASE_CHECK( const wxString& aId, const wxString& aName, bool aPassed,
                       const wxString& aMessage = wxEmptyString ) :
            check_id( aId ),
            name( aName ),
            passed( aPassed ),
            message( aMessage )
    {
    }
};


/**
 * A formal design release record.
 */
struct PLM_RELEASE
{
    wxString                        release_id;
    wxString                        project_id;
    wxString                        version;        ///< Version string (e.g., "1.0.0")
    wxString                        revision_id;    ///< Revision this release was built from
    PLM_RELEASE_STATUS              status = PLM_RELEASE_STATUS::DRAFT;
    PLM_VERSION_SCHEME              version_scheme = PLM_VERSION_SCHEME::SEMVER;
    wxString                        title;
    wxString                        notes;          ///< Release notes (markdown)
    std::vector<PLM_RELEASE_PACKAGE> packages;
    std::vector<PLM_RELEASE_CHECK>  checks;         ///< Pre-release validation results
    wxString                        released_by;
    int64_t                         created_at = 0;
    int64_t                         released_at = 0;
    wxString                        web_url;        ///< URL to release in web UI

    PLM_RELEASE() = default;

    bool IsFinalized() const { return status == PLM_RELEASE_STATUS::RELEASED; }

    bool AllChecksPassed() const
    {
        for( const auto& check : checks )
        {
            if( !check.passed )
                return false;
        }

        return true;
    }

    /**
     * Get human-readable status name.
     */
    wxString GetStatusName() const
    {
        switch( status )
        {
        case PLM_RELEASE_STATUS::DRAFT:       return wxS( "Draft" );
        case PLM_RELEASE_STATUS::VALIDATING:  return wxS( "Validating" );
        case PLM_RELEASE_STATUS::READY:       return wxS( "Ready" );
        case PLM_RELEASE_STATUS::RELEASED:    return wxS( "Released" );
        case PLM_RELEASE_STATUS::SUPERSEDED:  return wxS( "Superseded" );
        case PLM_RELEASE_STATUS::RECALLED:    return wxS( "Recalled" );
        default:                              return wxS( "Unknown" );
        }
    }
};


/**
 * Options for creating a new release.
 */
struct PLM_RELEASE_OPTIONS
{
    wxString            version;        ///< Version string
    PLM_VERSION_SCHEME  version_scheme = PLM_VERSION_SCHEME::SEMVER;
    wxString            title;
    wxString            notes;          ///< Release notes (markdown)
    bool                draft = false;  ///< Create as draft
    bool                auto_generate_artifacts = true; ///< Auto-generate manufacturing outputs

    PLM_RELEASE_OPTIONS() = default;

    PLM_RELEASE_OPTIONS( const wxString& aVersion, const wxString& aTitle ) :
            version( aVersion ),
            title( aTitle )
    {
    }
};


/**
 * Result of a release operation.
 */
struct PLM_RELEASE_RESULT
{
    bool            success = false;
    PLM_ERROR_CODE  error_code = PLM_ERROR_CODE::SUCCESS;
    wxString        error_message;
    PLM_RELEASE     release;

    PLM_RELEASE_RESULT() = default;

    static PLM_RELEASE_RESULT Success( const PLM_RELEASE& aRelease )
    {
        PLM_RELEASE_RESULT r;
        r.success = true;
        r.release = aRelease;
        return r;
    }

    static PLM_RELEASE_RESULT Failure( PLM_ERROR_CODE aCode, const wxString& aMessage )
    {
        PLM_RELEASE_RESULT r;
        r.error_code = aCode;
        r.error_message = aMessage;
        return r;
    }
};


/**
 * Deviation branch metadata for field issue resolution.
 *
 * A deviation branch allows modification of a released design to
 * address field issues without going through the full release cycle.
 */
struct PLM_DEVIATION_BRANCH
{
    wxString    branch_id;
    wxString    branch_name;
    wxString    release_id;     ///< Release this deviation is based on
    wxString    project_id;
    wxString    reason;         ///< Reason for the deviation
    wxString    created_by;
    int64_t     created_at = 0;

    PLM_DEVIATION_BRANCH() = default;

    PLM_DEVIATION_BRANCH( const wxString& aId, const wxString& aName,
                          const wxString& aReleaseId ) :
            branch_id( aId ),
            branch_name( aName ),
            release_id( aReleaseId )
    {
    }
};


#endif // PLM_RELEASE_H
