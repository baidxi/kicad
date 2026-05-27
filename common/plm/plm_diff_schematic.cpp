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

#include "plm_diff_schematic.h"

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/regex.h>
#include <wx/tokenzr.h>

#include <algorithm>
#include <set>


namespace
{

wxString ExtractSExprValue( const wxString& aContent, const wxString& aKey )
{
    wxString pattern = wxS( "\\(" ) + aKey + wxS( "\\s+\"?([^\")]+)\"?\\)" );
    wxRegEx regex( pattern );

    if( regex.Matches( aContent ) )
        return regex.GetMatch( aContent, 1 );

    return wxEmptyString;
}


wxString ExtractUUID( const wxString& aContent )
{
    wxRegEx uuidRegex( wxS( "\\(uuid\\s+\"?([a-f0-9-]+)\"?\\)" ) );

    if( uuidRegex.Matches( aContent ) )
        return uuidRegex.GetMatch( aContent, 1 );

    return wxEmptyString;
}


bool ExtractPosition( const wxString& aContent, int32_t& aX, int32_t& aY, int32_t& aRotation )
{
    wxRegEx atRegex( wxS( "\\(at\\s+([0-9.-]+)\\s+([0-9.-]+)(?:\\s+([0-9.-]+))?\\)" ) );

    if( atRegex.Matches( aContent ) )
    {
        double x, y, rot = 0;
        atRegex.GetMatch( aContent, 1 ).ToDouble( &x );
        atRegex.GetMatch( aContent, 2 ).ToDouble( &y );

        wxString rotStr = atRegex.GetMatch( aContent, 3 );

        if( !rotStr.IsEmpty() )
            rotStr.ToDouble( &rot );

        aX = static_cast<int32_t>( x * 1000 );
        aY = static_cast<int32_t>( y * 1000 );
        aRotation = static_cast<int32_t>( rot * 10 );
        return true;
    }

    return false;
}


bool ExtractEndpoints( const wxString& aContent, PLM_SCH_ENDPOINT& aStart, PLM_SCH_ENDPOINT& aEnd )
{
    wxRegEx ptsRegex( wxS( "\\(pts\\s+\\(xy\\s+([0-9.-]+)\\s+([0-9.-]+)\\)\\s+"
                          "\\(xy\\s+([0-9.-]+)\\s+([0-9.-]+)\\)\\)" ) );

    if( ptsRegex.Matches( aContent ) )
    {
        double x1, y1, x2, y2;
        ptsRegex.GetMatch( aContent, 1 ).ToDouble( &x1 );
        ptsRegex.GetMatch( aContent, 2 ).ToDouble( &y1 );
        ptsRegex.GetMatch( aContent, 3 ).ToDouble( &x2 );
        ptsRegex.GetMatch( aContent, 4 ).ToDouble( &y2 );

        aStart.x = static_cast<int32_t>( x1 * 1000 );
        aStart.y = static_cast<int32_t>( y1 * 1000 );
        aEnd.x = static_cast<int32_t>( x2 * 1000 );
        aEnd.y = static_cast<int32_t>( y2 * 1000 );
        return true;
    }

    return false;
}


wxString ReadFileContent( const wxString& aFilename )
{
    if( !wxFileName::FileExists( aFilename ) )
        return wxEmptyString;

    wxFile file( aFilename );

    if( !file.IsOpened() )
        return wxEmptyString;

    wxString content;
    file.ReadAll( &content );
    return content;
}


std::vector<wxString> FindSExprBlocks( const wxString& aContent, const wxString& aBlockType )
{
    std::vector<wxString> blocks;
    wxString searchStart = wxS( "(" ) + aBlockType + wxS( " " );

    size_t pos = 0;

    while( ( pos = aContent.find( searchStart, pos ) ) != wxString::npos )
    {
        int depth = 1;
        size_t start = pos;
        pos += searchStart.length();

        while( pos < aContent.length() && depth > 0 )
        {
            if( aContent[pos] == '(' )
                ++depth;
            else if( aContent[pos] == ')' )
                --depth;

            ++pos;
        }

        if( depth == 0 )
            blocks.push_back( aContent.Mid( start, pos - start ) );
    }

    return blocks;
}

} // anonymous namespace


