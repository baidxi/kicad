/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2020 Ian McInerney <Ian.S.McInerney at ieee.org>
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

#include <kiplatform/ui.h>

#include <wx/choice.h>
#include <wx/dataview.h>
#include <wx/dialog.h>
#include <wx/nonownedwnd.h>
#include <wx/popupwin.h>
#include <wx/settings.h>
#include <wx/window.h>
#include <wx/log.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gio/gio.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#ifdef KICAD_WAYLAND
#include "wayland-pointer-constraints-unstable-v1.h"
#endif

// Set WXTRACE=KICAD_WAYLAND to see logs
const wxString traceWayland = wxS( "KICAD_WAYLAND" );
static const wxChar traceRhi[] = wxT( "KICAD_RHI" );


bool KIPLATFORM::UI::IsDarkTheme()
{
    wxColour bg = wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW );

    // Weighted W3C formula
    double brightness = ( bg.Red() / 255.0 ) * 0.299 +
                        ( bg.Green() / 255.0 ) * 0.587 +
                        ( bg.Blue() / 255.0 ) * 0.117;

    return brightness < 0.5;
}


wxColour KIPLATFORM::UI::GetDialogBGColour()
{
    return wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE );
}


void KIPLATFORM::UI::GetInfoBarColours( wxColour& aFgColour, wxColour& aBgColour )
{
    // The GTK3.24 way of getting the colours is to use the style context
    // Earlier GTKs should be able to use the system settings
#if( GTK_CHECK_VERSION( 3, 24, 0 ) )
    GdkRGBA*         rgba;
    GtkWidgetPath*   path = gtk_widget_path_new();
    GtkStyleContext* sc = gtk_style_context_new();

    gtk_widget_path_append_type( path, GTK_TYPE_WINDOW );
    gtk_widget_path_iter_set_object_name( path, -1, "infobar" );
    gtk_widget_path_iter_add_class( path, -1, "info" );

    gtk_style_context_set_path( sc, path );
    gtk_style_context_set_state( sc, GTK_STATE_FLAG_NORMAL );

    gtk_style_context_get( sc, GTK_STATE_FLAG_NORMAL, "background-color", &rgba, NULL );
    aBgColour = wxColour( *rgba );
    gdk_rgba_free( rgba );

    gtk_style_context_get( sc, GTK_STATE_FLAG_NORMAL, "color", &rgba, NULL );
    aFgColour = wxColour( *rgba );
    gdk_rgba_free( rgba );

    // Some GTK themes use the plain infobar style, but if they don't, the background alpha
    // is generally 0. In this case, try the revealer and box as these are used for Adwaita
    // and other themes.
    if( aBgColour.Alpha() == 0 )
    {
        gtk_widget_path_append_type( path, G_TYPE_NONE );
        gtk_widget_path_iter_set_object_name( path, -1, "revealer" );
        gtk_widget_path_append_type( path, G_TYPE_NONE );
        gtk_widget_path_iter_set_object_name( path, -1, "box" );

        gtk_style_context_set_path( sc, path );
        gtk_style_context_set_state( sc, GTK_STATE_FLAG_NORMAL );

        gtk_style_context_get( sc, GTK_STATE_FLAG_NORMAL, "background-color", &rgba, NULL );
        aBgColour = wxColour( *rgba );
        gdk_rgba_free( rgba );

        gtk_style_context_get( sc, GTK_STATE_FLAG_NORMAL, "color", &rgba, NULL );
        aFgColour = wxColour( *rgba );
        gdk_rgba_free( rgba );
    }

    gtk_widget_path_free( path );
    g_object_unref( sc );

#else
    aBgColour = wxSystemSettings::GetColour( wxSYS_COLOUR_INFOBK );
    aFgColour = wxSystemSettings::GetColour( wxSYS_COLOUR_INFOTEXT );
#endif

}


#ifdef GDK_WINDOWING_WAYLAND
// Holds widgets over a canvas whose OpenGL subsurface covers everything GTK draws in the frame
// Subsurfaces stack in the order they are mapped, so this one is remapped after the canvas
class CANVAS_OVERLAY : public wxPopupWindow
{
public:
    CANVAS_OVERLAY( wxWindow* aCanvas ) :
            wxPopupWindow( aCanvas )
    {
        // wx asks for a compositor popup, and GDK maps a popup with any other hint as a subsurface
        gtk_window_set_type_hint( GTK_WINDOW( m_widget ), GDK_WINDOW_TYPE_HINT_NORMAL );

        aCanvas->Bind( wxEVT_SIZE, &CANVAS_OVERLAY::onCanvasSize, this );
        g_signal_connect( aCanvas->GetHandle(), "map", G_CALLBACK( onCanvasMap ), this );
    }

    ~CANVAS_OVERLAY() override
    {
        GetParent()->Unbind( wxEVT_SIZE, &CANVAS_OVERLAY::onCanvasSize, this );
        g_signal_handlers_disconnect_by_data( GetParent()->GetHandle(), this );
    }

    void Raise() override
    {
        // The canvas maps its surface from a queued event, so wait for that
        CallAfter( &CANVAS_OVERLAY::remap );
    }

    bool Show( bool aShow = true ) override
    {
        if( aShow )
            follow();

        return wxPopupWindow::Show( aShow );
    }

private:
    static void onCanvasMap( GtkWidget* aWidget, gpointer aOverlay )
    {
        static_cast<CANVAS_OVERLAY*>( aOverlay )->Raise();
    }

    void onCanvasSize( wxSizeEvent& aEvent )
    {
        follow();
        aEvent.Skip();
    }

    void follow()
    {
        // There is no position to follow until the frame is mapped
        if( gtk_widget_get_mapped( GetParent()->GetHandle() ) )
            Move( GetParent()->ClientToScreen( wxPoint( 0, 0 ) ) );
    }

    void remap()
    {
        if( !IsShown() )
            return;

        Hide();
        Show();
    }
};
#endif


wxWindow* KIPLATFORM::UI::CreateCanvasOverlay( wxWindow* aCanvas )
{
#ifdef GDK_WINDOWING_WAYLAND
    if( GDK_IS_WAYLAND_DISPLAY( gdk_display_get_default() ) )
        return new CANVAS_OVERLAY( aCanvas );
#endif

    return nullptr;
}


void KIPLATFORM::UI::ForceFocus( wxWindow* aWindow )
{
    aWindow->SetFocus();

    // On GTK, SetFocus() on a dialog may not be sufficient to receive keyboard events
    // (like ESC for closing). We need to ensure the window is presented and active.
    // Find the top-level window and present it to ensure keyboard focus.
    wxWindow* tlw = aWindow;

    while( tlw && !tlw->IsTopLevel() )
        tlw = tlw->GetParent();

    if( tlw )
    {
        GtkWidget* widget = static_cast<GtkWidget*>( tlw->GetHandle() );

        if( widget && GTK_IS_WINDOW( widget ) )
            gtk_window_present( GTK_WINDOW( widget ) );
    }
}


