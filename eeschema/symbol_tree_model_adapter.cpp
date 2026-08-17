/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2017 Chris Pavlina <pavlina.chris@gmail.com>
 * Copyright (C) 2014 Henner Zeller <h.zeller@acm.org>
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

#include "symbol_tree_model_adapter.h"

#include <wx/colour.h>
#include <wx/log.h>
#include <wx/tokenzr.h>
#include <wx/window.h>
#include <core/kicad_algo.h>
#include <pgm_base.h>
#include <settings/common_settings.h>
#include <project/project_file.h>
#include <widgets/wx_progress_reporters.h>
#include <dialogs/html_message_box.h>
#include <generate_alias_info.h>
#include <lib_symbol.h>
#include <sch_base_frame.h>
#include <locale_io.h>
#include <string_utils.h>
#include <trace_helpers.h>
#include <libraries/symbol_library_adapter.h>


namespace
{

class SYMBOL_NAME_TREE_ITEM : public LIB_TREE_ITEM
{
public:
    SYMBOL_NAME_TREE_ITEM( const wxString& aLibNickname, const SYMBOL_LIBRARY_ADAPTER::SYMBOL_INDEX& aIndex ) :
            m_libNickname( aLibNickname ),
            m_index( aIndex )
    {
        m_libId.SetLibNickname( m_libNickname );
        m_libId.SetLibItemName( m_index.m_Name );

        m_searchTerms.emplace_back( SEARCH_TERM( m_libNickname, 4 ) );
        m_searchTerms.emplace_back( SEARCH_TERM( m_index.m_Name, 8, true ) );
        m_searchTerms.emplace_back( SEARCH_TERM( m_libId.Format(), 16, true ) );

        wxStringTokenizer keywordTokenizer( m_index.m_KeyWords, " \t\r\n", wxTOKEN_STRTOK );

        while( keywordTokenizer.HasMoreTokens() )
            m_searchTerms.emplace_back( SEARCH_TERM( keywordTokenizer.GetNextToken(), 4 ) );

        m_searchTerms.emplace_back( SEARCH_TERM( m_index.m_KeyWords, 1 ) );
        m_searchTerms.emplace_back( SEARCH_TERM( m_index.m_Description, 1 ) );

        if( !m_index.m_Footprint.IsEmpty() )
            m_searchTerms.emplace_back( SEARCH_TERM( m_index.m_Footprint, 1 ) );
    }

    LIB_ID   GetLIB_ID() const override { return m_libId; }
    wxString GetName() const override { return m_index.m_Name; }
    wxString GetLibNickname() const override { return m_libNickname; }
    wxString GetDesc() override { return m_index.m_Description; }
    bool     IsPowerSymbol() const override { return m_index.m_IsPower; }
    wxString GetFootprint() override { return m_index.m_Footprint; }
    int      GetSubUnitCount() const override { return m_index.m_UnitCount; }
    wxString GetUnitName( int aUnit ) const override
    {
        if( m_index.m_UnitDisplayNames.contains( aUnit ) )
            return m_index.m_UnitDisplayNames.at( aUnit );

        return wxString::Format( _( "Unit %s" ), LIB_SYMBOL::LetterSubReference( aUnit, 'A' ) );
    }

    void GetChooserFields( std::map<wxString, wxString>& aColumnMap ) override { aColumnMap = m_index.m_ChooserFields; }

    std::vector<SEARCH_TERM>& GetSearchTerms() override { return m_searchTerms; }

private:
    wxString                             m_libNickname;
    SYMBOL_LIBRARY_ADAPTER::SYMBOL_INDEX m_index;
    LIB_ID                               m_libId;
    std::vector<SEARCH_TERM>             m_searchTerms;
};

} // namespace


wxObjectDataPtr<LIB_TREE_MODEL_ADAPTER>
SYMBOL_TREE_MODEL_ADAPTER::Create( SCH_BASE_FRAME* aParent, SYMBOL_LIBRARY_ADAPTER* aManager )
{
    auto* adapter = new SYMBOL_TREE_MODEL_ADAPTER( aParent, aManager );
    return wxObjectDataPtr<LIB_TREE_MODEL_ADAPTER>( adapter );
}


SYMBOL_TREE_MODEL_ADAPTER::SYMBOL_TREE_MODEL_ADAPTER( SCH_BASE_FRAME* aParent, SYMBOL_LIBRARY_ADAPTER* aLibs ) :
        LIB_TREE_MODEL_ADAPTER( aParent, "pinned_symbol_libs", aParent->GetViewerSettingsBase()->m_LibTree ),
        m_adapter( aLibs )
{
    m_colWidths[ GetDefaultFieldName( FIELD_T::VALUE, false ) ] = 300;
    m_colWidths[ GetDefaultFieldName( FIELD_T::FOOTPRINT, false ) ] = 600;

    m_availableColumns.emplace_back( GetDefaultFieldName( FIELD_T::VALUE, false ) );
    m_availableColumns.emplace_back( GetDefaultFieldName( FIELD_T::FOOTPRINT, false ) );
}