PLM_SCHEMATIC_DIFF_PARSER::PLM_SCHEMATIC_DIFF_PARSER()
{
}


PLM_SCHEMATIC_DIFF_PARSER::~PLM_SCHEMATIC_DIFF_PARSER()
{
}


PLM_SCH_DATA PLM_SCHEMATIC_DIFF_PARSER::ParseSchematic( const wxString& aFilename )
{
    PLM_SCH_DATA data;
    m_lastError.clear();

    wxString content = ReadFileContent( aFilename );

    if( content.IsEmpty() )
    {
        m_lastError = wxString::Format( wxS( "Failed to read schematic file: %s" ), aFilename );
        return data;
    }

    data.root_filename = aFilename;

    wxFileName fn( aFilename );
    data.project_name = fn.GetName();

    PLM_SCH_SHEET_DATA sheet = ParseSheet( aFilename, content );
    data.sheets.push_back( sheet );

    return data;
}


PLM_SCH_DATA PLM_SCHEMATIC_DIFF_PARSER::ParseSchematicHierarchy( const wxString& aFilename )
{
    PLM_SCH_DATA data;
    m_lastError.clear();

    wxString content = ReadFileContent( aFilename );

    if( content.IsEmpty() )
    {
        m_lastError = wxString::Format( wxS( "Failed to read schematic file: %s" ), aFilename );
        return data;
    }

    data.root_filename = aFilename;

    wxFileName fn( aFilename );
    data.project_name = fn.GetName();

    std::set<wxString> processedFiles;
    std::vector<wxString> filesToProcess;
    filesToProcess.push_back( aFilename );

    wxString baseDir = fn.GetPath();

    while( !filesToProcess.empty() )
    {
        wxString currentFile = filesToProcess.back();
        filesToProcess.pop_back();

        if( processedFiles.count( currentFile ) > 0 )
            continue;

        processedFiles.insert( currentFile );

        wxString fileContent = ReadFileContent( currentFile );

        if( fileContent.IsEmpty() )
            continue;

        PLM_SCH_SHEET_DATA sheetData = ParseSheet( currentFile, fileContent );
        data.sheets.push_back( sheetData );

        for( const PLM_SCH_SHEET& sheet : sheetData.sheets )
        {
            wxFileName sheetFn( sheet.filename );

            if( !sheetFn.IsAbsolute() )
                sheetFn.MakeAbsolute( baseDir );

            wxString sheetPath = sheetFn.GetFullPath();

            if( processedFiles.count( sheetPath ) == 0 )
                filesToProcess.push_back( sheetPath );
        }
    }

    return data;
}