bool KIPLATFORM::UI::IsWindowActive( wxWindow* aWindow )
{
    if( !aWindow )
        return false;

    GtkWindow* window = GTK_WINDOW( aWindow->GetHandle() );

    if( window )
        return gtk_window_is_active( window );

    // We shouldn't really ever reach this point
    return false;
}


void KIPLATFORM::UI::EnsureVisible( wxWindow* aWindow )
{
    // Not needed on this platform
}


namespace
{

// Temporary diagnostics for the "dialog vanishes after being shown" report under
// Wayland.  Enabled with KICAD_DIALOG_DEBUG=1; appends one line per state probe to
// /tmp/kicad_dialog_debug.log.
void dialogDebugLog( GtkWidget* aWidget, const char* aTag )
{
    static const bool s_enabled = []()
        {
            const char* env = getenv( "KICAD_DIALOG_DEBUG" );
            return env && *env;
        }();

    if( !s_enabled )
        return;

    GdkWindow*     window = gtk_widget_get_window( aWidget );
    GdkWindowState state = window ? gdk_window_get_state( window ) : GDK_WINDOW_STATE_WITHDRAWN;
    gint           x = 0, y = 0, w = 0, h = 0;

    if( window )
        gdk_window_get_geometry( window, &x, &y, &w, &h );

    if( FILE* f = fopen( "/tmp/kicad_dialog_debug.log", "a" ) )
    {
        fprintf( f, "[dialog] %s: mapped=%d visible=%d active=%d state=0x%x geom=%dx%d%+d%+d\n",
                 aTag,
                 gtk_widget_get_mapped( aWidget ) ? 1 : 0,
                 gtk_widget_get_visible( aWidget ) ? 1 : 0,
                 gtk_window_is_active( GTK_WINDOW( aWidget ) ) ? 1 : 0,
                 (unsigned) state, w, h, x, y );

        if( window )
        {
            GdkMonitor*  monitor = gdk_display_get_monitor_at_window( gtk_widget_get_display( aWidget ), window );
            GdkRectangle mgeo = { 0, 0, 0, 0 };
            const gchar* model = monitor ? gdk_monitor_get_model( monitor ) : nullptr;

            if( monitor )
                gdk_monitor_get_geometry( monitor, &mgeo );

            fprintf( f, "[dialog]   monitor=%s geo=%dx%d%+d%+d scale=%d\n",
                     model ? model : "?", mgeo.width, mgeo.height, mgeo.x, mgeo.y,
                     monitor ? gdk_monitor_get_scale_factor( monitor ) : 0 );
        }

        fclose( f );
    }
}

struct DialogDebugCtx
{
    GtkWidget* widget;
    int        rescued;     // forced remaps performed so far (bounded)
    int        attempts;    // polite present() rounds left
    int        patrolCount; // low-frequency patrol ticks while not active
    int        intervalMs;  // delay before the next probe
};

// Patrols a freshly shown toplevel until it is activated or destroyed.
//
// Two failure modes are handled:
//  1. Mutter occasionally maps a new transient toplevel without activating it, stacking
//     it below the (disabled) parent.  Activation is requested politely through
//     gtk_window_present() (xdg-activation) for a bounded number of rounds.
//  2. The compositor can unmap the dialog while its (quasi-)modal event loop is still
//     running.  Because the modal loop keeps the whole parent frame disabled, an
//     invisible dialog leaves every window unclickable -- the guard restores visibility
//     as soon as it notices the dialog vanished.
//
// The patrol runs for the lifetime of the dialog (the weak pointer self-clears when the
// window is destroyed) but only issues a bounded number of present() calls so it never
// steals focus from other applications the user switched to.
static gboolean dialogActivationGuard( gpointer aData )
{
    DialogDebugCtx* ctx = static_cast<DialogDebugCtx*>( aData );

    if( !ctx->widget )
    {
        // Window destroyed while we were waiting.
        delete ctx;
        return G_SOURCE_REMOVE;
    }

    if( gtk_window_is_active( GTK_WINDOW( ctx->widget ) ) )
    {
        // Active: done.  Clear any urgency hint set while the compositor had stacked the
        // dialog below its parent.
        gtk_window_set_urgency_hint( GTK_WINDOW( ctx->widget ), FALSE );
        g_object_remove_weak_pointer( G_OBJECT( ctx->widget ),
                                      reinterpret_cast<gpointer*>( &ctx->widget ) );
        delete ctx;
        return G_SOURCE_REMOVE;
    }

    if( !gtk_widget_get_visible( ctx->widget ) )
    {
        // The dialog vanished underneath a still-running modal loop; the weak pointer
        // is valid so the dialog object exists.  Bring it back and re-arm the activation
        // attempts.
        dialogDebugLog( ctx->widget, "activation-guard:reshow" );
        gtk_widget_show( ctx->widget );
        gtk_window_present( GTK_WINDOW( ctx->widget ) );
        ctx->attempts = 2;
        ctx->patrolCount = 0;
    }
    else if( ctx->attempts > 0 )
    {
        // Ask the compositor to activate us via xdg-activation and re-check shortly.
        dialogDebugLog( ctx->widget, "activation-guard:present" );
        gtk_window_present( GTK_WINDOW( ctx->widget ) );
        ctx->attempts--;
        ctx->patrolCount = 0;
    }
    else if( ++ctx->patrolCount >= 8 && ctx->rescued < 2 )
    {
        // Activation was denied and the dialog never became active: the compositor
        // stacked it below its (disabled) parent, which freezes the whole UI for as
        // long as the modal loop runs.  After a 2s grace period force a remap --
        // mutter re-evaluates the placement of a freshly mapped toplevel and then
        // activates the dialog.  Bounded to two attempts per dialog so a pathological
        // compositor cannot turn this into a flashing loop.
        dialogDebugLog( ctx->widget, "activation-guard:remap" );
        gtk_widget_hide( ctx->widget );
        gtk_widget_show( ctx->widget );
        gtk_window_present( GTK_WINDOW( ctx->widget ) );
        ctx->rescued++;
        ctx->attempts = 2;
        ctx->patrolCount = 0;
    }
    else if( ctx->patrolCount == 12 )
    {
        // Still not active ~3s after the rescue attempts: the compositor keeps the dialog
        // stacked below its (disabled) parent.  Mark it urgent so the taskbar/dock flashes
        // -- a single click on the dock entry brings the dialog back and unblocks the UI.
        dialogDebugLog( ctx->widget, "activation-guard:urgent" );
        gtk_window_set_urgency_hint( GTK_WINDOW( ctx->widget ), TRUE );
    }

    // Keep patrolling: fast while activation attempts remain, then a gentle 250ms check
    // that never calls present() on its own (so it cannot steal focus from the
    // application the user switched to).
    ctx->intervalMs = ( ctx->attempts > 1 ) ? 50 : 250;
    g_timeout_add( ctx->intervalMs, dialogActivationGuard, ctx );
    return G_SOURCE_REMOVE;
}

// Debug-only late probe (gated by KICAD_DIALOG_DEBUG).
static gboolean dialogDebugDelayed( gpointer aData )
{
    DialogDebugCtx* ctx = static_cast<DialogDebugCtx*>( aData );

    if( ctx->widget )
    {
        g_object_remove_weak_pointer( G_OBJECT( ctx->widget ),
                                      reinterpret_cast<gpointer*>( &ctx->widget ) );

        dialogDebugLog( ctx->widget,
                        gtk_widget_get_visible( ctx->widget ) ? "delayed:visible" : "delayed:hidden" );
    }

    delete ctx;
    return G_SOURCE_REMOVE;
}

// Schedule the one-shot activation guard for a freshly shown toplevel (see
// dialogActivationGuard above).  Safe to call repeatedly: the guard self-removes and
// holds the widget through a weak pointer, so a destroyed window is a no-op.
static void scheduleActivationGuard( GtkWidget* aWidget )
{
    DialogDebugCtx* ctx = new DialogDebugCtx{ aWidget, 0, 4, 0, 50 };
    g_object_add_weak_pointer( G_OBJECT( aWidget ),
                               reinterpret_cast<gpointer*>( &ctx->widget ) );
    g_timeout_add( 40, dialogActivationGuard, ctx );
}


} // namespace


