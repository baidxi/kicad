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

#include "dialog_plm_eco.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/datetime.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>


enum
{
    ID_CREATE_NEW = wxID_HIGHEST + 100,
    ID_SUBMIT_CREATE,
    ID_CANCEL_CREATE,
    ID_APPROVE,
    ID_REJECT,
    ID_SAVE_IMPACT
};


DIALOG_PLM_ECO::DIALOG_PLM_ECO( wxWindow* aParent ) :
        DIALOG_PLM_ECO_BASE( aParent ),
        m_plugin( nullptr ),
        m_selectedIndex( -1 ),
        m_createMode( false ),
        m_ecoCreated( false ),
        m_createPanel( nullptr ),
        m_createTitleCtrl( nullptr ),
        m_createTypeChoice( nullptr ),
        m_createPriorityChoice( nullptr ),
        m_createReasonChoice( nullptr ),
        m_createReasonDetailCtrl( nullptr ),
        m_createDescriptionCtrl( nullptr ),
        m_createNewBtn( nullptr ),
        m_submitCreateBtn( nullptr ),
        m_cancelCreateBtn( nullptr ),
        m_impactPanel( nullptr ),
        m_costImpactCtrl( nullptr ),
        m_scheduleImpactCtrl( nullptr ),
        m_mfgImpactCtrl( nullptr ),
        m_saveImpactBtn( nullptr ),
        m_statusPanel( nullptr ),
        m_approveBtn( nullptr ),
        m_rejectBtn( nullptr )
{
    initCommon();
}


DIALOG_PLM_ECO::DIALOG_PLM_ECO( wxWindow* aParent, PLM_PLUGIN* aPlugin ) :
        DIALOG_PLM_ECO_BASE( aParent ),
        m_plugin( aPlugin ),
        m_selectedIndex( -1 ),
        m_createMode( false ),
        m_ecoCreated( false ),
        m_createPanel( nullptr ),
        m_createTitleCtrl( nullptr ),
        m_createTypeChoice( nullptr ),
        m_createPriorityChoice( nullptr ),
        m_createReasonChoice( nullptr ),
        m_createReasonDetailCtrl( nullptr ),
        m_createDescriptionCtrl( nullptr ),
        m_createNewBtn( nullptr ),
        m_submitCreateBtn( nullptr ),
        m_cancelCreateBtn( nullptr ),
        m_impactPanel( nullptr ),
        m_costImpactCtrl( nullptr ),
        m_scheduleImpactCtrl( nullptr ),
        m_mfgImpactCtrl( nullptr ),
        m_saveImpactBtn( nullptr ),
        m_statusPanel( nullptr ),
        m_approveBtn( nullptr ),
        m_rejectBtn( nullptr )
{
    initCommon();

    if( m_plugin )
    {
        buildExtendedUI();
    }
}


void DIALOG_PLM_ECO::initCommon()
{
    m_fieldsLabel->Hide();
    m_fieldsScrolled->Hide();

    SetupStandardButtons();
    finishDialogSettings();
}


DIALOG_PLM_ECO::~DIALOG_PLM_ECO()
{
}


