/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2023 Andre F. K. Iwers <iwers11@gmail.com>
 * Copyright (C) 2024 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <fmt.h>
#include <lib_symbol.h>
#include <symbol_lib_table.h>

#include <http_lib/http_lib_connection.h>
#include <http_lib/http_lib_v1_connection.h>
#include <http_lib/http_lib_v2_connection.h>
#include "sch_io_http_lib.h"

void SCH_IO_HTTP_LIB::EnumerateSymbolLib( wxArrayString&         aSymbolNameList,
                                          const wxString&        aLibraryPath,
                                          const STRING_UTF8_MAP* aProperties )
{
    ensureSettings( aLibraryPath );
    ensureConnection();

    bool powerSymbolsOnly =
            ( aProperties
              && aProperties->find( SYMBOL_LIB_TABLE::PropPowerSymsOnly ) != aProperties->end() );

    std::vector<std::string> partNames;

    if( !m_conn->GetPartNames( partNames, powerSymbolsOnly ) )
    {
        m_lastError = m_conn->GetLastError();
        THROW_IO_ERROR( m_lastError );
    }

    for( const std::string partName : partNames )
    {
        aSymbolNameList.Add( partName );
    }
}


void SCH_IO_HTTP_LIB::EnumerateSymbolLib( std::vector<LIB_SYMBOL*>& aSymbolList,
                                          const wxString&           aLibraryPath,
                                          const STRING_UTF8_MAP*    aProperties )
{
    wxCHECK_RET( m_libTable, _( "httplib plugin missing library table handle!" ) );

    ensureSettings( aLibraryPath );
    ensureConnection();

    bool powerSymbolsOnly =
            ( aProperties
              && aProperties->find( SYMBOL_LIB_TABLE::PropPowerSymsOnly ) != aProperties->end() );

    std::vector<HTTP_LIB_PART*>* parts = m_conn->GetParts( powerSymbolsOnly );

    if( !parts )
    {
        m_lastError = m_conn->GetLastError();
        THROW_IO_ERROR( m_lastError );
    }

    for( HTTP_LIB_PART* part : *parts )
    {
        LIB_SYMBOL* symbol = loadSymbolFromPart( part );

        if( symbol && ( !powerSymbolsOnly || symbol->IsPower() ) )
            aSymbolList.emplace_back( symbol );
    }
}


LIB_SYMBOL* SCH_IO_HTTP_LIB::LoadSymbol( const wxString& aLibraryPath, const wxString& aAliasName,
                                         const STRING_UTF8_MAP* aProperties )
{
    wxCHECK( m_libTable, nullptr );

    ensureSettings( aLibraryPath );
    ensureConnection();

    std::string partName( aAliasName.ToUTF8() );

    bool powerSymbolsOnly =
            ( aProperties
              && aProperties->find( SYMBOL_LIB_TABLE::PropPowerSymsOnly ) != aProperties->end() );

    HTTP_LIB_PART* part = m_conn->GetPart( partName, powerSymbolsOnly );

    if( !part )
    {
        m_lastError = m_conn->GetLastError();
        THROW_IO_ERROR( m_lastError );
    }

    return loadSymbolFromPart( part );
}


void SCH_IO_HTTP_LIB::GetSubLibraryNames( std::vector<wxString>& aNames )
{
    ensureSettings( wxEmptyString );
    ensureConnection();

    aNames.clear();

    std::vector<std::string> categories;

    if( !m_conn->GetCategoryNames( categories ) )
    {
        m_lastError = m_conn->GetLastError();
        THROW_IO_ERROR( m_lastError );
    }

    for( const std::string category : categories )
    {
        aNames.emplace_back( category );
    }
}

wxString SCH_IO_HTTP_LIB::GetSubLibraryDescription( const wxString& aName )
{
    ensureSettings( wxEmptyString );
    ensureConnection();

    std::string description;

    if( !m_conn->GetCategoryDescription( description, std::string( aName.ToUTF8() ) ) )
    {
        m_lastError = m_conn->GetLastError();
        THROW_IO_ERROR( m_lastError );
    }

    return description;
}

void SCH_IO_HTTP_LIB::GetAvailableSymbolFields( std::vector<wxString>& aNames )
{
    std::copy( m_customFields.begin(), m_customFields.end(), std::back_inserter( aNames ) );
}

