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

#include <idx/kidx/schema/idx_base.h>
#include <idx/kidx/schema/foundation.h>
#include <wx/string.h>
#include <memory>
#include <vector>
#include <string>

namespace idx_foundation
{
class EDMD_OBJECT;
}

namespace idx_property
{

class ARRAY_EDMD_NAME;
class EDMD_ANGLE_PROPERTY;
class EDMD_ANY_PROPERTY_VALUE;
class EDMD_BOOLEAN_PROPERTY;
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
class EDMD_UNIT_RESISTANCE;
class EDMD_UNIT_SURFACE;
class EDMD_UNIT_VOLTAGE;
class EDMD_UNIT_VOLUME;
class EDMD_UNIT_WEIGHT;
class EDMD_UNIT_WORK;
class EDMD_USER_PROPERTY;
class EDMD_USER_SIMPLE_PROPERTY;
class EDMD_USER_COMPOUND_PROPERTY;
class EDMD_USER_ANY_PROPERTY;
class EDMD_USER_ANY_PROPERTY_VALUE;
class EDMD_USER_NON_VALUE_PROPERTY;

class ARRAY_EDMD_NAME : public IDX_BASE
{
public:
    ARRAY_EDMD_NAME() :
            IDX_BASE() {};
    ARRAY_EDMD_NAME( IDX_XML_NODE& aNode, bool aTopLevel );
    std::vector<std::string> m_EDMDname;
};

class EDMD_CONSTRAINED_PROPERTY : public idx_foundation::EDMD_OBJECT
{
public:
    EDMD_CONSTRAINED_PROPERTY() :
            idx_foundation::EDMD_OBJECT() {};
    EDMD_CONSTRAINED_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel );

    bool Parse( IDX_XML_NODE& aNode ) override;

    std::vector<std::shared_ptr<EDMD_UNIT>>                 m_validUnit;
    std::vector<std::string>                                m_validValues;
    std::vector<std::shared_ptr<EDMD_CONSTRAINED_PROPERTY>> m_maxValue;
    std::vector<std::shared_ptr<EDMD_CONSTRAINED_PROPERTY>> m_minValue;
    std::shared_ptr<idx_administration::EDMD_PERSON>        m_originator;
    std::optional<bool>                                     m_isChanged;
    std::optional<bool>                                     m_isNew;

    std::string m_unit;
};

class EDMD_USER_PROPERTY : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_USER_PROPERTY() :
            EDMD_CONSTRAINED_PROPERTY() {};
    EDMD_USER_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel );
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    std::vector<std::shared_ptr<idx_foundation::EDMD_NAME>> m_keys;
    std::string                                             m_value;
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
    EDMD_UNIT() {};
    EDMD_UNIT( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    std::string                m_unitName;
    double                     m_scalingFactor = 1.0;
    std::string                m_fundamental = "";
    std::shared_ptr<EDMD_UNIT> m_dependsOn = nullptr;

protected:
    std::string m_dependsOnKey = "";
};

// ------------------- Subclasses -------------------

class EDMD_UNIT_AMPERAGE : public EDMD_UNIT
{
public:
    EDMD_UNIT_AMPERAGE() {};
    EDMD_UNIT_AMPERAGE( IDX_XML_NODE& aNode, bool aTopLevel );
};

class EDMD_UNIT_ANGLE : public EDMD_UNIT
{
public:
    EDMD_UNIT_ANGLE() {};
    EDMD_UNIT_ANGLE( IDX_XML_NODE& aNode, bool aTopLevel );
};

class EDMD_UNIT_CAPACITANCE : public EDMD_UNIT
{
public:
    EDMD_UNIT_CAPACITANCE() {};
    EDMD_UNIT_CAPACITANCE( IDX_XML_NODE& aNode, bool aTopLevel );
};

class EDMD_UNIT_LENGTH : public EDMD_UNIT
{
public:
    EDMD_UNIT_LENGTH() {};
    EDMD_UNIT_LENGTH( IDX_XML_NODE& aNode, bool aTopLevel );
};

