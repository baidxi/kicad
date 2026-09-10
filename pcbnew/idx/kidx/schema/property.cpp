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
    std::string value;

    value = aNode.GetAttribute( "IsChanged", "" );

    if( !value.empty() )
        m_isChanged = parseBool( value );

    value = aNode.GetAttribute( "IsNew", "" );

    if( !value.empty() )
        m_isNew = parseBool( value );

    if( aTopLevel )
        ParseChildren( aNode );
};

bool EDMD_CONSTRAINED_PROPERTY::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "property:ValidValue" ) )
    {
        m_validValues.push_back( content );
        return true;
    }
    else if( IDX_keyword_compare( name, "property:MaxValue" ) )
    {
        m_maxValue.push_back( std::make_shared<EDMD_CONSTRAINED_PROPERTY>( aNode, true ) );
        return true;
    }
    else if( IDX_keyword_compare( name, "property:MinValue" ) )
    {
        m_minValue.push_back( std::make_shared<EDMD_CONSTRAINED_PROPERTY>( aNode, true ) );
        return true;
    }
    else if( IDX_keyword_compare( name, "property:Originator" ) )
    {
        // TODO: implement EDMD_PERSON resolution
        // m_originator = ...
        return true;
    }
    else if( IDX_keyword_compare( name, "property:ValidUnit" ) )
    {
        m_validUnit.push_back( std::make_shared<EDMD_UNIT>( aNode, true ) );
        return true;
    }
    else if( IDX_keyword_compare( name, "property:Unit" ) )
    {
        m_unit = content;
        return true;
    }

    return this->idx_foundation::EDMD_OBJECT::Parse( aNode );
}

void EDMD_CONSTRAINED_PROPERTY::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_OBJECT::Write( aNode );

    for( auto validUnit : m_validUnit )
    {
        if( validUnit )
            WriteChild( aNode, "property:ValidUnit", *validUnit );
    }
    for( const auto& validValue : m_validValues )
    {
        WriteValueChild( aNode, "property:ValidValue", validValue );
    }
    for( auto maxValue : m_maxValue )
    {
        if( maxValue )
            WriteChild( aNode, "property:MaxValue", *maxValue );
    }
    for( auto minValue : m_minValue )
    {
        if( minValue )
            WriteChild( aNode, "property:MinValue", *minValue );
    }
    // Originator TODO: implement when EDMD_PERSON is fully implemented
    WriteValueChild( aNode, "property:Unit", m_unit );
}

