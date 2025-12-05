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

#include <idx/kidx/schema/property.h>
#include <iostream>

namespace idx_property
{

// ------------------- ARRAY_EDMD_NAME -------------------

ARRAY_EDMD_NAME::ARRAY_EDMD_NAME( IDX_XML_NODE& aNode, bool aTopLevel ) :
        IDX_BASE()
{
    if( aTopLevel )
        ParseChildren( aNode );
};

// ------------------- EDMD_CONSTRAINED_PROPERTY -------------------

EDMD_CONSTRAINED_PROPERTY::EDMD_CONSTRAINED_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
};

bool EDMD_CONSTRAINED_PROPERTY::Parse( IDX_XML_NODE& aNode )
{
    return this->idx_foundation::EDMD_OBJECT::Parse( aNode );
}

// ------------------- EDMD_USER_PROPERTY -------------------

EDMD_USER_PROPERTY::EDMD_USER_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_CONSTRAINED_PROPERTY( aNode, false )
{
    m_xsiType = "property:EDMDUserSimpleProperty";

    if( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_USER_PROPERTY::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "property:Key" ) )
        m_keys.emplace_back( std::make_shared<idx_foundation::EDMD_NAME>( aNode, true ) );
    else if( IDX_keyword_compare( name, "property:Value" ) )
        m_value = content;
    else
        return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
    return true;
}

void EDMD_USER_PROPERTY::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CONSTRAINED_PROPERTY::Write( aNode );

    for( auto key : m_keys )
    {
        if( key )
            WriteChild( aNode, "property:Key", *key );
    }
    WriteValueChild( aNode, "property:Value", m_value );
}

// ------------------- EDMD_UNIT -------------------

EDMD_UNIT::EDMD_UNIT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_UNIT::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "foundation:UnitName" ) )
        m_unitName = content;
    else if( IDX_keyword_compare( name, "property:Fundamental" ) )
        m_fundamental = content;
    else if( IDX_keyword_compare( name, "foundation:DependendScaleFactor" ) )
        m_scalingFactor = parseDouble( content );
    else if( IDX_keyword_compare( name, "foundation:DependsOn" ) )
        m_dependsOnKey = content;
    else
        return this->idx_foundation::EDMD_COLLABORATION_OBJECT::Parse( aNode );

    return true;
}

// ------------------- EDMD_UNIT Subclasses -------------------

EDMD_UNIT_AMPERAGE::EDMD_UNIT_AMPERAGE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_UNIT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}


EDMD_UNIT_ANGLE::EDMD_UNIT_ANGLE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_UNIT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}


EDMD_UNIT_CAPACITANCE::EDMD_UNIT_CAPACITANCE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_UNIT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}


EDMD_UNIT_LENGTH::EDMD_UNIT_LENGTH( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_UNIT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}


EDMD_UNIT_POWER::EDMD_UNIT_POWER( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_UNIT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}


EDMD_UNIT_RESISTANCE::EDMD_UNIT_RESISTANCE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_UNIT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}


EDMD_UNIT_SURFACE::EDMD_UNIT_SURFACE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_UNIT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

// ------------------- EDMD_ANGLE_PROPERTY -------------------

EDMD_ANGLE_PROPERTY::EDMD_ANGLE_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_CONSTRAINED_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_ANGLE_PROPERTY::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "property:Value" ) )
    {
        m_value = parseDouble( content );
        return true;
    }
    else if( IDX_keyword_compare( name, "property:Unit" ) )
    {
        if( IDX_keyword_compare( content, "degrees" ) || IDX_keyword_compare( content, "grad" ) )
        {
            m_unit = content;
            return true;
        }
        else
            return false;
    }
    else
        return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
}

// ------------------- EDMD_LENGTH_PROPERTY -------------------

EDMD_LENGTH_PROPERTY::EDMD_LENGTH_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_CONSTRAINED_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_LENGTH_PROPERTY::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "property:Value" ) )
    {
        m_value = parseDouble( content );
        return true;
    }

    return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
}

// ------------------- EDMD_LOGIC_PROPERTY -------------------

EDMD_LOGIC_PROPERTY::EDMD_LOGIC_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_CONSTRAINED_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_LOGIC_PROPERTY::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "property:Value" ) )
    {
        m_value = ( IDX_keyword_compare( content, "true" ) );
        return true;
    }

    return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
}

} // namespace idx_property
