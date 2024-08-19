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

#include <wx/log.h>
#include <fmt/core.h>
#include <wx/translation.h>
#include <ctime>

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>
#include <wx/base64.h>

#include <kicad_curl/kicad_curl_easy.h>
#include <curl/curl.h>

#include <http_lib/http_lib_connection.h>
#include <http_lib/http_lib_v1_connection.h>

HTTP_LIB_V1_CONNECTION::HTTP_LIB_V1_CONNECTION( const HTTP_LIB_SOURCE& aSource ) :
        HTTP_LIB_CONNECTION( aSource )
{
    validateHTTPLibraryEndpoints();
    m_endpointValid = syncCategories();
}

bool HTTP_LIB_V1_CONNECTION::GetPartNames( std::vector<std::string>& aPartNames,
                                           const bool                powerSymbolsOnly )
{
    if( !syncParts() )
    {
        return false;
    }

    for( HTTP_LIB_PART* part : m_parts )
    {
        if( !powerSymbolsOnly || part->PowerSymbol )
        {
            aPartNames.push_back( part->Name );
        }
    }
    return true;
}

std::vector<HTTP_LIB_PART*>* HTTP_LIB_V1_CONNECTION::GetParts( const bool powerSymbolsOnly )
{
    if( !syncParts() )
    {
        return nullptr;
    }
    return &m_parts;
}

HTTP_LIB_PART* HTTP_LIB_V1_CONNECTION::GetPart( const std::string& aPartName,
                                                const bool         powerSymbolsOnly )
{
    if( !syncParts() )
    {
        return nullptr;
    }

    if( !m_partsByName.contains( aPartName ) )
    {
        m_lastError += wxString::Format( _( "GetPart: Part not found: '%s'" ) + "\n", aPartName );
        return nullptr;
    }

    HTTP_LIB_V1_PART* part = m_partsByName.at( aPartName );

    if( std::difftime( std::time( nullptr ), part->LastCached ) > m_source.timeout_parts )
    {
        if( !cacheUpdateOne( part ) )
        {
            return nullptr;
        }
    }

    if( powerSymbolsOnly && !part->PowerSymbol )
    {
        return nullptr;
    }

    return part;
}

bool HTTP_LIB_V1_CONNECTION::GetCategoryNames( std::vector<std::string>& aCategories )
{
    for( std::map<std::string, HTTP_LIB_V1_CATEGORY*>::iterator it = m_categoriesByName.begin();
         it != m_categoriesByName.end(); ++it )
    {
        aCategories.push_back( it->first );
    }
    return true;
}

bool HTTP_LIB_V1_CONNECTION::GetCategoryName( std::string&       aCategoryName,
                                              const std::string& aCategoryId )
{
    if( m_categoriesById.contains( aCategoryId ) )
    {
        aCategoryName = m_categoriesById.at( aCategoryId )->Name;
        return true;
    }
    m_lastError += wxString::Format( _( "GetCategoryName: Category not found: '%s'" ) + "\n",
                                     aCategoryId );
    return false;
}

bool HTTP_LIB_V1_CONNECTION::GetCategoryDescription( std::string&       aCategoryDescription,
                                                     const std::string& aCategoryName )
{
    if( m_categoriesByName.contains( aCategoryName ) )
    {
        aCategoryDescription = m_categoriesByName.at( aCategoryName )->Description;
        return true;
    }

    m_lastError += wxString::Format( _( "GetCategoryDescription: Category not found: '%s'" ) + "\n",
                                     aCategoryName );
    return false;
}

bool HTTP_LIB_V1_CONNECTION::validateHTTPLibraryEndpoints()
{
    m_endpointValid = false;
    std::string res = "";

    std::unique_ptr<KICAD_CURL_EASY> curl = createCurlEasyObject();
    curl->SetURL( m_source.root_url );

    try
    {
        curl->Perform();

        res = curl->GetBuffer();

        if( !checkServerResponse( curl ) )
            return false;

        if( res.length() == 0 )
        {
            m_lastError += wxString::Format( _( "KiCad received an empty response!" ) + "\n" );
        }
        else
        {
            nlohmann::json response = nlohmann::json::parse( res );

            // Check that the endpoints exist, if not fail.
            if( !response.at( http_endpoint_categories ).empty()
                && !response.at( http_endpoint_parts ).empty() )
            {
                m_endpointValid = true;
            }
        }
    }
    catch( const std::exception& e )
    {
        m_lastError += wxString::Format( _( "Error: %s" ) + "\n" + _( "API Response:  %s" ) + "\n",
                                         e.what(), res );

        wxLogTrace( traceHTTPLib,
                    wxT( "ValidateHTTPLibraryEndpoints: Exception occurred while testing the API "
                         "connection: %s" ),
                    m_lastError );

        m_endpointValid = false;
    }
    return m_endpointValid;
}

