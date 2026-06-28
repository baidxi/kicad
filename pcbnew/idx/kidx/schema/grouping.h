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

#ifndef IDX_GROUPING_H
#define IDX_GROUPING_H

#include <idx/kidx/schema/foundation.h>
#include <idx/kidx/schema/idx_base.h>
#include <memory>
#include <string>
#include <vector>

namespace idx_grouping
{

// Enum for item constraint types
enum class EDMD_ITEM_CONSTRAINT_TYPE
{
    RELATING = 0,    // Parts can only be moved together
    PLACEMENT,       // Placement of parts is fixed
    ROTATION         // Parts can only be rotated together
};

/**
 * @brief Abstract super type for grouping elements
 */
class EDMD_GROUP : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_GROUP() : idx_foundation::EDMD_COLLABORATION_OBJECT() {};
    EDMD_GROUP( IDX_XML_NODE& aNode, bool aTopLevel );

    bool Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    std::vector<std::string> m_related;  // IDREF to EDMDCollaborationObjects
    std::string m_owner;  // IDREF to EDMDPerson, EDMDSystem, or EDMDOrganisation
};

/**
 * @brief An EDMDItemConstraint is used to constrain the relation between two or more items
 */
class EDMD_ITEM_CONSTRAINT : public EDMD_GROUP
{
public:
    EDMD_ITEM_CONSTRAINT() : EDMD_GROUP() {};
    EDMD_ITEM_CONSTRAINT( IDX_XML_NODE& aNode, bool aTopLevel );

    bool Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    EDMD_ITEM_CONSTRAINT_TYPE m_itemConstraintType = EDMD_ITEM_CONSTRAINT_TYPE::RELATING;
};

/**
 * @brief Element to classify CollaborationObjects
 */
class EDMD_GENERAL_CLASSIFICATION : public EDMD_GROUP
{
public:
    EDMD_GENERAL_CLASSIFICATION() : EDMD_GROUP() {};
    EDMD_GENERAL_CLASSIFICATION( IDX_XML_NODE& aNode, bool aTopLevel );

    bool Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    std::vector<idx_foundation::EDMD_IDENTIFIER> m_identifier;
};

} // namespace idx_grouping

#endif // IDX_GROUPING_H
