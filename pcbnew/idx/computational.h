/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2024 KiCad Developers
 *
 * License: GNU GPL v2 or later
 */

#ifndef IDX_COMPUTATIONAL_HEADER_H
#define IDX_COMPUTATIONAL_HEADER_H

#include <wx/xml/xml.h>
#include <idx/foundation.h>
#include <idx/property.h>
#include <idx/pdm.h>
#include <memory>
#include <vector>
#include <string>
#include <iostream>

namespace idx_computational {

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
    EDMD_ABSTRACT_CHANGE(IDX_XML_NODE& aNode, bool aTopLevel);

    bool Parse(IDX_XML_NODE& aNode);

private:
    std::string m_actorRef;
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
    EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION(IDX_XML_NODE& aNode, bool aTopLevel);
};


// -----------------------------
// EDMD_TRANSACTION
// -----------------------------
class EDMD_TRANSACTION : public EDMD_ABSTRACT_CHANGE
{
public:
    EDMD_TRANSACTION();
    EDMD_TRANSACTION(IDX_XML_NODE& aNode, bool aTopLevel);
    std::vector<std::shared_ptr<EDMD_ABSTRACT_CHANGE>> m_changes;

    bool Parse(IDX_XML_NODE& aNode);
};


// -----------------------------
// EDMD_CHANGE
// -----------------------------
class EDMD_CHANGE : public EDMD_ABSTRACT_CHANGE
{
public:
    EDMD_CHANGE();
    EDMD_CHANGE(IDX_XML_NODE& aNode, bool aTopLevel);

    bool Parse(IDX_XML_NODE& aNode);


    idx_foundation::EDMD_DESIGNATOR m_newItem;
    idx_foundation::EDMD_DESIGNATOR m_predecessorItem;
    std::vector<idx_foundation::EDMD_NAME> m_deletedInstanceName;

};




// -----------------------------
// EDMD_PROCESS_INSTRUCTION_SEND_CHANGES
// -----------------------------
class EDMD_PROCESS_INSTRUCTION_SEND_CHANGES : public idx_foundation::EDMD_PROCESS_INSTRUCTION
{
public:
    EDMD_PROCESS_INSTRUCTION_SEND_CHANGES();
    EDMD_PROCESS_INSTRUCTION_SEND_CHANGES(IDX_XML_NODE& aNode, bool aTopLevel);

    bool Parse(IDX_XML_NODE& aNode);

private:
    std::shared_ptr<idx_administration::EDMD_PERSON> m_actor;
    std::string m_actorRef;
    EDMD_TRANSACTION m_transaction;
    
};


// -----------------------------
// EDMD_PROCESS_INSTRUCTION_SEND_INFO
// -----------------------------
class EDMD_PROCESS_INSTRUCTION_SEND_INFO : public idx_foundation::EDMD_PROCESS_INSTRUCTION
{
public:
    EDMD_PROCESS_INSTRUCTION_SEND_INFO();
    EDMD_PROCESS_INSTRUCTION_SEND_INFO(IDX_XML_NODE& aNode, bool aTopLevel);
};


} // namespace idx_computational



#endif // IDX_COMPUTATIONAL_HEADER_H
