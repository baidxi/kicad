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


#ifndef SCH_IO_HTTP_LIB_H
#define SCH_IO_HTTP_LIB_H

#include "http_lib/http_lib_settings.h"
#include <http_lib/http_lib_connection.h>
#include <http_lib/http_lib_v1_connection.h>
#include <http_lib/http_lib_v2_connection.h>

#include <sch_io/sch_io.h>
#include <sch_io/sch_io_mgr.h>
#include <wildcards_and_files_ext.h>

/**
 * A KiCad HTTP library provides both symbol and footprint metadata, so there are "shim" plugins
 * on both the symbol and footprint side of things that expose the database contents to the
 * schematic and board editors.  The architecture of these is slightly different from the other
 * plugins because the backing file is just a configuration file rather than something that
 * contains symbol or footprint data.
 */
class SCH_IO_HTTP_LIB : public SCH_IO
{
public:
    SCH_IO_HTTP_LIB() : SCH_IO( wxS( "HTTP library" ) ), m_libTable( nullptr ) {}
    virtual ~SCH_IO_HTTP_LIB() {}

    const IO_BASE::IO_FILE_DESC GetLibraryDesc() const override
    {
        return IO_BASE::IO_FILE_DESC( _HKI( "KiCad HTTP library files" ),
                                      { FILEEXT::HTTPLibraryFileExtension } );
    }

    int GetModifyHash() const override { return 0; }

    void EnumerateSymbolLib( wxArrayString& aSymbolNameList, const wxString& aLibraryPath,
                             const STRING_UTF8_MAP* aProperties = nullptr ) override;

    void EnumerateSymbolLib( std::vector<LIB_SYMBOL*>& aSymbolList, const wxString& aLibraryPath,
                             const STRING_UTF8_MAP* aProperties = nullptr ) override;

    LIB_SYMBOL* LoadSymbol( const wxString& aLibraryPath, const wxString& aAliasName,
                            const STRING_UTF8_MAP* aProperties = nullptr ) override;

    bool SupportsSubLibraries() const override { return true; }

    void GetSubLibraryNames( std::vector<wxString>& aNames ) override;

    wxString GetSubLibraryDescription( const wxString& aName ) override;

    void GetAvailableSymbolFields( std::vector<wxString>& aNames ) override;

    bool IsLibraryWritable( const wxString& aLibraryPath ) override { return false; }

    void SetLibTable( SYMBOL_LIB_TABLE* aTable ) override { m_libTable = aTable; }

    const wxString& GetError() const override { return m_lastError; }

private:
    void ensureSettings( const wxString& aSettingsPath );

    void ensureConnection();

    void establishConnection();

    LIB_SYMBOL* loadSymbolFromPart( HTTP_LIB_PART* aPart );

    SYMBOL_LIB_TABLE* m_libTable;

    std::map<std::string, LIB_SYMBOL*> m_partCache;

    /// Generally will be null if no valid connection is established
    std::unique_ptr<HTTP_LIB_CONNECTION> m_conn;

    std::unique_ptr<HTTP_LIB_SETTINGS> m_settings;

    std::set<wxString> m_customFields;

    wxString m_lastError;
};

#endif // SCH_IO_HTTP_LIB_H_
