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
#include <board_commit.h>
#include <math/vector2d.h>
#include <pcb_shape.h>

class IDX_HOLDER
{
public:
    IDX_HOLDER( wxXmlDocument& aDoc )
    {
        ReadHeader( aDoc );
        ReadBody( aDoc );
    };

    void ReadHeader( wxXmlDocument& aDoc );
    void ReadBody( wxXmlDocument& aDoc );
    void AddElements( BOARD_COMMIT& aCommit );

    void AddEDMDObject( wxXmlNode& aNode );
    void AddEDMDSystem( wxXmlNode& aNode );

    std::shared_ptr<idx_foundation::EDMD_HEADER> m_header;
    std::map<wxString, std::shared_ptr<idx_administration::EDMD_ACTOR>> m_actors;
    std::map<wxString, std::shared_ptr<idx_foundation::EDMD_SYSTEM>>    m_systems;
    std::map<wxString, std::shared_ptr<idx_property::EDMD_UNIT>>        m_units;
    std::map<wxString, std::shared_ptr<idx_geometry::EDMD_POINT>>       m_points;
    std::map<wxString, std::shared_ptr<idx_geometry::EDMD_CURVE>>       m_curves;

    IDX_REGISTRY m_registry;
};


void IDX_HOLDER::AddElements( BOARD_COMMIT& aCommit )
{
    for( auto const& x : m_curves )
    {
        std::shared_ptr<idx_geometry::EDMD_POLYLINE> polyline =
                std::dynamic_pointer_cast<idx_geometry::EDMD_POLYLINE>( x.second );

        if( polyline )
        {
            std::vector<VECTOR2I> polypoints;

            for( std::shared_ptr<idx_geometry::EDMD_POINT> p : polyline->m_point )
            {
                if( p )
                    polypoints.emplace_back( p->GetX(), p->GetY() );
            }

            for( size_t i = 0; i < polypoints.size() - 1; i++ )
            {
                PCB_SHAPE* s = new PCB_SHAPE();
                s->SetWidth( polyline->m_thickness.m_value * 1e6 );
                s->SetStart( polypoints[i] );
                s->SetEnd( polypoints[i + 1] );
                aCommit.Add( s );
            }

            continue;
        }

        std::shared_ptr<idx_geometry::EDMD_CIRCLE> circle =
                std::dynamic_pointer_cast<idx_geometry::EDMD_CIRCLE>( x.second );

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
                {
                    std::shared_ptr<idx_foundation::EDMD_SYSTEM> s =
                            std::make_shared<idx_foundation::EDMD_SYSTEM>( *child2 );
                    m_systems[s->m_id] = s;
                }
                else if( name == "foundation:Person" )
                {
                    std::shared_ptr<idx_administration::EDMD_PERSON> s =
                            std::make_shared<idx_administration::EDMD_PERSON>( *child2 );
                    m_actors[s->m_id] = s;
                }
                else if( name == "foundation:UnitLength" )
                {
                    std::shared_ptr<idx_property::EDMD_UNIT> s =
                            std::make_shared<idx_property::EDMD_UNIT>( *child2 );
                    m_units[s->m_id] = s;
                }
                else if( name == "foundation:CartesianPoint" )
                {
                    std::shared_ptr<idx_geometry::EDMD_CARTESIAN_POINT> s =
                            std::make_shared<idx_geometry::EDMD_CARTESIAN_POINT>( *child2 );
                    m_registry.registerObject<idx_geometry::EDMD_POINT>( s->m_id, s );
                    m_points[s->m_id] = s;
                    std::cout << "Adding point: " << s->m_id << std::endl;
                    ;
                }
                else if( name == "foundation:PolyLine" )
                {
                    std::shared_ptr<idx_geometry::EDMD_POLYLINE> s =
                            std::make_shared<idx_geometry::EDMD_POLYLINE>( *child2 );
                    m_curves[s->m_id] = s;
                }
                else if( name == "foundation:CircleCenter" )
                {
                    std::shared_ptr<idx_geometry::EDMD_CIRCLE_CENTER> s =
                            std::make_shared<idx_geometry::EDMD_CIRCLE_CENTER>( *child2 );
                    m_curves[s->m_id] = s;
                }
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

    for( auto curve : m_curves )
    {
        curve.second->Resolve( m_registry );
        //curve.second->LinkPoints( m_points );
        //curve.second->LinkPoints( m_curves   );
    }
}


void IDX_HOLDER::AddEDMDObject( wxXmlNode& aNode )
{
}

#endif