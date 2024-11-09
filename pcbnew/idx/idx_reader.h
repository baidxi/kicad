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

#ifndef IDX_READER_H
#define IDX_READER_H

#include <idx/idx.h>
#include <iostream>

class IDX_HOLDER
{
public:
    IDX_HOLDER( wxXmlDocument& aDoc )
    {
        ReadHeader( aDoc );
        ReadBody( aDoc );

        std::cout << "List of systems " << std::endl;

        for( auto s : m_systems )
        {
            std::cout << s.m_id << std::endl;
            std::cout << s.m_globalName << std::endl;
        }
    };

    void ReadHeader( wxXmlDocument& aDoc );
    void ReadBody( wxXmlDocument& aDoc );

    void AddEDMDObject( wxXmlNode& aNode );
    void AddEDMDSystem( wxXmlNode& aNode );


    std::shared_ptr<idx_foundation::EDMD_HEADER> m_header;
    std::vector<idx_foundation::EDMD_SYSTEM>     m_systems;
    std::vector<idx_property::EDMD_UNIT>         m_units;
};

void IDX_HOLDER::ReadHeader( wxXmlDocument& aDoc )
{
    m_header = nullptr;

    wxXmlNode* child = aDoc.GetRoot()->GetChildren();
    std::cout << "Trying to read header..." << std::endl;

    while( child )
    {
        std::cout << child->GetName() << std::endl;

        if( child->GetName() == "foundation:Header" )
        {
            m_header = std::make_shared<idx_foundation::EDMD_HEADER>( *child );
            break;
        }

        child = child->GetNext();
    }
}


void IDX_HOLDER::ReadBody( wxXmlDocument& aDoc )
{
    wxXmlNode* child = aDoc.GetRoot()->GetChildren();
    std::cout << "Trying to read body..." << std::endl;

    while( child )
    {
        std::cout << child->GetName() << std::endl;

        if( child->GetName() == "foundation:Body" )
        {
            wxXmlNode* child2 = child->GetChildren();

            while( child2 )
            {
                const wxString content = child2->GetNodeContent();
                const wxString name = child2->GetName();

                if( name == "foundation:System" )
                    m_systems.emplace_back( *child2 );
                else if( name == "foundation:UnitLength" )
                    m_units.emplace_back( *child2 );
                else
                {
                    std::cout << "unhandled keyword:" << name << std::endl;
                }
                child2 = child2->GetNext();
            }
            break;
        }

        child = child->GetNext();
    }
}


void IDX_HOLDER::AddEDMDObject( wxXmlNode& aNode )
{
}

#endif