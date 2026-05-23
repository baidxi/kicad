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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <dialogs/dialog_layer_roles.h>

#include <fstream>

#include <X2_gerber_attributes.h>
#include <gerber_file_image.h>
#include <gerber_file_image_list.h>
#include <gerbview_frame.h>
#include <gerbview_settings.h>
#include <json_common.h>

#include <wx/arrstr.h>
#include <wx/button.h>
#include <wx/clrpicker.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/grid.h>
#include <wx/msgdlg.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/wfstream.h>


namespace
{

constexpr int COL_FILE = 0;
constexpr int COL_X2   = 1;
constexpr int COL_ROLE = 2;


/// Build a wxArrayString of role display names suitable for a wxGridCellChoiceEditor.
wxArrayString rolesAsChoices()
{
    wxArrayString choices;

    for( GERBVIEW_LAYER_ROLE role : AllRoles() )
        choices.Add( RoleDisplayName( role ) );

    return choices;
}


/// Same normalization rule as in layer_roles.cpp - duplicated here to keep the dialog
/// self-contained (private detail).
wxString normalizeFilename( const wxString& aFullPath )
{
    wxString base = wxFileName( aFullPath ).GetFullName();

#ifdef __WINDOWS__
    base.MakeLower();
#endif

    return base;
}

} // namespace


DIALOG_LAYER_ROLES::DIALOG_LAYER_ROLES( GERBVIEW_FRAME* aParent ) :
        DIALOG_SHIM( aParent, wxID_ANY, _( "Layer Roles" ), wxDefaultPosition, wxSize( 720, 540 ),
                     wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER ),
        m_frame( aParent ),
        m_assignmentsGrid( nullptr ),
        m_colorPanel( nullptr )
{
    // Seed the working color set with whatever is currently stored, falling back to defaults.
    GERBVIEW_SETTINGS* cfg = m_frame->gvconfig();

    for( GERBVIEW_LAYER_ROLE role : AllRoles() )
    {
        if( role == GERBVIEW_LAYER_ROLE::Unassigned )
            continue;

        std::string key = RoleSerializedKey( role );

        if( cfg )
        {
            auto it = cfg->m_LayerRoles.colors.find( key );

            if( it != cfg->m_LayerRoles.colors.end() )
            {
                m_workingColors[key] = it->second;
                continue;
            }
        }

        m_workingColors[key] = DefaultColorFor( role );
    }

    wxBoxSizer* outer = new wxBoxSizer( wxVERTICAL );

    wxStaticBoxSizer* assignBox =
            new wxStaticBoxSizer( wxVERTICAL, this, _( "File -> PCB layer role" ) );

    m_assignmentsGrid = new wxGrid( assignBox->GetStaticBox(), wxID_ANY );
    assignBox->Add( m_assignmentsGrid, 1, wxEXPAND | wxALL, 4 );
    outer->Add( assignBox, 2, wxEXPAND | wxALL, 6 );

    buildAssignmentsGrid();

    wxStaticBoxSizer* colorBox = new wxStaticBoxSizer( wxVERTICAL, this, _( "Role colors" ) );

    m_colorPanel = new wxScrolledWindow( colorBox->GetStaticBox(), wxID_ANY );
    m_colorPanel->SetScrollRate( 0, 12 );
    colorBox->Add( m_colorPanel, 1, wxEXPAND | wxALL, 4 );
    outer->Add( colorBox, 1, wxEXPAND | wxALL, 6 );

    buildColorPanel();

    // Footer with utility buttons + standard OK / Cancel.
    wxBoxSizer* btns = new wxBoxSizer( wxHORIZONTAL );

    wxButton* importBtn = new wxButton( this, wxID_ANY, _( "Import..." ) );
    wxButton* exportBtn = new wxButton( this, wxID_ANY, _( "Export..." ) );
    wxButton* resetBtn  = new wxButton( this, wxID_ANY, _( "Reset role colors" ) );
    wxButton* applyBtn  = new wxButton( this, wxID_APPLY );

    btns->Add( importBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6 );
    btns->Add( exportBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6 );
    btns->Add( resetBtn,  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6 );
    btns->AddStretchSpacer( 1 );
    btns->Add( applyBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6 );

    wxStdDialogButtonSizer* okCancel = new wxStdDialogButtonSizer();
    wxButton* okBtn = new wxButton( this, wxID_OK );
    okCancel->AddButton( okBtn );
    okCancel->AddButton( new wxButton( this, wxID_CANCEL ) );
    okCancel->Realize();
    btns->Add( okCancel, 0, wxALIGN_CENTER_VERTICAL );

    outer->Add( btns, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8 );

    SetSizer( outer );
    Layout();

    importBtn->Bind( wxEVT_BUTTON, &DIALOG_LAYER_ROLES::onImport, this );
    exportBtn->Bind( wxEVT_BUTTON, &DIALOG_LAYER_ROLES::onExport, this );
    resetBtn ->Bind( wxEVT_BUTTON, &DIALOG_LAYER_ROLES::onResetColors, this );
    applyBtn ->Bind( wxEVT_BUTTON, &DIALOG_LAYER_ROLES::onApply, this );
    okBtn    ->Bind( wxEVT_BUTTON, &DIALOG_LAYER_ROLES::onOkButton, this );

    // Cancel by id so Escape (which wxDialog routes to wxID_CANCEL) also dismisses.
    Bind( wxEVT_BUTTON, &DIALOG_LAYER_ROLES::onCancelButton, this, wxID_CANCEL );

    finishDialogSettings();
}


