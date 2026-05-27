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

#include "plm_diff_analyzer.h"

#include <wx/log.h>

#include <chrono>


PLM_DIFF_ANALYZER::PLM_DIFF_ANALYZER()
{
}


PLM_DIFF_ANALYZER::~PLM_DIFF_ANALYZER()
{
}


PLM_DIFF_ANALYSIS_RESULT PLM_DIFF_ANALYZER::Analyze()
{
    PLM_DIFF_ANALYSIS_RESULT result;
    result.success = true;
    m_lastError.clear();

    auto startTime = std::chrono::high_resolution_clock::now();

    // Parse schematic files if provided
    PLM_SCH_DATA baseSch, newSch;
    bool hasSchematic = !m_baseSchematic.IsEmpty() && !m_newSchematic.IsEmpty();

    if( hasSchematic )
    {
        PLM_SCHEMATIC_DIFF_PARSER schParser;
        baseSch = schParser.ParseSchematicHierarchy( m_baseSchematic );

        if( schParser.HasError() )
        {
            m_lastError = schParser.GetLastError();
            result.success = false;
            result.error = m_lastError;
            return result;
        }

        newSch = schParser.ParseSchematicHierarchy( m_newSchematic );

        if( schParser.HasError() )
        {
            m_lastError = schParser.GetLastError();
            result.success = false;
            result.error = m_lastError;
            return result;
        }
    }

    // Parse PCB files if provided
    PLM_PCB_DATA basePcb, newPcb;
    bool hasPCB = !m_basePCB.IsEmpty() && !m_newPCB.IsEmpty();

    if( hasPCB )
    {
        PLM_PCB_DIFF_PARSER pcbParser;
        basePcb = pcbParser.ParsePCB( m_basePCB );

        if( pcbParser.HasError() )
        {
            m_lastError = pcbParser.GetLastError();
            result.success = false;
            result.error = m_lastError;
            return result;
        }

        newPcb = pcbParser.ParsePCB( m_newPCB );

        if( pcbParser.HasError() )
        {
            m_lastError = pcbParser.GetLastError();
            result.success = false;
            result.error = m_lastError;
            return result;
        }
    }

    // Build unified component maps
    std::map<wxString, PLM_UNIFIED_COMPONENT> baseComponents;
    std::map<wxString, PLM_UNIFIED_COMPONENT> newComponents;

    if( hasSchematic )
    {
        baseComponents = BuildComponentMapFromSchematic( baseSch );
        newComponents = BuildComponentMapFromSchematic( newSch );
    }

    if( hasPCB )
    {
        MergePCBData( baseComponents, basePcb );
        MergePCBData( newComponents, newPcb );
    }

    // Convert maps to vectors for result
    for( const auto& [ref, comp] : baseComponents )
        result.base_components.push_back( comp );

    for( const auto& [ref, comp] : newComponents )
        result.new_components.push_back( comp );

    // Compare components to generate changes
    CompareComponents( baseComponents, newComponents, result.changes );

    // Process schematic-specific changes
    if( hasSchematic )
    {
        PLM_SCHEMATIC_DIFF_PARSER schParser;
        PLM_SCH_DIFF_RESULT schDiff = schParser.ComputeDiff( baseSch, newSch );
        ProcessSchematicDiff( schDiff, result.changes );
    }

    // Process PCB-specific changes
    if( hasPCB )
    {
        PLM_PCB_DIFF_PARSER pcbParser;
        PLM_PCB_DIFF_RESULT pcbDiff = pcbParser.ComputeDiff( basePcb, newPcb );
        ProcessPCBDiff( pcbDiff, result.changes );
    }

    // Calculate statistics
    auto endTime = std::chrono::high_resolution_clock::now();
    result.statistics = CalculateStatistics( result.changes );
    result.statistics.analysis_time_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>( endTime - startTime ).count();

    return result;
}


