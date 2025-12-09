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

#include "plm_merge_generator.h"

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/regex.h>
#include <wx/textfile.h>
#include <wx/datetime.h>


PLM_MERGE_GENERATOR::PLM_MERGE_GENERATOR()
{
}


PLM_MERGE_GENERATOR::~PLM_MERGE_GENERATOR()
{
}


void PLM_MERGE_GENERATOR::SetBaseFiles( const wxString& aSchematic, const wxString& aPCB )
{
    m_baseSchematic = aSchematic;
    m_basePCB = aPCB;
    m_baseSchematicContent.Clear();
    m_basePCBContent.Clear();
}


void PLM_MERGE_GENERATOR::SetLocalFiles( const wxString& aSchematic, const wxString& aPCB )
{
    m_localSchematic = aSchematic;
    m_localPCB = aPCB;
    m_localSchematicContent.Clear();
    m_localPCBContent.Clear();
}


void PLM_MERGE_GENERATOR::SetServerFiles( const wxString& aSchematic, const wxString& aPCB )
{
    m_serverSchematic = aSchematic;
    m_serverPCB = aPCB;
    m_serverSchematicContent.Clear();
    m_serverPCBContent.Clear();
}


PLM_MERGE_RESULT PLM_MERGE_GENERATOR::ApplyMergePlan( const PLM_MERGE_PLAN& aPlan,
                                                       const wxString& aOutputSchematic,
                                                       const wxString& aOutputPCB )
{
    PLM_MERGE_RESULT result;
    m_lastError.Clear();

    wxString validationError = ValidatePlan( aPlan );

    if( !validationError.IsEmpty() )
    {
        m_lastError = validationError;
        result.error = validationError;
        return result;
    }

    if( !LoadSourceFiles() )
    {
        result.error = m_lastError;
        return result;
    }

    if( m_options.backup_originals )
    {
        if( !CreateBackups() )
        {
            result.error = m_lastError;
            return result;
        }
    }

    std::set<size_t> allActions;

    for( size_t i = 0; i < aPlan.actions.size(); ++i )
    {
        if( aPlan.actions[i].NeedsAction() )
            allActions.insert( i );
    }

    if( !aOutputSchematic.IsEmpty() && !m_baseSchematicContent.IsEmpty() )
    {
        ReportProgress( 0, 2, wxS( "Generating merged schematic..." ) );
        wxString mergedSchematic = GenerateMergedSchematic( aPlan, allActions );

        if( mergedSchematic.IsEmpty() && !m_lastError.IsEmpty() )
        {
            result.error = m_lastError;
            return result;
        }

        wxFile outFile( aOutputSchematic, wxFile::write );

        if( !outFile.IsOpened() )
        {
            m_lastError = wxString::Format( wxS( "Cannot write to %s" ), aOutputSchematic );
            result.error = m_lastError;
            return result;
        }

        outFile.Write( mergedSchematic );
        outFile.Close();
        result.merged_schematic = aOutputSchematic;

        if( m_options.validate_output )
        {
            if( !ValidateOutput( aOutputSchematic, true ) )
            {
                result.error = m_lastError;
                return result;
            }
        }
    }

    if( !aOutputPCB.IsEmpty() && !m_basePCBContent.IsEmpty() )
    {
        ReportProgress( 1, 2, wxS( "Generating merged PCB..." ) );
        wxString mergedPCB = GenerateMergedPCB( aPlan, allActions );

        if( mergedPCB.IsEmpty() && !m_lastError.IsEmpty() )
        {
            result.error = m_lastError;
            return result;
        }

        wxFile outFile( aOutputPCB, wxFile::write );

        if( !outFile.IsOpened() )
        {
            m_lastError = wxString::Format( wxS( "Cannot write to %s" ), aOutputPCB );
            result.error = m_lastError;
            return result;
        }

        outFile.Write( mergedPCB );
        outFile.Close();
        result.merged_pcb = aOutputPCB;

        if( m_options.validate_output )
        {
            if( !ValidateOutput( aOutputPCB, false ) )
            {
                result.error = m_lastError;
                return result;
            }
        }
    }

    result.success = true;
    result.elements_merged = allActions.size();

    for( size_t idx : allActions )
    {
        const PLM_MERGE_ACTION& action = aPlan.actions[idx];

        if( action.resolution == PLM_MERGE_RESOLUTION::TAKE_LOCAL )
            ++result.local_changes_applied;
        else if( action.resolution == PLM_MERGE_RESOLUTION::TAKE_SERVER )
            ++result.server_changes_applied;
    }

    if( m_options.generate_report && !m_options.report_path.IsEmpty() )
        GenerateReport( aPlan, result, m_options.report_path );

    ReportProgress( 2, 2, wxS( "Merge complete" ) );
    return result;
}