void SCH_IO_HTTP_LIB::ensureSettings( const wxString& aSettingsPath )
{
    auto tryLoad = [&]()
    {
        if( !m_settings->LoadFromFile() )
        {
            wxString msg = wxString::Format(
                    _( "HTTP library settings file %s missing or invalid" ), aSettingsPath );

            THROW_IO_ERROR( msg );
        }

        if( m_settings->m_Source.api_version.empty() )
        {
            wxString msg = wxString::Format(
                    _( "HTTP library settings file %s is missing the API version number!" ),
                    aSettingsPath );

            THROW_IO_ERROR( msg );
        }

        if( m_settings->getSupportedAPIVersion().find( "{" + m_settings->m_Source.api_version
                                                       + "}" )
            == std::string::npos )
        {
            wxString msg = wxString::Format( _( "HTTP library settings file %s uses API version "
                                                "%s, but KiCad requires version %s" ),
                                             aSettingsPath, m_settings->m_Source.api_version,
                                             m_settings->getSupportedAPIVersion() );

            THROW_IO_ERROR( msg );
        }

        if( m_settings->m_Source.root_url.empty() )
        {
            wxString msg = wxString::Format(
                    _( "HTTP library settings file %s is missing the root URL!" ), aSettingsPath );

            THROW_IO_ERROR( msg );
        }

        // map lib source type
        m_settings->m_Source.type = m_settings->get_HTTP_LIB_SOURCE_TYPE();

        if( m_settings->m_Source.type == HTTP_LIB_SOURCE_TYPE::INVALID )
        {
            wxString msg = wxString::Format(
                    _( "HTTP library settings file %s has an invalid library type" ),
                    aSettingsPath );

            THROW_IO_ERROR( msg );
        }

        // make sure that the root url finishes with a forward slash
        if( m_settings->m_Source.root_url.at( m_settings->m_Source.root_url.length() - 1 ) != '/' )
        {
            m_settings->m_Source.root_url += "/";
        }

        // Append api version to root URL
        m_settings->m_Source.root_url += m_settings->m_Source.api_version + "/";
    };

    if( !m_settings && !aSettingsPath.IsEmpty() )
    {
        std::string path( aSettingsPath.ToUTF8() );
        m_settings = std::make_unique<HTTP_LIB_SETTINGS>( path );

        m_settings->SetReadOnly( true );

        tryLoad();
    }
    else if( m_settings )
    {
        // Reload settings in case user is editing
        tryLoad();
    }
    else if( !m_settings )
    {
        wxLogTrace( traceHTTPLib, wxT( "ensureSettings: no settings available!" ) );
    }
}

void SCH_IO_HTTP_LIB::ensureConnection()
{
    wxCHECK_RET( m_settings, "Call ensureSettings before ensureConnection!" );


    if( m_conn )
    {
        if( m_conn->IsValidEndpoint( m_settings->m_Source ) )
        {
            return;
        }

        m_conn.reset();
    }

    establishConnection();

    if( m_conn && !m_conn->IsValidEndpoint( m_settings->m_Source ) )
    {
        m_lastError = m_conn->GetLastError();

        m_conn.reset();
    }

    if( !m_conn )
    {
        wxString msg = wxString::Format( _( "Could not connect to %s. Errors: %s" ),
                                         m_settings->m_Source.root_url, m_lastError );
        THROW_IO_ERROR( msg );
    }
}

void SCH_IO_HTTP_LIB::establishConnection()
{
    if( m_settings->m_Source.api_version == "v1" )
    {
        m_conn = std::make_unique<HTTP_LIB_V1_CONNECTION>( m_settings->m_Source );
    }

    if( m_settings->m_Source.api_version == "v2" )
    {
        m_conn = std::make_unique<HTTP_LIB_V2_CONNECTION>( m_settings->m_Source );
    }
}