void KIPLATFORM::UI::SetDialogModal( wxWindow* aWindow, bool aModal )
{
    GtkWidget* widget = static_cast<GtkWidget*>( aWindow->GetHandle() );

    if( !widget || !GTK_IS_WINDOW( widget ) )
        return;

    GdkDisplay* display = gtk_widget_get_display( widget );

    // X11 window managers honour raise requests, so keep the quasi-modal semantics there.
    if( !GDK_IS_WAYLAND_DISPLAY( display ) )
        return;

    // Under Wayland a quasi-modal dialog cannot reliably be raised above its disabled
    // parent (no client-side stacking, focus-stealing prevention denies activation to
    // the newly mapped window).  The modal flag must be set before the window is
    // mapped so GTK takes the full modal path from the start.
    // The modal flag MUST be set before the window is mapped: without it the failure
    // rate of the mutter-compositing bug below goes to 100%.  Keep the activation guard
    // disabled though -- WAYLAND_DEBUG traces prove the guard's present()/remap calls
    // cannot help: the compositor already activates the window (wl_keyboard.enter is
    // delivered) while its texture is never put on any output.
    gtk_window_set_modal( GTK_WINDOW( widget ), aModal ? TRUE : FALSE );

    if( aModal )
    {
        dialogDebugLog( widget, "on-show" );

        // scheduleActivationGuard( widget );

        // Late diagnostic probe, only when explicitly debugging.
        if( const char* env = getenv( "KICAD_DIALOG_DEBUG" ); env && *env )
        {
            DialogDebugCtx* probe = new DialogDebugCtx{ widget, 0, 0, 0, 0 };
            g_object_add_weak_pointer( G_OBJECT( widget ),
                                       reinterpret_cast<gpointer*>( &probe->widget ) );
            g_timeout_add( 2500, dialogDebugDelayed, probe );
        }
    }
    else
    {
        dialogDebugLog( widget, "on-hide" );
    }
}


void KIPLATFORM::UI::RaiseAndActivate( wxWindow* aWindow )
{
    if( !aWindow )
        return;

    GtkWidget* widget = static_cast<GtkWidget*>( aWindow->GetHandle() );

    if( !widget || !GTK_IS_WINDOW( widget ) )
        return;

    GdkDisplay* display = gtk_widget_get_display( widget );

    if( GDK_IS_WAYLAND_DISPLAY( display ) )
    {
        // No client-side stacking under Wayland: wxWindow::Raise() is an asynchronous hint
        // at best, and mutter routinely denies activation to a freshly mapped toplevel,
        // stacking it below the currently active window (often the maximized frame the
        // user clicked from).  gtk_window_present() routes through xdg-activation so the
        // compositor is asked to focus (and unminimize) the window; if activation is still
        // denied, the remap guard forces a re-evaluation a couple of frames later.
        gtk_window_present( GTK_WINDOW( widget ) );
        scheduleActivationGuard( widget );
    }
    else
    {
        // X11 window managers honour raise requests.
        aWindow->Raise();
        aWindow->SetFocus();
    }
}


void KIPLATFORM::UI::StabilizeWindowPosition( wxWindow* aWindow )
{
    if( !aWindow )
        return;

    GtkWidget* widget = static_cast<GtkWidget*>( aWindow->GetHandle() );

    if( widget && GTK_IS_WINDOW( widget ) )
        gtk_window_set_gravity( GTK_WINDOW( widget ), GDK_GRAVITY_STATIC );
}


static const char* const WM_CLASS_DATA_KEY = "kicad-wm-class";


// Re-assert our application id on the widget's window.  Bound to "realize" and "map" because GTK
// writes WM_CLASS from the program class when it realizes the window, so ours must be applied
// afterwards to survive; the Wayland application id additionally requires a mapped surface.  The
// id is read from the widget's object data so its lifetime is not tied to the signal closures.
static void setWindowClassHint( GtkWidget* aWidget, gpointer )
{
    const char* id = static_cast<const char*>( g_object_get_data( G_OBJECT( aWidget ),
                                                                  WM_CLASS_DATA_KEY ) );

    if( !id || !*id )
        return;

    GdkWindow* window = gtk_widget_get_window( aWidget );

    if( !window )
        return;

#ifdef GDK_WINDOWING_X11
    if( GDK_IS_X11_WINDOW( window ) )
    {
        // wxGTK sets res_class from the app display name; overwrite both WM_CLASS fields with
        // the desktop id so the window manager matches us to our installed .desktop launcher.
        if( XClassHint* hints = XAllocClassHint() )
        {
            hints->res_name  = const_cast<char*>( id );
            hints->res_class = const_cast<char*>( id );

            XSetClassHint( GDK_WINDOW_XDISPLAY( window ), GDK_WINDOW_XID( window ), hints );
            XFree( hints );
        }
    }
#endif

#if defined( GDK_WINDOWING_WAYLAND ) && GTK_CHECK_VERSION( 3, 24, 22 )
    // Covers wx < 3.3.1, which never propagates the class name to the Wayland application id.
    if( GDK_IS_WAYLAND_WINDOW( window ) && gtk_check_version( 3, 24, 22 ) == nullptr )
        gdk_wayland_window_set_application_id( window, id );
#endif
}