PLM_MERGE_RESULT PLM_MERGE_GENERATOR::CherryPick( const PLM_MERGE_PLAN& aPlan,
                                                   const std::vector<size_t>& aActionIndices,
                                                   const wxString& aOutputSchematic,
                                                   const wxString& aOutputPCB )
{
    PLM_MERGE_RESULT result;
    m_lastError.Clear();

    for( size_t idx : aActionIndices )
    {
        if( idx >= aPlan.actions.size() )
        {
            m_lastError = wxString::Format( wxS( "Invalid action index: %zu" ), idx );
            result.error = m_lastError;
            return result;
        }
    }

    if( !LoadSourceFiles() )
    {
        result.error = m_lastError;
        return result;
    }

    std::set<size_t> selectedActions( aActionIndices.begin(), aActionIndices.end() );

    if( !aOutputSchematic.IsEmpty() && !m_baseSchematicContent.IsEmpty() )
    {
        wxString mergedSchematic = GenerateMergedSchematic( aPlan, selectedActions );

        if( mergedSchematic.IsEmpty() && !m_lastError.IsEmpty() )
        {
            result.error = m_lastError;
            return result;
        }

        wxFile outFile( aOutputSchematic, wxFile::write );

        if( !outFile.IsOpened() )
        {
            m_lastError = wxString::Format( wxS( "Cannot write to %s" ), aOutputSchematic );
            result.error = m_lastError;
            return result;
        }

        outFile.Write( mergedSchematic );
        outFile.Close();
        result.merged_schematic = aOutputSchematic;
    }

    if( !aOutputPCB.IsEmpty() && !m_basePCBContent.IsEmpty() )
    {
        wxString mergedPCB = GenerateMergedPCB( aPlan, selectedActions );

        if( mergedPCB.IsEmpty() && !m_lastError.IsEmpty() )
        {
            result.error = m_lastError;
            return result;
        }

        wxFile outFile( aOutputPCB, wxFile::write );

        if( !outFile.IsOpened() )
        {
            m_lastError = wxString::Format( wxS( "Cannot write to %s" ), aOutputPCB );
            result.error = m_lastError;
            return result;
        }

        outFile.Write( mergedPCB );
        outFile.Close();
        result.merged_pcb = aOutputPCB;
    }

    result.success = true;
    result.elements_merged = selectedActions.size();

    for( size_t idx : selectedActions )
    {
        const PLM_MERGE_ACTION& action = aPlan.actions[idx];

        if( action.resolution == PLM_MERGE_RESOLUTION::TAKE_LOCAL )
            ++result.local_changes_applied;
        else if( action.resolution == PLM_MERGE_RESOLUTION::TAKE_SERVER )
            ++result.server_changes_applied;
    }

    return result;
}


wxString PLM_MERGE_GENERATOR::ValidatePlan( const PLM_MERGE_PLAN& aPlan )
{
    if( aPlan.GetUnresolvedConflictCount() > 0 )
    {
        return wxString::Format( wxS( "Plan has %zu unresolved conflicts" ),
                                 aPlan.GetUnresolvedConflictCount() );
    }

    if( m_baseSchematic.IsEmpty() )
        return wxS( "Base schematic file not set" );

    if( m_localSchematic.IsEmpty() )
        return wxS( "Local schematic file not set" );

    if( m_serverSchematic.IsEmpty() )
        return wxS( "Server schematic file not set" );

    return wxEmptyString;
}