PLM_DIFF_ANALYSIS_RESULT PLM_DIFF_ANALYZER::AnalyzeSchematic()
{
    PLM_DIFF_ANALYSIS_RESULT result;
    result.success = true;
    m_lastError.clear();

    if( m_baseSchematic.IsEmpty() || m_newSchematic.IsEmpty() )
    {
        m_lastError = wxS( "Base and new schematic files must be specified" );
        result.success = false;
        result.error = m_lastError;
        return result;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    PLM_SCHEMATIC_DIFF_PARSER parser;

    PLM_SCH_DATA baseSch = parser.ParseSchematicHierarchy( m_baseSchematic );

    if( parser.HasError() )
    {
        m_lastError = parser.GetLastError();
        result.success = false;
        result.error = m_lastError;
        return result;
    }

    PLM_SCH_DATA newSch = parser.ParseSchematicHierarchy( m_newSchematic );

    if( parser.HasError() )
    {
        m_lastError = parser.GetLastError();
        result.success = false;
        result.error = m_lastError;
        return result;
    }

    // Build component maps
    std::map<wxString, PLM_UNIFIED_COMPONENT> baseComponents =
            BuildComponentMapFromSchematic( baseSch );
    std::map<wxString, PLM_UNIFIED_COMPONENT> newComponents =
            BuildComponentMapFromSchematic( newSch );

    // Convert to vectors
    for( const auto& [ref, comp] : baseComponents )
        result.base_components.push_back( comp );

    for( const auto& [ref, comp] : newComponents )
        result.new_components.push_back( comp );

    // Compare and generate changes
    CompareComponents( baseComponents, newComponents, result.changes );

    PLM_SCH_DIFF_RESULT schDiff = parser.ComputeDiff( baseSch, newSch );
    ProcessSchematicDiff( schDiff, result.changes );

    // Mark all changes as schematic-only
    for( auto& change : result.changes )
    {
        change.affects_schematic = true;
        change.affects_pcb = false;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.statistics = CalculateStatistics( result.changes );
    result.statistics.analysis_time_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>( endTime - startTime ).count();
    result.statistics.schematic_only_changes = result.changes.size();

    return result;
}


PLM_DIFF_ANALYSIS_RESULT PLM_DIFF_ANALYZER::AnalyzePCB()
{
    PLM_DIFF_ANALYSIS_RESULT result;
    result.success = true;
    m_lastError.clear();

    if( m_basePCB.IsEmpty() || m_newPCB.IsEmpty() )
    {
        m_lastError = wxS( "Base and new PCB files must be specified" );
        result.success = false;
        result.error = m_lastError;
        return result;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    PLM_PCB_DIFF_PARSER parser;

    PLM_PCB_DATA basePcb = parser.ParsePCB( m_basePCB );

    if( parser.HasError() )
    {
        m_lastError = parser.GetLastError();
        result.success = false;
        result.error = m_lastError;
        return result;
    }

    PLM_PCB_DATA newPcb = parser.ParsePCB( m_newPCB );

    if( parser.HasError() )
    {
        m_lastError = parser.GetLastError();
        result.success = false;
        result.error = m_lastError;
        return result;
    }

    // Build component maps from PCB
    std::map<wxString, PLM_UNIFIED_COMPONENT> baseComponents;
    std::map<wxString, PLM_UNIFIED_COMPONENT> newComponents;

    MergePCBData( baseComponents, basePcb );
    MergePCBData( newComponents, newPcb );

    // Convert to vectors
    for( const auto& [ref, comp] : baseComponents )
        result.base_components.push_back( comp );

    for( const auto& [ref, comp] : newComponents )
        result.new_components.push_back( comp );

    // Compare and generate changes
    CompareComponents( baseComponents, newComponents, result.changes );

    PLM_PCB_DIFF_RESULT pcbDiff = parser.ComputeDiff( basePcb, newPcb );
    ProcessPCBDiff( pcbDiff, result.changes );

    // Mark all changes as PCB-only
    for( auto& change : result.changes )
    {
        change.affects_schematic = false;
        change.affects_pcb = true;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.statistics = CalculateStatistics( result.changes );
    result.statistics.analysis_time_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>( endTime - startTime ).count();
    result.statistics.pcb_only_changes = result.changes.size();

    return result;
}


wxString PLM_DIFF_ANALYZER::GetChangeTypeName( PLM_COMPONENT_CHANGE_TYPE aType )
{
    switch( aType )
    {
    case PLM_COMPONENT_CHANGE_TYPE::ADDED:             return wxS( "Added" );
    case PLM_COMPONENT_CHANGE_TYPE::REMOVED:           return wxS( "Removed" );
    case PLM_COMPONENT_CHANGE_TYPE::MOVED:             return wxS( "Moved" );
    case PLM_COMPONENT_CHANGE_TYPE::VALUE_CHANGED:     return wxS( "Value Changed" );
    case PLM_COMPONENT_CHANGE_TYPE::FOOTPRINT_CHANGED: return wxS( "Footprint Changed" );
    case PLM_COMPONENT_CHANGE_TYPE::REFERENCE_CHANGED: return wxS( "Reference Changed" );
    case PLM_COMPONENT_CHANGE_TYPE::PROPERTY_CHANGED:  return wxS( "Property Changed" );
    case PLM_COMPONENT_CHANGE_TYPE::CONNECTIVITY_CHANGED: return wxS( "Connectivity Changed" );
    default: return wxS( "Unknown" );
    }
}


wxString PLM_DIFF_ANALYZER::GetSeverityName( PLM_CHANGE_SEVERITY aSeverity )
{
    switch( aSeverity )
    {
    case PLM_CHANGE_SEVERITY::INFO:     return wxS( "Info" );
    case PLM_CHANGE_SEVERITY::LOW:      return wxS( "Low" );
    case PLM_CHANGE_SEVERITY::MEDIUM:   return wxS( "Medium" );
    case PLM_CHANGE_SEVERITY::HIGH:     return wxS( "High" );
    case PLM_CHANGE_SEVERITY::CRITICAL: return wxS( "Critical" );
    default: return wxS( "Unknown" );
    }
}


std::map<wxString, PLM_UNIFIED_COMPONENT> PLM_DIFF_ANALYZER::BuildComponentMapFromSchematic(
        const PLM_SCH_DATA& aSchData )
{
    std::map<wxString, PLM_UNIFIED_COMPONENT> components;

    for( const auto& sheet : aSchData.sheets )
    {
        for( const auto& sym : sheet.symbols )
        {
            if( sym.reference.IsEmpty() )
                continue;

            PLM_UNIFIED_COMPONENT& comp = components[sym.reference];
            comp.reference = sym.reference;
            comp.value = sym.value;
            comp.footprint = sym.footprint;
            comp.lib_id_sch = sym.lib_id;
            comp.sch_uuid = sym.uuid;
            comp.in_schematic = true;

            for( const auto& [key, val] : sym.properties )
                comp.properties[key] = val;
        }
    }

    return components;
}


void PLM_DIFF_ANALYZER::MergePCBData( std::map<wxString, PLM_UNIFIED_COMPONENT>& aComponents,
                                       const PLM_PCB_DATA& aPcbData )
{
    for( const auto& fp : aPcbData.footprints )
    {
        if( fp.reference.IsEmpty() )
            continue;

        PLM_UNIFIED_COMPONENT& comp = aComponents[fp.reference];
        comp.reference = fp.reference;

        if( comp.value.IsEmpty() )
            comp.value = fp.value;

        comp.lib_id_pcb = fp.lib_id;
        comp.pcb_uuid = fp.uuid;
        comp.in_pcb = true;

        for( const auto& [key, val] : fp.properties )
        {
            if( comp.properties.find( key ) == comp.properties.end() )
                comp.properties[key] = val;
        }
    }
}


void PLM_DIFF_ANALYZER::CompareComponents(
        const std::map<wxString, PLM_UNIFIED_COMPONENT>& aBase,
        const std::map<wxString, PLM_UNIFIED_COMPONENT>& aNew,
        std::vector<PLM_COMPONENT_CHANGE>& aChanges )
{
    // Find added components
    for( const auto& [ref, newComp] : aNew )
    {
        if( aBase.find( ref ) == aBase.end() )
        {
            PLM_COMPONENT_CHANGE change;
            change.type = PLM_COMPONENT_CHANGE_TYPE::ADDED;
            change.severity = PLM_CHANGE_SEVERITY::HIGH;
            change.reference = ref;
            change.description = wxString::Format( wxS( "Added component %s (%s)" ),
                                                   ref, newComp.value );
            change.affects_schematic = newComp.in_schematic;
            change.affects_pcb = newComp.in_pcb;
            aChanges.push_back( change );
        }
    }

    // Find removed components
    for( const auto& [ref, baseComp] : aBase )
    {
        if( aNew.find( ref ) == aNew.end() )
        {
            PLM_COMPONENT_CHANGE change;
            change.type = PLM_COMPONENT_CHANGE_TYPE::REMOVED;
            change.severity = PLM_CHANGE_SEVERITY::HIGH;
            change.reference = ref;
            change.description = wxString::Format( wxS( "Removed component %s (%s)" ),
                                                   ref, baseComp.value );
            change.affects_schematic = baseComp.in_schematic;
            change.affects_pcb = baseComp.in_pcb;
            aChanges.push_back( change );
        }
    }

    // Find modified components
    for( const auto& [ref, newComp] : aNew )
    {
        auto baseIt = aBase.find( ref );

        if( baseIt == aBase.end() )
            continue;

        const PLM_UNIFIED_COMPONENT& baseComp = baseIt->second;

        // Check value change
        if( baseComp.value != newComp.value )
        {
            PLM_COMPONENT_CHANGE change;
            change.type = PLM_COMPONENT_CHANGE_TYPE::VALUE_CHANGED;
            change.severity = DetermineSeverity( change.type, baseComp.value, newComp.value );
            change.reference = ref;
            change.description = wxString::Format( wxS( "Value changed for %s" ), ref );
            change.old_value = baseComp.value;
            change.new_value = newComp.value;
            change.affects_schematic = true;
            change.affects_pcb = newComp.in_pcb;
            aChanges.push_back( change );
        }

        // Check footprint change
        if( baseComp.footprint != newComp.footprint && m_config.track_property_changes )
        {
            PLM_COMPONENT_CHANGE change;
            change.type = PLM_COMPONENT_CHANGE_TYPE::FOOTPRINT_CHANGED;
            change.severity = PLM_CHANGE_SEVERITY::HIGH;
            change.reference = ref;
            change.description = wxString::Format( wxS( "Footprint changed for %s" ), ref );
            change.old_value = baseComp.footprint;
            change.new_value = newComp.footprint;
            change.affects_schematic = true;
            change.affects_pcb = true;
            aChanges.push_back( change );
        }

        // Check library ID change (symbol change)
        if( baseComp.lib_id_sch != newComp.lib_id_sch && m_config.track_property_changes )
        {
            PLM_COMPONENT_CHANGE change;
            change.type = PLM_COMPONENT_CHANGE_TYPE::PROPERTY_CHANGED;
            change.severity = PLM_CHANGE_SEVERITY::MEDIUM;
            change.reference = ref;
            change.description = wxString::Format( wxS( "Symbol type changed for %s" ), ref );
            change.old_value = baseComp.lib_id_sch;
            change.new_value = newComp.lib_id_sch;
            change.affects_schematic = true;
            change.affects_pcb = false;
            aChanges.push_back( change );
        }

        // Check other property changes
        if( m_config.track_property_changes )
        {
            for( const auto& [key, newVal] : newComp.properties )
            {
                auto baseIt2 = baseComp.properties.find( key );

                if( baseIt2 == baseComp.properties.end() )
                {
                    PLM_COMPONENT_CHANGE change;
                    change.type = PLM_COMPONENT_CHANGE_TYPE::PROPERTY_CHANGED;
                    change.severity = PLM_CHANGE_SEVERITY::LOW;
                    change.reference = ref;
                    change.description = wxString::Format( wxS( "Added property '%s' to %s" ),
                                                           key, ref );
                    change.new_value = newVal;
                    change.affects_schematic = newComp.in_schematic;
                    change.affects_pcb = newComp.in_pcb;
                    aChanges.push_back( change );
                }
                else if( baseIt2->second != newVal )
                {
                    PLM_COMPONENT_CHANGE change;
                    change.type = PLM_COMPONENT_CHANGE_TYPE::PROPERTY_CHANGED;
                    change.severity = PLM_CHANGE_SEVERITY::LOW;
                    change.reference = ref;
                    change.description = wxString::Format( wxS( "Changed property '%s' for %s" ),
                                                           key, ref );
                    change.old_value = baseIt2->second;
                    change.new_value = newVal;
                    change.affects_schematic = newComp.in_schematic;
                    change.affects_pcb = newComp.in_pcb;
                    aChanges.push_back( change );
                }
            }
        }
    }
}


PLM_CHANGE_SEVERITY PLM_DIFF_ANALYZER::DetermineSeverity( PLM_COMPONENT_CHANGE_TYPE aType,
                                                           const wxString& aOldValue,
                                                           const wxString& aNewValue )
{
    switch( aType )
    {
    case PLM_COMPONENT_CHANGE_TYPE::ADDED:
    case PLM_COMPONENT_CHANGE_TYPE::REMOVED:
        return PLM_CHANGE_SEVERITY::HIGH;

    case PLM_COMPONENT_CHANGE_TYPE::FOOTPRINT_CHANGED:
        return PLM_CHANGE_SEVERITY::HIGH;

    case PLM_COMPONENT_CHANGE_TYPE::VALUE_CHANGED:
        return PLM_CHANGE_SEVERITY::MEDIUM;

    case PLM_COMPONENT_CHANGE_TYPE::REFERENCE_CHANGED:
        return PLM_CHANGE_SEVERITY::MEDIUM;

    case PLM_COMPONENT_CHANGE_TYPE::MOVED:
        return m_config.ignore_cosmetic ? PLM_CHANGE_SEVERITY::INFO : PLM_CHANGE_SEVERITY::LOW;

    case PLM_COMPONENT_CHANGE_TYPE::CONNECTIVITY_CHANGED:
        return PLM_CHANGE_SEVERITY::CRITICAL;

    case PLM_COMPONENT_CHANGE_TYPE::PROPERTY_CHANGED:
        return PLM_CHANGE_SEVERITY::LOW;

    default:
        return PLM_CHANGE_SEVERITY::INFO;
    }
}


void PLM_DIFF_ANALYZER::ProcessSchematicDiff( const PLM_SCH_DIFF_RESULT& aDiff,
                                               std::vector<PLM_COMPONENT_CHANGE>& aChanges )
{
    for( const auto& schChange : aDiff.changes )
    {
        if( schChange.element_type != PLM_SCH_ELEMENT_TYPE::SYMBOL )
            continue;

        // Skip if we already have a component-level change for this
        bool alreadyTracked = false;

        for( const auto& existing : aChanges )
        {
            if( existing.reference == schChange.description )
            {
                alreadyTracked = true;
                break;
            }
        }

        if( alreadyTracked )
            continue;

        if( schChange.change_type == PLM_SCH_CHANGE_TYPE::MOVED && m_config.track_moves )
        {
            PLM_COMPONENT_CHANGE change;
            change.type = PLM_COMPONENT_CHANGE_TYPE::MOVED;
            change.severity = PLM_CHANGE_SEVERITY::LOW;
            change.reference = schChange.uuid;
            change.description = schChange.description;
            change.affects_schematic = true;
            change.affects_pcb = false;
            aChanges.push_back( change );
        }
    }
}


void PLM_DIFF_ANALYZER::ProcessPCBDiff( const PLM_PCB_DIFF_RESULT& aDiff,
                                         std::vector<PLM_COMPONENT_CHANGE>& aChanges )
{
    for( const auto& pcbChange : aDiff.changes )
    {
        if( pcbChange.element_type != PLM_PCB_ELEMENT_TYPE::FOOTPRINT )
            continue;

        // Skip if we already have a component-level change for this
        bool alreadyTracked = false;

        for( const auto& existing : aChanges )
        {
            if( existing.reference == pcbChange.description )
            {
                alreadyTracked = true;
                break;
            }
        }

        if( alreadyTracked )
            continue;

        if( pcbChange.change_type == PLM_PCB_CHANGE_TYPE::MOVED && m_config.track_moves )
        {
            PLM_COMPONENT_CHANGE change;
            change.type = PLM_COMPONENT_CHANGE_TYPE::MOVED;
            change.severity = PLM_CHANGE_SEVERITY::LOW;
            change.reference = pcbChange.uuid;
            change.description = pcbChange.description;
            change.affects_schematic = false;
            change.affects_pcb = true;
            aChanges.push_back( change );
        }
    }
}


PLM_CHANGE_STATISTICS PLM_DIFF_ANALYZER::CalculateStatistics(
        const std::vector<PLM_COMPONENT_CHANGE>& aChanges )
{
    PLM_CHANGE_STATISTICS stats;

    for( const auto& change : aChanges )
    {
        switch( change.type )
        {
        case PLM_COMPONENT_CHANGE_TYPE::ADDED:
            ++stats.components_added;
            break;

        case PLM_COMPONENT_CHANGE_TYPE::REMOVED:
            ++stats.components_removed;
            break;

        case PLM_COMPONENT_CHANGE_TYPE::MOVED:
            ++stats.components_moved;
            break;

        case PLM_COMPONENT_CHANGE_TYPE::CONNECTIVITY_CHANGED:
            ++stats.connectivity_changes;
            break;

        default:
            ++stats.components_modified;
            break;
        }

        if( change.affects_schematic && !change.affects_pcb )
            ++stats.schematic_only_changes;

        if( change.affects_pcb && !change.affects_schematic )
            ++stats.pcb_only_changes;

        if( change.severity == PLM_CHANGE_SEVERITY::CRITICAL )
            ++stats.critical_changes;
        else if( change.severity == PLM_CHANGE_SEVERITY::HIGH )
            ++stats.high_severity_changes;
    }

    return stats;
}
