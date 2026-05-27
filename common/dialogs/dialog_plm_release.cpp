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

#include "dialog_plm_release.h"

#include <plm/plm_plugin.h>

#include <wx/msgdlg.h>


DIALOG_PLM_RELEASE::DIALOG_PLM_RELEASE( wxWindow* aParent, PLM_PLUGIN* aPlugin,
                                         const wxString& aProjectId ) :
        DIALOG_PLM_RELEASE_BASE( aParent ),
        m_plugin( aPlugin ),
        m_projectId( aProjectId ),
        m_currentPage( PAGE_INFO )
{
    m_checksList->AppendTextColumn( _( "Check" ), wxDATAVIEW_CELL_INERT, 200,
                                    wxALIGN_LEFT,
                                    wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE );
    m_checksList->AppendTextColumn( _( "Status" ), wxDATAVIEW_CELL_INERT, 100,
                                    wxALIGN_CENTER,
                                    wxDATAVIEW_COL_RESIZABLE );
    m_checksList->AppendTextColumn( _( "Message" ), wxDATAVIEW_CELL_INERT, 300,
                                    wxALIGN_LEFT,
                                    wxDATAVIEW_COL_RESIZABLE );

    initVersionSchemes();

    if( m_plugin )
    {
        wxString suggested = m_plugin->SuggestNextVersion( m_projectId,
                                                            PLM_VERSION_SCHEME::SEMVER );

        if( !suggested.IsEmpty() )
            m_versionCtrl->SetValue( suggested );
    }

    updateNavigationButtons();

    SetMinSize( wxSize( 700, 550 ) );
    Layout();
}


DIALOG_PLM_RELEASE::~DIALOG_PLM_RELEASE()
{
}


void DIALOG_PLM_RELEASE::initVersionSchemes()
{
    m_schemeChoice->Append( _( "Semantic (major.minor.patch)" ) );
    m_schemeChoice->Append( _( "Calendar (YYYY.MM.DD)" ) );
    m_schemeChoice->Append( _( "Sequential (Rev A, Rev B, ...)" ) );
    m_schemeChoice->Append( _( "Custom" ) );
    m_schemeChoice->SetSelection( 0 );
}


void DIALOG_PLM_RELEASE::navigateToPage( int aPage )
{
    if( aPage < PAGE_INFO || aPage > PAGE_SUMMARY )
        return;

    if( aPage == PAGE_CHECKS && m_currentPage == PAGE_INFO )
        runValidationChecks();

    if( aPage == PAGE_SUMMARY )
        buildSummary();

    m_currentPage = aPage;
    m_wizard->SetSelection( m_currentPage );
    updateNavigationButtons();
}


void DIALOG_PLM_RELEASE::updateNavigationButtons()
{
    m_backBtn->Enable( m_currentPage > PAGE_INFO );

    if( m_currentPage == PAGE_SUMMARY )
    {
        m_nextBtn->SetLabel( _( "Create Release" ) );
    }
    else
    {
        m_nextBtn->SetLabel( _( "Next >" ) );
    }
}


void DIALOG_PLM_RELEASE::runValidationChecks()
{
    m_checksList->DeleteAllItems();
    m_checks.clear();

    if( !m_plugin )
        return;

    m_checks = m_plugin->ValidateForRelease( m_projectId );

    for( const PLM_RELEASE_CHECK& check : m_checks )
    {
        wxVector<wxVariant> data;

        data.push_back( wxVariant( check.name ) );
        data.push_back( wxVariant( check.passed ? wxString( _( "Passed" ) )
                                                : wxString( _( "Failed" ) ) ) );
        data.push_back( wxVariant( check.message ) );

        m_checksList->AppendItem( data );
    }
}


