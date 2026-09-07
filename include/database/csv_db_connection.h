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

#ifndef KICAD_CSV_DB_CONNECTION_H
#define KICAD_CSV_DB_CONNECTION_H

#include <database/database_connection_base.h>

/**
 * A database library backend that reads a directory of CSV files.
 *
 * Each `*.csv` file in the configured directory becomes a table named after the
 * file (without extension).  The first row is the header (column names); every
 * value is a string.  Files must be UTF-8 and comma-delimited, following
 * RFC 4180 quoting rules (fields containing commas, quotes or newlines are
 * enclosed in double quotes; embedded quotes are doubled).
 *
 * The backend is read-only and loads everything into memory on Connect(),
 * which is appropriate for the size of parts libraries (thousands of rows).
 */
class CSV_DB_CONNECTION : public DATABASE_CONNECTION_BASE
{
public:
    /**
     * @param aDirectoryPath path to a directory containing one or more .csv files.
     */
    CSV_DB_CONNECTION( const std::string& aDirectoryPath, bool aConnectNow = true );

    ~CSV_DB_CONNECTION() override;

    bool Connect() override;

    bool Disconnect() override;

    bool IsConnected() const override { return m_connected; }

    bool CacheTableInfo( const std::string& aTable, const std::set<std::string>& aRequiredColumns,
                         const std::set<std::string>& aOptionalColumns = {} ) override;

    bool SelectOne( const std::string& aTable, const std::pair<std::string, std::string>& aWhere,
                    ROW& aResult ) override;

    bool SelectAll( const std::string& aTable, const std::string& aKey, std::vector<ROW>& aResults ) override;

private:
    struct TABLE
    {
        std::vector<std::string> columns; ///< header, in file order
        std::vector<ROW>         rows;
    };

    /// Parse one CSV file into aTable.  Returns false and sets m_lastError on failure.
    bool parseFile( const std::string& aPath, TABLE& aTable );

    /// Case-insensitive lookup of a column name as stored; empty if not found.
    std::string findColumn( const TABLE& aTable, const std::string& aName ) const;

    std::string m_directory;
    bool        m_connected;

    /// table name (case as on disk) -> table data
    std::map<std::string, TABLE> m_tables;
};

#endif // KICAD_CSV_DB_CONNECTION_H
