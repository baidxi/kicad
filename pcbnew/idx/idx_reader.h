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
    IDX_HOLDER( wxXmlDocument& aDoc ) { Read( aDoc ); };

    void Read( wxXmlDocument& aDoc );

    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>>   GetBoardOutlineElements() const;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>>   GetComponents() const;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>>   GetNPTH() const;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>>   GetPTH() const;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>>   GetOthers() const;
    std::vector<std::shared_ptr<idx_pdm::EDMD_KEEP_CONSTRAINT>> GetKeepZones() const;

    std::shared_ptr<idx_foundation::EDMD_DATASET> m_dataset;


private:
};


void IDX_HOLDER::Read( wxXmlDocument& aDoc )
{
    std::shared_ptr<IDX_XML_NODE> child = std::make_shared<IDX_XML_NODE_KICAD>( aDoc.GetRoot() );
    std::cout << "Trying to read IDX file..." << std::endl;

    m_dataset = std::make_shared<idx_foundation::EDMD_DATASET>( *child );
    m_dataset->Resolve( m_dataset->m_body.m_registry );


    for( const std::shared_ptr<idx_pdm::EDMD_ITEM>& i : m_dataset->m_body.m_items )
    {
        for( const std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>& inst : i->m_itemInstance )
        {
            bool handled = false;

            // Check for board outline
            if( auto type = inst->GetUserProperty( "TYPE" ) )
            {
                if( type->m_value == "BOARDOUTLINE" )
                {
                    m_dataset->m_body.m_boardOutline.push_back( inst );
                    handled = true;
                    continue;
                }
            }

            // Check for interstratum features
            auto interstratum = std::dynamic_pointer_cast<idx_pdm::EDMD_INTERSTRATUM_FEATURE>( inst->m_item->m_shape );
            if( interstratum )
            {
                using FT = idx_pdm::INTERSTRATUM_FEATURE_TYPE;
                switch( interstratum->m_interstratumFeatureType )
                {
                case FT::BOARD_CUTOUT:
                    m_dataset->m_body.m_boardOutline.push_back( inst );
                    handled = true;
                    continue;
                case FT::CUTOUT:
                    m_dataset->m_body.m_NPTH.push_back( inst );
                    handled = true;
                    continue;
                case FT::PLATED_CUTOUT:
                    m_dataset->m_body.m_PTH.push_back( inst );
                    handled = true;
                    continue;
                default: break;
                }
            }


            if( !inst->m_name.empty() )
            {
                m_dataset->m_body.m_components.push_back( inst );
                handled = true;
                continue;
            }

            // If not stored anywhere, push to m_others
            if( !handled )
                m_dataset->m_body.m_others.push_back( inst );
        }
    }
}


std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetBoardOutlineElements() const
{
    return m_dataset->m_body.m_boardOutline;
}

std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetNPTH() const
{
    return m_dataset->m_body.m_NPTH;
}

std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetComponents() const
{
    return m_dataset->m_body.m_components;
}


std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetPTH() const
{
    return m_dataset->m_body.m_PTH;
}


std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetOthers() const
{
    return m_dataset->m_body.m_others;
}


std::vector<std::shared_ptr<idx_pdm::EDMD_KEEP_CONSTRAINT>> IDX_HOLDER::GetKeepZones() const
{
    return m_dataset->m_body.m_keeps;
}

#endif