void DIALOG_PLM_RELEASE::buildSummary()
{
    wxString summary;

    summary += wxString::Format( _( "Version: %s\n" ), m_versionCtrl->GetValue() );
    summary += wxString::Format( _( "Scheme: %s\n" ),
                                  getVersionSchemeName( static_cast<PLM_VERSION_SCHEME>(
                                          m_schemeChoice->GetSelection() ) ) );
    summary += wxString::Format( _( "Title: %s\n" ), m_titleCtrl->GetValue() );
    summary += wxS( "\n" );

    // Validation results
    int passed = 0;
    int failed = 0;

    for( const PLM_RELEASE_CHECK& check : m_checks )
    {
        if( check.passed )
            ++passed;
        else
            ++failed;
    }

    summary += wxString::Format( _( "Validation: %d passed, %d failed\n" ), passed, failed );
    summary += wxS( "\n" );

    // Selected outputs
    summary += _( "Manufacturing Outputs:\n" );

    if( m_chkGerber->GetValue() )
        summary += wxS( "  - Gerber\n" );

    if( m_chkDrill->GetValue() )
        summary += wxS( "  - Drill\n" );

    if( m_chkBom->GetValue() )
        summary += wxS( "  - BOM\n" );

    if( m_chkPnP->GetValue() )
        summary += wxS( "  - Pick and Place\n" );

    if( m_chkPdf->GetValue() )
        summary += wxS( "  - PDF\n" );

    if( m_chk3D->GetValue() )
        summary += wxS( "  - 3D Model\n" );

    if( !m_notesCtrl->GetValue().IsEmpty() )
    {
        summary += wxS( "\n" );
        summary += _( "Release Notes:\n" );
        summary += m_notesCtrl->GetValue();
    }

    m_summaryText->SetValue( summary );
}


PLM_RELEASE_OPTIONS DIALOG_PLM_RELEASE::buildReleaseOptions() const
{
    PLM_RELEASE_OPTIONS options;

    options.version = m_versionCtrl->GetValue();
    options.version_scheme = static_cast<PLM_VERSION_SCHEME>( m_schemeChoice->GetSelection() );
    options.title = m_titleCtrl->GetValue();
    options.notes = m_notesCtrl->GetValue();

    return options;
}


bool DIALOG_PLM_RELEASE::createRelease()
{
    if( !m_plugin )
        return false;

    PLM_RELEASE_OPTIONS options = buildReleaseOptions();

    PLM_RELEASE_RESULT result = m_plugin->CreateRelease( m_projectId, options );

    if( !result.success )
    {
        wxMessageBox( wxString::Format( _( "Failed to create release:\n%s" ),
                                         result.error_message ),
                      _( "Error" ), wxOK | wxICON_ERROR, this );
        return false;
    }

    m_release = result.release;

    PLM_RELEASE_RESULT finalResult = m_plugin->FinalizeRelease( m_release.release_id );

    if( !finalResult.success )
    {
        wxMessageBox( wxString::Format( _( "Release created but finalization failed:\n%s" ),
                                         finalResult.error_message ),
                      _( "Warning" ), wxOK | wxICON_WARNING, this );
        return true;
    }

    m_release = finalResult.release;
    return true;
}


wxString DIALOG_PLM_RELEASE::getVersionSchemeName( PLM_VERSION_SCHEME aScheme ) const
{
    switch( aScheme )
    {
    case PLM_VERSION_SCHEME::SEMVER:     return _( "Semantic" );
    case PLM_VERSION_SCHEME::CALVER:     return _( "Calendar" );
    case PLM_VERSION_SCHEME::SEQUENTIAL: return _( "Sequential" );
    case PLM_VERSION_SCHEME::CUSTOM:     return _( "Custom" );
    default:                             return _( "Unknown" );
    }
}


wxString DIALOG_PLM_RELEASE::getArtifactTypeName( PLM_ARTIFACT_TYPE aType ) const
{
    switch( aType )
    {
    case PLM_ARTIFACT_TYPE::GERBER:          return _( "Gerber" );
    case PLM_ARTIFACT_TYPE::DRILL:           return _( "Drill" );
    case PLM_ARTIFACT_TYPE::PICK_AND_PLACE:  return _( "Pick and Place" );
    case PLM_ARTIFACT_TYPE::BOM_EXPORT:      return _( "BOM" );
    case PLM_ARTIFACT_TYPE::SCHEMATIC_PDF:   return _( "Schematic PDF" );
    case PLM_ARTIFACT_TYPE::RENDER_3D:       return _( "3D Render" );
    default:                                 return _( "Other" );
    }
}


void DIALOG_PLM_RELEASE::onBack( wxCommandEvent& event )
{
    navigateToPage( m_currentPage - 1 );
}


void DIALOG_PLM_RELEASE::onNext( wxCommandEvent& event )
{
    if( m_currentPage == PAGE_INFO )
    {
        if( m_versionCtrl->GetValue().IsEmpty() )
        {
            wxMessageBox( _( "Please enter a version string." ), _( "Validation" ),
                          wxOK | wxICON_WARNING, this );
            return;
        }
    }

    if( m_currentPage == PAGE_SUMMARY )
    {
        if( createRelease() )
            EndModal( wxID_OK );

        return;
    }

    navigateToPage( m_currentPage + 1 );
}


void DIALOG_PLM_RELEASE::onCancel( wxCommandEvent& event )
{
    EndModal( wxID_CANCEL );
}