bool EDMD_CONSTRAINED_PROPERTY::Resolve( IDX_REGISTRY& aRegistry )
{
    for( auto& validUnit : m_validUnit )
    {
        if( validUnit )
            validUnit->Resolve( aRegistry );
    }
    for( auto& maxValue : m_maxValue )
    {
        if( maxValue )
            maxValue->Resolve( aRegistry );
    }
    for( auto& minValue : m_minValue )
    {
        if( minValue )
            minValue->Resolve( aRegistry );
    }
    return idx_foundation::EDMD_OBJECT::Resolve( aRegistry );
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

bool EDMD_USER_PROPERTY::Resolve( IDX_REGISTRY& aRegistry )
{
    if( !EDMD_CONSTRAINED_PROPERTY::Resolve( aRegistry ) )
        return false;

    for( auto& key : m_keys )
    {
        if( key && !key->Resolve( aRegistry ) )
            return false;
    }
    return true;
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


void EDMD_UNIT::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_COLLABORATION_OBJECT::Write( aNode );

    if( !m_unitName.empty() )
        aNode.AppendChild( "foundation:UnitName" )->SetText( m_unitName );

    if( !m_fundamental.empty() )
        aNode.AppendChild( "property:Fundamental" )->SetText( m_fundamental );

    if( m_scalingFactor != 1.0 )
        aNode.AppendChild( "foundation:DependendScaleFactor" )->SetText( std::to_string( m_scalingFactor ) );

    if( m_dependsOn )
    {
        std::shared_ptr<IDX_XML_NODE> dependsOnNode = aNode.AppendChild( "foundation:DependsOn" );
        m_dependsOn->Write( *dependsOnNode );
    }
    else if( !m_dependsOnKey.empty() )
    {
        aNode.AppendChild( "foundation:DependsOn" )->SetText( m_dependsOnKey );
    }
}


bool EDMD_UNIT::Resolve( IDX_REGISTRY& aRegistry )
{
    if( !idx_foundation::EDMD_COLLABORATION_OBJECT::Resolve( aRegistry ) )
        return false;

    if( !m_dependsOnKey.empty() && !m_dependsOn )
        resolve<EDMD_UNIT, EDMD_UNIT>( m_dependsOnKey, &m_dependsOn, aRegistry );

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


EDMD_UNIT_VOLTAGE::EDMD_UNIT_VOLTAGE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_UNIT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

// Parse methods for UNIT subclasses
bool EDMD_UNIT_AMPERAGE::Parse( IDX_XML_NODE& aNode )
{
    return this->EDMD_UNIT::Parse( aNode );
}

bool EDMD_UNIT_ANGLE::Parse( IDX_XML_NODE& aNode )
{
    return this->EDMD_UNIT::Parse( aNode );
}

bool EDMD_UNIT_CAPACITANCE::Parse( IDX_XML_NODE& aNode )
{
    return this->EDMD_UNIT::Parse( aNode );
}

bool EDMD_UNIT_LENGTH::Parse( IDX_XML_NODE& aNode )
{
    return this->EDMD_UNIT::Parse( aNode );
}

bool EDMD_UNIT_POWER::Parse( IDX_XML_NODE& aNode )
{
    return this->EDMD_UNIT::Parse( aNode );
}

bool EDMD_UNIT_RESISTANCE::Parse( IDX_XML_NODE& aNode )
{
    return this->EDMD_UNIT::Parse( aNode );
}

bool EDMD_UNIT_SURFACE::Parse( IDX_XML_NODE& aNode )
{
    return this->EDMD_UNIT::Parse( aNode );
}

bool EDMD_UNIT_VOLTAGE::Parse( IDX_XML_NODE& aNode )
{
    return this->EDMD_UNIT::Parse( aNode );
}

bool EDMD_UNIT_VOLUME::Parse( IDX_XML_NODE& aNode )
{
    return this->EDMD_UNIT::Parse( aNode );
}

bool EDMD_UNIT_WEIGHT::Parse( IDX_XML_NODE& aNode )
{
    return this->EDMD_UNIT::Parse( aNode );
}

bool EDMD_UNIT_WORK::Parse( IDX_XML_NODE& aNode )
{
    return this->EDMD_UNIT::Parse( aNode );
}


// Write and Resolve methods for UNIT subclasses
bool EDMD_UNIT_AMPERAGE::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->EDMD_UNIT::Resolve( aRegistry );
}

void EDMD_UNIT_AMPERAGE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_UNIT::Write( aNode );
}

bool EDMD_UNIT_ANGLE::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->EDMD_UNIT::Resolve( aRegistry );
}

void EDMD_UNIT_ANGLE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_UNIT::Write( aNode );
}

bool EDMD_UNIT_CAPACITANCE::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->EDMD_UNIT::Resolve( aRegistry );
}

void EDMD_UNIT_CAPACITANCE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_UNIT::Write( aNode );
}

bool EDMD_UNIT_LENGTH::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->EDMD_UNIT::Resolve( aRegistry );
}

void EDMD_UNIT_LENGTH::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_UNIT::Write( aNode );
}

bool EDMD_UNIT_POWER::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->EDMD_UNIT::Resolve( aRegistry );
}

void EDMD_UNIT_POWER::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_UNIT::Write( aNode );
}

bool EDMD_UNIT_RESISTANCE::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->EDMD_UNIT::Resolve( aRegistry );
}

void EDMD_UNIT_RESISTANCE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_UNIT::Write( aNode );
}

bool EDMD_UNIT_SURFACE::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->EDMD_UNIT::Resolve( aRegistry );
}

void EDMD_UNIT_SURFACE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_UNIT::Write( aNode );
}

bool EDMD_UNIT_VOLTAGE::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->EDMD_UNIT::Resolve( aRegistry );
}

void EDMD_UNIT_VOLTAGE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_UNIT::Write( aNode );
}

bool EDMD_UNIT_VOLUME::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->EDMD_UNIT::Resolve( aRegistry );
}

void EDMD_UNIT_VOLUME::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_UNIT::Write( aNode );
}

bool EDMD_UNIT_WEIGHT::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->EDMD_UNIT::Resolve( aRegistry );
}