void KIPLATFORM::UI::SetWMClass( wxWindow* aWindow, const wxString& aClass )
{
    if( !aWindow || aClass.IsEmpty() )
        return;

    GtkWidget* widget = static_cast<GtkWidget*>( aWindow->GetHandle() );

    if( !widget )
        return;

    // Own a single copy of the id on the widget; it is freed when the widget is destroyed.  The
    // signal handlers read it back from here rather than capturing it, so repeated calls stay safe.
    g_object_set_data_full( G_OBJECT( widget ), WM_CLASS_DATA_KEY, g_strdup( aClass.utf8_str() ),
                            g_free );

    // Guard against stacking duplicate handlers if this is called more than once for a widget.
    g_signal_handlers_disconnect_by_func( widget, reinterpret_cast<void*>( setWindowClassHint ),
                                          nullptr );

    g_signal_connect_after( widget, "realize", G_CALLBACK( setWindowClassHint ), nullptr );
    g_signal_connect_after( widget, "map", G_CALLBACK( setWindowClassHint ), nullptr );

    // Apply immediately for windows already realized when this is called.
    if( gtk_widget_get_realized( widget ) )
        setWindowClassHint( widget, nullptr );
}


void KIPLATFORM::UI::ReparentModal( wxNonOwnedWindow* aWindow )
{
    // Not needed on this platform
}


void KIPLATFORM::UI::ReparentWindow( wxNonOwnedWindow* aWindow, wxTopLevelWindow* aParent )
{
    // Not needed on this platform (only relevant for macOS child window ordering)
}


void KIPLATFORM::UI::FixupCancelButtonCmdKeyCollision( wxWindow *aWindow )
{
    // Not needed on this platform
}


bool KIPLATFORM::UI::IsStockCursorOk( wxStockCursor aCursor )
{
    switch( aCursor )
    {
    case wxCURSOR_BULLSEYE:
    case wxCURSOR_HAND:
    case wxCURSOR_ARROW:
    case wxCURSOR_BLANK:
        return true;
    default:
        return false;
    }
}


/**
 * The following two functions are based on the "hack" contained in the attached patch at
 * https://gitlab.gnome.org/GNOME/gtk/-/issues/1910 which is supposed to speed up creation of
 * GTK choice boxes.
 *
 * The basic idea is to disable some of the event handlers on the menus for the choice box to
 * prevent them from running, which will speed up the creation of the choice box and its popup menu.
 */
static void disable_area_apply_attributes_cb( GtkWidget* pItem, gpointer userdata )
{
    // GTK needs this enormous chain to get the actual type of item that we want
    GtkMenuItem*   pMenuItem   = GTK_MENU_ITEM( pItem );
    GtkWidget*     child       = gtk_bin_get_child( GTK_BIN( pMenuItem ) );
    GtkCellView*   pCellView   = GTK_CELL_VIEW( child );
    GtkCellLayout* pCellLayout = GTK_CELL_LAYOUT( pCellView );
    GtkCellArea*   pCellArea   = gtk_cell_layout_get_area( pCellLayout );

    g_signal_handlers_block_matched( pCellArea, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, userdata );
}


void KIPLATFORM::UI::LargeChoiceBoxHack( wxChoice* aChoice )
{
    AtkObject* atkObj = gtk_combo_box_get_popup_accessible( GTK_COMBO_BOX( aChoice->m_widget ) );

    if( !atkObj || !GTK_IS_ACCESSIBLE( atkObj ) )
        return;

    GtkWidget* widget = gtk_accessible_get_widget( GTK_ACCESSIBLE( atkObj ) );

    if( !widget || !GTK_IS_MENU( widget ) )
        return;

    GtkMenu* menu = GTK_MENU( widget );

    gtk_container_foreach( GTK_CONTAINER( menu ), disable_area_apply_attributes_cb, menu );
}


void KIPLATFORM::UI::EllipsizeChoiceBox( wxChoice* aChoice )
{
    // This function is based on the code inside the function post_process_ui in
    // gtkfilechooserwidget.c
    GList* cells = gtk_cell_layout_get_cells( GTK_CELL_LAYOUT( aChoice->m_widget ) );

    if( !cells )
        return;

    GtkCellRenderer* cell = (GtkCellRenderer*) cells->data;

    if( !cell )
        return;

    g_object_set( G_OBJECT( cell ), "ellipsize", PANGO_ELLIPSIZE_END, nullptr );

    // Only the list of cells must be freed, the renderer isn't ours to free
    g_list_free( cells );
}


double KIPLATFORM::UI::GetPixelScaleFactor( const wxWindow* aWindow )
{
    double val = 1.0;

    GtkWidget* widget = static_cast<GtkWidget*>( aWindow->GetHandle() );

    if( widget && gtk_check_version( 3, 10, 0 ) == nullptr )
        val = gtk_widget_get_scale_factor( widget );

    return val;
}


double KIPLATFORM::UI::GetContentScaleFactor( const wxWindow* aWindow )
{
    // TODO: Do we need something different here?
    return GetPixelScaleFactor( aWindow );
}


double KIPLATFORM::UI::GetDisplayRefreshRate( const wxWindow* aWindow )
{
#if GTK_CHECK_VERSION( 3, 22, 0 )
    GtkWidget* widget = static_cast<GtkWidget*>( aWindow->GetHandle() );
    GdkWindow* window = widget ? gtk_widget_get_window( widget ) : nullptr;

    if( !window )
        return 0.0;

    GdkMonitor* monitor = gdk_display_get_monitor_at_window( gdk_window_get_display( window ), window );

    // GDK reports millihertz, and 0 when it does not know
    return monitor ? gdk_monitor_get_refresh_rate( monitor ) / 1000.0 : 0.0;
#else
    return 0.0;
#endif
}


wxSize KIPLATFORM::UI::GetUnobscuredSize( const wxWindow* aWindow )
{
    return wxSize( aWindow->GetSize().GetX() - wxSystemSettings::GetMetric( wxSYS_VSCROLL_X ),
                   aWindow->GetSize().GetY() - wxSystemSettings::GetMetric( wxSYS_HSCROLL_Y ) );
}


void KIPLATFORM::UI::SetOverlayScrolling( const wxWindow* aWindow, bool overlay )
{
    gtk_scrolled_window_set_overlay_scrolling( GTK_SCROLLED_WINDOW( aWindow->GetHandle() ),
                                               overlay );
}


bool KIPLATFORM::UI::AllowIconsInMenus()
{
    gboolean allowed = 1;

    g_object_get( gtk_settings_get_default(), "gtk-menu-images", &allowed, NULL );

    return !!allowed;
}


bool KIPLATFORM::UI::SupportsKeyState()
{
#ifdef GDK_WINDOWING_X11
    GdkDisplay* display = gdk_display_get_default();

    return display && GDK_IS_X11_DISPLAY( display );
#else
    return false;
#endif
}


