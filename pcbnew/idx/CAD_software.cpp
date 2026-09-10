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
#include <idx/kidx/kidx.h>

// ----------------- IDX_BASE -----------------
void IDX_BASE::Report( std::string aMsg, IDX_REPORT_LEVEL aLvl ) const
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
    return m_node != nullptr ? m_node->GetName().ToStdString() : "";
}


void IDX_XML_NODE_KICAD::SetName( const std::string& aName )
{
    if( !m_node )
        return;

    m_node->SetName( aName );
}


void IDX_XML_NODE_KICAD::AddAttribute( const std::string& aName, const std::string& aValue )
{
    if( !m_node )
        return;
    m_node->AddAttribute( aName, aValue );
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


IDX_XML_DOCUMENT_KICAD::IDX_XML_DOCUMENT_KICAD( wxXmlDocument* aDoc ) :
        m_doc( aDoc )
{
}

bool IDX_FILE_REGISTRY_KICAD::Load( const std::string& aPath, const std::string& aFile )
{
    wxXmlDocument doc;
    wxString      fullpath = wxString::FromUTF8( aPath.c_str() );
    fullpath += wxFILE_SEP_PATH;
    fullpath += wxString::FromUTF8( aFile.c_str() );

    if( !doc.Load( fullpath ) )
        return false;


    std::shared_ptr<IDX_XML_NODE> child = std::make_shared<IDX_XML_NODE_KICAD>( doc.GetRoot() );

    std::shared_ptr<idx_foundation::EDMD_DATA_SET> dataset =
            std::make_shared<idx_foundation::EDMD_DATA_SET>( *child, true );


    if( !dataset )
        return false;

    m_soft = dataset->m_header.m_creatorSystem;
    m_path = fullpath;
    m_name = aFile;
    m_author = dataset->m_header.m_creatorName;
    m_baseline = dataset->IsBaseline();
    m_date = dataset->m_header.m_creationDateTime;

    return true;
}


bool KIDX_REGISTRY_KICAD::LoadFolder( const wxString& aFolder )
{
    auto folder = std::make_shared<wxDir>( aFolder );

    if( !folder->IsOpened() )
        return false;

    m_path = aFolder.ToStdString();

    wxString filename;
    bool     cont = folder->GetFirst( &filename, "*.idx", wxDIR_FILES );

    while( cont )
    {
        std::shared_ptr<IDX_FILE_REGISTRY_KICAD> file = std::make_shared<IDX_FILE_REGISTRY_KICAD>();
        file->Load( aFolder.ToStdString(), filename.ToStdString() );

        if( !IsFileKnown( file ) )
            m_files.push_back( file );

        cont = folder->GetNext( &filename );
    }


    return true;
}

#include <wx/xml/xml.h>
#include <wx/filefn.h>
#include <wx/wfstream.h>
#include <wx/stdpaths.h>
#include <iostream>


KIDX_REGISTRY_KICAD::KIDX_REGISTRY_KICAD( const std::string& aFile ) :
        KIDX_REGISTRY( aFile )
{
    LoadFromFile( aFile );
}

bool KIDX_REGISTRY_KICAD::LoadFromFile( const std::string& aFile )
{
    wxXmlDocument doc;

    // Ensure the file exists
    if( !wxFileExists( aFile ) )
    {
        std::cerr << "File does not exist: " << aFile << std::endl;
        return false;
    }

    wxFileInputStream input( aFile );
    if( !input.IsOk() )
    {
        std::cerr << "Cannot open file: " << aFile << std::endl;
        return false;
    }

    // Load XML
    if( !doc.Load( input ) )
    {
        std::cerr << "Failed to parse XML file: " << aFile << std::endl;
        return false;
    }

    // Root element <IDX_register>
    wxXmlNode* root = doc.GetRoot();
    if( !root || root->GetName() != "IDX_register" )
    {
        std::cerr << "Invalid XML root node" << std::endl;
        return false;
    }

    // Example: read version attribute
    wxString version = root->GetAttribute( "version", "unknown" );
    std::cout << "Registry version: " << version << std::endl;

    // Parse children (<folder>, <file>, ...)
    for( wxXmlNode* child = root->GetChildren(); child; child = child->GetNext() )
    {
        if( child->GetName() == "folder" )
        {
            m_path = child->GetAttribute( "path", "" );
        }
        else if( child->GetName() == "file" )
        {
            m_files.push_back( std::make_shared<IDX_FILE_REGISTRY>() );
            m_files.back()->SetName( child->GetAttribute( "name", "" ).ToStdString() );
            m_files.back()->SetDate( child->GetAttribute( "date", "" ).ToStdString() );
            m_files.back()->SetDate( child->GetAttribute( "date", "" ).ToStdString() );
            m_files.back()->SetSoft( child->GetAttribute( "software", "" ).ToStdString() );
            m_files.back()->SetStatus( IDX_FILE_REGISTRY::STATUS::IDX_PROCESSED );
        }
    }

    return true;
}

void KIDX_REGISTRY_KICAD::Save()
{
    // Create the root XML document + root node
    wxXmlDocument doc;
    wxXmlNode*    root = new wxXmlNode( wxXML_ELEMENT_NODE, "IDX_register" );
    auto          root_idx = std::make_shared<IDX_XML_NODE_KICAD>( root );
    doc.SetRoot( root );

    this->writeXml( root_idx );
    // Save the XML file
    wxFileOutputStream output( m_file );
    if( !output.IsOk() )
    {
        std::cerr << "Error: Cannot open file for writing: " << m_file << std::endl;
        return;
    }

    if( !doc.Save( output ) )
    {
        std::cerr << "Error: Failed to save XML to " << m_file << std::endl;
        return;
    }

    std::cout << "Saved XML successfully." << std::endl;
}
