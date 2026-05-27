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
 * @file plm_diff_analyzer.h
 * @brief Component change detection and analysis for PLM merge operations.
 */

#ifndef PLM_DIFF_ANALYZER_H
#define PLM_DIFF_ANALYZER_H

#include <kicommon.h>
#include "plm_diff_schematic.h"
#include "plm_diff_pcb.h"

#include <wx/string.h>

#include <cstdint>
#include <map>
#include <set>
#include <vector>


/**
 * Type of component change.
 */
enum class PLM_COMPONENT_CHANGE_TYPE
{
    ADDED,           ///< New component added
    REMOVED,         ///< Component removed
    MOVED,           ///< Component repositioned
    VALUE_CHANGED,   ///< Value property changed
    FOOTPRINT_CHANGED, ///< Footprint assignment changed
    REFERENCE_CHANGED, ///< Reference designator changed
    PROPERTY_CHANGED,  ///< Other property changed
    CONNECTIVITY_CHANGED ///< Net connections changed
};


/**
 * Severity of a change for review prioritization.
 */
enum class PLM_CHANGE_SEVERITY
{
    INFO,       ///< Informational, cosmetic changes
    LOW,        ///< Minor changes, low impact
    MEDIUM,     ///< Moderate changes, review recommended
    HIGH,       ///< Significant changes, careful review needed
    CRITICAL    ///< Breaking changes, must review
};


/**
 * A unified component across schematic and PCB.
 */
struct PLM_UNIFIED_COMPONENT
{
    wxString                     reference;      ///< Reference designator
    wxString                     value;          ///< Component value
    wxString                     footprint;      ///< Footprint assignment
    wxString                     lib_id_sch;     ///< Schematic symbol library ID
    wxString                     lib_id_pcb;     ///< PCB footprint library ID
    wxString                     sch_uuid;       ///< Schematic symbol UUID
    wxString                     pcb_uuid;       ///< PCB footprint UUID
    std::map<wxString, wxString> properties;     ///< All properties
    std::set<wxString>           nets;           ///< Connected net names
    bool                         in_schematic;   ///< Present in schematic
    bool                         in_pcb;         ///< Present in PCB

    PLM_UNIFIED_COMPONENT() : in_schematic( false ), in_pcb( false ) {}

    bool IsComplete() const { return in_schematic && in_pcb; }
    bool IsOrphan() const { return in_schematic != in_pcb; }
};


/**
 * A detected component change.
 */
struct PLM_COMPONENT_CHANGE
{
    PLM_COMPONENT_CHANGE_TYPE  type;             ///< Type of change
    PLM_CHANGE_SEVERITY        severity;         ///< Change severity
    wxString                   reference;        ///< Component reference
    wxString                   description;      ///< Human-readable description
    wxString                   old_value;        ///< Previous value
    wxString                   new_value;        ///< New value
    wxString                   affected_file;    ///< File containing change
    bool                       affects_schematic; ///< Change in schematic
    bool                       affects_pcb;      ///< Change in PCB

    PLM_COMPONENT_CHANGE() :
            type( PLM_COMPONENT_CHANGE_TYPE::ADDED ),
            severity( PLM_CHANGE_SEVERITY::INFO ),
            affects_schematic( false ),
            affects_pcb( false )
    {
    }
};


/**
 * Statistics about detected changes.
 */
struct PLM_CHANGE_STATISTICS
{
    size_t components_added = 0;
    size_t components_removed = 0;
    size_t components_moved = 0;
    size_t components_modified = 0;
    size_t connectivity_changes = 0;
    size_t schematic_only_changes = 0;
    size_t pcb_only_changes = 0;
    size_t critical_changes = 0;
    size_t high_severity_changes = 0;
    int64_t analysis_time_ms = 0;

    size_t GetTotalChanges() const
    {
        return components_added + components_removed + components_moved +
               components_modified + connectivity_changes;
    }
};


/**
 * Complete analysis result.
 */
