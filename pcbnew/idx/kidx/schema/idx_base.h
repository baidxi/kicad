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
#include <unordered_map>
#include <optional>

bool IDX_keyword_compare( std::string a, std::string b );

class IDX_XML_NODE;

namespace idx_administration
{
class EDMD_PERSON;
class EDMD_ACTOR;
} // namespace idx_administration

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
} // namespace idx_property

namespace idx_geometry
{
class EDMD_ARC;
class EDMD_BSPLINE_CURVE;
class EDMD_CARTESIAN_POINT;
class EDMD_CIRCLE;
class EDMD_CIRCLE_3_POINTS;
class EDMD_CIRCLE_CENTER;
class EDMD_COMPOSITE_CURVE;
class EDMD_CURVE;
class EDMD_CURVE_SET_2D;
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
} // namespace idx_geometry

namespace idx_pdm
{
class EDMD_SHAPE_ELEMENT;
class EDMD_FUNCTIONAL_ITEM_SHAPE;
class EDMD_STRATUM;
class EDMD_INTER_STRATUM_FEATURE;
class EDMD_KEEP_CONSTRAINT;
class EDMD_ITEM;
class EDMD_ITEM_INSTANCE;
class EDMD_ASSEMBLY_COMPONENT;
class EDMD_PACKAGE_PIN;
class EDMD_3D_MODEL;
class EDMD_BEND;
class EDMD_BEND_TYPE;
class EDMD_CIRCULAR_BEND_TYPE;
} // namespace idx_pdm

namespace idx_annotation
{
class EDMD_HISTORY;
class EDMD_ANNOTATION;
class EDMD_HISTORY_ENTRY;
class EDMD_HISTORY_ENTRY_CHANGE_ITEM_INSTANCE;
class EDMD_HISTORY_ENTRY_ADD_COMMENT;
class EDMD_HISTORY_ENTRY_CHANGE_OWNERSHIP;
class EDMD_HISTORY_ENTRY_ADD_ITEM_INSTANCE;
class EDMD_HISTORY_ENTRY_CHANGE_ACCEPTANCE;
class ARRAY_OF_EDMD_NAME;
} // namespace idx_annotation

namespace idx_text
{
class EDMD_SIMPLE_TEXT;
}

namespace idx_grouping
{
class EDMD_GROUP;
class EDMD_ITEM_CONSTRAINT;
class EDMD_GENERAL_CLASSIFICATION;
} // namespace idx_grouping

namespace idx_material
{
class EDMD_MATERIAL;
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
} // namespace idx_foundation


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

    // Resolve object by name. Return shared_ptr<IDX_BASE>.
    template <typename T>
    std::shared_ptr<IDX_BASE> resolve( const std::string& name, bool suppressWarning )
    {
        const auto& registry = getRegistry<T>();
        auto        it = registry.find( name );

        if( it != registry.end() )
        {
            return it->second; // return raw stored pointer
        }
        else
        {
            std::cerr << "FAILED : Object " << name << " not found in registry. ( " << typeid( T ).name() << " )"
                      << std::endl;

            if( !suppressWarning )
                resolveRegistryFailed<T>( name );

            return nullptr;
        }
    }

private:
    template <typename T>
    std::unordered_map<std::string, std::shared_ptr<IDX_BASE>>& getRegistry()
    {
        return registries[typeid( T )];
    }

    std::unordered_map<std::type_index, std::unordered_map<std::string, std::shared_ptr<IDX_BASE>>> registries;
};

class IDX_BASE
{
public:
    IDX_BASE() {};
    virtual ~IDX_BASE() = default;
    virtual bool Resolve( IDX_REGISTRY& aRegistry );

    enum IDX_REPORT_LEVEL : int
    {
        IDX_INFO = 0,
        IDX_ACTION,
        IDX_WARNING,
        IDX_ERROR
    };

    double parseDouble( const std::string& inputStr )
    {
        std::string str = inputStr;

        // Remove leading/trailing whitespace
        str.erase( 0, str.find_first_not_of( " \t\n\r" ) );
        str.erase( str.find_last_not_of( " \t\n\r" ) + 1 );

        // Replace comma with dot
        std::replace( str.begin(), str.end(), ',', '.' );

        // Use stringstream with C locale to ensure '.' is decimal separator
        std::istringstream iss( str );
        iss.imbue( std::locale::classic() ); // "C" locale

        double value;
        if( !( iss >> value ) )
        {
            std::cerr << "Error: '" << inputStr << "' is not a valid double.\n";
            return 0.0;
        }

        // Make sure there are no extra characters after the number
        char leftover;
        if( iss >> leftover )
        {
            std::cerr << "Error: Extra characters after number in '" << inputStr << "'.\n";
            return 0.0;
        }

        return value;
    }