bool PLM_MERGE_GENERATOR::GenerateReport( const PLM_MERGE_PLAN& aPlan,
                                           const PLM_MERGE_RESULT& aResult,
                                           const wxString& aReportPath )
{
    wxFile reportFile( aReportPath, wxFile::write );

    if( !reportFile.IsOpened() )
    {
        m_lastError = wxString::Format( wxS( "Cannot create report file: %s" ), aReportPath );
        return false;
    }

    wxString report;
    report += wxS( "PLM Merge Report\n" );
    report += wxS( "================\n\n" );
    report += wxString::Format( wxS( "Generated: %s\n\n" ),
                                wxDateTime::Now().Format( wxS( "%Y-%m-%d %H:%M:%S" ) ) );

    report += wxS( "Source Files\n" );
    report += wxS( "------------\n" );
    report += wxString::Format( wxS( "Base:   %s\n" ), m_baseSchematic );
    report += wxString::Format( wxS( "Local:  %s\n" ), m_localSchematic );
    report += wxString::Format( wxS( "Server: %s\n\n" ), m_serverSchematic );

    report += wxS( "Summary\n" );
    report += wxS( "-------\n" );
    report += wxString::Format( wxS( "Total actions:           %zu\n" ), aPlan.GetActionCount() );
    report += wxString::Format( wxS( "Auto-merged:             %zu\n" ), aPlan.auto_merged );
    report += wxString::Format( wxS( "Local changes kept:      %zu\n" ), aPlan.local_kept );
    report += wxString::Format( wxS( "Server changes applied:  %zu\n" ), aPlan.server_kept );
    report += wxString::Format( wxS( "Deleted elements:        %zu\n" ), aPlan.deleted );
    report += wxString::Format( wxS( "Conflicts resolved:      %zu\n\n" ),
                                aPlan.GetConflictCount() );

    report += wxS( "Result\n" );
    report += wxS( "------\n" );
    report += wxString::Format( wxS( "Success:                 %s\n" ),
                                aResult.success ? wxS( "Yes" ) : wxS( "No" ) );
    report += wxString::Format( wxS( "Elements merged:         %zu\n" ), aResult.elements_merged );

    if( !aResult.merged_schematic.IsEmpty() )
        report += wxString::Format( wxS( "Output schematic:        %s\n" ),
                                    aResult.merged_schematic );

    if( !aResult.merged_pcb.IsEmpty() )
        report += wxString::Format( wxS( "Output PCB:              %s\n" ), aResult.merged_pcb );

    if( !aResult.error.IsEmpty() )
        report += wxString::Format( wxS( "\nError: %s\n" ), aResult.error );

    if( !aPlan.actions.empty() )
    {
        report += wxS( "\nActions\n" );
        report += wxS( "-------\n" );

        for( size_t i = 0; i < aPlan.actions.size(); ++i )
        {
            const PLM_MERGE_ACTION& action = aPlan.actions[i];

            if( !action.NeedsAction() )
                continue;

            report += wxString::Format( wxS( "%zu. %s (%s): %s\n" ), i + 1, action.element_ref,
                                        PLM_MERGE_ALGORITHM::GetResolutionName( action.resolution ),
                                        action.property_name );

            if( !action.base_value.IsEmpty() )
                report += wxString::Format( wxS( "    Base:   %s\n" ), action.base_value );

            if( !action.local_value.IsEmpty() )
                report += wxString::Format( wxS( "    Local:  %s\n" ), action.local_value );

            if( !action.server_value.IsEmpty() )
                report += wxString::Format( wxS( "    Server: %s\n" ), action.server_value );

            if( !action.merged_value.IsEmpty() )
                report += wxString::Format( wxS( "    Merged: %s\n" ), action.merged_value );
        }
    }

    reportFile.Write( report );
    reportFile.Close();
    return true;
}


