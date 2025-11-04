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

#include <idx/property.h>
#include <iostream>

namespace idx_property
{

// ------------------- ARRAY_EDMD_NAME -------------------

ARRAY_EDMD_NAME::ARRAY_EDMD_NAME(wxXmlNode& aNode, bool aTopLevel) : IDX_BASE() 
{
    if ( aTopLevel )
        ParseChildren( aNode );
};

// ------------------- EDMD_CONSTRAINED_PROPERTY -------------------

EDMD_CONSTRAINED_PROPERTY::EDMD_CONSTRAINED_PROPERTY(wxXmlNode& aNode, bool aTopLevel )
    : idx_foundation::EDMD_OBJECT(aNode, false)
{
    if ( aTopLevel )
        ParseChildren( aNode );
};

bool EDMD_CONSTRAINED_PROPERTY::Parse( wxXmlNode& aNode )
{
    return this->idx_foundation::EDMD_OBJECT::Parse( aNode );
}

// ------------------- EDMD_USER_PROPERTY -------------------

EDMD_USER_PROPERTY::EDMD_USER_PROPERTY( wxXmlNode& aNode, bool aTopLevel )
    : EDMD_CONSTRAINED_PROPERTY(aNode, false)
{
    if ( aTopLevel )
        ParseChildren( aNode );
};


bool EDMD_USER_PROPERTY::Parse( wxXmlNode& aNode )
{
    const wxString name = aNode.GetName();
    const wxString content = aNode.GetNodeContent();

    if (name == "property:Key")
    {
        m_keys.emplace_back( aNode, true );
        return true;
    }
    else if (name == "property:Value")
    {
        m_value = content;
        return true;
    }

    return this->EDMD_CONSTRAINED_PROPERTY::Parse( aNode );
}

// ------------------- EDMD_UNIT -------------------

EDMD_UNIT::EDMD_UNIT(wxXmlNode& aNode, bool aTopLevel)
    : idx_foundation::EDMD_COLLABORATION_OBJECT(aNode, false)
{
    if ( aTopLevel )
        ParseChildren(aNode);
}

bool EDMD_UNIT::Parse(wxXmlNode& aNode)
{
    const wxString name = aNode.GetName();
    const wxString content = aNode.GetNodeContent();

    if (name == "foundation:UnitName")
        m_unitName = content;
    else if (name == "property:Fundamental")
        m_fundamental = content;
    else if (name == "foundation:DependendScaleFactor")
        content.ToCDouble(&m_scalingFactor);
    else if (name == "foundation:DependsOn")
        m_dependsOnKey = content;
    else
        return this->idx_foundation::EDMD_COLLABORATION_OBJECT::Parse(aNode);

    return true;
}

// ------------------- EDMD_UNIT Subclasses -------------------

EDMD_UNIT_AMPERAGE::EDMD_UNIT_AMPERAGE(wxXmlNode& aNode, bool aTopLevel) : EDMD_UNIT(aNode, false)
{
    if ( aTopLevel )
        ParseChildren( aNode );
}


EDMD_UNIT_ANGLE::EDMD_UNIT_ANGLE(wxXmlNode& aNode, bool aTopLevel) : EDMD_UNIT(aNode, false) {
    if ( aTopLevel )
        ParseChildren( aNode );
}


EDMD_UNIT_CAPACITANCE::EDMD_UNIT_CAPACITANCE(wxXmlNode& aNode, bool aTopLevel) : EDMD_UNIT(aNode, false) {
    if ( aTopLevel )
        ParseChildren( aNode );
}


EDMD_UNIT_LENGTH::EDMD_UNIT_LENGTH(wxXmlNode& aNode, bool aTopLevel) : EDMD_UNIT(aNode, false)
{
    if ( aTopLevel )
        ParseChildren( aNode );
}


EDMD_UNIT_POWER::EDMD_UNIT_POWER(wxXmlNode& aNode, bool aTopLevel) : EDMD_UNIT(aNode, false)
{
    if ( aTopLevel )
        ParseChildren( aNode );
}


EDMD_UNIT_RESISTANCE::EDMD_UNIT_RESISTANCE(wxXmlNode& aNode, bool aTopLevel) : EDMD_UNIT(aNode, false)
{
    if ( aTopLevel )
        ParseChildren( aNode );
}


EDMD_UNIT_SURFACE::EDMD_UNIT_SURFACE(wxXmlNode& aNode, bool aTopLevel) : EDMD_UNIT(aNode, false)
{
    if ( aTopLevel )
        ParseChildren( aNode );
}

// ------------------- EDMD_ANGLE_PROPERTY -------------------

EDMD_ANGLE_PROPERTY::EDMD_ANGLE_PROPERTY(wxXmlNode& aNode, bool aTopLevel)
    : EDMD_CONSTRAINED_PROPERTY(aNode, false)
{
    if ( aTopLevel )
        ParseChildren(aNode);
}

bool EDMD_ANGLE_PROPERTY::Parse(wxXmlNode& aNode)
{
    const wxString name = aNode.GetName();
    const wxString content = aNode.GetNodeContent();

    if (name == "property:Value")
        return content.ToCDouble(&m_value);

    return this->EDMD_CONSTRAINED_PROPERTY::Parse(aNode);
}

// ------------------- EDMD_LENGTH_PROPERTY -------------------

EDMD_LENGTH_PROPERTY::EDMD_LENGTH_PROPERTY(wxXmlNode& aNode, bool aTopLevel)
    : EDMD_CONSTRAINED_PROPERTY(aNode, false)
{
    if ( aTopLevel )
        ParseChildren(aNode);
}

bool EDMD_LENGTH_PROPERTY::Parse(wxXmlNode& aNode)
{
    const wxString name = aNode.GetName();
    const wxString content = aNode.GetNodeContent();

    if (name == "property:Value")
        return content.ToCDouble(&m_value);

    return this->EDMD_CONSTRAINED_PROPERTY::Parse(aNode);
}

} // namespace idx_property
