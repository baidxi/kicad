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
    void ReadBody( wxXmlNode& aDoc );
    void AddElements( BOARD_COMMIT& aCommit );
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>>  GetBoardOutlineElements();
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> GetNPTH();
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> GetPTH();
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> GetOthers();
    std::vector<std::shared_ptr<idx_pdm::EDMD_KEEP_CONSTRAINT>> GetKeepZones();



    std::shared_ptr<idx_foundation::EDMD_HEADER> m_header;
    std::vector<std::shared_ptr<idx_administration::EDMD_ACTOR>> m_actors;
    std::vector<std::shared_ptr<idx_foundation::EDMD_SYSTEM>>    m_systems;
    std::vector<std::shared_ptr<idx_property::EDMD_UNIT>>        m_units;
    std::vector<std::shared_ptr<idx_geometry::EDMD_POINT>>       m_points;
    std::vector<std::shared_ptr<idx_geometry::EDMD_CURVE>>       m_curves;
    std::vector<std::shared_ptr<idx_geometry::EDMD_CURVESET2D>>  m_curvesets;
    std::vector<std::shared_ptr<idx_pdm::EDMD_SHAPE_ELEMENT>>    m_shapeElements;
    std::vector<std::shared_ptr<idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE>>         m_functionalItemShapes;
    std::vector<std::shared_ptr<idx_pdm::EDMD_STRATUM>>          m_stratums;
    std::vector<std::shared_ptr<idx_pdm::EDMD_INTERSTRATUM_FEATURE>>          m_interStratums;
    std::vector<std::shared_ptr<idx_pdm::EDMD_KEEP_CONSTRAINT>>  m_keeps;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM>>             m_items;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ASSEMBLY_COMPONENT>> m_assemblies;


    IDX_REGISTRY m_registry;


    void print( int aIndex=0, int aMaxIndex=5 )
    {
        if ( aIndex >= aMaxIndex )
            return;

        for ( int i = 0; i < aIndex ; i++ )
            std::cout << "\t";

        std:: cout << "--> IDX_file" << std::endl;

        for ( auto inst: m_items )
        {
            inst->print( aIndex+1 );
        }
    }

private:
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> m_boardOutline;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> m_NPTH;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> m_PTH;
    std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> m_others;
};


    
void IDX_HOLDER::AddElements( BOARD_COMMIT& aCommit )
{
    for( auto const& x : m_curves )
    {
        std::shared_ptr<idx_geometry::EDMD_POLYLINE> polyline =
                std::dynamic_pointer_cast<idx_geometry::EDMD_POLYLINE>( x );

        if( polyline )
        {
            std::vector<VECTOR2I> polypoints;

            for( std::shared_ptr<idx_geometry::EDMD_POINT> p : polyline->m_point )
            {
                if( p )
                    polypoints.emplace_back( p->GetX(), p->GetY() );
            }
            PCB_SHAPE* s = new PCB_SHAPE();

            PCB_GROUP* group = new PCB_GROUP( s );
            group->SetName( polyline->m_id );
            aCommit.Add( group );

            for( size_t i = 0; i < polypoints.size() - 1; i++ )
            {
                PCB_SHAPE* s = new PCB_SHAPE();
                s->SetWidth( polyline->m_thickness.m_value * 1e6 );
                s->SetStart( polypoints[i] );
                s->SetEnd( polypoints[i + 1] );
                group->AddItem( s );
                aCommit.Stage( s, CHT_ADD );
                aCommit.Add( s );
            }


            continue;
        }

        std::shared_ptr<idx_geometry::EDMD_CIRCLE> circle =
                std::dynamic_pointer_cast<idx_geometry::EDMD_CIRCLE>( x  );

        if( circle && circle->GetCenter() )
        {
            PCB_SHAPE* s = new PCB_SHAPE( nullptr, SHAPE_T::CIRCLE );
            s->SetCenter( VECTOR2I( circle->GetCenter()->GetX(), circle->GetCenter()->GetY() ) );
            s->SetRadius( circle->GetRadius() );
            aCommit.Add( s );

            continue;
        }
    }
    aCommit.Push( "Import IDX" );
}

