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

#ifndef IDX_ANNOTATION_H
#define IDX_ANNOTATION_H
#include <idx/kidx/schema/foundation.h>
#include <idx/kidx/schema/idx_base.h>
#include <memory>
#include <vector>
#include <string>

namespace idx_annotation
{

// Forward declarations
class EDMD_ANNOTATION;
class EDMD_HISTORY_ENTRY;
class EDMD_HISTORY_ENTRY_CHANGE_ITEM_INSTANCE;
class EDMD_HISTORY_ENTRY_ADD_COMMENT;
class EDMD_HISTORY_ENTRY_CHANGE_OWNERSHIP;
class EDMD_HISTORY_ENTRY_ADD_ITEM_INSTANCE;
class EDMD_HISTORY_ENTRY_CHANGE_ACCEPTANCE;
class EDMD_HISTORY;
class ARRAY_OF_EDMD_NAME;

/**
 * @brief Array of EDMD_NAME elements
 */
class ARRAY_OF_EDMD_NAME : public IDX_BASE
{
public:
    ARRAY_OF_EDMD_NAME() :
            IDX_BASE() {};
    ARRAY_OF_EDMD_NAME( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool Parse( IDX_XML_NODE& aNode ) override;

    std::vector<std::shared_ptr<idx_foundation::EDMD_NAME>> m_EDMDName;
};

/**
 * @brief Annotation to an object
 */
class EDMD_ANNOTATION : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_ANNOTATION() :
            idx_foundation::EDMD_COLLABORATION_OBJECT() {};
    EDMD_ANNOTATION( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    std::string              m_subject;           // IDREF to EDMDObject
    std::string              m_creator;           // IDREF to EDMDPerson
    std::vector<std::string> m_relatedAnnotation; // IDREF to EDMDAnnotation
    std::string              m_caption;
    std::string              m_text;
    std::vector<std::string> m_reference; // anyURI
};

/**
 * @brief Abstract super type for history entry
 */
class EDMD_HISTORY_ENTRY : public EDMD_ANNOTATION
{
public:
    EDMD_HISTORY_ENTRY() :
            EDMD_ANNOTATION() {};
    EDMD_HISTORY_ENTRY( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    std::string m_history;          // IDREF to EDMDHistory
    std::string m_creationDateTime; // xs:dateTime
    std::string m_systemScope;      // IDREF to EDMDSystem
};

/**
 * @brief History entry indicating a change on an item instance
 */
class EDMD_HISTORY_ENTRY_CHANGE_ITEM_INSTANCE : public EDMD_HISTORY_ENTRY
{
public:
    EDMD_HISTORY_ENTRY_CHANGE_ITEM_INSTANCE() :
            EDMD_HISTORY_ENTRY() {};
    EDMD_HISTORY_ENTRY_CHANGE_ITEM_INSTANCE( IDX_XML_NODE& aNode, bool aTopLevel );
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;
};

/**
 * @brief History entry adding only a comment to the history
 */
class EDMD_HISTORY_ENTRY_ADD_COMMENT : public EDMD_HISTORY_ENTRY
{
public:
    EDMD_HISTORY_ENTRY_ADD_COMMENT() :
            EDMD_HISTORY_ENTRY() {};
    EDMD_HISTORY_ENTRY_ADD_COMMENT( IDX_XML_NODE& aNode, bool aTopLevel );
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;
};

/**
 * @brief History entry indicating the change of ownership
 */
class EDMD_HISTORY_ENTRY_CHANGE_OWNERSHIP : public EDMD_HISTORY_ENTRY
{
public:
    EDMD_HISTORY_ENTRY_CHANGE_OWNERSHIP() :
            EDMD_HISTORY_ENTRY() {};
    EDMD_HISTORY_ENTRY_CHANGE_OWNERSHIP( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    std::string m_oldOwner; // IDREF, nillable
};

/**
 * @brief History entry indicating addition of an item instance
 */
class EDMD_HISTORY_ENTRY_ADD_ITEM_INSTANCE : public EDMD_HISTORY_ENTRY
{
public:
    EDMD_HISTORY_ENTRY_ADD_ITEM_INSTANCE() :
            EDMD_HISTORY_ENTRY() {};
    EDMD_HISTORY_ENTRY_ADD_ITEM_INSTANCE( IDX_XML_NODE& aNode, bool aTopLevel );
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;
};

/**
 * @brief History entry indicating acceptance of a change
 */
class EDMD_HISTORY_ENTRY_CHANGE_ACCEPTANCE : public EDMD_HISTORY_ENTRY
{
public:
    EDMD_HISTORY_ENTRY_CHANGE_ACCEPTANCE() :
            EDMD_HISTORY_ENTRY() {};
    EDMD_HISTORY_ENTRY_CHANGE_ACCEPTANCE( IDX_XML_NODE& aNode, bool aTopLevel );
    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;
};

/**
 * @brief Represents a human readable history
 */
class EDMD_HISTORY : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_HISTORY();
    EDMD_HISTORY( IDX_XML_NODE& aNode );
    EDMD_HISTORY( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool         Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    ARRAY_OF_EDMD_NAME m_historyName;
    std::string        m_subject; // IDREF to EDMDObject
};

} // namespace idx_annotation
#endif