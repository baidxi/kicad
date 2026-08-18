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

#include "altium_props_utils.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <math/util.h>

#include <wx/crt.h>
#include <wx/log.h>
#include <wx/translation.h>


int32_t ALTIUM_PROPS_UTILS::ConvertToKicadUnit( const double aValue )
{
    constexpr double int_limit = ( std::numeric_limits<int>::max() - 10 ) / 2.54;

    int32_t iu = KiROUND( std::clamp( aValue, -int_limit, int_limit ) * 2.54 );

    // Altium's internal precision is 0.1uinch.  KiCad's is 1nm.  Round to nearest 10nm to clean
    // up most rounding errors.  This allows lossless conversion of increments of 0.05mils and
    // 0.01um.
    return KiROUND( (double) iu / 10.0 ) * 10;
}


int ALTIUM_PROPS_UTILS::ReadInt( const std::map<wxString, wxString>& aProps, const wxString& aKey,
                            int aDefault )
{
    const std::map<wxString, wxString>::const_iterator& value = aProps.find( aKey );

    if( value == aProps.end() )
        return aDefault;

    long parsed;

    if( !value->second.ToLong( &parsed ) || parsed < std::numeric_limits<int>::min()
        || parsed > std::numeric_limits<int>::max() )
    {
        return aDefault;
    }

    return static_cast<int>( parsed );
}


double ALTIUM_PROPS_UTILS::ReadDouble( const std::map<wxString, wxString>& aProps,
                                       const wxString& aKey, double aDefault )
{
    const std::map<wxString, wxString>::const_iterator& value = aProps.find( aKey );

    if( value == aProps.end() )
        return aDefault;

    // Locale independent str -> double conversation
    std::istringstream istr( (const char*) value->second.mb_str() );
    istr.imbue( std::locale::classic() );

    double doubleValue = aDefault;
    istr >> doubleValue;

    if( istr.fail() || !std::isfinite( doubleValue ) )
        return aDefault;

    istr >> std::ws;

    if( istr.peek() != std::char_traits<char>::eof() )
        return aDefault;

    return doubleValue;
}


bool ALTIUM_PROPS_UTILS::ReadBool( const std::map<wxString, wxString>& aProps, const wxString& aKey,
                                   bool aDefault )
{
    const std::map<wxString, wxString>::const_iterator& value = aProps.find( aKey );

    if( value == aProps.end() )
        return aDefault;
    if( value->second.IsSameAs( wxS( "T" ), false ) || value->second.IsSameAs( wxS( "TRUE" ), false ) )
    {
        return true;
    }

    if( value->second.IsSameAs( wxS( "F" ), false ) || value->second.IsSameAs( wxS( "FALSE" ), false ) )
    {
        return false;
    }

    return aDefault;
}


int32_t ALTIUM_PROPS_UTILS::ReadKicadUnit( const std::map<wxString, wxString>& aProps,
                                           const wxString& aKey, const wxString& aDefault )
{
    wxString value = ReadString( aProps, aKey, aDefault );
    wxString prefix;
    double   mils;

    auto parse = [&]( const wxString& aValue )
    {
        if( !aValue.EndsWith( "mil", &prefix ) )
            return false;

        prefix.StartsWith( "+", &prefix );
        return prefix.ToCDouble( &mils ) && std::isfinite( mils );
    };

    if( !parse( value ) )
    {
        wxLogTrace( "ALTIUM", wxT( "Cannot convert '%s' to a distance." ), value );

        if( !parse( aDefault ) )
            return 0;
    }

    return ConvertToKicadUnit( mils * 10000 );
}


wxString ALTIUM_PROPS_UTILS::RepairDoubleEncodedUtf8( const wxString& aString )
{
    if( aString.IsEmpty() )
        return aString;

    // Only a string whose codepoints all fit in a single byte can be a Latin-1 misreading of raw
    // UTF-8 bytes; anything outside that range (real Unicode) cannot be Latin-1 encoded and is
    // therefore already correct.
    wxScopedCharBuffer latin1 = aString.mb_str( wxConvISO8859_1 );

    if( !latin1 || latin1.length() == 0 )
        return aString;

    // Re-interpret those bytes as UTF-8.  A failed decode yields an empty string.
    wxString decoded( latin1.data(), wxConvUTF8, latin1.length() );

    if( decoded.IsEmpty() || decoded == aString )
        return aString;

    // Only accept the repair when the bytes were *exactly* valid UTF-8, i.e. re-encoding the
    // decoded text reproduces the original byte sequence.  This rejects lossy/partial decodes and
    // guarantees correctly-decoded values (e.g. a lone 0xB5 "µ") are never altered.
    wxScopedCharBuffer roundtrip = decoded.utf8_str();

    if( roundtrip.length() == latin1.length() && std::memcmp( roundtrip.data(), latin1.data(), latin1.length() ) == 0 )
    {
        return decoded;
    }

    return aString;
}


wxString ALTIUM_PROPS_UTILS::ReadString( const std::map<wxString, wxString>& aProps,
                                         const wxString& aKey, const wxString& aDefault )
{
    // The double-encoding repair is applied to BOTH the '%UTF8%' twin and the plain value on
    // purpose: real Altium files corrupt both forms (e.g. a plain ComponentDescription whose bytes
    // are already UTF-8, and a '%UTF8%' twin that has been UTF-8-encoded a second time).  The
    // repair's exact round-trip guard means a genuine Latin-1 value is only rewritten when its
    // bytes happen to form valid UTF-8 — a case that is, in practice, itself mojibake.
    const auto& utf8Value = aProps.find( wxString( "%UTF8%" ) + aKey );

    if( utf8Value != aProps.end() )
        return RepairDoubleEncodedUtf8( utf8Value->second );

    const auto& value = aProps.find( aKey );

    if( value != aProps.end() )
        return RepairDoubleEncodedUtf8( value->second );

    return aDefault;
}


wxString ALTIUM_PROPS_UTILS::ReadUnicodeString( const std::map<wxString, wxString>& aProps,
                                                const wxString& aKey, const wxString& aDefault )
{
    const auto& unicodeFlag = aProps.find( wxS( "UNICODE" ) );

    if( unicodeFlag != aProps.end() && unicodeFlag->second.Contains( wxS( "EXISTS" ) ) )
    {
        const auto& unicodeValue = aProps.find( wxString( "UNICODE__" ) + aKey );

        if( unicodeValue != aProps.end() )
        {
            wxArrayString arr = wxSplit( unicodeValue->second, ',', '\0' );
            wxString      out;

            for( wxString part : arr )
                out += wxString( wchar_t( wxAtoi( part ) ) );

            return out;
        }
    }

    return ReadString( aProps, aKey, aDefault );
}