void EDMD_UNIT_WEIGHT::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_UNIT::Write( aNode );
}

bool EDMD_UNIT_WORK::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->EDMD_UNIT::Resolve( aRegistry );
}

void EDMD_UNIT_WORK::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_UNIT::Write( aNode );
}


// ------------------- Unit Fundamental Classes -------------------

EDMD_UNIT_AMPERAGE_FUNDAMENTAL::EDMD_UNIT_AMPERAGE_FUNDAMENTAL( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_UNIT_AMPERAGE_FUNDAMENTAL::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( name == "#text" || name == "" )
    {
        m_value = content;
        return true;
    }

    return this->idx_foundation::EDMD_OBJECT::Parse( aNode );
}

void EDMD_UNIT_AMPERAGE_FUNDAMENTAL::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_OBJECT::Write( aNode );
    if( !m_value.empty() )
        aNode.SetText( m_value );
}

EDMD_UNIT_ANGLE_FUNDAMENTAL::EDMD_UNIT_ANGLE_FUNDAMENTAL( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_UNIT_ANGLE_FUNDAMENTAL::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( name == "#text" || name == "" )
    {
        m_value = content;
        return true;
    }

    return this->idx_foundation::EDMD_OBJECT::Parse( aNode );
}

void EDMD_UNIT_ANGLE_FUNDAMENTAL::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_OBJECT::Write( aNode );
    if( !m_value.empty() )
        aNode.SetText( m_value );
}

EDMD_UNIT_CAPACITANCE_FUNDAMENTAL::EDMD_UNIT_CAPACITANCE_FUNDAMENTAL( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_UNIT_CAPACITANCE_FUNDAMENTAL::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( name == "#text" || name == "" )
    {
        m_value = content;
        return true;
    }

    return this->idx_foundation::EDMD_OBJECT::Parse( aNode );
}

void EDMD_UNIT_CAPACITANCE_FUNDAMENTAL::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_OBJECT::Write( aNode );
    if( !m_value.empty() )
        aNode.SetText( m_value );
}

EDMD_UNIT_LENGTH_FUNDAMENTAL::EDMD_UNIT_LENGTH_FUNDAMENTAL( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_UNIT_LENGTH_FUNDAMENTAL::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( name == "#text" || name == "" )
    {
        m_value = content;
        return true;
    }

    return this->idx_foundation::EDMD_OBJECT::Parse( aNode );
}

void EDMD_UNIT_LENGTH_FUNDAMENTAL::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_OBJECT::Write( aNode );
    if( !m_value.empty() )
        aNode.SetText( m_value );
}

EDMD_UNIT_POWER_FUNDAMENTAL::EDMD_UNIT_POWER_FUNDAMENTAL( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_UNIT_POWER_FUNDAMENTAL::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( name == "#text" || name == "" )
    {
        m_value = content;
        return true;
    }

    return this->idx_foundation::EDMD_OBJECT::Parse( aNode );
}

void EDMD_UNIT_POWER_FUNDAMENTAL::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_OBJECT::Write( aNode );
    if( !m_value.empty() )
        aNode.SetText( m_value );
}

EDMD_UNIT_RESISTANCE_FUNDAMENTAL::EDMD_UNIT_RESISTANCE_FUNDAMENTAL( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_UNIT_RESISTANCE_FUNDAMENTAL::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( name == "#text" || name == "" )
    {
        m_value = content;
        return true;
    }

    return this->idx_foundation::EDMD_OBJECT::Parse( aNode );
}

void EDMD_UNIT_RESISTANCE_FUNDAMENTAL::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_OBJECT::Write( aNode );
    if( !m_value.empty() )
        aNode.SetText( m_value );
}

EDMD_UNIT_SURFACE_FUNDAMENTAL::EDMD_UNIT_SURFACE_FUNDAMENTAL( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_UNIT_SURFACE_FUNDAMENTAL::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( name == "#text" || name == "" )
    {
        m_value = content;
        return true;
    }

    return this->idx_foundation::EDMD_OBJECT::Parse( aNode );
}

void EDMD_UNIT_SURFACE_FUNDAMENTAL::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_OBJECT::Write( aNode );
    if( !m_value.empty() )
        aNode.SetText( m_value );
}