void DIALOG_PLM_ECO::buildExtendedUI()
{
    wxBoxSizer* mainSizer = dynamic_cast<wxBoxSizer*>( GetSizer() );

    if( !mainSizer )
        return;

    // Insert "Create New ECO" button above the standard buttons
    m_createNewBtn = new wxButton( this, ID_CREATE_NEW, _( "Create New ECO..." ) );
    mainSizer->Insert( mainSizer->GetItemCount() - 1, m_createNewBtn, 0, wxLEFT | wxRIGHT, 10 );

    m_createNewBtn->Bind( wxEVT_BUTTON, &DIALOG_PLM_ECO::onCreateNewEco, this );

    // Create the creation mode panel (initially hidden)
    m_createPanel = new wxPanel( this );
    wxBoxSizer* createSizer = new wxBoxSizer( wxVERTICAL );

    wxStaticBoxSizer* formSizer = new wxStaticBoxSizer( wxVERTICAL, m_createPanel,
                                                         _( "New Engineering Change" ) );

    wxFlexGridSizer* fgSizer = new wxFlexGridSizer( 2, 10, 5 );
    fgSizer->AddGrowableCol( 1 );

    // Title
    fgSizer->Add( new wxStaticText( formSizer->GetStaticBox(), wxID_ANY, _( "Title:" ) ),
                  0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    m_createTitleCtrl = new wxTextCtrl( formSizer->GetStaticBox(), wxID_ANY );
    fgSizer->Add( m_createTitleCtrl, 1, wxEXPAND );

    // Type
    fgSizer->Add( new wxStaticText( formSizer->GetStaticBox(), wxID_ANY, _( "Type:" ) ),
                  0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    m_createTypeChoice = new wxChoice( formSizer->GetStaticBox(), wxID_ANY );
    m_createTypeChoice->Append( _( "Change Request (ECR)" ) );
    m_createTypeChoice->Append( _( "Change Order (ECO)" ) );
    m_createTypeChoice->SetSelection( 0 );
    fgSizer->Add( m_createTypeChoice, 1, wxEXPAND );

    // Priority
    fgSizer->Add( new wxStaticText( formSizer->GetStaticBox(), wxID_ANY, _( "Priority:" ) ),
                  0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    m_createPriorityChoice = new wxChoice( formSizer->GetStaticBox(), wxID_ANY );
    m_createPriorityChoice->Append( _( "Low" ) );
    m_createPriorityChoice->Append( _( "Normal" ) );
    m_createPriorityChoice->Append( _( "High" ) );
    m_createPriorityChoice->Append( _( "Critical" ) );
    m_createPriorityChoice->SetSelection( 1 );
    fgSizer->Add( m_createPriorityChoice, 1, wxEXPAND );

    // Reason
    fgSizer->Add( new wxStaticText( formSizer->GetStaticBox(), wxID_ANY, _( "Reason:" ) ),
                  0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    m_createReasonChoice = new wxChoice( formSizer->GetStaticBox(), wxID_ANY );
    m_createReasonChoice->Append( _( "Corrective" ) );
    m_createReasonChoice->Append( _( "Preventive" ) );
    m_createReasonChoice->Append( _( "Cost Reduction" ) );
    m_createReasonChoice->Append( _( "Performance" ) );
    m_createReasonChoice->Append( _( "Regulatory" ) );
    m_createReasonChoice->Append( _( "Supply Chain" ) );
    m_createReasonChoice->Append( _( "Other" ) );
    m_createReasonChoice->SetSelection( 6 );
    fgSizer->Add( m_createReasonChoice, 1, wxEXPAND );

    // Reason detail
    fgSizer->Add( new wxStaticText( formSizer->GetStaticBox(), wxID_ANY, _( "Reason Detail:" ) ),
                  0, wxALIGN_RIGHT | wxALIGN_TOP );
    m_createReasonDetailCtrl = new wxTextCtrl( formSizer->GetStaticBox(), wxID_ANY, wxEmptyString,
                                                wxDefaultPosition, wxSize( -1, 50 ),
                                                wxTE_MULTILINE );
    fgSizer->Add( m_createReasonDetailCtrl, 1, wxEXPAND );

    // Description
    fgSizer->Add( new wxStaticText( formSizer->GetStaticBox(), wxID_ANY, _( "Description:" ) ),
                  0, wxALIGN_RIGHT | wxALIGN_TOP );
    m_createDescriptionCtrl = new wxTextCtrl( formSizer->GetStaticBox(), wxID_ANY, wxEmptyString,
                                              wxDefaultPosition, wxSize( -1, 80 ),
                                              wxTE_MULTILINE );
    fgSizer->Add( m_createDescriptionCtrl, 1, wxEXPAND );

    formSizer->Add( fgSizer, 1, wxEXPAND | wxALL, 5 );

    // Impact assessment fields
    wxStaticBoxSizer* impactSizer = new wxStaticBoxSizer( wxVERTICAL, formSizer->GetStaticBox(),
                                                           _( "Impact Assessment" ) );
    wxFlexGridSizer* impactGrid = new wxFlexGridSizer( 2, 10, 5 );
    impactGrid->AddGrowableCol( 1 );

    impactGrid->Add( new wxStaticText( impactSizer->GetStaticBox(), wxID_ANY, _( "Cost:" ) ),
                     0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    m_costImpactCtrl = new wxTextCtrl( impactSizer->GetStaticBox(), wxID_ANY );
    impactGrid->Add( m_costImpactCtrl, 1, wxEXPAND );

    impactGrid->Add( new wxStaticText( impactSizer->GetStaticBox(), wxID_ANY, _( "Schedule:" ) ),
                     0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    m_scheduleImpactCtrl = new wxTextCtrl( impactSizer->GetStaticBox(), wxID_ANY );
    impactGrid->Add( m_scheduleImpactCtrl, 1, wxEXPAND );

    impactGrid->Add(
            new wxStaticText( impactSizer->GetStaticBox(), wxID_ANY, _( "Manufacturing:" ) ),
            0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    m_mfgImpactCtrl = new wxTextCtrl( impactSizer->GetStaticBox(), wxID_ANY );
    impactGrid->Add( m_mfgImpactCtrl, 1, wxEXPAND );

    impactSizer->Add( impactGrid, 1, wxEXPAND | wxALL, 5 );

    formSizer->Add( impactSizer, 0, wxEXPAND | wxTOP, 5 );

    // Submit / Cancel buttons for creation
    wxBoxSizer* createBtnSizer = new wxBoxSizer( wxHORIZONTAL );
    createBtnSizer->AddStretchSpacer();
    m_submitCreateBtn = new wxButton( formSizer->GetStaticBox(), ID_SUBMIT_CREATE,
                                      _( "Submit" ) );
    m_cancelCreateBtn = new wxButton( formSizer->GetStaticBox(), ID_CANCEL_CREATE,
                                      _( "Back to List" ) );
    createBtnSizer->Add( m_cancelCreateBtn, 0, wxRIGHT, 5 );
    createBtnSizer->Add( m_submitCreateBtn, 0 );

    formSizer->Add( createBtnSizer, 0, wxEXPAND | wxTOP, 10 );

    createSizer->Add( formSizer, 1, wxEXPAND | wxALL, 10 );

    m_createPanel->SetSizer( createSizer );
    m_createPanel->Hide();

    mainSizer->Insert( mainSizer->GetItemCount() - 1, m_createPanel, 1, wxEXPAND );

    m_submitCreateBtn->Bind( wxEVT_BUTTON, &DIALOG_PLM_ECO::onSubmitCreate, this );
    m_cancelCreateBtn->Bind( wxEVT_BUTTON, &DIALOG_PLM_ECO::onCancelCreate, this );

    // Status transition buttons (shown in details panel when an ECO is selected)
    m_statusPanel = new wxPanel( m_detailsPanel );
    wxBoxSizer* statusSizer = new wxBoxSizer( wxHORIZONTAL );

    m_approveBtn = new wxButton( m_statusPanel, ID_APPROVE, _( "Approve" ) );
    m_rejectBtn = new wxButton( m_statusPanel, ID_REJECT, _( "Reject" ) );

    statusSizer->Add( m_approveBtn, 0, wxRIGHT, 5 );
    statusSizer->Add( m_rejectBtn, 0 );

    m_statusPanel->SetSizer( statusSizer );
    m_statusPanel->Hide();

    wxBoxSizer* detailsSizer = dynamic_cast<wxBoxSizer*>( m_detailsPanel->GetSizer() );

    if( detailsSizer )
    {
        detailsSizer->Add( m_statusPanel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10 );
    }

    m_approveBtn->Bind( wxEVT_BUTTON, &DIALOG_PLM_ECO::onApproveEco, this );
    m_rejectBtn->Bind( wxEVT_BUTTON, &DIALOG_PLM_ECO::onRejectEco, this );

    // Impact save button below the description in details view
    m_impactPanel = new wxPanel( m_detailsPanel );
    wxBoxSizer* impPanelSizer = new wxBoxSizer( wxVERTICAL );

    wxStaticBoxSizer* impEditSizer = new wxStaticBoxSizer( wxVERTICAL, m_impactPanel,
                                                            _( "Impact Assessment" ) );
    wxStaticText* impNote = new wxStaticText( impEditSizer->GetStaticBox(), wxID_ANY,
                                              _( "Edit impact fields above and save." ) );
    impEditSizer->Add( impNote, 0, wxALL, 5 );

    m_saveImpactBtn = new wxButton( impEditSizer->GetStaticBox(), ID_SAVE_IMPACT,
                                    _( "Save Impact" ) );
    impEditSizer->Add( m_saveImpactBtn, 0, wxALL, 5 );

    impPanelSizer->Add( impEditSizer, 0, wxEXPAND );
    m_impactPanel->SetSizer( impPanelSizer );
    m_impactPanel->Hide();

    if( detailsSizer )
    {
        detailsSizer->Add( m_impactPanel, 0, wxEXPAND | wxLEFT | wxRIGHT, 10 );
    }

    m_saveImpactBtn->Bind( wxEVT_BUTTON, &DIALOG_PLM_ECO::onSaveImpact, this );

    Layout();
}


void DIALOG_PLM_ECO::switchToCreateMode()
{
    m_createMode = true;

    m_infoLabel->Hide();
    m_splitter->Hide();
    m_fieldsLabel->Hide();
    m_fieldsScrolled->Hide();

    if( m_createNewBtn )
        m_createNewBtn->Hide();

    if( m_createPanel )
        m_createPanel->Show();

    m_sdbSizerOK->Hide();
    m_sdbSizerCancel->Hide();

    SetTitle( _( "PLM - Create Engineering Change" ) );
    Layout();
    Fit();
}


void DIALOG_PLM_ECO::switchToSelectMode()
{
    m_createMode = false;

    m_infoLabel->Show();
    m_splitter->Show();

    if( m_createNewBtn )
        m_createNewBtn->Show();

    if( m_createPanel )
        m_createPanel->Hide();

    m_sdbSizerOK->Show();
    m_sdbSizerCancel->Show();

    SetTitle( _( "PLM - Select Engineering Change Order" ) );
    Layout();
    Fit();
}


void DIALOG_PLM_ECO::SetEcoList( const std::vector<PLM_ECO>& aEcos )
{
    m_ecos = aEcos;
    m_ecoList->Clear();

    for( const auto& eco : m_ecos )
    {
        wxString displayText = wxString::Format( wxS( "%s: %s" ),
                                                 eco.eco_number,
                                                 eco.title );
        m_ecoList->Append( displayText );
    }

    if( !m_ecos.empty() )
    {
        m_ecoList->SetSelection( 0 );
        updateDetails( 0 );
    }
}


const PLM_ECO& DIALOG_PLM_ECO::GetSelectedEco() const
{
    if( m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>( m_ecos.size() ) )
    {
        static PLM_ECO emptyEco;
        return emptyEco;
    }

    return m_ecos[m_selectedIndex];
}


PLM_ECO_FIELD_VALUES DIALOG_PLM_ECO::GetFieldValues() const
{
    PLM_ECO_FIELD_VALUES values;

    for( const auto& fc : m_fieldControls )
    {
        wxString value;

        switch( fc.field.field_type )
        {
        case PLM_ECO_FIELD_TYPE::TEXT:
        case PLM_ECO_FIELD_TYPE::MULTILINE:
        case PLM_ECO_FIELD_TYPE::DATE:
            if( wxTextCtrl* textCtrl = dynamic_cast<wxTextCtrl*>( fc.control ) )
            {
                value = textCtrl->GetValue();
            }
            break;

        case PLM_ECO_FIELD_TYPE::DROPDOWN:
            if( wxChoice* choice = dynamic_cast<wxChoice*>( fc.control ) )
            {
                value = choice->GetStringSelection();
            }
            break;

        case PLM_ECO_FIELD_TYPE::CHECKBOX:
            if( wxCheckBox* checkbox = dynamic_cast<wxCheckBox*>( fc.control ) )
            {
                value = checkbox->GetValue() ? wxS( "true" ) : wxS( "false" );
            }
            break;

        default:
            break;
        }

        values[fc.field.field_id] = value;
    }

    return values;
}


bool DIALOG_PLM_ECO::ValidateFields()
{
    for( const auto& fc : m_fieldControls )
    {
        if( !fc.field.required )
            continue;

        wxString value;

        switch( fc.field.field_type )
        {
        case PLM_ECO_FIELD_TYPE::TEXT:
        case PLM_ECO_FIELD_TYPE::MULTILINE:
        case PLM_ECO_FIELD_TYPE::DATE:
            if( wxTextCtrl* textCtrl = dynamic_cast<wxTextCtrl*>( fc.control ) )
            {
                value = textCtrl->GetValue().Trim();
            }
            break;

        case PLM_ECO_FIELD_TYPE::DROPDOWN:
            if( wxChoice* choice = dynamic_cast<wxChoice*>( fc.control ) )
            {
                value = choice->GetStringSelection();
            }
            break;

        case PLM_ECO_FIELD_TYPE::CHECKBOX:
            continue;

        default:
            break;
        }

        if( value.IsEmpty() )
        {
            wxString msg = wxString::Format( _( "Field '%s' is required" ),
                                             fc.field.display_name );
            wxMessageBox( msg, _( "Validation Error" ), wxOK | wxICON_ERROR, this );

            if( fc.control )
                fc.control->SetFocus();

            return false;
        }
    }

    return true;
}


bool DIALOG_PLM_ECO::TransferDataFromWindow()
{
    if( m_createMode )
        return true;

    if( m_ecoCreated )
        return true;

    if( m_selectedIndex < 0 )
    {
        wxMessageBox( _( "Please select an ECO" ), _( "Validation Error" ),
                      wxOK | wxICON_ERROR, this );
        return false;
    }

    if( !ValidateFields() )
        return false;

    return true;
}


void DIALOG_PLM_ECO::onEcoSelected( wxCommandEvent& event )
{
    int selection = m_ecoList->GetSelection();

    if( selection != wxNOT_FOUND )
    {
        updateDetails( selection );
    }

    event.Skip();
}


void DIALOG_PLM_ECO::updateDetails( int aIndex )
{
    if( aIndex < 0 || aIndex >= static_cast<int>( m_ecos.size() ) )
        return;

    m_selectedIndex = aIndex;
    const PLM_ECO& eco = m_ecos[aIndex];

    m_ecoNumberLabel->SetLabel( eco.eco_number );
    m_ecoTitleLabel->SetLabel( eco.title );
    m_ecoOwnerLabel->SetLabel( eco.owner_name );
    m_ecoStatusLabel->SetLabel( eco.GetStatusName() );
    m_ecoDueDateLabel->SetLabel( formatDate( eco.due_date ) );
    m_ecoDescriptionCtrl->SetValue( eco.description );

    clearFieldControls();

    if( !eco.required_fields.empty() )
    {
        createFieldControls( eco.required_fields );
        m_fieldsLabel->Show();
        m_fieldsScrolled->Show();
    }
    else
    {
        m_fieldsLabel->Hide();
        m_fieldsScrolled->Hide();
    }

    updateStatusButtons();
    Layout();
}


void DIALOG_PLM_ECO::updateStatusButtons()
{
    if( !m_statusPanel || !m_impactPanel )
        return;

    if( m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>( m_ecos.size() ) )
    {
        m_statusPanel->Hide();
        m_impactPanel->Hide();
        return;
    }

    const PLM_ECO& eco = m_ecos[m_selectedIndex];

    // Show approve/reject for ECOs in reviewable states
    bool canTransition = ( eco.status == PLM_ECO_STATUS::PENDING_REVIEW
                           || eco.status == PLM_ECO_STATUS::IN_REVIEW );

    m_statusPanel->Show( canTransition );

    // Show impact editing for ECOs that aren't closed
    m_impactPanel->Show( !eco.IsClosed() );

    m_detailsPanel->Layout();
}


void DIALOG_PLM_ECO::onCreateNewEco( wxCommandEvent& aEvent )
{
    switchToCreateMode();
}


void DIALOG_PLM_ECO::onSubmitCreate( wxCommandEvent& aEvent )
{
    if( !m_plugin )
        return;

    wxString title = m_createTitleCtrl->GetValue().Trim();

    if( title.IsEmpty() )
    {
        wxMessageBox( _( "Title is required" ), _( "Validation Error" ),
                      wxOK | wxICON_ERROR, this );
        m_createTitleCtrl->SetFocus();
        return;
    }

    PLM_ECO_CREATE_OPTIONS opts;
    opts.title = title;
    opts.description = m_createDescriptionCtrl->GetValue();
    opts.reason_detail = m_createReasonDetailCtrl->GetValue();
    opts.type = m_createTypeChoice->GetSelection() == 0 ? PLM_ECO_TYPE::ECR : PLM_ECO_TYPE::ECO;
    opts.priority = static_cast<PLM_ECO_PRIORITY>( m_createPriorityChoice->GetSelection() );
    opts.reason = static_cast<PLM_ECO_REASON>( m_createReasonChoice->GetSelection() );

    opts.impact.cost_impact = m_costImpactCtrl->GetValue();
    opts.impact.schedule_impact = m_scheduleImpactCtrl->GetValue();
    opts.impact.manufacturing_impact = m_mfgImpactCtrl->GetValue();

    PLM_ECO_CREATE_RESULT result = m_plugin->CreateECO( opts );

    if( !result.success )
    {
        wxMessageBox( wxString::Format( _( "Failed to create ECO: %s" ),
                                        result.error_message ),
                      _( "Error" ), wxOK | wxICON_ERROR, this );
        return;
    }

    m_createdEco = result.eco;
    m_ecoCreated = true;

    wxMessageBox( wxString::Format( _( "ECO '%s' created successfully." ),
                                    result.eco.eco_number ),
                  _( "ECO Created" ), wxOK | wxICON_INFORMATION, this );

    EndModal( wxID_OK );
}


void DIALOG_PLM_ECO::onCancelCreate( wxCommandEvent& aEvent )
{
    switchToSelectMode();
}


void DIALOG_PLM_ECO::onApproveEco( wxCommandEvent& aEvent )
{
    if( !m_plugin || m_selectedIndex < 0 )
        return;

    const PLM_ECO& eco = m_ecos[m_selectedIndex];

    PLM_ECO_STATUS_TRANSITION transition( eco.eco_id, PLM_ECO_STATUS::APPROVED );
    PLM_ECO_UPDATE_RESULT result = m_plugin->UpdateECOStatus( transition );

    if( !result.success )
    {
        wxMessageBox( wxString::Format( _( "Failed to approve ECO: %s" ),
                                        result.error_message ),
                      _( "Error" ), wxOK | wxICON_ERROR, this );
        return;
    }

    m_ecos[m_selectedIndex] = result.eco;
    updateDetails( m_selectedIndex );

    wxMessageBox( _( "ECO approved." ), _( "Status Updated" ),
                  wxOK | wxICON_INFORMATION, this );
}


void DIALOG_PLM_ECO::onRejectEco( wxCommandEvent& aEvent )
{
    if( !m_plugin || m_selectedIndex < 0 )
        return;

    const PLM_ECO& eco = m_ecos[m_selectedIndex];

    PLM_ECO_STATUS_TRANSITION transition( eco.eco_id, PLM_ECO_STATUS::REJECTED );
    PLM_ECO_UPDATE_RESULT result = m_plugin->UpdateECOStatus( transition );

    if( !result.success )
    {
        wxMessageBox( wxString::Format( _( "Failed to reject ECO: %s" ),
                                        result.error_message ),
                      _( "Error" ), wxOK | wxICON_ERROR, this );
        return;
    }

    m_ecos[m_selectedIndex] = result.eco;
    updateDetails( m_selectedIndex );

    wxMessageBox( _( "ECO rejected." ), _( "Status Updated" ),
                  wxOK | wxICON_INFORMATION, this );
}


void DIALOG_PLM_ECO::onSaveImpact( wxCommandEvent& aEvent )
{
    if( !m_plugin || m_selectedIndex < 0 )
        return;

    const PLM_ECO& eco = m_ecos[m_selectedIndex];

    PLM_ECO_IMPACT impact;
    impact.cost_impact = m_costImpactCtrl->GetValue();
    impact.schedule_impact = m_scheduleImpactCtrl->GetValue();
    impact.manufacturing_impact = m_mfgImpactCtrl->GetValue();

    PLM_ECO_UPDATE_RESULT result = m_plugin->UpdateECOImpact( eco.eco_id, impact );

    if( !result.success )
    {
        wxMessageBox( wxString::Format( _( "Failed to save impact: %s" ),
                                        result.error_message ),
                      _( "Error" ), wxOK | wxICON_ERROR, this );
        return;
    }

    m_ecos[m_selectedIndex] = result.eco;

    wxMessageBox( _( "Impact assessment saved." ), _( "Saved" ),
                  wxOK | wxICON_INFORMATION, this );
}


void DIALOG_PLM_ECO::createFieldControls( const std::vector<PLM_ECO_FIELD>& aFields )
{
    wxFlexGridSizer* fieldSizer = new wxFlexGridSizer( 2, 10, 5 );
    fieldSizer->AddGrowableCol( 1 );

    for( const auto& field : aFields )
    {
        wxStaticText* label = new wxStaticText( m_fieldsScrolled, wxID_ANY,
                                                field.display_name + wxS( ":" ) );
        fieldSizer->Add( label, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );

        wxWindow* control = nullptr;

        switch( field.field_type )
        {
        case PLM_ECO_FIELD_TYPE::TEXT:
        {
            wxTextCtrl* textCtrl = new wxTextCtrl( m_fieldsScrolled, wxID_ANY );

            if( !field.default_value.IsEmpty() )
                textCtrl->SetValue( field.default_value );

            if( !field.description.IsEmpty() )
                textCtrl->SetToolTip( field.description );

            control = textCtrl;
            fieldSizer->Add( control, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL );
            break;
        }

        case PLM_ECO_FIELD_TYPE::MULTILINE:
        {
            wxTextCtrl* textCtrl = new wxTextCtrl( m_fieldsScrolled, wxID_ANY,
                                                   wxEmptyString, wxDefaultPosition,
                                                   wxSize( -1, 60 ),
                                                   wxTE_MULTILINE );

            if( !field.default_value.IsEmpty() )
                textCtrl->SetValue( field.default_value );

            if( !field.description.IsEmpty() )
                textCtrl->SetToolTip( field.description );

            control = textCtrl;
            fieldSizer->Add( control, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL );
            break;
        }

        case PLM_ECO_FIELD_TYPE::DROPDOWN:
        {
            wxChoice* choice = new wxChoice( m_fieldsScrolled, wxID_ANY );

            for( const auto& option : field.options )
            {
                choice->Append( option );
            }

            if( !field.default_value.IsEmpty() )
            {
                int defaultIndex = choice->FindString( field.default_value );

                if( defaultIndex != wxNOT_FOUND )
                    choice->SetSelection( defaultIndex );
            }

            if( !field.description.IsEmpty() )
                choice->SetToolTip( field.description );

            control = choice;
            fieldSizer->Add( control, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL );
            break;
        }

        case PLM_ECO_FIELD_TYPE::CHECKBOX:
        {
            wxCheckBox* checkbox = new wxCheckBox( m_fieldsScrolled, wxID_ANY,
                                                   wxEmptyString );

            if( field.default_value == wxS( "true" ) || field.default_value == wxS( "1" ) )
                checkbox->SetValue( true );

            if( !field.description.IsEmpty() )
                checkbox->SetToolTip( field.description );

            control = checkbox;
            fieldSizer->Add( control, 0, wxALIGN_CENTER_VERTICAL );
            break;
        }

        case PLM_ECO_FIELD_TYPE::DATE:
        {
            wxTextCtrl* textCtrl = new wxTextCtrl( m_fieldsScrolled, wxID_ANY );
            textCtrl->SetHint( _( "YYYY-MM-DD" ) );

            if( !field.default_value.IsEmpty() )
                textCtrl->SetValue( field.default_value );

            if( !field.description.IsEmpty() )
                textCtrl->SetToolTip( field.description );

            control = textCtrl;
            fieldSizer->Add( control, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL );
            break;
        }

        default:
        {
            wxStaticText* placeholder = new wxStaticText( m_fieldsScrolled, wxID_ANY,
                                                          _( "(Unsupported field type)" ) );
            control = placeholder;
            fieldSizer->Add( control, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL );
            break;
        }
        }

        m_fieldControls.push_back( { label, control, field } );
    }

    bFieldsScrollSizer->Add( fieldSizer, 1, wxEXPAND | wxALL, 5 );

    m_fieldsScrolled->SetSizer( bFieldsScrollSizer );
    m_fieldsScrolled->FitInside();
    m_fieldsScrolled->SetScrollRate( 5, 5 );
}


void DIALOG_PLM_ECO::clearFieldControls()
{
    for( auto& fc : m_fieldControls )
    {
        if( fc.label )
            fc.label->Destroy();

        if( fc.control )
            fc.control->Destroy();
    }

    m_fieldControls.clear();

    if( bFieldsScrollSizer )
        bFieldsScrollSizer->Clear();
}


wxString DIALOG_PLM_ECO::formatDate( int64_t aTimestamp ) const
{
    if( aTimestamp == 0 )
        return _( "Not set" );

    wxDateTime dt;
    dt.Set( static_cast<time_t>( aTimestamp ) );

    return dt.FormatISODate();
}
