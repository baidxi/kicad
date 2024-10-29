/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _GECKO_KEYS_H_
#define _GECKO_KEYS_H_

#include <stdint.h>
#include <string>

namespace GeckoKeys
{

#define NS_DEFINE_PHYSICAL_KEY_CODE_NAME( aCPPName, aDOMCodeName ) CODE_NAME_INDEX_##aCPPName,

typedef uint8_t CodeNameIndexType;
enum CodeNameIndex : CodeNameIndexType
{
#include "PhysicalKeyCodeNameList.h"
    // If a DOM keyboard event is synthesized by script, this is used.  Then,
    // specified code name should be stored and use it as .code value.
    CODE_NAME_INDEX_USE_STRING
};

#undef NS_DEFINE_PHYSICAL_KEY_CODE_NAME


unsigned int  ScanCodeFromKeyEvent( uint32_t aRawKeyCode, uint32_t aRawKeyFlags );
CodeNameIndex CodeNameIndexFromScanCode( unsigned int aScanCode );
int           WXKFromCodeNameIndex( CodeNameIndex aCodeNameIndex );
CodeNameIndex CodeNameIndexFromWXK( int aWXKey );
uint32_t      ScancodeFromCodeNameIndex( CodeNameIndex aCodeNameIndex );
int           WXKFromKeyEvent( uint32_t aRawKeyCode, uint32_t aRawKeyFlags );
wchar_t       CharFromScancode( uint32_t aScancode );

} // namespace GeckoKeys

#endif // _GECKO_KEYS_H_