EDMD_UNIT_VOLTAGE_FUNDAMENTAL::EDMD_UNIT_VOLTAGE_FUNDAMENTAL( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_UNIT_VOLTAGE_FUNDAMENTAL::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( name == "#text" || name == "" )
    {
        m_value = content;
        return true;
    }

    return this->idx_foundation::EDMD_OBJECT::Parse( aNode );
}

void EDMD_UNIT_VOLTAGE_FUNDAMENTAL::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_OBJECT::Write( aNode );
    if( !m_value.empty() )
        aNode.SetText( m_value );
}

EDMD_UNIT_VOLUME_FUNDAMENTAL::EDMD_UNIT_VOLUME_FUNDAMENTAL( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_UNIT_VOLUME_FUNDAMENTAL::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( name == "#text" || name == "" )
    {
        m_value = content;
        return true;
    }

    return this->idx_foundation::EDMD_OBJECT::Parse( aNode );
}

void EDMD_UNIT_VOLUME_FUNDAMENTAL::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_OBJECT::Write( aNode );
    if( !m_value.empty() )
        aNode.SetText( m_value );
}

EDMD_UNIT_WEIGHT_FUNDAMENTAL::EDMD_UNIT_WEIGHT_FUNDAMENTAL( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_UNIT_WEIGHT_FUNDAMENTAL::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( name == "#text" || name == "" )
    {
        m_value = content;
        return true;
    }

    return this->idx_foundation::EDMD_OBJECT::Parse( aNode );
}

void EDMD_UNIT_WEIGHT_FUNDAMENTAL::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_OBJECT::Write( aNode );
    if( !m_value.empty() )
        aNode.SetText( m_value );
}

EDMD_UNIT_WORK_FUNDAMENTAL::EDMD_UNIT_WORK_FUNDAMENTAL( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_UNIT_WORK_FUNDAMENTAL::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( name == "#text" || name == "" )
    {
        m_value = content;
        return true;
    }

    return this->idx_foundation::EDMD_OBJECT::Parse( aNode );
}

void EDMD_UNIT_WORK_FUNDAMENTAL::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_OBJECT::Write( aNode );
    if( !m_value.empty() )
        aNode.SetText( m_value );
}

// Resolve methods for Unit Fundamental Classes
bool EDMD_UNIT_AMPERAGE_FUNDAMENTAL::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->idx_foundation::EDMD_OBJECT::Resolve( aRegistry );
}

bool EDMD_UNIT_ANGLE_FUNDAMENTAL::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->idx_foundation::EDMD_OBJECT::Resolve( aRegistry );
}

bool EDMD_UNIT_CAPACITANCE_FUNDAMENTAL::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->idx_foundation::EDMD_OBJECT::Resolve( aRegistry );
}

bool EDMD_UNIT_LENGTH_FUNDAMENTAL::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->idx_foundation::EDMD_OBJECT::Resolve( aRegistry );
}

bool EDMD_UNIT_POWER_FUNDAMENTAL::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->idx_foundation::EDMD_OBJECT::Resolve( aRegistry );
}

bool EDMD_UNIT_RESISTANCE_FUNDAMENTAL::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->idx_foundation::EDMD_OBJECT::Resolve( aRegistry );
}

bool EDMD_UNIT_SURFACE_FUNDAMENTAL::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->idx_foundation::EDMD_OBJECT::Resolve( aRegistry );
}

bool EDMD_UNIT_VOLTAGE_FUNDAMENTAL::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->idx_foundation::EDMD_OBJECT::Resolve( aRegistry );
}

bool EDMD_UNIT_VOLUME_FUNDAMENTAL::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->idx_foundation::EDMD_OBJECT::Resolve( aRegistry );
}

bool EDMD_UNIT_WEIGHT_FUNDAMENTAL::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->idx_foundation::EDMD_OBJECT::Resolve( aRegistry );
}

bool EDMD_UNIT_WORK_FUNDAMENTAL::Resolve( IDX_REGISTRY& aRegistry )
{
    return this->idx_foundation::EDMD_OBJECT::Resolve( aRegistry );
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
        m_value_str = content;
        return true;
    }

    return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
}

void EDMD_LENGTH_PROPERTY::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CONSTRAINED_PROPERTY::Write( aNode );

    WriteValueChild( aNode, "property:Value", m_value_str );
}