SYMBOL_TREE_MODEL_ADAPTER::~SYMBOL_TREE_MODEL_ADAPTER()
{}


void SYMBOL_TREE_MODEL_ADAPTER::loadColumnConfig()
{
    for( const std::pair<const wxString, int>& pair : m_cfg.column_widths )
        m_colWidths[pair.first] = pair.second;

    m_shownColumns = m_cfg.columns;

    if( m_shownColumns.empty() )
        m_shownColumns = {  _HKI( "Item" ), _HKI( "Description" ), GetDefaultFieldName( FIELD_T::VALUE, false ) };

    if( m_shownColumns[0] != _HKI( "Item" ) )
        m_shownColumns.insert( m_shownColumns.begin(), _HKI( "Item" ) );
}


void SYMBOL_TREE_MODEL_ADAPTER::AddLibraries( SCH_BASE_FRAME* aFrame )
{
    m_compatCache.clear();

    COMMON_SETTINGS* cfg = Pgm().GetCommonSettings();
    PROJECT_FILE&    project = aFrame->Prj().GetProjectFile();

    std::unordered_set<wxString> pinned;
    std::ranges::copy( cfg->m_Session.pinned_symbol_libs, std::inserter( pinned, pinned.begin() ) );
    std::ranges::copy( project.m_PinnedSymbolLibs, std::inserter( pinned, pinned.begin() ) );

    LIBRARY_MANAGER& manager = Pgm().GetLibraryManager();

    for( const LIBRARY_TABLE_ROW* row : manager.Rows( LIBRARY_TABLE_TYPE::SYMBOL ) )
    {
        if( row->Disabled() || row->Hidden() )
            continue;

        const wxString libName = row->Nickname();

        bool alreadyPresent = std::ranges::any_of( m_tree.m_Children,
                                                   [&]( const std::unique_ptr<LIB_TREE_NODE>& aNode )
                                                   {
                                                       return aNode->m_Name == libName;
                                                   } );

        if( alreadyPresent )
            continue;

        std::optional<LIB_STATUS> status = m_adapter->GetLibraryStatus( libName );

        if( status && ( status->load_status == LOAD_STATUS::LOAD_ERROR || status->error ) )
            continue;

        DoAddLibraryNode( libName, row->Description(), pinned.contains( libName ) );
    }

    m_tree.AssignIntrinsicRanks( m_shownColumns );
}


void SYMBOL_TREE_MODEL_ADAPTER::AddLibrary( wxString const& aLibNickname, bool pinned )
{
    std::optional<LIBRARY_TABLE_ROW*> row = m_adapter->GetRow( aLibNickname );

    if( !row || ( *row )->Disabled() || ( *row )->Hidden() )
        return;

    bool alreadyPresent = std::ranges::any_of( m_tree.m_Children,
                                               [&]( const std::unique_ptr<LIB_TREE_NODE>& aNode )
                                               {
                                                   return aNode->m_Name == aLibNickname;
                                               } );

    if( alreadyPresent )
        return;

    std::optional<LIB_STATUS> status = m_adapter->GetLibraryStatus( aLibNickname );

    if( status && ( status->load_status == LOAD_STATUS::LOAD_ERROR || status->error ) )
        return;

    DoAddLibraryNode( aLibNickname, ( *row )->Description(), pinned );
}


bool SYMBOL_TREE_MODEL_ADAPTER::IsContainer( const wxDataViewItem& aItem ) const
{
    const LIB_TREE_NODE* node = ToNode( aItem );
    return node ? node->m_Type == LIB_TREE_NODE::TYPE::LIBRARY || !node->m_Children.empty() : true;
}


void SYMBOL_TREE_MODEL_ADAPTER::EnsureChildrenLoaded( const wxDataViewItem& aItem )
{
    LIB_TREE_NODE* node = ToNode( aItem );

    if( !node || node->m_Type != LIB_TREE_NODE::TYPE::LIBRARY )
        return;

    populateLibrary( *static_cast<LIB_TREE_NODE_LIBRARY*>( node ), true );
}


void SYMBOL_TREE_MODEL_ADAPTER::EnsureAllChildrenLoaded()
{
    std::unique_ptr<ResetTreeView> resetGuard;

    if( m_widget )
        resetGuard = std::make_unique<ResetTreeView>( *this );

    bool loadedAny = false;

    for( std::unique_ptr<LIB_TREE_NODE>& lib : m_tree.m_Children )
    {
        if( lib->m_Type == LIB_TREE_NODE::TYPE::LIBRARY )
            loadedAny |= populateLibrary( *static_cast<LIB_TREE_NODE_LIBRARY*>( lib.get() ), false );
    }

    if( loadedAny )
    {
        createMissingColumns();
        m_tree.AssignIntrinsicRanks( m_shownColumns );
    }

    resetGuard.reset();

    if( loadedAny && m_lazyLoadHandler )
        m_lazyLoadHandler();
}