class EDMD_UNIT_POWER : public EDMD_UNIT
{
public:
    EDMD_UNIT_POWER() {};
    EDMD_UNIT_POWER( IDX_XML_NODE& aNode, bool aTopLevel );
};

class EDMD_UNIT_RESISTANCE : public EDMD_UNIT
{
public:
    EDMD_UNIT_RESISTANCE() {};
    EDMD_UNIT_RESISTANCE( IDX_XML_NODE& aNode, bool aTopLevel );
};

class EDMD_UNIT_SURFACE : public EDMD_UNIT
{
public:
    EDMD_UNIT_SURFACE() {};
    EDMD_UNIT_SURFACE( IDX_XML_NODE& aNode, bool aTopLevel );
};

// ------------------- EDMD_LENGTH_PROPERTY -------------------

class EDMD_LENGTH_PROPERTY : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_LENGTH_PROPERTY() {};
    EDMD_LENGTH_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    double m_value = 0.0;
};


// ------------------- EDMD_LOGIC_PROPERTY -------------------

class EDMD_LOGIC_PROPERTY : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_LOGIC_PROPERTY() {};
    EDMD_LOGIC_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    double m_value = true;
};

// ------------------- EDMD_PROPERTY -------------------

class EDMD_PROPERTY : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_PROPERTY() {};
    EDMD_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    std::string m_value;
    std::string m_unit;
};


// ------------------- EDMD_ANGLE_PROPERTY -------------------

class EDMD_ANGLE_PROPERTY : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_ANGLE_PROPERTY() {};
    EDMD_ANGLE_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    double      m_value = 0.0;
    std::string m_unit;
};

// ------------------- EDMD_DATETIME_PROPERTY -------------------

class EDMD_DATETIME_PROPERTY : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_DATETIME_PROPERTY() {};
    EDMD_DATETIME_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    std::string m_value; // xs:dateTime
};

// ------------------- EDMD_INTEGER_PROPERTY -------------------

class EDMD_INTEGER_PROPERTY : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_INTEGER_PROPERTY() {};
    EDMD_INTEGER_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    int         m_value = 0;
    std::string m_unit;
};

// ------------------- EDMD_COLOR_PROPERTY -------------------

class EDMD_COLOR_PROPERTY : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_COLOR_PROPERTY() {};
    EDMD_COLOR_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    int  m_value = 0;
    bool m_mixed = false;
};

// ------------------- EDMD_CLEARANCE_STATE_PROPERTY -------------------

class EDMD_CLEARANCE_STATE_PROPERTY : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_CLEARANCE_STATE_PROPERTY() {};
    EDMD_CLEARANCE_STATE_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    std::vector<idx_foundation::EDMD_NAME> m_stateName;
    bool                                   m_accept = true;
};

// ------------------- EDMD_USER_SIMPLE_PROPERTY -------------------

class EDMD_USER_SIMPLE_PROPERTY : public EDMD_USER_PROPERTY
{
public:
    EDMD_USER_SIMPLE_PROPERTY() {};
    EDMD_USER_SIMPLE_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    std::string m_simpleValue;
    std::string m_unit;
};

// ------------------- EDMD_USER_COMPOUND_PROPERTY -------------------

class EDMD_USER_COMPOUND_PROPERTY : public EDMD_USER_PROPERTY
{
public:
    EDMD_USER_COMPOUND_PROPERTY() {};
    EDMD_USER_COMPOUND_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    std::vector<std::shared_ptr<EDMD_USER_PROPERTY>> m_values;
};

// ------------------- EDMD_USER_ANY_PROPERTY_VALUE -------------------