bool DIALOG_LAYER_ROLES::Show( bool show )
{
    if( show )
        return DIALOG_SHIM::Show( show );

    // On hide, bypass DIALOG_SHIM's SaveControlState() + focusParentCanvas()
    // (the recursive child traversal deadlocks on MSW for this dialog's mix of
    // wxColourPickerCtrl + wxGrid + wxStaticBox children).
    return wxDialog::Show( show );
}


void DIALOG_LAYER_ROLES::buildAssignmentsGrid()
{
    GERBER_FILE_IMAGE_LIST* list = m_frame->GetImagesList();

    m_slotIndices.clear();

    for( unsigned slot = 0; slot < list->ImagesMaxCount(); ++slot )
    {
        if( list->GetGbrImage( slot ) )
            m_slotIndices.push_back( int( slot ) );
    }

    m_assignmentsGrid->CreateGrid( int( m_slotIndices.size() ), 3 );
    m_assignmentsGrid->EnableDragRowSize( false );

    m_assignmentsGrid->SetColLabelValue( COL_FILE, _( "File" ) );
    m_assignmentsGrid->SetColLabelValue( COL_X2,   _( "X2 hint" ) );
    m_assignmentsGrid->SetColLabelValue( COL_ROLE, _( "Role" ) );

    m_assignmentsGrid->HideRowLabels();

    if( m_slotIndices.empty() )
    {
        m_assignmentsGrid->Disable();
        m_assignmentsGrid->SetColSize( COL_FILE, 360 );
        m_assignmentsGrid->SetColSize( COL_X2,   140 );
        m_assignmentsGrid->SetColSize( COL_ROLE, 180 );
        return;
    }

    wxArrayString choices = rolesAsChoices();
    GERBVIEW_SETTINGS* cfg = m_frame->gvconfig();

    for( int row = 0; row < int( m_slotIndices.size() ); ++row )
    {
        int slot = m_slotIndices[row];
        GERBER_FILE_IMAGE* img = list->GetGbrImage( slot );

        wxFileName fn( img->m_FileName );
        m_assignmentsGrid->SetCellValue( row, COL_FILE, fn.GetFullName() );
        m_assignmentsGrid->SetReadOnly( row, COL_FILE );

        wxString x2hint;

        if( img->m_FileFunction )
        {
            x2hint = img->m_FileFunction->GetFileType();

            wxString side = img->m_FileFunction->GetBrdLayerSide();

            if( !side.IsEmpty() )
                x2hint << wxT( "/" ) << side;
        }

        m_assignmentsGrid->SetCellValue( row, COL_X2, x2hint );
        m_assignmentsGrid->SetReadOnly( row, COL_X2 );
        m_assignmentsGrid->SetCellBackgroundColour( row, COL_X2, wxColour( 245, 245, 245 ) );

        GERBVIEW_LAYER_ROLE current = GERBVIEW_LAYER_ROLE::Unassigned;

        if( cfg )
        {
            wxString key = normalizeFilename( img->m_FileName );
            auto it = cfg->m_LayerRoles.assignments.find( key );

            if( it != cfg->m_LayerRoles.assignments.end() )
                current = it->second;
        }

        m_assignmentsGrid->SetCellValue( row, COL_ROLE, RoleDisplayName( current ) );
        m_assignmentsGrid->SetCellEditor( row, COL_ROLE, new wxGridCellChoiceEditor( choices ) );
    }

    m_assignmentsGrid->AutoSizeColumns();

    if( m_assignmentsGrid->GetColSize( COL_FILE ) < 280 )
        m_assignmentsGrid->SetColSize( COL_FILE, 280 );

    if( m_assignmentsGrid->GetColSize( COL_ROLE ) < 180 )
        m_assignmentsGrid->SetColSize( COL_ROLE, 180 );
}


