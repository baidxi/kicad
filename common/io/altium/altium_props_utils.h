/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2019-2020 Thomas Pointhuber <thomas.pointhuber@gmx.at>
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _ALTIUM_PROPS_UTILS_H
#define _ALTIUM_PROPS_UTILS_H

#include <stdint.h>
#include <map>

#include <wx/string.h>


class ALTIUM_PROPS_UTILS
{
public:
    static int32_t ConvertToKicadUnit( const double aValue );

    static int ReadInt( const std::map<wxString, wxString>& aProps, const wxString& aKey,
                        int aDefault );

    static double ReadDouble( const std::map<wxString, wxString>& aProps, const wxString& aKey,
                              double aDefault );

    static bool ReadBool( const std::map<wxString, wxString>& aProps, const wxString& aKey,
                          bool aDefault );

    static int32_t ReadKicadUnit( const std::map<wxString, wxString>& aProps, const wxString& aKey,
                                  const wxString& aDefault );

    static wxString ReadString( const std::map<wxString, wxString>& aProps, const wxString& aKey,
                                const wxString& aDefault );

    static wxString ReadUnicodeString( const std::map<wxString, wxString>& aProps,
                                       const wxString& aKey, const wxString& aDefault );

    /**
     * Recover a string that is UTF-8 which has been erroneously decoded as Latin-1 (classic
     * "mojibake", e.g. "µ" shown as "Âµ").  Some Altium files store such double-encoded values,
     * including in the '%UTF8%'-prefixed twins that are otherwise assumed to be correct UTF-8.
     *
     * The repair is deliberately conservative: it only rewrites @p aString when re-encoding it to
     * Latin-1 yields a byte sequence that is *exactly* valid UTF-8 and decodes to a different
     * string.  Correctly-decoded values (whose Latin-1 bytes are not valid UTF-8) are returned
     * unchanged, so this never corrupts already-correct text.
     */
    static wxString RepairDoubleEncodedUtf8( const wxString& aString );
};

#endif //_ALTIUM_PROPS_UTILS_H