// GNOME's "disable touchpad while typing" (DWT) freezes the touchpad while a character key is
// held down (including key auto-repeat).  That breaks panning with a non-modifier motion-pan
// key, so KiCad offers to manage this setting for the user (see PANEL_MOUSE_SETTINGS).
static GSettings* getTouchpadSettings()
{
    GSettingsSchemaSource* source = g_settings_schema_source_get_default();

    if( !source )
        return nullptr;

    GSettingsSchema* schema = g_settings_schema_source_lookup(
            source, "org.gnome.desktop.peripherals.touchpad", TRUE );

    if( !schema )
        return nullptr;

    GSettings* settings = g_settings_new_full( schema, nullptr, nullptr );
    g_settings_schema_unref( schema );

    return settings;
}


std::optional<bool> KIPLATFORM::UI::GetTouchpadDisableWhileTyping()
{
    GSettings* settings = getTouchpadSettings();

    if( !settings )
        return std::nullopt;

    std::optional<bool> result = g_settings_get_boolean( settings, "disable-while-typing" ) != FALSE;
    g_object_unref( settings );

    return result;
}


bool KIPLATFORM::UI::SetTouchpadDisableWhileTyping( bool aEnable )
{
    GSettings* settings = getTouchpadSettings();

    if( !settings )
        return false;

    g_settings_set_boolean( settings, "disable-while-typing", aEnable ? TRUE : FALSE );
    g_object_unref( settings );

    return true;
}


#if defined( GDK_WINDOWING_WAYLAND ) && defined( KICAD_WAYLAND )

static bool wayland_warp_pointer( GtkWidget* aWidget, GdkDisplay* aDisplay, GdkWindow* aWindow,
                                  GdkDevice* aPtrDev, int aX, int aY );

// GDK doesn't know if we've moved the cursor using Wayland pointer constraints.
// So emulate the actual position here
static wxPoint s_warped_from;
static wxPoint s_warped_to;

wxPoint KIPLATFORM::UI::GetMousePosition()
{
    wxPoint wx_pos = wxGetMousePosition();

    if( wx_pos == s_warped_from )
    {
        wxLogTrace( traceWayland, wxS( "Faked mouse pos %d %d -> %d %d" ), wx_pos.x, wx_pos.y,
                    s_warped_to.x, s_warped_to.y );

        return s_warped_to;
    }
    else
    {
        // Mouse has moved
        s_warped_from = wxPoint();
        s_warped_to = wxPoint();
    }

    return wx_pos;
}

#endif


bool KIPLATFORM::UI::WarpPointer( wxWindow* aWindow, int aX, int aY )
{
    if( !wxGetEnv( wxT( "WAYLAND_DISPLAY" ), nullptr ) )
    {
        aWindow->WarpPointer( aX, aY );
        return true;
    }
    else
    {
        GtkWidget* widget = static_cast<GtkWidget*>( aWindow->GetHandle() );

        GdkDisplay* disp = gtk_widget_get_display( widget );
        GdkSeat*    seat = gdk_display_get_default_seat( disp );
        GdkDevice*  ptrdev = gdk_seat_get_pointer( seat );

#if defined( GDK_WINDOWING_WAYLAND ) && defined( KICAD_WAYLAND )
        if( GDK_IS_WAYLAND_DISPLAY( disp ) )
        {
            wxPoint    initialPos = wxGetMousePosition();
            GdkWindow* win = aWindow->GTKGetDrawingWindow();

            if( wayland_warp_pointer( widget, disp, win, ptrdev, aX, aY ) )
            {
                s_warped_from = initialPos;
                s_warped_to = aWindow->ClientToScreen( wxPoint( aX, aY ) );

                wxLogTrace( traceWayland, wxS( "Set warped from %d %d to %d %d" ), s_warped_from.x,
                            s_warped_from.y, s_warped_to.x, s_warped_to.y );

                return true;
            }

            wxLogTrace( traceWayland, wxS( "*** Warp to %d %d failed ***" ), aX, aY );

            return false;
        }
#endif
#ifdef GDK_WINDOWING_X11
        if( GDK_IS_X11_DISPLAY( disp ) )
        {
            GdkWindow* win = gdk_device_get_window_at_position( ptrdev, nullptr, nullptr );

            if( !win )
            {
                aWindow->WarpPointer( aX, aY );
                return true;
            }

            GdkCursor* blank_cursor = gdk_cursor_new_for_display( disp, GDK_BLANK_CURSOR );
            GdkCursor* cur_cursor = gdk_window_get_cursor( win );

            if( cur_cursor )
                g_object_ref( cur_cursor );

            gdk_window_set_cursor( win, blank_cursor );
            aWindow->WarpPointer( aX, aY );
            gdk_window_set_cursor( win, cur_cursor );

            if( cur_cursor )
                g_object_unref( cur_cursor );

            if( blank_cursor )
                g_object_unref( blank_cursor );

            return true;
        }
#endif
    }

    return false;
}


void KIPLATFORM::UI::ImmControl( wxWindow* aWindow, bool aEnable )
{
}


void KIPLATFORM::UI::ImeNotifyCancelComposition( wxWindow* aWindow )
{
    wxWindowGTK* win = static_cast<wxWindowGTK*>( aWindow );
    if( win )
    {
        GtkIMContext* imContext = win->m_imContext;
        if( imContext )
        {
            gtk_im_context_focus_out( imContext );
        }
    }
}


void KIPLATFORM::UI::SetFloatLevel( wxWindow* aWindow )
{
}


void KIPLATFORM::UI::ReleaseChildWindow( wxNonOwnedWindow* aWindow )
{
    // Not needed on this platform
}


void KIPLATFORM::UI::AllowNetworkFileSystems( wxDialog* aDialog )
{
    GtkWidget* widget = static_cast<GtkWidget*>( aDialog->GetHandle() );

    if( widget && GTK_IS_FILE_CHOOSER( widget ) )
        gtk_file_chooser_set_local_only( GTK_FILE_CHOOSER( widget ), FALSE );
}


void KIPLATFORM::UI::CancelPendingScroll( wxDataViewCtrl* aCtrl )
{
    if( !aCtrl )
        return;

    GtkWidget* widget = aCtrl->GtkGetTreeView();

    if( !widget || !GTK_IS_TREE_VIEW( widget ) )
        return;

    GtkTreeView* view = GTK_TREE_VIEW( widget );

    // Need a live model and rbtree for the assertions in scroll_to_cell.
    if( !gtk_tree_view_get_model( view ) )
        return;

    GtkTreeViewColumn* column = gtk_tree_view_get_column( view, 0 );

    if( !column )
        return;

    // A column-only scroll_to_cell enters GTK's deferred-scroll path, which frees
    // priv->scroll_to_path and only installs scroll_to_column.  That breaks the race
    // where validate_visible_area dereferences a stale row reference after a model
    // reset.  The deferred branch is taken when the widget is not yet allocated, when
    // alloc is pending, or when rows are invalid -- queue_resize forces the latter so
    // a steady-state widget also takes that branch and actually clears the reference.
    gtk_widget_queue_resize( widget );
    gtk_tree_view_scroll_to_cell( view, nullptr, column, FALSE, 0.0, 0.0 );
}