struct PLM_DIFF_ANALYSIS_RESULT
{
    std::vector<PLM_COMPONENT_CHANGE>  changes;          ///< All detected changes
    std::vector<PLM_UNIFIED_COMPONENT> base_components;  ///< Components in base
    std::vector<PLM_UNIFIED_COMPONENT> new_components;   ///< Components in new
    PLM_CHANGE_STATISTICS              statistics;       ///< Change statistics
    bool                               success;          ///< Analysis succeeded
    wxString                           error;            ///< Error message

    std::vector<PLM_COMPONENT_CHANGE> GetChangesByType( PLM_COMPONENT_CHANGE_TYPE aType ) const
    {
        std::vector<PLM_COMPONENT_CHANGE> result;

        for( const auto& c : changes )
        {
            if( c.type == aType )
                result.push_back( c );
        }

        return result;
    }

    std::vector<PLM_COMPONENT_CHANGE> GetChangesBySeverity( PLM_CHANGE_SEVERITY aSeverity ) const
    {
        std::vector<PLM_COMPONENT_CHANGE> result;

        for( const auto& c : changes )
        {
            if( c.severity >= aSeverity )
                result.push_back( c );
        }

        return result;
    }

    std::vector<PLM_COMPONENT_CHANGE> GetChangesForComponent( const wxString& aRef ) const
    {
        std::vector<PLM_COMPONENT_CHANGE> result;

        for( const auto& c : changes )
        {
            if( c.reference == aRef )
                result.push_back( c );
        }

        return result;
    }
};


/**
 * Configuration for diff analysis.
 */
struct PLM_DIFF_ANALYZER_CONFIG
{
    bool track_moves = true;            ///< Detect component moves
    bool track_property_changes = true; ///< Track property modifications
    bool track_connectivity = true;     ///< Track net connection changes
    bool ignore_cosmetic = false;       ///< Ignore cosmetic-only changes
    int  move_threshold_nm = 100000;    ///< Minimum move distance to report (nm)
};


/**
 * Component change detection and analysis.
 *
 * Analyzes differences between schematic and PCB revisions to identify
 * component-level changes. Combines results from schematic and PCB diff
 * parsers to provide unified change detection across the design.
 *
 * Usage:
 * @code
 * PLM_DIFF_ANALYZER analyzer;
 *
 * // Set file paths
 * analyzer.SetBaseSchematic( "base.kicad_sch" );
 * analyzer.SetNewSchematic( "new.kicad_sch" );
 * analyzer.SetBasePCB( "base.kicad_pcb" );
 * analyzer.SetNewPCB( "new.kicad_pcb" );
 *
 * // Run analysis
 * PLM_DIFF_ANALYSIS_RESULT result = analyzer.Analyze();
 *
 * // Review changes
 * for( const auto& change : result.changes )
 *     wxLogMessage( "%s: %s - %s", change.reference,
 *                   GetChangeTypeName( change.type ), change.description );
 * @endcode
 */
class KICOMMON_API PLM_DIFF_ANALYZER
{
public:
    PLM_DIFF_ANALYZER();
    ~PLM_DIFF_ANALYZER();

    /**
     * Set base (older) schematic file.
     * @param aFilename Path to base schematic.
     */
    void SetBaseSchematic( const wxString& aFilename ) { m_baseSchematic = aFilename; }

    /**
     * Set new schematic file.
     * @param aFilename Path to new schematic.
     */
    void SetNewSchematic( const wxString& aFilename ) { m_newSchematic = aFilename; }

    /**
     * Set base (older) PCB file.
     * @param aFilename Path to base PCB.
     */
    void SetBasePCB( const wxString& aFilename ) { m_basePCB = aFilename; }

    /**
     * Set new PCB file.
     * @param aFilename Path to new PCB.
     */
    void SetNewPCB( const wxString& aFilename ) { m_newPCB = aFilename; }

