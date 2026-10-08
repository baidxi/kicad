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

#include <dialogs/dialog_gerber_item_properties.h>

#include <algorithm>
#include <vector>

#include <aperture_macro.h>
#include <class_draw_panel_gal.h>
#include <dcode.h>
#include <eda_units.h>
#include <gal/color4d.h>
#include <gerber_draw_item.h>
#include <gerber_file_image.h>
#include <gerber_file_image_list.h>
#include <gerbview_frame.h>
#include <gerbview_painter.h>
#include <geometry/shape_line_chain.h>
#include <geometry/shape_poly_set.h>
#include <view/view.h>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dcbuffer.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>


/**
 * Append a (label, value) row to a 2-column wxFlexGridSizer.
 */
static void appendRow( wxWindow* aParent, wxFlexGridSizer* aSizer, const wxString& aLabel,
                       const wxString& aValue )
{
    wxStaticText* label = new wxStaticText( aParent, wxID_ANY, aLabel + wxT( ":" ) );
    wxStaticText* value = new wxStaticText( aParent, wxID_ANY, aValue );

    aSizer->Add( label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8 );
    aSizer->Add( value, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 0 );
}


/**
 * Format a length stored in internal Gerber units using the frame's user units.
 */
static wxString fmtLen( EDA_DRAW_FRAME* aFrame, int aVal )
{
    return EDA_UNIT_UTILS::UI::StringFromValue( gerbIUScale, aFrame->GetUserUnits(), aVal, true );
}


/**
 * Custom panel that renders a scaled, aspect-preserving 2D preview of the aperture
 * (the flashed shape, including any hole, for both standard apertures and macros).
 *
 * The polygon is obtained from D_CODE::ConvertShapeToPolygon(), the same routine
 * used by the GerbView painter, so the preview matches what is drawn on the canvas.
 */
class APERTURE_PREVIEW_PANEL : public wxPanel
{
public:
    APERTURE_PREVIEW_PANEL( wxWindow* aParent, GERBER_DRAW_ITEM* aItem ) :
            wxPanel( aParent, wxID_ANY, wxDefaultPosition, wxSize( 240, 180 ),
                     wxBORDER_SUNKEN ),
            m_item( aItem )
    {
        SetBackgroundStyle( wxBG_STYLE_PAINT );
        SetBackgroundColour( wxColour( 240, 240, 232 ) );
        SetMinSize( wxSize( 200, 160 ) );

        Bind( wxEVT_PAINT, &APERTURE_PREVIEW_PANEL::onPaint, this );
        Bind( wxEVT_SIZE, &APERTURE_PREVIEW_PANEL::onSize, this );
    }

private:
    void onSize( wxSizeEvent& aEvt )
    {
        Refresh();
        aEvt.Skip();
    }

