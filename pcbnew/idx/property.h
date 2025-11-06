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

#ifndef IDX_PROPERTY_HEADER_H
#define IDX_PROPERTY_HEADER_H

#include <idx/idx_base.h>
#include <idx/foundation.h>
#include <wx/string.h>
#include <memory>
#include <vector>

namespace idx_foundation
{
    class EDMD_OBJECT;
}

namespace idx_property
{

class ARRAY_EDMD_NAME;
class EDMD_ANGLE_PROPERTY;
class EDMD_BOOLEAN_PROPERTY_TYPE;
class EDMD_CLEARANCE_STATE_PROPERTY;
class EDMD_COLOR_PROPERTY;
class EDMD_CONSTRAINED_PROPERTY;
class EDMD_CUTOUT_SIDE_PROPERTY;
class EDMD_DATETIME_PROPERTY;
class EDMD_INTEGER_PROPERTY;
class EDMD_LENGTH_PROPERTY;
class EDMD_LOGIC_PROPERTY;
class EDMD_PIN_TYPE_PROPERTY_TYPE;
class EDMD_PROPERTY;
class EDMD_RESTRICTED_AREA_PLACEMENT_SIDE_PROPERTY_TYPE;
class EDMD_RESTRICTED_AREA_ROUTING_SIDE_PROPERTY_TYPE;
class EDMD_RESTRICTED_AREA_ROUTING_SIDE_RANGE_TYPE;
class EDMD_UNIT;
class EDMD_UNIT_AMPERAGE;
class EDMD_UNIT_ANGLE;
class EDMD_UNIT_CAPACITANCE;
class EDMD_UNIT_LENGTH;
class EDMD_UNIT_POWER;
class EDMD_UNIT_SURFACE;
class EDMD_UNIT_VOLTAGE;
class EDMD_UNIT_VOLUME;
class EDMD_UNIT_WEIGHT;
class EDMD_UNIT_WORK;
class EDMD_USER_PROPERTY;

class ARRAY_EDMD_NAME : public IDX_BASE
{
public:
    ARRAY_EDMD_NAME(){};
    ARRAY_EDMD_NAME(wxXmlNode& aNode, bool aTopLevel);
    std::vector<wxString> m_EDMDname;
};

class EDMD_CONSTRAINED_PROPERTY : public idx_foundation::EDMD_OBJECT
{
public:
    EDMD_CONSTRAINED_PROPERTY() {};
    EDMD_CONSTRAINED_PROPERTY(wxXmlNode& aNode, bool aTopLevel);

    bool Parse(wxXmlNode& aNode);

    std::vector<EDMD_UNIT>                           m_validUnit;
    std::vector<wxString>                            m_validValues;
    std::vector<EDMD_CONSTRAINED_PROPERTY>           m_maxValue;
    std::vector<EDMD_CONSTRAINED_PROPERTY>           m_minValue;
    std::shared_ptr<idx_administration::EDMD_PERSON> m_originator;
    bool                                             m_isChanged = false;
    bool                                             m_isNew = false;

    wxString                                         m_unit;
};

class EDMD_USER_PROPERTY : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_USER_PROPERTY(){};
    EDMD_USER_PROPERTY(wxXmlNode& aNode, bool aTopLevel);
    bool Parse(wxXmlNode& aNode);

    std::vector<idx_foundation::EDMD_NAME> m_keys;
    wxString m_value;
};

enum class UNIT_FUNDAMENTAL
{
    UNDEFINED = 0,
    SURFACE,
    VOLUME,
    LENGTH,
    ANGLE,
    WEIGHT,
    AMPERAGE,
    VOLTAGE,
    POWER,
    WORK,
    CAPACITANCE,
};

// ------------------- EDMD_UNIT -------------------

class EDMD_UNIT : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_UNIT(){};
    EDMD_UNIT(wxXmlNode& aNode, bool aTopLevel);
    bool Parse(wxXmlNode& aNode);

    wxString                   m_unitName;
    double                     m_scalingFactor = 1.0;
    wxString                   m_fundamental = "";
    std::shared_ptr<EDMD_UNIT> m_dependsOn = nullptr;

protected:
    wxString m_dependsOnKey = "";
};

// ------------------- Subclasses -------------------

class EDMD_UNIT_AMPERAGE : public EDMD_UNIT
{
public:
    EDMD_UNIT_AMPERAGE(){};
    EDMD_UNIT_AMPERAGE(wxXmlNode& aNode, bool aTopLevel);
};

class EDMD_UNIT_ANGLE : public EDMD_UNIT
{
public:
    EDMD_UNIT_ANGLE(){};
    EDMD_UNIT_ANGLE(wxXmlNode& aNode, bool aTopLevel);
};

class EDMD_UNIT_CAPACITANCE : public EDMD_UNIT
{
public:
    EDMD_UNIT_CAPACITANCE(){};
    EDMD_UNIT_CAPACITANCE(wxXmlNode& aNode, bool aTopLevel);
};

class EDMD_UNIT_LENGTH : public EDMD_UNIT
{
public:
    EDMD_UNIT_LENGTH(){};
    EDMD_UNIT_LENGTH(wxXmlNode& aNode, bool aTopLevel);
};

class EDMD_UNIT_POWER : public EDMD_UNIT
{
public:
    EDMD_UNIT_POWER(){};
    EDMD_UNIT_POWER(wxXmlNode& aNode, bool aTopLevel);
};

class EDMD_UNIT_RESISTANCE : public EDMD_UNIT
{
public:
    EDMD_UNIT_RESISTANCE(){};
    EDMD_UNIT_RESISTANCE(wxXmlNode& aNode, bool aTopLevel);
};

class EDMD_UNIT_SURFACE : public EDMD_UNIT
{
public:
    EDMD_UNIT_SURFACE(){};
    EDMD_UNIT_SURFACE(wxXmlNode& aNode, bool aTopLevel);
};

// ------------------- EDMD_ANGLE_PROPERTY -------------------

class EDMD_ANGLE_PROPERTY : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_ANGLE_PROPERTY(){};
    EDMD_ANGLE_PROPERTY(wxXmlNode& aNode, bool aTopLevel);
    bool Parse(wxXmlNode& aNode);

    double m_value = 0.0;
};

// ------------------- EDMD_LENGTH_PROPERTY -------------------

class EDMD_LENGTH_PROPERTY : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_LENGTH_PROPERTY(){};
    EDMD_LENGTH_PROPERTY(wxXmlNode& aNode, bool aTopLevel);
    bool Parse(wxXmlNode& aNode);

    double m_value = 0.0;
};

} // namespace idx_property

#endif // IDX_PROPERTY_HEADER_H
