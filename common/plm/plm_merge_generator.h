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
 * @file plm_merge_generator.h
 * @brief Generates merged KiCad files from merge plans.
 */

#ifndef PLM_MERGE_GENERATOR_H
#define PLM_MERGE_GENERATOR_H

#include <kicommon.h>
#include "plm_merge_algorithm.h"
#include "plm_diff_schematic.h"
#include "plm_diff_pcb.h"

#include <wx/string.h>

#include <functional>
#include <map>
#include <set>
#include <vector>


/**
 * Result of applying a merge plan.
 */
struct PLM_MERGE_RESULT
{
    bool     success;               ///< True if merge succeeded
    wxString error;                 ///< Error message if failed
    wxString merged_schematic;      ///< Path to merged schematic
    wxString merged_pcb;            ///< Path to merged PCB
    size_t   elements_merged;       ///< Total elements processed
    size_t   local_changes_applied; ///< Changes from local applied
    size_t   server_changes_applied; ///< Changes from server applied

    PLM_MERGE_RESULT() :
            success( false ),
            elements_merged( 0 ),
            local_changes_applied( 0 ),
            server_changes_applied( 0 )
    {
    }
};


/**
 * Progress callback for merge operation.
 *
 * @param current Current element being processed.
 * @param total Total elements to process.
 * @param description Description of current operation.
 * @return True to continue, false to cancel.
 */
using PLM_MERGE_PROGRESS_CALLBACK = std::function<bool( size_t current, size_t total,
                                                         const wxString& description )>;


/**
 * Options for merge result generation.
 */
struct PLM_MERGE_GENERATOR_OPTIONS
{
    bool preserve_uuid_on_conflict = true;   ///< Keep base UUID when conflicting
    bool backup_originals = true;            ///< Create backup of source files
    bool validate_output = true;             ///< Validate generated files
    bool generate_report = false;            ///< Generate merge report file
    wxString report_path;                    ///< Path for merge report

    PLM_MERGE_PROGRESS_CALLBACK progress_callback; ///< Progress callback
};


/**
 * Generates merged KiCad files from merge plans.
 *
 * Takes a PLM_MERGE_PLAN (with all conflicts resolved) and applies
 * the merge actions to produce valid KiCad schematic and PCB files.
 *
 * Usage:
 * @code
 * PLM_MERGE_GENERATOR generator;
 *
 * generator.SetBaseFiles( "base.kicad_sch", "base.kicad_pcb" );
 * generator.SetLocalFiles( "local.kicad_sch", "local.kicad_pcb" );
 * generator.SetServerFiles( "server.kicad_sch", "server.kicad_pcb" );
 *
 * PLM_MERGE_PLAN plan = ...;  // From PLM_MERGE_ALGORITHM
 *
 * PLM_MERGE_RESULT result = generator.ApplyMergePlan(
 *     plan, "merged.kicad_sch", "merged.kicad_pcb" );
 *
 * if( !result.success )
 *     wxLogError( "Merge failed: %s", result.error );
 * @endcode
 */
class KICOMMON_API PLM_MERGE_GENERATOR
{
public:
    PLM_MERGE_GENERATOR();
    ~PLM_MERGE_GENERATOR();

    /**
     * Set base (common ancestor) files.
     * @param aSchematic Base schematic file.
     * @param aPCB Base PCB file (optional).
     */
    void SetBaseFiles( const wxString& aSchematic, const wxString& aPCB = wxEmptyString );

    /**
     * Set local (user's) files.
     * @param aSchematic Local schematic file.
     * @param aPCB Local PCB file (optional).
     */
    void SetLocalFiles( const wxString& aSchematic, const wxString& aPCB = wxEmptyString );

    /**
     * Set server (remote) files.
     * @param aSchematic Server schematic file.
     * @param aPCB Server PCB file (optional).
     */
    void SetServerFiles( const wxString& aSchematic, const wxString& aPCB = wxEmptyString );

    /**
     * Set generation options.
     * @param aOptions Generation options.
     */
    void SetOptions( const PLM_MERGE_GENERATOR_OPTIONS& aOptions ) { m_options = aOptions; }

    /**
     * Get current options.
     * @return Generation options.
     */
    const PLM_MERGE_GENERATOR_OPTIONS& GetOptions() const { return m_options; }

    /**
     * Apply a merge plan and generate output files.
     *
     * @param aPlan Merge plan to apply (must have all conflicts resolved).
     * @param aOutputSchematic Path for merged schematic output.
     * @param aOutputPCB Path for merged PCB output (optional).
     * @return Merge result with status and paths.
     */
    PLM_MERGE_RESULT ApplyMergePlan( const PLM_MERGE_PLAN& aPlan,
                                      const wxString& aOutputSchematic,
                                      const wxString& aOutputPCB = wxEmptyString );

