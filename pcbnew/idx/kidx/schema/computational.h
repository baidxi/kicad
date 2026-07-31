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

#ifndef IDX_COMPUTATIONAL_HEADER_H
#define IDX_COMPUTATIONAL_HEADER_H

#include <wx/xml/xml.h>
#include <idx/kidx/schema/foundation.h>
#include <idx/kidx/schema/property.h>
#include <idx/kidx/schema/pdm.h>
#include <memory>
#include <vector>
#include <string>
#include <iostream>

namespace idx_computational
{

class EDMD_ABSTRACT_CHANGE;
class EDMD_CHANGE;
class EDMD_TRANSACTION;

// -----------------------------
// EDMD_ABSTRACT_CHANGE
// -----------------------------
class EDMD_ABSTRACT_CHANGE : public idx_foundation::EDMD_EXTENSIBLE_OBJECT
{
public:
    EDMD_ABSTRACT_CHANGE();
    EDMD_ABSTRACT_CHANGE( IDX_XML_NODE& aNode, bool aTopLevel );

    bool Parse( IDX_XML_NODE& aNode ) override;

private:
    std::string                                      m_actorRef;
    std::shared_ptr<idx_administration::EDMD_PERSON> m_actor;
};


// -----------------------------
// EDMD_ERROR_CATEGORY
// -----------------------------
enum EDMD_ERROR_CATEGORY
{
    NOT_AUTHORIZED = 0,
    NOT_AUTHENTICATED,
    INVALID_MESSAGE,
    INVALID_XML_FORMAT,
    INVALID_FORMAT_CONSTRAINTS,
    SYSTEM_ERROR,
    OTHER
};


// Enums for information types
enum class INFORMATION_TYPE
{
    IDENTIFICATION = 0,
    STRUCTURE,
    CLASSIFICATION,
    GEOMETRY,
    EXTERNAL,
    OWNERSHIP,
    ALL
};


// -----------------------------
// EDMD_SERVICE_RESULT
// -----------------------------
class EDMD_SERVICE_RESULT : public idx_foundation::EDMD_EXTENSIBLE_OBJECT
{
public:
    EDMD_SERVICE_RESULT();
};


// -----------------------------
// EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION
// -----------------------------
class EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION : public idx_foundation::EDMD_PROCESS_INSTRUCTION
{
public:
    EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION();
    EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION( IDX_XML_NODE& aNode, bool aTopLevel );
};


// -----------------------------
// EDMD_TRANSACTION
// -----------------------------
class EDMD_TRANSACTION : public EDMD_ABSTRACT_CHANGE
{
public:
    EDMD_TRANSACTION();
    EDMD_TRANSACTION( IDX_XML_NODE& aNode, bool aTopLevel );
    std::vector<std::shared_ptr<EDMD_ABSTRACT_CHANGE>> m_changes;

    bool Parse( IDX_XML_NODE& aNode ) override;
};


// -----------------------------
// EDMD_CHANGE
// -----------------------------
class EDMD_CHANGE : public EDMD_ABSTRACT_CHANGE
{
public:
    EDMD_CHANGE();
    EDMD_CHANGE( IDX_XML_NODE& aNode, bool aTopLevel );

    bool Parse( IDX_XML_NODE& aNode ) override;


    std::shared_ptr<idx_foundation::EDMD_IDENTIFIER>        m_newItem;
    std::shared_ptr<idx_foundation::EDMD_IDENTIFIER>        m_predecessorItem;
    std::vector<std::shared_ptr<idx_foundation::EDMD_NAME>> m_deletedInstanceName;
};


// -----------------------------
// EDMD_PROCESS_INSTRUCTION_SEND_CHANGES
// -----------------------------
class EDMD_PROCESS_INSTRUCTION_SEND_CHANGES : public idx_foundation::EDMD_PROCESS_INSTRUCTION
{
public:
    EDMD_PROCESS_INSTRUCTION_SEND_CHANGES();
    EDMD_PROCESS_INSTRUCTION_SEND_CHANGES( IDX_XML_NODE& aNode, bool aTopLevel );

    bool Parse( IDX_XML_NODE& aNode ) override;

    std::shared_ptr<EDMD_TRANSACTION>                m_transaction;
    std::shared_ptr<idx_administration::EDMD_PERSON> m_actor;

private:
    std::string m_actorRef;
};


// -----------------------------
// EDMD_PROCESS_INSTRUCTION_SEND_INFORMATION
// -----------------------------
class EDMD_PROCESS_INSTRUCTION_SEND_INFORMATION : public idx_foundation::EDMD_PROCESS_INSTRUCTION
{
public:
    EDMD_PROCESS_INSTRUCTION_SEND_INFORMATION();
    EDMD_PROCESS_INSTRUCTION_SEND_INFORMATION( IDX_XML_NODE& aNode, bool aTopLevel );
};


} // namespace idx_computational


#endif // IDX_COMPUTATIONAL_HEADER_H