bool SYMBOL_TREE_MODEL_ADAPTER::populateLibrary( LIB_TREE_NODE_LIBRARY& aLibNode, bool aNotify )
{
    if( aLibNode.m_IsRecentlyUsedGroup || aLibNode.m_IsAlreadyPlacedGroup )
        return false;

    if( m_populated_libraries.count( aLibNode.m_Name ) )
        return false;

    try
    {
        std::optional<LIB_STATUS> status = m_adapter->GetLibraryStatus( aLibNode.m_Name );

        if( status && ( status->load_status == LOAD_STATUS::LOAD_ERROR || status->error ) )
            return false;

        if( !m_adapter->IsLibraryLoaded( aLibNode.m_Name ) )
        {
            status = m_adapter->LoadLibraryEntry( aLibNode.m_Name );

            if( !status || status->load_status != LOAD_STATUS::LOADED || status->error )
                return false;
        }

        wxDataViewItemArray                 addedItems;
        bool                                presorted = true;
        SYMBOL_LIBRARY_ADAPTER::SYMBOL_TYPE type = GetFilter() ? SYMBOL_LIBRARY_ADAPTER::SYMBOL_TYPE::POWER_ONLY
                                                               : SYMBOL_LIBRARY_ADAPTER::SYMBOL_TYPE::ALL_SYMBOLS;

        if( m_adapter->SupportsSubLibraries( aLibNode.m_Name ) )
        {
            std::vector<LIB_SYMBOL*> libSymbols = m_adapter->GetSymbols( aLibNode.m_Name, type );

            for( const wxString& column : m_adapter->GetAvailableExtraFields( aLibNode.m_Name ) )
                addColumnIfNecessary( column );

            for( LIB_SYMBOL* symbol : libSymbols )
            {
                if( !symbol )
                    continue;

                LIB_TREE_NODE_ITEM& item = aLibNode.AddItem( symbol );

                if( aNotify )
                    addedItems.Add( ToItem( &item ) );
            }

            presorted = false;
        }
        else
        {
            std::vector<SYMBOL_LIBRARY_ADAPTER::SYMBOL_INDEX> symbolIndex =
                    m_adapter->GetSymbolIndex( aLibNode.m_Name, type );

            for( const SYMBOL_LIBRARY_ADAPTER::SYMBOL_INDEX& symbol : symbolIndex )
            {
                for( const auto& chooserField : symbol.m_ChooserFields )
                    addColumnIfNecessary( chooserField.first );
            }

            for( const SYMBOL_LIBRARY_ADAPTER::SYMBOL_INDEX& symbol : symbolIndex )
            {
                SYMBOL_NAME_TREE_ITEM item( aLibNode.m_Name, symbol );
                LIB_TREE_NODE_ITEM&   treeItem = aLibNode.AddItem( &item );

                if( aNotify )
                    addedItems.Add( ToItem( &treeItem ) );
            }
        }

        aLibNode.AssignIntrinsicRanks( m_shownColumns, presorted );

        std::vector<std::unique_ptr<EDA_COMBINED_MATCHER>> matchers;
        aLibNode.UpdateScore( matchers, GetFilter() );

        m_populated_libraries.insert( aLibNode.m_Name );

        if( aNotify && m_widget )
        {
            createMissingColumns();

            if( addedItems.GetCount() )
                ItemsAdded( ToItem( &aLibNode ), addedItems );

            if( m_lazyLoadHandler )
                m_lazyLoadHandler();
        }

        return true;
    }
    catch( ... )
    {
        wxLogTrace( traceLibraries, "Sym: lazy tree population failed for %s", aLibNode.m_Name );
        return false;
    }
}


bool SYMBOL_TREE_MODEL_ADAPTER::GetAttr( const wxDataViewItem& aItem, unsigned int aCol,
                                         wxDataViewItemAttr& aAttr ) const
{
    bool base = LIB_TREE_MODEL_ADAPTER::GetAttr( aItem, aCol, aAttr );

    if( m_compatCallback && aCol == NAME_COL )
    {
        LIB_TREE_NODE* node = ToNode( aItem );

        if( node && node->m_Type == LIB_TREE_NODE::TYPE::ITEM )
        {
            auto it = m_compatCache.find( node->m_LibId );

            if( it == m_compatCache.end() )
            {
                std::vector<VARIANT_COMPAT_RESULT> issues = m_compatCallback( node->m_LibId );
                it = m_compatCache.emplace( node->m_LibId, !issues.empty() ).first;
            }

            if( it->second )
            {
                aAttr.SetColour( wxColour( 200, 120, 0 ) );
                return true;
            }
        }
    }

    return base;
}


wxString SYMBOL_TREE_MODEL_ADAPTER::GenerateInfo( LIB_ID const& aLibId, int aUnit )
{
    return GenerateAliasInfo( m_adapter, aLibId, aUnit );
}
