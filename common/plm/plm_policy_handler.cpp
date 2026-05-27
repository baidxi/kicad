/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
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

#include "plm_policy_handler.h"

#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/textfile.h>
#include <nlohmann/json.hpp>


PLM_POLICY_HANDLER::PLM_POLICY_HANDLER() :
        m_policyLoaded( false )
{
}


PLM_POLICY_HANDLER::~PLM_POLICY_HANDLER()
{
}


wxString PLM_POLICY_HANDLER::GetPolicyFileName()
{
    return wxS( "kicad_plm_policy.json" );
}


std::vector<wxString> PLM_POLICY_HANDLER::GetPolicySearchPaths() const
{
    std::vector<wxString> paths;

    wxStandardPaths& stdPaths = wxStandardPaths::Get();

#ifdef __WXMSW__
    paths.push_back( stdPaths.GetDataDir() );

    wxString programData;

    if( wxGetEnv( wxS( "ProgramData" ), &programData ) )
        paths.push_back( programData + wxS( "\\KiCad" ) );
#else
    paths.push_back( wxS( "/etc/kicad" ) );

    paths.push_back( stdPaths.GetDataDir() );

    wxString xdgConfigDirs;

    if( wxGetEnv( wxS( "XDG_CONFIG_DIRS" ), &xdgConfigDirs ) )
    {
        wxArrayString dirs = wxSplit( xdgConfigDirs, ':' );

        for( const auto& dir : dirs )
            paths.push_back( dir + wxS( "/kicad" ) );
    }
#endif

    paths.push_back( stdPaths.GetUserDataDir() );

    return paths;
}


wxString PLM_POLICY_HANDLER::findPolicyFile() const
{
    wxString filename = GetPolicyFileName();
    std::vector<wxString> searchPaths = GetPolicySearchPaths();

    for( const auto& path : searchPaths )
    {
        wxFileName fn( path, filename );

        if( fn.FileExists() )
            return fn.GetFullPath();
    }

    return wxEmptyString;
}


PLM_POLICY_LOAD_RESULT PLM_POLICY_HANDLER::LoadPolicy()
{
    wxString policyPath = findPolicyFile();

    if( policyPath.IsEmpty() )
        return PLM_POLICY_LOAD_RESULT::NoPolicy();

    return LoadPolicyFromFile( policyPath );
}


PLM_POLICY_LOAD_RESULT PLM_POLICY_HANDLER::LoadPolicyFromFile( const wxString& aPath )
{
    if( !wxFileExists( aPath ) )
        return PLM_POLICY_LOAD_RESULT::Failure( wxString::Format( "Policy file not found: %s", aPath ) );

    wxTextFile file;

    if( !file.Open( aPath ) )
        return PLM_POLICY_LOAD_RESULT::Failure( wxString::Format( "Cannot open policy file: %s", aPath ) );

    wxString jsonData;

    for( size_t i = 0; i < file.GetLineCount(); i++ )
        jsonData += file.GetLine( i ) + wxS( "\n" );

    file.Close();

    auto result = parsePolicy( jsonData );

    if( result.success )
    {
        result.source_path = aPath;
        m_policy = result.policy;
        m_policyLoaded = true;
        m_policyPath = aPath;
    }

    return result;
}