    /**
     * Cherry-pick specific actions from a merge plan.
     *
     * Applies only the specified actions to the base files.
     *
     * @param aPlan Source merge plan.
     * @param aActionIndices Indices of actions to apply.
     * @param aOutputSchematic Path for output schematic.
     * @param aOutputPCB Path for output PCB (optional).
     * @return Merge result.
     */
    PLM_MERGE_RESULT CherryPick( const PLM_MERGE_PLAN& aPlan,
                                  const std::vector<size_t>& aActionIndices,
                                  const wxString& aOutputSchematic,
                                  const wxString& aOutputPCB = wxEmptyString );

    /**
     * Validate merge plan can be applied.
     *
     * @param aPlan Merge plan to validate.
     * @return Empty string if valid, error message if invalid.
     */
    wxString ValidatePlan( const PLM_MERGE_PLAN& aPlan );

    /**
     * Generate a merge report.
     *
     * @param aPlan Merge plan to report on.
     * @param aResult Merge result.
     * @param aReportPath Path for report file.
     * @return True if report generated successfully.
     */
    bool GenerateReport( const PLM_MERGE_PLAN& aPlan, const PLM_MERGE_RESULT& aResult,
                         const wxString& aReportPath );

    /**
     * Check if last operation had an error.
     * @return True if error occurred.
     */
    bool HasError() const { return !m_lastError.IsEmpty(); }

    /**
     * Get the last error message.
     * @return Error message.
     */
    wxString GetLastError() const { return m_lastError; }

private:
    /**
     * Load and parse all source files.
     * @return True if all files loaded successfully.
     */
    bool LoadSourceFiles();

    /**
     * Generate merged schematic content.
     *
     * @param aPlan Merge plan to apply.
     * @param aActions Actions to apply (for cherry-pick, empty for all).
     * @return Merged schematic content.
     */
    wxString GenerateMergedSchematic( const PLM_MERGE_PLAN& aPlan,
                                       const std::set<size_t>& aActions );

    /**
     * Generate merged PCB content.
     *
     * @param aPlan Merge plan to apply.
     * @param aActions Actions to apply (for cherry-pick, empty for all).
     * @return Merged PCB content.
     */
    wxString GenerateMergedPCB( const PLM_MERGE_PLAN& aPlan, const std::set<size_t>& aActions );

    /**
     * Apply a single merge action to schematic content.
     *
     * @param aContent Current content.
     * @param aAction Action to apply.
     * @return Modified content.
     */
    wxString ApplySchematicAction( const wxString& aContent, const PLM_MERGE_ACTION& aAction );

    /**
     * Apply a single merge action to PCB content.
     *
     * @param aContent Current content.
     * @param aAction Action to apply.
     * @return Modified content.
     */
    wxString ApplyPCBAction( const wxString& aContent, const PLM_MERGE_ACTION& aAction );

    /**
     * Find element in S-expression content by UUID.
     *
     * @param aContent S-expression content.
     * @param aUuid UUID to find.
     * @return Start position or wxNOT_FOUND.
     */
    int FindElementByUuid( const wxString& aContent, const wxString& aUuid );

    /**
     * Extract element bounds in S-expression.
     *
     * @param aContent S-expression content.
     * @param aStartPos Start position.
     * @return End position of element.
     */
    int FindElementEnd( const wxString& aContent, int aStartPos );

    /**
     * Replace property value in element.
     *
     * @param aContent Element content.
     * @param aProperty Property name.
     * @param aNewValue New value.
     * @return Modified content.
     */
    wxString ReplaceProperty( const wxString& aContent, const wxString& aProperty,
                               const wxString& aNewValue );

    /**
     * Get element content from source file.
     *
     * @param aUuid Element UUID.
     * @param aFromLocal True for local, false for server.
     * @param aForSchematic True for schematic, false for PCB.
     * @return Element S-expression content.
     */
    wxString GetElementFromSource( const wxString& aUuid, bool aFromLocal, bool aForSchematic );

    /**
     * Create backup of source files.
     * @return True if backups created successfully.
     */
    bool CreateBackups();

    /**
     * Validate generated output file.
     *
     * @param aPath Output file path.
     * @param aIsSchematic True if schematic, false if PCB.
     * @return True if valid.
     */
    bool ValidateOutput( const wxString& aPath, bool aIsSchematic );

    /**
     * Report progress if callback is set.
     *
     * @param aCurrent Current item.
     * @param aTotal Total items.
     * @param aDescription Description.
     * @return True to continue, false if cancelled.
     */
    bool ReportProgress( size_t aCurrent, size_t aTotal, const wxString& aDescription );

    wxString m_baseSchematic;
    wxString m_basePCB;
    wxString m_localSchematic;
    wxString m_localPCB;
    wxString m_serverSchematic;
    wxString m_serverPCB;

    wxString m_baseSchematicContent;
    wxString m_basePCBContent;
    wxString m_localSchematicContent;
    wxString m_localPCBContent;
    wxString m_serverSchematicContent;
    wxString m_serverPCBContent;

    PLM_MERGE_GENERATOR_OPTIONS m_options;
    wxString                    m_lastError;
};


#endif // PLM_MERGE_GENERATOR_H
