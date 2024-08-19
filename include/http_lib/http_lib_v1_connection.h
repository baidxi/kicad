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

#ifndef KICAD_HTTP_LIB_V1_CONNECTION_H
#define KICAD_HTTP_LIB_V1_CONNECTION_H

#include <any>
#include <boost/algorithm/string.hpp>

#include "http_lib/http_lib_settings.h"
#include <kicad_curl/kicad_curl_easy.h>
#include <http_lib/http_lib_connection.h>

class HTTP_LIB_V1_CONNECTION : public HTTP_LIB_CONNECTION
{
public:
    HTTP_LIB_V1_CONNECTION( const HTTP_LIB_SOURCE& aSource );

    ~HTTP_LIB_V1_CONNECTION() {}

    bool GetPartNames( std::vector<std::string>& aPartNames, const bool powerSymbolsOnly ) override;

    std::vector<HTTP_LIB_PART*>* GetParts( const bool powerSymbolsOnly ) override;

    HTTP_LIB_PART* GetPart( const std::string& aPartName, const bool powerSymbolsOnly ) override;

    bool GetCategoryNames( std::vector<std::string>& aCategories ) override;

    bool GetCategoryName( std::string& CategoryName, const std::string& aCategoryId ) override;

    bool GetCategoryDescription( std::string&       aCategoryDescription,
                                 const std::string& aCategoryName ) override;

private:
    struct HTTP_LIB_V1_PART : HTTP_LIB_PART
    {
        HTTP_LIB_V1_PART( const std::string id ) : HTTP_LIB_PART( id ) {}
        std::time_t LastCached = 0;
        bool        PreloadedDescription = false;
    };

    struct HTTP_LIB_V1_CATEGORY
    {
        HTTP_LIB_V1_CATEGORY() {}
        HTTP_LIB_V1_CATEGORY( const std::string& id ) { Id = id; }
        std::string Id;          ///< id of category
        std::string Name;        ///< name of category
        std::string Description; ///< description of category

        std::time_t LastCached = 0;
        std::list<HTTP_LIB_V1_PART*> Parts;
    };

    bool validateHTTPLibraryEndpoints();

    bool syncCategories();
    bool syncParts();
    bool cacheAll( HTTP_LIB_V1_CATEGORY* aCategory );
    bool cacheUpdateOne( HTTP_LIB_V1_PART* aPart );

    bool boolFromString( const std::any& aVal, bool aDefaultValue );

    std::map<std::string, HTTP_LIB_V1_CATEGORY*> m_categoriesById;
    std::map<std::string, HTTP_LIB_V1_CATEGORY*> m_categoriesByName;

    std::vector<HTTP_LIB_PART*>              m_parts;
    std::map<std::string, HTTP_LIB_V1_PART*> m_partsById;
    std::map<std::string, HTTP_LIB_V1_PART*> m_partsByName;

    const std::string http_endpoint_categories = "categories";
    const std::string http_endpoint_parts = "parts";

    const std::string footprint_field = "footprint";
    const std::string description_field = "description";
    const std::string keywords_field = "keywords";
    const std::string value_field = "value";
    const std::string datasheet_field = "datasheet";
    const std::string reference_field = "reference";
};

#endif //KICAD_HTTP_LIB_V1_CONNECTION_H
