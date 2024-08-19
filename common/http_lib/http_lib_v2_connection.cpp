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
#include <http_lib/http_lib_v2_connection.h>

HTTP_LIB_V2_CONNECTION::HTTP_LIB_V2_CONNECTION( const HTTP_LIB_SOURCE& aSource ) :
        HTTP_LIB_CONNECTION( aSource )
{
    validateEndpoints();
}

bool HTTP_LIB_V2_CONNECTION::GetPartNames( std::vector<std::string>& aPartNames,
                                           const bool                powerSymbolsOnly )
{
    if( !syncCache() )
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
std::vector<HTTP_LIB_PART*>* HTTP_LIB_V2_CONNECTION::GetParts( const bool powerSymbolsOnly )
{
    if( !syncCache() )
    {
        return nullptr;
    }
    return &m_parts;
}

HTTP_LIB_PART* HTTP_LIB_V2_CONNECTION::GetPart( const std::string& aPartName,
                                                const bool         powerSymbolsOnly )
{
    if( !syncCache() )
    {
        return nullptr;
    }

    HTTP_LIB_PART* part = m_partsByName.at( aPartName );
    if( powerSymbolsOnly && !part->PowerSymbol )
    {
        return nullptr;
    }

    return part;
}

bool HTTP_LIB_V2_CONNECTION::GetCategoryNames( std::vector<std::string>& aCategories )
{
    if( !syncCache() )
    {
        return false;
    }

    for( std::map<std::string, HTTP_LIB_V2_CATEGORY*>::iterator it = m_categoriesByName.begin();
         it != m_categoriesByName.end(); ++it )
    {
        aCategories.push_back( it->first );
    }

    return true;
}

bool HTTP_LIB_V2_CONNECTION::GetCategoryName( std::string&       aCategoryName,
                                              const std::string& aCategoryId )
{
    if( m_categoriesById.contains( aCategoryId ) )
    {
        aCategoryName = m_categoriesById.at( aCategoryId )->Name;
        return true;
    }

    m_lastError += fmt::format( "GetCategoryName: Category not found: '{}'\n", aCategoryId );
    return false;
}

bool HTTP_LIB_V2_CONNECTION::GetCategoryDescription( std::string&       aCategoryDescription,
                                                     const std::string& aCategoryName )
{
    if( m_categoriesByName.contains( aCategoryName ) )
    {
        aCategoryDescription = m_categoriesByName.at( aCategoryName )->Description;
        return true;
    }

    m_lastError +=
            fmt::format( "GetCategoryDescription: Category not found: '{}'\n", aCategoryName );
    return false;
}

void HTTP_LIB_V2_CONNECTION::validateEndpoints()
{
    std::string res;
    m_endpointValid = false;

    std::unique_ptr<KICAD_CURL_EASY> curl = createCurlEasyObject();
    curl->SetURL( m_source.root_url );

    try
    {
        curl->Perform();

        res = curl->GetBuffer();

        if( !checkServerResponse( curl ) )
        {
            return;
        }

        if( res.length() == 0 )
        {
            m_lastError = "KiCad received an empty response!\n";
            return;
        }

        nlohmann::json response = nlohmann::json::parse( res );

        if( !response.contains( "version" ) )
        {
            m_lastError = "KiCad received no version from the API server!\n";
            return;
        }

        std::string version = response.at( "version" );

        if( m_supportedApiServerVersions.find( "{" + version + "}" ) == std::string::npos )
        {
            m_lastError = fmt::format(
                    "KiCad does not support the received API server version: '{}'!\n", version );
            return;
        }

        if( response.contains( "content" ) )
        {
            std::string content = response.at( "content" );

            if( !content.empty() )
            {
                size_t pos = content.find( "{}" );
                if( pos == std::string::npos || content.find( "{" ) != pos
                    || content.find( "}" ) != pos + 1
                    || content.find( "{", pos + 2 ) != std::string::npos
                    || content.find( "}", pos + 2 ) != std::string::npos )
                {
                    m_lastError = fmt::format( "KiCad does not support the received API server "
                                               "content endpoint: '{}'!\n",
                                               content );
                    return;
                }
                m_contentEndpoint = content;
            }
        }
        m_endpointValid = true;
    }
    catch( const std::exception& e )
    {
        m_lastError = fmt::format( "Error: {}\nAPI Response:  {}\n", e.what(), res );

        wxLogTrace( traceHTTPLib,
                    wxT( "ValidateHTTPLibraryEndpoints: Exception occurred while testing the API "
                         "connection: %s" ),
                    m_lastError );

        m_endpointValid = false;
    }
}

bool HTTP_LIB_V2_CONNECTION::syncCache()
{
    if( !m_endpointValid )
    {
        wxLogTrace( traceHTTPLib, wxT( "syncCache: without valid connection!" ) );
        return false;
    }

    std::string res;

    if( std::difftime( std::time( nullptr ), m_lastCached ) > m_source.timeout_cache )
    {
        m_lastCached = std::time( nullptr );

        std::unique_ptr<KICAD_CURL_EASY> curl = createCurlEasyObject();

        curl->SetURL( m_source.root_url
                      + fmt::vformat( m_contentEndpoint, fmt::make_format_args( m_timestamp ) ) );

        m_timestamp = 0;
        try
        {
            curl->Perform();

            res = curl->GetBuffer();

            if( !checkServerResponse( curl ) )
            {
                return false;
            }

            if( res.length() == 0 )
            {
                m_lastError = "KiCad received an empty response!\n";
                return false;
            }

            nlohmann::ordered_json response = nlohmann::ordered_json::parse( res );

            if( response.contains( "timestamp" ) )
            {
                m_timestamp = response.at( "timestamp" );
            }

            if( response.contains( "categories" ) )
            {
                for( const auto& category : response.at( "categories" ).items() )
                {
                    readCategory( category.value() );
                }
            }

            if( response.contains( "parts" ) )
            {
                for( const auto& part : response.at( "parts" ).items() )
                {
                    readPart( part.value() );
                }
            }
            return true;
        }
        catch( const std::exception& e )
        {
            m_lastError = fmt::format( "Error: {}\nAPI Response:  {}\n", e.what(), res );

            wxLogTrace( traceHTTPLib,
                        wxT( "Exception occurred while syncing parts from REST API: %s" ),
                        m_lastError );

            m_categoriesById.clear();
            m_categoriesByName.clear();
            m_parts.clear();
            m_partsById.clear();
            m_partsByName.clear();
            return false;
        }
    }
    return true;
}

void HTTP_LIB_V2_CONNECTION::readCategory( const nlohmann::json& aCategory )
{
    std::string id = aCategory.at( "id" );
    std::string name;

    HTTP_LIB_V2_CATEGORY* category = nullptr;

    if( m_categoriesById.contains( id ) )
    {
        category = m_categoriesById.at( id );
    }

    if( aCategory.contains( "active" ) )
    {
        if( !aCategory.at( "active" ) )
        {
            deleteCategory( category );
        }
    }

    if( aCategory.contains( "name" ) )
    {
        name = aCategory.at( "name" );
    }

    if( !category )
    {
        category = createCategory( id, name );
    }
    else
    {
        updateCategory( category, name );
    }

    if( aCategory.contains( "description" ) )
    {
        category->Description = aCategory.at( "description" );
    }
}

void HTTP_LIB_V2_CONNECTION::readPart( const nlohmann::ordered_json& aPart )
{
    std::string id = aPart.at( "id" );
    std::string categoryId = aPart.at( "category_id" );
    std::string name;
    std::string symbol;

    HTTP_LIB_PART* part = nullptr;

    if( m_partsById.contains( id ) )
    {
        part = m_partsById.at( id );
    }

    if( !m_categoriesById.contains( categoryId ) )
    {
        deletePart( part );
        return;
    }

    if( aPart.contains( "active" ) )
    {
        if( !aPart.at( "active" ) )
        {
            deletePart( part );
            return;
        }
    }

    if( aPart.contains( "name" ) )
    {
        name = aPart.at( "name" );
    }
    symbol = aPart.at( "symbol" );

    if( !part )
    {
        part = createPart( id, name, categoryId, symbol );
    }
    else
    {
        updatePart( part, name, categoryId, symbol );
    }

    if( aPart.contains( "keywords" ) )
    {
        part->Keywords = aPart.at( "keywords" );
    }

    if( aPart.contains( "reference" ) )
    {
        readField( part->Reference, aPart.at( "reference" ) );
    }

    if( aPart.contains( "value" ) )
    {
        readField( part->Value, aPart.at( "value" ) );
    }

    if( aPart.contains( "footprint" ) )
    {
        readField( part->Footprint, aPart.at( "footprint" ) );
    }

    if( aPart.contains( "datasheet" ) )
    {
        readField( part->Datasheet, aPart.at( "datasheet" ) );
    }

    if( aPart.contains( "description" ) )
    {
        readField( part->Description, aPart.at( "description" ) );
    }

    if( aPart.contains( "fields" ) )
    {
        std::string fieldName;

        for( const auto& jsonField : aPart.at( "fields" ).items() )
        {
            fieldName = jsonField.key();

            HTTP_LIB_FIELD field = HTTP_LIB_FIELD();
            readField( field, jsonField.value() );

            part->Fields.push_back( std::make_pair( fieldName, field ) );
        }
    }

    if( aPart.contains( "exclude_from_bom" ) )
    {
        part->ExcludeFromBom = aPart.at( "exclude_from_bom" );
    }

    if( aPart.contains( "exclude_from_board" ) )
    {
        part->ExcludeFromBoard = aPart.at( "exclude_from_board" );
    }

    if( aPart.contains( "exclude_from_sim" ) )
    {
        part->ExcludeFromSim = aPart.at( "exclude_from_sim" );
    }

    if( aPart.contains( "power_symbol" ) )
    {
        part->PowerSymbol = aPart.at( "power_symbol" );
    }
}

void HTTP_LIB_V2_CONNECTION::readField( HTTP_LIB_FIELD& aField, const nlohmann::json& aJsonField )
{
    if( aJsonField.contains( "value" ) )
    {
        aField.Value = aJsonField.at( "value" );
    }
    if( aJsonField.contains( "show" ) )
    {
        aField.Show = aJsonField.at( "show" );
    }
    if( aJsonField.contains( "show_name" ) )
    {
        aField.ShowName = aJsonField.at( "show_name" );
    }
}

HTTP_LIB_V2_CONNECTION::HTTP_LIB_V2_CATEGORY*
HTTP_LIB_V2_CONNECTION::createCategory( const std::string& aId, const std::string& aName )
{
    HTTP_LIB_V2_CATEGORY* category = new HTTP_LIB_V2_CATEGORY( aId );

    if( aName.empty() )
    {
        category->Name = category->Id;
    }
    else
    {
        category->Name = aName;
    }

    m_categoriesByName[category->Name] = category;
    m_categoriesById[category->Id] = category;
    return category;
}

void HTTP_LIB_V2_CONNECTION::updateCategory( HTTP_LIB_V2_CATEGORY* aCategory,
                                             const std::string&    aName )
{
    if( aCategory )
    {
        std::string oldName = aCategory->Name;

        if( aName.empty() )
        {
            aCategory->Name = aCategory->Id;
        }
        else
        {
            aCategory->Name = aName;
        }

        if( aCategory->Name != oldName )
        {
            m_categoriesByName.erase( oldName );
            m_categoriesByName[aCategory->Name] = aCategory;
        }
    }
}

void HTTP_LIB_V2_CONNECTION::deleteCategory( HTTP_LIB_V2_CATEGORY* aCategory )
{
    if( aCategory )
    {
        std::vector<HTTP_LIB_PART*> obsoletParts;

        for( std::map<std::string, HTTP_LIB_PART*>::iterator it = m_partsById.begin();
             it != m_partsById.end(); ++it )
        {
            if( it->second->CategoryId == aCategory->Id )
            {
                obsoletParts.push_back( it->second );
            }
        }

        for( auto& it : obsoletParts )
        {
            deletePart( it );
        }

        m_categoriesByName.erase( aCategory->Name );
        m_categoriesById.erase( aCategory->Id );
        delete aCategory;
        aCategory = nullptr;
    }
}
HTTP_LIB_PART* HTTP_LIB_V2_CONNECTION::createPart( const std::string& aId, const std::string& aName,
                                                   const std::string& aCategoryId,
                                                   const std::string& aSymbol )
{
    HTTP_LIB_PART* part = new HTTP_LIB_PART( aId );

    if( aName.empty() )
    {
        part->Name = part->Id;
    }
    else
    {
        part->Name = aName;
    }

    part->CategoryId = aCategoryId;
    part->Symbol = aSymbol;
    m_partsByName[part->Name] = part;
    m_partsById[part->Id] = part;
    m_parts.push_back( part );
    return part;
}

void HTTP_LIB_V2_CONNECTION::updatePart( HTTP_LIB_PART* aPart, const std::string& aName,
                                         const std::string& aCategoryId,
                                         const std::string& aSymbol )
{
    if( aPart )
    {
        std::string oldName = aPart->Name;

        if( aName.empty() )
        {
            aPart->Name = aPart->Id;
        }
        else
        {
            aPart->Name = aName;
        }

        if( aPart->Name != oldName )
        {
            m_partsByName.erase( oldName );
            m_partsByName[aPart->Name] = aPart;
        }

        aPart->CategoryId = aCategoryId;
        if( aPart->Symbol != aSymbol )
        {
            aPart->Symbol = aSymbol;
            aPart->IsSymbolUpdated = true;
        }
        aPart->IsUpdated = true;
    }
}

void HTTP_LIB_V2_CONNECTION::deletePart( HTTP_LIB_PART* aPart )
{
    if( aPart )
    {
        m_parts.erase( std::find( m_parts.begin(), m_parts.end(), aPart ) );
        m_partsByName.erase( aPart->Name );
        m_partsById.erase( aPart->Id );
        delete aPart;
        aPart = nullptr;
    }
}