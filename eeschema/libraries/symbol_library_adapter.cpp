/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 * @author Jon Evans <jon@craftyjon.com>
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

#include <magic_enum.hpp>
#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>

#include <common.h>
#include <dialog_shim.h>
#include <ki_exception.h>
#include <wx/log.h>

#include <lib_symbol.h>
#include <libraries/symbol_library_adapter.h>
#include <sch_io/kicad_sexpr/sch_io_kicad_sexpr.h>

#include <env_vars.h>
#include <pgm_base.h>
#include <project.h>
#include <template_fieldnames.h>
#include <thread_pool.h>
#include <trace_helpers.h>
#include <settings/settings_manager.h>

using namespace std::chrono_literals;


const char* SYMBOL_LIBRARY_ADAPTER::PropPowerSymsOnly = "pwr_sym_only";
const char* SYMBOL_LIBRARY_ADAPTER::PropNonPowerSymsOnly = "non_pwr_sym_only";

LEAK_AT_EXIT<std::map<wxString, LIB_DATA>> SYMBOL_LIBRARY_ADAPTER::GlobalLibraries;

LEAK_AT_EXIT<std::shared_mutex> SYMBOL_LIBRARY_ADAPTER::GlobalLibraryMutex;

LEAK_AT_EXIT<std::map<wxString, std::vector<SYMBOL_LIBRARY_ADAPTER::SYMBOL_INDEX>>>
        SYMBOL_LIBRARY_ADAPTER::PreloadedSymbolIndex;

std::shared_mutex SYMBOL_LIBRARY_ADAPTER::PreloadedSymbolIndexMutex;


SYMBOL_LIBRARY_ADAPTER::SYMBOL_LIBRARY_ADAPTER( LIBRARY_MANAGER& aManager ) :
            LIBRARY_MANAGER_ADAPTER( aManager )
{
}


SYMBOL_LIBRARY_ADAPTER::~SYMBOL_LIBRARY_ADAPTER()
{
    evictOwnedGlobalEntries();
}


wxString SYMBOL_LIBRARY_ADAPTER::GlobalPathEnvVariableName()
{
    return ENV_VAR::GetVersionedEnvVarName( wxS( "SYMBOL_DIR" ) );
}


SCH_IO* SYMBOL_LIBRARY_ADAPTER::schplugin( const LIB_DATA* aRow )
{
    SCH_IO* ret = dynamic_cast<SCH_IO*>( aRow->plugin.get() );
    wxCHECK( aRow->plugin && ret, nullptr );
    return ret;
}


namespace
{

std::vector<SYMBOL_LIBRARY_ADAPTER::SYMBOL_INDEX> enumerateSymbolIndex( const LIB_DATA* aLib, const wxString& aUri,
                                                                        bool aPowerOnly )
{
    std::map<std::string, UTF8> options = aLib->row->GetOptionsMap();

    if( aPowerOnly )
        options[SYMBOL_LIBRARY_ADAPTER::PropPowerSymsOnly] = "";

    std::vector<SYMBOL_LIBRARY_ADAPTER::SYMBOL_INDEX> symbolIndex;
    SCH_IO*                                           plugin = dynamic_cast<SCH_IO*>( aLib->plugin.get() );
    wxCHECK( aLib->plugin && plugin, symbolIndex );

    if( SCH_IO_KICAD_SEXPR* kicadPlugin = dynamic_cast<SCH_IO_KICAD_SEXPR*>( plugin ) )
    {
        kicadPlugin->EnumerateSymbolLibIndex( symbolIndex, aUri, &options );
    }
    else
    {
        wxArrayString namesAS;
        plugin->EnumerateSymbolLib( namesAS, aUri, &options );

        symbolIndex.reserve( namesAS.size() );

        for( const wxString& name : namesAS )
        {
            SYMBOL_LIBRARY_ADAPTER::SYMBOL_INDEX index;
            index.m_Name = name;
            index.m_IsPower = aPowerOnly;
            index.m_ChooserFields[GetDefaultFieldName( FIELD_T::VALUE, false )] = name;
            symbolIndex.push_back( std::move( index ) );
        }
    }

    return symbolIndex;
}

} // namespace


