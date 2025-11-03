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
#include <idx/idx_base.h>
#include <idx/foundation.h>
#include <memory>


namespace idx_annotation
{

class EDMD_HISTORY : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_HISTORY() :
            idx_foundation::EDMD_COLLABORATION_OBJECT() {};

    EDMD_HISTORY( wxXmlNode& aNode ) :
            idx_foundation::EDMD_COLLABORATION_OBJECT( aNode, false )
    {
        wxXmlNode* child = aNode.GetChildren();

        while( child )
        {
            const wxString content = child->GetNodeContent();
            const wxString name = child->GetName();

            if( name == "annotation:HistoryName" )
            {
                m_historyName.m_objectName = content;
            }
            else
            {
                unknownKeywordIDX( name );
            }
            child = child->GetNext();
        }
    }

    idx_foundation::EDMD_NAME m_historyName;
};

} // namespace idx_annotation
#endif