LIB_SYMBOL* SCH_IO_HTTP_LIB::loadSymbolFromPart( HTTP_LIB_PART* aPart )
{
    LIB_SYMBOL* symbol = nullptr;
    LIB_SYMBOL* originalSymbol = nullptr;
    LIB_ID      symbolId;
    std::string categoryName;

    symbol = m_partCache[aPart->Id];
    if( !aPart->IsUpdated && symbol )
    {
        return symbol;
    }

    if( !m_conn->GetCategoryName( categoryName, aPart->CategoryId ) )
    {
        wxLogTrace( traceHTTPLib, wxT( "loadSymbolFromPart: category name not fount for id '%s'" ),
                    aPart->CategoryId );
    }
    if( symbol && aPart->IsSymbolUpdated )
    {
        m_partCache.erase( aPart->Id );
        delete symbol;
        symbol = nullptr;
    }

    // Get or Create the symbol using the found symbol
    if( !symbol )
    {
        std::string symbolIdStr = aPart->Symbol;
        symbolId.Parse( symbolIdStr );

        if( symbolId.IsValid() )
        {
            originalSymbol = m_libTable->LoadSymbol( symbolId );
        }

        if( originalSymbol )
        {
            wxLogTrace( traceHTTPLib, wxT( "loadSymbolFromPart: found original symbol '%s'" ),
                        symbolIdStr );

            symbol = originalSymbol->Duplicate();
            symbol->SetSourceLibId( symbolId );
        }
        else if( !symbolId.IsValid() )
        {
            wxLogTrace( traceHTTPLib,
                        wxT( "loadSymbolFromPart: source symbol id '%s' is invalid, "
                             "will create empty symbol" ),
                        symbolIdStr );
        }
        else
        {
            wxLogTrace( traceHTTPLib,
                        wxT( "loadSymbolFromPart: source symbol '%s' not found, "
                             "will create empty symbol" ),
                        symbolIdStr );
        }
    }

    if( !symbol )
    {
        // Actual symbol not found: return metadata only; error will be
        // indicated in the symbol chooser
        symbol = new LIB_SYMBOL( aPart->Name );
    }

    symbol->SetName( aPart->Name );

    LIB_ID libId = symbol->GetLibId();
    libId.SetSubLibraryName( categoryName );
    symbol->SetLibId( libId );

    symbol->SetKeyWords( aPart->Keywords );

    symbol->SetExcludedFromBOM( aPart->ExcludeFromBom );
    symbol->SetExcludedFromBoard( aPart->ExcludeFromBoard );
    symbol->SetExcludedFromSim( aPart->ExcludeFromSim );

    if( aPart->PowerSymbol )
    {
        symbol->SetPower();
    }

    SCH_FIELD* field;

    field = &symbol->GetFootprintField();
    field->SetText( aPart->Footprint.Value );
    field->SetVisible( aPart->Footprint.Show );
    field->SetNameShown( aPart->Footprint.ShowName );

    field = &symbol->GetDescriptionField();
    field->SetText( aPart->Description.Value );
    field->SetVisible( aPart->Description.Show );
    field->SetNameShown( aPart->Description.ShowName );

    field = &symbol->GetValueField();
    field->SetText( aPart->Value.Value );
    field->SetVisible( aPart->Value.Show );
    field->SetNameShown( aPart->Value.ShowName );

    field = &symbol->GetDatasheetField();
    field->SetText( aPart->Datasheet.Value );
    field->SetVisible( aPart->Datasheet.Show );
    field->SetNameShown( aPart->Datasheet.ShowName );

    field = &symbol->GetReferenceField();
    field->SetText( aPart->Reference.Value );
    field->SetVisible( aPart->Reference.Show );
    field->SetNameShown( aPart->Reference.ShowName );


    for( auto& customField : aPart->Fields )
    {
        wxString fieldName = wxString( customField.first );
        auto&    fieldProperties = customField.second;
        {
            // Check if field exists, if so replace Text and adjust visiblity.
            //
            // This proves useful in situations where, for instance, an individual requires a particular value, such as
            // the material type showcased at a specific position for a capacitor. Subsequently, this value could be defined
            // in the symbol itself and then, potentially, be modified by the HTTP library as necessary.
            field = symbol->FindField( fieldName );

            if( field != nullptr )
            {
                // adjust values accordingly
                field->SetText( fieldProperties.Value );
                field->SetVisible( fieldProperties.Show );
                field->SetNameShown( fieldProperties.ShowName );
            }
            else
            {
                // Generic fields
                field = new SCH_FIELD( nullptr, symbol->GetNextAvailableFieldId() );
                field->SetName( fieldName );
                field->SetText( fieldProperties.Value );
                field->SetVisible( fieldProperties.Show );
                field->SetNameShown( fieldProperties.ShowName );
                symbol->AddField( field );

                m_customFields.insert( fieldName );
            }
        }
    }
    aPart->IsUpdated = false;
    aPart->IsSymbolUpdated = false;
    m_partCache[aPart->Id] = symbol;
    return symbol;
}