    void onPaint( wxPaintEvent& )
    {
        wxAutoBufferedPaintDC dc( this );
        dc.SetBackground( wxBrush( GetBackgroundColour() ) );
        dc.Clear();

        if( !m_item )
            return;

        D_CODE* dcode = m_item->GetDcodeDescr();

        if( !dcode )
            return;

        // Build (or refresh) the polygon representation of the aperture in dcode-local
        // coordinates (centered around the origin).  Safe to call repeatedly: this is
        // exactly what the painter does for every redraw.
        dcode->ConvertShapeToPolygon( m_item );

        const SHAPE_POLY_SET& poly = dcode->m_Polygon;

        if( poly.OutlineCount() == 0 )
            return;

        BOX2I bbox = poly.BBox();

        if( bbox.GetWidth() <= 0 || bbox.GetHeight() <= 0 )
            return;

        const wxSize sz = GetClientSize();
        const int    margin = 12;
        const int    avail_w = std::max( 1, sz.GetWidth() - 2 * margin );
        const int    avail_h = std::max( 1, sz.GetHeight() - 2 * margin );

        const double sx = double( avail_w ) / double( bbox.GetWidth() );
        const double sy = double( avail_h ) / double( bbox.GetHeight() );
        const double scale = std::min( sx, sy );

        const double     cx = sz.GetWidth() / 2.0;
        const double     cy = sz.GetHeight() / 2.0;
        const VECTOR2I   bbCenter = bbox.GetCenter();

        auto toScreen = [&]( const VECTOR2I& p ) -> wxPoint
        {
            return wxPoint( int( cx + ( p.x - bbCenter.x ) * scale ),
                            int( cy + ( p.y - bbCenter.y ) * scale ) );
        };

        // Flatten outlines + holes for wxDC::DrawPolyPolygon.  Using wxODDEVEN_RULE
        // causes inner holes (which wind opposite to the outer outline) to be
        // subtracted from the filled area.
        std::vector<wxPoint> pts;
        std::vector<int>     counts;

        for( int oi = 0; oi < poly.OutlineCount(); ++oi )
        {
            const SHAPE_LINE_CHAIN& outline = poly.COutline( oi );
            int                     n = 0;

            for( int pi = 0; pi < outline.PointCount(); ++pi )
            {
                pts.push_back( toScreen( outline.CPoint( pi ) ) );
                ++n;
            }

            counts.push_back( n );

            for( int hi = 0; hi < poly.HoleCount( oi ); ++hi )
            {
                const SHAPE_LINE_CHAIN& hole = poly.CHole( oi, hi );
                int                     nh = 0;

                for( int pi = 0; pi < hole.PointCount(); ++pi )
                {
                    pts.push_back( toScreen( hole.CPoint( pi ) ) );
                    ++nh;
                }

                counts.push_back( nh );
            }
        }

        if( pts.empty() )
            return;

        // High-contrast fixed colors that work regardless of the user's layer color theme.
        // (Layer color is reserved for the canvas; the preview just needs to convey shape.)
        const wxColour fillColor   ( 200, 80,  40 );   // warm copper-ish
        const wxColour borderColor (  80, 30,  10 );

        dc.SetPen( wxPen( borderColor, 1 ) );
        dc.SetBrush( wxBrush( fillColor ) );
        dc.DrawPolyPolygon( int( counts.size() ), counts.data(), pts.data(), 0, 0,
                            wxODDEVEN_RULE );
    }

    GERBER_DRAW_ITEM* m_item;
};


DIALOG_GERBER_ITEM_PROPERTIES::DIALOG_GERBER_ITEM_PROPERTIES( GERBVIEW_FRAME* aParent,
                                                              GERBER_DRAW_ITEM* aItem ) :
        DIALOG_SHIM( aParent, wxID_ANY, _( "Gerber Item Properties" ), wxDefaultPosition,
                     wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER ),
        m_frame( aParent ),
        m_item( aItem ),
        m_contentPanel( nullptr ),
        m_contentSizer( nullptr ),
        m_highlightDCode( nullptr )
{
    wxBoxSizer* outer = new wxBoxSizer( wxVERTICAL );

    m_contentPanel = new wxPanel( this, wxID_ANY );
    m_contentSizer = new wxBoxSizer( wxVERTICAL );
    m_contentPanel->SetSizer( m_contentSizer );

    outer->Add( m_contentPanel, 1, wxEXPAND | wxALL, 4 );

    wxBoxSizer* bottom = new wxBoxSizer( wxHORIZONTAL );

    m_highlightDCode = new wxCheckBox( this, wxID_ANY,
                                       _( "Highlight items using this D-code" ) );
    bottom->Add( m_highlightDCode, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6 );

    wxButton* closeBtn = new wxButton( this, wxID_CANCEL, _( "Close" ) );
    closeBtn->SetDefault();
    bottom->Add( closeBtn, 0, wxALIGN_CENTER_VERTICAL | wxALL, 6 );

    outer->Add( bottom, 0, wxEXPAND | wxLEFT | wxRIGHT, 4 );

    SetSizer( outer );

    buildContent();

    Bind( wxEVT_BUTTON, &DIALOG_GERBER_ITEM_PROPERTIES::onCloseButton, this, wxID_CANCEL );
    Bind( wxEVT_CHECKBOX, &DIALOG_GERBER_ITEM_PROPERTIES::onHighlightToggled, this,
          m_highlightDCode->GetId() );
    Bind( wxEVT_CLOSE_WINDOW, &DIALOG_GERBER_ITEM_PROPERTIES::onClose, this );
}