void IDX_HOLDER::ReadBody( wxXmlNode& aNode )
{
    wxXmlNode* child2 = aNode.GetChildren();

    while( child2 )
    {
        const wxString content = child2->GetNodeContent();
        const wxString name = child2->GetName();
        
        if( name == "foundation:System" )
        {
            std::shared_ptr<idx_foundation::EDMD_SYSTEM> s =
                    std::make_shared<idx_foundation::EDMD_SYSTEM>( *child2 );
            m_systems.push_back( s );
        }
        else if( name == "foundation:History" )
        {
            std::shared_ptr<idx_annotation::EDMD_HISTORY> s =
                    std::make_shared<idx_annotation::EDMD_HISTORY>( *child2 );
        }
        else if( name == "foundation:Person" )
        {
            std::shared_ptr<idx_administration::EDMD_PERSON> s =
                    std::make_shared<idx_administration::EDMD_PERSON>( *child2 );
            m_actors.push_back( s );
        }
        else if( name == "foundation:UnitLength" )
        {
            std::shared_ptr<idx_property::EDMD_UNIT> s =
                    std::make_shared<idx_property::EDMD_UNIT>( *child2 );
            m_units.push_back( s );
        }
        else if( name == "foundation:CartesianPoint" )
        {
            std::shared_ptr<idx_geometry::EDMD_CARTESIAN_POINT> s =
                    std::make_shared<idx_geometry::EDMD_CARTESIAN_POINT>( *child2, true );
            m_registry.registerObject<idx_geometry::EDMD_POINT>( s->m_id, s );
        }
        else if( name == "foundation:PolyLine" )
        {
            std::shared_ptr<idx_geometry::EDMD_POLYLINE> s =
                    std::make_shared<idx_geometry::EDMD_POLYLINE>( *child2, true );
            m_registry.registerObject<idx_geometry::EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT>(
                    s->m_id, s );
            m_curves.push_back( s );
        }
        else if( name == "foundation:CircleCenter" )
        {
            std::shared_ptr<idx_geometry::EDMD_CIRCLE_CENTER> s =
                    std::make_shared<idx_geometry::EDMD_CIRCLE_CENTER>( *child2 );
            m_registry.registerObject<idx_geometry::EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT>(
                    s->m_id, s );
            m_curves.push_back( s );
        }
        else if( name == "foundation:Arc" )
        {
            std::shared_ptr<idx_geometry::EDMD_ARC> s =
                    std::make_shared<idx_geometry::EDMD_ARC>( *child2 );
            m_registry.registerObject<idx_geometry::EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT>(
                    s->m_id, s );
            m_curves.push_back( s );
        }
        else if( name == "foundation:CurveSet2d" )
        {
            std::shared_ptr<idx_geometry::EDMD_CURVESET2D> s =
                    std::make_shared<idx_geometry::EDMD_CURVESET2D>( *child2 );
            m_registry.registerObject<idx_pdm::EDMD_SHAPE_DESCRIPTION>( s->m_id, s );
            m_curvesets.push_back( s );
        }
        else if( name == "foundation:ShapeElement" )
        {
            std::shared_ptr<idx_pdm::EDMD_SHAPE_ELEMENT> s =
                    std::make_shared<idx_pdm::EDMD_SHAPE_ELEMENT>( *child2, true );
            m_registry.registerObject<idx_pdm::EDMD_SHAPE_ELEMENT>( s->m_id, s );
            m_shapeElements.push_back( s );
        }
        else if( name == "foundation:Stratum" )
        {
            std::shared_ptr<idx_pdm::EDMD_STRATUM> s =
                    std::make_shared<idx_pdm::EDMD_STRATUM>( *child2, true );
            m_registry.registerObject<idx_pdm::EDMD_ITEM_SHAPE>( s->m_id, s );
            m_stratums.push_back( s );
        }
        else if( name == "foundation:KeepOut" )
        {
            std::shared_ptr<idx_pdm::EDMD_KEEPOUT> s =
                    std::make_shared<idx_pdm::EDMD_KEEPOUT>( *child2, true );
            //m_registry.registerObject<idx_pdm::EDMD_SHAPE_DESCRIPTION>( s->m_id, s );
            m_keeps.push_back( s );
        }
        else if( name == "foundation:KeepIn" )
        {
            std::shared_ptr<idx_pdm::EDMD_KEEPIN> s =
                    std::make_shared<idx_pdm::EDMD_KEEPIN>( *child2, true );
            //m_registry.registerObject<idx_pdm::EDMD_SHAPE_DESCRIPTION>( s->m_id, s );
            m_keeps.push_back( s );
        }
        else if( name == "foundation:Item" )
        {
            std::shared_ptr<idx_pdm::EDMD_ITEM> s =
                    std::make_shared<idx_pdm::EDMD_ITEM>( *child2, true );
            m_registry.registerObject<idx_pdm::EDMD_ITEM>( s->m_id, s );
            m_items.push_back( s );
        }
        else if( name == "foundation:AssemblyComponent" )
        {
            std::shared_ptr<idx_pdm::EDMD_ASSEMBLY_COMPONENT> s =
                    std::make_shared<idx_pdm::EDMD_ASSEMBLY_COMPONENT>( *child2, true );
            m_registry.registerObject<idx_pdm::EDMD_ITEM_SHAPE>( s->m_id, s );
            m_assemblies.push_back( s );  
        }
        else if( name == "foundation:InterStratumFeature" )
        {
            std::shared_ptr<idx_pdm::EDMD_INTERSTRATUM_FEATURE> s =
                    std::make_shared<idx_pdm::EDMD_INTERSTRATUM_FEATURE>( *child2, true );
            m_registry.registerObject<idx_pdm::EDMD_ITEM_SHAPE>( s->m_id, s );
            m_interStratums.push_back( s );
        }

        else if( name == "foundation:FunctionalItemShape" )
        {
            std::shared_ptr<idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE> s =
                    std::make_shared<idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE>( *child2 );
            m_registry.registerObject<idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE>( s->m_id, s );
            m_functionalItemShapes.push_back( s );
        }
        else
        {
            std::cout << "unhandled keyword:" << name << std::endl;
        }
        
        child2 = child2->GetNext();
    }
}