void SYMBOL_LIBRARY_ADAPTER::enumerateLibrary( LIB_DATA* aLib, const wxString& aUri )
{
    wxString                  nickname = aLib->row->Nickname();
    std::vector<SYMBOL_INDEX> symbolIndex;

    {
        std::lock_guard pluginGuard( pluginMutex( nickname ) );
        symbolIndex = enumerateSymbolIndex( aLib, aUri, false );
    }

    std::unique_lock lock( PreloadedSymbolIndexMutex );
    PreloadedSymbolIndex.Get()[nickname] = std::move( symbolIndex );
}


std::optional<LIB_STATUS> SYMBOL_LIBRARY_ADAPTER::LoadOne( LIB_DATA* aLib )
{
    aLib->status.load_status = LOAD_STATUS::LOADING;

    try
    {
        enumerateLibrary( aLib, getUri( aLib->row ) );
        wxLogTrace( traceLibraries, "Sym: %s: library enumerated", aLib->row->Nickname() );
        aLib->status.load_status = LOAD_STATUS::LOADED;
    }
    catch( IO_ERROR& e )
    {
        aLib->status.load_status = LOAD_STATUS::LOAD_ERROR;
        aLib->status.error = LIBRARY_ERROR( { e.What() } );
        wxLogTrace( traceLibraries, "Sym: %s: plugin threw exception: %s", aLib->row->Nickname(), e.What() );
    }

    return aLib->status;
}


std::optional<LIB_STATUS> SYMBOL_LIBRARY_ADAPTER::LoadOne( const wxString& nickname )
{
    LIBRARY_RESULT<LIB_DATA*> result = loadIfNeeded( nickname );

    if( result.has_value() )
        return loadLibraryEntry( nickname, *result );

    return LIB_STATUS { .load_status = LOAD_STATUS::LOAD_ERROR,
                        .error = LIBRARY_ERROR( { result.error() } ) };
}


LIBRARY_RESULT<IO_BASE*> SYMBOL_LIBRARY_ADAPTER::createPlugin( const LIBRARY_TABLE_ROW* row )
{
    SCH_IO_MGR::SCH_FILE_T type = SCH_IO_MGR::EnumFromStr( row->Type() );

    if( type == SCH_IO_MGR::SCH_NESTED_TABLE )
    {
        wxString   msg;
        wxFileName fileName( m_manager.GetFullURI( row, true ) );

        if( fileName.FileExists() )
            return tl::unexpected( LIBRARY_TABLE_OK() );
        else
            msg = wxString::Format( _( "Nested table '%s' not found." ), row->URI() );

        return tl::unexpected( LIBRARY_ERROR( msg ) );
    }
    else if( type == SCH_IO_MGR::SCH_FILE_UNKNOWN )
    {
        wxLogTrace( traceLibraries, "Sym: Plugin type %s is unknown!", row->Type() );
        wxString msg = wxString::Format( _( "Unknown library type %s " ), row->Type() );
        return tl::unexpected( LIBRARY_ERROR( msg ) );
    }

    SCH_IO* plugin = SCH_IO_MGR::FindPlugin( type );
    wxCHECK( plugin, tl::unexpected( LIBRARY_ERROR( _( "Internal error" ) ) ) );

    plugin->SetLibraryManagerAdapter( this );

    wxLogTrace( traceLibraries, "Sym: Library %s (%s) plugin created", row->Nickname(),
                magic_enum::enum_name( row->Scope() ) );

    return plugin;
}


