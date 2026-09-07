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

#ifndef KICAD_DATABASE_CONNECTION_BASE_H
#define KICAD_DATABASE_CONNECTION_BASE_H

#include <any>
#include <map>
#include <set>
#include <string>
#include <vector>

/**
 * Common interface for database library backends (ODBC, CSV).
 *
 * All values in a ROW are stored as std::string regardless of backend,
 * matching what SCH_IO_DATABASE expects when building symbol fields.
 */
class DATABASE_CONNECTION_BASE
{
public:
    typedef std::map<std::string, std::any> ROW;

    virtual ~DATABASE_CONNECTION_BASE() = default;

    virtual bool Connect() = 0;

    virtual bool Disconnect() = 0;

    virtual bool IsConnected() const = 0;

    /**
     * Caches schema information for a table so that subsequent queries know which
     * columns exist.  Matching is case-insensitive.
     */
    virtual bool CacheTableInfo( const std::string& aTable, const std::set<std::string>& aRequiredColumns,
                                 const std::set<std::string>& aOptionalColumns = {} ) = 0;

    /// Retrieve a single row where aWhere.first (column) equals aWhere.second (value).
    virtual bool SelectOne( const std::string& aTable, const std::pair<std::string, std::string>& aWhere,
                            ROW& aResult ) = 0;

    /// Retrieve all rows of a table.  Returns true if at least one ROW was found.
    virtual bool SelectAll( const std::string& aTable, const std::string& aKey, std::vector<ROW>& aResults ) = 0;

    virtual void ClearCache( const std::string& aTable ) {}

    /// Configure single-row result caching; backends without such a cache ignore this.
    virtual void SetCacheParams( int aMaxSize, int aMaxAge ) {}

    std::string GetLastError() const { return m_lastError; }

protected:
    std::string m_lastError;
};

#endif // KICAD_DATABASE_CONNECTION_BASE_H
