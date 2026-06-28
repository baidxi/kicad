/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2024 KiCad Developers, see AUTHORS.txt for contributors.
 * @author Fabien Corona <f.corona at laposte.net>
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
 * along with this program; if not, you may write to the Free Software Foundation, Inc.,
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef IDX_MATERIAL_H
#define IDX_MATERIAL_H

#include <idx/kidx/schema/foundation.h>
#include <idx/kidx/schema/idx_base.h>
#include <memory>
#include <string>
#include <vector>

namespace idx_material
{

/**
 * @brief EDMDMaterial represents a material and its properties
 * 
 * NOTE: Material issues are not yet defined in the collaboration model.
 * This is a placeholder for future material property definitions.
 */
class EDMD_MATERIAL : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_MATERIAL() : idx_foundation::EDMD_COLLABORATION_OBJECT() {};
    EDMD_MATERIAL( IDX_XML_NODE& aNode, bool aTopLevel );

    bool Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    // Material properties can be extended here as the schema evolves
    // Currently, the schema only defines this as an extension of EDMDCollaborationObject
    // with no additional elements, but this provides a foundation for future expansion.

    // Common material properties that could be added:
    std::string m_name;             // Material name
    std::string m_description;      // Material description
    std::string m_type;             // Material type (conductor, dielectric, etc.)
    std::string m_manufacturer;    // Manufacturer information
    std::string m_partNumber;      // Manufacturer part number

    // Electrical properties
    double m_conductivity = 0.0;    // Electrical conductivity
    double m_permittivity = 1.0;    // Relative permittivity
    double m_permeability = 1.0;    // Relative permeability

    // Thermal properties
    double m_thermalConductivity = 0.0;  // Thermal conductivity (W/m·K)
    double m_thermalExpansion = 0.0;      // Coefficient of thermal expansion
    double m_meltingPoint = 0.0;          // Melting point in Celsius

    // Mechanical properties
    double m_density = 0.0;              // Density (kg/m³)
    double m_elasticModulus = 0.0;       // Young's modulus (Pa)
    double m_poissonsRatio = 0.0;        // Poisson's ratio
    double m_hardness = 0.0;             // Material hardness

    // Optical properties
    std::string m_color;                 // Material color
    double m_refractiveIndex = 1.0;      // Refractive index
};

} // namespace idx_material

#endif // IDX_MATERIAL_H