std::vector<LIB_SYMBOL*> SYMBOL_LIBRARY_ADAPTER::GetSymbols( const wxString& aNickname, SYMBOL_TYPE aType )
{
    std::vector<LIB_SYMBOL*> symbols;

    std::optional<const LIB_DATA*> maybeLib = fetchIfLoaded( aNickname );

    if( !maybeLib )
        return symbols;

    const LIB_DATA* lib = *maybeLib;
    std::map<std::string, UTF8> options = lib->row->GetOptionsMap();

    if( aType == SYMBOL_TYPE::POWER_ONLY )
        options[PropPowerSymsOnly] = "";

    try
    {
        std::lock_guard pluginGuard( pluginMutex( aNickname ) );
        schplugin( lib )->EnumerateSymbolLib( symbols, getUri( lib->row ), &options );
    }
    catch( IO_ERROR& e )
    {
        wxLogTrace( traceLibraries, "Sym: Exception enumerating library %s: %s", lib->row->Nickname(), e.What() );
    }

    for( LIB_SYMBOL* symbol : symbols )
    {
        LIB_ID id = symbol->GetLibId();
        id.SetLibNickname( lib->row->Nickname() );
        symbol->SetLibId( id );
    }

    return symbols;
}


std::vector<wxString> SYMBOL_LIBRARY_ADAPTER::GetSymbolNames( const wxString& aNickname, SYMBOL_TYPE aType )
{
    std::vector<wxString> names;
    std::vector<SYMBOL_INDEX> symbolIndex = GetSymbolIndex( aNickname, aType );

    names.reserve( symbolIndex.size() );

    for( const SYMBOL_INDEX& symbol : symbolIndex )
        names.emplace_back( symbol.m_Name );

    return names;
}


std::vector<SYMBOL_LIBRARY_ADAPTER::SYMBOL_INDEX> SYMBOL_LIBRARY_ADAPTER::GetSymbolIndex( const wxString& aNickname,
                                                                                          SYMBOL_TYPE     aType )
{
    std::optional<const LIB_DATA*> maybeLib = fetchIfLoaded( aNickname );

    if( !maybeLib )
        return {};

    {
        std::shared_lock lock( PreloadedSymbolIndexMutex );
        auto             it = PreloadedSymbolIndex.Get().find( aNickname );

        if( it != PreloadedSymbolIndex.Get().end() )
        {
            if( aType == SYMBOL_TYPE::ALL_SYMBOLS )
                return it->second;

            std::vector<SYMBOL_INDEX> powerSymbols;

            std::ranges::copy_if( it->second, std::back_inserter( powerSymbols ),
                                  []( const SYMBOL_INDEX& aSymbol )
                                  {
                                      return aSymbol.m_IsPower;
                                  } );

            return powerSymbols;
        }
    }

    const LIB_DATA*           lib = *maybeLib;
    std::vector<SYMBOL_INDEX> symbolIndex;

    try
    {
        std::lock_guard pluginGuard( pluginMutex( aNickname ) );
        symbolIndex = enumerateSymbolIndex( lib, getUri( lib->row ), aType == SYMBOL_TYPE::POWER_ONLY );
    }
    catch( const IO_ERROR& e )
    {
        wxLogTrace( traceLibraries, "Sym: Exception enumerating library %s: %s", lib->row->Nickname(), e.What() );
    }

    if( aType == SYMBOL_TYPE::ALL_SYMBOLS )
    {
        std::unique_lock lock( PreloadedSymbolIndexMutex );
        PreloadedSymbolIndex.Get()[aNickname] = symbolIndex;
    }

    return symbolIndex;
}