    bool parseBool( const std::string& inputStr )
    {
        std::string str = inputStr;
        str.erase( 0, str.find_first_not_of( " \t\n\r" ) );
        str.erase( str.find_last_not_of( " \t\n\r" ) + 1 );

        if( str == "true" || str == "1" )
            return true;
        else if( str == "false" || str == "0" )
            return false;
        else
        {
            std::cerr << "Warning: '" << inputStr << "' is not a valid boolean. Defaulting to false." << std::endl;
            return false;
        }
    }

    int parseInt( const std::string& inputStr )
    {
        std::string str = inputStr;
        str.erase( 0, str.find_first_not_of( " \t\n\r" ) );
        str.erase( str.find_last_not_of( " \t\n\r" ) + 1 );

        try
        {
            return std::stoi( str );
        }
        catch( const std::exception& e )
        {
            std::cerr << "Error: '" << inputStr << "' is not a valid integer." << std::endl;
            return 0;
        }
    }


protected:
    void         Report( std::string aMsg, IDX_REPORT_LEVEL aLvl ) const;
    void         ParseChildren( IDX_XML_NODE& node );
    virtual bool Parse( IDX_XML_NODE& aNode ) { return false; };

    bool m_topLevel = false;

    void unknownKeywordIDX( std::string aName );
    void invalidValueIDX( std::string aKeyword, std::string aValue );

    template <typename T, typename U>
    bool resolve( std::string aName, std::shared_ptr<U>* aDest, IDX_REGISTRY& aRegistry, bool aSuppressWarning = false )
    {
        if( !aDest )
            return false;

        auto resolvedPoint = aRegistry.resolve<U>( aName, aSuppressWarning );

        if( !resolvedPoint )
            return false;

        // Single dynamic cast
        *aDest = std::dynamic_pointer_cast<U>( resolvedPoint );

        return static_cast<bool>( *aDest );
    }

    template <typename T, typename U>
    bool resolve( std::string aName, std::shared_ptr<U>& aDest, IDX_REGISTRY& aRegistry, bool aSuppressWarning = false )
    {
        auto resolvedPoint = aRegistry.resolve<U>( aName, aSuppressWarning );

        if( !resolvedPoint )
            return false;

        // Single dynamic cast
        aDest = std::dynamic_pointer_cast<U>( resolvedPoint );

        return static_cast<bool>( aDest );
    }

    template <typename T, typename U>
    bool resolve( std::vector<std::string>& aName, std::vector<std::shared_ptr<U>>& aDest, IDX_REGISTRY& aRegistry )
    {
        bool result = true;

        for( std::string& name : aName )
        {
            std::shared_ptr<U> tmp;

            if( resolve<T, U>( name, &tmp, aRegistry ) )
            {
                aDest.push_back( tmp );
            }

            else
            {
                result = false;
            }
        }

        aName = {};
        return result;
    }
};

// -----------------------------------------------------------------------------
// IDX_XML_NODE
// -----------------------------------------------------------------------------

class IDX_XML_NODE
{
public:
    virtual ~IDX_XML_NODE() = default;

    virtual void AddAttribute( const std::string& name, const std::string& Value ) = 0;

    virtual std::string GetName() const = 0;
    virtual void        SetName( const std::string& ) = 0;
    virtual std::string GetAttribute( const std::string& name, const std::string& defaultValue ) const = 0;
    virtual std::string GetNodeContent() const = 0;
    virtual void        SetAttribute( const std::string& name, const std::string& value ) = 0;

    void SetAttribute( const std::string& name, bool value )
    {
        this->SetAttribute( name, std::string( value ? "true" : "false" ) );
    }


    virtual std::string GetText() const = 0;
    virtual void        SetText( const std::string& text ) = 0;

    virtual std::shared_ptr<IDX_XML_NODE> GetParent() const = 0;
    virtual std::shared_ptr<IDX_XML_NODE> GetChildren() const = 0;
    virtual std::shared_ptr<IDX_XML_NODE> AppendChild( const std::string& name ) = 0;
    virtual std::shared_ptr<IDX_XML_NODE> GetNext() const = 0;

    virtual void RemoveChild( const std::shared_ptr<IDX_XML_NODE>& child ) = 0;
};

class IDX_XML_DOCUMENT
{
public:
    IDX_XML_DOCUMENT() {};
    virtual ~IDX_XML_DOCUMENT() = default;
};

#endif
