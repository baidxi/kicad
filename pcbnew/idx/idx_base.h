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
#include <wx/string.h>
#include <wx/datetime.h>
#include <wx/xml/xml.h>
#include <iostream>
#include <memory>
#include <typeindex>

class IDX_BASE;

// Registry to manage different types of objects
class IDX_REGISTRY
{
public:
    // Register a new object of any type
    template <typename T>
    void registerObject( const wxString& name, std::shared_ptr<T> object )
    {
        auto& registry = getRegistry<T>();
        registry[name] = object;
    }

    // Resolve an object of any type by name
    template <typename T>
    std::shared_ptr<IDX_BASE> resolve( const wxString& name )
    {
        const auto& registry = getRegistry<T>();
        auto        it = registry.find( name );
        if( it != registry.end() )
        {
            return std::dynamic_pointer_cast<T>( it->second );
        }
        else
        {
            std::cerr << "Warning: Object " << name << " not found in registry. ( " << typeid( T ).name() <<  " )" << std::endl;
            return nullptr;
        }
    }

private:
    // Helper function to get the registry for a specific type
    template <typename T>
    std::unordered_map<wxString, std::shared_ptr<IDX_BASE>>& getRegistry()
    {
        return registries[typeid( T )];
    }

    /*
const std::unordered_map<wxString, std::shared_ptr<IDX_BASE>>& getRegistry() const {
    // Try to find the registry for the type T
    auto registryIt = registries.find(typeid(T));
    if (registryIt != registries.end()) {
        // If found, return a reference to the inner map
        return registryIt->second;
    }
    */
    /*
    template <typename T>
    const std::unordered_map<wxString, std::shared_ptr<EDMD_OBJECT>>& getRegistry() const {
        auto it = registries.find(typeid(T));
        if (it != registries.end()) {
            return it->second;
        } else {
            static const std::unordered_map<wxString, std::shared_ptr<idx::EDMD_OBJECT>> emptyRegistry;
            return emptyRegistry;
        }
    }
    */

    std::unordered_map<std::type_index, std::unordered_map<wxString, std::shared_ptr<IDX_BASE>>>
            registries;
};

class IDX_BASE
{
public:
    IDX_BASE() {};
    virtual ~IDX_BASE() = default; // Virtual destructor makes the class polymorphic
    bool Resolve( IDX_REGISTRY& aRegistry ) { return true; };

protected:
    template <typename T, typename U>
    bool resolve( wxString aName, std::shared_ptr<U>* aDest, IDX_REGISTRY& aRegistry )
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
    bool resolve( std::vector<wxString>& aName, std::vector<std::shared_ptr<U>>& aDest,
                  IDX_REGISTRY& aRegistry )
    {
        bool result = true;

        for( wxString& name : aName )
        {
            std::shared_ptr<U> tmp;

            if( resolve<U>( name, &tmp, aRegistry ) )
                aDest.push_back( tmp );
            else
                result = false;
        }
        return result;
    }
};

#endif