class EDMD_USER_ANY_PROPERTY_VALUE : public idx_foundation::EDMD_OBJECT
{
public:
    EDMD_USER_ANY_PROPERTY_VALUE() {};
    EDMD_USER_ANY_PROPERTY_VALUE( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    // Contains any XML data
    std::string m_anyValue;
};

// ------------------- EDMD_USER_ANY_PROPERTY -------------------

class EDMD_USER_ANY_PROPERTY : public EDMD_USER_PROPERTY
{
public:
    EDMD_USER_ANY_PROPERTY() {};
    EDMD_USER_ANY_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    std::shared_ptr<EDMD_USER_ANY_PROPERTY_VALUE> m_value;
};

// ------------------- EDMD_USER_NON_VALUE_PROPERTY -------------------

class EDMD_USER_NON_VALUE_PROPERTY : public EDMD_USER_PROPERTY
{
public:
    EDMD_USER_NON_VALUE_PROPERTY() {};
    EDMD_USER_NON_VALUE_PROPERTY( IDX_XML_NODE& aNode, bool aTopLevel );
};

// ------------------- Missing Unit Classes -------------------

class EDMD_UNIT_VOLUME : public EDMD_UNIT
{
public:
    EDMD_UNIT_VOLUME() {};
    EDMD_UNIT_VOLUME( IDX_XML_NODE& aNode, bool aTopLevel );
};

class EDMD_UNIT_WEIGHT : public EDMD_UNIT
{
public:
    EDMD_UNIT_WEIGHT() {};
    EDMD_UNIT_WEIGHT( IDX_XML_NODE& aNode, bool aTopLevel );
};

class EDMD_UNIT_WORK : public EDMD_UNIT
{
public:
    EDMD_UNIT_WORK() {};
    EDMD_UNIT_WORK( IDX_XML_NODE& aNode, bool aTopLevel );
};

// ------------------- Enum Property Types -------------------

class EDMD_BOOLEAN_PROPERTY_TYPE : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_BOOLEAN_PROPERTY_TYPE() {};
    EDMD_BOOLEAN_PROPERTY_TYPE( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    int m_value = -1; // EDMDBooleanRestrictionType: -1 or 1
};

class EDMD_CUTOUT_SIDE_PROPERTY_TYPE : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_CUTOUT_SIDE_PROPERTY_TYPE() {};
    EDMD_CUTOUT_SIDE_PROPERTY_TYPE( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    std::string m_value; // Top, Bottom, All
};

class EDMD_PIN_TYPE_PROPERTY_TYPE : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_PIN_TYPE_PROPERTY_TYPE() {};
    EDMD_PIN_TYPE_PROPERTY_TYPE( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    std::string m_value; // I, L, Z, C
};

class EDMD_RESTRICTED_AREA_PLACEMENT_SIDE_PROPERTY_TYPE : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_RESTRICTED_AREA_PLACEMENT_SIDE_PROPERTY_TYPE() {};
    EDMD_RESTRICTED_AREA_PLACEMENT_SIDE_PROPERTY_TYPE( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    std::string m_value; // Both, Top, Bottom
};

class EDMD_RESTRICTED_AREA_ROUTING_SIDE_PROPERTY_TYPE : public EDMD_CONSTRAINED_PROPERTY
{
public:
    EDMD_RESTRICTED_AREA_ROUTING_SIDE_PROPERTY_TYPE() {};
    EDMD_RESTRICTED_AREA_ROUTING_SIDE_PROPERTY_TYPE( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    std::string m_value; // Both, Top, Bottom, All, Inner
};

class EDMD_RESTRICTED_AREA_ROUTING_SIDE_RANGE_TYPE : public IDX_BASE
{
public:
    EDMD_RESTRICTED_AREA_ROUTING_SIDE_RANGE_TYPE() {};
    EDMD_RESTRICTED_AREA_ROUTING_SIDE_RANGE_TYPE( IDX_XML_NODE& aNode, bool aTopLevel );
    bool Parse( IDX_XML_NODE& aNode ) override;

    int m_i = 0;
    int m_j = 0;
};

} // namespace idx_property

#endif // IDX_PROPERTY_HEADER_H
