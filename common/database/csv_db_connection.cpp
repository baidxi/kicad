/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <fmt/format.h>

#include <macros.h>

#include <database/csv_db_connection.h>

namespace fs = std::filesystem;


static std::string toLower( const std::string& aStr )
{
    std::string ret( aStr );
    std::transform( ret.begin(), ret.end(), ret.begin(),
                    []( unsigned char c )
                    {
                        return std::tolower( c );
                    } );
    return ret;
}


CSV_DB_CONNECTION::CSV_DB_CONNECTION( const std::string& aDirectoryPath, bool aConnectNow ) :
        m_directory( aDirectoryPath ),
        m_connected( false )
{
    if( aConnectNow )
        Connect();
}


CSV_DB_CONNECTION::~CSV_DB_CONNECTION()
{
    Disconnect();
}


bool CSV_DB_CONNECTION::Connect()
{
    m_tables.clear();
    m_connected = false;

    std::error_code ec;

    if( !fs::is_directory( m_directory, ec ) )
    {
        m_lastError = fmt::format( "'{}' is not a directory", m_directory );
        return false;
    }

    for( const fs::directory_entry& entry : fs::directory_iterator( m_directory, ec ) )
    {
        if( !entry.is_regular_file() )
            continue;

        if( toLower( entry.path().extension().string() ) != ".csv" )
            continue;

        TABLE table;

        if( !parseFile( entry.path().string(), table ) )
            return false; // m_lastError set by parseFile

        m_tables[entry.path().stem().string()] = std::move( table );
    }

    if( ec )
    {
        m_lastError = fmt::format( "error reading directory '{}': {}", m_directory, ec.message() );
        return false;
    }

    if( m_tables.empty() )
    {
        m_lastError = fmt::format( "no .csv files found in '{}'", m_directory );
        return false;
    }

    m_connected = true;
    return true;
}


bool CSV_DB_CONNECTION::Disconnect()
{
    m_tables.clear();
    m_connected = false;
    return true;
}