PLM_SCH_SHEET_DATA PLM_SCHEMATIC_DIFF_PARSER::ParseSheet( const wxString& aFilename,
                                                           const wxString& aContent )
{
    PLM_SCH_SHEET_DATA sheet;
    sheet.filename = aFilename;
    sheet.uuid = ExtractUUID( aContent );

    // Parse symbols
    std::vector<wxString> symbolBlocks = FindSExprBlocks( aContent, wxS( "symbol" ) );

    for( const wxString& block : symbolBlocks )
    {
        if( block.Contains( wxS( "(lib_id" ) ) )
        {
            PLM_SCH_SYMBOL sym = ParseSymbol( block );
            sheet.symbols.push_back( sym );
        }
    }

    // Parse wires
    std::vector<wxString> wireBlocks = FindSExprBlocks( aContent, wxS( "wire" ) );

    for( const wxString& block : wireBlocks )
    {
        PLM_SCH_CONNECTION conn = ParseConnection( block, false );
        sheet.connections.push_back( conn );
    }

    // Parse buses
    std::vector<wxString> busBlocks = FindSExprBlocks( aContent, wxS( "bus" ) );

    for( const wxString& block : busBlocks )
    {
        PLM_SCH_CONNECTION conn = ParseConnection( block, true );
        sheet.connections.push_back( conn );
    }

    // Parse labels
    std::vector<wxString> labelBlocks = FindSExprBlocks( aContent, wxS( "label" ) );

    for( const wxString& block : labelBlocks )
    {
        PLM_SCH_LABEL label = ParseLabel( block, PLM_SCH_ELEMENT_TYPE::LABEL );
        sheet.labels.push_back( label );
    }

    // Parse global labels
    std::vector<wxString> globalLabelBlocks = FindSExprBlocks( aContent, wxS( "global_label" ) );

    for( const wxString& block : globalLabelBlocks )
    {
        PLM_SCH_LABEL label = ParseLabel( block, PLM_SCH_ELEMENT_TYPE::GLOBAL_LABEL );
        sheet.labels.push_back( label );
    }

    // Parse hierarchical labels
    std::vector<wxString> hierLabelBlocks = FindSExprBlocks( aContent, wxS( "hierarchical_label" ) );

    for( const wxString& block : hierLabelBlocks )
    {
        PLM_SCH_LABEL label = ParseLabel( block, PLM_SCH_ELEMENT_TYPE::HIER_LABEL );
        sheet.labels.push_back( label );
    }

    // Parse text annotations
    std::vector<wxString> textBlocks = FindSExprBlocks( aContent, wxS( "text" ) );

    for( const wxString& block : textBlocks )
    {
        PLM_SCH_TEXT text;
        text.uuid = ExtractUUID( block );
        text.is_textbox = false;

        wxRegEx textRegex( wxS( "\\(text\\s+\"([^\"]+)\"" ) );

        if( textRegex.Matches( block ) )
            text.text = textRegex.GetMatch( block, 1 );

        int32_t x, y, rot;

        if( ExtractPosition( block, x, y, rot ) )
        {
            text.geometry.x = x;
            text.geometry.y = y;
            text.geometry.rotation = rot;
        }

        sheet.texts.push_back( text );
    }

    // Parse sheets (sub-schematics)
    std::vector<wxString> sheetBlocks = FindSExprBlocks( aContent, wxS( "sheet" ) );

    for( const wxString& block : sheetBlocks )
    {
        PLM_SCH_SHEET sheetRef;
        sheetRef.uuid = ExtractUUID( block );

        wxRegEx nameRegex( wxS( "\\(property\\s+\"Sheetname\"\\s+\"([^\"]+)\"" ) );

        if( nameRegex.Matches( block ) )
            sheetRef.name = nameRegex.GetMatch( block, 1 );

        wxRegEx fileRegex( wxS( "\\(property\\s+\"Sheetfile\"\\s+\"([^\"]+)\"" ) );

        if( fileRegex.Matches( block ) )
            sheetRef.filename = fileRegex.GetMatch( block, 1 );

        int32_t x, y, rot;

        if( ExtractPosition( block, x, y, rot ) )
        {
            sheetRef.geometry.x = x;
            sheetRef.geometry.y = y;
            sheetRef.geometry.rotation = rot;
        }

        wxRegEx sizeRegex( wxS( "\\(size\\s+([0-9.-]+)\\s+([0-9.-]+)\\)" ) );

        if( sizeRegex.Matches( block ) )
        {
            double w, h;
            sizeRegex.GetMatch( block, 1 ).ToDouble( &w );
            sizeRegex.GetMatch( block, 2 ).ToDouble( &h );
            sheetRef.geometry.width = static_cast<int32_t>( w * 1000 );
            sheetRef.geometry.height = static_cast<int32_t>( h * 1000 );
        }

        sheet.sheets.push_back( sheetRef );
    }

    return sheet;
}


PLM_SCH_SYMBOL PLM_SCHEMATIC_DIFF_PARSER::ParseSymbol( const wxString& aContent )
{
    PLM_SCH_SYMBOL sym;

    sym.uuid = ExtractUUID( aContent );
    sym.lib_id = ExtractSExprValue( aContent, wxS( "lib_id" ) );

    int32_t x, y, rot;

    if( ExtractPosition( aContent, x, y, rot ) )
    {
        sym.geometry.x = x;
        sym.geometry.y = y;
        sym.geometry.rotation = rot;
    }

    if( aContent.Contains( wxS( "(mirror x)" ) ) || aContent.Contains( wxS( "(mirror y)" ) ) )
        sym.geometry.mirrored = true;

    wxRegEx unitRegex( wxS( "\\(unit\\s+([0-9]+)\\)" ) );

    if( unitRegex.Matches( aContent ) )
    {
        long unit;
        unitRegex.GetMatch( aContent, 1 ).ToLong( &unit );
        sym.unit = static_cast<int>( unit );
    }

    // Extract properties (Reference, Value, Footprint)
    std::vector<wxString> propertyBlocks = FindSExprBlocks( aContent, wxS( "property" ) );

    for( const wxString& propBlock : propertyBlocks )
    {
        wxRegEx propRegex( wxS( "\\(property\\s+\"([^\"]+)\"\\s+\"([^\"]*)\"" ) );

        if( propRegex.Matches( propBlock ) )
        {
            wxString propName = propRegex.GetMatch( propBlock, 1 );
            wxString propValue = propRegex.GetMatch( propBlock, 2 );

            if( propName == wxS( "Reference" ) )
                sym.reference = propValue;
            else if( propName == wxS( "Value" ) )
                sym.value = propValue;
            else if( propName == wxS( "Footprint" ) )
                sym.footprint = propValue;
            else
                sym.properties[propName] = propValue;
        }
    }

    return sym;
}


