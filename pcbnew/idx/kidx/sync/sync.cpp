/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2025 KiCad Developers, see AUTHORS.txt for contributors.
 * @author Fabien Corona <f.corona<at>laposte.net>
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

#include "sync.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>


KIDX_REGISTRY::KIDX_REGISTRY( const std::string& aFile ) :
        m_file( aFile )
{
    std::ifstream in( aFile );
    m_baselined = in.good();
}

bool KIDX_REGISTRY::IsFileKnown( const std::shared_ptr<IDX_FILE_REGISTRY> aFile ) const
{
    if( !aFile )
        return false;

    for( const auto& file : m_files )
    {
        if( *file == *aFile )
            return true;
    }
    return false;
}


bool KIDX_REGISTRY::AddFile( const std::shared_ptr<IDX_FILE_REGISTRY> aFile )
{
    if( !aFile )
        return false;

    m_files.push_back( aFile );
    return true;
}


void KIDX_REGISTRY::ComputeNext()
{
    for( std::shared_ptr<IDX_FILE_REGISTRY> file : GetFiles() )
    {
        if( !m_baselined )
            file->m_next = file->IsBaseline();
        else
        {
            file->m_next = !file->IsBaseline();
        }
    }
}

void KIDX_REGISTRY::writeXml( std::shared_ptr<IDX_XML_NODE> aRoot )
{
    if( !aRoot )
        return;

    aRoot->SetAttribute( "version", "1.0" );


    auto child = aRoot->AppendChild( "folder" );
    child->SetAttribute( "path", this->GetPath() );

    for( auto file : m_files )
    {
        if( file->GetStatus() == IDX_FILE_REGISTRY::STATUS::IDX_UNPROCESSED )
            continue;

        auto child2 = aRoot->AppendChild( "file" );
        child2->SetAttribute( "name", file->GetName() );
        child2->SetAttribute( "author", file->GetAuthor() );
        child2->SetAttribute( "date", file->GetDate() );
        child2->SetAttribute( "software", file->GetSoft() );
    }

    auto ds = aRoot->AppendChild( "repository" );

    for( auto& item : m_itemRepository )
    {
        if( item.m_shape )
        {
            auto i = ds->AppendChild( "" );
            item.m_shape->Write( *i );
        }

        auto i = ds->AppendChild( "" );
        item.Write( *i );
    }
}

bool KIDX_REGISTRY::isIdentifierKnown( const idx_foundation::EDMD_IDENTIFIER& aId ) const
{
    for( const auto& id : m_identifiers )
    {
        if( id == aId )
            return true;
    }
    return false;
}

void KIDX_REGISTRY::addIdentifier( const idx_foundation::EDMD_IDENTIFIER& aId )
{
    m_identifiers.push_back( aId );
}


bool KIDX_REGISTRY::SC_Process( idx_computational::EDMD_CHANGE& aChange )
{
    if( !SC_1CheckIdentifierOfNewItem( aChange ) )
        return false;

    if( !SC_2CheckIdentifierOfPredecessorItem( aChange ) )
    {
        return false;
        // TODO Implement the request for information message...
    }

    return true;
}


bool KIDX_REGISTRY::SC_1CheckIdentifierOfNewItem( const idx_computational::EDMD_CHANGE& aChange ) const
{
    if( !aChange.m_newItem )
        return false;

    return this->isIdentifierKnown( *( aChange.m_newItem ) );
}

bool KIDX_REGISTRY::SC_2CheckIdentifierOfPredecessorItem( const idx_computational::EDMD_CHANGE& aChange ) const
{
    if( !aChange.m_predecessorItem )
        return false;

    return this->isIdentifierKnown( *( aChange.m_predecessorItem ) );
}