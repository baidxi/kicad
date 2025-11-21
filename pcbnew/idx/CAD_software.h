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

#pragma once

#include <idx/kidx/schema/idx_base.h>
#include <idx/kidx/sync/sync.h>
#include <wx/string.h>
#include <wx/xml/xml.h>
#include <wx/string.h>
#include <wx/dir.h>
#include <iostream>
#include <memory>


class IDX_XML_NODE_KICAD : public IDX_XML_NODE
{
public:
    IDX_XML_NODE_KICAD( wxXmlNode* node );

    void AddAttribute( const std::string& name, const std::string& Value );

    std::string GetName() const override;
    std::string GetAttribute( const std::string& name, const std::string& defaultValue ) const override;
    void        SetAttribute( const std::string& name, const std::string& value ) override;
    std::string GetText() const override;
    void        SetText( const std::string& text ) override;
    std::shared_ptr<IDX_XML_NODE> GetParent() const override;
    std::shared_ptr<IDX_XML_NODE> GetChildren() const override;
    std::shared_ptr<IDX_XML_NODE> AppendChild( const std::string& name ) override;
    void                          RemoveChild( const std::shared_ptr<IDX_XML_NODE>& child ) override;
    std::string                   GetNodeContent() const override;
    std::shared_ptr<IDX_XML_NODE> GetNext() const override;

    wxXmlNode* m_node;
};


class IDX_XML_DOCUMENT_KICAD : public IDX_XML_DOCUMENT
{
public:
    IDX_XML_DOCUMENT_KICAD( wxXmlDocument* );


private:
    wxXmlDocument* m_doc;
};


class IDX_FILE_REGISTRY_KICAD : public IDX_FILE_REGISTRY
{
public:
    IDX_FILE_REGISTRY_KICAD() {};

    bool Load( const std::string& aPath, const std::string& aFile );
};


class KIDX_REGISTRY_KICAD : public KIDX_REGISTRY
{
public:
    KIDX_REGISTRY_KICAD( const std::string& aFile );
    bool LoadFromFile( const std::string& aFile );

    bool LoadFolder( const wxString& aFolder );
    void Save() override;
};