//
// **** Wayland hacks ahead ****
//

#if defined( GDK_WINDOWING_WAYLAND ) && defined( KICAD_WAYLAND )

static bool                               s_wl_initialized = false;
static struct wl_compositor*              s_wl_compositor = NULL;
static struct zwp_pointer_constraints_v1* s_wl_pointer_constraints = NULL;
static struct zwp_confined_pointer_v1*    s_wl_confined_pointer = NULL;
static struct wl_region*                  s_wl_confinement_region = NULL;
static bool                               s_wl_locked_flag = false;

static void handle_global( void* data, struct wl_registry* registry, uint32_t name,
                           const char* interface, uint32_t version )
{
    wxLogTrace( traceWayland, "handle_global received %s name %u version %u", interface,
                (unsigned int) name, (unsigned int) version );

    if( strcmp( interface, wl_compositor_interface.name ) == 0 )
    {
        s_wl_compositor = static_cast<wl_compositor*>(
                wl_registry_bind( registry, name, &wl_compositor_interface, version ) );
    }
    else if( strcmp( interface, zwp_pointer_constraints_v1_interface.name ) == 0 )
    {
        s_wl_pointer_constraints = static_cast<zwp_pointer_constraints_v1*>( wl_registry_bind(
                registry, name, &zwp_pointer_constraints_v1_interface, version ) );
    }
}

static void handle_global_remove( void*, struct wl_registry*, uint32_t name )
{
    wxLogTrace( traceWayland, "handle_global_remove name %u", (unsigned int) name );
}

static const struct wl_registry_listener registry_listener = {
    .global = handle_global,
    .global_remove = handle_global_remove,
};

static void confined_handler( void* data, struct zwp_confined_pointer_v1* zwp_confined_pointer_v1 )
{
    wxLogTrace( traceWayland, wxS( "Pointer confined" ) );
}

static void unconfined_handler( void*                           data,
                                struct zwp_confined_pointer_v1* zwp_confined_pointer_v1 )
{
    wxLogTrace( traceWayland, wxS( "Pointer unconfined" ) );
}

static const struct zwp_confined_pointer_v1_listener confined_pointer_listener = {
    .confined = confined_handler,
    .unconfined = unconfined_handler
};

static void locked_handler( void* data, struct zwp_locked_pointer_v1* zwp_locked_pointer_v1 )
{
    s_wl_locked_flag = true;
    wxLogTrace( traceWayland, wxS( "Pointer locked" ) );
}

static void unlocked_handler( void* data, struct zwp_locked_pointer_v1* zwp_locked_pointer_v1 )
{
    wxLogTrace( traceWayland, wxS( "Pointer unlocked" ) );
}

static const struct zwp_locked_pointer_v1_listener locked_pointer_listener = {
    .locked = locked_handler,
    .unlocked = unlocked_handler
};


/**
 * Initializes `compositor` and `pointer_constraints` global variables.
 */
static void initialize_wayland( wl_display* wldisp )
{
    if( s_wl_initialized )
    {
        return;
    }

    struct wl_registry* registry = wl_display_get_registry( wldisp );
    wl_registry_add_listener( registry, &registry_listener, NULL );
    wl_display_roundtrip( wldisp );
    s_wl_initialized = true;
}


// GNOME's mutter (verified on GNOME Shell 50) only evaluates pointer constraints on
// XWayland surfaces or Wayland subsurfaces: a constraint created on a plain toplevel
// surface makes should_constraint_be_enabled() fail its runtime check, mutter logs
// "WL: error in client communication" and kills our Wayland connection -- which exits
// the whole application without a crash, a core file or any client-side trace.
// Skip the pointer-constraints protocols there; the infinite-drag and cursor-warp
// callers degrade to plain drags.  Set KICAD_WAYLAND_INFINITE_DRAG=1 to opt back in
// (e.g. when testing a compositor that fixed the restriction).
static bool waylandConstraintsKilledByCompositor()
{
    static const bool s_broken = []
        {
            wxString desktop;

            if( wxGetEnv( wxT( "XDG_CURRENT_DESKTOP" ), &desktop )
                    && desktop.Upper().Contains( wxT( "GNOME" ) ) )
            {
                wxString optin;

                if( !wxGetEnv( wxT( "KICAD_WAYLAND_INFINITE_DRAG" ), &optin ) || optin.IsEmpty()
                        || optin == wxT( "0" ) )
                {
                    wxLogTrace( traceWayland,
                                wxS( "Pointer constraints disabled: mutter kills toplevel "
                                     "constraints (WL: error in client communication)" ) );
                    return true;
                }
            }

            return false;
        }();

    return s_broken;
}


struct zwp_locked_pointer_v1* s_wl_locked_pointer = NULL;

static int s_after_paint_handler_id = 0;

static void on_frame_clock_after_paint( GdkFrameClock* clock, GtkWidget* widget )
{
    if( s_wl_locked_pointer )
    {
        zwp_locked_pointer_v1_destroy( s_wl_locked_pointer );
        s_wl_locked_pointer = NULL;

        wxLogTrace( traceWayland, wxS( "after-paint: locked_pointer destroyed" ) );

        g_signal_handler_disconnect( (gpointer) clock, s_after_paint_handler_id );
        s_after_paint_handler_id = 0;

        // restore confinement
        if( s_wl_confinement_region != NULL )
        {
            wxLogTrace( traceWayland, wxS( "after-paint: Restoring confinement" ) );

            GdkDisplay* disp = gtk_widget_get_display( widget );
            GdkSeat*    seat = gdk_display_get_default_seat( disp );
            GdkDevice*  ptrdev = gdk_seat_get_pointer( seat );
            GdkWindow*  window = gtk_widget_get_window( widget );

            wl_display* wldisp = gdk_wayland_display_get_wl_display( disp );
            wl_surface* wlsurf = gdk_wayland_window_get_wl_surface( window );
            wl_pointer* wlptr = gdk_wayland_device_get_wl_pointer( ptrdev );

            s_wl_confined_pointer = zwp_pointer_constraints_v1_confine_pointer(
                    s_wl_pointer_constraints, wlsurf, wlptr, s_wl_confinement_region,
                    ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT );

            wl_display_roundtrip( wldisp );
        }
    }
}