bool PLM_MERGE_GENERATOR::LoadSourceFiles()
{
    auto loadFile = [this]( const wxString& aPath, wxString& aContent ) -> bool
    {
        if( aPath.IsEmpty() )
            return true;

        wxFile file( aPath );

        if( !file.IsOpened() )
        {
            m_lastError = wxString::Format( wxS( "Cannot open file: %s" ), aPath );
            return false;
        }

        wxFileOffset len = file.Length();
        wxCharBuffer buf( len );
        file.Read( buf.data(), len );
        aContent = wxString::FromUTF8( buf.data(), len );
        return true;
    };

    if( !loadFile( m_baseSchematic, m_baseSchematicContent ) )
        return false;

    if( !loadFile( m_basePCB, m_basePCBContent ) )
        return false;

    if( !loadFile( m_localSchematic, m_localSchematicContent ) )
        return false;

    if( !loadFile( m_localPCB, m_localPCBContent ) )
        return false;

    if( !loadFile( m_serverSchematic, m_serverSchematicContent ) )
        return false;

    if( !loadFile( m_serverPCB, m_serverPCBContent ) )
        return false;

    return true;
}


wxString PLM_MERGE_GENERATOR::GenerateMergedSchematic( const PLM_MERGE_PLAN& aPlan,
                                                        const std::set<size_t>& aActions )
{
    wxString result = m_baseSchematicContent;

    for( size_t idx : aActions )
    {
        if( idx >= aPlan.actions.size() )
            continue;

        const PLM_MERGE_ACTION& action = aPlan.actions[idx];

        if( !action.affects_schematic )
            continue;

        result = ApplySchematicAction( result, action );

        if( result.IsEmpty() && !m_lastError.IsEmpty() )
            return wxEmptyString;
    }

    return result;
}


wxString PLM_MERGE_GENERATOR::GenerateMergedPCB( const PLM_MERGE_PLAN& aPlan,
                                                  const std::set<size_t>& aActions )
{
    wxString result = m_basePCBContent;

    for( size_t idx : aActions )
    {
        if( idx >= aPlan.actions.size() )
            continue;

        const PLM_MERGE_ACTION& action = aPlan.actions[idx];

        if( !action.affects_pcb )
            continue;

        result = ApplyPCBAction( result, action );

        if( result.IsEmpty() && !m_lastError.IsEmpty() )
            return wxEmptyString;
    }

    return result;
}


wxString PLM_MERGE_GENERATOR::ApplySchematicAction( const wxString& aContent,
                                                     const PLM_MERGE_ACTION& aAction )
{
    wxString result = aContent;

    switch( aAction.resolution )
    {
    case PLM_MERGE_RESOLUTION::NO_CHANGE:
        break;

    case PLM_MERGE_RESOLUTION::TAKE_LOCAL:
    case PLM_MERGE_RESOLUTION::TAKE_SERVER:
    case PLM_MERGE_RESOLUTION::AUTO_MERGED:
    {
        if( aAction.property_name.IsEmpty() )
        {
            bool fromLocal = ( aAction.resolution == PLM_MERGE_RESOLUTION::TAKE_LOCAL );
            wxString newElement = GetElementFromSource( aAction.element_uuid, fromLocal, true );

            if( newElement.IsEmpty() )
            {
                m_lastError = wxString::Format( wxS( "Element %s not found in source" ),
                                                aAction.element_uuid );
                return wxEmptyString;
            }

            int pos = FindElementByUuid( result, aAction.element_uuid );

            if( pos != wxNOT_FOUND )
            {
                int endPos = FindElementEnd( result, pos );
                result = result.Left( pos ) + newElement + result.Mid( endPos );
            }
            else
            {
                int insertPos = result.Find( wxS( ")" ) );

                if( insertPos == wxNOT_FOUND )
                    insertPos = result.Length();
                else
                    --insertPos;

                result = result.Left( insertPos ) + wxS( "\n  " ) + newElement +
                         result.Mid( insertPos );
            }
        }
        else
        {
            int pos = FindElementByUuid( result, aAction.element_uuid );

            if( pos != wxNOT_FOUND )
            {
                int endPos = FindElementEnd( result, pos );
                wxString elementContent = result.Mid( pos, endPos - pos );
                wxString modifiedElement =
                        ReplaceProperty( elementContent, aAction.property_name,
                                         aAction.merged_value );
                result = result.Left( pos ) + modifiedElement + result.Mid( endPos );
            }
        }

        break;
    }

    case PLM_MERGE_RESOLUTION::DELETED:
    {
        int pos = FindElementByUuid( result, aAction.element_uuid );

        if( pos != wxNOT_FOUND )
        {
            int endPos = FindElementEnd( result, pos );
            result = result.Left( pos ) + result.Mid( endPos );
        }

        break;
    }

    case PLM_MERGE_RESOLUTION::CONFLICT:
        break;
    }

    return result;
}


