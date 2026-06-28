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

#ifndef IDX_EXTERNAL_HEADER_H
#define IDX_EXTERNAL_HEADER_H

#include <idx/kidx/schema/idx_base.h>
#include <idx/kidx/schema/foundation.h>
#include <idx/kidx/schema/pdm.h>
#include <wx/xml/xml.h>
#include <memory>
#include <map>


namespace idx_external
{

class EDMD_EXTERNAL_SOURCE : public idx_foundation::EDMD_COLLABORATION_OBJECT
{
public:
    EDMD_EXTERNAL_SOURCE() : idx_foundation::EDMD_COLLABORATION_OBJECT() {};
    EDMD_EXTERNAL_SOURCE( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Parse( IDX_XML_NODE& aNode ) override;


    std::string m_mimeType;
    std::string m_location;
};


class EDMD_EXTERNAL_GEOMETRIC_MODEL : public idx_pdm::EDMD_SHAPE_DESCRIPTION
{
public:
    EDMD_EXTERNAL_GEOMETRIC_MODEL() : idx_pdm::EDMD_SHAPE_DESCRIPTION() {};
    EDMD_EXTERNAL_GEOMETRIC_MODEL( IDX_XML_NODE& aNode, bool aTopLevel );

    virtual bool Parse( IDX_XML_NODE& aNode ) override;


    std::string m_externalSourceRef;

    std::shared_ptr<EDMD_EXTERNAL_SOURCE> m_externalSource;
};

} // namespace idx_external

#endif // IDX_EXTERNAL_HEADER_H