DIALOG_GERBER_ITEM_PROPERTIES::~DIALOG_GERBER_ITEM_PROPERTIES()
{
    if( m_frame && m_frame->m_propertiesDialog == this )
        m_frame->m_propertiesDialog = nullptr;
}


void DIALOG_GERBER_ITEM_PROPERTIES::SetItem( GERBER_DRAW_ITEM* aItem )
{
    m_item = aItem;
    buildContent();

    // If the user wants D-code highlight active, re-apply (or clear) it for the new item:
    // - new item with D-code      -> set highlight to its D-code (auto-clears the previous one)
    // - new item without D-code   -> clear highlight (nothing meaningful to keep)
    // - no selection (nullptr)    -> clear highlight
    if( m_highlightDCode && m_highlightDCode->IsChecked() )
    {
        if( m_item && m_item->GetDcodeDescr() )
            applyHighlight();
        else
            clearHighlight();
    }
}


void DIALOG_GERBER_ITEM_PROPERTIES::buildContent()
{
    m_contentPanel->DestroyChildren();
    m_contentSizer->Clear( /* delete_windows = */ false );

    if( !m_item )
    {
        wxStaticText* label =
                new wxStaticText( m_contentPanel, wxID_ANY,
                                  _( "Select a Gerber item to view its properties." ) );
        m_contentSizer->Add( label, 0, wxALL, 12 );

        if( m_highlightDCode )
            m_highlightDCode->Enable( false );

        Layout();
        Fit();
        return;
    }

    // ---- General ----------------------------------------------------------------------
    {
        wxStaticBoxSizer* box =
                new wxStaticBoxSizer( wxVERTICAL, m_contentPanel, _( "General" ) );
        wxFlexGridSizer* grid = new wxFlexGridSizer( 0, 2, 4, 8 );
        grid->AddGrowableCol( 1 );

        appendRow( box->GetStaticBox(), grid, _( "Type" ), m_item->ShowGBRShape() );

        if( m_item->m_DCode > 0 )
            appendRow( box->GetStaticBox(), grid, _( "D Code" ),
                       wxString::Format( wxT( "D%d" ), m_item->m_DCode ) );

        D_CODE* dcode = m_item->GetDcodeDescr();
        wxString aperFn = m_item->m_AperFunction;

        if( aperFn.IsEmpty() && dcode )
            aperFn = dcode->m_AperFunction;

        appendRow( box->GetStaticBox(), grid, _( "Aperture function" ),
                   aperFn.IsEmpty() ? _( "No attribute" ) : aperFn );

        wxString layerName = GERBER_FILE_IMAGE_LIST::GetImagesList().GetDisplayName(
                m_item->GetLayer(), true );
        appendRow( box->GetStaticBox(), grid, _( "Graphic Layer" ), layerName );

        appendRow( box->GetStaticBox(), grid, _( "Polarity" ),
                   m_item->GetLayerPolarity() ? _( "Clear" ) : _( "Dark" ) );

        appendRow( box->GetStaticBox(), grid, _( "Mirror" ),
                   wxString::Format( wxT( "A: %s   B: %s" ),
                                     m_item->GetMirrorA() ? _( "Yes" ) : _( "No" ),
                                     m_item->GetMirrorB() ? _( "Yes" ) : _( "No" ) ) );

        const GBR_NETLIST_METADATA& net = m_item->GetNetAttributes();

        if( !net.m_Cmpref.IsEmpty() )
            appendRow( box->GetStaticBox(), grid, _( "Component" ), net.m_Cmpref );

        if( !net.m_Netname.IsEmpty() )
            appendRow( box->GetStaticBox(), grid, _( "Net" ), net.m_Netname );

        box->Add( grid, 1, wxEXPAND | wxALL, 6 );
        m_contentSizer->Add( box, 0, wxEXPAND | wxALL, 6 );
    }

    // ---- Aperture (D_CODE) ------------------------------------------------------------
    D_CODE* dcode = m_item->GetDcodeDescr();

    if( dcode )
    {
        wxString title = wxString::Format( _( "Aperture (D%d)" ), dcode->m_Num_Dcode );
        wxStaticBoxSizer* box = new wxStaticBoxSizer( wxVERTICAL, m_contentPanel, title );
        wxFlexGridSizer* grid = new wxFlexGridSizer( 0, 2, 4, 8 );
        grid->AddGrowableCol( 1 );

        appendRow( box->GetStaticBox(), grid, _( "Aperture type" ),
                   D_CODE::ShowApertureType( dcode->m_ApertType ) );

        // m_Size is only meaningful for standard apertures (parsed from %ADD ... C/R/O/P,X[xY]).
        // For aperture macros it is left at 0, so show the actual flash bounding box instead.
        switch( dcode->m_ApertType )
        {
        case APT_CIRCLE:
            appendRow( box->GetStaticBox(), grid, _( "Diameter" ),
                       fmtLen( m_frame, dcode->m_Size.x ) );
            break;

        case APT_RECT:
        case APT_OVAL:
            appendRow( box->GetStaticBox(), grid, _( "Size X" ),
                       fmtLen( m_frame, dcode->m_Size.x ) );
            appendRow( box->GetStaticBox(), grid, _( "Size Y" ),
                       fmtLen( m_frame, dcode->m_Size.y ) );
            break;

        case APT_POLYGON:
            appendRow( box->GetStaticBox(), grid, _( "Outer diameter" ),
                       fmtLen( m_frame, dcode->m_Size.x ) );
            appendRow( box->GetStaticBox(), grid, _( "Edges" ),
                       wxString::Format( wxT( "%d" ), dcode->m_EdgesCount ) );
            appendRow( box->GetStaticBox(), grid, _( "Aperture rotation" ),
                       wxString::Format( wxT( "%.3f\u00B0" ),
                                         dcode->m_Rotation.AsDegrees() ) );
            break;

        case APT_MACRO:
        default:
            break;
        }

        // Always show the actual flash bounding box: this is what the preview draws,
        // and for macro apertures it is the only meaningful overall size.
        if( dcode->m_Polygon.OutlineCount() == 0 )
            dcode->ConvertShapeToPolygon( m_item );

        if( dcode->m_Polygon.OutlineCount() > 0 )
        {
            BOX2I bb = dcode->m_Polygon.BBox();
            appendRow( box->GetStaticBox(), grid, _( "Bounding box" ),
                       wxString::Format( wxT( "%s \u00D7 %s" ),
                                         fmtLen( m_frame, bb.GetWidth() ),
                                         fmtLen( m_frame, bb.GetHeight() ) ) );
        }

        wxString drillShape;

        switch( dcode->m_DrillShape )
        {
        case APT_DEF_NO_HOLE:    drillShape = _( "None" );        break;
        case APT_DEF_ROUND_HOLE: drillShape = _( "Round" );       break;
        case APT_DEF_RECT_HOLE:  drillShape = _( "Rectangular" ); break;
        default:                 drillShape = wxT( "?" );         break;
        }

        appendRow( box->GetStaticBox(), grid, _( "Drill shape" ), drillShape );

        if( dcode->m_DrillShape != APT_DEF_NO_HOLE )
        {
            appendRow( box->GetStaticBox(), grid, _( "Drill X" ),
                       fmtLen( m_frame, dcode->m_Drill.x ) );

            if( dcode->m_DrillShape == APT_DEF_RECT_HOLE )
                appendRow( box->GetStaticBox(), grid, _( "Drill Y" ),
                           fmtLen( m_frame, dcode->m_Drill.y ) );
        }

        box->Add( grid, 0, wxEXPAND | wxALL, 6 );

        // Graphical preview of the actual flashed shape.
        APERTURE_PREVIEW_PANEL* preview =
                new APERTURE_PREVIEW_PANEL( box->GetStaticBox(), m_item );
        box->Add( preview, 1, wxEXPAND | wxALL, 6 );

        m_contentSizer->Add( box, 1, wxEXPAND | wxALL, 6 );

        // ---- Aperture Macro -----------------------------------------------------------
        if( dcode->m_ApertType == APT_MACRO && dcode->GetMacro() )
        {
            wxStaticBoxSizer* mbox =
                    new wxStaticBoxSizer( wxVERTICAL, m_contentPanel, _( "Aperture Macro" ) );
            wxFlexGridSizer* mgrid = new wxFlexGridSizer( 0, 2, 4, 8 );
            mgrid->AddGrowableCol( 1 );

            appendRow( mbox->GetStaticBox(), mgrid, _( "Macro name" ),
                       dcode->GetMacro()->m_AmName );

            wxString params;

            for( unsigned i = 1; i <= dcode->GetParamCount(); ++i )
            {
                if( i > 1 )
                    params << wxT( ", " );

                params << wxString::Format( wxT( "$%u=%g" ), i, dcode->GetParam( i ) );
            }

            appendRow( mbox->GetStaticBox(), mgrid, _( "Parameters" ),
                       params.IsEmpty() ? wxString( wxT( "\u2014" ) ) : params );

            mbox->Add( mgrid, 1, wxEXPAND | wxALL, 6 );
            m_contentSizer->Add( mbox, 0, wxEXPAND | wxALL, 6 );
        }
    }

    if( m_highlightDCode )
        m_highlightDCode->Enable( dcode != nullptr );

    Layout();
    Fit();
}


