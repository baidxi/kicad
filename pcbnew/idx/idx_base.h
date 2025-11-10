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

#ifndef IDX_BASE_H
#define IDX_BASE_H


#include <map>
#include <wx/datetime.h>
#include <wx/xml/xml.h>
#include <iostream>
#include <memory>
#include <typeindex>
#include <iostream>
#include <sstream>
#include <string>

// Here we define some helper functions, we don't want to use std::string.
// Hopefuly, this code will be shared with open sources MCAD softwares

bool IDX_keyword_compare( std::string a, std::string b );


class IDX_XML_NODE;

namespace idx_administration
{
class EDMD_PERSON;
class EDMD_ACTOR;
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
}


namespace idx_geometry
{
class EDMD_ARC;
class EDMD_BSPLINE_CURVE;
class EDMD_CARTESIAN_POINT;
class EDMD_CIRCLE;
class EDMD_CIRCLE_3POINTS;
class EDMD_CIRCLE_CENTER;
class EDMD_COMPOSITE_CURVE;
class EDMD_CURVE;
class EDMD_CURVESET2D;
class EDMD_DETAILED_GEOMETRIC_MODEL_ELEMENT;
class EDMD_ELLIPSE;
class EDMD_HYPERBOLA;
class EDMD_LINE;
class EDMD_OFFSET_CURVE;
class EDMD_PARABOLA;
class EDMD_POINT;
class EDMD_POINT_ON_CURVE_DISTANCE;
class EDMD_POINT_ON_CURVE_INTERSECTION;
class EDMD_POLYLINE;
class EDMD_TRIMMED_CURVE;
}


namespace idx_pdm
{
class EDMD_SHAPE_ELEMENT;
class EDMD_FUNCTIONAL_ITEM_SHAPE;
class EDMD_STRATUM;
class EDMD_INTERSTRATUM_FEATURE;
class EDMD_KEEP_CONSTRAINT;
class EDMD_ITEM;
class EDMD_ITEM_INSTANCE;
class EDMD_ASSEMBLY_COMPONENT;
}

namespace idx_annotation
{
class EDMD_HISTORY;
}

namespace idx_foundation
{
class EDMD_COLLABORATION_OBJECT;
class EDMD_DATASET;
class EDMD_DATASET_BODY;
class EDMD_DESIGNATOR;
class EDMD_EXTENSIBLE_OBJECT;
class EDMD_HEADER;
class EDMD_IDENTIFIER;
class EDMD_NAME;
class EDMD_OBJECT;
class EDMD_PROCESS_INSTRUCTION;
class EDMD_SYSTEM;
}


class IDX_BASE;

// Registry to manage different types of objects
class IDX_REGISTRY
{
public:
    // Register a new object of any type
    template <typename T>
    void registerObject( const std::string& name, std::shared_ptr<T> object )
    {
        auto& registry = getRegistry<T>();
        registry[name] = object;
    }

    template <typename T>
    void resolveRegistryFailed( std::string aName )
    {
        std::cerr << "Warning: Object " << aName << " not found in registry. ( " << typeid( T ).name() << " )"
                    << std::endl;
    }


    // Resolve an object of any type by name
    template <typename T>
    std::shared_ptr<IDX_BASE> resolve( const std::string& name, bool suppressWarning = false )
    {
        const auto& registry = getRegistry<T>();
        auto        it = registry.find( name );
        if( it != registry.end() )
        {
            return std::dynamic_pointer_cast<T>( it->second );
        }
        else
        {
            if( !suppressWarning )
                resolveRegistryFailed<T>( name );
            return nullptr;
        }
    }

private:
    // Helper function to get the registry for a specific type
    template <typename T>
    std::unordered_map<std::string, std::shared_ptr<IDX_BASE>>& getRegistry()
    {
        return registries[typeid( T )];
    }

    std::unordered_map<std::type_index, std::unordered_map<std::string, std::shared_ptr<IDX_BASE>>>
            registries;
};

class IDX_BASE
{
public:
    IDX_BASE() {};
    virtual ~IDX_BASE() = default; // Virtual destructor makes the class polymorphic
    bool Resolve( IDX_REGISTRY& aRegistry );


    enum OPTIONAL_LOGIC: int
    {
        UNSPECIFIED=-1,
        LOGIC_FALSE=0,
        LOGIC_TRUE=1
    };

    enum REPORT_LEVEL: int
    {
        INFO = 0,
        ACTION,
        WARNING,
        ERROR
    };


double parseDouble(const std::string& inputStr)
{
    std::string str = inputStr;

    // Remove leading/trailing whitespace
    str.erase(0, str.find_first_not_of(" \t\n\r"));
    str.erase(str.find_last_not_of(" \t\n\r") + 1);

    // Replace comma with dot
    std::replace(str.begin(), str.end(), ',', '.');

    // Use stringstream with C locale to ensure '.' is decimal separator
    std::istringstream iss(str);
    iss.imbue(std::locale::classic()); // "C" locale

    double value;
    if (!(iss >> value)) {
        std::cerr << "Error: '" << inputStr << "' is not a valid double.\n";
        return 0.0;
    }

    // Make sure there are no extra characters after the number
    char leftover;
    if (iss >> leftover) {
        std::cerr << "Error: Extra characters after number in '" << inputStr << "'.\n";
        return 0.0;
    }

    return value;
}
protected:

    // Function to report in your CAD software
    void Report( std::string aMsg, REPORT_LEVEL aLvl );
    void ParseChildren( IDX_XML_NODE& node );

    virtual bool Parse( IDX_XML_NODE& aNode ) { return false; };
    bool m_topLevel = false;

    void unknownKeywordIDX( std::string aName );
    void invalidValueIDX(std::string aKeyword, std::string aValue);



    template <typename T, typename U>
    bool resolve( std::string aName, std::shared_ptr<U>* aDest, IDX_REGISTRY& aRegistry )
    {
        if( !aDest )
            return false;

        auto resolvedPoint = aRegistry.resolve<T>( aName );

        if( !resolvedPoint )
            return false;

        *aDest = std::dynamic_pointer_cast<U> ( resolvedPoint )     ;
        return true;
    }

    template <typename T, typename U>
    bool resolve( std::vector<std::string>& aName, std::vector<std::shared_ptr<U>>& aDest, IDX_REGISTRY& aRegistry )
    {
        bool result = true;

        for( std::string& name : aName )
        {
            std::shared_ptr<U> tmp;

            if( resolve<U>( name, &tmp, aRegistry ) )
                aDest.push_back( tmp );
            else
                result = false;
        }

        aName = {}; // To prevent double inclusions.
        return result;
    }
};


class IDX_XML_NODE
{
public:
    virtual ~IDX_XML_NODE() = default;

    // --- Basic accessors ---
    virtual std::string GetName() const = 0;
    virtual std::string GetAttribute(const std::string& name, const std::string& defaultValue) const = 0;
    virtual std::string GetNodeContent() const = 0;
    virtual void SetAttribute(const std::string& name, const std::string& value) = 0;

    virtual std::string GetText() const = 0;
    virtual void SetText(const std::string& text) = 0;

    // --- Tree navigation ---
    virtual std::shared_ptr<IDX_XML_NODE> GetParent() const = 0;
    virtual std::shared_ptr<IDX_XML_NODE> GetChildren() const = 0;
    virtual std::shared_ptr<IDX_XML_NODE> AppendChild(const std::string& name) = 0;
    virtual std::shared_ptr<IDX_XML_NODE> GetNext() const = 0;

    // --- Utility ---
    virtual void RemoveChild(const std::shared_ptr<IDX_XML_NODE>& child) = 0;
};


#endif