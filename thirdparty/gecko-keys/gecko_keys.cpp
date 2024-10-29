/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gecko_keys.h>
#include <wx/defs.h>
#include <wx/time.h>

#if defined( _WIN32 )
#include <Windows.h>
#elif defined(__WXGTK__)
#include <gdk/gdk.h>
#include <x11/keysym2ucs.h>

#if 0
#include <gdk/gdkx.h>
#include <X11/XKBlib.h>
#endif

#endif


namespace GeckoKeys
{

unsigned int ScanCodeFromKeyEvent( uint32_t aRawKeyCode, uint32_t aRawKeyFlags )
{
    printf( "\n" );
    printf( "%lu ScanCodeFromKeyEvent rawKeyCode %08x rawKeyFlags %08x\n", wxGetLocalTime(), aRawKeyCode,
            aRawKeyFlags );

#if defined( _WIN32 )

    uint16_t scanCode = ( aRawKeyFlags >> 16 ) & 0xFF;

    if( ( aRawKeyFlags & 0x1000000 ) != 0 ) // Extended
    {
        scanCode |= 0xE000;
    }

    printf( "%lu  ScanCodeFromKeyEvent return scanCode %08x\n", wxGetLocalTime(), scanCode );

    return scanCode;
#elif defined( __APPLE__ )
    return aRawKeyCode;
#else
    return aRawKeyFlags;
#endif
}


CodeNameIndex CodeNameIndexFromScanCode( unsigned int aScanCode )
{
    printf( "%lu CodeNameIndexFromScanCode aScanCode %08x\n", wxGetLocalTime(), aScanCode );

    // clang-format off
    switch (aScanCode) {
#define NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX(aNativeKey, aCodeNameIndex) \
    case aNativeKey:                                                     \
      return aCodeNameIndex;

#include "NativeKeyToDOMCodeName.h"

#undef NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX

      default:
          break;
    }
    // clang-format on

    return CODE_NAME_INDEX_UNKNOWN;
}


int WXKFromCodeNameIndex( CodeNameIndex aCodeNameIndex )
{
    printf( "%lu WXKFromCodeNameIndex aCodeNameIndex %d\n", wxGetLocalTime(),
            (int) aCodeNameIndex );

    // clang-format off
    switch (aCodeNameIndex) {
#define NS_WX_KEY_TO_DOM_CODE_NAME_INDEX(aWXKey, aCPPCodeName) \
    case aCPPCodeName:                                         \
      return aWXKey;

#include "NativeKeyToDOMCodeName.h"

#undef NS_WX_KEY_TO_DOM_CODE_NAME_INDEX

      default:
          break;
    }
    // clang-format on

    return WXK_NONE;
}


CodeNameIndex CodeNameIndexFromWXK( int aWXKey )
{
    printf( "%lu CodeNameIndexFromWXK aWXKey %d\n", wxGetLocalTime(), aWXKey );

    // clang-format off
    switch (aWXKey) {
#define NS_WX_KEY_TO_DOM_CODE_NAME_INDEX(aWXKey, aCPPCodeName) \
    case aWXKey:                                               \
      return aCPPCodeName;

#include "NativeKeyToDOMCodeName.h"

#undef NS_WX_KEY_TO_DOM_CODE_NAME_INDEX

      default:
          break;
    }
    // clang-format on

    return CODE_NAME_INDEX_UNKNOWN;
}


uint32_t ScancodeFromCodeNameIndex( CodeNameIndex aCodeNameIndex )
{
    printf( "%lu ScancodeFromCodeNameIndex aCodeNameIndex %d\n", wxGetLocalTime(),
            (int) aCodeNameIndex );

    // clang-format off
#define NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX(aNativeKey, aCodeNameIndexIt) \
    if( aCodeNameIndex == aCodeNameIndexIt) return aNativeKey;

#include "NativeKeyToDOMCodeName.h"

#undef NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX
    // clang-format on

    return WXK_NONE;
}


int WXKFromKeyEvent( uint32_t aRawKeyCode, uint32_t aRawKeyFlags )
{
    printf( "\n" );
    printf( "%lu WXKFromKeyEvent rawKeyCode %08x rawKeyFlags %08x\n", wxGetLocalTime(), aRawKeyCode,
            aRawKeyFlags );

    return WXKFromCodeNameIndex(
            CodeNameIndexFromScanCode( ScanCodeFromKeyEvent( aRawKeyCode, aRawKeyFlags ) ) );
}


int WXKFromScancode( int aScancode )
{
    printf( "%lu WXKFromScancode aScanCode %08x\n", wxGetLocalTime(), aScancode );

    return WXKFromCodeNameIndex( CodeNameIndexFromScanCode( aScancode ) );
}


wchar_t CharFromScancode( uint32_t aScancode )
{
    printf( "%lu CharFromScancode aScanCode %08x\n", wxGetLocalTime(), aScancode );

#if defined( _WIN32 )
    HKL layout = ::GetKeyboardLayout( 0 );
    uint32_t vk = ::MapVirtualKeyExW( aScancode, MAPVK_VSC_TO_VK, layout );
    wchar_t ch = ::MapVirtualKeyExW( vk, MAPVK_VK_TO_CHAR, layout );

    printf( "%lu  CharFromScancode layout %016x vk %08x ch %08x\n", wxGetLocalTime(), layout, vk,
            ch );

    return ch;
#elif defined( __APPLE__ )
    const UCKeyboardLayout* UCKey = GetUCKeyboardLayout();
    UInt32                  kbType = ::LMGetKbdType();

    UInt32       modifiers = 0;
    UInt32       deadKeyState = 0;
    UniCharCount len = 0;
    UniChar      chars[5] = { 0 };
    OSStatus     err = ::UCKeyTranslate( UCKey, aScancode, kUCKeyActionDown, modifiers >> 8, kbType,
                                         kUCKeyTranslateNoDeadKeysMask, &deadKeyState, 5, &len, chars );

    return chars[0];
#else
    GdkDisplay* disp = gdk_display_get_default();
    GdkKeymap* keymap = gdk_keymap_get_for_display( disp );

    GdkKeymapKey* keys = nullptr;
    gint          count = 0;
    gint          minGroup = -1;
    if( gdk_keymap_get_entries_for_keyval( keymap, 0x061 /* GDK_a */, &keys, &count ) )
    {
        // find the minimum number group for latin inputtable layout
        for( gint i = 0; i < count && minGroup != 0; ++i )
        {
            if( keys[i].level != 0 && keys[i].level != 1 )
            {
                continue;
            }
            if( minGroup >= 0 && keys[i].group > minGroup )
            {
                continue;
            }
            minGroup = keys[i].group;
        }
        g_free( keys );
    }

    if( minGroup == -1 )
        return 0;

    guint state = 0;
    guint keyval = 0;

    gdk_keymap_translate_keyboard_state( keymap, aScancode, GdkModifierType( state ), minGroup,
                                         &keyval, nullptr, nullptr, nullptr );

#if 0
    Display* display_x11 = gdk_x11_display_get_xdisplay( GDK_X11_DISPLAY( disp ) );

    if( display_x11 )
    {
        const KeySym keysym = XkbKeycodeToKeysym( display_x11, aScancode, 0, 0 );

        if( keysym == NoSymbol )
            return 0;

        static const long MAX_UNICODE = 0x10FFFF;

        // we're supposedly printable, let's try to convert
        long ucs = keysym2ucs( keysym );
        if( ( ucs != -1 ) && ( ucs < MAX_UNICODE ) )
        {
            printf( "%lu  CharFromScancode keymap %p keysym %08x ucs %08lx\n", wxGetLocalTime(),
                    keymap, keysym, ucs );

            return ucs;
        }
    }
#endif

    static const long MAX_UNICODE = 0x10FFFF;

    // we're supposedly printable, let's try to convert
    long ucs = keysym2ucs( keyval );
    if( ( ucs != -1 ) && ( ucs < MAX_UNICODE ) )
    {
        printf( "%lu  CharFromScancode keymap %p keyval %08x ucs %08lx\n", wxGetLocalTime(), keymap,
                keyval, ucs );

        return ucs;
    }

    // I guess we couldn't convert
    return 0;
#endif
}

}
