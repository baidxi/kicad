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

#include <idx/kidx/schema/text.h>
#include <iostream>

namespace idx_text
{

// Helper function to parse enum from string
EDMD_FONT_FAMILY parseFontFamily( const std::string& value )
{
    if( IDX_keyword_compare( value, "Serif" ) )
        return EDMD_FONT_FAMILY::SERIF;
    else if( IDX_keyword_compare( value, "SansSerif" ) )
        return EDMD_FONT_FAMILY::SANS_SERIF;
    else if( IDX_keyword_compare( value, "Cursive" ) )
        return EDMD_FONT_FAMILY::CURSIVE;
    else if( IDX_keyword_compare( value, "Monospace" ) )
        return EDMD_FONT_FAMILY::MONOSPACE;
    return EDMD_FONT_FAMILY::SERIF;
}

EDMD_TEXT_DECORATION parseTextDecoration( const std::string& value )
{
    if( IDX_keyword_compare( value, "None" ) )
        return EDMD_TEXT_DECORATION::NONE;
    else if( IDX_keyword_compare( value, "Underline" ) )
        return EDMD_TEXT_DECORATION::UNDERLINE;
    else if( IDX_keyword_compare( value, "Overline" ) )
        return EDMD_TEXT_DECORATION::OVERLINE;
    else if( IDX_keyword_compare( value, "LineThrough" ) )
        return EDMD_TEXT_DECORATION::LINE_THROUGH;
    return EDMD_TEXT_DECORATION::NONE;
}

EDMD_FONT_STYLE parseFontStyle( const std::string& value )
{
    if( IDX_keyword_compare( value, "Normal" ) )
        return EDMD_FONT_STYLE::NORMAL;
    else if( IDX_keyword_compare( value, "Italic" ) )
        return EDMD_FONT_STYLE::ITALIC;
    else if( IDX_keyword_compare( value, "Oblique" ) )
        return EDMD_FONT_STYLE::OBLIQUE;
    return EDMD_FONT_STYLE::NORMAL;
}

EDMD_FONT_WEIGHT parseFontWeight( const std::string& value )
{
    if( IDX_keyword_compare( value, "Normal" ) )
        return EDMD_FONT_WEIGHT::NORMAL;
    else if( IDX_keyword_compare( value, "Bold" ) )
        return EDMD_FONT_WEIGHT::BOLD;
    else if( IDX_keyword_compare( value, "Bolder" ) )
        return EDMD_FONT_WEIGHT::BOLDER;
    else if( IDX_keyword_compare( value, "Lighter" ) )
        return EDMD_FONT_WEIGHT::LIGHTER;
    return EDMD_FONT_WEIGHT::NORMAL;
}

EDMD_VERTICAL_ALIGNMENT parseVerticalAlignment( const std::string& value )
{
    if( IDX_keyword_compare( value, "top" ) )
        return EDMD_VERTICAL_ALIGNMENT::TOP;
    else if( IDX_keyword_compare( value, "bottom" ) )
        return EDMD_VERTICAL_ALIGNMENT::BOTTOM;
    else if( IDX_keyword_compare( value, "middle" ) )
        return EDMD_VERTICAL_ALIGNMENT::MIDDLE;
    else if( IDX_keyword_compare( value, "stretch" ) )
        return EDMD_VERTICAL_ALIGNMENT::STRETCH;
    return EDMD_VERTICAL_ALIGNMENT::TOP;
}

EDMD_HORIZONTAL_ALIGNMENT parseHorizontalAlignment( const std::string& value )
{
    if( IDX_keyword_compare( value, "left" ) )
        return EDMD_HORIZONTAL_ALIGNMENT::LEFT;
    else if( IDX_keyword_compare( value, "right" ) )
        return EDMD_HORIZONTAL_ALIGNMENT::RIGHT;
    else if( IDX_keyword_compare( value, "center" ) )
        return EDMD_HORIZONTAL_ALIGNMENT::CENTER;
    else if( IDX_keyword_compare( value, "stretch" ) )
        return EDMD_HORIZONTAL_ALIGNMENT::STRETCH;
    return EDMD_HORIZONTAL_ALIGNMENT::LEFT;
}

// Helper function to write enum to XML
void writeFontFamily( IDX_XML_NODE& aNode, const std::string& name, EDMD_FONT_FAMILY value )
{
    const char* str = "Serif";
    switch( value )
    {
        case EDMD_FONT_FAMILY::SERIF:
            str = "Serif";
            break;
        case EDMD_FONT_FAMILY::SANS_SERIF:
            str = "SansSerif";
            break;
        case EDMD_FONT_FAMILY::CURSIVE:
            str = "Cursive";
            break;
        case EDMD_FONT_FAMILY::MONOSPACE:
            str = "Monospace";
            break;
    }
    aNode.AppendChild( name )->SetText( str );
}

void writeTextDecoration( IDX_XML_NODE& aNode, const std::string& name, EDMD_TEXT_DECORATION value )
{
    const char* str = "None";
    switch( value )
    {
        case EDMD_TEXT_DECORATION::NONE:
            str = "None";
            break;
        case EDMD_TEXT_DECORATION::UNDERLINE:
            str = "Underline";
            break;
        case EDMD_TEXT_DECORATION::OVERLINE:
            str = "Overline";
            break;
        case EDMD_TEXT_DECORATION::LINE_THROUGH:
            str = "LineThrough";
            break;
    }
    aNode.AppendChild( name )->SetText( str );
}

void writeFontStyle( IDX_XML_NODE& aNode, const std::string& name, EDMD_FONT_STYLE value )
{
    const char* str = "Normal";
    switch( value )
    {
        case EDMD_FONT_STYLE::NORMAL:
            str = "Normal";
            break;
        case EDMD_FONT_STYLE::ITALIC:
            str = "Italic";
            break;
        case EDMD_FONT_STYLE::OBLIQUE:
            str = "Oblique";
            break;
    }
    aNode.AppendChild( name )->SetText( str );
}

void writeFontWeight( IDX_XML_NODE& aNode, const std::string& name, EDMD_FONT_WEIGHT value )
{
    const char* str = "Normal";
    switch( value )
    {
        case EDMD_FONT_WEIGHT::NORMAL:
            str = "Normal";
            break;
        case EDMD_FONT_WEIGHT::BOLD:
            str = "Bold";
            break;
        case EDMD_FONT_WEIGHT::BOLDER:
            str = "Bolder";
            break;
        case EDMD_FONT_WEIGHT::LIGHTER:
            str = "Lighter";
            break;
    }
    aNode.AppendChild( name )->SetText( str );
}

void writeVerticalAlignment( IDX_XML_NODE& aNode, const std::string& name, EDMD_VERTICAL_ALIGNMENT value )
{
    const char* str = "top";
    switch( value )
    {
        case EDMD_VERTICAL_ALIGNMENT::TOP:
            str = "top";
            break;
        case EDMD_VERTICAL_ALIGNMENT::BOTTOM:
            str = "bottom";
            break;
        case EDMD_VERTICAL_ALIGNMENT::MIDDLE:
            str = "middle";
            break;
        case EDMD_VERTICAL_ALIGNMENT::STRETCH:
            str = "stretch";
            break;
    }
    aNode.AppendChild( name )->SetText( str );
}

void writeHorizontalAlignment( IDX_XML_NODE& aNode, const std::string& name, EDMD_HORIZONTAL_ALIGNMENT value )
{
    const char* str = "left";
    switch( value )
    {
        case EDMD_HORIZONTAL_ALIGNMENT::LEFT:
            str = "left";
            break;
        case EDMD_HORIZONTAL_ALIGNMENT::RIGHT:
            str = "right";
            break;
        case EDMD_HORIZONTAL_ALIGNMENT::CENTER:
            str = "center";
            break;
        case EDMD_HORIZONTAL_ALIGNMENT::STRETCH:
            str = "stretch";
            break;
    }
    aNode.AppendChild( name )->SetText( str );
}

// ------------------- EDMD_SIMPLE_TEXT -------------------

EDMD_SIMPLE_TEXT::EDMD_SIMPLE_TEXT( IDX_XML_NODE& aNode, bool aTopLevel ) :
        idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
{
    if( aTopLevel )
        ParseChildren( aNode );
}

bool EDMD_SIMPLE_TEXT::Parse( IDX_XML_NODE& aNode )
{
    auto child = aNode.GetChildren();

    while( child )
    {
        const std::string content = child->GetNodeContent();
        const std::string name = child->GetName();

        if( IDX_keyword_compare( name, "text:TextValue" ) )
        {
            m_textValue = content;
        }
        else if( IDX_keyword_compare( name, "text:UpperLeft" ) )
        {
            m_upperLeft = content;
        }
        else if( IDX_keyword_compare( name, "text:LowerRight" ) )
        {
            m_lowerRight = content;
        }
        else if( IDX_keyword_compare( name, "text:FontFace" ) )
        {
            m_fontFace = content;
        }
        else if( IDX_keyword_compare( name, "text:FontFamily" ) )
        {
            m_fontFamily = parseFontFamily( content );
        }
        else if( IDX_keyword_compare( name, "text:TextDecoration" ) )
        {
            m_textDecoration = parseTextDecoration( content );
        }
        else if( IDX_keyword_compare( name, "text:FontStyle" ) )
        {
            m_fontStyle = parseFontStyle( content );
        }
        else if( IDX_keyword_compare( name, "text:FontWeight" ) )
        {
            m_fontWeight = parseFontWeight( content );
        }
        else if( IDX_keyword_compare( name, "text:VerticalAlignment" ) )
        {
            m_verticalAlignment = parseVerticalAlignment( content );
        }
        else if( IDX_keyword_compare( name, "text:HorizontalAlignment" ) )
        {
            m_horizontalAlignment = parseHorizontalAlignment( content );
        }
        else
        {
            // Try base class parsing
            if( !this->idx_foundation::EDMD_COLLABORATION_OBJECT::Parse( *child ) )
                unknownKeywordIDX( name );
        }
        child = child->GetNext();
    }
    return true;
}

void EDMD_SIMPLE_TEXT::Write( IDX_XML_NODE& aNode ) const
{
    this->idx_foundation::EDMD_COLLABORATION_OBJECT::Write( aNode );

    aNode.AppendChild( "text:TextValue" )->SetText( m_textValue );
    aNode.AppendChild( "text:UpperLeft" )->SetText( m_upperLeft );
    aNode.AppendChild( "text:LowerRight" )->SetText( m_lowerRight );

    if( !m_fontFace.empty() )
        aNode.AppendChild( "text:FontFace" )->SetText( m_fontFace );

    writeFontFamily( aNode, "text:FontFamily", m_fontFamily );

    // Only write optional fields if they're not default values
    if( m_textDecoration != EDMD_TEXT_DECORATION::NONE )
        writeTextDecoration( aNode, "text:TextDecoration", m_textDecoration );

    if( m_fontStyle != EDMD_FONT_STYLE::NORMAL )
        writeFontStyle( aNode, "text:FontStyle", m_fontStyle );

    if( m_fontWeight != EDMD_FONT_WEIGHT::NORMAL )
        writeFontWeight( aNode, "text:FontWeight", m_fontWeight );

    if( m_verticalAlignment != EDMD_VERTICAL_ALIGNMENT::TOP )
        writeVerticalAlignment( aNode, "text:VerticalAlignment", m_verticalAlignment );

    if( m_horizontalAlignment != EDMD_HORIZONTAL_ALIGNMENT::LEFT )
        writeHorizontalAlignment( aNode, "text:HorizontalAlignment", m_horizontalAlignment );
}

} // namespace idx_text
