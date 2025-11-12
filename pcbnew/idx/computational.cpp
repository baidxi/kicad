/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2024 KiCad Developers
 *
 * License: GNU GPL v2 or later
 */

#include "computational.h"

namespace idx_computational {

// -----------------------------
// EDMD_PROCESS_INSTRUCTION_SEND_CHANGES
// -----------------------------
EDMD_PROCESS_INSTRUCTION_SEND_CHANGES::EDMD_PROCESS_INSTRUCTION_SEND_CHANGES()
    : idx_foundation::EDMD_PROCESS_INSTRUCTION()
{
}

EDMD_PROCESS_INSTRUCTION_SEND_CHANGES::EDMD_PROCESS_INSTRUCTION_SEND_CHANGES(IDX_XML_NODE& aNode, bool aTopLevel)
    : idx_foundation::EDMD_PROCESS_INSTRUCTION(aNode, false)
{
    m_processType = idx_foundation::EDMD_PROCESS_INSTRUCTION::PROCESS_TYPE::SEND_CHANGES;
    if (aTopLevel)
        ParseChildren(aNode);
}

bool EDMD_PROCESS_INSTRUCTION_SEND_CHANGES::Parse(IDX_XML_NODE& aNode)
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();

    if (IDX_keyword_compare(name, "computational:Actor"))
    {
        m_actorRef = content;
        return true;
    }
    else if (IDX_keyword_compare(name, "computational:Changes"))
    {
        m_transaction = EDMD_TRANSACTION( aNode, true );
        return true;
    }
    return this->EDMD_PROCESS_INSTRUCTION::Parse(aNode);
}


// -----------------------------
// EDMD_PROCESS_INSTRUCTION_SEND_INFO
// -----------------------------
EDMD_PROCESS_INSTRUCTION_SEND_INFO::EDMD_PROCESS_INSTRUCTION_SEND_INFO()
    : idx_foundation::EDMD_PROCESS_INSTRUCTION()
{
}

EDMD_PROCESS_INSTRUCTION_SEND_INFO::EDMD_PROCESS_INSTRUCTION_SEND_INFO(IDX_XML_NODE& aNode, bool aTopLevel)
    : idx_foundation::EDMD_PROCESS_INSTRUCTION(aNode, false)
{
    m_processType = idx_foundation::EDMD_PROCESS_INSTRUCTION::PROCESS_TYPE::BASELINE;
    if (aTopLevel)
        ParseChildren(aNode);
}


// -----------------------------
// EDMD_ABSTRACT_CHANGE
// -----------------------------
EDMD_ABSTRACT_CHANGE::EDMD_ABSTRACT_CHANGE()
    : idx_foundation::EDMD_EXTENSIBLE_OBJECT()
{
}

EDMD_ABSTRACT_CHANGE::EDMD_ABSTRACT_CHANGE(IDX_XML_NODE& aNode, bool aTopLevel)
    : idx_foundation::EDMD_EXTENSIBLE_OBJECT(aNode, false)
{
    if (aTopLevel)
        ParseChildren(aNode);
}


bool EDMD_ABSTRACT_CHANGE::Parse(IDX_XML_NODE& aNode)
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();
    
    if (IDX_keyword_compare(name, "computational:Actor"))
    {
        m_actorRef = content;
        return true;
    }
    return this->EDMD_EXTENSIBLE_OBJECT::Parse(aNode);
}

// -----------------------------
// EDMD_SERVICE_RESULT
// -----------------------------
EDMD_SERVICE_RESULT::EDMD_SERVICE_RESULT()
    : idx_foundation::EDMD_EXTENSIBLE_OBJECT()
{
}


// -----------------------------
// EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION
// -----------------------------
EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION::EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION()
    : idx_foundation::EDMD_PROCESS_INSTRUCTION()
{
}

EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION::EDMD_PROCESS_INSTRUCTION_REQUEST_FOR_INFORMATION(IDX_XML_NODE& aNode, bool aTopLevel)
    : idx_foundation::EDMD_PROCESS_INSTRUCTION(aNode, false)
{
    if (aTopLevel)
        ParseChildren(aNode);
}


// -----------------------------
// EDMD_TRANSACTION
// -----------------------------
EDMD_TRANSACTION::EDMD_TRANSACTION()
    : EDMD_ABSTRACT_CHANGE()
{
}

EDMD_TRANSACTION::EDMD_TRANSACTION(IDX_XML_NODE& aNode, bool aTopLevel)
    : EDMD_ABSTRACT_CHANGE(aNode, false)
{
    if (aTopLevel)
        ParseChildren(aNode);
}


bool EDMD_TRANSACTION::Parse(IDX_XML_NODE& aNode)
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();
    
    if (IDX_keyword_compare(name, "computational:Change"))
    {
        std::string type = aNode.GetAttribute("xsi:type", "");
        if (IDX_keyword_compare(type, "computational:EDMDChange"))
            m_changes.push_back(std::make_shared<EDMD_CHANGE>(aNode, true));
        else
            invalidValueIDX(name, type);
        return true;
    }
    return this->EDMD_ABSTRACT_CHANGE::Parse(aNode);
}


// -----------------------------
// EDMD_CHANGE
// -----------------------------
EDMD_CHANGE::EDMD_CHANGE()
    : EDMD_ABSTRACT_CHANGE()
{
}

EDMD_CHANGE::EDMD_CHANGE(IDX_XML_NODE& aNode, bool aTopLevel)
    : EDMD_ABSTRACT_CHANGE(aNode, false)
{
    if (aTopLevel)
        ParseChildren(aNode);
}


bool EDMD_CHANGE::Parse(IDX_XML_NODE& aNode)
{
    const std::string name = aNode.GetName();
    const std::string content = aNode.GetNodeContent();
    
    if (IDX_keyword_compare(name, "computational:NewItem"))
    {
        m_newItem = idx_foundation::EDMD_IDENTIFIER( aNode, true );
        return true;
    }
    else if (IDX_keyword_compare(name, "computational:PredecessorItem"))
    {
        m_predecessorItem = idx_foundation::EDMD_IDENTIFIER( aNode, true );
        return true;
    }
    else if (IDX_keyword_compare(name, "computational:DeletedInstanceName"))
    {
        m_deletedInstanceName.emplace_back( aNode, true );
        return true;
    }
    return this->EDMD_ABSTRACT_CHANGE::Parse(aNode);
}


} // namespace idx_computational