bool EDMD_LENGTH_PROPERTY::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_CONSTRAINED_PROPERTY::Resolve( aRegistry );
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

void EDMD_LOGIC_PROPERTY::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CONSTRAINED_PROPERTY::Write( aNode );

    WriteValueChild( aNode, "property:Value", std::string( m_value ? "true" : "false" ) );
}


bool EDMD_LOGIC_PROPERTY::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_CONSTRAINED_PROPERTY::Resolve( aRegistry );
}


// ------------------- EDMD_PROPERTY -------------------

EDMD_PROPERTY::EDMD_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_CONSTRAINED_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_PROPERTY::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "property:Value" ) )
    {
        m_value = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "property:Unit" ) )
    {
        m_unit = content;
        return true;
    }
    else
    {
        return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
    }
}

void EDMD_PROPERTY::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CONSTRAINED_PROPERTY::Write( aNode );

    WriteValueChild( aNode, "property:Value", m_value );
    WriteValueChild( aNode, "property:Unit", m_unit );
}

bool EDMD_PROPERTY::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_CONSTRAINED_PROPERTY::Resolve( aRegistry );
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
        m_unit = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "property:ValidValue" ) )
    {
        m_validValues.push_back( content );
        return true;
    }
    else if( IDX_keyword_compare( name, "property:MaxValue" ) )
    {
        m_maxValue.push_back( std::make_shared<EDMD_CONSTRAINED_PROPERTY>( aNode, true ) );
        return true;
    }
    else if( IDX_keyword_compare( name, "property:MinValue" ) )
    {
        m_minValue.push_back( std::make_shared<EDMD_CONSTRAINED_PROPERTY>( aNode, true ) );
        return true;
    }
    else if( IDX_keyword_compare( name, "property:Originator" ) )
    {
        // TODO: implement EDMD_PERSON resolution
        // m_originator = ...
        return true;
    }
    else if( IDX_keyword_compare( name, "property:ValidUnit" ) )
    {
        m_validUnit.push_back( std::make_shared<EDMD_UNIT>( aNode, true ) );
        return true;
    }
    else
    {
        return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
    }
}

void EDMD_ANGLE_PROPERTY::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CONSTRAINED_PROPERTY::Write( aNode );

    aNode.AppendChild( "property:Value" )->SetText( std::to_string( m_value ) );
    WriteValueChild( aNode, "property:Unit", m_unit );
}

bool EDMD_ANGLE_PROPERTY::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_CONSTRAINED_PROPERTY::Resolve( aRegistry );
}

// ------------------- EDMD_DATE_TIME_PROPERTY -------------------

EDMD_DATE_TIME_PROPERTY::EDMD_DATE_TIME_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_CONSTRAINED_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_DATE_TIME_PROPERTY::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "property:Value" ) )
    {
        m_value = content;
        return true;
    }
    else
    {
        return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
    }
}

void EDMD_DATE_TIME_PROPERTY::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CONSTRAINED_PROPERTY::Write( aNode );

    WriteValueChild( aNode, "property:Value", m_value );
}

bool EDMD_DATE_TIME_PROPERTY::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_CONSTRAINED_PROPERTY::Resolve( aRegistry );
}

// ------------------- EDMD_INTEGER_PROPERTY -------------------

EDMD_INTEGER_PROPERTY::EDMD_INTEGER_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_CONSTRAINED_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_INTEGER_PROPERTY::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "property:Value" ) )
    {
        m_value = std::stoi( content );
        return true;
    }
    else if( IDX_keyword_compare( name, "property:Unit" ) )
    {
        m_unit = content;
        return true;
    }
    else
    {
        return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
    }
}

void EDMD_INTEGER_PROPERTY::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CONSTRAINED_PROPERTY::Write( aNode );

    WriteValueChild( aNode, "property:Value", std::to_string( m_value ) );
    WriteValueChild( aNode, "property:Unit", m_unit );
}

bool EDMD_INTEGER_PROPERTY::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_CONSTRAINED_PROPERTY::Resolve( aRegistry );
}

// ------------------- EDMD_COLOR_PROPERTY -------------------

EDMD_COLOR_PROPERTY::EDMD_COLOR_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_CONSTRAINED_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_COLOR_PROPERTY::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "property:Value" ) )
    {
        m_value = std::stoi( content );
        return true;
    }
    else if( IDX_keyword_compare( name, "property:Mixed" ) )
    {
        m_mixed = ( IDX_keyword_compare( content, "true" ) );
        return true;
    }
    else
    {
        return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
    }
}