PLM_SCH_CONNECTION PLM_SCHEMATIC_DIFF_PARSER::ParseConnection( const wxString& aContent,
                                                                 bool aIsBus )
{
    PLM_SCH_CONNECTION conn;
    conn.uuid = ExtractUUID( aContent );
    conn.is_bus = aIsBus;

    ExtractEndpoints( aContent, conn.start, conn.end );

    return conn;
}


PLM_SCH_LABEL PLM_SCHEMATIC_DIFF_PARSER::ParseLabel( const wxString& aContent,
                                                       PLM_SCH_ELEMENT_TYPE aType )
{
    PLM_SCH_LABEL label;
    label.uuid = ExtractUUID( aContent );
    label.type = aType;

    wxRegEx textRegex( wxS( "\"([^\"]+)\"" ) );

    if( textRegex.Matches( aContent ) )
        label.text = textRegex.GetMatch( aContent, 1 );

    int32_t x, y, rot;

    if( ExtractPosition( aContent, x, y, rot ) )
    {
        label.geometry.x = x;
        label.geometry.y = y;
        label.geometry.rotation = rot;
    }

    return label;
}


PLM_SCH_DIFF_RESULT PLM_SCHEMATIC_DIFF_PARSER::ComputeDiff( const PLM_SCH_DATA& aBase,
                                                             const PLM_SCH_DATA& aNew )
{
    PLM_SCH_DIFF_RESULT result;
    result.base_file = aBase.root_filename;
    result.new_file = aNew.root_filename;
    result.success = true;

    // Build maps by UUID for quick lookup
    std::map<wxString, const PLM_SCH_SYMBOL*> baseSymbols;
    std::map<wxString, const PLM_SCH_SYMBOL*> newSymbols;
    std::map<wxString, wxString> symbolSheetMap;

    for( const auto& sheet : aBase.sheets )
    {
        for( const auto& sym : sheet.symbols )
            baseSymbols[sym.uuid] = &sym;
    }

    for( const auto& sheet : aNew.sheets )
    {
        for( const auto& sym : sheet.symbols )
        {
            newSymbols[sym.uuid] = &sym;
            symbolSheetMap[sym.uuid] = sheet.uuid;
        }
    }

    // Find added and modified symbols
    for( const auto& [uuid, newSym] : newSymbols )
    {
        auto baseIt = baseSymbols.find( uuid );

        if( baseIt == baseSymbols.end() )
        {
            PLM_SCH_CHANGE change;
            change.change_type = PLM_SCH_CHANGE_TYPE::ADDED;
            change.element_type = PLM_SCH_ELEMENT_TYPE::SYMBOL;
            change.uuid = uuid;
            change.sheet_uuid = symbolSheetMap[uuid];
            change.description = wxString::Format( wxS( "Added symbol %s (%s)" ),
                                                   newSym->reference, newSym->lib_id );
            result.changes.push_back( change );
        }
        else
        {
            CompareSymbols( *baseIt->second, *newSym, symbolSheetMap[uuid], result.changes );
        }
    }

    // Find removed symbols
    for( const auto& [uuid, baseSym] : baseSymbols )
    {
        if( newSymbols.find( uuid ) == newSymbols.end() )
        {
            PLM_SCH_CHANGE change;
            change.change_type = PLM_SCH_CHANGE_TYPE::REMOVED;
            change.element_type = PLM_SCH_ELEMENT_TYPE::SYMBOL;
            change.uuid = uuid;
            change.description = wxString::Format( wxS( "Removed symbol %s (%s)" ),
                                                   baseSym->reference, baseSym->lib_id );
            result.changes.push_back( change );
        }
    }

    // Build connection maps
    std::map<wxString, const PLM_SCH_CONNECTION*> baseConns;
    std::map<wxString, const PLM_SCH_CONNECTION*> newConns;
    std::map<wxString, wxString> connSheetMap;

    for( const auto& sheet : aBase.sheets )
    {
        for( const auto& conn : sheet.connections )
            baseConns[conn.uuid] = &conn;
    }

    for( const auto& sheet : aNew.sheets )
    {
        for( const auto& conn : sheet.connections )
        {
            newConns[conn.uuid] = &conn;
            connSheetMap[conn.uuid] = sheet.uuid;
        }
    }

    // Find added and modified connections
    for( const auto& [uuid, newConn] : newConns )
    {
        auto baseIt = baseConns.find( uuid );

        if( baseIt == baseConns.end() )
        {
            PLM_SCH_CHANGE change;
            change.change_type = PLM_SCH_CHANGE_TYPE::ADDED;
            change.element_type = newConn->is_bus ? PLM_SCH_ELEMENT_TYPE::BUS
                                                   : PLM_SCH_ELEMENT_TYPE::WIRE;
            change.uuid = uuid;
            change.sheet_uuid = connSheetMap[uuid];
            change.description = wxString::Format( wxS( "Added %s" ),
                                                   newConn->is_bus ? wxS( "bus" ) : wxS( "wire" ) );
            result.changes.push_back( change );
        }
        else
        {
            CompareConnections( *baseIt->second, *newConn, connSheetMap[uuid], result.changes );
        }
    }

    // Find removed connections
    for( const auto& [uuid, baseConn] : baseConns )
    {
        if( newConns.find( uuid ) == newConns.end() )
        {
            PLM_SCH_CHANGE change;
            change.change_type = PLM_SCH_CHANGE_TYPE::REMOVED;
            change.element_type = baseConn->is_bus ? PLM_SCH_ELEMENT_TYPE::BUS
                                                    : PLM_SCH_ELEMENT_TYPE::WIRE;
            change.uuid = uuid;
            change.description = wxString::Format( wxS( "Removed %s" ),
                                                   baseConn->is_bus ? wxS( "bus" ) : wxS( "wire" ) );
            result.changes.push_back( change );
        }
    }

    // Build label maps
    std::map<wxString, const PLM_SCH_LABEL*> baseLabels;
    std::map<wxString, const PLM_SCH_LABEL*> newLabels;
    std::map<wxString, wxString> labelSheetMap;

    for( const auto& sheet : aBase.sheets )
    {
        for( const auto& label : sheet.labels )
            baseLabels[label.uuid] = &label;
    }

    for( const auto& sheet : aNew.sheets )
    {
        for( const auto& label : sheet.labels )
        {
            newLabels[label.uuid] = &label;
            labelSheetMap[label.uuid] = sheet.uuid;
        }
    }

    // Find added labels
    for( const auto& [uuid, newLabel] : newLabels )
    {
        if( baseLabels.find( uuid ) == baseLabels.end() )
        {
            PLM_SCH_CHANGE change;
            change.change_type = PLM_SCH_CHANGE_TYPE::ADDED;
            change.element_type = newLabel->type;
            change.uuid = uuid;
            change.sheet_uuid = labelSheetMap[uuid];
            change.description = wxString::Format( wxS( "Added label '%s'" ), newLabel->text );
            result.changes.push_back( change );
        }
    }

    // Find removed labels
    for( const auto& [uuid, baseLabel] : baseLabels )
    {
        if( newLabels.find( uuid ) == newLabels.end() )
        {
            PLM_SCH_CHANGE change;
            change.change_type = PLM_SCH_CHANGE_TYPE::REMOVED;
            change.element_type = baseLabel->type;
            change.uuid = uuid;
            change.description = wxString::Format( wxS( "Removed label '%s'" ), baseLabel->text );
            result.changes.push_back( change );
        }
    }

    return result;
}