bool HTTP_LIB_V1_CONNECTION::syncCategories()
{
    if( !m_endpointValid )
    {
        wxLogTrace( traceHTTPLib, wxT( "syncCategories: without valid connection!" ) );
        return false;
    }

    std::string res = "";

    std::unique_ptr<KICAD_CURL_EASY> curl = createCurlEasyObject();
    curl->SetURL( m_source.root_url + http_endpoint_categories + ".json" );

    try
    {
        curl->Perform();

        res = curl->GetBuffer();

        if( !checkServerResponse( curl ) )
            return false;

        nlohmann::json response = nlohmann::json::parse( res );

        // collect the categories in dictionary
        for( const auto& item : response.items() )
        {
            auto&       value = item.value();
            std::string id = value["id"];

            HTTP_LIB_V1_CATEGORY* category = new HTTP_LIB_V1_CATEGORY( id );

            if( value.contains( "name" ) )
            {
                category->Name = value["name"];
            }
            else
            {
                category->Name = category->Id;
            }

            if( value.contains( "description" ) )
            {
                category->Description = value["description"];
            }

            m_categoriesByName[category->Name] = category;
            m_categoriesById[category->Id] = category;
        }
    }
    catch( const std::exception& e )
    {
        m_lastError += wxString::Format( _( "Error: %s" ) + "\n" + _( "API Response:  %s" ) + "\n",
                                         e.what(), res );

        wxLogTrace( traceHTTPLib,
                    wxT( "syncCategories: Exception occurred while syncing categories: %s" ),
                    m_lastError );

        m_categoriesByName.clear();
        m_categoriesById.clear();
        return false;
    }
    return true;
}

bool HTTP_LIB_V1_CONNECTION::syncParts()
{
    for( std::map<std::string, HTTP_LIB_V1_CATEGORY*>::iterator it = m_categoriesById.begin();
         it != m_categoriesById.end(); ++it )
    {
        if( std::difftime( std::time( nullptr ), it->second->LastCached )
            > m_source.timeout_categories )
        {
            if( !cacheAll( it->second ) )
            {
                return false;
            }
            it->second->LastCached = std::time( nullptr );
        }
    }
    return true;
}

bool HTTP_LIB_V1_CONNECTION::cacheAll( HTTP_LIB_V1_CATEGORY* aCategory )
{
    if( !m_endpointValid )
    {
        wxLogTrace( traceHTTPLib, wxT( "SelectAll: without valid connection!" ) );
        return false;
    }

    std::string res = "";

    std::unique_ptr<KICAD_CURL_EASY> curl = createCurlEasyObject();

    curl->SetURL( m_source.root_url
                  + fmt::format( "{}/category/{}.json", http_endpoint_parts, aCategory->Id ) );

    try
    {
        curl->Perform();

        res = curl->GetBuffer();

        std::list<HTTP_LIB_V1_PART*> parts;

        nlohmann::json response = nlohmann::json::parse( res );

        for( nlohmann::json& item : response )
        {
            std::string id = item.at( "id" );
            std::string name;

            HTTP_LIB_V1_PART* part = m_partsById[id];

            if( !part )
            {
                part = new HTTP_LIB_V1_PART( id );
                m_partsById[part->Id] = part;
                m_parts.push_back( part );
            }

            part->CategoryId = aCategory->Id;
            part->PreloadedDescription = false;
            part->IsUpdated = true;

            // API might not want to return an optional name.
            if( item.contains( "name" ) )
            {
                name = item.at( "name" );
            }
            else
            {
                name = id;
            }

            if( name != part->Name )
            {
                m_partsByName.erase( part->Name );
                part->Name = name;
                m_partsByName[part->Name] = part;
            }

            // API might not want to return an optional description.
            if( item.contains( description_field ) )
            {
                part->Description.Value = item.at( description_field );
                part->PreloadedDescription = true;
            }

            if( item.contains( "power_symbol" ) )
            {
                std::string pow = item.at( "power_symbol" );
                part->PowerSymbol = boolFromString( pow, false );
            }
            aCategory->Parts.remove( part );
            parts.push_back( part );
        }
        for( HTTP_LIB_V1_PART* part : aCategory->Parts )
        {
            m_partsByName.erase( part->Name );
            m_partsById.erase( part->Id );
            m_parts.erase( std::find( m_parts.begin(), m_parts.end(), part ) );
            delete part;
        }
        aCategory->Parts = parts;
    }
    catch( const std::exception& e )
    {
        m_lastError += wxString::Format( _( "Error: %s" ) + "\n" + _( "API Response:  %s" ) + "\n",
                                         e.what(), res );

        wxLogTrace( traceHTTPLib, wxT( "Exception occurred while syncing parts from REST API: %s" ),
                    m_lastError );

        return false;
    }

    return true;
}

