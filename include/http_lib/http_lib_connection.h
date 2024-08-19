/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2023 Andre F. K. Iwers <iwers11@gmail.com>
 * Copyright (C) redesign and expansion with version 2, 2024 Rosy <rosy@rosy-logic.ch>
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

#ifndef KICAD_HTTP_LIB_CONNECTION_H
#define KICAD_HTTP_LIB_CONNECTION_H

#include <any>
#include <boost/algorithm/string.hpp>

#include "http_lib/http_lib_settings.h"
#include <kicad_curl/kicad_curl_easy.h>


const char* const traceHTTPLib = "KICAD_HTTP_LIB";

struct HTTP_LIB_FIELD
{
    HTTP_LIB_FIELD() {}
    HTTP_LIB_FIELD( const std::string& value ) { Value = value; }
    HTTP_LIB_FIELD( const bool show ) { Show = show; }
    HTTP_LIB_FIELD( const std::string& value, const bool show )
    {
        Value = value;
        Show = show;
    }
    HTTP_LIB_FIELD( const std::string& value, const bool show, const bool showName )
    {
        Value = value;
        Show = show;
        ShowName = showName;
    }

    std::string Value;
    bool        Show = false;
    bool        ShowName = false;
};

struct HTTP_LIB_PART
{
    HTTP_LIB_PART() {}
    HTTP_LIB_PART( const std::string& id ) { Id = id; }

    std::string Id;
    std::string Name;
    std::string CategoryId;
    std::string Symbol;
    std::string Keywords;

    HTTP_LIB_FIELD Reference = HTTP_LIB_FIELD( "#", true );
    HTTP_LIB_FIELD Value = HTTP_LIB_FIELD( true );
    HTTP_LIB_FIELD Footprint;
    HTTP_LIB_FIELD Datasheet;
    HTTP_LIB_FIELD Description;

    std::vector<std::pair<std::string, HTTP_LIB_FIELD>> Fields;

    bool Active = true;
    bool ExcludeFromBom = false;
    bool ExcludeFromBoard = false;
    bool ExcludeFromSim = false;
    bool PowerSymbol = false;

    bool IsUpdated = true;
    bool IsSymbolUpdated = true;
};

class HTTP_LIB_CONNECTION
{
public:
    virtual bool GetPartNames( std::vector<std::string>& aPartNames,
                               const bool                powerSymbolsOnly ) = 0;

    virtual std::vector<HTTP_LIB_PART*>* GetParts( const bool powerSymbolsOnly ) = 0;

    virtual HTTP_LIB_PART* GetPart( const std::string& aPartName, const bool powerSymbolsOnly ) = 0;

    virtual bool GetCategoryNames( std::vector<std::string>& aCategories ) = 0;

    virtual bool GetCategoryName( std::string& aCategoryName, const std::string& aCategoryId ) = 0;

    virtual bool GetCategoryDescription( std::string&       aCategoryDescription,
                                         const std::string& aCategoryName ) = 0;

    bool IsValidEndpoint( const HTTP_LIB_SOURCE& aSource ) const
    {
        if( aSource.api_version != m_source.api_version )
            return false;
        if( aSource.root_url != m_source.root_url )
            return false;
        if( aSource.token != m_source.token )
            return false;
        if( aSource.timeout_cache != m_source.timeout_cache )
            return false;
        if( aSource.timeout_categories != m_source.timeout_categories )
            return false;
        if( aSource.timeout_parts != m_source.timeout_parts )
            return false;
        return m_endpointValid;
    }

    std::string GetLastError() const { return m_lastError; }

    virtual ~HTTP_LIB_CONNECTION() {}

protected:
    HTTP_LIB_CONNECTION( const HTTP_LIB_SOURCE& aSource ) { m_source = aSource; }

    std::unique_ptr<KICAD_CURL_EASY> createCurlEasyObject();

    bool checkServerResponse( std::unique_ptr<KICAD_CURL_EASY>& aCurl );

    wxString httpErrorCodeDescription( uint16_t aHttpCode );

    HTTP_LIB_SOURCE m_source;
    bool            m_endpointValid = false;
    std::string     m_lastError;
};

#endif //KICAD_HTTP_LIB_CONNECTION_H