wxString PLM_MERGE_GENERATOR::ApplyPCBAction( const wxString& aContent,
                                               const PLM_MERGE_ACTION& aAction )
{
    wxString result = aContent;

    switch( aAction.resolution )
    {
    case PLM_MERGE_RESOLUTION::NO_CHANGE:
        break;

    case PLM_MERGE_RESOLUTION::TAKE_LOCAL:
    case PLM_MERGE_RESOLUTION::TAKE_SERVER:
    case PLM_MERGE_RESOLUTION::AUTO_MERGED:
    {
        if( aAction.property_name.IsEmpty() )
        {
            bool fromLocal = ( aAction.resolution == PLM_MERGE_RESOLUTION::TAKE_LOCAL );
            wxString newElement = GetElementFromSource( aAction.element_uuid, fromLocal, false );

            if( newElement.IsEmpty() )
            {
                m_lastError = wxString::Format( wxS( "Element %s not found in source" ),
                                                aAction.element_uuid );
                return wxEmptyString;
            }

            int pos = FindElementByUuid( result, aAction.element_uuid );

            if( pos != wxNOT_FOUND )
            {
                int endPos = FindElementEnd( result, pos );
                result = result.Left( pos ) + newElement + result.Mid( endPos );
            }
            else
            {
                int insertPos = result.Find( wxS( ")" ) );

                if( insertPos == wxNOT_FOUND )
                    insertPos = result.Length();
                else
                    --insertPos;

                result = result.Left( insertPos ) + wxS( "\n  " ) + newElement +
                         result.Mid( insertPos );
            }
        }
        else
        {
            int pos = FindElementByUuid( result, aAction.element_uuid );

            if( pos != wxNOT_FOUND )
            {
                int endPos = FindElementEnd( result, pos );
                wxString elementContent = result.Mid( pos, endPos - pos );
                wxString modifiedElement =
                        ReplaceProperty( elementContent, aAction.property_name,
                                         aAction.merged_value );
                result = result.Left( pos ) + modifiedElement + result.Mid( endPos );
            }
        }

        break;
    }

    case PLM_MERGE_RESOLUTION::DELETED:
    {
        int pos = FindElementByUuid( result, aAction.element_uuid );

        if( pos != wxNOT_FOUND )
        {
            int endPos = FindElementEnd( result, pos );
            result = result.Left( pos ) + result.Mid( endPos );
        }

        break;
    }

    case PLM_MERGE_RESOLUTION::CONFLICT:
        break;
    }

    return result;
}


int PLM_MERGE_GENERATOR::FindElementByUuid( const wxString& aContent, const wxString& aUuid )
{
    wxString pattern = wxString::Format( wxS( "\\(uuid \"%s\"\\)" ), aUuid );
    wxRegEx regex( pattern );

    if( !regex.IsValid() )
        return wxNOT_FOUND;

    size_t start, len;

    if( !regex.Matches( aContent ) )
        return wxNOT_FOUND;

    if( !regex.GetMatch( &start, &len ) )
        return wxNOT_FOUND;

    int pos = static_cast<int>( start );
    int depth = 0;

    while( pos > 0 )
    {
        --pos;

        if( aContent[pos] == ')' )
            ++depth;
        else if( aContent[pos] == '(' )
        {
            if( depth == 0 )
                return pos;

            --depth;
        }
    }

    return wxNOT_FOUND;
}