static bool wayland_warp_pointer( GtkWidget* aWidget, GdkDisplay* aDisplay, GdkWindow* aWindow,
                                  GdkDevice* aPtrDev, int aX, int aY )
{
    wl_display* wldisp = gdk_wayland_display_get_wl_display( aDisplay );
    wl_surface* wlsurf = gdk_wayland_window_get_wl_surface( aWindow );
    wl_pointer* wlptr = gdk_wayland_device_get_wl_pointer( aPtrDev );

    if( s_after_paint_handler_id )
    {
        // Previous paint not done yet
        wxLogTrace( traceWayland, wxS( "Not warping: after-paint pending" ) );
        return false;
    }

    initialize_wayland( wldisp );

    if( waylandConstraintsKilledByCompositor() )
        return false;

    if( s_wl_locked_pointer )
    {
        // This shouldn't happen but let's be safe
        wxLogTrace( traceWayland, wxS( "** Destroying previous locked_pointer **" ) );
        zwp_locked_pointer_v1_destroy( s_wl_locked_pointer );
        wl_display_roundtrip( wldisp );
        s_wl_locked_pointer = NULL;
    }

    // wl_surface_commit causes an assert on GNOME, but has to be called on KDE
    // before destroying the locked pointer, so wait until GDK commits the surface.
    GdkFrameClock* frame_clock = gdk_window_get_frame_clock( aWindow );
    s_after_paint_handler_id = g_signal_connect_after(
            frame_clock, "after-paint", G_CALLBACK( on_frame_clock_after_paint ), aWidget );

    // temporary disable confinement to allow pointer warping
    if( s_wl_confinement_region && s_wl_confined_pointer )
    {
        zwp_confined_pointer_v1_destroy( s_wl_confined_pointer );
        wl_display_roundtrip( wldisp );
        s_wl_confined_pointer = NULL;
    }

    s_wl_locked_flag = false;

    s_wl_locked_pointer =
            zwp_pointer_constraints_v1_lock_pointer( s_wl_pointer_constraints, wlsurf, wlptr, NULL,
                                                     ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT );

    zwp_locked_pointer_v1_add_listener(s_wl_locked_pointer, &locked_pointer_listener, NULL);

    gint wx, wy;
    gtk_widget_translate_coordinates( aWidget, gtk_widget_get_toplevel( aWidget ), 0, 0, &wx, &wy );

    zwp_locked_pointer_v1_set_cursor_position_hint( s_wl_locked_pointer, wl_fixed_from_int( aX + wx ),
                                                    wl_fixed_from_int( aY + wy ) );

    // Don't call wl_surface_commit, wait for GDK because of an assert on GNOME.
    wl_display_roundtrip( wldisp ); // To receive "locked" event.
    gtk_widget_queue_draw( aWidget ); // Needed on some GNOME environment to trigger
                                      // the "after-paint" event handler.

    return s_wl_locked_flag;
}


bool KIPLATFORM::UI::InfiniteDragPrepareWindow( wxWindow* aWindow )
{
    wxLogTrace( traceWayland, wxS( "InfiniteDragPrepareWindow" ) );

    GtkWidget*  widget = static_cast<GtkWidget*>( aWindow->GetHandle() );
    GdkDisplay* disp = gtk_widget_get_display( widget );

    if( GDK_IS_WAYLAND_DISPLAY( disp ) )
    {
        if( s_wl_confined_pointer != NULL )
        {
            KIPLATFORM::UI::InfiniteDragReleaseWindow();
        }

        GdkSeat*   seat = gdk_display_get_default_seat( disp );
        GdkDevice* ptrdev = gdk_seat_get_pointer( seat );
        GdkWindow* win = aWindow->GTKGetDrawingWindow();

        wl_display* wldisp = gdk_wayland_display_get_wl_display( disp );
        wl_surface* wlsurf = gdk_wayland_window_get_wl_surface( win );
        wl_pointer* wlptr = gdk_wayland_device_get_wl_pointer( ptrdev );

        initialize_wayland( wldisp );

        gint x, y, width, height;
        gdk_window_get_geometry( gdk_window_get_toplevel( win ), &x, &y, &width, &height );

        if( waylandConstraintsKilledByCompositor() )
            return false;

        wxLogTrace( traceWayland, wxS( "Confine region: %d %d %d %d" ), x, y, width, height );

        s_wl_confinement_region = wl_compositor_create_region( s_wl_compositor );
        wl_region_add( s_wl_confinement_region, x, y, width, height );

        s_wl_confined_pointer = zwp_pointer_constraints_v1_confine_pointer(
                s_wl_pointer_constraints, wlsurf, wlptr, s_wl_confinement_region,
                ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT );

        zwp_confined_pointer_v1_add_listener( s_wl_confined_pointer, &confined_pointer_listener,
                                              NULL );

        wl_display_roundtrip( wldisp );
    }
    else if( wxGetEnv( wxT( "WAYLAND_DISPLAY" ), nullptr ) )
    {
        // Not working under XWayland
        return false;
    }

    return true;
};


void KIPLATFORM::UI::InfiniteDragReleaseWindow()
{
    wxLogTrace( traceWayland, wxS( "InfiniteDragReleaseWindow" ) );

    if( s_wl_confined_pointer )
    {
        zwp_confined_pointer_v1_destroy( s_wl_confined_pointer );
        s_wl_confined_pointer = NULL;
    }

    if( s_wl_confinement_region )
    {
        wl_region_destroy( s_wl_confinement_region );
        s_wl_confinement_region = NULL;
    }
};


#else // No Wayland support


bool KIPLATFORM::UI::InfiniteDragPrepareWindow( wxWindow* aWindow )
{
    // Not working under XWayland
    return !wxGetEnv( wxT( "WAYLAND_DISPLAY" ), nullptr );
};


void KIPLATFORM::UI::InfiniteDragReleaseWindow()
{
    // Not needed on X11
};


wxPoint KIPLATFORM::UI::GetMousePosition()
{
    return wxGetMousePosition();
}


#endif