LIB_SYMBOL* SYMBOL_LIBRARY_ADAPTER::LoadSymbol( const wxString& aNickname, const wxString& aName )
{
    std::optional<const LIB_DATA*> lib = fetchIfLoaded( aNickname );

    if( !lib )
    {
        std::optional<LIB_STATUS> status = LoadLibraryEntry( aNickname );

        if( !status || status->load_status != LOAD_STATUS::LOADED || status->error )
        {
            wxLogTrace( traceLibraries, "LoadSymbol: requested library %s not loaded", aNickname );
            return nullptr;
        }

        lib = fetchIfLoaded( aNickname );
    }

    if( !lib )
    {
        wxLogTrace( traceLibraries, "LoadSymbol: requested library %s not loaded", aNickname );
        return nullptr;
    }

    std::lock_guard pluginGuard( pluginMutex( aNickname ) );

    if( LIB_SYMBOL* symbol = schplugin( *lib )->LoadSymbol( getUri( ( *lib )->row ), aName ) )
    {
        LIB_ID id = symbol->GetLibId();
        id.SetLibNickname( ( *lib )->row->Nickname() );
        symbol->SetLibId( id );
        return symbol;
    }

    return nullptr;
}


SYMBOL_LIBRARY_ADAPTER::SAVE_T SYMBOL_LIBRARY_ADAPTER::SaveSymbol( const wxString& aNickname,
                                                                   const LIB_SYMBOL* aSymbol, bool aOverwrite )
{
    wxCHECK( aSymbol, SAVE_SKIPPED );

    LIBRARY_RESULT<LIB_DATA*> libResult = loadIfNeeded( aNickname );

    if( !libResult.has_value() )
    {
        wxLogTrace( traceLibraries, "SaveSymbol: unable to load library %s: %s", aNickname,
                    libResult.error().message );
        return SAVE_SKIPPED;
    }

    LIB_DATA* lib = *libResult;

    if( !lib )
    {
        wxLogTrace( traceLibraries, "SaveSymbol: library %s not found", aNickname );
        return SAVE_SKIPPED;
    }

    SCH_IO* plugin = schplugin( lib );
    wxCHECK( plugin, SAVE_SKIPPED );

    std::map<std::string, UTF8> options = lib->row->GetOptionsMap();
    std::lock_guard             pluginGuard( pluginMutex( aNickname ) );

    if( !aOverwrite )
    {
        try
        {
            std::unique_ptr<LIB_SYMBOL> existing( plugin->LoadSymbol( getUri( lib->row ), aSymbol->GetName(),
                                                                      &options ) );

            if( existing )
                return SAVE_SKIPPED;
        }
        catch( const IO_ERROR& e )
        {
            wxLogTrace( traceLibraries, "SaveSymbol: error checking for existing symbol %s:%s: %s", aNickname,
                        aSymbol->GetName(), e.What() );
            return SAVE_SKIPPED;
        }
    }

    try
    {
        plugin->SaveSymbol( getUri( lib->row ), aSymbol, &options );
    }
    catch( const IO_ERROR& e )
    {
        wxLogTrace( traceLibraries, "SaveSymbol: error saving %s:%s: %s", aNickname, aSymbol->GetName(), e.What() );
        return SAVE_SKIPPED;
    }

    {
        std::unique_lock lock( PreloadedSymbolIndexMutex );
        PreloadedSymbolIndex.Get().erase( aNickname );
    }

    return SAVE_OK;
}


void SYMBOL_LIBRARY_ADAPTER::DeleteSymbol( const wxString& aNickname, const wxString& aSymbolName )
{
    wxCHECK_MSG( false, /* void */, "Unimplemented!" );
}


bool SYMBOL_LIBRARY_ADAPTER::IsSymbolLibWritable( const wxString& aLib )
{
    // Route through fetchIfLoaded() so LOAD_ERROR sentinel entries, which carry a null
    // plugin, are filtered out instead of dereferenced.
    if( std::optional<const LIB_DATA*> lib = fetchIfLoaded( aLib ) )
    {
        std::lock_guard pluginGuard( pluginMutex( aLib ) );

        return ( *lib )->plugin->IsLibraryWritable( getUri( ( *lib )->row ) );
    }

    return false;
}