void EDMD_COLOR_PROPERTY::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CONSTRAINED_PROPERTY::Write( aNode );

    WriteValueChild( aNode, "property:Value", std::to_string( m_value ) );
    aNode.AppendChild( "property:Mixed" )->SetText( m_mixed ? "true" : "false" );
}

bool EDMD_COLOR_PROPERTY::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_CONSTRAINED_PROPERTY::Resolve( aRegistry );
}

// ------------------- EDMD_CLEARANCE_STATE_PROPERTY -------------------

EDMD_CLEARANCE_STATE_PROPERTY::EDMD_CLEARANCE_STATE_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_CONSTRAINED_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_CLEARANCE_STATE_PROPERTY::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "property:StateName" ) )
    {
        m_stateName.emplace_back( idx_foundation::EDMD_NAME( aNode, true ) );
        return true;
    }
    else if( IDX_keyword_compare( name, "property:Accept" ) )
    {
        m_accept = ( IDX_keyword_compare( content, "true" ) );
        return true;
    }
    else
    {
        return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
    }
}

void EDMD_CLEARANCE_STATE_PROPERTY::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CONSTRAINED_PROPERTY::Write( aNode );

    for( const auto& stateName : m_stateName )
    {
        WriteChild( aNode, "property:StateName", stateName );
    }
    aNode.AppendChild( "property:Accept" )->SetText( m_accept ? "true" : "false" );
}

bool EDMD_CLEARANCE_STATE_PROPERTY::Resolve( IDX_REGISTRY& aRegistry )
{
    if( !EDMD_CONSTRAINED_PROPERTY::Resolve( aRegistry ) )
        return false;

    for( auto& stateName : m_stateName )
    {
        if( !stateName.Resolve( aRegistry ) )
            return false;
    }
    return true;
}

// ------------------- EDMD_USER_SIMPLE_PROPERTY -------------------

EDMD_USER_SIMPLE_PROPERTY::EDMD_USER_SIMPLE_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_USER_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_USER_SIMPLE_PROPERTY::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "property:Value" ) )
    {
        m_simpleValue = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "property:Unit" ) )
    {
        m_unit = content;
        return true;
    }
    else
    {
        return this->EDMD_USER_PROPERTY::Parse( aNode );
    }
}

void EDMD_USER_SIMPLE_PROPERTY::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_USER_PROPERTY::Write( aNode );

    WriteValueChild( aNode, "property:Value", m_simpleValue );
    WriteValueChild( aNode, "property:Unit", m_unit );
}

bool EDMD_USER_SIMPLE_PROPERTY::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_USER_PROPERTY::Resolve( aRegistry );
}

// ------------------- EDMD_USER_COMPOUND_PROPERTY -------------------

EDMD_USER_COMPOUND_PROPERTY::EDMD_USER_COMPOUND_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_USER_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_USER_COMPOUND_PROPERTY::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();

    if( IDX_keyword_compare( name, "property:Value" ) )
    {
        // For compound properties, Value elements contain nested EDMDUserProperty elements
        auto child = aNode.GetChildren();
        while( child )
        {
            // Create appropriate user property based on the child element
            // This is a simplified approach - in a full implementation, you'd need to determine the type
            m_values.push_back( std::make_shared<EDMD_USER_SIMPLE_PROPERTY>( *child, true ) );
            child = child->GetNext();
        }
        return true;
    }
    else
    {
        return this->EDMD_USER_PROPERTY::Parse( aNode );
    }
}

void EDMD_USER_COMPOUND_PROPERTY::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_USER_PROPERTY::Write( aNode );

    // Write compound property values
    for( auto value : m_values )
    {
        if( value )
        {
            auto valueNode = aNode.AppendChild( "property:Value" );
            value->Write( *valueNode );
        }
    }
}

bool EDMD_USER_COMPOUND_PROPERTY::Resolve( IDX_REGISTRY& aRegistry )
{
    if( !EDMD_USER_PROPERTY::Resolve( aRegistry ) )
        return false;

    for( auto& value : m_values )
    {
        if( value && !value->Resolve( aRegistry ) )
            return false;
    }
    return true;
}