    /**
     * Set analyzer configuration.
     * @param aConfig Configuration options.
     */
    void SetConfig( const PLM_DIFF_ANALYZER_CONFIG& aConfig ) { m_config = aConfig; }

    /**
     * Get current configuration.
     * @return Configuration options.
     */
    const PLM_DIFF_ANALYZER_CONFIG& GetConfig() const { return m_config; }

    /**
     * Run the diff analysis.
     *
     * Parses all specified files and performs comprehensive change detection.
     *
     * @return Analysis result with all detected changes.
     */
    PLM_DIFF_ANALYSIS_RESULT Analyze();

    /**
     * Analyze schematic changes only.
     *
     * @return Analysis result for schematic.
     */
    PLM_DIFF_ANALYSIS_RESULT AnalyzeSchematic();

    /**
     * Analyze PCB changes only.
     *
     * @return Analysis result for PCB.
     */
    PLM_DIFF_ANALYSIS_RESULT AnalyzePCB();

    /**
     * Get human-readable name for change type.
     * @param aType Change type.
     * @return Display name.
     */
    static wxString GetChangeTypeName( PLM_COMPONENT_CHANGE_TYPE aType );

    /**
     * Get human-readable name for severity.
     * @param aSeverity Severity level.
     * @return Display name.
     */
    static wxString GetSeverityName( PLM_CHANGE_SEVERITY aSeverity );

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
     * Build unified component list from schematic data.
     * @param aSchData Schematic data.
     * @return Map of reference to unified component.
     */
    std::map<wxString, PLM_UNIFIED_COMPONENT> BuildComponentMapFromSchematic(
            const PLM_SCH_DATA& aSchData );

    /**
     * Merge PCB data into unified component map.
     * @param aComponents Component map to update.
     * @param aPcbData PCB data.
     */
    void MergePCBData( std::map<wxString, PLM_UNIFIED_COMPONENT>& aComponents,
                       const PLM_PCB_DATA& aPcbData );

    /**
     * Compare two component sets and generate changes.
     * @param aBase Base components.
     * @param aNew New components.
     * @param aChanges Output change list.
     */
    void CompareComponents( const std::map<wxString, PLM_UNIFIED_COMPONENT>& aBase,
                            const std::map<wxString, PLM_UNIFIED_COMPONENT>& aNew,
                            std::vector<PLM_COMPONENT_CHANGE>& aChanges );

    /**
     * Determine severity for a change type.
     * @param aType Change type.
     * @param aOldValue Old value.
     * @param aNewValue New value.
     * @return Appropriate severity level.
     */
    PLM_CHANGE_SEVERITY DetermineSeverity( PLM_COMPONENT_CHANGE_TYPE aType,
                                            const wxString& aOldValue,
                                            const wxString& aNewValue );

    /**
     * Process schematic diff results into component changes.
     * @param aDiff Schematic diff result.
     * @param aChanges Output change list.
     */
    void ProcessSchematicDiff( const PLM_SCH_DIFF_RESULT& aDiff,
                               std::vector<PLM_COMPONENT_CHANGE>& aChanges );

    /**
     * Process PCB diff results into component changes.
     * @param aDiff PCB diff result.
     * @param aChanges Output change list.
     */
    void ProcessPCBDiff( const PLM_PCB_DIFF_RESULT& aDiff,
                         std::vector<PLM_COMPONENT_CHANGE>& aChanges );

    /**
     * Calculate change statistics.
     * @param aChanges Change list.
     * @return Statistics.
     */
    PLM_CHANGE_STATISTICS CalculateStatistics(
            const std::vector<PLM_COMPONENT_CHANGE>& aChanges );

    wxString                  m_baseSchematic;
    wxString                  m_newSchematic;
    wxString                  m_basePCB;
    wxString                  m_newPCB;
    PLM_DIFF_ANALYZER_CONFIG  m_config;
    wxString                  m_lastError;
};


#endif // PLM_DIFF_ANALYZER_H
