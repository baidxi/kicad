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

#ifndef KICAD_HTTP_LIB_V2_CONNECTION_H
#define KICAD_HTTP_LIB_V2_CONNECTION_H

#include <any>
#include <boost/algorithm/string.hpp>

#include "http_lib/http_lib_settings.h"
#include <kicad_curl/kicad_curl_easy.h>
#include <http_lib/http_lib_connection.h>

class HTTP_LIB_V2_CONNECTION : public HTTP_LIB_CONNECTION
{
public:
    HTTP_LIB_V2_CONNECTION( const HTTP_LIB_SOURCE& aSource );

    ~HTTP_LIB_V2_CONNECTION() {}

    bool GetPartNames( std::vector<std::string>& aPartNames, const bool powerSymbolsOnly ) override;

    std::vector<HTTP_LIB_PART*>* GetParts( const bool powerSymbolsOnly ) override;

    HTTP_LIB_PART* GetPart( const std::string& aPartName, const bool powerSymbolsOnly ) override;

    bool GetCategoryNames( std::vector<std::string>& aCategories ) override;

    bool GetCategoryName( std::string& aCategoryName, const std::string& aCategoryId ) override;

    bool GetCategoryDescription( std::string&       aCategoryDescription,
                                 const std::string& aCategoryName ) override;

private:
    struct HTTP_LIB_V2_CATEGORY
    {
        HTTP_LIB_V2_CATEGORY() {}
        HTTP_LIB_V2_CATEGORY( const std::string& id ) { Id = id; }
        std::string Id;          ///< id of category
        std::string Name;        ///< name of category
        std::string Description; ///< description of category
    };

    void validateEndpoints();
    bool syncCache();
    void readCategory( const nlohmann::json& aCategory );
    void readPart( const nlohmann::ordered_json& aPart );
    void readField( HTTP_LIB_FIELD& aField, const nlohmann::json& aJsonField );

    HTTP_LIB_V2_CATEGORY* createCategory( const std::string& aId, const std::string& aName );
    void updateCategory( HTTP_LIB_V2_CATEGORY* aCategory, const std::string& aName );
    void deleteCategory( HTTP_LIB_V2_CATEGORY* aCategory );

    HTTP_LIB_PART* createPart( const std::string& aId, const std::string& aName,
                               const std::string& aCategoryId, const std::string& aSymbol );
    void updatePart( HTTP_LIB_PART* aPart, const std::string& aName, const std::string& aCategoryId,
                     const std::string& aSymbol );
    void deletePart( HTTP_LIB_PART* aPart );

    long long   m_timestamp = 0;
    std::time_t m_lastCached = 0;
    std::string m_supportedApiServerVersions = "{v2}";
    std::string m_contentEndpoint = "{}";

    std::map<std::string, HTTP_LIB_V2_CATEGORY*> m_categoriesById;
    std::map<std::string, HTTP_LIB_V2_CATEGORY*> m_categoriesByName;

    std::vector<HTTP_LIB_PART*>           m_parts;
    std::map<std::string, HTTP_LIB_PART*> m_partsById;
    std::map<std::string, HTTP_LIB_PART*> m_partsByName;
};

#endif //KICAD_HTTP_LIB_V2_CONNECTION_H
