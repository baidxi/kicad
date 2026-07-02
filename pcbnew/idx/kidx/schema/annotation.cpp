/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2024 KiCad Developers, see AUTHORS.txt for contributors.
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
 * along with this program; if not, you may write to the Free Software Foundation, Inc.,
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <idx/kidx/schema/annotation.h>
#include <iostream>

namespace idx_annotation
{

// ------------------- ARRAY_OF_EDMD_NAME -------------------

ARRAY_OF_EDMD_NAME::ARRAY_OF_EDMD_NAME( IDX_XML_NODE& aNode, bool aTopLevel ) :
        IDX_BASE()
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool ARRAY_OF_EDMD_NAME::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "annotation:EDMDName" ) )
        {
            m_EDMDName.emplace_back( std::make_shared<idx_foundation::EDMD_NAME>( *child, true ) );
        }
        else
        {
            unknownKeywordIDX( name );
        }
        child = child->GetNext();
    }
    return true;
}

// ------------------- EDMD_ANNOTATION -------------------

EDMD_ANNOTATION::EDMD_ANNOTATION( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_ANNOTATION::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "annotation:Subject" ) )
        {
            m_subject = content;
        }
        else if( IDX_keyword_compare( name, "annotation:Creator" ) )
        {
            m_creator = content;
        }
        else if( IDX_keyword_compare( name, "annotation:RelatedAnnotation" ) )
        {
            m_relatedAnnotation.push_back( content );
        }
        else if( IDX_keyword_compare( name, "annotation:Caption" ) )
        {
            m_caption = content;
        }
        else if( IDX_keyword_compare( name, "annotation:Text" ) )
        {
            m_text = content;
        }
        else if( IDX_keyword_compare( name, "annotation:Reference" ) )
        {
            m_reference.push_back( content );
        }
        else
        {
            return this->idx_foundation::EDMD_COLLABORATION_OBJECT::Parse( aNode );
        }
        child = child->GetNext();
    }
    return true;
}

// ------------------- EDMD_HISTORY_ENTRY -------------------

EDMD_HISTORY_ENTRY::EDMD_HISTORY_ENTRY( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_ANNOTATION( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_HISTORY_ENTRY::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "annotation:History" ) )
        {
            m_history = content;
        }
        else if( IDX_keyword_compare( name, "annotation:CreationDateTime" ) )
        {
            m_creationDateTime = content;
        }
        else if( IDX_keyword_compare( name, "annotation:SystemScope" ) )
        {
            m_systemScope = content;
        }
        else
        {
            return this->EDMD_ANNOTATION::Parse( aNode );
        }
        child = child->GetNext();
    }
    return true;
}

// ------------------- EDMD_HISTORY_ENTRY_CHANGE_ITEM_INSTANCE -------------------

EDMD_HISTORY_ENTRY_CHANGE_ITEM_INSTANCE::EDMD_HISTORY_ENTRY_CHANGE_ITEM_INSTANCE( IDX_XML_NODE& aNode,
                                                                                  bool          aTopLevel ) :
        EDMD_HISTORY_ENTRY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

// ------------------- EDMD_HISTORY_ENTRY_ADD_COMMENT -------------------

EDMD_HISTORY_ENTRY_ADD_COMMENT::EDMD_HISTORY_ENTRY_ADD_COMMENT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_HISTORY_ENTRY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

// ------------------- EDMD_HISTORY_ENTRY_CHANGE_OWNERSHIP -------------------

EDMD_HISTORY_ENTRY_CHANGE_OWNERSHIP::EDMD_HISTORY_ENTRY_CHANGE_OWNERSHIP( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_HISTORY_ENTRY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_HISTORY_ENTRY_CHANGE_OWNERSHIP::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "annotation:OldOwner" ) )
        {
            m_oldOwner = content;
        }
        else
        {
            return this->EDMD_HISTORY_ENTRY::Parse( aNode );
        }
        child = child->GetNext();
    }
    return true;
}

// ------------------- EDMD_HISTORY_ENTRY_ADD_ITEM_INSTANCE -------------------

EDMD_HISTORY_ENTRY_ADD_ITEM_INSTANCE::EDMD_HISTORY_ENTRY_ADD_ITEM_INSTANCE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_HISTORY_ENTRY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

// ------------------- EDMD_HISTORY_ENTRY_CHANGE_ACCEPTANCE -------------------

EDMD_HISTORY_ENTRY_CHANGE_ACCEPTANCE::EDMD_HISTORY_ENTRY_CHANGE_ACCEPTANCE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_HISTORY_ENTRY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

// ------------------- EDMD_HISTORY -------------------

EDMD_HISTORY::EDMD_HISTORY() :
        idx_foundation::EDMD_COLLABORATION_OBJECT()
{
}

EDMD_HISTORY::EDMD_HISTORY( IDX_XML_NODE& aNode ) :
        idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
    ParseChildren( aNode );
}

EDMD_HISTORY::EDMD_HISTORY( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_HISTORY::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "annotation:HistoryName" ) )
        {
            m_historyName = ARRAY_OF_EDMD_NAME( *child, true );
        }
        else if( IDX_keyword_compare( name, "annotation:Subject" ) )
        {
            m_subject = content;
        }
        else
        {
            return this->idx_foundation::EDMD_COLLABORATION_OBJECT::Parse( aNode );
        }
        child = child->GetNext();
    }
    return true;
}


void EDMD_HISTORY::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_COLLABORATION_OBJECT::Write( aNode );

    // Write HistoryName
    if( !m_historyName.m_EDMDName.empty() )
    {
        std::shared_ptr<IDX_XML_NODE> historyNameNode = aNode.AppendChild( "annotation:HistoryName" );
        for( const auto& name : m_historyName.m_EDMDName )
        {
            if( name )
                name->Write( *historyNameNode );
        }
    }

    // Write Subject
    if( !m_subject.empty() )
        aNode.AppendChild( "annotation:Subject" )->SetText( m_subject );
}


} // namespace idx_annotation
