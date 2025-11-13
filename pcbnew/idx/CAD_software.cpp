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
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "CAD_software.h"

// ----------------- IDX_BASE -----------------
void IDX_BASE::Report( std::string aMsg, IDX_REPORT_LEVEL aLvl )
{
    wxString ws( aMsg.c_str(), wxConvUTF8 );
    switch( aLvl )
    {
    case IDX_REPORT_LEVEL::IDX_INFO: std::cout << "INFO: "; break;
    case IDX_REPORT_LEVEL::IDX_ACTION: std::cout << "ACTION: "; break;
    case IDX_REPORT_LEVEL::IDX_WARNING: std::cout << "WARNING: "; break;
    case IDX_REPORT_LEVEL::IDX_ERROR: std::cout << "ERROR: "; break;
    }
    std::cout << aMsg << std::endl;
}

// ----------------- IDX_XML_NODE_KICAD -----------------
IDX_XML_NODE_KICAD::IDX_XML_NODE_KICAD( wxXmlNode* aNode ) :
        m_node( aNode )
{
}

std::string IDX_XML_NODE_KICAD::GetName() const
{
    return m_node != nullptr ? m_node->GetName().ToStdString() : "nullptr";
}

std::string IDX_XML_NODE_KICAD::GetAttribute( const std::string& name, const std::string& defaultValue ) const
{
    if( !m_node )
        return "";
    return m_node->GetAttribute( name, defaultValue ).ToStdString();
}

void IDX_XML_NODE_KICAD::SetAttribute( const std::string& name, const std::string& value )
{
    if( m_node )
        m_node->AddAttribute( name, value );
}

std::string IDX_XML_NODE_KICAD::GetText() const
{
    if( !m_node )
        return "";
    wxXmlNode* child = m_node->GetChildren();
    if( child && child->GetType() == wxXML_TEXT_NODE )
        return child->GetContent().ToStdString();
    return "";
}

void IDX_XML_NODE_KICAD::SetText( const std::string& text )
{
    if( !m_node )
        return;
    wxXmlNode* child = m_node->GetChildren();
    if( child && child->GetType() == wxXML_TEXT_NODE )
        child->SetContent( text );
    else
        m_node->AddChild( new wxXmlNode( wxXML_TEXT_NODE, "", text ) );
}

std::shared_ptr<IDX_XML_NODE> IDX_XML_NODE_KICAD::GetParent() const
{
    return nullptr;
}

std::shared_ptr<IDX_XML_NODE> IDX_XML_NODE_KICAD::GetChildren() const
{
    if( !m_node )
        return nullptr;
    wxXmlNode* firstChild = m_node->GetChildren();
    if( !firstChild )
        return nullptr;
    return std::make_shared<IDX_XML_NODE_KICAD>( firstChild );
}

std::shared_ptr<IDX_XML_NODE> IDX_XML_NODE_KICAD::AppendChild( const std::string& name )
{
    if( !m_node )
        return nullptr;
    auto* newChild = new wxXmlNode( wxXML_ELEMENT_NODE, name );
    m_node->AddChild( newChild );
    return std::make_shared<IDX_XML_NODE_KICAD>( newChild );
}

void IDX_XML_NODE_KICAD::RemoveChild( const std::shared_ptr<IDX_XML_NODE>& child )
{
    auto wxChild = std::dynamic_pointer_cast<IDX_XML_NODE_KICAD>( child );
    if( m_node && wxChild && wxChild->m_node )
        m_node->RemoveChild( wxChild->m_node );
}

std::string IDX_XML_NODE_KICAD::GetNodeContent() const
{
    return m_node != nullptr ? m_node->GetNodeContent().ToStdString() : "nullptr";
}

std::shared_ptr<IDX_XML_NODE> IDX_XML_NODE_KICAD::GetNext() const
{
    if( !m_node || !m_node->GetNext() )
        return nullptr;
    return std::make_shared<IDX_XML_NODE_KICAD>( m_node->GetNext() );
}
