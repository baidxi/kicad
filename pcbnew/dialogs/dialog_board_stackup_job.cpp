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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <dialogs/dialog_board_stackup_job.h>

#include <i18n_utility.h>
#include <map>
#include <wildcards_and_files_ext.h>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

static std::map<JOB_EXPORT_PCB_STACKUP::OUTPUT_FORMAT, wxString> stackupOutputFormatMap = {
    { JOB_EXPORT_PCB_STACKUP::OUTPUT_FORMAT::CSV, _HKI( "CSV" ) },
    { JOB_EXPORT_PCB_STACKUP::OUTPUT_FORMAT::REPORT, _HKI( "Report" ) }
};


DIALOG_BOARD_STACKUP_JOB::DIALOG_BOARD_STACKUP_JOB( wxWindow* aParent,
                                                    JOB_EXPORT_PCB_STACKUP* aJob ) :
        DIALOG_SHIM( aParent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                     wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER ),
        m_job( aJob )
{
    m_fieldToggles = {
        { _( "Thickness" ), &m_job->m_includeThickness },
        { _( "Material" ), &m_job->m_includeMaterial },
        { _( "Color" ), &m_job->m_includeColor },
        { _( "Epsilon R" ), &m_job->m_includeEpsilonR },
        { _( "Loss tangent" ), &m_job->m_includeLossTangent },
        { _( "Board finish" ), &m_job->m_includeFinish },
        { _( "Board options" ), &m_job->m_includeBoardOptions },
    };

    wxBoxSizer* mainSizer = new wxBoxSizer( wxVERTICAL );
    wxFlexGridSizer* grid = new wxFlexGridSizer( 0, 2, 5, 5 );
    grid->AddGrowableCol( 1, 1 );

    grid->Add( new wxStaticText( this, wxID_ANY, _( "Output file:" ) ), 0,
               wxALIGN_CENTER_VERTICAL | wxRIGHT, 5 );
    m_textCtrlOutputPath = new wxTextCtrl( this, wxID_ANY );
    grid->Add( m_textCtrlOutputPath, 1, wxEXPAND );

    grid->Add( new wxStaticText( this, wxID_ANY, _( "Format:" ) ), 0,
               wxALIGN_CENTER_VERTICAL | wxRIGHT, 5 );
    m_choiceFormat = new wxChoice( this, wxID_ANY );

    for( const auto& [format, name] : stackupOutputFormatMap )
        m_choiceFormat->Append( wxGetTranslation( name ) );

    grid->Add( m_choiceFormat, 1, wxEXPAND );

    grid->Add( new wxStaticText( this, wxID_ANY, _( "Units:" ) ), 0,
               wxALIGN_CENTER_VERTICAL | wxRIGHT, 5 );
    m_choiceUnits = new wxChoice( this, wxID_ANY );
    m_choiceUnits->Append( _( "Millimeters" ) );
    m_choiceUnits->Append( _( "Inches" ) );
    grid->Add( m_choiceUnits, 1, wxEXPAND );

    mainSizer->Add( grid, 0, wxEXPAND | wxALL, 10 );
    mainSizer->Add( new wxStaticText( this, wxID_ANY, _( "Include fields:" ) ), 0,
                    wxLEFT | wxRIGHT | wxTOP, 10 );

    wxBoxSizer* checksSizer = new wxBoxSizer( wxVERTICAL );

    for( FIELD_TOGGLE& field : m_fieldToggles )
    {
        field.checkbox = new wxCheckBox( this, wxID_ANY, field.label );
        checksSizer->Add( field.checkbox, 0, wxBOTTOM, 3 );
    }

    mainSizer->Add( checksSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10 );

    wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer();
    buttons->AddButton( new wxButton( this, wxID_OK ) );
    buttons->AddButton( new wxButton( this, wxID_CANCEL ) );
    buttons->Realize();
    mainSizer->Add( buttons, 0, wxEXPAND | wxALL, 5 );

    SetSizer( mainSizer );

    if( aJob )
        SetTitle( aJob->GetSettingsDialogTitle() );

    m_choiceFormat->Bind( wxEVT_CHOICE, &DIALOG_BOARD_STACKUP_JOB::OnFormatChoice, this );

    SetupStandardButtons();
    finishDialogSettings();
}


JOB_EXPORT_PCB_STACKUP::OUTPUT_FORMAT DIALOG_BOARD_STACKUP_JOB::getSelectedFormat()
{
    int  selIndx = m_choiceFormat->GetSelection();
    auto it = stackupOutputFormatMap.begin();
    std::advance( it, selIndx );
    return it->first;
}


void DIALOG_BOARD_STACKUP_JOB::setSelectedFormat( JOB_EXPORT_PCB_STACKUP::OUTPUT_FORMAT aFormat )
{
    auto it = stackupOutputFormatMap.find( aFormat );

    if( it != stackupOutputFormatMap.end() )
        m_choiceFormat->SetSelection( std::distance( stackupOutputFormatMap.begin(), it ) );
}


void DIALOG_BOARD_STACKUP_JOB::OnFormatChoice( wxCommandEvent& aEvent )
{
    if( m_textCtrlOutputPath->GetValue().IsEmpty() )
        return;

    wxFileName fn( m_textCtrlOutputPath->GetValue() );

    if( getSelectedFormat() == JOB_EXPORT_PCB_STACKUP::OUTPUT_FORMAT::REPORT )
        fn.SetExt( FILEEXT::ReportFileExtension );
    else
        fn.SetExt( FILEEXT::CsvFileExtension );

    m_textCtrlOutputPath->SetValue( fn.GetFullPath() );
}


bool DIALOG_BOARD_STACKUP_JOB::TransferDataToWindow()
{
    m_textCtrlOutputPath->SetValue( m_job->GetConfiguredOutputPath() );
    setSelectedFormat( m_job->m_format );
    m_choiceUnits->SetSelection( m_job->m_units == JOB_EXPORT_PCB_STACKUP::UNITS::MM ? 0 : 1 );

    for( FIELD_TOGGLE& field : m_fieldToggles )
        field.checkbox->SetValue( *field.jobFlag );

    return true;
}


bool DIALOG_BOARD_STACKUP_JOB::TransferDataFromWindow()
{
    m_job->SetConfiguredOutputPath( m_textCtrlOutputPath->GetValue() );
    m_job->m_format = getSelectedFormat();
    m_job->m_units = m_choiceUnits->GetSelection() == 0 ? JOB_EXPORT_PCB_STACKUP::UNITS::MM
                                                        : JOB_EXPORT_PCB_STACKUP::UNITS::INCH;

    for( FIELD_TOGGLE& field : m_fieldToggles )
        *field.jobFlag = field.checkbox->GetValue();

    return true;
}
