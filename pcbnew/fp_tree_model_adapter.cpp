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

#include <pgm_base.h>
#include <pcb_base_frame.h>
#include <core/kicad_algo.h>
#include <settings/common_settings.h>
#include <pcbnew_settings.h>
#include <project/project_file.h>
#include <wx/tokenzr.h>
#include <string_utils.h>
#include <footprint_library_adapter.h>
#include <footprint_info_impl.h>
#include <generate_footprint_info.h>
#include <trace_helpers.h>
#include <memory>

#include "fp_tree_model_adapter.h"

wxObjectDataPtr<LIB_TREE_MODEL_ADAPTER>
FP_TREE_MODEL_ADAPTER::Create( PCB_BASE_FRAME* aParent, FOOTPRINT_LIBRARY_ADAPTER* aLibs )
{
    auto* adapter = new FP_TREE_MODEL_ADAPTER( aParent, aLibs );
    return wxObjectDataPtr<LIB_TREE_MODEL_ADAPTER>( adapter );
}


FP_TREE_MODEL_ADAPTER::FP_TREE_MODEL_ADAPTER( PCB_BASE_FRAME* aParent, FOOTPRINT_LIBRARY_ADAPTER* aLibs ) :
        LIB_TREE_MODEL_ADAPTER( aParent, wxT( "pinned_footprint_libs" ),
                                aParent->GetViewerSettingsBase()->m_LibTree ),
        m_libs( aLibs )
{}


void FP_TREE_MODEL_ADAPTER::AddLibraries( EDA_BASE_FRAME* aParent )
{
    COMMON_SETTINGS* cfg = Pgm().GetCommonSettings();
    PROJECT_FILE&    project = aParent->Prj().GetProjectFile();

    for( const LIBRARY_TABLE_ROW* row : m_libs->Rows() )
    {
        wxString libName = row->Nickname();

        if( row->Disabled() || row->Hidden() )
            continue;

        std::optional<LIB_STATUS> libStatus = m_libs->GetLibraryStatus( libName );

        if( libStatus && ( libStatus->load_status == LOAD_STATUS::LOAD_ERROR || libStatus->error ) )
            continue;

        bool pinned = alg::contains( cfg->m_Session.pinned_fp_libs, libName )
                        || alg::contains( project.m_PinnedFootprintLibs, libName );

        DoAddLibraryNode( libName, row->Description(), pinned );
    }

    m_tree.AssignIntrinsicRanks( m_shownColumns );
}


bool FP_TREE_MODEL_ADAPTER::IsContainer( const wxDataViewItem& aItem ) const
{
    const LIB_TREE_NODE* node = ToNode( aItem );
    return node ? node->m_Type == LIB_TREE_NODE::TYPE::LIBRARY : true;
}


void FP_TREE_MODEL_ADAPTER::EnsureChildrenLoaded( const wxDataViewItem& aItem )
{
    LIB_TREE_NODE* node = ToNode( aItem );

    if( !node || node->m_Type != LIB_TREE_NODE::TYPE::LIBRARY )
        return;

    populateLibrary( *static_cast<LIB_TREE_NODE_LIBRARY*>( node ), true );
}


void FP_TREE_MODEL_ADAPTER::EnsureAllChildrenLoaded()
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

    resetGuard.reset();

    if( loadedAny && m_lazyLoadHandler )
        m_lazyLoadHandler();
}


bool FP_TREE_MODEL_ADAPTER::populateLibrary( LIB_TREE_NODE_LIBRARY& aLibNode, bool aNotify )
{
    if( m_populatedLibs.count( aLibNode.m_Name ) )
        return false;

    try
    {
        if( !m_libs->IsLibraryLoaded( aLibNode.m_Name ) )
        {
            std::optional<LIB_STATUS> status = m_libs->LoadLibraryEntry( aLibNode.m_Name );

            if( !status || status->load_status != LOAD_STATUS::LOADED || status->error )
                return false;
        }

        m_libs->RefreshLibraryIfChanged( aLibNode.m_Name );

        std::vector<wxString> footprintNames = m_libs->GetFootprintNames( aLibNode.m_Name, true );
        wxDataViewItemArray   addedItems;

        for( const wxString& footprintName : footprintNames )
        {
            FOOTPRINT_INFO_IMPL fpInfo( aLibNode.m_Name, footprintName );
            LIB_TREE_NODE_ITEM& item = aLibNode.AddItem( &fpInfo );

            if( aNotify )
                addedItems.Add( ToItem( &item ) );
        }

        aLibNode.AssignIntrinsicRanks( m_shownColumns, true );

        std::vector<std::unique_ptr<EDA_COMBINED_MATCHER>> matchers;
        aLibNode.UpdateScore( matchers, GetFilter() );

        m_populatedLibs.insert( aLibNode.m_Name );

        if( aNotify && m_widget && addedItems.GetCount() )
            ItemsAdded( ToItem( &aLibNode ), addedItems );

        if( aNotify && m_lazyLoadHandler )
            m_lazyLoadHandler();

        return true;
    }
    catch( ... )
    {
        wxLogTrace( traceLibraries, "FP: lazy tree population failed for %s", aLibNode.m_Name );
        return false;
    }
}


wxString FP_TREE_MODEL_ADAPTER::GenerateInfo( LIB_ID const& aLibId, int aUnit )
{
    return GenerateFootprintInfo( m_libs, aLibId );
}
