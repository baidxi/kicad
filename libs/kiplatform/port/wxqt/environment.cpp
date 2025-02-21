/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2024 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <kiplatform/environment.h>

#include <wx/filename.h>
#include <wx/utils.h>

#include "qtconv.h"
#include <QString>
#include <QFile>
#include <QStandardPaths>


void KIPLATFORM::ENV::Init()
{
}


bool KIPLATFORM::ENV::MoveToTrash( const wxString& aPath, wxString& aError )
{
    return QFile::moveToTrash( QString( wxString( aPath.fn_str() ) ) );
}


bool KIPLATFORM::ENV::IsNetworkPath( const wxString& aPath )
{
    // placeholder, we "nerf" behavior if its a network path so return false by default
    return false;
}


wxString KIPLATFORM::ENV::GetDocumentsPath()
{
    return wxQtConvertString(
            QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) );
}


wxString KIPLATFORM::ENV::GetUserConfigPath()
{
    return wxQtConvertString(
            QStandardPaths::writableLocation( QStandardPaths::AppConfigLocation ) );
}


wxString KIPLATFORM::ENV::GetUserDataPath()
{
    return wxQtConvertString(
            QStandardPaths::writableLocation( QStandardPaths::GenericDataLocation ) );
}


wxString KIPLATFORM::ENV::GetUserLocalDataPath()
{
    return wxQtConvertString(
            QStandardPaths::writableLocation( QStandardPaths::GenericDataLocation ) );
}


wxString KIPLATFORM::ENV::GetUserCachePath()
{
    return wxQtConvertString(
            QStandardPaths::writableLocation( QStandardPaths::CacheLocation ) );
}


bool KIPLATFORM::ENV::GetSystemProxyConfig( const wxString& aURL, PROXY_CONFIG& aCfg )
{
    return false;
}


bool KIPLATFORM::ENV::VerifyFileSignature( const wxString& aPath )
{
    return true;
}