int PLM_MERGE_GENERATOR::FindElementEnd( const wxString& aContent, int aStartPos )
{
    if( aStartPos < 0 || static_cast<size_t>( aStartPos ) >= aContent.Length() )
        return aContent.Length();

    int depth = 0;
    size_t pos = aStartPos;

    while( pos < aContent.Length() )
    {
        if( aContent[pos] == '(' )
            ++depth;
        else if( aContent[pos] == ')' )
        {
            --depth;

            if( depth == 0 )
                return pos + 1;
        }

        ++pos;
    }

    return aContent.Length();
}


wxString PLM_MERGE_GENERATOR::ReplaceProperty( const wxString& aContent,
                                                const wxString& aProperty,
                                                const wxString& aNewValue )
{
    wxString pattern = wxString::Format( wxS( "\\(%s \"[^\"]*\"\\)" ), aProperty );
    wxRegEx regex( pattern );

    if( !regex.IsValid() )
        return aContent;

    wxString replacement = wxString::Format( wxS( "(%s \"%s\")" ), aProperty, aNewValue );
    wxString result = aContent;
    regex.Replace( &result, replacement );
    return result;
}


wxString PLM_MERGE_GENERATOR::GetElementFromSource( const wxString& aUuid, bool aFromLocal,
                                                     bool aForSchematic )
{
    wxString sourceContent;

    if( aForSchematic )
        sourceContent = aFromLocal ? m_localSchematicContent : m_serverSchematicContent;
    else
        sourceContent = aFromLocal ? m_localPCBContent : m_serverPCBContent;

    int pos = FindElementByUuid( sourceContent, aUuid );

    if( pos == wxNOT_FOUND )
        return wxEmptyString;

    int endPos = FindElementEnd( sourceContent, pos );
    return sourceContent.Mid( pos, endPos - pos );
}


bool PLM_MERGE_GENERATOR::CreateBackups()
{
    auto backupFile = [this]( const wxString& aPath ) -> bool
    {
        if( aPath.IsEmpty() )
            return true;

        if( !wxFileExists( aPath ) )
            return true;

        wxString backupPath = aPath + wxS( ".bak" );

        if( !wxCopyFile( aPath, backupPath ) )
        {
            m_lastError = wxString::Format( wxS( "Failed to create backup: %s" ), backupPath );
            return false;
        }

        return true;
    };

    if( !backupFile( m_baseSchematic ) )
        return false;

    if( !backupFile( m_basePCB ) )
        return false;

    return true;
}


bool PLM_MERGE_GENERATOR::ValidateOutput( const wxString& aPath, bool aIsSchematic )
{
    if( !wxFileExists( aPath ) )
    {
        m_lastError = wxString::Format( wxS( "Output file not created: %s" ), aPath );
        return false;
    }

    wxFile file( aPath );

    if( !file.IsOpened() )
    {
        m_lastError = wxString::Format( wxS( "Cannot read output file: %s" ), aPath );
        return false;
    }

    wxFileOffset len = file.Length();
    wxCharBuffer buf( len );
    file.Read( buf.data(), len );
    wxString content = wxString::FromUTF8( buf.data(), len );

    if( aIsSchematic )
    {
        if( content.Find( wxS( "(kicad_sch" ) ) == wxNOT_FOUND )
        {
            m_lastError = wxS( "Invalid schematic file format" );
            return false;
        }
    }
    else
    {
        if( content.Find( wxS( "(kicad_pcb" ) ) == wxNOT_FOUND )
        {
            m_lastError = wxS( "Invalid PCB file format" );
            return false;
        }
    }

    int openParens = 0;
    int closeParens = 0;

    for( size_t i = 0; i < content.Length(); ++i )
    {
        if( content[i] == '(' )
            ++openParens;
        else if( content[i] == ')' )
            ++closeParens;
    }

    if( openParens != closeParens )
    {
        m_lastError = wxS( "Unbalanced parentheses in output file" );
        return false;
    }

    return true;
}


bool PLM_MERGE_GENERATOR::ReportProgress( size_t aCurrent, size_t aTotal,
                                           const wxString& aDescription )
{
    if( m_options.progress_callback )
        return m_options.progress_callback( aCurrent, aTotal, aDescription );

    return true;
}