void KIPLATFORM::UI::GetNativeWindowHandles( wxWindow* aWindow, void** aHandle, void** aDisplay,
                                              int* aDisplayType )
{
    *aHandle = nullptr;
    *aDisplay = nullptr;

    if( aDisplayType )
        *aDisplayType = NATIVE_DISPLAY_TYPE::DISPLAY_UNKNOWN;

    if( !aWindow )
        return;

    GtkWidget* gtkWidget = static_cast<GtkWidget*>( aWindow->GetHandle() );

    if( !gtkWidget )
        return;

    gtk_widget_realize( gtkWidget );
    GdkWindow* gdkWindow = gtk_widget_get_window( gtkWidget );

    if( !gdkWindow )
        return;

    if( !gdk_window_ensure_native( gdkWindow ) )
    {
        wxLogTrace( traceRhi, wxS( "GetNativeWindowHandles: gdk_window_ensure_native failed "
                                    "widget=%p window=%p" ),
                    static_cast<void*>( gtkWidget ),
                    static_cast<void*>( gdkWindow ) );
        return;
    }

    if( !gdk_window_is_viewable( gdkWindow ) )
    {
        // Hide() unmaps the GDK window, and mapping it anyway would put a hidden preview canvas back
        // on screen over its replacement.
        if( !gtk_widget_get_visible( gtkWidget ) )
        {
            wxLogTrace( traceRhi, wxS( "GetNativeWindowHandles: widget hidden, skipping "
                                        "native window map widget=%p window=%p" ),
                        static_cast<void*>( gtkWidget ),
                        static_cast<void*>( gdkWindow ) );
            return;
        }

        wxLogTrace( traceRhi, wxS( "GetNativeWindowHandles: mapping native window "
                                    "widget=%p window=%p mapped=%s viewable=%s" ),
                    static_cast<void*>( gtkWidget ),
                    static_cast<void*>( gdkWindow ),
                    gtk_widget_get_mapped( gtkWidget ) ? "yes" : "no",
                    gdk_window_is_viewable( gdkWindow ) ? "yes" : "no" );

        gdk_window_show_unraised( gdkWindow );

        wxLogTrace( traceRhi, wxS( "GetNativeWindowHandles: native window map result "
                                    "window=%p viewable=%s" ),
                    static_cast<void*>( gdkWindow ),
                    gdk_window_is_viewable( gdkWindow ) ? "yes" : "no" );
    }

    GdkDisplay* gdkDisplay = gtk_widget_get_display( gtkWidget );

    // The render thread uses its own display connection and must see the window before creating a surface
    gdk_display_flush( gdkDisplay );

#ifdef GDK_WINDOWING_X11
    if( GDK_IS_X11_DISPLAY( gdkDisplay ) )
    {
        *aHandle = reinterpret_cast<void*>( GDK_WINDOW_XID( gdkWindow ) );
        *aDisplay = gdk_x11_display_get_xdisplay( gdkDisplay );

        if( aDisplayType )
            *aDisplayType = NATIVE_DISPLAY_TYPE::DISPLAY_X11;

        wxLogTrace( traceRhi, wxS( "GetNativeWindowHandles: X11 widget=%p window=%p xid=0x%lx "
                                    "display=%p" ),
                    static_cast<void*>( gtkWidget ),
                    static_cast<void*>( gdkWindow ),
                    static_cast<unsigned long>( GDK_WINDOW_XID( gdkWindow ) ),
                    *aDisplay );
        return;
    }
#endif

#ifdef GDK_WINDOWING_WAYLAND
    if( GDK_IS_WAYLAND_DISPLAY( gdkDisplay ) )
    {
        *aHandle = gdk_wayland_window_get_wl_surface( gdkWindow );
        *aDisplay = gdk_wayland_display_get_wl_display( gdkDisplay );

        if( aDisplayType )
            *aDisplayType = NATIVE_DISPLAY_TYPE::DISPLAY_WAYLAND;

        wxLogTrace( traceRhi, wxS( "GetNativeWindowHandles: Wayland widget=%p window=%p "
                                    "surface=%p display=%p" ),
                    static_cast<void*>( gtkWidget ),
                    static_cast<void*>( gdkWindow ),
                    *aHandle,
                    *aDisplay );
        return;
    }
#endif
}


void KIPLATFORM::UI::PrepareForExternalRendering( wxWindow* aWindow )
{
    if( !aWindow )
        return;

    GtkWidget* gtkWidget = static_cast<GtkWidget*>( aWindow->GetHandle() );

    if( !gtkWidget )
    {
        wxLogTrace( traceRhi, wxS( "PrepareForExternalRendering: no GtkWidget handle" ) );
        return;
    }

    bool realized = gtk_widget_get_realized( gtkWidget );

    wxLogTrace( traceRhi, wxS( "PrepareForExternalRendering: widget=%p realized=%s" ),
                static_cast<void*>( gtkWidget ), realized ? "yes" : "no" );

    // Do not realize here. Before layout that creates a 1x1 X11 window and poisons Vulkan's surface
    // capabilities, so GetNativeWindowHandles() realizes it on the first paint instead.

    // GTK3's cairo backing surface would otherwise blit over the GPU output after every draw
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_widget_set_double_buffered( gtkWidget, FALSE );
    G_GNUC_END_IGNORE_DEPRECATIONS

    // Otherwise GTK clears to the GDK background before every draw and flashes black between presents
    gtk_widget_set_app_paintable( gtkWidget, TRUE );

    wxLogTrace( traceRhi, wxS( "PrepareForExternalRendering: double-buffering disabled, "
                                "app-paintable enabled" ) );

    // Keep areas not yet presented from showing the GTK theme. This needs a realized widget.
    if( realized )
    {
        GdkWindow* gdkWindow = gtk_widget_get_window( gtkWidget );

        if( gdkWindow )
        {
#ifdef GDK_WINDOWING_X11
            if( GDK_IS_X11_DISPLAY( gdk_display_get_default() ) )
            {
                // The X server repaints exposed regions with the BackgroundPixmap before the
                // client presents, flashing black; None leaves the previous frame in place.
                Display* xDisplay = GDK_DISPLAY_XDISPLAY( gdk_display_get_default() );
                Window   xWindow  = GDK_WINDOW_XID( gdkWindow );
                XSetWindowBackgroundPixmap( xDisplay, xWindow, None );

                wxLogTrace( traceRhi, wxS( "PrepareForExternalRendering: X11 "
                                            "BackgroundPixmap set to None" ) );
            }
            else
#endif
            {
                GdkRGBA black = { 0.0, 0.0, 0.0, 1.0 };

                G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                gdk_window_set_background_rgba( gdkWindow, &black );
                G_GNUC_END_IGNORE_DEPRECATIONS

                wxLogTrace( traceRhi, wxS( "PrepareForExternalRendering: GDK background "
                                            "set to black" ) );
            }
        }
        else
        {
            wxLogTrace( traceRhi, wxS( "PrepareForExternalRendering: no GdkWindow" ) );
        }

    }

    // Children shown earlier have been painting into the window the GPU now presents to
    for( wxWindow* child : aWindow->GetChildren() )
        KeepAboveExternalRendering( child );
}


void KIPLATFORM::UI::KeepAboveExternalRendering( wxWindow* aChild )
{
#ifdef GDK_WINDOWING_X11
    if( !aChild || !aChild->GetParent() )
        return;

    GtkWidget* child = static_cast<GtkWidget*>( aChild->GetHandle() );
    GtkWidget* parent = static_cast<GtkWidget*>( aChild->GetParent()->GetHandle() );

    if( !child || !parent || !GDK_IS_X11_DISPLAY( gtk_widget_get_display( child ) ) )
        return;

    // PrepareForExternalRendering() is what turns double buffering off
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    const bool external = !gtk_widget_get_double_buffered( parent );
    G_GNUC_END_IGNORE_DEPRECATIONS

    if( !external )
        return;

    GdkWindow* window = gtk_widget_get_window( child );

    // A widget without a window of its own reports its parent's
    if( !window || window == gtk_widget_get_window( parent ) )
        return;

    if( !gdk_window_ensure_native( window ) )
        wxLogTrace( traceRhi, wxS( "KeepAboveExternalRendering: gdk_window_ensure_native failed" ) );
#endif
}