PLM_POLICY_LOAD_RESULT PLM_POLICY_HANDLER::parsePolicy( const wxString& aJsonData )
{
    try
    {
        nlohmann::json j = nlohmann::json::parse( aJsonData.utf8_string() );

        PLM_POLICY policy;

        if( j.contains( "organization" ) )
            policy.organization_name = wxString::FromUTF8( j["organization"].get<std::string>() );

        if( j.contains( "mandatory_server" ) )
            policy.mandatory_server = j["mandatory_server"].get<bool>();

        if( j.contains( "allow_local_projects" ) )
            policy.allow_local_projects = j["allow_local_projects"].get<bool>();

        if( j.contains( "default_server_url" ) )
            policy.default_server_url = wxString::FromUTF8( j["default_server_url"].get<std::string>() );

        if( j.contains( "require_eco" ) )
            policy.require_eco = j["require_eco"].get<bool>();

        if( j.contains( "require_lock_before_edit" ) )
            policy.require_lock_before_edit = j["require_lock_before_edit"].get<bool>();

        if( j.contains( "require_comment_on_checkin" ) )
            policy.require_comment_on_checkin = j["require_comment_on_checkin"].get<bool>();

        if( j.contains( "allow_force_unlock" ) )
            policy.allow_force_unlock = j["allow_force_unlock"].get<bool>();

        if( j.contains( "require_approval_for_release" ) )
            policy.require_approval_for_release = j["require_approval_for_release"].get<bool>();

        if( j.contains( "session_timeout_minutes" ) )
            policy.session_timeout_minutes = j["session_timeout_minutes"].get<int>();

        if( j.contains( "lock_timeout_hours" ) )
            policy.lock_timeout_hours = j["lock_timeout_hours"].get<int>();

        if( j.contains( "locked_settings" ) && j["locked_settings"].is_array() )
        {
            for( const auto& setting : j["locked_settings"] )
                policy.locked_settings.insert( wxString::FromUTF8( setting.get<std::string>() ) );
        }

        if( j.contains( "mandatory_values" ) && j["mandatory_values"].is_object() )
        {
            for( auto& [key, value] : j["mandatory_values"].items() )
            {
                m_mandatoryValues[wxString::FromUTF8( key )] =
                        wxString::FromUTF8( value.get<std::string>() );
            }
        }

        return PLM_POLICY_LOAD_RESULT::Success( wxEmptyString, policy );
    }
    catch( const nlohmann::json::exception& e )
    {
        return PLM_POLICY_LOAD_RESULT::Failure( wxString::Format( "JSON parse error: %s", e.what() ) );
    }
}


void PLM_POLICY_HANDLER::ApplyPolicy()
{
    if( !m_policyLoaded || !m_lockCallback )
        return;

    for( const auto& setting : m_policy.locked_settings )
        m_lockCallback( setting, true );
}


bool PLM_POLICY_HANDLER::IsSettingLocked( const wxString& aSettingId ) const
{
    if( !m_policyLoaded )
        return false;

    return m_policy.IsSettingLocked( aSettingId );
}


std::set<wxString> PLM_POLICY_HANDLER::GetLockedSettings() const
{
    if( !m_policyLoaded )
        return std::set<wxString>();

    return m_policy.locked_settings;
}


PLM_POLICY_VALIDATION PLM_POLICY_HANDLER::ValidatePolicy() const
{
    PLM_POLICY_VALIDATION result;

    if( !m_policyLoaded )
    {
        result.AddWarning( "No policy file loaded" );
        return result;
    }

    if( m_policy.mandatory_server && m_policy.default_server_url.IsEmpty() )
        result.AddError( "Policy requires mandatory server but no default URL is specified" );

    if( m_policy.mandatory_server && m_policy.allow_local_projects )
        result.AddWarning( "Policy allows local projects but also requires mandatory server" );

    if( m_policy.session_timeout_minutes < 5 )
        result.AddWarning( "Session timeout is very short (< 5 minutes)" );

    if( m_policy.lock_timeout_hours < 1 )
        result.AddWarning( "Lock timeout is very short (< 1 hour)" );

    if( m_policy.lock_timeout_hours > 168 )
        result.AddWarning( "Lock timeout is very long (> 1 week)" );

    return result;
}


void PLM_POLICY_HANDLER::SetSettingLockCallback( PLM_SETTING_LOCK_CALLBACK aCallback )
{
    m_lockCallback = aCallback;
}


bool PLM_POLICY_HANDLER::IsSettingValueAllowed( const wxString& aSettingId,
                                                 const wxString& aValue ) const
{
    if( !m_policyLoaded )
        return true;

    auto it = m_mandatoryValues.find( aSettingId );

    if( it != m_mandatoryValues.end() )
        return it->second == aValue;

    return true;
}


wxString PLM_POLICY_HANDLER::GetMandatoryValue( const wxString& aSettingId ) const
{
    auto it = m_mandatoryValues.find( aSettingId );

    if( it != m_mandatoryValues.end() )
        return it->second;

    return wxEmptyString;
}
