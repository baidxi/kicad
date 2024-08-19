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

#ifndef KICAD_HTTP_LIB_SETTINGS_H
#define KICAD_HTTP_LIB_SETTINGS_H

#include <settings/json_settings.h>
#include <ctime>


enum class HTTP_LIB_SOURCE_TYPE
{
    REST_API,
    INVALID
};


struct HTTP_LIB_SOURCE
{
    HTTP_LIB_SOURCE_TYPE type;
    std::string          root_url;
    std::string          api_version;
    std::string          token;
    int                  timeout_cache;
    int                  timeout_parts;
    int                  timeout_categories;
};

class HTTP_LIB_SETTINGS : public JSON_SETTINGS
{
public:
    HTTP_LIB_SETTINGS( const std::string& aFilename );

    virtual ~HTTP_LIB_SETTINGS() {}

    HTTP_LIB_SOURCE m_Source;

    HTTP_LIB_SOURCE_TYPE get_HTTP_LIB_SOURCE_TYPE()
    {
        if( sourceType.compare( "REST_API" ) == 0 )
        {
            return HTTP_LIB_SOURCE_TYPE::REST_API;
        }

        return HTTP_LIB_SOURCE_TYPE::INVALID;
    }

    std::string getSupportedAPIVersion() { return api_version; }

protected:
    wxString getFileExt() const override;

private:
    std::string sourceType;
    std::string api_version = "{v1}, {v2}";
};

#endif //KICAD_HTTP_LIB_SETTINGS_H