void DIALOG_GERBER_ITEM_PROPERTIES::onCloseButton( wxCommandEvent& aEvent )
{
    Close();
}


void DIALOG_GERBER_ITEM_PROPERTIES::onClose( wxCloseEvent& aEvent )
{
    if( m_frame && m_frame->m_propertiesDialog == this )
        m_frame->m_propertiesDialog = nullptr;

    Destroy();
}


void DIALOG_GERBER_ITEM_PROPERTIES::onHighlightToggled( wxCommandEvent& aEvent )
{
    if( m_highlightDCode && m_highlightDCode->IsChecked() )
        applyHighlight();
    else
        clearHighlight();
}


void DIALOG_GERBER_ITEM_PROPERTIES::applyHighlight()
{
    if( !m_item || !m_frame )
        return;

    D_CODE* dcode = m_item->GetDcodeDescr();

    if( !dcode )
        return;

    GERBER_FILE_IMAGE* gerber = m_frame->GetGbrImage( m_frame->GetActiveLayer() );

    if( !gerber )
        return;

    auto* painter =
            static_cast<KIGFX::GERBVIEW_PAINTER*>( m_frame->GetCanvas()->GetView()->GetPainter() );
    KIGFX::GERBVIEW_RENDER_SETTINGS* settings = painter->GetSettings();

    settings->m_dcodeHighlightValue = dcode->m_Num_Dcode;
    gerber->m_Selected_Tool = dcode->m_Num_Dcode;
    m_frame->syncLayerBox( false );

    m_frame->GetCanvas()->GetView()->UpdateAllItems( KIGFX::COLOR );
    m_frame->GetCanvas()->Refresh();
}


void DIALOG_GERBER_ITEM_PROPERTIES::clearHighlight()
{
    if( !m_frame )
        return;

    auto* painter =
            static_cast<KIGFX::GERBVIEW_PAINTER*>( m_frame->GetCanvas()->GetView()->GetPainter() );

    if( !painter )
        return;

    KIGFX::GERBVIEW_RENDER_SETTINGS* settings = painter->GetSettings();

    settings->m_dcodeHighlightValue = -1;

    if( GERBER_FILE_IMAGE* gerber = m_frame->GetGbrImage( m_frame->GetActiveLayer() ) )
        gerber->m_Selected_Tool = 0;

    m_frame->syncLayerBox( false );
    m_frame->GetCanvas()->GetView()->UpdateAllItems( KIGFX::COLOR );
    m_frame->GetCanvas()->Refresh();
}