void DIALOG_LAYER_ROLES::buildColorPanel()
{
    // Two role/color pairs per row: [Label | Picker] [Label | Picker].
    wxFlexGridSizer* grid = new wxFlexGridSizer( 0, 4, 4, 12 );
    grid->AddGrowableCol( 1, 1 );
    grid->AddGrowableCol( 3, 1 );

    m_colorPickers.clear();
    m_coloredRoles.clear();

    for( GERBVIEW_LAYER_ROLE role : AllRoles() )
    {
        if( role == GERBVIEW_LAYER_ROLE::Unassigned )
            continue;

        m_coloredRoles.push_back( role );

        wxStaticText* label = new wxStaticText( m_colorPanel, wxID_ANY,
                                                RoleDisplayName( role ) );

        std::string key = RoleSerializedKey( role );
        wxColourPickerCtrl* picker = new wxColourPickerCtrl(
                m_colorPanel, wxID_ANY, m_workingColors.at( key ).ToColour() );
        picker->SetClientData( reinterpret_cast<void*>(
                static_cast<intptr_t>( m_coloredRoles.size() - 1 ) ) );
        picker->Bind( wxEVT_COLOURPICKER_CHANGED, &DIALOG_LAYER_ROLES::onColorPicked, this );

        grid->Add( label,  0, wxALIGN_CENTER_VERTICAL );
        grid->Add( picker, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND );

        m_colorPickers.push_back( picker );
    }

    m_colorPanel->SetSizer( grid );
    m_colorPanel->FitInside();
}


void DIALOG_LAYER_ROLES::refreshColorPickers()
{
    for( size_t i = 0; i < m_colorPickers.size(); ++i )
    {
        std::string key = RoleSerializedKey( m_coloredRoles[i] );
        m_colorPickers[i]->SetColour( m_workingColors.at( key ).ToColour() );
    }
}


void DIALOG_LAYER_ROLES::onColorPicked( wxColourPickerEvent& aEvent )
{
    wxColourPickerCtrl* picker = dynamic_cast<wxColourPickerCtrl*>( aEvent.GetEventObject() );

    if( !picker )
        return;

    intptr_t idx = reinterpret_cast<intptr_t>( picker->GetClientData() );

    if( idx < 0 || idx >= intptr_t( m_coloredRoles.size() ) )
        return;

    GERBVIEW_LAYER_ROLE role = m_coloredRoles[idx];
    std::string         key  = RoleSerializedKey( role );

    m_workingColors[key] = KIGFX::COLOR4D( aEvent.GetColour() );
}


GERBVIEW_LAYER_ROLE DIALOG_LAYER_ROLES::roleFromDisplayName( const wxString& aName ) const
{
    for( GERBVIEW_LAYER_ROLE role : AllRoles() )
    {
        if( RoleDisplayName( role ) == aName )
            return role;
    }

    return GERBVIEW_LAYER_ROLE::Unassigned;
}


