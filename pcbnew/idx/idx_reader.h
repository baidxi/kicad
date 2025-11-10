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


#include <idx/idx.h>
#include <iostream>
#include <board_commit.h>
#include <math/vector2d.h>
#include <pcb_shape.h>
#include <pcb_group.h>
#include <idx/CAD_software.h>


#ifndef IDX_READER_H
#define IDX_READER_H

#pragma once

class IDX_HOLDER
{
public:
    IDX_HOLDER( wxXmlDocument& aDoc )
    {
        Read( aDoc );
    };

    void Read( wxXmlDocument& aDoc );
    void ReadProcessInstruction( IDX_XML_NODE& aDoc );
    
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> GetBoardOutlineElements() const;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> GetComponents() const;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> GetNPTH() const;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> GetPTH() const;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> GetOthers() const;
    std::vector<std::shared_ptr<idx_pdm::EDMD_KEEP_CONSTRAINT>> GetKeepZones() const;

    std::shared_ptr<idx_foundation::EDMD_HEADER> m_header;
    std::shared_ptr<idx_foundation::EDMD_DATASET_BODY> m_dataset_body;


private:
};


void IDX_HOLDER::ReadProcessInstruction( IDX_XML_NODE& aNode )
{
    auto child2 = aNode.GetChildren();

    while( child2 )
    {
        const std::string content = child2->GetNodeContent();
        const std::string name = child2->GetName();
        
        if( false )
        {
        }
        else
        {
            std::cout << "unhandled body keyword:" << name << std::endl;
        }
        
        child2 = child2->GetNext();
    }
}


void IDX_HOLDER::Read( wxXmlDocument& aDoc )
{

    m_header = nullptr;

    std::shared_ptr<IDX_XML_NODE> child = std::make_shared<IDX_XML_NODE_KICAD>( aDoc.GetRoot()->GetChildren() );
    std::cout << "Trying to read IDX file..." << std::endl;

    while( child )
    {    
        std::string name = child->GetName();
        
        if ( IDX_keyword_compare( name, "foundation:Header" ) )
        {
            m_header = std::make_shared<idx_foundation::EDMD_HEADER>( *child, true );
        }
        else if ( IDX_keyword_compare( name, "foundation:Body" ) )
        {
            m_dataset_body = std::make_shared<idx_foundation::EDMD_DATASET_BODY>( *child );
        }
        else if ( IDX_keyword_compare( name, "foundation:ProcessInstruction" ) )
        {
            this->ReadProcessInstruction( *child );
        }
        else
        {
        }
        child = child->GetNext();
    }

    std::cout << "Resolving curves " << std::endl;
    for( auto& curve : m_dataset_body->m_curves )
        curve->Resolve( m_dataset_body->m_registry );
    std::cout << "Resolving curvesets " << std::endl;
    for( auto& curveset : m_dataset_body->m_curvesets )
        curveset->Resolve( m_dataset_body->m_registry );
    std::cout << "Resolving shapeElements " << std::endl;
    for( auto& shapeE : m_dataset_body->m_shapeElements )
        shapeE->Resolve( m_dataset_body->m_registry );
    std::cout << "Resolving stratum " << std::endl;
    for( auto& stratum : m_dataset_body->m_stratums )
        stratum->Resolve( m_dataset_body->m_registry );
    std::cout << "Resolving interstratum " << std::endl;
    for( auto& interstratum : m_dataset_body->m_interStratums )
        interstratum->Resolve( m_dataset_body->m_registry );
    std::cout << "Resolving keeps " << std::endl;
    for( auto& keep : m_dataset_body->m_keeps )
        keep->Resolve( m_dataset_body->m_registry );
    std::cout << "Resolving items " << std::endl;
    for( auto& item : m_dataset_body->m_items )
    {
        item->Resolve( m_dataset_body->m_registry );
    }
    std::cout << "Resolving functional shapes" << std::endl;
    for( auto& fs : m_dataset_body->m_functionalItemShapes )
    {
        fs->Resolve( m_dataset_body->m_registry );
    }


    for (const std::shared_ptr<idx_pdm::EDMD_ITEM>& i : m_dataset_body->m_items)
    {
        for (const std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>& inst : i->m_itemInstance)
        {
            bool handled = false;

            // Check for board outline
            if (auto type = inst->GetUserProperty("TYPE"))
            {
                if (type->m_value == "BOARDOUTLINE")
                {
                    m_dataset_body->m_boardOutline.push_back(inst);
                    handled = true;
                    continue;
                }
            }

            // Check for interstratum features
            auto interstratum = std::dynamic_pointer_cast<idx_pdm::EDMD_INTERSTRATUM_FEATURE>(inst->m_item->m_shape);
            if (interstratum)
            {
                using FT = idx_pdm::INTERSTRATUM_FEATURE_TYPE;
                switch (interstratum->m_interstratumFeatureType)
                {
                    case FT::BOARD_CUTOUT:
                        m_dataset_body->m_boardOutline.push_back(inst);
                        handled = true;
                        continue;
                    case FT::CUTOUT:
                        m_dataset_body->m_NPTH.push_back(inst);
                        handled = true;
                        continue;
                    case FT::PLATED_CUTOUT:
                        m_dataset_body->m_PTH.push_back(inst);
                        handled = true;
                        continue;
                    default:
                        break;
                }
            }


            if( !inst->m_name.empty() )
            {
                m_dataset_body->m_components.push_back(inst);
                handled = true;
                continue;
            }

            // If not stored anywhere, push to m_others
            if (!handled)
                m_dataset_body->m_others.push_back(inst);
        }
    }

    
}



std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetBoardOutlineElements() const
{
    return m_dataset_body->m_boardOutline;
}

std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetNPTH() const
{
    return m_dataset_body->m_NPTH; 
}

std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetComponents() const
{
    return m_dataset_body->m_components; 
}


std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetPTH() const
{
    return m_dataset_body->m_PTH;
}


std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetOthers() const
{
    return m_dataset_body->m_others;
}


std::vector<std::shared_ptr<idx_pdm::EDMD_KEEP_CONSTRAINT>> IDX_HOLDER::GetKeepZones() const
{
    return m_dataset_body->m_keeps;
}

#endif