// ------------------- EDMD_USER_ANY_PROPERTY_VALUE -------------------

EDMD_USER_ANY_PROPERTY_VALUE::EDMD_USER_ANY_PROPERTY_VALUE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_USER_ANY_PROPERTY_VALUE::Parse( IDX_XML_NODE& aNode )
{
    // Capture the entire XML content as a string
    m_anyValue = aNode.GetText();
    return true;
}

void EDMD_USER_ANY_PROPERTY_VALUE::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_OBJECT::Write( aNode );
    if( !m_anyValue.empty() )
        aNode.SetText( m_anyValue );
}

bool EDMD_USER_ANY_PROPERTY_VALUE::Resolve( IDX_REGISTRY& aRegistry )
{
    return idx_foundation::EDMD_OBJECT::Resolve( aRegistry );
}

// ------------------- EDMD_USER_ANY_PROPERTY -------------------

EDMD_USER_ANY_PROPERTY::EDMD_USER_ANY_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_USER_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_USER_ANY_PROPERTY::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();

    if( IDX_keyword_compare( name, "property:Value" ) )
    {
        m_value = std::make_shared<EDMD_USER_ANY_PROPERTY_VALUE>( aNode, true );
        return true;
    }
    else
    {
        return this->EDMD_USER_PROPERTY::Parse( aNode );
    }
}

void EDMD_USER_ANY_PROPERTY::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_USER_PROPERTY::Write( aNode );

    if( m_value )
    {
        auto valueNode = aNode.AppendChild( "property:Value" );
        m_value->Write( *valueNode );
    }
}

bool EDMD_USER_ANY_PROPERTY::Resolve( IDX_REGISTRY& aRegistry )
{
    if( !EDMD_USER_PROPERTY::Resolve( aRegistry ) )
        return false;

    if( m_value && !m_value->Resolve( aRegistry ) )
        return false;

    return true;
}

// ------------------- EDMD_USER_NON_VALUE_PROPERTY -------------------

EDMD_USER_NON_VALUE_PROPERTY::EDMD_USER_NON_VALUE_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_USER_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}


bool EDMD_USER_NON_VALUE_PROPERTY::Parse( IDX_XML_NODE& aNode )
{
    return this->EDMD_USER_PROPERTY::Parse( aNode );
}

void EDMD_USER_NON_VALUE_PROPERTY::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_USER_PROPERTY::Write( aNode );
}

bool EDMD_USER_NON_VALUE_PROPERTY::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_USER_PROPERTY::Resolve( aRegistry );
}

// ------------------- Missing Unit Classes -------------------

EDMD_UNIT_VOLUME::EDMD_UNIT_VOLUME( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_UNIT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

EDMD_UNIT_WEIGHT::EDMD_UNIT_WEIGHT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_UNIT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

EDMD_UNIT_WORK::EDMD_UNIT_WORK( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_UNIT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

// ------------------- Enum Property Types -------------------

EDMD_BOOLEAN_PROPERTY_TYPE::EDMD_BOOLEAN_PROPERTY_TYPE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_CONSTRAINED_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_BOOLEAN_PROPERTY_TYPE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "property:Value" ) )
    {
        m_value = std::stoi( content );
        return true;
    }
    else
    {
        return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
    }
}

void EDMD_BOOLEAN_PROPERTY_TYPE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CONSTRAINED_PROPERTY::Write( aNode );

    WriteValueChild( aNode, "property:Value", std::to_string( m_value ) );
}

bool EDMD_BOOLEAN_PROPERTY_TYPE::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_CONSTRAINED_PROPERTY::Resolve( aRegistry );
}

EDMD_CUT_OUT_SIDE_PROPERTY_TYPE::EDMD_CUT_OUT_SIDE_PROPERTY_TYPE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_CONSTRAINED_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_CUT_OUT_SIDE_PROPERTY_TYPE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "property:Value" ) )
    {
        m_value = content;
        return true;
    }
    else
    {
        return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
    }
}

void EDMD_CUT_OUT_SIDE_PROPERTY_TYPE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CONSTRAINED_PROPERTY::Write( aNode );

    WriteValueChild( aNode, "property:Value", m_value );
}

bool EDMD_CUT_OUT_SIDE_PROPERTY_TYPE::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_CONSTRAINED_PROPERTY::Resolve( aRegistry );
}