void DIALOG_LAYER_ROLES::commitToSettings()
{
    GERBVIEW_SETTINGS* cfg = m_frame->gvconfig();

    if( !cfg )
        return;

    // Make sure any in-progress cell editor commits its current value.
    if( m_assignmentsGrid )
        m_assignmentsGrid->SaveEditControlValue();

    cfg->m_LayerRoles.colors = m_workingColors;
    cfg->m_LayerRoles.assignments.clear();

    if( m_assignmentsGrid && !m_slotIndices.empty() )
    {
        GERBER_FILE_IMAGE_LIST* list = m_frame->GetImagesList();

        for( int row = 0; row < int( m_slotIndices.size() ); ++row )
        {
            int slot = m_slotIndices[row];
            GERBER_FILE_IMAGE* img = list->GetGbrImage( slot );

            if( !img )
                continue;

            GERBVIEW_LAYER_ROLE role =
                    roleFromDisplayName( m_assignmentsGrid->GetCellValue( row, COL_ROLE ) );

            if( role == GERBVIEW_LAYER_ROLE::Unassigned )
                continue;

            cfg->m_LayerRoles.assignments[normalizeFilename( img->m_FileName )] = role;
        }
    }

    // The heavy AUI / toolbar refresh is intentionally deferred via
    // scheduleReapply() by the callers, to avoid re-entrancy with the event
    // dispatch that drove the current button click.
}


void DIALOG_LAYER_ROLES::scheduleReapply()
{
    // Queue ReapplyAllLayerRoles() on the parent frame so the heavy AUI /
    // toolbar work happens AFTER the current event handler has returned.
    // This avoids re-entrancy with whatever is currently driving the event
    // dispatch (e.g. the OK / Apply button click).
    GERBVIEW_FRAME* frame = m_frame;
    m_frame->CallAfter( [frame]() { ReapplyAllLayerRoles( frame ); } );
}


void DIALOG_LAYER_ROLES::onApply( wxCommandEvent& aEvent )
{
    commitToSettings();
    scheduleReapply();
}


void DIALOG_LAYER_ROLES::onOkButton( wxCommandEvent& aEvent )
{
    commitToSettings();
    scheduleReapply();
    Destroy();
}


void DIALOG_LAYER_ROLES::onCancelButton( wxCommandEvent& aEvent )
{
    Destroy();
}


void DIALOG_LAYER_ROLES::onResetColors( wxCommandEvent& aEvent )
{
    for( GERBVIEW_LAYER_ROLE role : AllRoles() )
    {
        if( role == GERBVIEW_LAYER_ROLE::Unassigned )
            continue;

        m_workingColors[RoleSerializedKey( role )] = DefaultColorFor( role );
    }

    refreshColorPickers();
}


void DIALOG_LAYER_ROLES::onExport( wxCommandEvent& aEvent )
{
    if( m_assignmentsGrid )
        m_assignmentsGrid->SaveEditControlValue();

    wxFileDialog dlg( this, _( "Export Layer Roles" ), wxEmptyString,
                      wxT( "layer_roles.json" ),
                      _( "JSON files (*.json)|*.json|All files (*.*)|*.*" ),
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT );

    if( dlg.ShowModal() != wxID_OK )
        return;

    nlohmann::json js;
    js["version"] = 1;

    nlohmann::json colors = nlohmann::json::object();

    for( const auto& [key, c] : m_workingColors )
        colors[key] = c.ToHexString().ToUTF8().data();

    js["role_colors"] = colors;

    nlohmann::json assignArr = nlohmann::json::array();

    if( m_assignmentsGrid && !m_slotIndices.empty() )
    {
        GERBER_FILE_IMAGE_LIST* list = m_frame->GetImagesList();

        for( int row = 0; row < int( m_slotIndices.size() ); ++row )
        {
            GERBER_FILE_IMAGE* img = list->GetGbrImage( m_slotIndices[row] );

            if( !img )
                continue;

            GERBVIEW_LAYER_ROLE role =
                    roleFromDisplayName( m_assignmentsGrid->GetCellValue( row, COL_ROLE ) );

            if( role == GERBVIEW_LAYER_ROLE::Unassigned )
                continue;

            nlohmann::json entry = nlohmann::json::object();
            wxFileName fn( img->m_FileName );
            entry["filename"] = std::string( fn.GetFullName().ToUTF8() );
            entry["role"]     = RoleSerializedKey( role );
            assignArr.push_back( entry );
        }
    }

    js["assignments"] = assignArr;

    try
    {
        std::ofstream out( dlg.GetPath().ToStdString() );

        if( !out.is_open() )
        {
            wxMessageBox( wxString::Format( _( "Could not open '%s' for writing." ),
                                            dlg.GetPath() ),
                          _( "Export Layer Roles" ), wxICON_ERROR | wxOK, this );
            return;
        }

        out << js.dump( 2 );
    }
    catch( const std::exception& e )
    {
        wxMessageBox( wxString::Format( _( "Failed to export: %s" ),
                                        wxString::FromUTF8( e.what() ) ),
                      _( "Export Layer Roles" ), wxICON_ERROR | wxOK, this );
    }
}


