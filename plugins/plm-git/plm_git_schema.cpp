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
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "plm_git_schema.h"

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/textfile.h>

#include <nlohmann/json.hpp>


static const wxString PLM_ATTRS_FILENAME = wxS( ".kicad_plm_attrs" );


GIT_SCHEMA_MANAGER::GIT_SCHEMA_MANAGER()
{
}


GIT_SCHEMA_MANAGER::~GIT_SCHEMA_MANAGER()
{
}


PLM_SCHEMA GIT_SCHEMA_MANAGER::GetDefaultSchema() const
{
    PLM_SCHEMA schema( wxS( "1.0" ) );

    schema.attributes.push_back(
            CreateAttributeDef( wxS( "description" ), wxS( "Description" ), PLM_ATTR_TYPE::STRING ) );

    schema.attributes.push_back(
            CreateAttributeDef( wxS( "author" ), wxS( "Author" ), PLM_ATTR_TYPE::STRING ) );

    schema.attributes.push_back(
            CreateAttributeDef( wxS( "version" ), wxS( "Version" ), PLM_ATTR_TYPE::STRING ) );

    PLM_ATTRIBUTE_SCHEMA statusAttr =
            CreateAttributeDef( wxS( "status" ), wxS( "Status" ), PLM_ATTR_TYPE::ENUM );
    statusAttr.enum_values = { wxS( "draft" ), wxS( "review" ), wxS( "released" ),
                               wxS( "obsolete" ) };
    statusAttr.default_value = wxS( "draft" );
    schema.attributes.push_back( statusAttr );

    schema.attributes.push_back(
            CreateAttributeDef( wxS( "created_date" ), wxS( "Created" ), PLM_ATTR_TYPE::DATETIME ) );

    schema.attributes.push_back(
            CreateAttributeDef( wxS( "modified_date" ), wxS( "Modified" ), PLM_ATTR_TYPE::DATETIME ) );

    schema.attributes.push_back(
            CreateAttributeDef( wxS( "notes" ), wxS( "Notes" ), PLM_ATTR_TYPE::STRING ) );

    schema.attributes.push_back(
            CreateAttributeDef( wxS( "category" ), wxS( "Category" ), PLM_ATTR_TYPE::STRING ) );

    return schema;
}


PLM_ATTRIBUTE_SET GIT_SCHEMA_MANAGER::LoadAttributes( const wxString& aProjectPath )
{
    PLM_ATTRIBUTE_SET attrs;
    wxString filePath = GetAttributesFilePath( aProjectPath );

    if( !wxFileExists( filePath ) )
        return attrs;

    wxFile file( filePath, wxFile::read );

    if( !file.IsOpened() )
        return attrs;

    wxString content;
    file.ReadAll( &content );
    file.Close();

    return ParseJson( content );
}


bool GIT_SCHEMA_MANAGER::SaveAttributes( const wxString&          aProjectPath,
                                          const PLM_ATTRIBUTE_SET& aAttributes )
{
    wxString filePath = GetAttributesFilePath( aProjectPath );
    wxString json = ToJson( aAttributes );

    wxFile file( filePath, wxFile::write );

    if( !file.IsOpened() )
        return false;

    file.Write( json );
    file.Close();

    return true;
}


wxString GIT_SCHEMA_MANAGER::GetAttributesFilePath( const wxString& aProjectPath )
{
    return aProjectPath + wxFileName::GetPathSeparator() + PLM_ATTRS_FILENAME;
}


bool GIT_SCHEMA_MANAGER::HasAttributes( const wxString& aProjectPath )
{
    return wxFileExists( GetAttributesFilePath( aProjectPath ) );
}


PLM_ATTRIBUTE_SCHEMA GIT_SCHEMA_MANAGER::CreateAttributeDef( const wxString& aId,
                                                              const wxString& aName,
                                                              PLM_ATTR_TYPE aType, bool aRequired )
{
    PLM_ATTRIBUTE_SCHEMA attr;
    attr.attr_id = aId;
    attr.display_name = aName;
    attr.type = aType;
    attr.required = aRequired;
    attr.editable_in_kicad = true;
    attr.display_order = 0;
    return attr;
}


PLM_ATTRIBUTE_SET GIT_SCHEMA_MANAGER::ParseJson( const wxString& aJson )
{
    PLM_ATTRIBUTE_SET attrs;

    try
    {
        nlohmann::json j = nlohmann::json::parse( aJson.ToStdString() );

        if( j.contains( "attributes" ) && j["attributes"].is_object() )
        {
            for( auto& [key, value] : j["attributes"].items() )
            {
                PLM_ATTRIBUTE_VALUE attrVal;
                attrVal.attr_id = wxString::FromUTF8( key );

                if( value.is_string() )
                    attrVal.value = wxString::FromUTF8( value.get<std::string>() );
                else if( value.is_number_integer() )
                    attrVal.value = wxString::Format( wxS( "%d" ), value.get<int>() );
                else if( value.is_number_float() )
                    attrVal.value = wxString::Format( wxS( "%f" ), value.get<double>() );
                else if( value.is_boolean() )
                    attrVal.value = value.get<bool>() ? wxS( "true" ) : wxS( "false" );

                if( !attrVal.value.IsEmpty() )
                    attrs.values.push_back( attrVal );
            }
        }
    }
    catch( const nlohmann::json::exception& )
    {
        // Return empty set on parse error
    }

    return attrs;
}


wxString GIT_SCHEMA_MANAGER::ToJson( const PLM_ATTRIBUTE_SET& aAttributes )
{
    nlohmann::json j;
    nlohmann::json attrsObj;

    for( const auto& val : aAttributes.values )
    {
        attrsObj[val.attr_id.ToStdString()] = val.value.ToStdString();
    }

    j["attributes"] = attrsObj;
    j["schema_version"] = "1.0";

    return wxString::FromUTF8( j.dump( 2 ) );
}