EDMD_PIN_TYPE_PROPERTY_TYPE::EDMD_PIN_TYPE_PROPERTY_TYPE( IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_CONSTRAINED_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_PIN_TYPE_PROPERTY_TYPE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "property:Value" ) )
    {
        m_value = content;
        return true;
    }
    else
    {
        return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
    }
}

void EDMD_PIN_TYPE_PROPERTY_TYPE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CONSTRAINED_PROPERTY::Write( aNode );

    WriteValueChild( aNode, "property:Value", m_value );
}

bool EDMD_PIN_TYPE_PROPERTY_TYPE::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_CONSTRAINED_PROPERTY::Resolve( aRegistry );
}

EDMD_RESTRICTED_AREA_PLACEMENT_SIDE_PROPERTY_TYPE::EDMD_RESTRICTED_AREA_PLACEMENT_SIDE_PROPERTY_TYPE(
        IDX_XML_NODE& aNode, bool aTopLevel ) :
        EDMD_CONSTRAINED_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_RESTRICTED_AREA_PLACEMENT_SIDE_PROPERTY_TYPE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "property:Value" ) )
    {
        m_value = content;
        return true;
    }
    else
    {
        return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
    }
}

void EDMD_RESTRICTED_AREA_PLACEMENT_SIDE_PROPERTY_TYPE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CONSTRAINED_PROPERTY::Write( aNode );

    WriteValueChild( aNode, "property:Value", m_value );
}

bool EDMD_RESTRICTED_AREA_PLACEMENT_SIDE_PROPERTY_TYPE::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_CONSTRAINED_PROPERTY::Resolve( aRegistry );
}

EDMD_RESTRICTED_AREA_ROUTING_SIDE_PROPERTY_TYPE::EDMD_RESTRICTED_AREA_ROUTING_SIDE_PROPERTY_TYPE( IDX_XML_NODE& aNode,
                                                                                                  bool aTopLevel ) :
        EDMD_CONSTRAINED_PROPERTY( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_RESTRICTED_AREA_ROUTING_SIDE_PROPERTY_TYPE::Parse( IDX_XML_NODE& aNode )
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if( IDX_keyword_compare( name, "property:Value" ) )
    {
        m_value = content;
        return true;
    }
    else if( IDX_keyword_compare( name, "property:Range" ) )
    {
        // TODO: implement Range parsing
        // m_range = ...
        return true;
    }
    else
    {
        return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
    }
}

void EDMD_RESTRICTED_AREA_ROUTING_SIDE_PROPERTY_TYPE::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_CONSTRAINED_PROPERTY::Write( aNode );

    WriteValueChild( aNode, "property:Value", m_value );
}

bool EDMD_RESTRICTED_AREA_ROUTING_SIDE_PROPERTY_TYPE::Resolve( IDX_REGISTRY& aRegistry )
{
    return EDMD_CONSTRAINED_PROPERTY::Resolve( aRegistry );
}

EDMD_RESTRICTED_AREA_ROUTING_SIDE_RANGE_TYPE::EDMD_RESTRICTED_AREA_ROUTING_SIDE_RANGE_TYPE( IDX_XML_NODE& aNode,
                                                                                            bool          aTopLevel ) :
        IDX_BASE()
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_RESTRICTED_AREA_ROUTING_SIDE_RANGE_TYPE::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string name = child->GetName();
        const std::string content = child->GetNodeContent();

        if( IDX_keyword_compare( name, "property:i" ) )
        {
            m_i = std::stoi( content );
        }
        else if( IDX_keyword_compare( name, "property:j" ) )
        {
            m_j = std::stoi( content );
        }
        else
        {
            unknownKeywordIDX( name );
        }
        child = child->GetNext();
    }
    return true;
}

void EDMD_RESTRICTED_AREA_ROUTING_SIDE_RANGE_TYPE::Write( IDX_XML_NODE& aNode ) const
{
    aNode.AppendChild( "property:i" )->SetText( std::to_string( m_i ) );
    aNode.AppendChild( "property:j" )->SetText( std::to_string( m_j ) );
}

bool EDMD_RESTRICTED_AREA_ROUTING_SIDE_RANGE_TYPE::Resolve( IDX_REGISTRY& aRegistry )
{
    return IDX_BASE::Resolve( aRegistry );
}

} // namespace idx_property
