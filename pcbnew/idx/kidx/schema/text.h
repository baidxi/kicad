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

#ifndef IDX_TEXT_H
#define IDX_TEXT_H

#include <idx/kidx/schema/foundation.h>
#include <idx/kidx/schema/idx_base.h>
#include <memory>
#include <string>
#include <vector>

namespace idx_text
{

// Enum types for text properties
enum class EDMD_FONT_FAMILY
{
    SERIF = 0,
    SANS_SERIF,
    CURSIVE,
    MONOSPACE
};

enum class EDMD_TEXT_DECORATION
{
    NONE = 0,
    UNDERLINE,
    OVERLINE,
    LINE_THROUGH
};

enum class EDMD_FONT_STYLE
{
    NORMAL = 0,
    ITALIC,
    OBLIQUE
};

enum class EDMD_FONT_WEIGHT
{
    NORMAL = 0,
    BOLD,
    BOLDER,
    LIGHTER
};

enum class EDMD_VERTICAL_ALIGNMENT
{
    TOP = 0,
    BOTTOM,
    MIDDLE,
    STRETCH
};

enum class EDMD_HORIZONTAL_ALIGNMENT
{
    LEFT = 0,
    RIGHT,
    CENTER,
    STRETCH
};

/**
 * @brief Simple text element that describes the geometry of a text
 */
class EDMD_SIMPLE_TEXT : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_SIMPLE_TEXT() : idx_foundation::EDMD_COLLABORATION_OBJECT() {};
    EDMD_SIMPLE_TEXT( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Resolve( IDX_REGISTRY& aRegistry ) override;
    bool Parse( IDX_XML_NODE& aNode ) override;
    virtual void Write( IDX_XML_NODE& aNode ) const override;

    // Text content
    std::string m_textValue;

    // Geometry references (IDREF to EDMDPoint)
    std::string m_upperLeft;
    std::string m_lowerRight;

    // Font properties
    std::string m_fontFace;  // Optional name of the font face
    EDMD_FONT_FAMILY m_fontFamily = EDMD_FONT_FAMILY::SERIF;
    EDMD_TEXT_DECORATION m_textDecoration = EDMD_TEXT_DECORATION::NONE;
    EDMD_FONT_STYLE m_fontStyle = EDMD_FONT_STYLE::NORMAL;
    EDMD_FONT_WEIGHT m_fontWeight = EDMD_FONT_WEIGHT::NORMAL;

    // Alignment
    EDMD_VERTICAL_ALIGNMENT m_verticalAlignment = EDMD_VERTICAL_ALIGNMENT::TOP;
    EDMD_HORIZONTAL_ALIGNMENT m_horizontalAlignment = EDMD_HORIZONTAL_ALIGNMENT::LEFT;

    // Resolved pointers to actual point objects
    std::shared_ptr<idx_geometry::EDMD_POINT> m_upperLeftPoint;
    std::shared_ptr<idx_geometry::EDMD_POINT> m_lowerRightPoint;
};

} // namespace idx_text

#endif // IDX_TEXT_H
