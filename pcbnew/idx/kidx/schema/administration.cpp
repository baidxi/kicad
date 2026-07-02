/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2024 KiCad Developers, see AUTHORS.txt for contributors.
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
 * along with this program; if not, you can find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may write to the Free Software Foundation, Inc.
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "administration.h"

namespace idx_administration
{

void EDMD_PERSON::Write( IDX_XML_NODE& aNode ) const
{
    this->EDMD_ACTOR::Write( aNode );

    if( !m_surname.empty() )
        aNode.AppendChild( "administration:SurName" )->SetText( m_surname );
    if( !m_givenName.empty() )
        aNode.AppendChild( "administration:GivenName" )->SetText( m_givenName );
    if( !m_initials.empty() )
        aNode.AppendChild( "administration:Initials" )->SetText( m_initials );
    if( !m_emailAddress.empty() )
        aNode.AppendChild( "administration:EMailAddress" )->SetText( m_emailAddress );
    if( !m_phoneNumber.empty() )
        aNode.AppendChild( "administration:PhoneNumber" )->SetText( m_phoneNumber );
    if( !m_faxNumber.empty() )
        aNode.AppendChild( "administration:FaxNumber" )->SetText( m_faxNumber );
}


} // namespace idx_administration