void PLM_SCHEMATIC_DIFF_PARSER::CompareSymbols( const PLM_SCH_SYMBOL& aBase,
                                                  const PLM_SCH_SYMBOL& aNew,
                                                  const wxString& aSheetUuid,
                                                  std::vector<PLM_SCH_CHANGE>& aChanges )
{
    if( aBase == aNew )
        return;

    bool positionOnly = ( aBase.lib_id == aNew.lib_id && aBase.reference == aNew.reference &&
                          aBase.value == aNew.value && aBase.footprint == aNew.footprint &&
                          aBase.unit == aNew.unit && aBase.properties == aNew.properties );

    if( positionOnly && aBase.geometry != aNew.geometry )
    {
        PLM_SCH_CHANGE change;
        change.change_type = PLM_SCH_CHANGE_TYPE::MOVED;
        change.element_type = PLM_SCH_ELEMENT_TYPE::SYMBOL;
        change.uuid = aNew.uuid;
        change.sheet_uuid = aSheetUuid;
        change.description = wxString::Format( wxS( "Moved symbol %s" ), aNew.reference );
        aChanges.push_back( change );
        return;
    }

    if( aBase.reference != aNew.reference )
    {
        PLM_SCH_CHANGE change;
        change.change_type = PLM_SCH_CHANGE_TYPE::MODIFIED;
        change.element_type = PLM_SCH_ELEMENT_TYPE::SYMBOL;
        change.uuid = aNew.uuid;
        change.sheet_uuid = aSheetUuid;
        change.description = wxS( "Changed reference designator" );
        change.old_value = aBase.reference;
        change.new_value = aNew.reference;
        aChanges.push_back( change );
    }

    if( aBase.value != aNew.value )
    {
        PLM_SCH_CHANGE change;
        change.change_type = PLM_SCH_CHANGE_TYPE::MODIFIED;
        change.element_type = PLM_SCH_ELEMENT_TYPE::SYMBOL;
        change.uuid = aNew.uuid;
        change.sheet_uuid = aSheetUuid;
        change.description = wxString::Format( wxS( "Changed value of %s" ), aNew.reference );
        change.old_value = aBase.value;
        change.new_value = aNew.value;
        aChanges.push_back( change );
    }

    if( aBase.footprint != aNew.footprint )
    {
        PLM_SCH_CHANGE change;
        change.change_type = PLM_SCH_CHANGE_TYPE::MODIFIED;
        change.element_type = PLM_SCH_ELEMENT_TYPE::SYMBOL;
        change.uuid = aNew.uuid;
        change.sheet_uuid = aSheetUuid;
        change.description = wxString::Format( wxS( "Changed footprint of %s" ), aNew.reference );
        change.old_value = aBase.footprint;
        change.new_value = aNew.footprint;
        aChanges.push_back( change );
    }

    if( aBase.lib_id != aNew.lib_id )
    {
        PLM_SCH_CHANGE change;
        change.change_type = PLM_SCH_CHANGE_TYPE::MODIFIED;
        change.element_type = PLM_SCH_ELEMENT_TYPE::SYMBOL;
        change.uuid = aNew.uuid;
        change.sheet_uuid = aSheetUuid;
        change.description = wxString::Format( wxS( "Changed symbol type of %s" ), aNew.reference );
        change.old_value = aBase.lib_id;
        change.new_value = aNew.lib_id;
        aChanges.push_back( change );
    }
}


void PLM_SCHEMATIC_DIFF_PARSER::CompareConnections( const PLM_SCH_CONNECTION& aBase,
                                                      const PLM_SCH_CONNECTION& aNew,
                                                      const wxString& aSheetUuid,
                                                      std::vector<PLM_SCH_CHANGE>& aChanges )
{
    if( aBase == aNew )
        return;

    if( aBase.start != aNew.start || aBase.end != aNew.end )
    {
        PLM_SCH_CHANGE change;
        change.change_type = PLM_SCH_CHANGE_TYPE::MODIFIED;
        change.element_type = aNew.is_bus ? PLM_SCH_ELEMENT_TYPE::BUS : PLM_SCH_ELEMENT_TYPE::WIRE;
        change.uuid = aNew.uuid;
        change.sheet_uuid = aSheetUuid;
        change.description = wxString::Format( wxS( "Modified %s endpoints" ),
                                               aNew.is_bus ? wxS( "bus" ) : wxS( "wire" ) );
        aChanges.push_back( change );
    }
}