bool CSV_DB_CONNECTION::parseFile( const std::string& aPath, TABLE& aTable )
{
    std::ifstream file( aPath, std::ios::binary );

    if( !file.is_open() )
    {
        m_lastError = fmt::format( "could not open '{}'", aPath );
        return false;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string text = ss.str();

    // Strip UTF-8 BOM if present (Excel likes to add one)
    if( text.size() >= 3 && text.compare( 0, 3, "\xEF\xBB\xBF" ) == 0 )
        text.erase( 0, 3 );

    // RFC 4180 state machine.  Handles quoted fields, doubled quotes,
    // embedded commas/newlines, and both LF and CRLF line endings.
    std::vector<std::vector<std::string>> records;
    std::vector<std::string>              record;
    std::string                           field;
    bool                                  inQuotes = false;
    size_t                                line = 1;

    auto endField = [&]()
    {
        record.emplace_back( std::move( field ) );
        field.clear();
    };
    auto endRecord = [&]()
    {
        endField();

        // Skip records that are entirely empty (e.g. trailing newline)
        if( record.size() > 1 || !record[0].empty() )
            records.emplace_back( std::move( record ) );

        record.clear();
    };

    for( size_t i = 0; i < text.size(); ++i )
    {
        char c = text[i];

        if( inQuotes )
        {
            if( c == '"' )
            {
                if( i + 1 < text.size() && text[i + 1] == '"' )
                {
                    field += '"';
                    ++i;
                }
                else
                {
                    inQuotes = false;
                }
            }
            else
            {
                if( c == '\n' )
                    ++line;

                field += c;
            }
        }
        else
        {
            switch( c )
            {
            case '"':
                if( !field.empty() )
                {
                    m_lastError = fmt::format( "{}:{}: unexpected quote inside unquoted field", aPath, line );
                    return false;
                }
                inQuotes = true;
                break;

            case ',': endField(); break;

            case '\r':
                if( i + 1 < text.size() && text[i + 1] == '\n' )
                    break; // handled by the '\n' case next iteration
                KI_FALLTHROUGH;

            case '\n':
                endRecord();
                ++line;
                break;

            default: field += c;
            }
        }
    }

    if( inQuotes )
    {
        m_lastError = fmt::format( "{}: unterminated quoted field", aPath );
        return false;
    }

    if( !field.empty() || !record.empty() )
        endRecord();

    if( records.empty() )
    {
        m_lastError = fmt::format( "{}: file is empty", aPath );
        return false;
    }

    aTable.columns = records[0];

    // Helpful diagnostic for the most common regional-settings mistake:
    // Excel configured for locales using ',' as decimal separator exports
    // semicolon-delimited files.
    if( aTable.columns.size() == 1 && aTable.columns[0].find( ';' ) != std::string::npos )
    {
        m_lastError = fmt::format( "{}: header contains ';' but no ',' - this file appears to "
                                   "be semicolon-delimited (a common Excel regional-settings "
                                   "issue).  Re-save it as comma-delimited UTF-8 CSV.",
                                   aPath );
        return false;
    }

    // Reject duplicate column names (case-insensitive)
    std::set<std::string> seen;

    for( const std::string& col : aTable.columns )
    {
        if( col.empty() )
        {
            m_lastError = fmt::format( "{}: header contains an empty column name", aPath );
            return false;
        }

        if( !seen.insert( toLower( col ) ).second )
        {
            m_lastError = fmt::format( "{}: duplicate column name '{}'", aPath, col );
            return false;
        }
    }

    size_t nCols = aTable.columns.size();

    for( size_t r = 1; r < records.size(); ++r )
    {
        if( records[r].size() != nCols )
        {
            m_lastError =
                    fmt::format( "{}: row {} has {} fields, expected {}", aPath, r + 1, records[r].size(), nCols );
            return false;
        }

        ROW row;

        for( size_t cIdx = 0; cIdx < nCols; ++cIdx )
            row[aTable.columns[cIdx]] = records[r][cIdx];

        aTable.rows.emplace_back( std::move( row ) );
    }

    return true;
}


std::string CSV_DB_CONNECTION::findColumn( const TABLE& aTable, const std::string& aName ) const
{
    std::string needle = toLower( aName );

    for( const std::string& col : aTable.columns )
    {
        if( toLower( col ) == needle )
            return col;
    }

    return std::string();
}


bool CSV_DB_CONNECTION::CacheTableInfo( const std::string& aTable, const std::set<std::string>& aRequiredColumns,
                                        const std::set<std::string>& aOptionalColumns )
{
    auto it = m_tables.find( aTable );

    if( it == m_tables.end() )
    {
        m_lastError = fmt::format( "table '{}' not found (no file '{}.csv' in '{}')", aTable, aTable, m_directory );
        return false;
    }

    for( const std::string& required : aRequiredColumns )
    {
        if( findColumn( it->second, required ).empty() )
        {
            m_lastError = fmt::format( "table '{}' is missing required column '{}'", aTable, required );
            return false;
        }
    }

    return true;
}


bool CSV_DB_CONNECTION::SelectOne( const std::string& aTable, const std::pair<std::string, std::string>& aWhere,
                                   ROW& aResult )
{
    auto it = m_tables.find( aTable );

    if( it == m_tables.end() )
    {
        m_lastError = fmt::format( "table '{}' not found", aTable );
        return false;
    }

    std::string column = findColumn( it->second, aWhere.first );

    if( column.empty() )
    {
        m_lastError = fmt::format( "column '{}' not found in table '{}'", aWhere.first, aTable );
        return false;
    }

    for( const ROW& row : it->second.rows )
    {
        auto cell = row.find( column );

        if( cell != row.end() && std::any_cast<std::string>( cell->second ) == aWhere.second )
        {
            aResult = row;
            return true;
        }
    }

    return false;
}


bool CSV_DB_CONNECTION::SelectAll( const std::string& aTable, const std::string& aKey, std::vector<ROW>& aResults )
{
    auto it = m_tables.find( aTable );

    if( it == m_tables.end() )
    {
        m_lastError = fmt::format( "table '{}' not found", aTable );
        return false;
    }

    aResults = it->second.rows;

    return !aResults.empty();
}
