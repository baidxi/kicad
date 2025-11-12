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
 */

#include "plm_kicad_pro_metadata.h"

#include <nlohmann/json.hpp>
#include <wx/file.h>


std::optional<PLM_KICAD_PRO_METADATA> ReadKicadProMetadata( const wxString& aPath )
{
    if( aPath.IsEmpty() )
        return std::nullopt;

    wxFile file( aPath, wxFile::read );

    if( !file.IsOpened() )
        return std::nullopt;

    // Read raw bytes rather than going through wxString so the JSON parser
    // receives UTF-8 unconditionally. wxFile::ReadAll(wxString*) and
    // wxString::ToStdString() both apply the current locale's conversion,
    // which corrupts non-ASCII content under the C locale.
    wxFileOffset size = file.Length();

    if( size < 0 )
        return std::nullopt;

    std::string bytes;
    bytes.resize( static_cast<size_t>( size ) );

    if( size > 0 )
    {
        ssize_t read = file.Read( bytes.data(), bytes.size() );

        if( read < 0 || static_cast<size_t>( read ) != bytes.size() )
            return std::nullopt;
    }

    file.Close();

    nlohmann::json doc;

    try
    {
        doc = nlohmann::json::parse( bytes );
    }
    catch( const nlohmann::json::exception& )
    {
        return std::nullopt;
    }

    PLM_KICAD_PRO_METADATA meta;

    if( doc.contains( "meta" ) && doc["meta"].is_object() )
    {
        const auto& metaJson = doc["meta"];

        if( metaJson.contains( "filename" ) && metaJson["filename"].is_string() )
            meta.filename = wxString::FromUTF8( metaJson["filename"].get<std::string>() );

        if( metaJson.contains( "version" ) && metaJson["version"].is_number_integer() )
            meta.version = metaJson["version"].get<int>();
    }

    if( doc.contains( "sheets" ) && doc["sheets"].is_array() )
        meta.sheet_count = static_cast<int>( doc["sheets"].size() );

    if( doc.contains( "text_variables" ) && doc["text_variables"].is_object() )
    {
        for( auto it = doc["text_variables"].begin(); it != doc["text_variables"].end(); ++it )
        {
            if( !it.value().is_string() )
                continue;

            wxString key = wxString::FromUTF8( it.key() );
            wxString val = wxString::FromUTF8( it.value().get<std::string>() );
            meta.text_variables.emplace( key, val );
        }
    }

    return meta;
}