void IDX_HOLDER::Read( wxXmlDocument& aDoc )
{

    m_header = nullptr;

    wxXmlNode* child = aDoc.GetRoot()->GetChildren();
    std::cout << "Trying to read IDX file..." << std::endl;

    while( child )
    {
        if( child->GetName() == "foundation:Header" )
        {
            m_header = std::make_shared<idx_foundation::EDMD_HEADER>( *child, true );
        }
        else if( child->GetName() == "foundation:Body" )
        {
            this->ReadBody( *child );
        }
        else
        {
            std::cout << "unhandled keyword:" << child->GetName() << std::endl;
        }
        child = child->GetNext();
    }

    std::cout << "Resolving curves " << std::endl;
    for( auto& curve : m_curves )
        curve->Resolve( m_registry );
    std::cout << "Resolving curvesets " << std::endl;
    for( auto& curveset : m_curvesets )
        curveset->Resolve( m_registry );
    std::cout << "Resolving shapeElements " << std::endl;
    for( auto& shapeE : m_shapeElements )
        shapeE->Resolve( m_registry );
    std::cout << "Resolving stratum " << std::endl;
    for( auto& stratum : m_stratums )
        stratum->Resolve( m_registry );
    std::cout << "Resolving interstratum " << std::endl;
    for( auto& interstratum : m_interStratums )
        interstratum->Resolve( m_registry );
    std::cout << "Resolving keeps " << std::endl;
    for( auto& keep : m_keeps )
        keep->Resolve( m_registry );
    std::cout << "Resolving items " << std::endl;
    for( auto& item : m_items )
    {
        item->Resolve( m_registry );
    }
    std::cout << "Resolving functional shapes" << std::endl;
    for( auto& fs : m_functionalItemShapes )
    {
        fs->Resolve( m_registry );
    }

    m_boardOutline = {};
    m_NPTH = {};
    m_PTH = {};


    for (const std::shared_ptr<idx_pdm::EDMD_ITEM>& i : m_items)
    {
        for (const std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>& inst : i->m_itemInstance)
        {
            bool handled = false;

            // Check for board outline
            if (auto type = inst->GetUserProperty("TYPE"))
            {
                std::cout << "Has type " << std::endl ;

                if (type->m_value == "BOARDOUTLINE")
                {
                    std::cout << "BOARD OUTLINE FOUND " << std::endl ;

                    m_boardOutline.push_back(inst);
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
                        m_boardOutline.push_back(inst);
                        handled = true;
                        continue;
                    case FT::CUTOUT:
                        m_NPTH.push_back(inst);
                        handled = true;
                        continue;
                    case FT::PLATED_CUTOUT:
                        m_PTH.push_back(inst);
                        handled = true;
                        continue;
                    default:
                        break;
                }
            }

            // If not stored anywhere, push to m_others
            if (!handled)
                m_others.push_back(inst);
        }
    }

    
}



std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetBoardOutlineElements()
{
    return m_boardOutline;
}

std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetNPTH()
{
    return m_NPTH; 
}


std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetPTH()
{
    return m_PTH;
}


std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetOthers()
{
    return m_others;
}


std::vector<std::shared_ptr<idx_pdm::EDMD_KEEP_CONSTRAINT>> IDX_HOLDER::GetKeepZones()
{
    return m_keeps;
}

#endif