bool HTTP_LIB_V1_CONNECTION::cacheUpdateOne( HTTP_LIB_V1_PART* aPart )
{
    if( !m_endpointValid )
    {
        wxLogTrace( traceHTTPLib, wxT( "SelectAll: without valid connection!" ) );
        return false;
    }

    std::string res = "";

    std::unique_ptr<KICAD_CURL_EASY> curl = createCurlEasyObject();
    std::string                      url =
            m_source.root_url + fmt::format( "{}/{}.json", http_endpoint_parts, aPart->Id );
    curl->SetURL( url );

    try
    {
        curl->Perform();

        res = curl->GetBuffer();

        if( !checkServerResponse( curl ) )
        {
            return false;
        }

        nlohmann::ordered_json response = nlohmann::ordered_json::parse( res );
        std::string            key = "";
        std::string            value = "";

        // get a timestamp for caching
        aPart->LastCached = std::time( nullptr );

        std::string symbol = response.at( "symbolIdStr" );

        if( aPart->Symbol != symbol )
        {
            aPart->Symbol = symbol;
            aPart->IsSymbolUpdated = true;
        }

        // initially assume no exclusion
        std::string exclude;

        if( response.contains( "exclude_from_bom" ) )
        {
            // if key value doesn't exists default to false
            exclude = response.at( "exclude_from_bom" );
            aPart->ExcludeFromBom = boolFromString( exclude, false );
        }

        // initially assume no exclusion
        if( response.contains( "exclude_from_board" ) )
        {
            // if key value doesn't exists default to false
            exclude = response.at( "exclude_from_board" );
            aPart->ExcludeFromBoard = boolFromString( exclude, false );
        }

        // initially assume no exclusion
        if( response.contains( "exclude_from_sim" ) )
        {
            // if key value doesn't exists default to false
            exclude = response.at( "exclude_from_sim" );
            aPart->ExcludeFromSim = boolFromString( exclude, false );
        }

        // remove previously loaded fields
        aPart->Fields.clear();

        // Extract available fields
        for( const auto& field : response.at( "fields" ).items() )
        {
            bool visible = true;

            // name of the field
            key = field.key();
            wxString fieldName = wxString( key ).Lower();

            // this is a dict
            auto& properties = field.value();

            value = properties.at( "value" );

            // check if user wants to display field in schematic
            if( properties.contains( "visible" ) )
            {
                std::string vis = properties.at( "visible" );
                visible = boolFromString( vis, true );
            }

            if( fieldName == footprint_field )
            {
                aPart->Footprint.Value = value;
                aPart->Footprint.Show = visible;
            }
            else if( fieldName == description_field )
            {
                if( !aPart->PreloadedDescription )
                {
                    aPart->Description.Value = value;
                }
                aPart->Description.Show = visible;
            }
            else if( fieldName == value_field )
            {
                aPart->Value.Value = value;
                aPart->Value.Show = visible;
            }
            else if( fieldName == datasheet_field )
            {
                aPart->Datasheet.Value = value;
                aPart->Datasheet.Show = visible;
            }
            else if( fieldName == reference_field )
            {
                aPart->Reference.Value = value;
                aPart->Reference.Show = visible;
            }
            else if( fieldName == keywords_field )
            {
                aPart->Keywords = value;
            }
            else
            {
                // Add field to fields list
                aPart->Fields.push_back( std::make_pair( key, HTTP_LIB_FIELD( value, visible ) ) );
            }
        }
        aPart->IsUpdated = true;
    }
    catch( const std::exception& e )
    {
        m_lastError += wxString::Format( _( "Error: %s" ) + "\n" + _( "API Response:  %s" ) + "\n",
                                         e.what(), res );

        wxLogTrace( traceHTTPLib,
                    wxT( "SelectOne: Exception occurred while retrieving part from REST API: %s" ),
                    m_lastError );

        return false;
    }

    return true;
}

bool HTTP_LIB_V1_CONNECTION::boolFromString( const std::any& aVal, bool aDefaultValue )
{
    try
    {
        wxString strval( std::any_cast<std::string>( aVal ).c_str(), wxConvUTF8 );

        if( strval.IsEmpty() )
            return aDefaultValue;

        strval.MakeLower();

        for( const auto& trueVal : { wxS( "true" ), wxS( "yes" ), wxS( "y" ), wxS( "1" ) } )
        {
            if( strval.Matches( trueVal ) )
                return true;
        }

        for( const auto& falseVal : { wxS( "false" ), wxS( "no" ), wxS( "n" ), wxS( "0" ) } )
        {
            if( strval.Matches( falseVal ) )
                return false;
        }
    }
    catch( const std::bad_any_cast& )
    {
    }

    return aDefaultValue;
}