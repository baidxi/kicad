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

#include <algorithm>
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
#include <wx/font.h>
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
wxArrayString rolesAsChoices( int aVisibleInnerCount )
{
    wxArrayString choices;

    for( GERBVIEW_LAYER_ROLE role : AllRoles() )
    {
        int innerIdx = InnerCopperIndex( role );

        if( innerIdx > aVisibleInnerCount )
            continue;

        choices.Add( RoleDisplayName( role ) );
    }

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
        DIALOG_SHIM( aParent, wxID_ANY, _( "Layer Roles" ), wxDefaultPosition,
                     wxSize( 1100, 760 ),
                     wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER ),
        m_frame( aParent ),
        m_assignmentsGrid( nullptr ),
        m_colorPanel( nullptr ),
        m_schemeLabel( nullptr ),
        m_addInnerBtn( nullptr ),
        m_removeInnerBtn( nullptr ),
        m_visibleInnerCount( 2 )
{
    GERBVIEW_SETTINGS* cfg = m_frame->gvconfig();

    if( cfg )
    {
        m_visibleInnerCount = std::clamp( cfg->m_LayerRoles.visibleInnerCount,
                                          2, MAX_INNER_COPPER_LAYERS );
        m_loadedSchemeName  = cfg->m_LayerRoles.loadedSchemeName;
    }

    // Seed working colors from settings or defaults.
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

    // Auto-expand the visible inner-count to cover any existing assignments
    // that already reference higher-numbered Inner Copper layers.
    if( cfg )
    {
        for( const auto& [_, role] : cfg->m_LayerRoles.assignments )
        {
            int idx = InnerCopperIndex( role );

            if( idx > m_visibleInnerCount )
                m_visibleInnerCount = std::min( idx, MAX_INNER_COPPER_LAYERS );
        }
    }

    // === Layout =============================================================
    //
    //  outer (V)
    //    columns (H)
    //      assignBox (V)              <-- left column: file -> role grid
    //        m_assignmentsGrid
    //      rightCol (V)               <-- right column
    //        m_schemeLabel
    //        m_colorPanel (scrolled)
    //          top / inner / bottom / other sections
    //        innerBtns                <-- + Add / - Remove inner layer
    //        fileBtns                 <-- Import / Export / Reset
    //    okRow (H)                    <-- Apply ... OK | Cancel
    //
    wxBoxSizer* outer   = new wxBoxSizer( wxVERTICAL );
    wxBoxSizer* columns = new wxBoxSizer( wxHORIZONTAL );

    // ---- Left column: assignments grid -----------------------------------
    wxStaticBoxSizer* assignBox =
            new wxStaticBoxSizer( wxVERTICAL, this, _( "File -> PCB layer role" ) );

    m_assignmentsGrid = new wxGrid( assignBox->GetStaticBox(), wxID_ANY );
    assignBox->Add( m_assignmentsGrid, 1, wxEXPAND | wxALL, 4 );
    columns->Add( assignBox, 3, wxEXPAND | wxALL, 6 );

    buildAssignmentsGrid();

    // ---- Right column: scheme label, colors, buttons ---------------------
    wxBoxSizer* rightCol = new wxBoxSizer( wxVERTICAL );

    m_schemeLabel = new wxStaticText( this, wxID_ANY, wxEmptyString );
    rightCol->Add( m_schemeLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6 );

    wxStaticBoxSizer* colorBox =
            new wxStaticBoxSizer( wxVERTICAL, this, _( "Role colors" ) );

    m_colorPanel = new wxScrolledWindow( colorBox->GetStaticBox(), wxID_ANY );
    m_colorPanel->SetScrollRate( 0, 12 );
    colorBox->Add( m_colorPanel, 1, wxEXPAND | wxALL, 4 );
    rightCol->Add( colorBox, 1, wxEXPAND | wxALL, 4 );

    // The + Add / - Remove inner-layer buttons live inside the Inner Copper
    // section itself (created by buildColorPanel) so they sit visually under
    // the inner-copper color rows.
    buildColorPanel();

    wxBoxSizer* fileBtns = new wxBoxSizer( wxHORIZONTAL );
    wxButton* importBtn = new wxButton( this, wxID_ANY, _( "Import..." ) );
    wxButton* exportBtn = new wxButton( this, wxID_ANY, _( "Export..." ) );
    wxButton* resetBtn  = new wxButton( this, wxID_ANY, _( "Reset" ) );
    fileBtns->Add( importBtn, 1, wxEXPAND | wxRIGHT, 4 );
    fileBtns->Add( exportBtn, 1, wxEXPAND | wxRIGHT, 4 );
    fileBtns->Add( resetBtn,  1, wxEXPAND );
    rightCol->Add( fileBtns, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 6 );

    columns->Add( rightCol, 2, wxEXPAND | wxALL, 6 );
    outer->Add( columns, 1, wxEXPAND );

    // ---- Bottom row: Apply / OK / Cancel ---------------------------------
    wxBoxSizer* okRow = new wxBoxSizer( wxHORIZONTAL );

    wxButton* applyBtn = new wxButton( this, wxID_APPLY );
    okRow->Add( applyBtn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8 );
    okRow->AddStretchSpacer( 1 );

    wxStdDialogButtonSizer* okCancel = new wxStdDialogButtonSizer();
    wxButton* okBtn = new wxButton( this, wxID_OK );
    okCancel->AddButton( okBtn );
    okCancel->AddButton( new wxButton( this, wxID_CANCEL ) );
    okCancel->Realize();
    okRow->Add( okCancel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8 );

    outer->Add( okRow, 0, wxEXPAND | wxTOP | wxBOTTOM, 8 );

    SetSizer( outer );
    Layout();

    // ---- Bind events -----------------------------------------------------
    // Inner +/- buttons are created inside buildColorPanel() and bound there too.
    importBtn       ->Bind( wxEVT_BUTTON, &DIALOG_LAYER_ROLES::onImport,           this );
    exportBtn       ->Bind( wxEVT_BUTTON, &DIALOG_LAYER_ROLES::onExport,           this );
    resetBtn        ->Bind( wxEVT_BUTTON, &DIALOG_LAYER_ROLES::onResetColors,      this );
    applyBtn        ->Bind( wxEVT_BUTTON, &DIALOG_LAYER_ROLES::onApply,            this );
    okBtn           ->Bind( wxEVT_BUTTON, &DIALOG_LAYER_ROLES::onOkButton,         this );

    // Cancel by id so Escape (which wxDialog routes to wxID_CANCEL) also dismisses.
    Bind( wxEVT_BUTTON, &DIALOG_LAYER_ROLES::onCancelButton, this, wxID_CANCEL );

    updateSchemeLabel();

    // Make the scrolled color panel large enough to fully display all four
    // sections, so the dialog opens at a size that does NOT require scrolling.
    // After the user adds more Inner Copper rows, the scrolled window can
    // still scroll if the dialog hasn't been resized.
    m_colorPanel->FitInside();
    wxSize colorContent = m_colorPanel->GetVirtualSize();
    m_colorPanel->SetMinClientSize( colorContent );

    // Recompute the layout with the new color-panel min size, size the dialog
    // to its contents, then pin that as the minimum allowed size.
    Layout();
    Fit();

    // Don't let the dialog exceed the display work area; otherwise on small
    // screens the OK/Cancel row could be pushed off-screen.
    wxSize screen = wxGetClientDisplayRect().GetSize();
    wxSize current = GetSize();
    int cappedW = std::min( current.GetWidth(),  screen.GetWidth()  - 40 );
    int cappedH = std::min( current.GetHeight(), screen.GetHeight() - 40 );
    SetSize( cappedW, cappedH );

    SetMinSize( GetSize() );

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

    wxArrayString choices = rolesAsChoices( m_visibleInnerCount );
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

    if( m_assignmentsGrid->GetColSize( COL_FILE ) < 240 )
        m_assignmentsGrid->SetColSize( COL_FILE, 240 );

    if( m_assignmentsGrid->GetColSize( COL_ROLE ) < 160 )
        m_assignmentsGrid->SetColSize( COL_ROLE, 160 );
}


void DIALOG_LAYER_ROLES::addColorRow( wxSizer* aSizer, GERBVIEW_LAYER_ROLE aRole,
                                      const wxString& aDisplayLabel )
{
    wxStaticBoxSizer* sbs    = dynamic_cast<wxStaticBoxSizer*>( aSizer );
    wxWindow*         parent = sbs ? static_cast<wxWindow*>( sbs->GetStaticBox() ) : m_colorPanel;

    wxStaticText* label = new wxStaticText( parent, wxID_ANY, aDisplayLabel );

    std::string key = RoleSerializedKey( aRole );
    wxColourPickerCtrl* picker = new wxColourPickerCtrl(
            parent, wxID_ANY, m_workingColors.at( key ).ToColour() );

    m_coloredRoles.push_back( aRole );
    m_colorPickers.push_back( picker );

    // Capture role by value so the lambda is independent of the m_coloredRoles ordering.
    GERBVIEW_LAYER_ROLE captured = aRole;
    picker->Bind( wxEVT_COLOURPICKER_CHANGED,
                  [this, captured]( wxColourPickerEvent& aEvt )
                  {
                      std::string k = RoleSerializedKey( captured );
                      m_workingColors[k] = KIGFX::COLOR4D( aEvt.GetColour() );

                      // Once the user edits a color, the palette is no longer
                      // identical to whatever scheme was imported.
                      if( !m_loadedSchemeName.IsEmpty() )
                      {
                          m_loadedSchemeName.clear();
                          updateSchemeLabel();
                      }
                  } );

    wxBoxSizer* row = new wxBoxSizer( wxHORIZONTAL );
    row->Add( label,  1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8 );
    row->Add( picker, 0, wxALIGN_CENTER_VERTICAL );
    aSizer->Add( row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 3 );
}


void DIALOG_LAYER_ROLES::buildColorPanel()
{
    m_colorPickers.clear();
    m_coloredRoles.clear();

    wxBoxSizer* mainSizer = new wxBoxSizer( wxVERTICAL );

    // Top section.
    {
        wxStaticBoxSizer* box =
                new wxStaticBoxSizer( wxVERTICAL, m_colorPanel, _( "Top Layers" ) );
        addColorRow( box, GERBVIEW_LAYER_ROLE::TopSolderPaste, _( "Solder Paste" ) );
        addColorRow( box, GERBVIEW_LAYER_ROLE::TopSolderMask,  _( "Solder Mask" ) );
        addColorRow( box, GERBVIEW_LAYER_ROLE::TopCopper,      _( "Top Copper" ) );
        mainSizer->Add( box, 0, wxEXPAND | wxALL, 4 );
    }

    // Inner copper section.  The + Add / - Remove buttons live INSIDE this
    // static box so they sit visually under the inner-copper color rows.
    {
        wxStaticBoxSizer* box =
                new wxStaticBoxSizer( wxVERTICAL, m_colorPanel, _( "Inner Copper Layers" ) );

        for( int i = 1; i <= m_visibleInnerCount; ++i )
        {
            addColorRow( box, InnerCopperRoleAt( i ),
                         wxString::Format( _( "Inner Copper %d" ), i ) );
        }

        wxStaticBox* sbox = box->GetStaticBox();

        wxBoxSizer* innerBtns = new wxBoxSizer( wxHORIZONTAL );
        m_addInnerBtn    = new wxButton( sbox, wxID_ANY, _( "+ Add Inner Layer" ) );
        m_removeInnerBtn = new wxButton( sbox, wxID_ANY, _( "- Remove Last" ) );

        m_addInnerBtn   ->Enable( m_visibleInnerCount < MAX_INNER_COPPER_LAYERS );
        m_removeInnerBtn->Enable( m_visibleInnerCount > 2 );

        m_addInnerBtn   ->Bind( wxEVT_BUTTON, &DIALOG_LAYER_ROLES::onAddInnerLayer,
                                this );
        m_removeInnerBtn->Bind( wxEVT_BUTTON, &DIALOG_LAYER_ROLES::onRemoveInnerLayer,
                                this );

        innerBtns->Add( m_addInnerBtn,    1, wxEXPAND | wxRIGHT, 4 );
        innerBtns->Add( m_removeInnerBtn, 1, wxEXPAND );
        box->Add( innerBtns, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 4 );

        mainSizer->Add( box, 0, wxEXPAND | wxALL, 4 );
    }

    // Bottom section (mirror of Top: copper first, then mask, then paste).
    {
        wxStaticBoxSizer* box =
                new wxStaticBoxSizer( wxVERTICAL, m_colorPanel, _( "Bottom Layers" ) );
        addColorRow( box, GERBVIEW_LAYER_ROLE::BottomCopper,      _( "Bottom Copper" ) );
        addColorRow( box, GERBVIEW_LAYER_ROLE::BottomSolderMask,  _( "Solder Mask" ) );
        addColorRow( box, GERBVIEW_LAYER_ROLE::BottomSolderPaste, _( "Solder Paste" ) );
        mainSizer->Add( box, 0, wxEXPAND | wxALL, 4 );
    }

    // Other layers section: silk + drill + contour (covered by the role enum
    // but not in the user-requested core stackup ordering).
    {
        wxStaticBoxSizer* box =
                new wxStaticBoxSizer( wxVERTICAL, m_colorPanel, _( "Silk Screen, Drill and Contour" ) );
        addColorRow( box, GERBVIEW_LAYER_ROLE::TopSilkScreen,    _( "Top Silk Screen" ) );
        addColorRow( box, GERBVIEW_LAYER_ROLE::BottomSilkScreen, _( "Bottom Silk Screen" ) );
        addColorRow( box, GERBVIEW_LAYER_ROLE::DrillPlated,      _( "Drill (Plated)" ) );
        addColorRow( box, GERBVIEW_LAYER_ROLE::DrillUnplated,    _( "Drill (Unplated)" ) );
        addColorRow( box, GERBVIEW_LAYER_ROLE::ContourPlated,    _( "Contour (Plated)" ) );
        addColorRow( box, GERBVIEW_LAYER_ROLE::ContourUnplated,  _( "Contour (Unplated)" ) );
        mainSizer->Add( box, 0, wxEXPAND | wxALL, 4 );
    }

    m_colorPanel->SetSizer( mainSizer );
    m_colorPanel->FitInside();
}


void DIALOG_LAYER_ROLES::rebuildColorSection()
{
    Freeze();

    // DestroyChildren() also kills the + Add / - Remove buttons; buildColorPanel
    // recreates them with current enable state.
    m_colorPanel->DestroyChildren();
    m_addInnerBtn    = nullptr;
    m_removeInnerBtn = nullptr;

    // SetSizer(deleteOld=true, the default in buildColorPanel) drops the
    // orphaned sizer left over from the destroyed children.
    buildColorPanel();
    m_colorPanel->Layout();
    m_colorPanel->FitInside();

    Layout();
    Thaw();
}


void DIALOG_LAYER_ROLES::refreshColorPickers()
{
    for( size_t i = 0; i < m_colorPickers.size(); ++i )
    {
        std::string key = RoleSerializedKey( m_coloredRoles[i] );
        m_colorPickers[i]->SetColour( m_workingColors.at( key ).ToColour() );
    }
}


void DIALOG_LAYER_ROLES::updateSchemeLabel()
{
    if( !m_schemeLabel )
        return;

    if( m_loadedSchemeName.IsEmpty() )
        m_schemeLabel->SetLabel( _( "Color scheme: (custom)" ) );
    else
        m_schemeLabel->SetLabel( wxString::Format( _( "Color scheme: %s" ),
                                                   m_loadedSchemeName ) );
}


void DIALOG_LAYER_ROLES::onAddInnerLayer( wxCommandEvent& aEvent )
{
    if( m_visibleInnerCount >= MAX_INNER_COPPER_LAYERS )
        return;

    ++m_visibleInnerCount;
    rebuildColorSection();

    // The grid's role dropdown must also reflect the new visible inner roles.
    if( m_assignmentsGrid )
    {
        m_assignmentsGrid->SaveEditControlValue();

        wxArrayString choices = rolesAsChoices( m_visibleInnerCount );

        for( int row = 0; row < int( m_slotIndices.size() ); ++row )
            m_assignmentsGrid->SetCellEditor( row, COL_ROLE,
                                              new wxGridCellChoiceEditor( choices ) );
    }
}


void DIALOG_LAYER_ROLES::onRemoveInnerLayer( wxCommandEvent& aEvent )
{
    if( m_visibleInnerCount <= 2 )
        return;

    // Removing a visible inner layer would orphan any file currently assigned
    // to InnerCopper<m_visibleInnerCount>.  Warn and bail if that's the case.
    GERBVIEW_LAYER_ROLE doomed = InnerCopperRoleAt( m_visibleInnerCount );

    if( m_assignmentsGrid )
        m_assignmentsGrid->SaveEditControlValue();

    bool inUse = false;

    if( m_assignmentsGrid )
    {
        for( int row = 0; row < int( m_slotIndices.size() ); ++row )
        {
            if( roleFromDisplayName( m_assignmentsGrid->GetCellValue( row, COL_ROLE ) )
                == doomed )
            {
                inUse = true;
                break;
            }
        }
    }

    if( inUse )
    {
        wxMessageBox( wxString::Format(
                              _( "Cannot remove '%s' because one or more files "
                                 "are currently assigned to it." ),
                              RoleDisplayName( doomed ) ),
                      _( "Layer Roles" ), wxICON_WARNING | wxOK, this );
        return;
    }

    --m_visibleInnerCount;
    rebuildColorSection();

    if( m_assignmentsGrid )
    {
        wxArrayString choices = rolesAsChoices( m_visibleInnerCount );

        for( int row = 0; row < int( m_slotIndices.size() ); ++row )
            m_assignmentsGrid->SetCellEditor( row, COL_ROLE,
                                              new wxGridCellChoiceEditor( choices ) );
    }
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

    cfg->m_LayerRoles.colors             = m_workingColors;
    cfg->m_LayerRoles.visibleInnerCount  = m_visibleInnerCount;
    cfg->m_LayerRoles.loadedSchemeName   = m_loadedSchemeName;
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

    m_loadedSchemeName.clear();
    refreshColorPickers();
    updateSchemeLabel();
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
    js["visible_inner_count"] = m_visibleInnerCount;

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

        // Track the exported scheme name as the "current" scheme too.
        m_loadedSchemeName = wxFileName( dlg.GetPath() ).GetFullName();
        updateSchemeLabel();
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

            if( !c.SetFromHexString( wxString::FromUTF8( it.value().get<std::string>() ) ) )
                continue;

            // Migrate the legacy "inner_copper" key to "inner_copper_1".
            std::string key = it.key();

            if( key == "inner_copper" )
                key = "inner_copper_1";

            m_workingColors[key] = c;
        }
    }

    // Visible inner count.
    if( js.contains( "visible_inner_count" ) && js["visible_inner_count"].is_number_integer() )
    {
        int n = js["visible_inner_count"].get<int>();
        m_visibleInnerCount = std::clamp( n, 2, MAX_INNER_COPPER_LAYERS );
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

        // If imported assignments reference Inner Copper N higher than the current
        // visible count, expand the visible count to cover them.
        for( const auto& [_, role] : imported )
        {
            int idx = InnerCopperIndex( role );

            if( idx > m_visibleInnerCount )
                m_visibleInnerCount = std::min( idx, MAX_INNER_COPPER_LAYERS );
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

        // Refresh grid editors so the dropdown reflects any expanded inner count.
        wxArrayString choices = rolesAsChoices( m_visibleInnerCount );

        for( int row = 0; row < int( m_slotIndices.size() ); ++row )
            m_assignmentsGrid->SetCellEditor( row, COL_ROLE,
                                              new wxGridCellChoiceEditor( choices ) );
    }

    m_loadedSchemeName = wxFileName( dlg.GetPath() ).GetFullName();

    rebuildColorSection();
    updateSchemeLabel();
}