std::vector<wxString> SYMBOL_LIBRARY_ADAPTER::GetAvailableExtraFields( const wxString& aNickname )
{
    std::vector<wxString> fields;

    if( std::optional<LIB_DATA*> result = fetchIfLoaded( aNickname ) )
    {
        LIB_DATA* rowData = *result;
        std::lock_guard pluginGuard( pluginMutex( aNickname ) );

        int hash = schplugin( rowData )->GetModifyHash();

        if( hash != rowData->modify_hash )
        {
            rowData->modify_hash = hash;
            schplugin( rowData )->GetAvailableSymbolFields( rowData->available_fields_cache );
        }

        return rowData->available_fields_cache;
    }

    return fields;
}


bool SYMBOL_LIBRARY_ADAPTER::SupportsSubLibraries( const wxString& aNickname ) const
{
    if( std::optional<const LIB_DATA*> result = fetchIfLoaded( aNickname ) )
    {
        const LIB_DATA* rowData = *result;
        std::lock_guard pluginGuard( pluginMutex( aNickname ) );
        return schplugin( rowData )->SupportsSubLibraries();
    }

    return false;
}


std::vector<SUB_LIBRARY> SYMBOL_LIBRARY_ADAPTER::GetSubLibraries( const wxString& aNickname ) const
{
    std::vector<SUB_LIBRARY> ret;

    if( std::optional<const LIB_DATA*> result = fetchIfLoaded( aNickname ) )
    {
        const LIB_DATA* rowData = *result;
        std::lock_guard pluginGuard( pluginMutex( aNickname ) );

        try
        {
            std::vector<wxString> names;
            schplugin( rowData )->GetSubLibraryNames( names );

            for( const wxString& name : names )
            {
                ret.emplace_back( SUB_LIBRARY {
                        .nickname = name,
                        .description = schplugin( rowData )->GetSubLibraryDescription( name ) } );
            }
        }
        catch( const IO_ERROR& e )
        {
            wxLogTrace( traceLibraries, "Sym: Exception getting sub-libraries for %s: %s",
                        aNickname, e.What() );
        }
    }

    return ret;
}


bool SYMBOL_LIBRARY_ADAPTER::SupportsConfigurationDialog( const wxString& aNickname ) const
{
    if( std::optional<const LIB_DATA*> result = fetchIfLoaded( aNickname ) )
    {
        std::lock_guard pluginGuard( pluginMutex( aNickname ) );
        return ( *result )->plugin->SupportsConfigurationDialog();
    }

    return false;
}


void SYMBOL_LIBRARY_ADAPTER::ShowConfigurationDialog( const wxString& aNickname, wxWindow* aParent ) const
{
    std::optional<const LIB_DATA*> optRow = fetchIfLoaded( aNickname );

    if( !optRow )
        return;

    DIALOG_SHIM* dialog = nullptr;

    {
        std::lock_guard pluginGuard( pluginMutex( aNickname ) );

        if( !( *optRow )->plugin->SupportsConfigurationDialog() )
            return;

        dialog = ( *optRow )->plugin->CreateConfigurationDialog( aParent );
    }

    if( !dialog )
        return;

    dialog->ShowModal();
}


int SYMBOL_LIBRARY_ADAPTER::GetModifyHash() const
{
    int hash = 0;

    for( const LIBRARY_TABLE_ROW* row : m_manager.Rows( Type() ) )
    {
        if( std::optional<const LIB_DATA*> result = fetchIfLoaded( row->Nickname() ) )
        {
            const LIB_DATA* rowData = *result;
            wxCHECK2( rowData->row, continue );
            std::lock_guard pluginGuard( pluginMutex( row->Nickname() ) );
            hash += schplugin( rowData )->GetModifyHash();
        }
    }

    return hash;
}


std::optional<int> SYMBOL_LIBRARY_ADAPTER::GetLibraryModifyHash( const wxString& aNickname ) const
{
    if( std::optional<const LIB_DATA*> result = fetchIfLoaded( aNickname ) )
    {
        const LIB_DATA* rowData = *result;
        wxCHECK( rowData->row, std::nullopt );
        std::lock_guard pluginGuard( pluginMutex( aNickname ) );
        return schplugin( rowData )->GetModifyHash();
    }

    return std::nullopt;
}