void DIALOG_LAYER_ROLES::onImport( wxCommandEvent& aEvent )
{
    wxFileDialog dlg( this, _( "Import Layer Roles" ), wxEmptyString, wxEmptyString,
                      _( "JSON files (*.json)|*.json|All files (*.*)|*.*" ),
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST );

    if( dlg.ShowModal() != wxID_OK )
        return;

    nlohmann::json js;

    try
    {
        std::ifstream in( dlg.GetPath().ToStdString() );

        if( !in.is_open() )
        {
            wxMessageBox( wxString::Format( _( "Could not open '%s'." ), dlg.GetPath() ),
                          _( "Import Layer Roles" ), wxICON_ERROR | wxOK, this );
            return;
        }

        in >> js;
    }
    catch( const std::exception& e )
    {
        wxMessageBox( wxString::Format( _( "Failed to parse JSON: %s" ),
                                        wxString::FromUTF8( e.what() ) ),
                      _( "Import Layer Roles" ), wxICON_ERROR | wxOK, this );
        return;
    }

    if( !js.is_object() )
        return;

    // Colors.
    if( js.contains( "role_colors" ) && js["role_colors"].is_object() )
    {
        for( auto it = js["role_colors"].begin(); it != js["role_colors"].end(); ++it )
        {
            if( !it.value().is_string() )
                continue;

            KIGFX::COLOR4D c;

            if( c.SetFromHexString( wxString::FromUTF8( it.value().get<std::string>() ) ) )
                m_workingColors[it.key()] = c;
        }
    }

    // Assignments: applied to any matching row by current filename.
    if( m_assignmentsGrid && js.contains( "assignments" ) && js["assignments"].is_array()
        && !m_slotIndices.empty() )
    {
        std::map<wxString, GERBVIEW_LAYER_ROLE> imported;

        for( const nlohmann::json& entry : js["assignments"] )
        {
            if( !entry.is_object() )
                continue;

            if( !entry.contains( "filename" ) || !entry["filename"].is_string() )
                continue;

            if( !entry.contains( "role" ) || !entry["role"].is_string() )
                continue;

            wxString fname = wxString::FromUTF8( entry["filename"].get<std::string>() );
#ifdef __WINDOWS__
            fname.MakeLower();
#endif

            imported[fname] =
                    RoleFromSerializedKey( entry["role"].get<std::string>() );
        }

        GERBER_FILE_IMAGE_LIST* list = m_frame->GetImagesList();

        for( int row = 0; row < int( m_slotIndices.size() ); ++row )
        {
            GERBER_FILE_IMAGE* img = list->GetGbrImage( m_slotIndices[row] );

            if( !img )
                continue;

            wxString key = normalizeFilename( img->m_FileName );

            auto it = imported.find( key );

            if( it == imported.end() )
                continue;

            m_assignmentsGrid->SetCellValue( row, COL_ROLE, RoleDisplayName( it->second ) );
        }
    }

    refreshColorPickers();
}
