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

#include "plm_odoo_plugin.h"
#include "odoo_error_mapper.h"

#include <plm/plm_dirty_tracker.h>
#include <plm/plm_file.h>
#include <plm/plm_eco.h>
#include <plm/plm_notifications.h>
#include <plm/plm_permissions.h>
#include <plm/plm_schema.h>
#include <ctime>
#include <map>
#include <set>
#include <wx/base64.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/log.h>
#include <wx/filename.h>
#include <wx/timer.h>
#include <wx/uri.h>
#include <wx/utils.h>
#include <wx/wfstream.h>


static const wxString ODOO_PLM_METADATA_FILE = wxS( ".kicad_plm_odoo" );


PLM_ODOO_PLUGIN::PLM_ODOO_PLUGIN() :
        m_rpcClient( std::make_unique<OdooRpcClient>() ),
        m_userId( 0 ),
        m_isAuthenticated( false )
{
}


PLM_ODOO_PLUGIN::~PLM_ODOO_PLUGIN()
{
    if( m_isAuthenticated )
        Logout();
}


PLM_PLUGIN_INFO PLM_ODOO_PLUGIN::GetPluginInfo() const
{
    PLM_PLUGIN_INFO info;

    info.plugin_id = wxS( "odoo-plm" );
    info.display_name = wxS( "OdooPLM" );
    info.description = wxS( "Integration with OdooPLM for product lifecycle management" );
    info.version = PLM_VERSION( 1, 0, 0 );
    info.vendor = wxS( "KiCad" );
    info.vendor_url = wxS( "https://www.odoo.com" );
    info.icon_name = wxS( "odoo" );

    return info;
}


PLM_CAPABILITIES PLM_ODOO_PLUGIN::GetCapabilities() const
{
    // Authenticate() / Logout() write m_auditTrailProbe and m_ecoTypeProbe
    // under m_mutex. Reading them here without the lock is a data race; take
    // the recursive_mutex to make the visibility explicit.
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_CAPABILITIES caps;

    // Core file operations
    caps.supports_checkout = true;
    caps.supports_locking = true;
    caps.supports_attributes = true;
    caps.supports_search = true;

    // OdooPLM (engineering_* fields on product.product) is required for real
    // revision history. m_odooPlmProbe in Authenticate() reports whether the
    // module is installed; before auth we don't know, so default to false.
    caps.supports_revisions = m_odooPlmProbe.value_or( false );

    caps.supports_api_keys = true;

    caps.supports_part_database = true;
    caps.supports_part_qualification = true;
    caps.supports_bom_validation = true;
    caps.supports_bom_management = true;
    caps.supports_routing = true;
    caps.supports_state_transitions = true;
    caps.supports_review = true;

    // ECO support requires the mrp.eco / mrp.eco.type model pair from Odoo
    // Enterprise (mrp_plm). Community OdooPLM (OmniaSolutions) does not ship
    // these models. m_ecoTypeProbe records whether mrp.eco.type was readable
    // during Authenticate(); before the first auth we don't yet know, so
    // default to false so callers don't dispatch RPCs that would 404 with a
    // KeyError. Once the probe runs the flags reflect the deployment.
    bool ecoAvailable = m_ecoTypeProbe.value_or( false );

    caps.supports_eco = ecoAvailable;
    caps.supports_eco_create = ecoAvailable;
    caps.supports_eco_transitions = ecoAvailable;
    caps.supports_eco_items = ecoAvailable;
    caps.supports_eco_impact = ecoAvailable;
    caps.supports_ecr_to_eco = ecoAvailable;
    caps.supports_where_used = true;
    caps.supports_variants = true;
    caps.supports_folder_creation = true;
    caps.supports_project_creation = true;
    caps.supports_cloning = true;

    // SubscribeNotifications is implemented via polling (POLL_INTERVAL_MS);
    // the flag name is somewhat misleading but the plugin does deliver
    // change events to listeners.
    caps.supports_push_notifications = true;

    // GetAuditLog reads mail.message; the probe in Authenticate() tries a
    // 1-row search_read and caches whether the active session has access.
    // Before authentication we cannot know, so report false.
    caps.supports_audit_trail = m_auditTrailProbe.value_or( false );

    return caps;
}


bool PLM_ODOO_PLUGIN::OnLoad()
{
    InitManagers();
    return true;
}


void PLM_ODOO_PLUGIN::OnUnload()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( m_isAuthenticated )
        Logout();

    TeardownManagers();
}


PLM_AUTH_RESULT PLM_ODOO_PLUGIN::Authenticate( const PLM_CREDENTIALS& aCredentials )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_AUTH_RESULT result;

    if( m_isAuthenticated )
        Logout();

    ParseServerUrl( aCredentials.server_url );

    // Allow explicit database via the properties map, overriding URL-derived value
    wxString dbProperty = aCredentials.GetProperty( wxS( "database" ) );

    if( !dbProperty.IsEmpty() )
        m_database = dbProperty;

    if( m_serverUrl.IsEmpty() )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_INVALID_PARAMETER;
        result.error_message = wxS( "Server URL is required" );
        return result;
    }

    if( m_database.IsEmpty() )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_INVALID_PARAMETER;
        result.error_message = wxS( "Database name is required. Either include it in the URL "
                                    "(e.g., https://mycompany.odoo.com/mydb) or set the "
                                    "'database' property in credentials." );
        return result;
    }

    m_rpcClient->SetServer( m_serverUrl, m_database );

    ODOO_RPC_RESULT rpcResult = m_rpcClient->Authenticate( aCredentials.username,
                                                            aCredentials.password.Get() );

    if( rpcResult.success )
    {
        const ODOO_SESSION& odooSession = m_rpcClient->GetSession();

        m_sessionId = odooSession.session_id;
        m_userId = odooSession.uid;
        m_username = aCredentials.username;
        m_isAuthenticated = true;

        m_session.token = PLM_SECURE_STRING( m_sessionId );
        m_session.server_url = m_serverUrl;
        m_session.user_info.user_id = wxString::Format( wxS( "%d" ), m_userId );
        m_session.user_info.display_name = odooSession.partner_display_name;
        m_session.user_info.email = aCredentials.username;

        m_userDisplayNames[m_session.user_info.user_id] = odooSession.partner_display_name;

        // Query user group memberships from Odoo
        QueryUserGroups();

        // Odoo default session timeout is 7200 seconds (2 hours)
        int64_t now = static_cast<int64_t>( std::time( nullptr ) );
        m_session.expires_at = now + 7200;

        // Probe whether the session can read mail.message; capability flag
        // supports_audit_trail derives from this so callers don't issue
        // GetAuditLog calls that would always fail with permission errors.
        {
            nlohmann::json probeDomain = nlohmann::json::array();
            ODOO_RPC_RESULT probe =
                    m_rpcClient->SearchRead( wxS( "mail.message" ), probeDomain,
                                              nlohmann::json::array( { "id" } ), 1 );
            m_auditTrailProbe = probe.success;
        }

        // Probe whether the deployment exposes mrp.eco.type. Odoo Enterprise
        // ships mrp_plm with this model; community OdooPLM (OmniaSolutions)
        // does not. GetECORequiredFields uses this to decide whether to read
        // ECO field requirements live or fall back to the static list.
        {
            nlohmann::json probeDomain = nlohmann::json::array();
            ODOO_RPC_RESULT probe = m_rpcClient->SearchRead(
                    wxS( "mrp.eco.type" ), probeDomain,
                    nlohmann::json::array( { "id" } ), 1 );
            m_ecoTypeProbe = probe.success;
        }

        // Probe whether OdooPLM (OmniaSolutions community) is installed by
        // asking for the engineering_state field on product.product. The
        // field only exists when the plm module is loaded. Drives
        // supports_revisions and the GetRevisionHistory implementation
        // which walks engineering_branch_parent_id. fields_get is cheaper
        // than search_read and tolerates ACL restrictions on rows.
        {
            nlohmann::json fieldsArg = nlohmann::json::array(
                    { "engineering_state", "engineering_revision",
                      "engineering_branch_parent_id" } );
            nlohmann::json args = nlohmann::json::array();
            args.push_back( fieldsArg );

            ODOO_RPC_RESULT probe = m_rpcClient->CallMethod(
                    wxS( "product.product" ), wxS( "fields_get" ), args );

            m_odooPlmProbe = probe.success && probe.result.is_object()
                             && probe.result.contains( "engineering_state" )
                             && probe.result.contains( "engineering_revision" )
                             && probe.result.contains( "engineering_branch_parent_id" );
        }

        SetupActivityTracker();
        SetupStateMapper();

        result.success = true;
        result.token = PLM_SECURE_STRING( m_sessionId );
        result.expires_at = m_session.expires_at;
    }
    else
    {
        result.success = false;

        if( rpcResult.error_code == 401 )
            result.error_code = PLM_ERROR_CODE::ERR_AUTH_FAILED;
        else if( rpcResult.error_code != 0 )
            result.error_code = PLM_ERROR_CODE::ERR_NETWORK_UNREACHABLE;
        else
            result.error_code = PLM_ERROR_CODE::ERR_INTERNAL;

        result.error_message = rpcResult.error_message;

        if( !rpcResult.error_data.IsEmpty() )
            result.error_message += wxS( ": " ) + rpcResult.error_data;
    }

    return result;
}


void PLM_ODOO_PLUGIN::QueryUserGroups()
{
    if( !m_rpcClient || m_userId == 0 )
        return;

    // Read the groups_id field from the authenticated user
    nlohmann::json userIds = nlohmann::json::array( { m_userId } );
    nlohmann::json userFields = nlohmann::json::array( { "groups_id" } );

    ODOO_RPC_RESULT userResult = m_rpcClient->Read( wxS( "res.users" ), userIds, userFields );

    if( !userResult.success || !userResult.result.is_array() || userResult.result.empty() )
        return;

    try
    {
        const auto& user = userResult.result[0];

        if( !user.contains( "groups_id" ) || !user["groups_id"].is_array() )
            return;

        const auto& groupIds = user["groups_id"];

        if( groupIds.empty() )
            return;

        // Resolve group IDs to group names
        nlohmann::json domain = nlohmann::json::array();
        nlohmann::json idList = nlohmann::json::array();

        for( const auto& gid : groupIds )
            idList.push_back( gid.get<int>() );

        domain.push_back( nlohmann::json::array( { "id", "in", idList } ) );

        nlohmann::json groupFields = nlohmann::json::array( { "full_name" } );

        ODOO_RPC_RESULT groupResult = m_rpcClient->SearchRead(
                wxS( "res.groups" ), domain, groupFields, 0 );

        if( !groupResult.success || !groupResult.result.is_array() )
            return;

        m_session.user_info.groups.clear();

        for( const auto& group : groupResult.result )
        {
            if( group.contains( "full_name" ) && group["full_name"].is_string() )
            {
                wxString groupName = wxString::FromUTF8( group["full_name"].get<std::string>() );
                m_session.user_info.groups.push_back( groupName );
            }
        }
    }
    catch( const nlohmann::json::exception& )
    {
        // Group query is best-effort; auth still succeeded
    }
}


bool PLM_ODOO_PLUGIN::Logout()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    bool success = true;

    // Drop notification subscriptions before tearing down the session so the
    // poll timer cannot fire RPC calls against an unauthenticated client.
    UnsubscribeAllNotifications();

    if( m_rpcClient && m_isAuthenticated )
        success = m_rpcClient->Logout();

    m_isAuthenticated = false;
    m_sessionId.Clear();
    m_userId = 0;
    m_username.Clear();
    m_session.Clear();
    m_userDisplayNames.clear();
    m_categoryCache.clear();
    m_auditTrailProbe.reset();
    m_ecoTypeProbe.reset();
    m_odooPlmProbe.reset();

    // Keep the tracker alive for any subsequent unauthenticated audit
    // events; clear cached entries from the previous session and unbind
    // the fetch function so it can no longer query GetAuditLog.
    if( m_activityTracker )
    {
        m_activityTracker->ClearActivity();
        m_activityTracker->SetFetchFunction( {} );
    }

    // Same lifecycle for the state mapper.
    if( m_stateMapper )
    {
        m_stateMapper->ClearMappings();
        m_stateMapper->SetMappingCallback( {} );
    }

    RefreshPermissions();

    return success;
}




bool PLM_ODOO_PLUGIN::IsAuthenticated() const
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );
    return m_isAuthenticated;
}


bool PLM_ODOO_PLUGIN::Ping()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return false;

    // Use a minimal search on res.users as a lightweight connectivity check
    nlohmann::json domain = nlohmann::json::array();
    nlohmann::json fields = nlohmann::json::array( { "id" } );

    ODOO_RPC_RESULT result = m_rpcClient->SearchRead( wxS( "res.users" ), domain, fields, 1 );
    return result.success;
}


PLM_SESSION PLM_ODOO_PLUGIN::GetCurrentSession() const
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );
    return m_session;
}


PLM_AUTH_RESULT PLM_ODOO_PLUGIN::RefreshSession()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_AUTH_RESULT result;

    if( !m_isAuthenticated || !m_rpcClient )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_AUTH_REQUIRED;
        result.error_message = wxS( "Not authenticated" );
        return result;
    }

    ODOO_RPC_RESULT rpcResult = m_rpcClient->CallMethod( wxS( "res.users" ),
                                                          wxS( "check_credentials" ),
                                                          nlohmann::json::array(),
                                                          nlohmann::json::object() );

    if( rpcResult.success )
    {
        int64_t now = static_cast<int64_t>( std::time( nullptr ) );
        m_session.expires_at = now + 7200;

        result.success = true;
        result.token = PLM_SECURE_STRING( m_sessionId );
        result.expires_at = m_session.expires_at;
    }
    else
    {
        m_isAuthenticated = false;
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_AUTH_EXPIRED;
        result.error_message = wxS( "Session has expired" );
    }

    return result;
}


std::vector<PLM_FOLDER> PLM_ODOO_PLUGIN::GetRootFolders()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_FOLDER> folders;

    if( !m_isAuthenticated || !m_rpcClient )
        return folders;

    nlohmann::json domain = nlohmann::json::array();
    domain.push_back( nlohmann::json::array( { "parent_id", "=", false } ) );

    nlohmann::json fields = nlohmann::json::array( { "id", "name", "child_id" } );

    ODOO_RPC_RESULT result = m_rpcClient->SearchRead( wxS( "product.category" ),
                                                       domain, fields, 0, 0, wxS( "name" ) );

    if( result.success && result.result.is_array() )
    {
        try
        {
            for( const auto& cat : result.result )
            {
                PLM_FOLDER folder;
                folder.id = wxString::Format( wxS( "%d" ), cat["id"].get<int>() );
                folder.name = wxString::FromUTF8( cat["name"].get<std::string>() );
                folder.has_children = cat.contains( "child_id" ) && cat["child_id"].is_array() &&
                                      !cat["child_id"].empty();

                m_categoryCache[cat["id"].get<int>()] = folder.name;

                folders.push_back( folder );
            }
        }
        catch( const nlohmann::json::exception& )
        {
            // Malformed JSON response from server, return what we have so far
        }
    }

    return folders;
}


std::vector<PLM_FOLDER> PLM_ODOO_PLUGIN::GetChildFolders( const wxString& aFolderId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_FOLDER> folders;

    if( !m_isAuthenticated || !m_rpcClient || aFolderId.IsEmpty() )
        return folders;

    long parentId = 0;

    if( !aFolderId.ToLong( &parentId ) )
        return folders;

    nlohmann::json domain = nlohmann::json::array();
    domain.push_back( nlohmann::json::array( { "parent_id", "=", static_cast<int>( parentId ) } ) );

    nlohmann::json fields = nlohmann::json::array( { "id", "name", "child_id" } );

    ODOO_RPC_RESULT result = m_rpcClient->SearchRead( wxS( "product.category" ),
                                                       domain, fields, 0, 0, wxS( "name" ) );

    if( result.success && result.result.is_array() )
    {
        try
        {
            for( const auto& cat : result.result )
            {
                PLM_FOLDER folder;
                folder.id = wxString::Format( wxS( "%d" ), cat["id"].get<int>() );
                folder.name = wxString::FromUTF8( cat["name"].get<std::string>() );
                folder.parent_id = aFolderId;
                folder.has_children = cat.contains( "child_id" ) && cat["child_id"].is_array() &&
                                      !cat["child_id"].empty();

                m_categoryCache[cat["id"].get<int>()] = folder.name;

                folders.push_back( folder );
            }
        }
        catch( const nlohmann::json::exception& )
        {
            // Malformed JSON response from server, return what we have so far
        }
    }

    return folders;
}


PLM_FOLDER_PATH PLM_ODOO_PLUGIN::GetFolderPath( const wxString& aFolderId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_FOLDER_PATH path;

    if( !m_isAuthenticated || !m_rpcClient || aFolderId.IsEmpty() )
        return path;

    long categoryId = 0;

    if( !aFolderId.ToLong( &categoryId ) )
        return path;

    try
    {
        while( categoryId > 0 )
        {
            nlohmann::json domain = nlohmann::json::array();
            domain.push_back( nlohmann::json::array( { "id", "=", static_cast<int>( categoryId ) } ) );

            nlohmann::json fields = nlohmann::json::array( { "id", "name", "parent_id" } );

            ODOO_RPC_RESULT result = m_rpcClient->SearchRead( wxS( "product.category" ),
                                                               domain, fields, 1 );

            if( !result.success || !result.result.is_array() || result.result.empty() )
                break;

            const auto& cat = result.result[0];

            PLM_PATH_ELEMENT element;
            element.id = wxString::Format( wxS( "%d" ), cat["id"].get<int>() );
            element.name = wxString::FromUTF8( cat["name"].get<std::string>() );

            if( cat.contains( "parent_id" ) && cat["parent_id"].is_array() &&
                cat["parent_id"].size() >= 1 )
            {
                categoryId = cat["parent_id"][0].get<int>();
            }
            else
            {
                categoryId = 0;
            }

            path.insert( path.begin(), element );
        }
    }
    catch( const nlohmann::json::exception& )
    {
        // Malformed JSON response from server, return partial path
    }

    return path;
}


PLM_LIST_RESULT<PLM_FILE_INFO> PLM_ODOO_PLUGIN::ListFiles( const wxString&         aFolderId,
                                                           const PLM_LIST_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_LIST_RESULT<PLM_FILE_INFO> result;

    if( !m_isAuthenticated || !m_rpcClient )
        return result;

    nlohmann::json domain = nlohmann::json::array();

    if( !aFolderId.IsEmpty() )
    {
        long catId = 0;

        if( aFolderId.ToLong( &catId ) )
            domain.push_back( nlohmann::json::array( { "categ_id", "=", static_cast<int>( catId ) } ) );
    }

    nlohmann::json fields = nlohmann::json::array( {
        "id", "name", "default_code", "categ_id", "write_date", "description", "state"
    } );

    wxString order = aOptions.sort_field;

    if( order.IsEmpty() )
        order = wxS( "name" );

    if( !aOptions.sort_ascending )
        order += wxS( " desc" );

    nlohmann::json  countArgs = nlohmann::json::array( { domain } );
    ODOO_RPC_RESULT countResult = m_rpcClient->CallMethod( wxS( "product.product" ),
                                                            wxS( "search_count" ), countArgs );
    int totalCount = 0;

    if( countResult.success && countResult.result.is_number_integer() )
        totalCount = countResult.result.get<int>();

    ODOO_RPC_RESULT searchResult = m_rpcClient->SearchRead( wxS( "product.product" ),
                                                             domain, fields,
                                                             aOptions.limit, aOptions.offset, order );

    if( searchResult.success && searchResult.result.is_array() )
    {
        try
        {
            for( const auto& product : searchResult.result )
            {
                PLM_FILE_INFO file;
                PopulateFileInfoFromProduct( file, product );
                result.items.push_back( file );
            }
        }
        catch( const nlohmann::json::exception& )
        {
            // Malformed JSON response from server, return what we have so far
        }
    }

    result.total_count = totalCount;
    result.offset = aOptions.offset;
    result.limit = aOptions.limit;

    return result;
}


PLM_LIST_RESULT<PLM_FILE_INFO> PLM_ODOO_PLUGIN::SearchFiles( const wxString&         aQuery,
                                                              const PLM_LIST_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_LIST_RESULT<PLM_FILE_INFO> result;

    if( !m_isAuthenticated || !m_rpcClient || aQuery.IsEmpty() )
        return result;

    nlohmann::json domain = nlohmann::json::array();

    nlohmann::json nameFilter = nlohmann::json::array( { "name", "ilike", aQuery.ToStdString() } );
    nlohmann::json codeFilter = nlohmann::json::array( { "default_code", "ilike", aQuery.ToStdString() } );
    nlohmann::json descFilter = nlohmann::json::array( { "description", "ilike", aQuery.ToStdString() } );

    domain.push_back( "|" );
    domain.push_back( "|" );
    domain.push_back( nameFilter );
    domain.push_back( codeFilter );
    domain.push_back( descFilter );

    nlohmann::json fields = nlohmann::json::array( {
        "id", "name", "default_code", "categ_id", "write_date", "description", "state"
    } );

    wxString order = aOptions.sort_field;

    if( order.IsEmpty() )
        order = wxS( "name" );

    if( !aOptions.sort_ascending )
        order += wxS( " desc" );

    nlohmann::json  countArgs = nlohmann::json::array( { domain } );
    ODOO_RPC_RESULT countResult = m_rpcClient->CallMethod( wxS( "product.product" ),
                                                            wxS( "search_count" ), countArgs );
    int totalCount = 0;

    if( countResult.success && countResult.result.is_number_integer() )
        totalCount = countResult.result.get<int>();

    ODOO_RPC_RESULT searchResult = m_rpcClient->SearchRead( wxS( "product.product" ),
                                                             domain, fields,
                                                             aOptions.limit, aOptions.offset, order );

    if( searchResult.success && searchResult.result.is_array() )
    {
        try
        {
            for( const auto& product : searchResult.result )
            {
                PLM_FILE_INFO file;
                PopulateFileInfoFromProduct( file, product );
                result.items.push_back( file );
            }
        }
        catch( const nlohmann::json::exception& )
        {
            // Malformed JSON response from server, return what we have so far
        }
    }

    result.total_count = totalCount;
    result.offset = aOptions.offset;
    result.limit = aOptions.limit;

    return result;
}


PLM_LIST_RESULT<PLM_FILE_INFO>
PLM_ODOO_PLUGIN::SearchWithFilters( const std::map<wxString, wxString>& aFilters,
                                     const PLM_LIST_OPTIONS&             aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_LIST_RESULT<PLM_FILE_INFO> result;

    if( !m_isAuthenticated || !m_rpcClient )
        return result;

    // Build Odoo domain from filters
    nlohmann::json domain = nlohmann::json::array();

    // Name filter
    auto nameIt = aFilters.find( wxS( "name" ) );

    if( nameIt != aFilters.end() )
        domain.push_back( nlohmann::json::array( { "name", "ilike", nameIt->second.ToStdString() } ) );

    // Category filter (categ_id)
    auto categoryIt = aFilters.find( wxS( "category" ) );

    if( categoryIt != aFilters.end() )
    {
        long catId = 0;

        if( categoryIt->second.ToLong( &catId ) )
        {
            domain.push_back(
                    nlohmann::json::array( { "categ_id", "=", static_cast<int>( catId ) } ) );
        }
    }

    // Modified after filter
    auto afterIt = aFilters.find( wxS( "modified_after" ) );

    if( afterIt != aFilters.end() )
    {
        long timestamp = 0;

        if( afterIt->second.ToLong( &timestamp ) )
        {
            wxDateTime dt( static_cast<time_t>( timestamp ) );
            wxString   dateStr = dt.FormatISOCombined( ' ' );
            domain.push_back(
                    nlohmann::json::array( { "write_date", ">=", dateStr.ToStdString() } ) );
        }
    }

    // Modified before filter
    auto beforeIt = aFilters.find( wxS( "modified_before" ) );

    if( beforeIt != aFilters.end() )
    {
        long timestamp = 0;

        if( beforeIt->second.ToLong( &timestamp ) )
        {
            wxDateTime dt( static_cast<time_t>( timestamp ) );
            wxString   dateStr = dt.FormatISOCombined( ' ' );
            domain.push_back(
                    nlohmann::json::array( { "write_date", "<=", dateStr.ToStdString() } ) );
        }
    }

    // Locked by filter
    auto lockedIt = aFilters.find( wxS( "locked_by" ) );

    if( lockedIt != aFilters.end() )
    {
        if( lockedIt->second == wxS( "me" ) )
        {
            domain.push_back( nlohmann::json::array( { "x_kicad_locked_by", "=", m_userId } ) );
        }
        else if( lockedIt->second == wxS( "any" ) )
        {
            domain.push_back( nlohmann::json::array( { "x_kicad_locked_by", "!=", false } ) );
        }
        else
        {
            long userId = 0;

            if( lockedIt->second.ToLong( &userId ) )
            {
                domain.push_back(
                        nlohmann::json::array( { "x_kicad_locked_by", "=", static_cast<int>( userId ) } ) );
            }
        }
    }

    nlohmann::json fields = nlohmann::json::array( { "id", "name", "default_code", "categ_id",
                                                      "write_date", "description", "state" } );

    wxString order = aOptions.sort_field;

    if( order.IsEmpty() )
        order = wxS( "name" );

    if( !aOptions.sort_ascending )
        order += wxS( " desc" );

    nlohmann::json  countArgs = nlohmann::json::array( { domain } );
    ODOO_RPC_RESULT countResult = m_rpcClient->CallMethod( wxS( "product.product" ),
                                                            wxS( "search_count" ), countArgs );
    int             totalCount = 0;

    if( countResult.success && countResult.result.is_number_integer() )
        totalCount = countResult.result.get<int>();

    ODOO_RPC_RESULT searchResult = m_rpcClient->SearchRead(
            wxS( "product.product" ), domain, fields, aOptions.limit, aOptions.offset, order );

    if( searchResult.success && searchResult.result.is_array() )
    {
        try
        {
            for( const auto& product : searchResult.result )
            {
                PLM_FILE_INFO file;
                PopulateFileInfoFromProduct( file, product );
                result.items.push_back( file );
            }
        }
        catch( const nlohmann::json::exception& )
        {
            // Malformed JSON response from server, return what we have so far
        }
    }

    result.total_count = totalCount;
    result.offset = aOptions.offset;
    result.limit = aOptions.limit;

    return result;
}


std::vector<PLM_SEARCH_FIELD> PLM_ODOO_PLUGIN::GetSearchFields()
{
    std::vector<PLM_SEARCH_FIELD> fields;

    fields.push_back( PLM_SEARCH_FIELD::Text( wxS( "name" ), wxS( "Product Name" ) ) );
    fields.push_back( PLM_SEARCH_FIELD::Text( wxS( "category" ), wxS( "Category ID" ) ) );
    fields.push_back( PLM_SEARCH_FIELD::Date( wxS( "modified_after" ), wxS( "Modified After" ) ) );
    fields.push_back( PLM_SEARCH_FIELD::Date( wxS( "modified_before" ), wxS( "Modified Before" ) ) );

    fields.push_back( PLM_SEARCH_FIELD::Dropdown(
            wxS( "locked_by" ), wxS( "Locked By" ),
            { wxS( "me" ), wxS( "any" ) } ) );

    return fields;
}


PLM_LIST_RESULT<PLM_PART> PLM_ODOO_PLUGIN::SearchParts( const PLM_PART_SEARCH_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_LIST_RESULT<PLM_PART> result;

    if( !m_isAuthenticated || !m_rpcClient )
        return result;

    nlohmann::json domain = nlohmann::json::array();

    if( !aOptions.query.IsEmpty() )
    {
        nlohmann::json orClause = nlohmann::json::array();
        orClause.push_back( wxS( "|" ) );
        orClause.push_back( nlohmann::json::array( { "name", "ilike", aOptions.query.ToStdString() } ) );
        orClause.push_back( wxS( "|" ) );
        orClause.push_back( nlohmann::json::array( { "default_code", "ilike", aOptions.query.ToStdString() } ) );
        orClause.push_back( nlohmann::json::array( { "description", "ilike", aOptions.query.ToStdString() } ) );

        for( const auto& item : orClause )
            domain.push_back( item );
    }

    if( !aOptions.category_filter.IsEmpty() )
    {
        long catId = 0;

        if( aOptions.category_filter.ToLong( &catId ) )
        {
            domain.push_back(
                    nlohmann::json::array( { "categ_id", "=", static_cast<int>( catId ) } ) );
        }
        else
        {
            domain.push_back( nlohmann::json::array(
                    { "categ_id.name", "ilike", aOptions.category_filter.ToStdString() } ) );
        }
    }

    if( !aOptions.manufacturer_filter.IsEmpty() )
    {
        domain.push_back( nlohmann::json::array(
                { "x_manufacturer", "ilike", aOptions.manufacturer_filter.ToStdString() } ) );
    }

    if( !aOptions.include_obsolete )
    {
        domain.push_back( nlohmann::json::array( { "active", "=", true } ) );
    }

    if( aOptions.modified_since > 0 )
    {
        wxDateTime sinceTime( static_cast<time_t>( aOptions.modified_since ) );
        wxString   sinceStr = sinceTime.FormatISOCombined( ' ' );
        domain.push_back( nlohmann::json::array( { "write_date", ">", sinceStr.ToStdString() } ) );
    }

    nlohmann::json fields = nlohmann::json::array( { "id", "name", "default_code", "categ_id",
                                                      "description", "active", "write_date",
                                                      "x_manufacturer", "x_manufacturer_pn",
                                                      "x_datasheet_url", "x_plm_state" } );

    wxString order = aOptions.sort_field;

    if( order.IsEmpty() )
        order = wxS( "name" );

    if( !aOptions.sort_ascending )
        order += wxS( " desc" );

    nlohmann::json  countArgs = nlohmann::json::array( { domain } );
    ODOO_RPC_RESULT countResult = m_rpcClient->CallMethod( wxS( "product.product" ),
                                                            wxS( "search_count" ), countArgs );
    int             totalCount = 0;

    if( countResult.success && countResult.result.is_number_integer() )
        totalCount = countResult.result.get<int>();

    ODOO_RPC_RESULT searchResult = m_rpcClient->SearchRead(
            wxS( "product.product" ), domain, fields, static_cast<int>( aOptions.limit ),
            static_cast<int>( aOptions.offset ), order );

    if( searchResult.success && searchResult.result.is_array() )
    {
        try
        {
            for( const auto& product : searchResult.result )
            {
                PLM_PART part;
                part.part_id = wxString::Format( wxS( "%d" ), product.value( "id", 0 ) );
                part.part_number = wxString::FromUTF8(
                        product.value( "default_code", std::string() ) );
                part.description = wxString::FromUTF8( product.value( "name", std::string() ) );

                if( product.contains( "categ_id" ) && product["categ_id"].is_array() &&
                    product["categ_id"].size() >= 2 )
                {
                    part.category = wxString::FromUTF8( product["categ_id"][1].get<std::string>() );
                }

                part.manufacturer = wxString::FromUTF8(
                        product.value( "x_manufacturer", std::string() ) );
                part.manufacturer_pn = wxString::FromUTF8(
                        product.value( "x_manufacturer_pn", std::string() ) );
                part.datasheet_url = wxString::FromUTF8(
                        product.value( "x_datasheet_url", std::string() ) );

                wxString stateStr = wxString::FromUTF8(
                        product.value( "x_plm_state", std::string( "active" ) ) );

                if( stateStr == wxS( "obsolete" ) )
                    part.state = PLM_PART_STATE::OBSOLETE;
                else if( stateStr == wxS( "eol" ) )
                    part.state = PLM_PART_STATE::END_OF_LIFE;
                else if( stateStr == wxS( "preferred" ) )
                    part.state = PLM_PART_STATE::PREFERRED;
                else if( stateStr == wxS( "not_recommended" ) )
                    part.state = PLM_PART_STATE::NOT_RECOMMENDED;
                else
                    part.state = PLM_PART_STATE::ACTIVE;

                result.items.push_back( part );
            }
        }
        catch( const nlohmann::json::exception& )
        {
        }
    }

    result.total_count = totalCount;
    result.offset = static_cast<int32_t>( aOptions.offset );
    result.limit = static_cast<int32_t>( aOptions.limit );

    return result;
}


PLM_PART PLM_ODOO_PLUGIN::GetPart( const wxString& aPartId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_PART part;

    if( !m_isAuthenticated || !m_rpcClient || aPartId.IsEmpty() )
        return part;

    long productId = 0;

    if( !aPartId.ToLong( &productId ) )
        return part;

    nlohmann::json domain =
            nlohmann::json::array( { nlohmann::json::array( { "id", "=", static_cast<int>( productId ) } ) } );

    nlohmann::json fields = nlohmann::json::array( { "id", "name", "default_code", "categ_id",
                                                      "description", "active", "write_date",
                                                      "create_date", "product_tmpl_id",
                                                      "x_manufacturer", "x_manufacturer_pn",
                                                      "x_datasheet_url", "x_plm_state",
                                                      "x_replacement_id", "x_symbol_ref",
                                                      "x_footprint_ref" } );

    ODOO_RPC_RESULT searchResult =
            m_rpcClient->SearchRead( wxS( "product.product" ), domain, fields, 1, 0, wxS( "id" ) );

    if( searchResult.success && searchResult.result.is_array() &&
        !searchResult.result.empty() )
    {
        try
        {
            const auto& product = searchResult.result[0];

            part.part_id = wxString::Format( wxS( "%d" ), product.value( "id", 0 ) );
            part.part_number = wxString::FromUTF8( product.value( "default_code", std::string() ) );
            part.description = wxString::FromUTF8( product.value( "name", std::string() ) );

            if( product.contains( "categ_id" ) && product["categ_id"].is_array() &&
                product["categ_id"].size() >= 2 )
            {
                part.category = wxString::FromUTF8( product["categ_id"][1].get<std::string>() );
            }

            part.manufacturer = wxString::FromUTF8(
                    product.value( "x_manufacturer", std::string() ) );
            part.manufacturer_pn = wxString::FromUTF8(
                    product.value( "x_manufacturer_pn", std::string() ) );
            part.datasheet_url = wxString::FromUTF8(
                    product.value( "x_datasheet_url", std::string() ) );
            part.symbol_ref = wxString::FromUTF8( product.value( "x_symbol_ref", std::string() ) );
            part.footprint_ref = wxString::FromUTF8(
                    product.value( "x_footprint_ref", std::string() ) );

            if( product.contains( "x_replacement_id" ) && product["x_replacement_id"].is_array() &&
                product["x_replacement_id"].size() >= 1 )
            {
                part.replacement_id = wxString::Format(
                        wxS( "%d" ), product["x_replacement_id"][0].get<int>() );
            }

            wxString stateStr = wxString::FromUTF8(
                    product.value( "x_plm_state", std::string( "active" ) ) );

            if( stateStr == wxS( "obsolete" ) )
                part.state = PLM_PART_STATE::OBSOLETE;
            else if( stateStr == wxS( "eol" ) )
                part.state = PLM_PART_STATE::END_OF_LIFE;
            else if( stateStr == wxS( "discontinued" ) )
                part.state = PLM_PART_STATE::DISCONTINUED;
            else if( stateStr == wxS( "preferred" ) )
                part.state = PLM_PART_STATE::PREFERRED;
            else if( stateStr == wxS( "not_recommended" ) )
                part.state = PLM_PART_STATE::NOT_RECOMMENDED;
            else
                part.state = PLM_PART_STATE::ACTIVE;

            if( product.contains( "create_date" ) && product["create_date"].is_string() )
            {
                wxDateTime dt;

                if( dt.ParseISOCombined( wxString::FromUTF8( product["create_date"].get<std::string>() ) ) )
                    part.created_at = dt.GetTicks();
            }

            if( product.contains( "write_date" ) && product["write_date"].is_string() )
            {
                wxDateTime dt;

                if( dt.ParseISOCombined( wxString::FromUTF8( product["write_date"].get<std::string>() ) ) )
                    part.modified_at = dt.GetTicks();
            }

            // Fetch supplier/vendor info from product.supplierinfo
            int tmplId = 0;

            if( product.contains( "product_tmpl_id" ) && product["product_tmpl_id"].is_array()
                && product["product_tmpl_id"].size() >= 1 )
            {
                tmplId = product["product_tmpl_id"][0].get<int>();
            }

            if( tmplId > 0 )
            {
                nlohmann::json supplierDomain = nlohmann::json::array(
                        { nlohmann::json::array( { "product_tmpl_id", "=", tmplId } ) } );
                nlohmann::json supplierFields = nlohmann::json::array(
                        { "partner_id", "product_code", "price", "delay" } );

                ODOO_RPC_RESULT supplierResult = m_rpcClient->SearchRead(
                        wxS( "product.supplierinfo" ), supplierDomain, supplierFields,
                        1, 0, wxS( "sequence" ) );

                if( supplierResult.success && supplierResult.result.is_array()
                    && !supplierResult.result.empty() )
                {
                    const auto& supplier = supplierResult.result[0];

                    if( supplier.contains( "partner_id" ) && supplier["partner_id"].is_array()
                        && supplier["partner_id"].size() >= 2 )
                    {
                        part.attributes.SetValue( wxS( "vendor_name" ),
                                wxString::FromUTF8( supplier["partner_id"][1].get<std::string>() ) );
                    }

                    wxString supplierCode = wxString::FromUTF8(
                            supplier.value( "product_code", std::string() ) );

                    if( !supplierCode.IsEmpty() )
                        part.attributes.SetValue( wxS( "vendor_part_number" ), supplierCode );

                    double price = supplier.value( "price", 0.0 );

                    if( price > 0.0 )
                    {
                        part.attributes.SetValue( wxS( "price" ),
                                wxString::Format( wxS( "%.4f" ), price ) );
                    }

                    int delay = supplier.value( "delay", 0 );

                    if( delay > 0 )
                    {
                        part.attributes.SetValue( wxS( "lead_time" ),
                                wxString::Format( wxS( "%d" ), delay ) );
                    }
                }
            }
        }
        catch( const nlohmann::json::exception& )
        {
        }
    }

    return part;
}


PLM_BOM_VALIDATION_RESULT PLM_ODOO_PLUGIN::ValidateBOM( const std::vector<wxString>& aPartNumbers )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_BOM_VALIDATION_RESULT result;
    result.total_parts = aPartNumbers.size();

    if( !m_isAuthenticated || !m_rpcClient || aPartNumbers.empty() )
    {
        result.valid_parts = result.total_parts;
        return result;
    }

    nlohmann::json partNumbersArray = nlohmann::json::array();

    for( const auto& partNum : aPartNumbers )
    {
        if( partNum.IsEmpty() )
        {
            ++result.valid_parts;
            continue;
        }

        partNumbersArray.push_back( partNum.ToStdString() );
    }

    if( partNumbersArray.empty() )
    {
        result.all_valid = true;
        return result;
    }

    nlohmann::json domain = nlohmann::json::array(
            { nlohmann::json::array( { "default_code", "in", partNumbersArray } ) } );

    nlohmann::json fields = nlohmann::json::array(
            { "id", "default_code", "active", "x_plm_state", "x_replacement_id",
              "product_tmpl_id" } );

    ODOO_RPC_RESULT searchResult = m_rpcClient->SearchRead(
            wxS( "product.product" ), domain, fields, 0, 0, wxS( "id" ) );

    std::map<std::string, nlohmann::json> productMap;

    if( searchResult.success && searchResult.result.is_array() )
    {
        for( const auto& product : searchResult.result )
        {
            try
            {
                std::string code = product.value( "default_code", std::string() );

                if( !code.empty() )
                    productMap[code] = product;
            }
            catch( const nlohmann::json::exception& )
            {
            }
        }
    }

    // Pre-compute AVL coverage in a single supplierinfo query.
    // product.supplierinfo can attach to either product_id (variant) or
    // product_tmpl_id (template). Fetch both keyings at once so we can do
    // O(1) per-part lookups in the issue loop below without N RPCs.
    //
    // avlQueryOk gates the per-part NoApprovedVendor emission below: a
    // transient RPC failure must not flip the whole BOM to "unvendored",
    // since the user-visible signal there would be a false alarm rather
    // than missing AVL data. If the query fails (or never runs because no
    // products were found) the AVL check silently skips.
    std::set<int> productsWithVendor;
    std::set<int> templatesWithVendor;
    bool          avlQueryOk = false;

    {
        nlohmann::json productIdsJson = nlohmann::json::array();
        nlohmann::json tmplIdsJson = nlohmann::json::array();

        for( const auto& [code, prod] : productMap )
        {
            if( prod.contains( "id" ) && prod["id"].is_number_integer() )
                productIdsJson.push_back( prod["id"].get<int>() );

            if( prod.contains( "product_tmpl_id" ) && prod["product_tmpl_id"].is_array()
                && !prod["product_tmpl_id"].empty()
                && prod["product_tmpl_id"][0].is_number_integer() )
            {
                tmplIdsJson.push_back( prod["product_tmpl_id"][0].get<int>() );
            }
        }

        if( !productIdsJson.empty() || !tmplIdsJson.empty() )
        {
            nlohmann::json avlDomain = nlohmann::json::array();
            avlDomain.push_back( "|" );
            avlDomain.push_back( nlohmann::json::array(
                    { "product_id", "in", productIdsJson } ) );
            avlDomain.push_back( nlohmann::json::array(
                    { "product_tmpl_id", "in", tmplIdsJson } ) );

            nlohmann::json avlFields = nlohmann::json::array(
                    { "product_id", "product_tmpl_id" } );

            ODOO_RPC_RESULT avlResult = m_rpcClient->SearchRead(
                    wxS( "product.supplierinfo" ), avlDomain, avlFields, 0, 0, wxS( "id" ) );

            if( avlResult.success && avlResult.result.is_array() )
            {
                avlQueryOk = true;

                for( const auto& sup : avlResult.result )
                {
                    if( sup.contains( "product_id" ) && sup["product_id"].is_array()
                        && !sup["product_id"].empty()
                        && sup["product_id"][0].is_number_integer() )
                    {
                        productsWithVendor.insert( sup["product_id"][0].get<int>() );
                    }

                    if( sup.contains( "product_tmpl_id" ) && sup["product_tmpl_id"].is_array()
                        && !sup["product_tmpl_id"].empty()
                        && sup["product_tmpl_id"][0].is_number_integer() )
                    {
                        templatesWithVendor.insert( sup["product_tmpl_id"][0].get<int>() );
                    }
                }
            }
        }
    }

    for( const auto& partNum : aPartNumbers )
    {
        if( partNum.IsEmpty() )
            continue;

        auto it = productMap.find( partNum.ToStdString() );

        if( it == productMap.end() )
        {
            result.AddIssue( PLM_BOM_ISSUE::Missing( partNum, partNum ) );
            continue;
        }

        try
        {
            const auto& product = it->second;
            wxString    stateStr = wxString::FromUTF8(
                    product.value( "x_plm_state", std::string( "active" ) ) );

            bool active = product.value( "active", true );

            wxString replacement;

            if( product.contains( "x_replacement_id" ) && product["x_replacement_id"].is_array()
                && product["x_replacement_id"].size() >= 1 )
            {
                replacement = wxString::Format(
                        wxS( "%d" ), product["x_replacement_id"][0].get<int>() );
            }

            bool hasLifecycleIssue = false;

            if( !active || stateStr == wxS( "obsolete" ) || stateStr == wxS( "eol" )
                || stateStr == wxS( "discontinued" ) )
            {
                result.AddIssue( PLM_BOM_ISSUE::Obsolete( partNum, partNum, replacement ) );
                hasLifecycleIssue = true;
            }
            else if( stateStr == wxS( "not_recommended" ) )
            {
                result.AddIssue(
                        PLM_BOM_ISSUE::NotRecommended( partNum, partNum, replacement ) );
                hasLifecycleIssue = true;
            }

            // AVL coverage check; runs even when the part is also flagged
            // for a lifecycle issue so procurement gets the full picture.
            // Skipped entirely when the supplierinfo query failed or never
            // ran, to avoid emitting false-positive NoApprovedVendor on a
            // transient RPC error. valid_parts then falls back to the
            // pre-AVL semantic (lifecycle-clean = valid).
            bool hasVendor = true;

            if( avlQueryOk )
            {
                int productId = product.value( "id", 0 );
                int tmplId = 0;

                if( product.contains( "product_tmpl_id" )
                    && product["product_tmpl_id"].is_array()
                    && !product["product_tmpl_id"].empty()
                    && product["product_tmpl_id"][0].is_number_integer() )
                {
                    tmplId = product["product_tmpl_id"][0].get<int>();
                }

                hasVendor = ( productId != 0 && productsWithVendor.count( productId ) > 0 )
                            || ( tmplId != 0 && templatesWithVendor.count( tmplId ) > 0 );

                if( !hasVendor )
                    result.AddIssue( PLM_BOM_ISSUE::NoApprovedVendor( partNum, partNum ) );
            }

            if( !hasLifecycleIssue && hasVendor )
                ++result.valid_parts;
        }
        catch( const nlohmann::json::exception& )
        {
            result.AddIssue( PLM_BOM_ISSUE::Missing( partNum, partNum ) );
        }
    }

    result.all_valid = result.issues.empty();
    return result;
}


std::vector<PLM_PART_REPLACEMENT> PLM_ODOO_PLUGIN::GetReplacements( const wxString& aPartId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_PART_REPLACEMENT> replacements;

    if( !m_isAuthenticated || !m_rpcClient || aPartId.IsEmpty() )
        return replacements;

    PLM_PART part = GetPart( aPartId );

    if( part.HasReplacement() )
    {
        PLM_PART replacementPart = GetPart( part.replacement_id );

        if( !replacementPart.part_id.IsEmpty() )
        {
            PLM_PART_REPLACEMENT repl( aPartId, replacementPart.part_id,
                                        wxS( "Recommended replacement from PLM" ) );
            repl.verified = true;
            replacements.push_back( repl );
        }
    }

    return replacements;
}


bool PLM_ODOO_PLUGIN::DownloadPartLibrary( const wxString& aPartId, const wxString& aLocalPath )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient || aPartId.IsEmpty() )
        return false;

    long productId = 0;

    if( !aPartId.ToLong( &productId ) )
        return false;

    nlohmann::json domain = nlohmann::json::array( {
            nlohmann::json::array( { "res_model", "=", "product.product" } ),
            nlohmann::json::array( { "res_id", "=", static_cast<int>( productId ) } ),
            wxS( "|" ),
            nlohmann::json::array( { "name", "ilike", ".kicad_sym" } ),
            nlohmann::json::array( { "name", "ilike", ".kicad_mod" } )
    } );

    nlohmann::json fields = nlohmann::json::array( { "id", "name", "datas" } );

    ODOO_RPC_RESULT searchResult = m_rpcClient->SearchRead(
            wxS( "ir.attachment" ), domain, fields, 100, 0, wxS( "name" ) );

    if( !searchResult.success || !searchResult.result.is_array() ||
        searchResult.result.empty() )
    {
        return false;
    }

    if( !wxDirExists( aLocalPath ) )
        wxMkdir( aLocalPath );

    bool anyDownloaded = false;

    try
    {
        for( const auto& attachment : searchResult.result )
        {
            wxString filename = wxString::FromUTF8( attachment.value( "name", std::string() ) );

            if( filename.IsEmpty() )
                continue;

            wxString base64Data = wxString::FromUTF8( attachment.value( "datas", std::string() ) );

            if( base64Data.IsEmpty() )
                continue;

            wxMemoryBuffer data = DecodeBase64( base64Data );

            if( data.GetDataLen() > 0 )
            {
                wxString filePath =
                        aLocalPath + wxFileName::GetPathSeparator() + filename;
                wxFile file( filePath, wxFile::write );

                if( file.IsOpened() )
                {
                    file.Write( data.GetData(), data.GetDataLen() );
                    file.Close();
                    anyDownloaded = true;
                }
            }
        }
    }
    catch( const nlohmann::json::exception& )
    {
        return false;
    }

    return anyDownloaded;
}


PLM_CHECKOUT_RESULT PLM_ODOO_PLUGIN::Checkout( const wxString&             aProjectId,
                                                const wxString&             aLocalPath,
                                                const PLM_CHECKOUT_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_CHECKOUT_RESULT result;

    if( !m_isAuthenticated || !m_rpcClient )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_AUTH_REQUIRED;
        result.error_message = wxS( "Not authenticated" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT, aProjectId, false, result.error_message );
        return result;
    }

    if( aProjectId.IsEmpty() )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_INVALID_PARAMETER;
        result.error_message = wxS( "Project ID is required" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT, aProjectId, false, result.error_message );
        return result;
    }

    PLM_FILE_INFO fileInfo = GetFileInfo( aProjectId );

    if( fileInfo.id.IsEmpty() )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_FILE_NOT_FOUND;
        result.error_message = wxS( "Product not found" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT, aProjectId, false, result.error_message );
        return result;
    }

    if( aOptions.acquire_lock )
    {
        PLM_LOCK_REQUEST lockReq;
        lockReq.project_id = aProjectId;
        PLM_LOCK_RESULT lockResult = Lock( lockReq );

        if( !lockResult.success )
        {
            result.success = false;
            result.error_code = lockResult.error_code;
            result.error_message = lockResult.error_message;
            RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT, aProjectId, false,
                              result.error_message );
            return result;
        }

        fileInfo.lock_owner = m_username;
    }

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_INVALID_PARAMETER;
        result.error_message = wxS( "Invalid project ID" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT, aProjectId, false, result.error_message );
        return result;
    }

    wxString downloadWarnings;

    if( !DownloadProductAttachments( static_cast<int>( productId ), aLocalPath,
                                     &downloadWarnings ) )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_INTERNAL;
        result.error_message = wxS( "Failed to download attachments" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT, aProjectId, false, result.error_message );
        return result;
    }

    if( !WriteCheckoutMetadata( aLocalPath, aProjectId, fileInfo.revision ) )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_INTERNAL;
        result.error_message = wxS( "Failed to write checkout metadata" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT, aProjectId, false, result.error_message );
        return result;
    }

    WritePLMCheckoutMetadata( aLocalPath, wxS( "odoo-plm" ), aProjectId, fileInfo.revision,
                              m_username, m_serverUrl );

    // Record baseline file hashes for dirty tracking
    if( wxDirExists( aLocalPath ) )
    {
        wxDir checkoutDir( aLocalPath );
        wxString filename;
        bool cont = checkoutDir.GetFirst( &filename, wxEmptyString, wxDIR_FILES );

        while( cont )
        {
            wxString filePath = aLocalPath + wxFileName::GetPathSeparator() + filename;

            PLM_DIRTY_TRACKER::GetInstance().RecordCheckout(
                    filePath, PLM_DIRTY_TRACKER::CalculateFileHash( filePath ) );

            cont = checkoutDir.GetNext( &filename );
        }
    }

    result.success = true;
    result.local_path = aLocalPath;
    result.file_info = fileInfo;

    if( !downloadWarnings.IsEmpty() )
        result.error_message = downloadWarnings;

    PersistProjectState( aLocalPath, aProjectId, fileInfo.revision );
    RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT, aProjectId, true, fileInfo.revision );

    return result;
}


PLM_CHECKIN_RESULT PLM_ODOO_PLUGIN::Checkin( const wxString&            aLocalPath,
                                              const PLM_CHECKIN_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_CHECKIN_RESULT result;

    if( !m_isAuthenticated || !m_rpcClient )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_AUTH_REQUIRED;
        result.error_message = wxS( "Not authenticated" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKIN, wxEmptyString, false, result.error_message );
        return result;
    }

    PLM_PERMISSIONS perms = GetPermissions( aLocalPath );

    if( !perms.can_checkin )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_AUTH_INSUFFICIENT;
        result.error_message = OdooErrorMapper::FormatPermissionError( wxS( "check in" ),
                                                                       ODOO_RPC_RESULT() );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKIN, wxEmptyString, false, result.error_message );
        return result;
    }

    if( aLocalPath.IsEmpty() || !wxDirExists( aLocalPath ) )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_INVALID_PARAMETER;
        result.error_message = wxS( "Invalid local path" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKIN, wxEmptyString, false, result.error_message );
        return result;
    }

    wxString projectId;
    wxString revision;

    if( !ReadCheckoutMetadata( aLocalPath, projectId, revision ) )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_INVALID_PARAMETER;
        result.error_message = wxS( "No checkout metadata found - was this directory checked out?" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKIN, wxEmptyString, false, result.error_message );
        return result;
    }

    long productId = 0;

    if( !projectId.ToLong( &productId ) )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_INVALID_PARAMETER;
        result.error_message = wxS( "Invalid product ID in checkout metadata" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKIN, projectId, false, result.error_message );
        return result;
    }

    // Verify the current user still holds the lock before uploading
    PLM_LOCK_INFO lockInfo = GetLockInfo( projectId );

    if( !lockInfo.is_locked || !lockInfo.locked_by_me )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_LOCK_NOT_HELD;
        result.error_message = wxS( "Lock is not held by the current user" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKIN, projectId, false, result.error_message );
        return result;
    }

    wxString uploadWarnings;

    if( !UploadProjectFiles( static_cast<int>( productId ), aLocalPath, &uploadWarnings ) )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_INTERNAL;
        result.error_message = wxS( "Failed to upload files" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKIN, projectId, false, result.error_message );
        return result;
    }

    // Mark all checked-out files as clean after successful upload
    if( wxDirExists( aLocalPath ) )
    {
        wxDir cleanDir( aLocalPath );
        wxString cleanFilename;
        bool hasCont = cleanDir.GetFirst( &cleanFilename, wxEmptyString, wxDIR_FILES );

        while( hasCont )
        {
            wxString filePath = aLocalPath + wxFileName::GetPathSeparator() + cleanFilename;

            PLM_DIRTY_TRACKER::GetInstance().MarkClean( filePath );
            hasCont = cleanDir.GetNext( &cleanFilename );
        }
    }

    wxString metadataPath = aLocalPath + wxFileName::GetPathSeparator() + ODOO_PLM_METADATA_FILE;

    if( wxFileExists( metadataPath ) )
        wxRemoveFile( metadataPath );

    result.success = true;

    if( !uploadWarnings.IsEmpty() )
        result.error_message = uploadWarnings;

    // Post the revision comment to the product's chatter
    if( !aOptions.revision_comment.IsEmpty() )
    {
        nlohmann::json msgArgs = nlohmann::json::array();
        msgArgs.push_back( static_cast<int>( productId ) );
        nlohmann::json msgKwargs;
        msgKwargs["body"] = aOptions.revision_comment.ToStdString();
        msgKwargs["message_type"] = "comment";

        ODOO_RPC_RESULT msgResult = m_rpcClient->CallMethod( wxS( "product.product" ),
                                                              wxS( "message_post" ),
                                                              msgArgs, msgKwargs );

        if( !msgResult.success )
        {
            if( !result.error_message.IsEmpty() )
                result.error_message += wxS( "; " );

            result.error_message += wxS( "Revision comment could not be posted" );
        }
    }

    // Query server for actual write_date to use as revision identifier
    nlohmann::json domain = nlohmann::json::array();
    domain.push_back( nlohmann::json::array( { "id", "=", static_cast<int>( productId ) } ) );

    nlohmann::json fields = nlohmann::json::array( { "write_date" } );

    ODOO_RPC_RESULT readResult = m_rpcClient->SearchRead( wxS( "product.product" ),
                                                          domain, fields, 1 );

    if( readResult.success && readResult.result.is_array() && !readResult.result.empty() )
    {
        try
        {
            const auto& product = readResult.result[0];

            if( product.contains( "write_date" ) && product["write_date"].is_string() )
                result.new_revision = wxString::FromUTF8( product["write_date"].get<std::string>() );
            else
                result.new_revision = wxDateTime::Now().FormatISOCombined();
        }
        catch( const nlohmann::json::exception& )
        {
            result.new_revision = wxDateTime::Now().FormatISOCombined();
        }
    }
    else
    {
        result.new_revision = wxDateTime::Now().FormatISOCombined();
    }

    // Associate the checkin with the requested ECO. mrp.eco inherits
    // mail.thread, so message_post is schema-stable across Odoo deployments
    // (a relation column would need OdooPLM-specific schema). The chatter
    // entry references the product/checkin so the ECO record carries the
    // forward link. Skipped on community OdooPLM where mrp.eco doesn't
    // exist; the checkin itself still succeeds.
    if( !aOptions.eco_id.IsEmpty() && m_ecoTypeProbe.value_or( false ) )
    {
        long ecoIdLong = 0;

        if( aOptions.eco_id.ToLong( &ecoIdLong ) && ecoIdLong > 0 )
        {
            wxString body = wxString::Format(
                    wxS( "Checkin from KiCad PLM: project %s, revision %s" ),
                    projectId, result.new_revision );

            nlohmann::json ecoArgs = nlohmann::json::array();
            ecoArgs.push_back( static_cast<int>( ecoIdLong ) );
            nlohmann::json ecoKwargs;
            ecoKwargs["body"] = body.ToStdString();
            ecoKwargs["message_type"] = "comment";

            ODOO_RPC_RESULT ecoResult = m_rpcClient->CallMethod( wxS( "mrp.eco" ),
                                                                  wxS( "message_post" ),
                                                                  ecoArgs, ecoKwargs );

            if( !ecoResult.success )
            {
                if( !result.error_message.IsEmpty() )
                    result.error_message += wxS( "; " );

                result.error_message += wxString::Format( wxS( "ECO %s could not be linked" ),
                                                           aOptions.eco_id );
            }
        }
        else
        {
            if( !result.error_message.IsEmpty() )
                result.error_message += wxS( "; " );

            result.error_message += wxString::Format( wxS( "Invalid ECO id: %s" ),
                                                       aOptions.eco_id );
        }
    }

    // Release the lock after successful checkin if requested
    if( aOptions.release_lock )
    {
        PLM_UNLOCK_REQUEST unlockReq;
        unlockReq.project_id = projectId;
        Unlock( unlockReq );
    }

    if( result.success )
        PersistProjectState( aLocalPath, projectId, result.new_revision );

    RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKIN, projectId, result.success,
                      result.new_revision );

    return result;
}


PLM_FILE_INFO PLM_ODOO_PLUGIN::GetFileInfo( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_FILE_INFO file;

    if( !m_isAuthenticated || !m_rpcClient || aProjectId.IsEmpty() )
        return file;

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) )
        return file;

    nlohmann::json domain = nlohmann::json::array();
    domain.push_back( nlohmann::json::array( { "id", "=", static_cast<int>( productId ) } ) );

    nlohmann::json fields = nlohmann::json::array( {
        "id", "name", "default_code", "categ_id", "write_date", "description", "state"
    } );

    ODOO_RPC_RESULT result = m_rpcClient->SearchRead( wxS( "product.product" ),
                                                       domain, fields, 1 );

    if( result.success && result.result.is_array() && !result.result.empty() )
        PopulateFileInfoFromProduct( file, result.result[0] );

    return file;
}


PLM_CHECKOUT_RESULT PLM_ODOO_PLUGIN::CreateProject( const wxString&          aFolderId,
                                                     const wxString&          aName,
                                                     const PLM_ATTRIBUTE_SET& aMetadata,
                                                     const wxString&          aTemplateId,
                                                     const PLM_CLONE_OPTIONS& aCloneOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_CHECKOUT_RESULT result;

    if( !m_isAuthenticated || !m_rpcClient )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_AUTH_REQUIRED;
        result.error_message = wxS( "Not authenticated" );
        return result;
    }

    if( aName.IsEmpty() )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_INVALID_PARAMETER;
        result.error_message = wxS( "Project name is required" );
        return result;
    }

    long sourceProductId = 0;
    bool isCloneFromProduct = !aTemplateId.IsEmpty() && aTemplateId.ToLong( &sourceProductId );

    if( !aTemplateId.IsEmpty() && !isCloneFromProduct )
    {
        nlohmann::json dupDomain = nlohmann::json::array();
        dupDomain.push_back( nlohmann::json::array(
                { "default_code", "=", aTemplateId.ToStdString() } ) );

        nlohmann::json dupFields = nlohmann::json::array( { "id" } );

        ODOO_RPC_RESULT dupResult = m_rpcClient->SearchRead(
                wxS( "product.product" ), dupDomain, dupFields, 1, 0 );

        if( dupResult.success && dupResult.result.is_array() && !dupResult.result.empty() )
        {
            result.success = false;
            result.error_code = PLM_ERROR_CODE::ERR_FILE_EXISTS;
            result.error_message = wxS( "A product with this code already exists" );
            return result;
        }
    }

    ODOO_RPC_RESULT createResult;

    if( isCloneFromProduct )
    {
        nlohmann::json copyArgs = nlohmann::json::array( { sourceProductId } );
        nlohmann::json defaults;
        defaults["name"] = aName.ToStdString();

        if( !aFolderId.IsEmpty() )
        {
            long catId = 0;

            if( aFolderId.ToLong( &catId ) )
                defaults["categ_id"] = static_cast<int>( catId );
        }

        nlohmann::json kwargs;
        kwargs["default"] = defaults;

        createResult = m_rpcClient->CallMethod( wxS( "product.product" ), wxS( "copy" ),
                                                copyArgs, kwargs );
    }
    else
    {
        nlohmann::json values;
        values["name"] = aName.ToStdString();

        if( !aTemplateId.IsEmpty() )
            values["default_code"] = aTemplateId.ToStdString();

        if( !aFolderId.IsEmpty() )
        {
            long catId = 0;

            if( aFolderId.ToLong( &catId ) )
                values["categ_id"] = static_cast<int>( catId );
        }

        createResult = m_rpcClient->Create( wxS( "product.product" ), values );
    }

    if( createResult.success && createResult.result.is_number() )
    {
        try
        {
            int newId = createResult.result.get<int>();

            result.success = true;
            result.file_info.id = wxString::Format( wxS( "%d" ), newId );
            result.file_info.name = aName;
            result.file_info.folder_id = aFolderId;
            result.file_info.state_category = PLM_STATE_CATEGORY::EDITABLE;
        }
        catch( const nlohmann::json::exception& )
        {
            result.success = false;
            result.error_code = PLM_ERROR_CODE::ERR_INTERNAL;
            result.error_message = wxS( "Invalid server response" );
        }
    }
    else
    {
        result.success = false;
        result.error_code = OdooErrorMapper::FromRpcResult( createResult );
        result.error_message = OdooErrorMapper::FormatErrorMessage( createResult );
    }

    return result;
}


bool PLM_ODOO_PLUGIN::DeleteProject( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient || aProjectId.IsEmpty() )
        return false;

    PLM_PERMISSIONS perms = GetPermissions( aProjectId );

    if( !perms.can_delete )
    {
        wxLogWarning( wxS( "Permission denied: cannot delete project. "
                           "Check your Odoo group memberships." ) );
        return false;
    }

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) )
        return false;

    nlohmann::json ids = nlohmann::json::array();
    ids.push_back( static_cast<int>( productId ) );

    ODOO_RPC_RESULT result = m_rpcClient->Unlink( wxS( "product.product" ), ids );

    return result.success;
}


PLM_CANCEL_CHECKOUT_RESULT PLM_ODOO_PLUGIN::CancelCheckout( const wxString& aLocalPath )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
    {
        return PLM_CANCEL_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                                     wxS( "Not authenticated" ) );
    }

    if( aLocalPath.IsEmpty() || !wxDirExists( aLocalPath ) )
    {
        return PLM_CANCEL_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_FILE_NOT_FOUND,
                                                     wxS( "Checkout path not found" ) );
    }

    wxString projectId;
    wxString revision;

    if( !ReadCheckoutMetadata( aLocalPath, projectId, revision ) )
    {
        return PLM_CANCEL_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_FILE_NOT_FOUND,
                                                     wxS( "No checkout metadata found" ) );
    }

    PLM_CANCEL_CHECKOUT_RESULT cancelResult;
    cancelResult.success = true;

    // Release any locks we may have acquired during checkout
    PLM_LOCK_INFO lockInfo = GetLockInfo( projectId );

    if( lockInfo.is_locked && lockInfo.locked_by_me )
    {
        PLM_UNLOCK_REQUEST unlockReq;
        unlockReq.project_id = projectId;
        PLM_LOCK_RESULT unlockResult = Unlock( unlockReq );

        if( !unlockResult.success )
        {
            return PLM_CANCEL_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_CANCEL_UNLOCK_FAILED,
                                                         wxS( "Failed to release lock" ) );
        }

        cancelResult.lock_released = true;
    }

    // Remove checkout metadata file
    wxString metadataPath = aLocalPath + wxFileName::GetPathSeparator() + ODOO_PLM_METADATA_FILE;

    if( wxFileExists( metadataPath ) )
        wxRemoveFile( metadataPath );

    // Remove common PLM metadata file
    wxString commonMetadataPath = aLocalPath + wxFileName::GetPathSeparator() + wxS( ".kicad_plm" );

    if( wxFileExists( commonMetadataPath ) )
        wxRemoveFile( commonMetadataPath );

    // Remove all local project files from the checkout directory
    wxDir dir( aLocalPath );
    wxString filename;
    bool cont = dir.GetFirst( &filename, wxEmptyString, wxDIR_FILES );

    while( cont )
    {
        wxString filePath = aLocalPath + wxFileName::GetPathSeparator() + filename;
        wxRemoveFile( filePath );
        cont = dir.GetNext( &filename );
    }

    cancelResult.files_restored = true;
    RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::CHECKOUT_CANCELLED, projectId, true );
    return cancelResult;
}


PLM_REVISION_HISTORY PLM_ODOO_PLUGIN::GetRevisionHistory( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_REVISION_HISTORY history;
    history.file_id = aProjectId;

    if( !m_isAuthenticated || !m_rpcClient || aProjectId.IsEmpty() )
        return history;

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) || productId <= 0 )
        return history;

    // On stock Odoo (no OdooPLM module) there is no revision history; surface
    // the current product as a single synthetic "rev 1" entry so the revision
    // dialog still has something to display.
    if( !m_odooPlmProbe.value_or( false ) )
    {
        PLM_FILE_INFO fileInfo = GetFileInfo( aProjectId );

        if( !fileInfo.id.IsEmpty() )
        {
            PLM_REVISION rev;
            rev.revision_id = wxS( "1" );
            rev.revision_number = wxS( "1" );
            rev.author = m_username;
            rev.timestamp = fileInfo.modified_timestamp;
            rev.comment = wxS( "Initial version" );
            rev.state_category = fileInfo.state_category;
            rev.native_state = fileInfo.native_state;

            history.revisions.push_back( rev );
        }

        return history;
    }

    // OdooPLM tracks revisions as separate product.product records linked
    // through engineering_branch_parent_id (newer revision points to older).
    // Walk the chain back to the root by following parent pointers, then
    // also walk forward to capture later revisions of the same engineering
    // code. The starting product itself is in both walks.
    auto readRevisionFields = [&]( long aId ) -> nlohmann::json
    {
        nlohmann::json ids = nlohmann::json::array( { static_cast<int>( aId ) } );
        nlohmann::json fields = nlohmann::json::array(
                { "id", "engineering_code", "engineering_revision",
                  "engineering_revision_letter", "engineering_state",
                  "engineering_release_date", "engineering_release_user",
                  "engineering_revision_user", "engineering_revision_date",
                  "engineering_branch_parent_id", "write_date" } );

        ODOO_RPC_RESULT res = m_rpcClient->Read( wxS( "product.product" ), ids, fields );

        if( res.success && res.result.is_array() && !res.result.empty() )
            return res.result[0];

        return nlohmann::json();
    };

    auto recordToRevision = [&]( const nlohmann::json& aRec ) -> PLM_REVISION
    {
        PLM_REVISION rev;

        if( aRec.contains( "id" ) && aRec["id"].is_number_integer() )
            rev.revision_id = wxString::Format( wxS( "%d" ), aRec["id"].get<int>() );

        if( aRec.contains( "engineering_revision" )
            && aRec["engineering_revision"].is_number_integer() )
        {
            rev.revision_number = wxString::Format(
                    wxS( "%d" ), aRec["engineering_revision"].get<int>() );
        }
        else
        {
            rev.revision_number = rev.revision_id;
        }

        if( aRec.contains( "engineering_revision_letter" )
            && aRec["engineering_revision_letter"].is_string() )
        {
            wxString letter = wxString::FromUTF8(
                    aRec["engineering_revision_letter"].get<std::string>() );

            if( !letter.IsEmpty() )
                rev.revision_number += wxS( " (" ) + letter + wxS( ")" );
        }

        if( aRec.contains( "engineering_revision_user" )
            && aRec["engineering_revision_user"].is_array()
            && aRec["engineering_revision_user"].size() >= 2
            && aRec["engineering_revision_user"][1].is_string() )
        {
            rev.author = wxString::FromUTF8(
                    aRec["engineering_revision_user"][1].get<std::string>() );
        }
        else
        {
            rev.author = m_username;
        }

        wxString tsStr;

        if( aRec.contains( "engineering_revision_date" )
            && aRec["engineering_revision_date"].is_string() )
        {
            tsStr = wxString::FromUTF8(
                    aRec["engineering_revision_date"].get<std::string>() );
        }
        else if( aRec.contains( "write_date" ) && aRec["write_date"].is_string() )
        {
            tsStr = wxString::FromUTF8( aRec["write_date"].get<std::string>() );
        }

        if( !tsStr.IsEmpty() )
        {
            wxDateTime dt;

            // Odoo serialises datetimes as "YYYY-MM-DD HH:MM:SS" (space, not 'T').
            if( dt.ParseISOCombined( tsStr, ' ' ) || dt.ParseISOCombined( tsStr ) )
                rev.timestamp = dt.GetTicks();
        }

        if( aRec.contains( "engineering_state" ) && aRec["engineering_state"].is_string() )
        {
            wxString state = wxString::FromUTF8(
                    aRec["engineering_state"].get<std::string>() );

            rev.native_state = state;

            // Map OdooPLM lifecycle states to the abstracted KiCad enum.
            // obsoleted → LOCKED because the revision is read-only; users
            // must work from a newer revision rather than edit it.
            if( state == wxS( "released" ) )
                rev.state_category = PLM_STATE_CATEGORY::RELEASED;
            else if( state == wxS( "obsoleted" ) )
                rev.state_category = PLM_STATE_CATEGORY::LOCKED;
            else if( state == wxS( "confirmed" ) )
                rev.state_category = PLM_STATE_CATEGORY::PENDING_REVIEW;
            else
                rev.state_category = PLM_STATE_CATEGORY::EDITABLE;
        }

        return rev;
    };

    // Walk parent chain back to the root.
    std::vector<PLM_REVISION> chain;
    std::set<long>            visited;
    long                      currentId = productId;
    wxString                  rootEngCode;

    while( currentId > 0 && visited.insert( currentId ).second )
    {
        nlohmann::json rec = readRevisionFields( currentId );

        if( rec.is_null() )
            break;

        chain.push_back( recordToRevision( rec ) );

        if( rec.contains( "engineering_code" ) && rec["engineering_code"].is_string() )
            rootEngCode = wxString::FromUTF8( rec["engineering_code"].get<std::string>() );

        long parent = 0;

        if( rec.contains( "engineering_branch_parent_id" )
            && rec["engineering_branch_parent_id"].is_number_integer() )
        {
            parent = rec["engineering_branch_parent_id"].get<int>();
        }

        if( parent <= 0 || parent == currentId )
            break;

        currentId = parent;
    }

    // Walk forward through later revisions on the same engineering_code by
    // searching for products that branch from any id we've already visited.
    if( !rootEngCode.IsEmpty() )
    {
        nlohmann::json domain = nlohmann::json::array();
        domain.push_back( nlohmann::json::array(
                { "engineering_code", "=", rootEngCode.ToStdString() } ) );

        nlohmann::json fields = nlohmann::json::array(
                { "id", "engineering_code", "engineering_revision",
                  "engineering_revision_letter", "engineering_state",
                  "engineering_release_date", "engineering_release_user",
                  "engineering_revision_user", "engineering_revision_date",
                  "engineering_branch_parent_id", "write_date" } );

        ODOO_RPC_RESULT all = m_rpcClient->SearchRead(
                wxS( "product.product" ), domain, fields, 0, 0,
                wxS( "engineering_revision asc" ) );

        if( all.success && all.result.is_array() )
        {
            for( const auto& rec : all.result )
            {
                if( !rec.contains( "id" ) || !rec["id"].is_number_integer() )
                    continue;

                long id = rec["id"].get<int>();

                if( !visited.insert( id ).second )
                    continue;

                chain.push_back( recordToRevision( rec ) );
            }
        }
    }

    // chain is built parent-first then forward sweep; reorder by revision
    // number ascending so consumers see chronological history. PLM_REVISION
    // exposes revision_number as a string, so compare as long when both
    // sides parse cleanly and fall back to lexicographic otherwise.
    std::sort( chain.begin(), chain.end(),
               []( const PLM_REVISION& a, const PLM_REVISION& b )
               {
                   long la = 0;
                   long lb = 0;
                   bool aok = a.revision_number.BeforeFirst( wxChar( ' ' ) ).ToLong( &la );
                   bool bok = b.revision_number.BeforeFirst( wxChar( ' ' ) ).ToLong( &lb );

                   if( aok && bok )
                       return la < lb;

                   return a.revision_number < b.revision_number;
               } );

    history.revisions = std::move( chain );
    return history;
}


PLM_REVISION PLM_ODOO_PLUGIN::GetRevision( const wxString& aProjectId, const wxString& aRevisionId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_REVISION rev;

    if( !m_isAuthenticated || !m_rpcClient || aProjectId.IsEmpty() )
        return rev;

    PLM_REVISION_HISTORY history = GetRevisionHistory( aProjectId );

    for( const auto& revision : history.revisions )
    {
        if( revision.revision_id == aRevisionId )
            return revision;
    }

    return rev;
}


bool PLM_ODOO_PLUGIN::DownloadRevision( const wxString& aProjectId, const wxString& aRevisionId,
                                         const wxString& aLocalPath )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient || aProjectId.IsEmpty() || aLocalPath.IsEmpty() )
        return false;

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) )
        return false;

    // Standard Odoo doesn't maintain historical attachment versions.
    // Download the current attachments for this product.
    // OdooPLM module users should configure attachment archiving separately.
    if( !wxDirExists( aLocalPath ) && !wxMkdir( aLocalPath ) )
        return false;

    wxString warnings;

    if( !DownloadProductAttachments( static_cast<int>( productId ), aLocalPath, &warnings ) )
        return false;

    if( !warnings.IsEmpty() )
        wxLogWarning( wxS( "DownloadRevision partial failures: %s" ), warnings );

    return true;
}


PLM_REVISION_COMPARE PLM_ODOO_PLUGIN::CompareRevisions( const wxString& aProjectId,
                                                         const wxString& aRevision1,
                                                         const wxString& aRevision2 )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_REVISION_COMPARE compare;

    if( !m_isAuthenticated || !m_rpcClient || aProjectId.IsEmpty() )
        return compare;

    // Fetch history once and resolve both revisions in-memory. Going through
    // GetRevision() instead would walk the OdooPLM parent chain twice and
    // re-run the forward sweep, doubling the RPC cost on long histories for
    // no semantic benefit.
    PLM_REVISION_HISTORY history = GetRevisionHistory( aProjectId );

    auto findRevision = [&]( const wxString& aRevId ) -> PLM_REVISION
    {
        for( const auto& rev : history.revisions )
        {
            if( rev.revision_id == aRevId )
                return rev;
        }

        return PLM_REVISION();
    };

    compare.base_revision = findRevision( aRevision1 );
    compare.compare_revision = findRevision( aRevision2 );

    // Per-attachment diff is not available without OdooPLM document
    // archiving; the revision metadata above is the comparable surface.
    return compare;
}


bool PLM_ODOO_PLUGIN::RestoreRevision( const wxString& aProjectId, const wxString& aRevisionId,
                                        const wxString& aComment )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );
    return false;
}


PLM_LOCK_RESULT PLM_ODOO_PLUGIN::Lock( const PLM_LOCK_REQUEST& aRequest )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_LOCK_RESULT result;

    if( !m_isAuthenticated || !m_rpcClient )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_AUTH_REQUIRED;
        result.error_message = wxS( "Not authenticated" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED, aRequest.project_id, false,
                          result.error_message );
        return result;
    }

    PLM_PERMISSIONS perms = GetPermissions( aRequest.project_id );

    if( !perms.can_lock )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_AUTH_INSUFFICIENT;
        result.error_message = OdooErrorMapper::FormatPermissionError( wxS( "lock" ),
                                                                       ODOO_RPC_RESULT() );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_DENIED, aRequest.project_id, false,
                          result.error_message );
        return result;
    }

    if( aRequest.project_id.IsEmpty() )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_INVALID_PARAMETER;
        result.error_message = wxS( "Project ID is required" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED, wxEmptyString, false,
                          result.error_message );
        return result;
    }

    PLM_LOCK_INFO currentLock = GetLockInfo( aRequest.project_id );

    if( currentLock.is_locked && !currentLock.locked_by_me && !aRequest.force )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_LOCK_CONFLICT;
        result.error_message = wxString::Format( wxS( "Locked by %s" ), currentLock.lock_owner );
        result.lock_info = currentLock;
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_DENIED, aRequest.project_id, false,
                          result.error_message );
        return result;
    }

    long productId = 0;

    if( !aRequest.project_id.ToLong( &productId ) )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_INVALID_PARAMETER;
        result.error_message = wxS( "Invalid project ID" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED, aRequest.project_id, false,
                          result.error_message );
        return result;
    }

    wxString lockTime = wxDateTime::Now().FormatISOCombined();
    wxString hostName = wxGetHostName();

    nlohmann::json values;
    values["x_kicad_locked_by"] = m_userId;
    values["x_kicad_lock_time"] = lockTime.ToStdString();
    values["x_kicad_lock_comment"] = aRequest.comment.ToStdString();
    values["x_kicad_lock_machine"] = hostName.ToStdString();

    // File-level locking: when file_paths is set, locate matching ir.attachment
    // records and write the lock fields on each. Falls back to project-level
    // locking on product.product when file_paths is empty (the historical
    // behavior). Mixing both is not supported here -- callers ask for one or
    // the other.
    if( !aRequest.file_paths.empty() )
    {
        nlohmann::json attachmentIds = nlohmann::json::array();

        for( const wxString& filePath : aRequest.file_paths )
        {
            wxString fileName = wxFileName( filePath ).GetFullName();

            if( fileName.IsEmpty() )
                continue;

            nlohmann::json domain = nlohmann::json::array( {
                    nlohmann::json::array( { "res_model", "=", "product.product" } ),
                    nlohmann::json::array( { "res_id", "=", static_cast<int>( productId ) } ),
                    nlohmann::json::array( { "name", "=", fileName.ToStdString() } )
            } );

            nlohmann::json fields = nlohmann::json::array( { "id", "x_kicad_locked_by" } );

            ODOO_RPC_RESULT search =
                    m_rpcClient->SearchRead( wxS( "ir.attachment" ), domain, fields, 1 );

            if( !search.success || !search.result.is_array() || search.result.empty() )
                continue;

            const auto& row = search.result[0];

            // Detect a conflicting file-level lock held by another user.
            // x_kicad_locked_by is either false or a [user_id, "name"] tuple.
            if( !aRequest.force && row.contains( "x_kicad_locked_by" )
                && row["x_kicad_locked_by"].is_array() && !row["x_kicad_locked_by"].empty() )
            {
                int      lockedByUid = row["x_kicad_locked_by"][0].get<int>();
                wxString lockedByName = row["x_kicad_locked_by"].size() > 1
                                                && row["x_kicad_locked_by"][1].is_string()
                                                ? wxString::FromUTF8(
                                                          row["x_kicad_locked_by"][1]
                                                                  .get<std::string>() )
                                                : wxString();

                if( lockedByUid != m_userId )
                {
                    result.success = false;
                    result.error_code = PLM_ERROR_CODE::ERR_LOCK_CONFLICT;
                    result.error_message =
                            wxString::Format( wxS( "File %s is locked by %s" ), fileName,
                                              lockedByName );
                    result.lock_info.is_locked = true;
                    result.lock_info.locked_by_me = false;
                    result.lock_info.lock_owner = lockedByName;
                    RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_DENIED, aRequest.project_id,
                                      false, result.error_message );
                    return result;
                }
            }

            attachmentIds.push_back( row.value( "id", 0 ) );
        }

        if( attachmentIds.empty() )
        {
            result.success = false;
            result.error_code = PLM_ERROR_CODE::ERR_FILE_NOT_FOUND;
            result.error_message =
                    wxS( "No ir.attachment records matched the requested file paths" );
            RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED, aRequest.project_id, false,
                              result.error_message );
            return result;
        }

        ODOO_RPC_RESULT attachWrite = m_rpcClient->Write( wxS( "ir.attachment" ), attachmentIds,
                                                          values );

        if( !attachWrite.success )
        {
            result.success = false;
            result.error_code = PLM_ERROR_CODE::ERR_INTERNAL;
            result.error_message = wxS( "Failed to acquire file-level lock on server" );
            RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED, aRequest.project_id, false,
                              result.error_message );
            return result;
        }
    }
    else
    {
        // Project-level lock: write the same fields on the product.product row.
        nlohmann::json ids = nlohmann::json::array();
        ids.push_back( static_cast<int>( productId ) );

        ODOO_RPC_RESULT writeResult = m_rpcClient->Write( wxS( "product.product" ), ids, values );

        if( !writeResult.success )
        {
            result.success = false;
            result.error_code = PLM_ERROR_CODE::ERR_INTERNAL;
            result.error_message = wxS( "Failed to acquire lock on server" );
            RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED, aRequest.project_id, false,
                              result.error_message );
            return result;
        }
    }

    result.success = true;
    result.lock_info.is_locked = true;
    result.lock_info.lock_owner = m_username;
    result.lock_info.locked_by_me = true;
    result.lock_info.lock_timestamp = static_cast<int64_t>( std::time( nullptr ) );
    result.lock_info.lock_comment = aRequest.comment;
    result.lock_info.lock_machine = hostName;

    RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_ACQUIRED, aRequest.project_id, true,
                      aRequest.comment );

    return result;
}


PLM_LOCK_RESULT PLM_ODOO_PLUGIN::Unlock( const PLM_UNLOCK_REQUEST& aRequest )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_LOCK_RESULT result;

    if( !m_isAuthenticated || !m_rpcClient )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_AUTH_REQUIRED;
        result.error_message = wxS( "Not authenticated" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_RELEASED, aRequest.project_id, false,
                          result.error_message );
        return result;
    }

    if( aRequest.project_id.IsEmpty() )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_INVALID_PARAMETER;
        result.error_message = wxS( "Project ID is required" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_RELEASED, wxEmptyString, false,
                          result.error_message );
        return result;
    }

    long productId = 0;

    if( !aRequest.project_id.ToLong( &productId ) )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_INVALID_PARAMETER;
        result.error_message = wxS( "Invalid project ID" );
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_RELEASED, aRequest.project_id, false,
                          result.error_message );
        return result;
    }

    PLM_LOCK_INFO productLock = GetLockInfo( aRequest.project_id );

    // Search for any attachment-level locks under this product. The unlock
    // contract is "release everything you locked here", so we have to find
    // all rows that need clearing before deciding the operation has anything
    // to do. The filter narrows by m_userId unless force=true so one user's
    // unlock cannot release another user's file locks.
    nlohmann::json attachIds = nlohmann::json::array();

    {
        nlohmann::json attachDomain = nlohmann::json::array();
        attachDomain.push_back( nlohmann::json::array( { "res_model", "=", "product.product" } ) );
        attachDomain.push_back( nlohmann::json::array( { "res_id", "=",
                                                          static_cast<int>( productId ) } ) );

        if( aRequest.force )
        {
            attachDomain.push_back(
                    nlohmann::json::array( { "x_kicad_locked_by", "!=", false } ) );
        }
        else
        {
            attachDomain.push_back(
                    nlohmann::json::array( { "x_kicad_locked_by", "=", m_userId } ) );
        }

        ODOO_RPC_RESULT attachSearch = m_rpcClient->SearchRead( wxS( "ir.attachment" ),
                                                                attachDomain,
                                                                nlohmann::json::array( { "id" } ),
                                                                0 );

        if( attachSearch.success && attachSearch.result.is_array() )
        {
            for( const auto& row : attachSearch.result )
                attachIds.push_back( row.value( "id", 0 ) );
        }
    }

    bool productLocked = productLock.is_locked;
    bool productLockedByMe = productLock.locked_by_me;
    bool haveAttachmentLocks = !attachIds.empty();

    if( !productLocked && !haveAttachmentLocks )
    {
        result.success = true;
        result.lock_info = PLM_LOCK_INFO::Unlocked();
        return result;
    }

    if( productLocked && !productLockedByMe && !aRequest.force )
    {
        result.success = false;
        result.error_code = PLM_ERROR_CODE::ERR_LOCK_NOT_HELD;
        result.error_message = wxString::Format( wxS( "Lock is held by %s" ),
                                                  productLock.lock_owner );
        result.lock_info = productLock;
        RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_DENIED, aRequest.project_id, false,
                          result.error_message );
        return result;
    }

    nlohmann::json values;
    values["x_kicad_locked_by"] = false;
    values["x_kicad_lock_time"] = false;
    values["x_kicad_lock_comment"] = false;
    values["x_kicad_lock_machine"] = false;

    // Clear lock fields on product.product when the project row holds a lock.
    if( productLocked )
    {
        nlohmann::json ids = nlohmann::json::array();
        ids.push_back( static_cast<int>( productId ) );

        ODOO_RPC_RESULT writeResult = m_rpcClient->Write( wxS( "product.product" ), ids, values );

        if( !writeResult.success )
        {
            result.success = false;
            result.error_code = PLM_ERROR_CODE::ERR_INTERNAL;
            result.error_message = wxS( "Failed to release lock on server" );
            RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_RELEASED, aRequest.project_id, false,
                              result.error_message );
            return result;
        }
    }

    // Clear matching attachment-level locks. The earlier search already
    // restricted to current-user locks (or all when force=true). For
    // file-level-only locks this is the primary unlock path, so a failure
    // here means the locks are still held and the operation must report
    // failure to the caller.
    if( !attachIds.empty() )
    {
        ODOO_RPC_RESULT attachWrite = m_rpcClient->Write( wxS( "ir.attachment" ), attachIds,
                                                          values );

        if( !attachWrite.success )
        {
            result.success = false;
            result.error_code = PLM_ERROR_CODE::ERR_INTERNAL;
            result.error_message = wxS( "Failed to release file-level lock on server" );
            RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_RELEASED, aRequest.project_id, false,
                              result.error_message );
            return result;
        }
    }

    result.success = true;
    result.lock_info = PLM_LOCK_INFO::Unlocked();

    RecordAuditEvent( PLM_AUDIT_EVENT_TYPE::LOCK_RELEASED, aRequest.project_id, true );

    return result;
}


PLM_LOCK_INFO PLM_ODOO_PLUGIN::GetLockInfo( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_LOCK_INFO info;

    if( !m_isAuthenticated || !m_rpcClient || aProjectId.IsEmpty() )
        return info;

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) )
        return info;

    // Query lock fields from product.product
    nlohmann::json domain = nlohmann::json::array();
    domain.push_back( nlohmann::json::array( { "id", "=", static_cast<int>( productId ) } ) );

    nlohmann::json fields = nlohmann::json::array();
    fields.push_back( "x_kicad_locked_by" );
    fields.push_back( "x_kicad_lock_time" );
    fields.push_back( "x_kicad_lock_comment" );
    fields.push_back( "x_kicad_lock_machine" );

    ODOO_RPC_RESULT result = m_rpcClient->SearchRead( wxS( "product.product" ), domain, fields, 0, 1 );

    if( !result.success || !result.result.is_array() || result.result.empty() )
        return info;

    try
    {
        const auto& product = result.result[0];

        // Check if lock fields exist and have values
        bool hasLockBy = product.contains( "x_kicad_locked_by" ) &&
                         !product["x_kicad_locked_by"].is_null() &&
                         !product["x_kicad_locked_by"].is_boolean();

        if( hasLockBy )
        {
            // x_kicad_locked_by is either false (not locked) or [user_id, "user_name"]
            const auto& lockedBy = product["x_kicad_locked_by"];

            if( lockedBy.is_array() && lockedBy.size() >= 2 )
            {
                info.is_locked = true;
                int lockUserId = lockedBy[0].get<int>();
                info.lock_owner = wxString::FromUTF8( lockedBy[1].get<std::string>() );
                info.locked_by_me = ( lockUserId == m_userId );

                if( product.contains( "x_kicad_lock_time" ) && product["x_kicad_lock_time"].is_string() )
                {
                    wxString timeStr = wxString::FromUTF8( product["x_kicad_lock_time"].get<std::string>() );
                    wxDateTime dt;

                    if( dt.ParseISOCombined( timeStr ) )
                        info.lock_timestamp = dt.GetTicks();
                }

                if( product.contains( "x_kicad_lock_comment" ) && product["x_kicad_lock_comment"].is_string() )
                {
                    info.lock_comment = wxString::FromUTF8( product["x_kicad_lock_comment"].get<std::string>() );
                }

                if( product.contains( "x_kicad_lock_machine" ) && product["x_kicad_lock_machine"].is_string() )
                {
                    info.lock_machine = wxString::FromUTF8( product["x_kicad_lock_machine"].get<std::string>() );
                }
            }
        }
    }
    catch( const nlohmann::json::exception& )
    {
        // Lock fields may not exist on this Odoo instance, return unlocked
    }

    return info;
}


PLM_SCHEMA PLM_ODOO_PLUGIN::GetSchema()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_SCHEMA schema( wxS( "1.0" ) );

    PLM_ATTRIBUTE_SCHEMA name;
    name.attr_id = wxS( "name" );
    name.display_name = wxS( "Product Name" );
    name.description = wxS( "The name of the product" );
    name.type = PLM_ATTR_TYPE::STRING;
    name.required = true;
    name.editable_in_kicad = true;
    name.display_order = 1;
    schema.attributes.push_back( name );

    PLM_ATTRIBUTE_SCHEMA defaultCode;
    defaultCode.attr_id = wxS( "default_code" );
    defaultCode.display_name = wxS( "Internal Reference" );
    defaultCode.description = wxS( "The internal reference code for this product" );
    defaultCode.type = PLM_ATTR_TYPE::STRING;
    defaultCode.required = false;
    defaultCode.editable_in_kicad = true;
    defaultCode.display_order = 2;
    schema.attributes.push_back( defaultCode );

    PLM_ATTRIBUTE_SCHEMA description;
    description.attr_id = wxS( "description" );
    description.display_name = wxS( "Description" );
    description.description = wxS( "Product description" );
    description.type = PLM_ATTR_TYPE::STRING;
    description.required = false;
    description.editable_in_kicad = true;
    description.display_order = 3;
    schema.attributes.push_back( description );

    PLM_ATTRIBUTE_SCHEMA type;
    type.attr_id = wxS( "type" );
    type.display_name = wxS( "Product Type" );
    type.description = wxS( "Type of product in Odoo" );
    type.type = PLM_ATTR_TYPE::ENUM;
    type.required = true;
    type.editable_in_kicad = false;
    type.enum_values = { wxS( "consu" ), wxS( "service" ), wxS( "product" ) };
    type.default_value = wxS( "consu" );
    type.display_order = 4;
    schema.attributes.push_back( type );

    PLM_ATTRIBUTE_SCHEMA active;
    active.attr_id = wxS( "active" );
    active.display_name = wxS( "Active" );
    active.description = wxS( "Whether the product is active" );
    active.type = PLM_ATTR_TYPE::BOOLEAN;
    active.required = false;
    active.editable_in_kicad = true;
    active.default_value = wxS( "true" );
    active.display_order = 5;
    schema.attributes.push_back( active );

    return schema;
}


PLM_ATTRIBUTE_SET PLM_ODOO_PLUGIN::GetAttributes( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_ATTRIBUTE_SET attrs;

    if( !m_isAuthenticated || !m_rpcClient || aProjectId.IsEmpty() )
        return attrs;

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) )
        return attrs;

    nlohmann::json domain = nlohmann::json::array();
    domain.push_back( nlohmann::json::array( { "id", "=", static_cast<int>( productId ) } ) );

    nlohmann::json fields = nlohmann::json::array( {
        "name", "default_code", "description", "type", "active"
    } );

    ODOO_RPC_RESULT result = m_rpcClient->SearchRead( wxS( "product.product" ),
                                                       domain, fields, 1 );

    if( result.success && result.result.is_array() && !result.result.empty() )
    {
        try
        {
            const auto& product = result.result[0];

            if( product.contains( "name" ) && !product["name"].is_null() )
            {
                attrs.values.emplace_back( wxS( "name" ),
                                           wxString::FromUTF8( product["name"].get<std::string>() ) );
            }

            if( product.contains( "default_code" ) && !product["default_code"].is_null() )
            {
                attrs.values.emplace_back( wxS( "default_code" ),
                                           wxString::FromUTF8( product["default_code"].get<std::string>() ) );
            }
            else
            {
                attrs.values.emplace_back( wxS( "default_code" ), wxEmptyString );
            }

            if( product.contains( "description" ) && !product["description"].is_null() )
            {
                attrs.values.emplace_back( wxS( "description" ),
                                           wxString::FromUTF8( product["description"].get<std::string>() ) );
            }
            else
            {
                attrs.values.emplace_back( wxS( "description" ), wxEmptyString );
            }

            if( product.contains( "type" ) && !product["type"].is_null() )
            {
                attrs.values.emplace_back( wxS( "type" ),
                                           wxString::FromUTF8( product["type"].get<std::string>() ) );
            }

            if( product.contains( "active" ) )
            {
                bool active = product["active"].get<bool>();
                attrs.values.emplace_back( wxS( "active" ), active ? wxS( "true" ) : wxS( "false" ) );
            }
        }
        catch( const nlohmann::json::exception& )
        {
            // Malformed JSON response, return partial attributes
        }
    }

    return attrs;
}


bool PLM_ODOO_PLUGIN::SetAttributes( const wxString&          aProjectId,
                                      const PLM_ATTRIBUTE_SET& aAttributes )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient || aProjectId.IsEmpty() )
        return false;

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) )
        return false;

    nlohmann::json values;

    for( const auto& attr : aAttributes.values )
    {
        if( attr.attr_id == wxS( "name" ) )
        {
            values["name"] = attr.value.ToStdString();
        }
        else if( attr.attr_id == wxS( "default_code" ) )
        {
            if( attr.value.IsEmpty() )
                values["default_code"] = false;
            else
                values["default_code"] = attr.value.ToStdString();
        }
        else if( attr.attr_id == wxS( "description" ) )
        {
            if( attr.value.IsEmpty() )
                values["description"] = false;
            else
                values["description"] = attr.value.ToStdString();
        }
        else if( attr.attr_id == wxS( "active" ) )
        {
            values["active"] = ( attr.value == wxS( "true" ) );
        }
    }

    if( values.empty() )
        return true;

    nlohmann::json ids = nlohmann::json::array();
    ids.push_back( static_cast<int>( productId ) );

    ODOO_RPC_RESULT result = m_rpcClient->Write( wxS( "product.product" ), ids, values );

    return result.success;
}


PLM_PERMISSIONS PLM_ODOO_PLUGIN::QueryPermissions( const wxString& aProjectId )
{
    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_PERMISSIONS();

    bool hasManager = false;
    bool hasUser = false;

    for( const wxString& group : m_session.user_info.groups )
    {
        if( group.Contains( wxS( "PLM / Manager" ) ) || group.Contains( wxS( "Administration" ) ) )
            hasManager = true;
        else if( group.Contains( wxS( "PLM" ) ) )
            hasUser = true;
    }

    if( hasManager )
        return PLM_PERMISSIONS::FullAccess();

    if( hasUser )
        return PLM_PERMISSIONS::StandardEdit();

    return PLM_PERMISSIONS::ReadOnly();
}


wxString PLM_ODOO_PLUGIN::GetUserDisplayName( const wxString& aUserId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    auto it = m_userDisplayNames.find( aUserId );

    if( it != m_userDisplayNames.end() )
        return it->second;

    return aUserId;
}


void PLM_ODOO_PLUGIN::ParseServerUrl( const wxString& aServerUrl )
{
    m_serverUrl.Clear();
    m_database.Clear();

    if( aServerUrl.IsEmpty() )
        return;

    wxURI uri( aServerUrl );
    wxString path = uri.GetPath();

    wxString scheme = uri.GetScheme();
    wxString server = uri.GetServer();

    if( scheme.IsEmpty() )
        scheme = wxS( "https" );

    if( server.IsEmpty() )
    {
        m_serverUrl = aServerUrl;
        return;
    }

    m_serverUrl = scheme + wxS( "://" ) + server;

    if( uri.HasPort() )
        m_serverUrl += wxS( ":" ) + uri.GetPort();

    if( !path.IsEmpty() && path != wxS( "/" ) )
    {
        if( path.StartsWith( wxS( "/" ) ) )
            path = path.Mid( 1 );

        int slashPos = path.Find( wxS( "/" ) );

        if( slashPos != wxNOT_FOUND )
            m_database = path.Left( slashPos );
        else
            m_database = path;
    }
}


void PLM_ODOO_PLUGIN::PopulateFileInfoFromProduct( PLM_FILE_INFO& aFile, const nlohmann::json& aProduct )
{
    try
    {
        aFile.id = wxString::Format( wxS( "%d" ), aProduct["id"].get<int>() );

        if( aProduct.contains( "default_code" ) && !aProduct["default_code"].is_null() )
        {
            wxString code = wxString::FromUTF8( aProduct["default_code"].get<std::string>() );

            if( !code.IsEmpty() )
                aFile.name = code + wxS( " - " ) +
                             wxString::FromUTF8( aProduct["name"].get<std::string>() );
            else
                aFile.name = wxString::FromUTF8( aProduct["name"].get<std::string>() );
        }
        else
        {
            aFile.name = wxString::FromUTF8( aProduct["name"].get<std::string>() );
        }

        aFile.revision = wxS( "1" );

        wxString productState = wxS( "draft" );

        if( aProduct.contains( "state" ) && aProduct["state"].is_string() )
            productState = wxString::FromUTF8( aProduct["state"].get<std::string>() );

        aFile.native_state = productState;

        if( productState == wxS( "released" ) || productState == wxS( "obsolete" ) )
            aFile.state_category = PLM_STATE_CATEGORY::RELEASED;
        else
            aFile.state_category = PLM_STATE_CATEGORY::EDITABLE;

        if( aProduct.contains( "categ_id" ) && aProduct["categ_id"].is_array() &&
            aProduct["categ_id"].size() >= 1 )
        {
            aFile.folder_id = wxString::Format( wxS( "%d" ), aProduct["categ_id"][0].get<int>() );
        }

        if( aProduct.contains( "description" ) && !aProduct["description"].is_null() )
            aFile.description = wxString::FromUTF8( aProduct["description"].get<std::string>() );

        if( aProduct.contains( "write_date" ) && aProduct["write_date"].is_string() )
        {
            wxString dateStr = wxString::FromUTF8( aProduct["write_date"].get<std::string>() );
            wxDateTime dt;

            if( dt.ParseISOCombined( dateStr, ' ' ) )
                aFile.modified_timestamp = dt.GetTicks();
        }
    }
    catch( const nlohmann::json::exception& )
    {
        // Malformed JSON, leave aFile partially populated
    }
}


bool PLM_ODOO_PLUGIN::DownloadProductAttachments( int aProductId, const wxString& aLocalPath,
                                                   wxString* aWarnings )
{
    if( !m_rpcClient )
        return false;

    ODOO_RPC_RESULT attachments = m_rpcClient->GetAttachments( wxS( "product.product" ),
                                                                 aProductId );

    if( !attachments.success || !attachments.result.is_array() )
        return false;

    if( !wxDirExists( aLocalPath ) )
    {
        if( !wxFileName::Mkdir( aLocalPath, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL ) )
            return false;
    }

    int  totalAttachments = 0;
    int  successCount = 0;
    wxString failedFiles;

    try
    {
        for( const auto& attachment : attachments.result )
        {
            if( !attachment.contains( "id" ) || !attachment.contains( "name" ) )
                continue;

            totalAttachments++;

            int attachmentId = attachment["id"].get<int>();
            wxString filename = wxString::FromUTF8( attachment["name"].get<std::string>() );

            int expectedSize = 0;

            if( attachment.contains( "file_size" ) && attachment["file_size"].is_number() )
                expectedSize = attachment["file_size"].get<int>();

            ODOO_RPC_RESULT data = m_rpcClient->DownloadAttachment( attachmentId );

            if( !data.success || !data.result.contains( "datas" ) || data.result["datas"].is_null() )
            {
                if( !failedFiles.IsEmpty() )
                    failedFiles += wxS( ", " );

                failedFiles += filename;
                continue;
            }

            wxString base64Data = wxString::FromUTF8( data.result["datas"].get<std::string>() );
            wxMemoryBuffer binaryData = DecodeBase64( base64Data );

            if( binaryData.GetDataLen() == 0 )
            {
                if( !failedFiles.IsEmpty() )
                    failedFiles += wxS( ", " );

                failedFiles += filename;
                continue;
            }

            // Verify decoded size matches expected size when the server reports a positive value
            if( expectedSize > 0
                && static_cast<int>( binaryData.GetDataLen() ) != expectedSize )
            {
                if( !failedFiles.IsEmpty() )
                    failedFiles += wxS( ", " );

                failedFiles += filename;
                continue;
            }

            wxString filePath = aLocalPath + wxFileName::GetPathSeparator() + filename;
            wxFile file( filePath, wxFile::write );

            if( !file.IsOpened() )
            {
                if( !failedFiles.IsEmpty() )
                    failedFiles += wxS( ", " );

                failedFiles += filename;
                continue;
            }

            file.Write( binaryData.GetData(), binaryData.GetDataLen() );
            file.Close();
            successCount++;
        }
    }
    catch( const nlohmann::json::exception& )
    {
        return false;
    }

    if( totalAttachments > 0 && successCount == 0 )
        return false;

    if( !failedFiles.IsEmpty() && aWarnings )
    {
        *aWarnings = wxString::Format( wxS( "Some attachments could not be downloaded: %s" ),
                                       failedFiles );
    }

    return true;
}


bool PLM_ODOO_PLUGIN::UploadProjectFiles( int aProductId, const wxString& aLocalPath,
                                           wxString* aWarnings )
{
    if( !m_rpcClient || !wxDirExists( aLocalPath ) )
        return false;

    wxDir dir( aLocalPath );

    if( !dir.IsOpened() )
        return false;

    int      totalFiles = 0;
    int      successCount = 0;
    wxString failedFiles;
    wxString filename;
    bool     cont = dir.GetFirst( &filename, wxEmptyString, wxDIR_FILES );

    while( cont )
    {
        wxString filePath = aLocalPath + wxFileName::GetPathSeparator() + filename;
        wxFile   file( filePath, wxFile::read );

        if( file.IsOpened() )
        {
            totalFiles++;
            wxFileOffset   fileSize = file.Length();
            wxMemoryBuffer buffer( fileSize );
            file.Read( buffer.GetData(), fileSize );
            buffer.SetDataLen( fileSize );
            file.Close();

            wxString base64Data = EncodeBase64( buffer );

            ODOO_RPC_RESULT uploadResult = m_rpcClient->UploadAttachment(
                    wxS( "product.product" ), aProductId, filename, base64Data );

            if( uploadResult.success )
            {
                successCount++;
            }
            else
            {
                if( !failedFiles.IsEmpty() )
                    failedFiles += wxS( ", " );

                failedFiles += filename;
            }
        }

        cont = dir.GetNext( &filename );
    }

    if( totalFiles == 0 )
        return false;

    if( successCount == 0 )
        return false;

    if( !failedFiles.IsEmpty() && aWarnings )
    {
        *aWarnings = wxString::Format( wxS( "Some files could not be uploaded: %s" ), failedFiles );
    }

    return true;
}


wxMemoryBuffer PLM_ODOO_PLUGIN::DecodeBase64( const wxString& aBase64 )
{
    return wxBase64Decode( aBase64 );
}


wxString PLM_ODOO_PLUGIN::EncodeBase64( const wxMemoryBuffer& aData )
{
    return wxBase64Encode( aData.GetData(), aData.GetDataLen() );
}


bool PLM_ODOO_PLUGIN::WriteCheckoutMetadata( const wxString& aLocalPath,
                                               const wxString& aProjectId,
                                               const wxString& aRevision )
{
    wxString metadataPath = aLocalPath + wxFileName::GetPathSeparator() + ODOO_PLM_METADATA_FILE;

    nlohmann::json metadata;
    metadata["plugin"] = "odoo-plm";
    metadata["server"] = m_serverUrl.ToStdString();
    metadata["database"] = m_database.ToStdString();
    metadata["product_id"] = aProjectId.ToStdString();
    metadata["revision"] = aRevision.ToStdString();
    metadata["user"] = m_username.ToStdString();
    metadata["checkout_time"] = wxDateTime::Now().FormatISOCombined().ToStdString();

    wxFile file( metadataPath, wxFile::write );

    if( !file.IsOpened() )
        return false;

    std::string content = metadata.dump( 2 );
    file.Write( content.c_str(), content.size() );
    file.Close();

    return true;
}


bool PLM_ODOO_PLUGIN::ReadCheckoutMetadata( const wxString& aLocalPath,
                                              wxString& aProjectId,
                                              wxString& aRevision )
{
    wxString metadataPath = aLocalPath + wxFileName::GetPathSeparator() + ODOO_PLM_METADATA_FILE;

    if( !wxFileExists( metadataPath ) )
        return false;

    wxFile file( metadataPath, wxFile::read );

    if( !file.IsOpened() )
        return false;

    wxString content;
    file.ReadAll( &content );
    file.Close();

    try
    {
        nlohmann::json metadata = nlohmann::json::parse( content.ToStdString() );

        if( metadata.contains( "product_id" ) )
            aProjectId = wxString::FromUTF8( metadata["product_id"].get<std::string>() );

        if( metadata.contains( "revision" ) )
            aRevision = wxString::FromUTF8( metadata["revision"].get<std::string>() );

        return true;
    }
    catch( ... )
    {
        return false;
    }
}


wxString PLM_ODOO_PLUGIN::GetOdooWebUrl( const wxString& aModel, int aRecordId ) const
{
    return wxString::Format( wxS( "%s/web#id=%d&model=%s&view_type=form" ),
                             m_serverUrl, aRecordId, aModel );
}


bool PLM_ODOO_PLUGIN::SubscribeNotifications( const wxString&                aProjectId,
                                               PLM_NOTIFICATION_LISTENER*     aListener,
                                               const PLM_NOTIFICATION_CONFIG& aConfig )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient || !aListener )
        return false;

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) )
        return false;

    NotificationSubscription sub;
    sub.listener = aListener;
    sub.config = aConfig;

    PLM_FILE_INFO info = GetFileInfo( aProjectId );
    sub.last_write_date = wxString::Format( wxS( "%lld" ), info.modified_timestamp );
    sub.last_lock_state = info.IsLocked();

    m_subscriptions[aProjectId] = sub;

    if( !m_pollTimer )
    {
        m_pollTimer = std::make_unique<wxTimer>();
        m_pollTimer->Bind( wxEVT_TIMER, &PLM_ODOO_PLUGIN::OnPollTimer, this );
        m_pollTimer->Start( POLL_INTERVAL_MS );
    }

    return true;
}


bool PLM_ODOO_PLUGIN::UnsubscribeNotifications( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    auto it = m_subscriptions.find( aProjectId );

    if( it == m_subscriptions.end() )
        return false;

    m_subscriptions.erase( it );

    if( m_subscriptions.empty() && m_pollTimer )
    {
        m_pollTimer->Stop();
        m_pollTimer.reset();
    }

    return true;
}


void PLM_ODOO_PLUGIN::UnsubscribeAllNotifications()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    m_subscriptions.clear();

    if( m_pollTimer )
    {
        m_pollTimer->Stop();
        m_pollTimer.reset();
    }
}


void PLM_ODOO_PLUGIN::OnPollTimer( wxTimerEvent& aEvent )
{
    PollForChanges();
}


void PLM_ODOO_PLUGIN::PollForChanges()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient || m_subscriptions.empty() )
        return;

    for( auto& [projectId, sub] : m_subscriptions )
    {
        PLM_FILE_INFO info = GetFileInfo( projectId );
        wxString modifiedStr = wxString::Format( wxS( "%lld" ), info.modified_timestamp );

        if( modifiedStr != sub.last_write_date )
        {
            PLM_NOTIFICATION notification = PLM_NOTIFICATION::RevisionCreated(
                    projectId, info.revision, info.lock_owner, info.lock_owner );
            sub.listener->OnNotification( notification );
            sub.last_write_date = modifiedStr;
        }

        bool currentLocked = info.IsLocked();

        if( currentLocked != sub.last_lock_state )
        {
            PLM_NOTIFICATION notification;

            if( currentLocked )
            {
                notification = PLM_NOTIFICATION::LockAcquired(
                        projectId, info.lock_owner, info.lock_owner );
            }
            else
            {
                notification = PLM_NOTIFICATION::LockReleased(
                        projectId, wxEmptyString, wxEmptyString );
            }

            sub.listener->OnNotification( notification );
            sub.last_lock_state = currentLocked;
        }
    }
}


std::vector<PLM_ECO> PLM_ODOO_PLUGIN::GetActiveECOs()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_ECO> ecos;

    if( !m_isAuthenticated || !m_rpcClient || !m_ecoTypeProbe.value_or( false ) )
        return ecos;

    nlohmann::json domain = nlohmann::json::array( {
            nlohmann::json::array( { "state", "in",
                                     nlohmann::json::array( { "draft", "confirmed", "progress" } ) } )
    } );

    nlohmann::json fields = nlohmann::json::array(
            { "id", "name", "description", "state", "type_id", "date", "create_uid",
              "product_tmpl_id" } );

    ODOO_RPC_RESULT searchResult = m_rpcClient->SearchRead(
            wxS( "mrp.eco" ), domain, fields, 100, 0, wxS( "date desc" ) );

    if( !searchResult.success || !searchResult.result.is_array() )
        return ecos;

    try
    {
        for( const auto& record : searchResult.result )
        {
            PLM_ECO eco;
            eco.eco_id = wxString::Format( wxS( "%d" ), record.value( "id", 0 ) );
            eco.eco_number = wxString::FromUTF8( record.value( "name", std::string() ) );
            eco.title = eco.eco_number;
            eco.description = wxString::FromUTF8( record.value( "description", std::string() ) );

            wxString state = wxString::FromUTF8( record.value( "state", std::string() ) );

            if( state == wxS( "draft" ) )
                eco.status = PLM_ECO_STATUS::DRAFT;
            else if( state == wxS( "confirmed" ) )
                eco.status = PLM_ECO_STATUS::PENDING_REVIEW;
            else if( state == wxS( "progress" ) )
                eco.status = PLM_ECO_STATUS::IN_PROGRESS;
            else if( state == wxS( "done" ) )
                eco.status = PLM_ECO_STATUS::COMPLETED;
            else if( state == wxS( "cancel" ) )
                eco.status = PLM_ECO_STATUS::CANCELLED;

            if( record.contains( "create_uid" ) && record["create_uid"].is_array() &&
                record["create_uid"].size() >= 2 )
            {
                eco.owner_id = wxString::Format( wxS( "%d" ), record["create_uid"][0].get<int>() );
                eco.owner_name = wxString::FromUTF8( record["create_uid"][1].get<std::string>() );
            }

            if( record.contains( "product_tmpl_id" ) && record["product_tmpl_id"].is_array()
                && record["product_tmpl_id"].size() >= 1 )
            {
                wxString productId = wxString::Format(
                        wxS( "%d" ), record["product_tmpl_id"][0].get<int>() );
                eco.affected_projects.push_back( productId );
            }

            eco.required_fields.push_back( PLM_ECO_FIELD::Text( wxS( "reason" ), wxS( "Change Reason" ) ) );
            eco.required_fields.push_back(
                    PLM_ECO_FIELD::Multiline( wxS( "description" ), wxS( "Change Description" ) ) );

            ecos.push_back( eco );
        }
    }
    catch( const nlohmann::json::exception& )
    {
    }

    return ecos;
}


std::vector<PLM_ECO_FIELD> PLM_ODOO_PLUGIN::GetECORequiredFields( const wxString& aEcoId )
{
    std::vector<PLM_ECO_FIELD> fields;

    // ECO workflows depend on mrp.eco / mrp.eco.type from Odoo Enterprise.
    // On community OdooPLM the probe in Authenticate() reports false, and
    // returning empty here lets dialogs skip the ECO-required section
    // entirely instead of asking for fields that have nowhere to land.
    if( !m_ecoTypeProbe.value_or( false ) )
        return fields;

    fields.push_back( PLM_ECO_FIELD::Text( wxS( "reason" ), wxS( "Change Reason" ) ) );
    fields.push_back( PLM_ECO_FIELD::Multiline( wxS( "description" ), wxS( "Change Description" ) ) );
    fields.push_back( PLM_ECO_FIELD::Attachment( wxS( "attachment" ), wxS( "Supporting Documents" ) ) );

    return fields;
}


PLM_ECO_VALIDATION_RESULT PLM_ODOO_PLUGIN::ValidateECOCheckin( const wxString&            aEcoId,
                                                                 const PLM_ECO_FIELD_VALUES& aFields )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_ECO_VALIDATION_RESULT result;

    if( aEcoId.IsEmpty() )
    {
        result.AddError( wxS( "ECO ID is required" ) );
        return result;
    }

    auto reasonIt = aFields.find( wxS( "reason" ) );

    if( reasonIt == aFields.end() || reasonIt->second.IsEmpty() )
    {
        result.AddError( wxS( "Change reason is required" ) );
        return result;
    }

    auto descIt = aFields.find( wxS( "description" ) );

    if( descIt == aFields.end() || descIt->second.IsEmpty() )
    {
        result.AddError( wxS( "Change description is required" ) );
        return result;
    }

    result.eco_id = aEcoId;
    return result;
}


//-------------------------------------------------------------------------
// ECO Extended Interface
//-------------------------------------------------------------------------

PLM_ECO_CREATE_RESULT PLM_ODOO_PLUGIN::CreateECO( const PLM_ECO_CREATE_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_ECO_CREATE_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                                wxS( "Not authenticated" ) );

    if( !m_ecoTypeProbe.value_or( false ) )
        return PLM_ECO_CREATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                                wxS( "ECO workflow requires Odoo Enterprise (mrp_plm)" ) );

    if( aOptions.title.IsEmpty() )
        return PLM_ECO_CREATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                                wxS( "ECO title is required" ) );

    try
    {
        nlohmann::json vals;
        vals["name"] = aOptions.title.ToStdString();

        if( !aOptions.description.IsEmpty() )
            vals["note"] = aOptions.description.ToStdString();

        // Map ECO type to Odoo type_id by searching for matching ECO type
        wxString typeFilter = aOptions.type == PLM_ECO_TYPE::ECO ? wxS( "ECO" ) : wxS( "ECR" );
        nlohmann::json typeDomain = nlohmann::json::array(
                { nlohmann::json::array( { "name", "ilike", typeFilter.ToStdString() } ) } );
        nlohmann::json typeFields = nlohmann::json::array( { "id" } );

        ODOO_RPC_RESULT typeResult = m_rpcClient->SearchRead(
                wxS( "mrp.eco.type" ), typeDomain, typeFields, 1 );

        if( typeResult.success && typeResult.result.is_array() && !typeResult.result.empty() )
            vals["type_id"] = typeResult.result[0]["id"].get<int>();

        // Set affected product if specified
        if( !aOptions.affected_item_ids.empty() )
        {
            long tmplId = 0;

            if( aOptions.affected_item_ids[0].ToLong( &tmplId ) )
                vals["product_tmpl_id"] = static_cast<int>( tmplId );
        }

        ODOO_RPC_RESULT createResult = m_rpcClient->Create( wxS( "mrp.eco" ), vals );

        if( !createResult.success )
        {
            return PLM_ECO_CREATE_RESULT::Failure( OdooErrorMapper::FromRpcResult( createResult ),
                                                    OdooErrorMapper::FormatErrorMessage( createResult ) );
        }

        int newId = createResult.result.get<int>();

        PLM_ECO eco;
        eco.eco_id = wxString::Format( wxS( "%d" ), newId );
        eco.eco_number = aOptions.title;
        eco.title = aOptions.title;
        eco.description = aOptions.description;
        eco.status = PLM_ECO_STATUS::DRAFT;
        eco.type = aOptions.type;
        eco.owner_id = wxString::Format( wxS( "%d" ), m_userId );
        eco.owner_name = m_username;

        return PLM_ECO_CREATE_RESULT::Success( eco );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_ECO_CREATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                                wxS( "Invalid server response" ) );
    }
}


PLM_ECO_UPDATE_RESULT PLM_ODOO_PLUGIN::GetECO( const wxString& aEcoId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                                wxS( "Not authenticated" ) );

    if( !m_ecoTypeProbe.value_or( false ) )
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                                wxS( "ECO workflow requires Odoo Enterprise (mrp_plm)" ) );

    if( aEcoId.IsEmpty() )
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                                wxS( "ECO ID is required" ) );

    long ecoIdInt = 0;

    if( !aEcoId.ToLong( &ecoIdInt ) )
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                                wxS( "Invalid ECO ID" ) );

    try
    {
        nlohmann::json domain = nlohmann::json::array(
                { nlohmann::json::array( { "id", "=", static_cast<int>( ecoIdInt ) } ) } );

        nlohmann::json fields = nlohmann::json::array(
                { "id", "name", "description", "state", "type_id", "stage_id",
                  "date", "create_uid", "product_tmpl_id", "note",
                  "bom_change_ids", "create_date", "write_date" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "mrp.eco" ), domain, fields, 1 );

        if( !result.success || !result.result.is_array() || result.result.empty() )
        {
            return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_FILE_NOT_FOUND,
                                                    wxS( "ECO not found" ) );
        }

        const auto& record = result.result[0];

        PLM_ECO eco;
        eco.eco_id = wxString::Format( wxS( "%d" ), record.value( "id", 0 ) );
        eco.eco_number = wxString::FromUTF8( record.value( "name", std::string() ) );
        eco.title = eco.eco_number;
        eco.description = wxString::FromUTF8( record.value( "note", std::string() ) );

        if( eco.description.IsEmpty() )
            eco.description = wxString::FromUTF8( record.value( "description", std::string() ) );

        wxString state = wxString::FromUTF8( record.value( "state", std::string() ) );

        if( state == wxS( "draft" ) )
            eco.status = PLM_ECO_STATUS::DRAFT;
        else if( state == wxS( "confirmed" ) )
            eco.status = PLM_ECO_STATUS::PENDING_REVIEW;
        else if( state == wxS( "progress" ) )
            eco.status = PLM_ECO_STATUS::IN_PROGRESS;
        else if( state == wxS( "done" ) )
            eco.status = PLM_ECO_STATUS::COMPLETED;
        else if( state == wxS( "cancel" ) )
            eco.status = PLM_ECO_STATUS::CANCELLED;

        if( record.contains( "type_id" ) && record["type_id"].is_array()
            && record["type_id"].size() >= 2 )
        {
            wxString typeName = wxString::FromUTF8(
                    record["type_id"][1].get<std::string>() );

            eco.type = typeName.Contains( wxS( "ECO" ) ) ? PLM_ECO_TYPE::ECO : PLM_ECO_TYPE::ECR;
        }

        if( record.contains( "create_uid" ) && record["create_uid"].is_array()
            && record["create_uid"].size() >= 2 )
        {
            eco.owner_id = wxString::Format( wxS( "%d" ),
                    record["create_uid"][0].get<int>() );
            eco.owner_name = wxString::FromUTF8(
                    record["create_uid"][1].get<std::string>() );
        }

        if( record.contains( "product_tmpl_id" ) && record["product_tmpl_id"].is_array()
            && record["product_tmpl_id"].size() >= 1 )
        {
            eco.affected_projects.push_back( wxString::Format(
                    wxS( "%d" ), record["product_tmpl_id"][0].get<int>() ) );
        }

        if( record.contains( "create_date" ) && record["create_date"].is_string() )
        {
            wxDateTime dt;

            if( dt.ParseISOCombined( wxString::FromUTF8(
                    record["create_date"].get<std::string>() ) ) )
            {
                eco.created_at = dt.GetTicks();
            }
        }

        if( record.contains( "write_date" ) && record["write_date"].is_string() )
        {
            wxDateTime dt;

            if( dt.ParseISOCombined( wxString::FromUTF8(
                    record["write_date"].get<std::string>() ) ) )
            {
                eco.updated_at = dt.GetTicks();
            }
        }

        return PLM_ECO_UPDATE_RESULT::Success( eco );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                                wxS( "Invalid server response" ) );
    }
}


PLM_ECO_UPDATE_RESULT PLM_ODOO_PLUGIN::UpdateECOStatus(
        const PLM_ECO_STATUS_TRANSITION& aTransition )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                                wxS( "Not authenticated" ) );

    if( !m_ecoTypeProbe.value_or( false ) )
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                                wxS( "ECO workflow requires Odoo Enterprise (mrp_plm)" ) );

    if( aTransition.eco_id.IsEmpty() )
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                                wxS( "ECO ID is required" ) );

    long ecoIdInt = 0;

    if( !aTransition.eco_id.ToLong( &ecoIdInt ) )
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                                wxS( "Invalid ECO ID" ) );

    try
    {
        ODOO_RPC_RESULT rpcResult;

        switch( aTransition.target_status )
        {
        case PLM_ECO_STATUS::APPROVED:
            rpcResult = m_rpcClient->CallMethod( wxS( "mrp.eco" ), wxS( "action_approve" ),
                    nlohmann::json::array( { nlohmann::json::array( { static_cast<int>( ecoIdInt ) } ) } ) );
            break;

        case PLM_ECO_STATUS::CANCELLED:
        {
            nlohmann::json ids = nlohmann::json::array( { static_cast<int>( ecoIdInt ) } );
            nlohmann::json vals;
            vals["state"] = "cancel";
            rpcResult = m_rpcClient->Write( wxS( "mrp.eco" ), ids, vals );
            break;
        }

        case PLM_ECO_STATUS::IN_PROGRESS:
        {
            nlohmann::json ids = nlohmann::json::array( { static_cast<int>( ecoIdInt ) } );
            nlohmann::json vals;
            vals["state"] = "progress";
            rpcResult = m_rpcClient->Write( wxS( "mrp.eco" ), ids, vals );
            break;
        }

        case PLM_ECO_STATUS::COMPLETED:
        {
            nlohmann::json ids = nlohmann::json::array( { static_cast<int>( ecoIdInt ) } );
            nlohmann::json vals;
            vals["state"] = "done";
            rpcResult = m_rpcClient->Write( wxS( "mrp.eco" ), ids, vals );
            break;
        }

        case PLM_ECO_STATUS::PENDING_REVIEW:
        {
            nlohmann::json ids = nlohmann::json::array( { static_cast<int>( ecoIdInt ) } );
            nlohmann::json vals;
            vals["state"] = "confirmed";
            rpcResult = m_rpcClient->Write( wxS( "mrp.eco" ), ids, vals );
            break;
        }

        default:
            return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                                    wxS( "Unsupported status transition" ) );
        }

        if( !rpcResult.success )
        {
            return PLM_ECO_UPDATE_RESULT::Failure( OdooErrorMapper::FromRpcResult( rpcResult ),
                                                    OdooErrorMapper::FormatErrorMessage( rpcResult ) );
        }

        // Post transition comment if provided
        if( !aTransition.comment.IsEmpty() )
        {
            nlohmann::json msgArgs = nlohmann::json::array(
                    { static_cast<int>( ecoIdInt ) } );
            nlohmann::json msgKwargs;
            msgKwargs["body"] = aTransition.comment.ToStdString();
            msgKwargs["message_type"] = "comment";

            m_rpcClient->CallMethod( wxS( "mrp.eco" ), wxS( "message_post" ),
                                     msgArgs, msgKwargs );
        }

        // Re-read the ECO to return its updated state
        return GetECO( aTransition.eco_id );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                                wxS( "Invalid server response" ) );
    }
}


PLM_ECO_UPDATE_RESULT PLM_ODOO_PLUGIN::UpdateECOItems(
        const wxString& aEcoId, const std::vector<PLM_ECO_ITEM>& aItems )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                                wxS( "Not authenticated" ) );

    if( !m_ecoTypeProbe.value_or( false ) )
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                                wxS( "ECO workflow requires Odoo Enterprise (mrp_plm)" ) );

    if( aEcoId.IsEmpty() )
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                                wxS( "ECO ID is required" ) );

    long ecoIdInt = 0;

    if( !aEcoId.ToLong( &ecoIdInt ) )
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                                wxS( "Invalid ECO ID" ) );

    try
    {
        // Write affected items to the ECO's note field as structured text
        wxString itemsText;

        for( const auto& item : aItems )
        {
            if( !itemsText.IsEmpty() )
                itemsText += wxS( "\n" );

            itemsText += wxString::Format( wxS( "- %s (%s)" ),
                    item.item_name, item.item_type );

            if( !item.notes.IsEmpty() )
                itemsText += wxS( " " ) + item.notes;
        }

        nlohmann::json ids = nlohmann::json::array( { static_cast<int>( ecoIdInt ) } );
        nlohmann::json vals;
        vals["note"] = itemsText.ToStdString();

        ODOO_RPC_RESULT writeResult = m_rpcClient->Write( wxS( "mrp.eco" ), ids, vals );

        if( !writeResult.success )
        {
            return PLM_ECO_UPDATE_RESULT::Failure( OdooErrorMapper::FromRpcResult( writeResult ),
                                                    OdooErrorMapper::FormatErrorMessage( writeResult ) );
        }

        return GetECO( aEcoId );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                                wxS( "Invalid server response" ) );
    }
}


PLM_ECO_UPDATE_RESULT PLM_ODOO_PLUGIN::UpdateECOImpact( const wxString&       aEcoId,
                                                          const PLM_ECO_IMPACT& aImpact )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                                wxS( "Not authenticated" ) );

    if( !m_ecoTypeProbe.value_or( false ) )
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                                wxS( "ECO workflow requires Odoo Enterprise (mrp_plm)" ) );

    if( aEcoId.IsEmpty() )
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                                wxS( "ECO ID is required" ) );

    long ecoIdInt = 0;

    if( !aEcoId.ToLong( &ecoIdInt ) )
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                                wxS( "Invalid ECO ID" ) );

    try
    {
        // Post impact assessment as a chatter message since Odoo lacks dedicated impact fields
        wxString impactHtml = wxS( "<p><strong>Impact Assessment</strong></p><ul>" );

        if( !aImpact.cost_impact.IsEmpty() )
            impactHtml += wxS( "<li>Cost: " ) + aImpact.cost_impact + wxS( "</li>" );

        if( !aImpact.schedule_impact.IsEmpty() )
            impactHtml += wxS( "<li>Schedule: " ) + aImpact.schedule_impact + wxS( "</li>" );

        if( !aImpact.manufacturing_impact.IsEmpty() )
            impactHtml += wxS( "<li>Manufacturing: " ) + aImpact.manufacturing_impact + wxS( "</li>" );

        if( !aImpact.regulatory_impact.IsEmpty() )
            impactHtml += wxS( "<li>Regulatory: " ) + aImpact.regulatory_impact + wxS( "</li>" );

        if( !aImpact.supply_chain_impact.IsEmpty() )
            impactHtml += wxS( "<li>Supply Chain: " ) + aImpact.supply_chain_impact + wxS( "</li>" );

        impactHtml += wxS( "</ul>" );

        nlohmann::json msgArgs = nlohmann::json::array( { static_cast<int>( ecoIdInt ) } );
        nlohmann::json msgKwargs;
        msgKwargs["body"] = impactHtml.ToStdString();
        msgKwargs["message_type"] = "comment";

        ODOO_RPC_RESULT msgResult = m_rpcClient->CallMethod( wxS( "mrp.eco" ),
                wxS( "message_post" ), msgArgs, msgKwargs );

        if( !msgResult.success )
        {
            return PLM_ECO_UPDATE_RESULT::Failure( OdooErrorMapper::FromRpcResult( msgResult ),
                                                    OdooErrorMapper::FormatErrorMessage( msgResult ) );
        }

        return GetECO( aEcoId );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_ECO_UPDATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                                wxS( "Invalid server response" ) );
    }
}


std::vector<PLM_ECO_STATUS_TRANSITION> PLM_ODOO_PLUGIN::GetECOHistory( const wxString& aEcoId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_ECO_STATUS_TRANSITION> history;

    if( !m_isAuthenticated || !m_rpcClient || aEcoId.IsEmpty()
        || !m_ecoTypeProbe.value_or( false ) )
    {
        return history;
    }

    long ecoIdInt = 0;

    if( !aEcoId.ToLong( &ecoIdInt ) )
        return history;

    try
    {
        nlohmann::json domain = nlohmann::json::array( {
            nlohmann::json::array( { "res_model", "=", "mrp.eco" } ),
            nlohmann::json::array( { "res_id", "=", static_cast<int>( ecoIdInt ) } ),
            nlohmann::json::array( { "tracking_value_ids", "!=", false } )
        } );

        nlohmann::json fields = nlohmann::json::array(
                { "id", "date", "body", "author_id", "tracking_value_ids" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "mail.message" ), domain, fields, 50, 0, wxS( "date desc" ) );

        if( !result.success || !result.result.is_array() )
            return history;

        for( const auto& msg : result.result )
        {
            PLM_ECO_STATUS_TRANSITION transition;
            transition.eco_id = aEcoId;

            wxString body = wxString::FromUTF8( msg.value( "body", std::string() ) );
            transition.comment = body;

            if( msg.contains( "author_id" ) && msg["author_id"].is_array()
                && msg["author_id"].size() >= 2 )
            {
                transition.reviewer_id = wxString::Format( wxS( "%d" ),
                        msg["author_id"][0].get<int>() );
            }

            history.push_back( transition );
        }
    }
    catch( const nlohmann::json::exception& )
    {
    }

    return history;
}


PLM_ECO_CREATE_RESULT PLM_ODOO_PLUGIN::ConvertECRToECO( const wxString& aEcrId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_ECO_CREATE_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                                wxS( "Not authenticated" ) );

    if( !m_ecoTypeProbe.value_or( false ) )
        return PLM_ECO_CREATE_RESULT::Failure( PLM_ERROR_CODE::ERR_NOT_SUPPORTED,
                                                wxS( "ECO workflow requires Odoo Enterprise (mrp_plm)" ) );

    if( aEcrId.IsEmpty() )
        return PLM_ECO_CREATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                                wxS( "ECR ID is required" ) );

    long ecrIdInt = 0;

    if( !aEcrId.ToLong( &ecrIdInt ) )
        return PLM_ECO_CREATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                                wxS( "Invalid ECR ID" ) );

    try
    {
        // Find the ECO type record to switch to
        nlohmann::json typeDomain = nlohmann::json::array(
                { nlohmann::json::array( { "name", "ilike", "ECO" } ) } );
        nlohmann::json typeFields = nlohmann::json::array( { "id" } );

        ODOO_RPC_RESULT typeResult = m_rpcClient->SearchRead(
                wxS( "mrp.eco.type" ), typeDomain, typeFields, 1 );

        if( !typeResult.success || !typeResult.result.is_array() || typeResult.result.empty() )
        {
            return PLM_ECO_CREATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                                    wxS( "ECO type not found in Odoo configuration" ) );
        }

        int ecoTypeId = typeResult.result[0]["id"].get<int>();

        nlohmann::json ids = nlohmann::json::array( { static_cast<int>( ecrIdInt ) } );
        nlohmann::json vals;
        vals["type_id"] = ecoTypeId;

        ODOO_RPC_RESULT writeResult = m_rpcClient->Write( wxS( "mrp.eco" ), ids, vals );

        if( !writeResult.success )
        {
            return PLM_ECO_CREATE_RESULT::Failure( OdooErrorMapper::FromRpcResult( writeResult ),
                                                    OdooErrorMapper::FormatErrorMessage( writeResult ) );
        }

        PLM_ECO_UPDATE_RESULT ecoResult = GetECO( aEcrId );

        if( !ecoResult.success )
        {
            return PLM_ECO_CREATE_RESULT::Failure( ecoResult.error_code,
                                                    ecoResult.error_message );
        }

        ecoResult.eco.type = PLM_ECO_TYPE::ECO;
        ecoResult.eco.linked_ecr_id = aEcrId;

        return PLM_ECO_CREATE_RESULT::Success( ecoResult.eco );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_ECO_CREATE_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                                wxS( "Invalid server response" ) );
    }
}


//-------------------------------------------------------------------------
// BOM Management Interface
//-------------------------------------------------------------------------

PLM_BOM PLM_ODOO_PLUGIN::GetBOM( const wxString& aProductId, PLM_BOM_TYPE aType )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_BOM bom;

    if( !m_isAuthenticated || !m_rpcClient || aProductId.IsEmpty() )
        return bom;

    long productId = 0;

    if( !aProductId.ToLong( &productId ) )
        return bom;

    try
    {
        // Map PLM BOM type to Odoo BOM type string
        std::string odooType = "normal";

        if( aType == PLM_BOM_TYPE::PHANTOM )
            odooType = "phantom";

        nlohmann::json domain = nlohmann::json::array( {
            nlohmann::json::array( { "product_tmpl_id", "=", static_cast<int>( productId ) } ),
            nlohmann::json::array( { "type", "=", odooType } )
        } );

        nlohmann::json fields = nlohmann::json::array(
                { "id", "product_tmpl_id", "type", "product_qty",
                  "bom_line_ids", "create_date", "write_date" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "mrp.bom" ), domain, fields, 1, 0, wxS( "id desc" ) );

        if( !result.success || !result.result.is_array() || result.result.empty() )
            return bom;

        const auto& bomRecord = result.result[0];

        bom.bom_id = wxString::Format( wxS( "%d" ), bomRecord.value( "id", 0 ) );
        bom.product_id = aProductId;
        bom.type = aType;

        if( bomRecord.contains( "product_tmpl_id" ) && bomRecord["product_tmpl_id"].is_array()
            && bomRecord["product_tmpl_id"].size() >= 2 )
        {
            bom.product_name = wxString::FromUTF8(
                    bomRecord["product_tmpl_id"][1].get<std::string>() );
        }

        // Read BOM lines
        if( bomRecord.contains( "bom_line_ids" ) && bomRecord["bom_line_ids"].is_array()
            && !bomRecord["bom_line_ids"].empty() )
        {
            nlohmann::json lineIds = bomRecord["bom_line_ids"];
            nlohmann::json lineFields = nlohmann::json::array(
                    { "id", "product_id", "product_qty", "product_uom_id", "sequence" } );

            ODOO_RPC_RESULT lineResult = m_rpcClient->Read(
                    wxS( "mrp.bom.line" ), lineIds, lineFields );

            if( lineResult.success && lineResult.result.is_array() )
            {
                for( const auto& line : lineResult.result )
                {
                    PLM_BOM_LINE bomLine;
                    bomLine.line_id = wxString::Format( wxS( "%d" ), line.value( "id", 0 ) );
                    bomLine.quantity = line.value( "product_qty", 1.0 );
                    bomLine.sequence = line.value( "sequence", 0 );

                    if( line.contains( "product_id" ) && line["product_id"].is_array()
                        && line["product_id"].size() >= 2 )
                    {
                        bomLine.part_id = wxString::Format( wxS( "%d" ),
                                line["product_id"][0].get<int>() );
                        bomLine.description = wxString::FromUTF8(
                                line["product_id"][1].get<std::string>() );
                    }

                    if( line.contains( "product_uom_id" ) && line["product_uom_id"].is_array()
                        && line["product_uom_id"].size() >= 2 )
                    {
                        bomLine.uom = wxString::FromUTF8(
                                line["product_uom_id"][1].get<std::string>() );
                    }

                    bom.lines.push_back( bomLine );
                }
            }
        }
    }
    catch( const nlohmann::json::exception& )
    {
    }

    return bom;
}


std::vector<PLM_BOM> PLM_ODOO_PLUGIN::GetBOMs( const wxString& aProductId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_BOM> boms;

    if( !m_isAuthenticated || !m_rpcClient || aProductId.IsEmpty() )
        return boms;

    long productId = 0;

    if( !aProductId.ToLong( &productId ) )
        return boms;

    try
    {
        nlohmann::json domain = nlohmann::json::array(
                { nlohmann::json::array( { "product_tmpl_id", "=", static_cast<int>( productId ) } ) } );

        nlohmann::json fields = nlohmann::json::array(
                { "id", "product_tmpl_id", "type", "product_qty",
                  "bom_line_ids", "create_date", "write_date" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "mrp.bom" ), domain, fields, 100, 0, wxS( "id" ) );

        if( !result.success || !result.result.is_array() )
            return boms;

        for( const auto& bomRecord : result.result )
        {
            PLM_BOM bom;
            bom.bom_id = wxString::Format( wxS( "%d" ), bomRecord.value( "id", 0 ) );
            bom.product_id = aProductId;

            wxString odooType = wxString::FromUTF8( bomRecord.value( "type", std::string( "normal" ) ) );
            bom.type = ( odooType == wxS( "phantom" ) ) ? PLM_BOM_TYPE::PHANTOM : PLM_BOM_TYPE::ENGINEERING;

            if( bomRecord.contains( "product_tmpl_id" ) && bomRecord["product_tmpl_id"].is_array()
                && bomRecord["product_tmpl_id"].size() >= 2 )
            {
                bom.product_name = wxString::FromUTF8(
                        bomRecord["product_tmpl_id"][1].get<std::string>() );
            }

            // Line count is available; full lines would need a separate Read call
            if( bomRecord.contains( "bom_line_ids" ) && bomRecord["bom_line_ids"].is_array() )
            {
                nlohmann::json lineIds = bomRecord["bom_line_ids"];

                if( !lineIds.empty() )
                {
                    nlohmann::json lineFields = nlohmann::json::array(
                            { "id", "product_id", "product_qty", "product_uom_id", "sequence" } );

                    ODOO_RPC_RESULT lineResult = m_rpcClient->Read(
                            wxS( "mrp.bom.line" ), lineIds, lineFields );

                    if( lineResult.success && lineResult.result.is_array() )
                    {
                        for( const auto& line : lineResult.result )
                        {
                            PLM_BOM_LINE bomLine;
                            bomLine.line_id = wxString::Format( wxS( "%d" ), line.value( "id", 0 ) );
                            bomLine.quantity = line.value( "product_qty", 1.0 );
                            bomLine.sequence = line.value( "sequence", 0 );

                            if( line.contains( "product_id" ) && line["product_id"].is_array()
                                && line["product_id"].size() >= 2 )
                            {
                                bomLine.part_id = wxString::Format( wxS( "%d" ),
                                        line["product_id"][0].get<int>() );
                                bomLine.description = wxString::FromUTF8(
                                        line["product_id"][1].get<std::string>() );
                            }

                            if( line.contains( "product_uom_id" )
                                && line["product_uom_id"].is_array()
                                && line["product_uom_id"].size() >= 2 )
                            {
                                bomLine.uom = wxString::FromUTF8(
                                        line["product_uom_id"][1].get<std::string>() );
                            }

                            bom.lines.push_back( bomLine );
                        }
                    }
                }
            }

            boms.push_back( bom );
        }
    }
    catch( const nlohmann::json::exception& )
    {
    }

    return boms;
}


PLM_BOM_RESULT PLM_ODOO_PLUGIN::CreateBOM( const PLM_BOM& aBom )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_BOM_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                         wxS( "Not authenticated" ) );

    if( aBom.product_id.IsEmpty() )
        return PLM_BOM_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                         wxS( "Product ID is required" ) );

    long productId = 0;

    if( !aBom.product_id.ToLong( &productId ) )
        return PLM_BOM_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                         wxS( "Invalid product ID" ) );

    try
    {
        nlohmann::json vals;
        vals["product_tmpl_id"] = static_cast<int>( productId );
        vals["type"] = ( aBom.type == PLM_BOM_TYPE::PHANTOM ) ? "phantom" : "normal";

        // Build BOM lines using Odoo one2many create commands: (0, 0, {vals})
        nlohmann::json lineCommands = nlohmann::json::array();

        for( const auto& line : aBom.lines )
        {
            nlohmann::json lineVals;

            long partId = 0;

            if( line.part_id.ToLong( &partId ) )
                lineVals["product_id"] = static_cast<int>( partId );

            lineVals["product_qty"] = line.quantity;

            if( line.sequence > 0 )
                lineVals["sequence"] = line.sequence;

            lineCommands.push_back( nlohmann::json::array( { 0, 0, lineVals } ) );
        }

        if( !lineCommands.empty() )
            vals["bom_line_ids"] = lineCommands;

        ODOO_RPC_RESULT createResult = m_rpcClient->Create( wxS( "mrp.bom" ), vals );

        if( !createResult.success )
        {
            return PLM_BOM_RESULT::Failure( OdooErrorMapper::FromRpcResult( createResult ),
                                             OdooErrorMapper::FormatErrorMessage( createResult ) );
        }

        int newId = createResult.result.get<int>();
        wxString bomIdStr = wxString::Format( wxS( "%d" ), newId );

        return PLM_BOM_RESULT::Success( bomIdStr,
                GetOdooWebUrl( wxS( "mrp.bom" ), newId ) );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_BOM_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                         wxS( "Invalid server response" ) );
    }
}


PLM_BOM_RESULT PLM_ODOO_PLUGIN::UpdateBOM( const PLM_BOM& aBom )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_BOM_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                         wxS( "Not authenticated" ) );

    if( aBom.bom_id.IsEmpty() )
        return PLM_BOM_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                         wxS( "BOM ID is required" ) );

    long bomId = 0;

    if( !aBom.bom_id.ToLong( &bomId ) )
        return PLM_BOM_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                         wxS( "Invalid BOM ID" ) );

    try
    {
        nlohmann::json ids = nlohmann::json::array( { static_cast<int>( bomId ) } );
        nlohmann::json vals;

        // Replace all BOM lines: (5, 0, 0) clears existing, then (0, 0, {}) creates new
        nlohmann::json lineCommands = nlohmann::json::array();
        lineCommands.push_back( nlohmann::json::array( { 5, 0, 0 } ) );

        for( const auto& line : aBom.lines )
        {
            nlohmann::json lineVals;

            long partId = 0;

            if( line.part_id.ToLong( &partId ) )
                lineVals["product_id"] = static_cast<int>( partId );

            lineVals["product_qty"] = line.quantity;

            if( line.sequence > 0 )
                lineVals["sequence"] = line.sequence;

            lineCommands.push_back( nlohmann::json::array( { 0, 0, lineVals } ) );
        }

        vals["bom_line_ids"] = lineCommands;

        ODOO_RPC_RESULT writeResult = m_rpcClient->Write( wxS( "mrp.bom" ), ids, vals );

        if( !writeResult.success )
        {
            return PLM_BOM_RESULT::Failure( OdooErrorMapper::FromRpcResult( writeResult ),
                                             OdooErrorMapper::FormatErrorMessage( writeResult ) );
        }

        return PLM_BOM_RESULT::Success( aBom.bom_id,
                GetOdooWebUrl( wxS( "mrp.bom" ), static_cast<int>( bomId ) ) );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_BOM_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                         wxS( "Invalid server response" ) );
    }
}


bool PLM_ODOO_PLUGIN::DeleteBOM( const wxString& aBomId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient || aBomId.IsEmpty() )
        return false;

    long bomId = 0;

    if( !aBomId.ToLong( &bomId ) )
        return false;

    nlohmann::json ids = nlohmann::json::array( { static_cast<int>( bomId ) } );
    ODOO_RPC_RESULT result = m_rpcClient->Unlink( wxS( "mrp.bom" ), ids );

    return result.success;
}


PLM_BOM_DIFF_RESULT PLM_ODOO_PLUGIN::CompareBOMs( const PLM_BOM&              aBomA,
                                                    const PLM_BOM&              aBomB,
                                                    const PLM_BOM_DIFF_OPTIONS& aOptions )
{
    PLM_BOM_DIFF_RESULT diff;

    // Build lookup of BOM B lines by part_id for matching
    std::map<wxString, const PLM_BOM_LINE*> bLookup;

    for( const auto& line : aBomB.lines )
    {
        if( !line.dnp || !aOptions.ignore_dnp )
        {
            wxString key = aOptions.match_by_reference ? line.reference : line.part_id;
            bLookup[key] = &line;
        }
    }

    // Find added and changed lines
    std::set<wxString> matchedKeys;

    for( const auto& lineA : aBomA.lines )
    {
        if( lineA.dnp && aOptions.ignore_dnp )
            continue;

        wxString key = aOptions.match_by_reference ? lineA.reference : lineA.part_id;
        auto it = bLookup.find( key );

        if( it == bLookup.end() )
        {
            PLM_BOM_DIFF_ENTRY entry;
            entry.change_type = PLM_BOM_DIFF_TYPE::REMOVED;
            entry.old_line = lineA;
            diff.entries.push_back( entry );
            ++diff.removed_count;
        }
        else
        {
            matchedKeys.insert( key );
            const PLM_BOM_LINE* lineB = it->second;

            if( lineA.quantity != lineB->quantity || lineA.part_id != lineB->part_id )
            {
                PLM_BOM_DIFF_ENTRY entry;
                entry.change_type = PLM_BOM_DIFF_TYPE::CHANGED;
                entry.old_line = lineA;
                entry.new_line = *lineB;
                diff.entries.push_back( entry );
                ++diff.changed_count;
            }
            else
            {
                ++diff.unchanged_count;
            }
        }
    }

    // Find lines in B but not in A (additions)
    for( const auto& lineB : aBomB.lines )
    {
        if( lineB.dnp && aOptions.ignore_dnp )
            continue;

        wxString key = aOptions.match_by_reference ? lineB.reference : lineB.part_id;

        if( matchedKeys.find( key ) == matchedKeys.end() )
        {
            PLM_BOM_DIFF_ENTRY entry;
            entry.change_type = PLM_BOM_DIFF_TYPE::ADDED;
            entry.new_line = lineB;
            diff.entries.push_back( entry );
            ++diff.added_count;
        }
    }

    return diff;
}


PLM_BOM_VALIDATION_RESULT PLM_ODOO_PLUGIN::ValidateBOMEntries(
        const std::vector<PLM_BOM_ENTRY>& aEntries )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_BOM_VALIDATION_RESULT result;
    result.total_parts = aEntries.size();

    if( !m_isAuthenticated || !m_rpcClient || aEntries.empty() )
    {
        result.valid_parts = result.total_parts;
        result.all_valid = true;
        return result;
    }

    // Collect part numbers to validate in a single search
    nlohmann::json partNumbers = nlohmann::json::array();

    for( const auto& entry : aEntries )
    {
        if( !entry.part_number.IsEmpty() && !entry.dnp )
            partNumbers.push_back( entry.part_number.ToStdString() );
    }

    if( partNumbers.empty() )
    {
        result.valid_parts = result.total_parts;
        result.all_valid = true;
        return result;
    }

    std::vector<wxString> partNumVec;

    for( const auto& entry : aEntries )
    {
        if( !entry.part_number.IsEmpty() && !entry.dnp )
            partNumVec.push_back( entry.part_number );
    }

    // Delegate to existing ValidateBOM which already handles the logic
    PLM_BOM_VALIDATION_RESULT valResult = ValidateBOM( partNumVec );

    // Adjust counts for DNP entries
    size_t dnpCount = aEntries.size() - partNumVec.size();
    valResult.total_parts = aEntries.size();
    valResult.valid_parts += dnpCount;

    return valResult;
}


//-------------------------------------------------------------------------
// Routing Interface
//-------------------------------------------------------------------------

PLM_ROUTING PLM_ODOO_PLUGIN::GetRouting( const wxString& aProductId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_ROUTING routing;

    if( !m_isAuthenticated || !m_rpcClient || aProductId.IsEmpty() )
        return routing;

    long productId = 0;

    if( !aProductId.ToLong( &productId ) )
        return routing;

    try
    {
        // First find the BOM for this product to get the operations
        nlohmann::json bomDomain = nlohmann::json::array(
                { nlohmann::json::array( { "product_tmpl_id", "=", static_cast<int>( productId ) } ) } );
        nlohmann::json bomFields = nlohmann::json::array( { "id", "operation_ids" } );

        ODOO_RPC_RESULT bomResult = m_rpcClient->SearchRead(
                wxS( "mrp.bom" ), bomDomain, bomFields, 1, 0, wxS( "id desc" ) );

        if( !bomResult.success || !bomResult.result.is_array() || bomResult.result.empty() )
            return routing;

        const auto& bomRecord = bomResult.result[0];

        if( !bomRecord.contains( "operation_ids" ) || !bomRecord["operation_ids"].is_array()
            || bomRecord["operation_ids"].empty() )
        {
            return routing;
        }

        routing.product_id = aProductId;
        routing.routing_id = wxString::Format( wxS( "%d" ), bomRecord.value( "id", 0 ) );
        routing.name = wxS( "Manufacturing Routing" );

        nlohmann::json opIds = bomRecord["operation_ids"];
        nlohmann::json opFields = nlohmann::json::array(
                { "id", "name", "sequence", "workcenter_id", "time_cycle_manual", "note" } );

        ODOO_RPC_RESULT opResult = m_rpcClient->Read(
                wxS( "mrp.routing.workcenter" ), opIds, opFields );

        if( opResult.success && opResult.result.is_array() )
        {
            for( const auto& op : opResult.result )
            {
                PLM_OPERATION operation;
                operation.operation_id = wxString::Format( wxS( "%d" ), op.value( "id", 0 ) );
                operation.name = wxString::FromUTF8( op.value( "name", std::string() ) );
                operation.sequence = op.value( "sequence", 0 );
                operation.duration_minutes = op.value( "time_cycle_manual", 0.0 );
                operation.description = wxString::FromUTF8( op.value( "note", std::string() ) );

                if( op.contains( "workcenter_id" ) && op["workcenter_id"].is_array()
                    && op["workcenter_id"].size() >= 2 )
                {
                    operation.workcenter_id = wxString::Format( wxS( "%d" ),
                            op["workcenter_id"][0].get<int>() );
                }

                routing.operations.push_back( operation );
            }
        }
    }
    catch( const nlohmann::json::exception& )
    {
    }

    return routing;
}


std::vector<PLM_WORKCENTER> PLM_ODOO_PLUGIN::GetWorkcenters()
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_WORKCENTER> workcenters;

    if( !m_isAuthenticated || !m_rpcClient )
        return workcenters;

    try
    {
        nlohmann::json domain = nlohmann::json::array(
                { nlohmann::json::array( { "active", "=", true } ) } );
        nlohmann::json fields = nlohmann::json::array(
                { "id", "name", "code", "note", "costs_hour" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "mrp.workcenter" ), domain, fields, 100, 0, wxS( "name" ) );

        if( !result.success || !result.result.is_array() )
            return workcenters;

        for( const auto& wc : result.result )
        {
            PLM_WORKCENTER workcenter;
            workcenter.workcenter_id = wxString::Format( wxS( "%d" ), wc.value( "id", 0 ) );
            workcenter.name = wxString::FromUTF8( wc.value( "name", std::string() ) );
            workcenter.code = wxString::FromUTF8( wc.value( "code", std::string() ) );
            workcenter.description = wxString::FromUTF8( wc.value( "note", std::string() ) );
            workcenter.cost_per_hour = wc.value( "costs_hour", 0.0 );
            workcenter.active = true;

            workcenters.push_back( workcenter );
        }
    }
    catch( const nlohmann::json::exception& )
    {
    }

    return workcenters;
}


PLM_ROUTING_RESULT PLM_ODOO_PLUGIN::SaveRouting( const PLM_ROUTING& aRouting )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_ROUTING_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                             wxS( "Not authenticated" ) );

    if( aRouting.routing_id.IsEmpty() )
        return PLM_ROUTING_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                             wxS( "Routing ID (BOM ID) is required" ) );

    long bomId = 0;

    if( !aRouting.routing_id.ToLong( &bomId ) )
        return PLM_ROUTING_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                             wxS( "Invalid routing ID" ) );

    try
    {
        // Replace all operations on the BOM
        nlohmann::json ids = nlohmann::json::array( { static_cast<int>( bomId ) } );
        nlohmann::json opCommands = nlohmann::json::array();
        opCommands.push_back( nlohmann::json::array( { 5, 0, 0 } ) );

        for( const auto& op : aRouting.operations )
        {
            nlohmann::json opVals;
            opVals["name"] = op.name.ToStdString();
            opVals["sequence"] = op.sequence;
            opVals["time_cycle_manual"] = op.duration_minutes;

            long wcId = 0;

            if( op.workcenter_id.ToLong( &wcId ) )
                opVals["workcenter_id"] = static_cast<int>( wcId );

            if( !op.description.IsEmpty() )
                opVals["note"] = op.description.ToStdString();

            opCommands.push_back( nlohmann::json::array( { 0, 0, opVals } ) );
        }

        nlohmann::json vals;
        vals["operation_ids"] = opCommands;

        ODOO_RPC_RESULT writeResult = m_rpcClient->Write( wxS( "mrp.bom" ), ids, vals );

        if( !writeResult.success )
        {
            return PLM_ROUTING_RESULT::Failure( OdooErrorMapper::FromRpcResult( writeResult ),
                                                 OdooErrorMapper::FormatErrorMessage( writeResult ) );
        }

        return PLM_ROUTING_RESULT::Success( aRouting.routing_id );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_ROUTING_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                             wxS( "Invalid server response" ) );
    }
}


//-------------------------------------------------------------------------
// Lifecycle Transition Interface
//-------------------------------------------------------------------------

std::vector<PLM_STATE_INFO> PLM_ODOO_PLUGIN::GetStateMappings()
{
    std::vector<PLM_STATE_INFO> mappings;

    PLM_STATE_INFO draft;
    draft.native_state = wxS( "draft" );
    draft.display_name = wxS( "Draft" );
    draft.category = PLM_STATE_CATEGORY::EDITABLE;
    mappings.push_back( draft );

    PLM_STATE_INFO confirmed;
    confirmed.native_state = wxS( "confirmed" );
    confirmed.display_name = wxS( "Confirmed" );
    confirmed.category = PLM_STATE_CATEGORY::PENDING_REVIEW;
    mappings.push_back( confirmed );

    PLM_STATE_INFO released;
    released.native_state = wxS( "released" );
    released.display_name = wxS( "Released" );
    released.category = PLM_STATE_CATEGORY::RELEASED;
    mappings.push_back( released );

    PLM_STATE_INFO obsolete;
    obsolete.native_state = wxS( "obsolete" );
    obsolete.display_name = wxS( "Obsolete" );
    obsolete.category = PLM_STATE_CATEGORY::RELEASED;
    mappings.push_back( obsolete );

    return mappings;
}


std::vector<PLM_VALID_TRANSITION> PLM_ODOO_PLUGIN::GetValidTransitions(
        const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_VALID_TRANSITION> transitions;

    if( !m_isAuthenticated || !m_rpcClient || aProjectId.IsEmpty() )
        return transitions;

    PLM_STATE_INFO currentState;

    if( !GetCurrentState( aProjectId, currentState ) )
        return transitions;

    if( currentState.native_state == wxS( "draft" ) )
    {
        transitions.push_back( PLM_VALID_TRANSITION(
                wxS( "confirmed" ), PLM_STATE_CATEGORY::PENDING_REVIEW, wxS( "Confirm" ) ) );
        transitions.push_back( PLM_VALID_TRANSITION(
                wxS( "released" ), PLM_STATE_CATEGORY::RELEASED, wxS( "Release" ) ) );
    }
    else if( currentState.native_state == wxS( "confirmed" ) )
    {
        transitions.push_back( PLM_VALID_TRANSITION(
                wxS( "draft" ), PLM_STATE_CATEGORY::EDITABLE, wxS( "Set to Draft" ) ) );
        transitions.push_back( PLM_VALID_TRANSITION(
                wxS( "released" ), PLM_STATE_CATEGORY::RELEASED, wxS( "Release" ) ) );
    }
    else if( currentState.native_state == wxS( "released" ) )
    {
        transitions.push_back( PLM_VALID_TRANSITION(
                wxS( "obsolete" ), PLM_STATE_CATEGORY::RELEASED, wxS( "Obsolete" ) ) );
    }

    return transitions;
}


PLM_STATE_TRANSITION_RESULT PLM_ODOO_PLUGIN::ChangeState( const wxString& aProjectId,
                                                           const wxString& aTargetState,
                                                           const wxString& aComment )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_STATE_TRANSITION_RESULT::Error( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                                    wxS( "Not authenticated" ) );

    if( aProjectId.IsEmpty() || aTargetState.IsEmpty() )
        return PLM_STATE_TRANSITION_RESULT::Error( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                                    wxS( "Project ID and target state are required" ) );

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) )
        return PLM_STATE_TRANSITION_RESULT::Error( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                                    wxS( "Invalid project ID" ) );

    try
    {
        nlohmann::json ids = nlohmann::json::array( { static_cast<int>( productId ) } );
        nlohmann::json vals;
        vals["state"] = aTargetState.ToStdString();

        ODOO_RPC_RESULT writeResult = m_rpcClient->Write( wxS( "product.template" ), ids, vals );

        if( !writeResult.success )
        {
            return PLM_STATE_TRANSITION_RESULT::Error(
                    OdooErrorMapper::FromRpcResult( writeResult ),
                    OdooErrorMapper::FormatErrorMessage( writeResult ) );
        }

        // Post transition comment if provided
        if( !aComment.IsEmpty() )
        {
            nlohmann::json msgArgs = nlohmann::json::array( { static_cast<int>( productId ) } );
            nlohmann::json msgKwargs;
            msgKwargs["body"] = aComment.ToStdString();
            msgKwargs["message_type"] = "comment";

            m_rpcClient->CallMethod( wxS( "product.template" ), wxS( "message_post" ),
                                     msgArgs, msgKwargs );
        }

        // Map the target state to a category
        PLM_STATE_CATEGORY category = PLM_STATE_CATEGORY::EDITABLE;

        if( aTargetState == wxS( "released" ) )
            category = PLM_STATE_CATEGORY::RELEASED;
        else if( aTargetState == wxS( "confirmed" ) )
            category = PLM_STATE_CATEGORY::PENDING_REVIEW;
        else if( aTargetState == wxS( "obsolete" ) )
            category = PLM_STATE_CATEGORY::RELEASED;

        return PLM_STATE_TRANSITION_RESULT::Success( aTargetState, category );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_STATE_TRANSITION_RESULT::Error( PLM_ERROR_CODE::ERR_INTERNAL,
                                                    wxS( "Invalid server response" ) );
    }
}


bool PLM_ODOO_PLUGIN::GetCurrentState( const wxString& aProjectId, PLM_STATE_INFO& aStateInfo )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient || aProjectId.IsEmpty() )
        return false;

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) )
        return false;

    try
    {
        nlohmann::json domain = nlohmann::json::array(
                { nlohmann::json::array( { "id", "=", static_cast<int>( productId ) } ) } );
        nlohmann::json fields = nlohmann::json::array( { "state" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "product.template" ), domain, fields, 1 );

        if( !result.success || !result.result.is_array() || result.result.empty() )
            return false;

        wxString state = wxString::FromUTF8(
                result.result[0].value( "state", std::string( "draft" ) ) );

        aStateInfo.native_state = state;

        if( state == wxS( "released" ) )
        {
            aStateInfo.category = PLM_STATE_CATEGORY::RELEASED;
            aStateInfo.display_name = wxS( "Released" );
        }
        else if( state == wxS( "confirmed" ) )
        {
            aStateInfo.category = PLM_STATE_CATEGORY::PENDING_REVIEW;
            aStateInfo.display_name = wxS( "Confirmed" );
        }
        else if( state == wxS( "obsolete" ) )
        {
            aStateInfo.category = PLM_STATE_CATEGORY::RELEASED;
            aStateInfo.display_name = wxS( "Obsolete" );
        }
        else
        {
            aStateInfo.category = PLM_STATE_CATEGORY::EDITABLE;
            aStateInfo.display_name = wxS( "Draft" );
        }

        return true;
    }
    catch( const nlohmann::json::exception& )
    {
        return false;
    }
}


//-------------------------------------------------------------------------
// Review Interface
//-------------------------------------------------------------------------

PLM_REVIEW_RESULT PLM_ODOO_PLUGIN::SubmitForReview(
        const wxString& aProjectId, const PLM_REVIEW_SUBMIT_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                            wxS( "Not authenticated" ) );

    if( aProjectId.IsEmpty() )
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                            wxS( "Project ID is required" ) );

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) )
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                            wxS( "Invalid project ID" ) );

    try
    {
        // Create approval activities for each reviewer
        for( const auto& reviewerId : aOptions.reviewer_ids )
        {
            long userId = 0;

            if( !reviewerId.ToLong( &userId ) )
                continue;

            nlohmann::json actVals;
            actVals["res_model_id"] = 0; // Will be looked up
            actVals["res_id"] = static_cast<int>( productId );
            actVals["user_id"] = static_cast<int>( userId );
            actVals["summary"] = aOptions.title.ToStdString();
            actVals["note"] = aOptions.description.ToStdString();
            actVals["activity_type_id"] = 4; // Approval type in standard Odoo

            // Look up model ID for product.template
            nlohmann::json modelDomain = nlohmann::json::array(
                    { nlohmann::json::array( { "model", "=", "product.template" } ) } );
            nlohmann::json modelFields = nlohmann::json::array( { "id" } );

            ODOO_RPC_RESULT modelResult = m_rpcClient->SearchRead(
                    wxS( "ir.model" ), modelDomain, modelFields, 1 );

            if( modelResult.success && modelResult.result.is_array()
                && !modelResult.result.empty() )
            {
                actVals["res_model_id"] = modelResult.result[0]["id"].get<int>();
            }

            m_rpcClient->Create( wxS( "mail.activity" ), actVals );
        }

        PLM_REVIEW review;
        review.review_id = aProjectId;
        review.project_id = aProjectId;
        review.title = aOptions.title;
        review.description = aOptions.description;
        review.status = PLM_REVIEW_STATUS::OPEN;
        review.submitter_id = wxString::Format( wxS( "%d" ), m_userId );
        review.submitter_name = m_username;
        review.web_url = GetOdooWebUrl( wxS( "product.template" ),
                                         static_cast<int>( productId ) );

        return PLM_REVIEW_RESULT::Success( review );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                            wxS( "Invalid server response" ) );
    }
}


std::vector<PLM_REVIEW> PLM_ODOO_PLUGIN::GetReviews( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_REVIEW> reviews;

    if( !m_isAuthenticated || !m_rpcClient || aProjectId.IsEmpty() )
        return reviews;

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) )
        return reviews;

    try
    {
        nlohmann::json domain = nlohmann::json::array( {
            nlohmann::json::array( { "res_model", "=", "product.template" } ),
            nlohmann::json::array( { "res_id", "=", static_cast<int>( productId ) } )
        } );

        nlohmann::json fields = nlohmann::json::array(
                { "id", "summary", "note", "user_id", "create_uid",
                  "date_deadline", "create_date", "activity_type_id" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "mail.activity" ), domain, fields, 50, 0, wxS( "create_date desc" ) );

        if( !result.success || !result.result.is_array() )
            return reviews;

        for( const auto& activity : result.result )
        {
            PLM_REVIEW review;
            review.review_id = wxString::Format( wxS( "%d" ), activity.value( "id", 0 ) );
            review.project_id = aProjectId;
            review.title = wxString::FromUTF8( activity.value( "summary", std::string() ) );
            review.description = wxString::FromUTF8( activity.value( "note", std::string() ) );
            review.status = PLM_REVIEW_STATUS::OPEN;

            if( activity.contains( "create_uid" ) && activity["create_uid"].is_array()
                && activity["create_uid"].size() >= 2 )
            {
                review.submitter_id = wxString::Format( wxS( "%d" ),
                        activity["create_uid"][0].get<int>() );
                review.submitter_name = wxString::FromUTF8(
                        activity["create_uid"][1].get<std::string>() );
            }

            if( activity.contains( "user_id" ) && activity["user_id"].is_array()
                && activity["user_id"].size() >= 2 )
            {
                PLM_REVIEWER reviewer;
                reviewer.user_id = wxString::Format( wxS( "%d" ),
                        activity["user_id"][0].get<int>() );
                reviewer.display_name = wxString::FromUTF8(
                        activity["user_id"][1].get<std::string>() );
                reviewer.role = PLM_REVIEW_ROLE::REQUIRED;
                review.reviewers.push_back( reviewer );
            }

            reviews.push_back( review );
        }
    }
    catch( const nlohmann::json::exception& )
    {
    }

    return reviews;
}


PLM_REVIEW PLM_ODOO_PLUGIN::GetReview( const wxString& aReviewId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_REVIEW review;

    if( !m_isAuthenticated || !m_rpcClient || aReviewId.IsEmpty() )
        return review;

    long activityId = 0;

    if( !aReviewId.ToLong( &activityId ) )
        return review;

    try
    {
        nlohmann::json domain = nlohmann::json::array(
                { nlohmann::json::array( { "id", "=", static_cast<int>( activityId ) } ) } );

        nlohmann::json fields = nlohmann::json::array(
                { "id", "summary", "note", "user_id", "create_uid",
                  "res_id", "date_deadline", "create_date" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "mail.activity" ), domain, fields, 1 );

        if( !result.success || !result.result.is_array() || result.result.empty() )
            return review;

        const auto& activity = result.result[0];

        review.review_id = aReviewId;
        review.project_id = wxString::Format( wxS( "%d" ), activity.value( "res_id", 0 ) );
        review.title = wxString::FromUTF8( activity.value( "summary", std::string() ) );
        review.description = wxString::FromUTF8( activity.value( "note", std::string() ) );
        review.status = PLM_REVIEW_STATUS::OPEN;

        if( activity.contains( "create_uid" ) && activity["create_uid"].is_array()
            && activity["create_uid"].size() >= 2 )
        {
            review.submitter_id = wxString::Format( wxS( "%d" ),
                    activity["create_uid"][0].get<int>() );
            review.submitter_name = wxString::FromUTF8(
                    activity["create_uid"][1].get<std::string>() );
        }

        if( activity.contains( "user_id" ) && activity["user_id"].is_array()
            && activity["user_id"].size() >= 2 )
        {
            PLM_REVIEWER reviewer;
            reviewer.user_id = wxString::Format( wxS( "%d" ),
                    activity["user_id"][0].get<int>() );
            reviewer.display_name = wxString::FromUTF8(
                    activity["user_id"][1].get<std::string>() );
            reviewer.role = PLM_REVIEW_ROLE::REQUIRED;
            review.reviewers.push_back( reviewer );
        }
    }
    catch( const nlohmann::json::exception& )
    {
    }

    return review;
}


std::vector<PLM_REVIEW_COMMENT> PLM_ODOO_PLUGIN::GetReviewComments( const wxString& aReviewId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_REVIEW_COMMENT> comments;

    if( !m_isAuthenticated || !m_rpcClient || aReviewId.IsEmpty() )
        return comments;

    // aReviewId may be an activity ID; we need the res_id to get messages
    PLM_REVIEW review = GetReview( aReviewId );

    if( review.project_id.IsEmpty() )
        return comments;

    long productId = 0;

    if( !review.project_id.ToLong( &productId ) )
        return comments;

    try
    {
        nlohmann::json domain = nlohmann::json::array( {
            nlohmann::json::array( { "res_model", "=", "product.template" } ),
            nlohmann::json::array( { "res_id", "=", static_cast<int>( productId ) } ),
            nlohmann::json::array( { "message_type", "=", "comment" } )
        } );

        nlohmann::json fields = nlohmann::json::array(
                { "id", "body", "author_id", "date", "parent_id" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "mail.message" ), domain, fields, 100, 0, wxS( "date desc" ) );

        if( !result.success || !result.result.is_array() )
            return comments;

        for( const auto& msg : result.result )
        {
            PLM_REVIEW_COMMENT comment;
            comment.comment_id = wxString::Format( wxS( "%d" ), msg.value( "id", 0 ) );
            comment.review_id = aReviewId;
            comment.body = wxString::FromUTF8( msg.value( "body", std::string() ) );

            if( msg.contains( "author_id" ) && msg["author_id"].is_array()
                && msg["author_id"].size() >= 2 )
            {
                comment.author_id = wxString::Format( wxS( "%d" ),
                        msg["author_id"][0].get<int>() );
                comment.author_name = wxString::FromUTF8(
                        msg["author_id"][1].get<std::string>() );
            }

            if( msg.contains( "date" ) && msg["date"].is_string() )
            {
                wxDateTime dt;

                if( dt.ParseISOCombined( wxString::FromUTF8(
                        msg["date"].get<std::string>() ) ) )
                {
                    comment.created_at = dt.GetTicks();
                }
            }

            if( msg.contains( "parent_id" ) && msg["parent_id"].is_array()
                && msg["parent_id"].size() >= 1 )
            {
                comment.parent_comment_id = wxString::Format( wxS( "%d" ),
                        msg["parent_id"][0].get<int>() );
            }

            comments.push_back( comment );
        }
    }
    catch( const nlohmann::json::exception& )
    {
    }

    return comments;
}


bool PLM_ODOO_PLUGIN::AddReviewComment( const PLM_REVIEW_COMMENT& aComment )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient || aComment.review_id.IsEmpty() )
        return false;

    PLM_REVIEW review = GetReview( aComment.review_id );

    if( review.project_id.IsEmpty() )
        return false;

    long productId = 0;

    if( !review.project_id.ToLong( &productId ) )
        return false;

    try
    {
        nlohmann::json msgArgs = nlohmann::json::array( { static_cast<int>( productId ) } );
        nlohmann::json msgKwargs;
        msgKwargs["body"] = aComment.body.ToStdString();
        msgKwargs["message_type"] = "comment";

        if( !aComment.parent_comment_id.IsEmpty() )
        {
            long parentId = 0;

            if( aComment.parent_comment_id.ToLong( &parentId ) )
                msgKwargs["parent_id"] = static_cast<int>( parentId );
        }

        ODOO_RPC_RESULT result = m_rpcClient->CallMethod( wxS( "product.template" ),
                wxS( "message_post" ), msgArgs, msgKwargs );

        return result.success;
    }
    catch( const nlohmann::json::exception& )
    {
        return false;
    }
}


bool PLM_ODOO_PLUGIN::ResolveReviewComment( const wxString& aCommentId, bool aResolved )
{
    // Odoo mail.message doesn't have a native resolved flag. Best-effort no-op.
    return false;
}


PLM_REVIEW_RESULT PLM_ODOO_PLUGIN::SubmitReviewDecision( const wxString&     aReviewId,
                                                           PLM_REVIEW_DECISION aDecision,
                                                           const wxString&     aComment )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                            wxS( "Not authenticated" ) );

    if( aReviewId.IsEmpty() )
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                            wxS( "Review ID is required" ) );

    long activityId = 0;

    if( !aReviewId.ToLong( &activityId ) )
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                            wxS( "Invalid review ID" ) );

    try
    {
        // Mark the activity as done with feedback
        nlohmann::json args = nlohmann::json::array(
                { nlohmann::json::array( { static_cast<int>( activityId ) } ) } );
        nlohmann::json kwargs;
        kwargs["feedback"] = aComment.ToStdString();

        ODOO_RPC_RESULT result = m_rpcClient->CallMethod( wxS( "mail.activity" ),
                wxS( "action_feedback" ), args, kwargs );

        if( !result.success )
        {
            return PLM_REVIEW_RESULT::Failure( OdooErrorMapper::FromRpcResult( result ),
                                                OdooErrorMapper::FormatErrorMessage( result ) );
        }

        PLM_REVIEW review;
        review.review_id = aReviewId;

        if( aDecision == PLM_REVIEW_DECISION::APPROVE
            || aDecision == PLM_REVIEW_DECISION::APPROVE_WITH_CONDITIONS )
        {
            review.status = PLM_REVIEW_STATUS::APPROVED;
        }
        else if( aDecision == PLM_REVIEW_DECISION::REJECT )
        {
            review.status = PLM_REVIEW_STATUS::REJECTED;
        }
        else
        {
            review.status = PLM_REVIEW_STATUS::CLOSED;
        }

        return PLM_REVIEW_RESULT::Success( review );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                            wxS( "Invalid server response" ) );
    }
}


PLM_REVIEW_RESULT PLM_ODOO_PLUGIN::MergeReview( const wxString& aReviewId,
                                                  const wxString& aStrategy )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                            wxS( "Not authenticated" ) );

    // In Odoo, "merging" a review means transitioning the product to the next state
    PLM_REVIEW review = GetReview( aReviewId );

    if( review.project_id.IsEmpty() )
        return PLM_REVIEW_RESULT::Failure( PLM_ERROR_CODE::ERR_FILE_NOT_FOUND,
                                            wxS( "Review not found" ) );

    PLM_STATE_TRANSITION_RESULT stateResult = ChangeState( review.project_id,
            wxS( "released" ), wxS( "Approved via review" ) );

    if( !stateResult.success )
    {
        return PLM_REVIEW_RESULT::Failure( stateResult.error_code,
                                            stateResult.error_message );
    }

    review.status = PLM_REVIEW_STATUS::MERGED;
    return PLM_REVIEW_RESULT::Success( review );
}


//-------------------------------------------------------------------------
// Forge Provider Interface
//-------------------------------------------------------------------------

PLM_FORGE_PROVIDER* PLM_ODOO_PLUGIN::GetForgeProvider()
{
    return nullptr;
}


//-------------------------------------------------------------------------
// Part Management Extended Interface
//-------------------------------------------------------------------------

PLM_PART PLM_ODOO_PLUGIN::CreatePart( const PLM_PART_CREATE_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_PART part;

    if( !m_isAuthenticated || !m_rpcClient )
        return part;

    try
    {
        nlohmann::json vals;
        vals["name"] = aOptions.description.ToStdString();

        if( !aOptions.part_number.IsEmpty() )
            vals["default_code"] = aOptions.part_number.ToStdString();

        vals["type"] = "consu";

        if( !aOptions.category.IsEmpty() )
        {
            long catId = 0;

            if( aOptions.category.ToLong( &catId ) )
                vals["categ_id"] = static_cast<int>( catId );
        }

        if( !aOptions.manufacturer.IsEmpty() )
            vals["x_manufacturer"] = aOptions.manufacturer.ToStdString();

        if( !aOptions.manufacturer_pn.IsEmpty() )
            vals["x_manufacturer_pn"] = aOptions.manufacturer_pn.ToStdString();

        wxString stateStr = wxS( "active" );

        if( aOptions.initial_state == PLM_PART_STATE::DRAFT )
            stateStr = wxS( "draft" );
        else if( aOptions.initial_state == PLM_PART_STATE::PREFERRED )
            stateStr = wxS( "preferred" );

        vals["x_plm_state"] = stateStr.ToStdString();

        ODOO_RPC_RESULT createResult = m_rpcClient->Create( wxS( "product.product" ), vals );

        if( createResult.success && createResult.result.is_number() )
        {
            int newId = createResult.result.get<int>();
            return GetPart( wxString::Format( wxS( "%d" ), newId ) );
        }
    }
    catch( const nlohmann::json::exception& )
    {
    }

    return part;
}


PLM_PART PLM_ODOO_PLUGIN::UpdatePart( const PLM_PART& aPart )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_PART result;

    if( !m_isAuthenticated || !m_rpcClient || aPart.part_id.IsEmpty() )
        return result;

    long productId = 0;

    if( !aPart.part_id.ToLong( &productId ) )
        return result;

    try
    {
        nlohmann::json ids = nlohmann::json::array( { static_cast<int>( productId ) } );
        nlohmann::json vals;

        if( !aPart.description.IsEmpty() )
            vals["name"] = aPart.description.ToStdString();

        if( !aPart.part_number.IsEmpty() )
            vals["default_code"] = aPart.part_number.ToStdString();

        if( !aPart.manufacturer.IsEmpty() )
            vals["x_manufacturer"] = aPart.manufacturer.ToStdString();

        if( !aPart.manufacturer_pn.IsEmpty() )
            vals["x_manufacturer_pn"] = aPart.manufacturer_pn.ToStdString();

        if( !aPart.datasheet_url.IsEmpty() )
            vals["x_datasheet_url"] = aPart.datasheet_url.ToStdString();

        if( !aPart.symbol_ref.IsEmpty() )
            vals["x_symbol_ref"] = aPart.symbol_ref.ToStdString();

        if( !aPart.footprint_ref.IsEmpty() )
            vals["x_footprint_ref"] = aPart.footprint_ref.ToStdString();

        if( !vals.empty() )
        {
            ODOO_RPC_RESULT writeResult = m_rpcClient->Write(
                    wxS( "product.product" ), ids, vals );

            if( !writeResult.success )
                return result;
        }

        return GetPart( aPart.part_id );
    }
    catch( const nlohmann::json::exception& )
    {
        return result;
    }
}


PLM_PART PLM_ODOO_PLUGIN::UpdatePartState( const wxString& aPartId,
                                             PLM_PART_STATE  aNewState,
                                             const wxString& aComment )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    PLM_PART result;

    if( !m_isAuthenticated || !m_rpcClient || aPartId.IsEmpty() )
        return result;

    long productId = 0;

    if( !aPartId.ToLong( &productId ) )
        return result;

    try
    {
        wxString stateStr;

        switch( aNewState )
        {
        case PLM_PART_STATE::ACTIVE:          stateStr = wxS( "active" ); break;
        case PLM_PART_STATE::PREFERRED:       stateStr = wxS( "preferred" ); break;
        case PLM_PART_STATE::NOT_RECOMMENDED: stateStr = wxS( "not_recommended" ); break;
        case PLM_PART_STATE::OBSOLETE:        stateStr = wxS( "obsolete" ); break;
        case PLM_PART_STATE::END_OF_LIFE:     stateStr = wxS( "eol" ); break;
        case PLM_PART_STATE::DISCONTINUED:    stateStr = wxS( "discontinued" ); break;
        case PLM_PART_STATE::DRAFT:           stateStr = wxS( "draft" ); break;
        default:                              stateStr = wxS( "active" ); break;
        }

        nlohmann::json ids = nlohmann::json::array( { static_cast<int>( productId ) } );
        nlohmann::json vals;
        vals["x_plm_state"] = stateStr.ToStdString();

        // Set Odoo active flag to false for obsolete/discontinued
        if( aNewState == PLM_PART_STATE::OBSOLETE || aNewState == PLM_PART_STATE::DISCONTINUED )
            vals["active"] = false;

        ODOO_RPC_RESULT writeResult = m_rpcClient->Write(
                wxS( "product.product" ), ids, vals );

        if( !writeResult.success )
            return result;

        // Post state change comment
        if( !aComment.IsEmpty() )
        {
            nlohmann::json msgArgs = nlohmann::json::array( { static_cast<int>( productId ) } );
            nlohmann::json msgKwargs;
            msgKwargs["body"] = aComment.ToStdString();
            msgKwargs["message_type"] = "comment";

            m_rpcClient->CallMethod( wxS( "product.product" ), wxS( "message_post" ),
                                     msgArgs, msgKwargs );
        }

        return GetPart( aPartId );
    }
    catch( const nlohmann::json::exception& )
    {
        return result;
    }
}


std::vector<PLM_VENDOR_SOURCE> PLM_ODOO_PLUGIN::GetVendorSources( const wxString& aPartId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_VENDOR_SOURCE> sources;

    if( !m_isAuthenticated || !m_rpcClient || aPartId.IsEmpty() )
        return sources;

    long productId = 0;

    if( !aPartId.ToLong( &productId ) )
        return sources;

    try
    {
        // Get product_tmpl_id first
        nlohmann::json prodDomain = nlohmann::json::array(
                { nlohmann::json::array( { "id", "=", static_cast<int>( productId ) } ) } );
        nlohmann::json prodFields = nlohmann::json::array( { "product_tmpl_id" } );

        ODOO_RPC_RESULT prodResult = m_rpcClient->SearchRead(
                wxS( "product.product" ), prodDomain, prodFields, 1 );

        if( !prodResult.success || !prodResult.result.is_array() || prodResult.result.empty() )
            return sources;

        int tmplId = 0;
        const auto& prod = prodResult.result[0];

        if( prod.contains( "product_tmpl_id" ) && prod["product_tmpl_id"].is_array()
            && prod["product_tmpl_id"].size() >= 1 )
        {
            tmplId = prod["product_tmpl_id"][0].get<int>();
        }

        if( tmplId == 0 )
            return sources;

        nlohmann::json domain = nlohmann::json::array(
                { nlohmann::json::array( { "product_tmpl_id", "=", tmplId } ) } );

        nlohmann::json fields = nlohmann::json::array(
                { "id", "partner_id", "product_code", "price", "delay",
                  "min_qty", "currency_id", "sequence" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "product.supplierinfo" ), domain, fields, 100, 0, wxS( "sequence" ) );

        if( !result.success || !result.result.is_array() )
            return sources;

        bool first = true;

        for( const auto& supplier : result.result )
        {
            PLM_VENDOR_SOURCE source;
            source.vendor_id = wxString::Format( wxS( "%d" ), supplier.value( "id", 0 ) );

            if( supplier.contains( "partner_id" ) && supplier["partner_id"].is_array()
                && supplier["partner_id"].size() >= 2 )
            {
                source.vendor_name = wxString::FromUTF8(
                        supplier["partner_id"][1].get<std::string>() );
            }

            source.vendor_part_number = wxString::FromUTF8(
                    supplier.value( "product_code", std::string() ) );
            source.unit_price = supplier.value( "price", 0.0 );
            source.lead_time_days = supplier.value( "delay", 0 );
            source.min_order_qty = static_cast<int>( supplier.value( "min_qty", 0.0 ) );
            source.preferred = first;
            first = false;

            if( supplier.contains( "currency_id" ) && supplier["currency_id"].is_array()
                && supplier["currency_id"].size() >= 2 )
            {
                source.currency = wxString::FromUTF8(
                        supplier["currency_id"][1].get<std::string>() );
            }

            sources.push_back( source );
        }
    }
    catch( const nlohmann::json::exception& )
    {
    }

    return sources;
}


std::vector<PLM_WHERE_USED_ENTRY> PLM_ODOO_PLUGIN::GetWhereUsed( const wxString& aPartId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    std::vector<PLM_WHERE_USED_ENTRY> entries;

    if( !m_isAuthenticated || !m_rpcClient || aPartId.IsEmpty() )
        return entries;

    long productId = 0;

    if( !aPartId.ToLong( &productId ) )
        return entries;

    try
    {
        nlohmann::json domain = nlohmann::json::array(
                { nlohmann::json::array( { "product_id", "=", static_cast<int>( productId ) } ) } );

        nlohmann::json fields = nlohmann::json::array(
                { "id", "bom_id", "product_qty" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "mrp.bom.line" ), domain, fields, 100, 0, wxS( "id" ) );

        if( !result.success || !result.result.is_array() )
            return entries;

        for( const auto& line : result.result )
        {
            PLM_WHERE_USED_ENTRY entry;
            entry.quantity = line.value( "product_qty", 0.0 );
            entry.parent_type = wxS( "bom" );

            if( line.contains( "bom_id" ) && line["bom_id"].is_array()
                && line["bom_id"].size() >= 2 )
            {
                entry.parent_id = wxString::Format( wxS( "%d" ),
                        line["bom_id"][0].get<int>() );
                entry.parent_name = wxString::FromUTF8(
                        line["bom_id"][1].get<std::string>() );
            }

            entries.push_back( entry );
        }
    }
    catch( const nlohmann::json::exception& )
    {
    }

    return entries;
}


//-------------------------------------------------------------------------
// Release Interface
//-------------------------------------------------------------------------

std::vector<PLM_RELEASE_CHECK> PLM_ODOO_PLUGIN::ValidateForRelease( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );
    std::vector<PLM_RELEASE_CHECK> checks;

    if( !m_isAuthenticated || !m_rpcClient )
        return checks;

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) )
        return checks;

    try
    {
        nlohmann::json domain = nlohmann::json::array(
                { nlohmann::json::array( { "id", "=", productId } ) } );
        nlohmann::json fields = nlohmann::json::array(
                { "name", "default_code", "state", "bom_ids" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "product.template" ), domain, fields, 1 );

        if( !result.success || !result.result.is_array() || result.result.empty() )
        {
            checks.emplace_back( wxS( "exists" ), wxS( "Product Exists" ), false,
                                 wxS( "Product not found" ) );
            return checks;
        }

        const auto& product = result.result[0];

        checks.emplace_back( wxS( "exists" ), wxS( "Product Exists" ), true );

        bool hasPartNum = product.contains( "default_code" )
                          && product["default_code"].is_string()
                          && !product.value( "default_code", std::string() ).empty();

        checks.emplace_back( wxS( "part_number" ), wxS( "Part Number Assigned" ), hasPartNum,
                             hasPartNum ? wxS( "Part number is set" )
                                        : wxS( "Missing part number" ) );

        bool isReleased = product.contains( "state" ) && product["state"].is_string()
                          && product.value( "state", std::string() ) == "released";

        checks.emplace_back( wxS( "state" ), wxS( "Lifecycle State" ), isReleased,
                             isReleased ? wxS( "Product is in released state" )
                                        : wxS( "Product must be in released state" ) );

        bool hasBom = product.contains( "bom_ids" ) && product["bom_ids"].is_array()
                      && !product["bom_ids"].empty();

        checks.emplace_back( wxS( "bom" ), wxS( "BOM Exists" ), hasBom,
                             hasBom ? wxS( "BOM is defined" ) : wxS( "No BOM found" ) );
    }
    catch( const nlohmann::json::exception& )
    {
        checks.emplace_back( wxS( "error" ), wxS( "Validation Error" ), false,
                             wxS( "Failed to validate product" ) );
    }

    return checks;
}


PLM_RELEASE_RESULT PLM_ODOO_PLUGIN::CreateRelease( const wxString&            aProjectId,
                                                     const PLM_RELEASE_OPTIONS& aOptions )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_RELEASE_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                            wxS( "Not authenticated" ) );

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) )
        return PLM_RELEASE_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                            wxS( "Invalid project ID" ) );

    try
    {
        nlohmann::json ecoTypeDomain = nlohmann::json::array(
                { nlohmann::json::array( { "name", "ilike", "release" } ) } );
        nlohmann::json ecoTypeFields = nlohmann::json::array( { "id" } );

        ODOO_RPC_RESULT typeResult = m_rpcClient->SearchRead(
                wxS( "mrp.eco.type" ), ecoTypeDomain, ecoTypeFields, 1 );

        int ecoTypeId = 0;

        if( typeResult.success && typeResult.result.is_array() && !typeResult.result.empty() )
            ecoTypeId = typeResult.result[0].value( "id", 0 );

        nlohmann::json vals;
        vals["name"] = std::string( aOptions.title.utf8_str() );
        vals["product_tmpl_id"] = productId;

        if( ecoTypeId > 0 )
            vals["type_id"] = ecoTypeId;

        if( !aOptions.notes.IsEmpty() )
            vals["note"] = std::string( aOptions.notes.utf8_str() );

        ODOO_RPC_RESULT createResult = m_rpcClient->Create( wxS( "mrp.eco" ), vals );

        if( !createResult.success || !createResult.result.is_number() )
        {
            return PLM_RELEASE_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                                wxS( "Failed to create release record" ) );
        }

        int ecoId = createResult.result.get<int>();

        PLM_RELEASE release;
        release.release_id = wxString::Format( wxS( "%d" ), ecoId );
        release.project_id = aProjectId;
        release.version = aOptions.version;
        release.version_scheme = aOptions.version_scheme;
        release.title = aOptions.title;
        release.notes = aOptions.notes;
        release.status = aOptions.draft ? PLM_RELEASE_STATUS::DRAFT
                                        : PLM_RELEASE_STATUS::VALIDATING;
        release.released_by = m_username;

        wxDateTime now = wxDateTime::Now();
        release.created_at = now.GetTicks();

        return PLM_RELEASE_RESULT::Success( release );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_RELEASE_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                            wxS( "Invalid server response" ) );
    }
}


PLM_RELEASE_RESULT PLM_ODOO_PLUGIN::FinalizeRelease( const wxString& aReleaseId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_RELEASE_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                            wxS( "Not authenticated" ) );

    long ecoId = 0;

    if( !aReleaseId.ToLong( &ecoId ) )
        return PLM_RELEASE_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                            wxS( "Invalid release ID" ) );

    try
    {
        nlohmann::json args = nlohmann::json::array(
                { nlohmann::json::array( { ecoId } ) } );

        m_rpcClient->CallMethod( wxS( "mrp.eco" ), wxS( "action_approve" ), args );

        nlohmann::json domain = nlohmann::json::array(
                { nlohmann::json::array( { "id", "=", ecoId } ) } );
        nlohmann::json fields = nlohmann::json::array(
                { "name", "state", "product_tmpl_id", "note", "create_uid",
                  "create_date", "write_date" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "mrp.eco" ), domain, fields, 1 );

        if( !result.success || !result.result.is_array() || result.result.empty() )
        {
            return PLM_RELEASE_RESULT::Failure( PLM_ERROR_CODE::ERR_FILE_NOT_FOUND,
                                                wxS( "Release record not found after approval" ) );
        }

        const auto& record = result.result[0];

        PLM_RELEASE release;
        release.release_id = aReleaseId;
        release.title = wxString::FromUTF8( record.value( "name", std::string() ) );
        release.status = PLM_RELEASE_STATUS::RELEASED;
        release.notes = wxString::FromUTF8( record.value( "note", std::string() ) );

        if( record.contains( "product_tmpl_id" ) && record["product_tmpl_id"].is_array()
            && record["product_tmpl_id"].size() >= 1 )
        {
            release.project_id = wxString::Format(
                    wxS( "%d" ), record["product_tmpl_id"][0].get<int>() );
        }

        if( record.contains( "create_uid" ) && record["create_uid"].is_array()
            && record["create_uid"].size() >= 2 )
        {
            release.released_by = wxString::FromUTF8(
                    record["create_uid"][1].get<std::string>() );
        }

        wxDateTime now = wxDateTime::Now();
        release.released_at = now.GetTicks();

        return PLM_RELEASE_RESULT::Success( release );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_RELEASE_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                            wxS( "Invalid server response" ) );
    }
}


std::vector<PLM_RELEASE> PLM_ODOO_PLUGIN::ListReleases( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );
    std::vector<PLM_RELEASE> releases;

    if( !m_isAuthenticated || !m_rpcClient )
        return releases;

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) )
        return releases;

    try
    {
        nlohmann::json domain = nlohmann::json::array(
                { nlohmann::json::array( { "product_tmpl_id", "=", productId } ) } );
        nlohmann::json fields = nlohmann::json::array(
                { "name", "state", "note", "create_uid", "create_date", "write_date" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "mrp.eco" ), domain, fields, 0, 0, wxS( "create_date desc" ) );

        if( !result.success || !result.result.is_array() )
            return releases;

        for( const auto& record : result.result )
        {
            PLM_RELEASE release;
            release.release_id = wxString::Format( wxS( "%d" ), record.value( "id", 0 ) );
            release.project_id = aProjectId;
            release.title = wxString::FromUTF8( record.value( "name", std::string() ) );
            release.notes = wxString::FromUTF8( record.value( "note", std::string() ) );

            std::string state = record.value( "state", std::string( "draft" ) );

            if( state == "done" )
                release.status = PLM_RELEASE_STATUS::RELEASED;
            else if( state == "confirmed" || state == "progress" )
                release.status = PLM_RELEASE_STATUS::VALIDATING;
            else
                release.status = PLM_RELEASE_STATUS::DRAFT;

            if( record.contains( "create_uid" ) && record["create_uid"].is_array()
                && record["create_uid"].size() >= 2 )
            {
                release.released_by = wxString::FromUTF8(
                        record["create_uid"][1].get<std::string>() );
            }

            if( record.contains( "create_date" ) && record["create_date"].is_string() )
            {
                wxDateTime dt;

                if( dt.ParseISOCombined( wxString::FromUTF8(
                        record.value( "create_date", std::string() ) ) ) )
                {
                    release.created_at = dt.GetTicks();
                }
            }

            releases.push_back( release );
        }
    }
    catch( const nlohmann::json::exception& )
    {
        // Return whatever we collected
    }

    return releases;
}


PLM_RELEASE PLM_ODOO_PLUGIN::GetRelease( const wxString& aReleaseId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );
    PLM_RELEASE release;

    if( !m_isAuthenticated || !m_rpcClient )
        return release;

    long ecoId = 0;

    if( !aReleaseId.ToLong( &ecoId ) )
        return release;

    try
    {
        nlohmann::json domain = nlohmann::json::array(
                { nlohmann::json::array( { "id", "=", ecoId } ) } );
        nlohmann::json fields = nlohmann::json::array(
                { "name", "state", "product_tmpl_id", "note", "create_uid",
                  "create_date", "write_date" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "mrp.eco" ), domain, fields, 1 );

        if( !result.success || !result.result.is_array() || result.result.empty() )
            return release;

        const auto& record = result.result[0];

        release.release_id = aReleaseId;
        release.title = wxString::FromUTF8( record.value( "name", std::string() ) );
        release.notes = wxString::FromUTF8( record.value( "note", std::string() ) );

        std::string state = record.value( "state", std::string( "draft" ) );

        if( state == "done" )
            release.status = PLM_RELEASE_STATUS::RELEASED;
        else if( state == "confirmed" || state == "progress" )
            release.status = PLM_RELEASE_STATUS::VALIDATING;
        else
            release.status = PLM_RELEASE_STATUS::DRAFT;

        if( record.contains( "product_tmpl_id" ) && record["product_tmpl_id"].is_array()
            && record["product_tmpl_id"].size() >= 1 )
        {
            release.project_id = wxString::Format(
                    wxS( "%d" ), record["product_tmpl_id"][0].get<int>() );
        }

        if( record.contains( "create_uid" ) && record["create_uid"].is_array()
            && record["create_uid"].size() >= 2 )
        {
            release.released_by = wxString::FromUTF8(
                    record["create_uid"][1].get<std::string>() );
        }

        if( record.contains( "create_date" ) && record["create_date"].is_string() )
        {
            wxDateTime dt;

            if( dt.ParseISOCombined( wxString::FromUTF8(
                    record.value( "create_date", std::string() ) ) ) )
            {
                release.created_at = dt.GetTicks();
            }
        }

        if( state == "done" && record.contains( "write_date" )
            && record["write_date"].is_string() )
        {
            wxDateTime dt;

            if( dt.ParseISOCombined( wxString::FromUTF8(
                    record.value( "write_date", std::string() ) ) ) )
            {
                release.released_at = dt.GetTicks();
            }
        }
    }
    catch( const nlohmann::json::exception& )
    {
        // Return default
    }

    return release;
}


wxString PLM_ODOO_PLUGIN::SuggestNextVersion( const wxString&    aProjectId,
                                               PLM_VERSION_SCHEME aScheme )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return wxEmptyString;

    long productId = 0;

    if( !aProjectId.ToLong( &productId ) )
        return wxEmptyString;

    try
    {
        nlohmann::json domain = nlohmann::json::array(
                { nlohmann::json::array( { "product_tmpl_id", "=", productId } ),
                  nlohmann::json::array( { "state", "=", "done" } ) } );
        nlohmann::json fields = nlohmann::json::array( { "name" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "mrp.eco" ), domain, fields, 0, 0, wxS( "create_date desc" ) );

        int releaseCount = 0;

        if( result.success && result.result.is_array() )
            releaseCount = static_cast<int>( result.result.size() );

        switch( aScheme )
        {
        case PLM_VERSION_SCHEME::SEMVER:
            return wxString::Format( wxS( "%d.0.0" ), releaseCount + 1 );

        case PLM_VERSION_SCHEME::CALVER:
        {
            wxDateTime now = wxDateTime::Now();
            return wxString::Format( wxS( "%04d.%02d.%02d" ),
                                     now.GetYear(), now.GetMonth() + 1, now.GetDay() );
        }

        case PLM_VERSION_SCHEME::SEQUENTIAL:
        {
            char letter = 'A' + static_cast<char>( releaseCount % 26 );
            return wxString::Format( wxS( "Rev %c" ), letter );
        }

        default:
            return wxString::Format( wxS( "%d" ), releaseCount + 1 );
        }
    }
    catch( const nlohmann::json::exception& )
    {
        return wxEmptyString;
    }
}


//-------------------------------------------------------------------------
// Deviation Branch Interface
// Odoo PLM has no native deviation branch concept. These remain unimplemented.
//-------------------------------------------------------------------------

PLM_DEVIATION_BRANCH PLM_ODOO_PLUGIN::CreateDeviationBranch( const wxString& aProjectId,
                                                               const wxString& aBaseRevision,
                                                               const wxString& aReason )
{
    return PLM_DEVIATION_BRANCH();
}


std::vector<PLM_DEVIATION_BRANCH> PLM_ODOO_PLUGIN::ListDeviationBranches(
        const wxString& aProjectId )
{
    return {};
}


//-------------------------------------------------------------------------
// Variant Interface
//-------------------------------------------------------------------------

PLM_VARIANT PLM_ODOO_PLUGIN::GetVariants( const wxString& aProjectId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );
    PLM_VARIANT variant( aProjectId );

    if( !m_isAuthenticated || !m_rpcClient )
        return variant;

    long productTmplId = 0;

    if( !aProjectId.ToLong( &productTmplId ) )
        return variant;

    try
    {
        nlohmann::json domain = nlohmann::json::array(
                { nlohmann::json::array( { "product_tmpl_id", "=", productTmplId } ) } );
        nlohmann::json fields = nlohmann::json::array(
                { "name", "default_code", "active", "product_template_attribute_value_ids",
                  "combination_indices" } );

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "product.product" ), domain, fields, 0 );

        if( !result.success || !result.result.is_array() )
            return variant;

        // Find the base BOM for the template
        nlohmann::json bomDomain = nlohmann::json::array(
                { nlohmann::json::array( { "product_tmpl_id", "=", productTmplId } ),
                  nlohmann::json::array( { "type", "=", "normal" } ) } );
        nlohmann::json bomFields = nlohmann::json::array( { "id" } );

        ODOO_RPC_RESULT bomResult = m_rpcClient->SearchRead(
                wxS( "mrp.bom" ), bomDomain, bomFields, 1 );

        if( bomResult.success && bomResult.result.is_array() && !bomResult.result.empty() )
        {
            variant.base_bom_id = wxString::Format(
                    wxS( "%d" ), bomResult.result[0].value( "id", 0 ) );
        }

        for( const auto& record : result.result )
        {
            PLM_VARIANT_CONFIG config;
            config.config_id = wxString::Format( wxS( "%d" ), record.value( "id", 0 ) );
            config.name = wxString::FromUTF8( record.value( "name", std::string() ) );
            config.active = record.value( "active", true );

            if( record.contains( "default_code" ) && record["default_code"].is_string() )
            {
                config.description = wxString::FromUTF8(
                        record.value( "default_code", std::string() ) );
            }

            variant.configs.push_back( config );
        }
    }
    catch( const nlohmann::json::exception& )
    {
        // Return whatever we collected
    }

    return variant;
}


PLM_VARIANT_RESULT PLM_ODOO_PLUGIN::CreateVariant( const wxString&           aProjectId,
                                                     const PLM_VARIANT_CONFIG& aConfig )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_VARIANT_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                            wxS( "Not authenticated" ) );

    long productTmplId = 0;

    if( !aProjectId.ToLong( &productTmplId ) )
        return PLM_VARIANT_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                            wxS( "Invalid project ID" ) );

    try
    {
        nlohmann::json vals;
        vals["product_tmpl_id"] = productTmplId;
        vals["name"] = std::string( aConfig.name.utf8_str() );
        vals["active"] = aConfig.active;

        if( !aConfig.description.IsEmpty() )
            vals["default_code"] = std::string( aConfig.description.utf8_str() );

        ODOO_RPC_RESULT createResult = m_rpcClient->Create(
                wxS( "product.product" ), vals );

        if( !createResult.success || !createResult.result.is_number() )
        {
            return PLM_VARIANT_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                                wxS( "Failed to create variant" ) );
        }

        int variantId = createResult.result.get<int>();
        return PLM_VARIANT_RESULT::Success( wxString::Format( wxS( "%d" ), variantId ) );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_VARIANT_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                            wxS( "Invalid server response" ) );
    }
}


PLM_VARIANT_RESULT PLM_ODOO_PLUGIN::UpdateVariant( const PLM_VARIANT_CONFIG& aConfig )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_VARIANT_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                            wxS( "Not authenticated" ) );

    long variantId = 0;

    if( !aConfig.config_id.ToLong( &variantId ) )
        return PLM_VARIANT_RESULT::Failure( PLM_ERROR_CODE::ERR_INVALID_PARAMETER,
                                            wxS( "Invalid variant ID" ) );

    try
    {
        nlohmann::json vals;

        if( !aConfig.name.IsEmpty() )
            vals["name"] = std::string( aConfig.name.utf8_str() );

        if( !aConfig.description.IsEmpty() )
            vals["default_code"] = std::string( aConfig.description.utf8_str() );

        vals["active"] = aConfig.active;

        nlohmann::json ids = nlohmann::json::array( { variantId } );
        m_rpcClient->Write( wxS( "product.product" ), ids, vals );

        return PLM_VARIANT_RESULT::Success( aConfig.config_id );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_VARIANT_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                            wxS( "Invalid server response" ) );
    }
}


bool PLM_ODOO_PLUGIN::DeleteVariant( const wxString& aConfigId )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return false;

    long variantId = 0;

    if( !aConfigId.ToLong( &variantId ) )
        return false;

    try
    {
        nlohmann::json ids = nlohmann::json::array( { variantId } );
        nlohmann::json vals;
        vals["active"] = false;
        m_rpcClient->Write( wxS( "product.product" ), ids, vals );
        return true;
    }
    catch( const nlohmann::json::exception& )
    {
        return false;
    }
}


//-------------------------------------------------------------------------
// CI/CD Pipeline Interface
// Odoo PLM has no native CI/CD pipeline concept. These remain unimplemented.
//-------------------------------------------------------------------------

std::vector<PLM_PIPELINE_RUN> PLM_ODOO_PLUGIN::GetPipelineRuns(
        const wxString& aProjectId, const PLM_PIPELINE_FILTER& aFilter )
{
    return {};
}


PLM_PIPELINE_RUN PLM_ODOO_PLUGIN::GetPipelineRun( const wxString& aProjectId,
                                                    const wxString& aRunId )
{
    return PLM_PIPELINE_RUN();
}


bool PLM_ODOO_PLUGIN::DownloadCIArtifact( const wxString& aProjectId,
                                           const wxString& aRunId,
                                           const wxString& aArtifactId,
                                           const wxString& aLocalPath )
{
    return false;
}


PLM_CI_RUN_STATUS PLM_ODOO_PLUGIN::GetCommitPipelineStatus( const wxString& aProjectId,
                                                              const wxString& aCommitSha )
{
    return PLM_CI_RUN_STATUS::UNKNOWN;
}


//-------------------------------------------------------------------------
// Audit Trail Interface
//-------------------------------------------------------------------------

PLM_AUDIT_QUERY_RESULT PLM_ODOO_PLUGIN::GetAuditLog( const PLM_AUDIT_FILTER& aFilter )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );
    PLM_AUDIT_QUERY_RESULT queryResult;

    if( !m_isAuthenticated || !m_rpcClient )
    {
        queryResult.error = PLM_ERROR_CODE::ERR_AUTH_REQUIRED;
        queryResult.error_message = wxS( "Not authenticated" );
        return queryResult;
    }

    try
    {
        nlohmann::json domain = nlohmann::json::array();

        if( !aFilter.project_id.IsEmpty() )
        {
            long productId = 0;

            if( aFilter.project_id.ToLong( &productId ) )
            {
                domain.push_back( nlohmann::json::array( { "res_id", "=", productId } ) );
                domain.push_back( nlohmann::json::array(
                        { "model", "=", "product.template" } ) );
            }
        }

        if( !aFilter.user_id.IsEmpty() )
        {
            long userId = 0;

            if( aFilter.user_id.ToLong( &userId ) )
                domain.push_back( nlohmann::json::array( { "author_id", "=", userId } ) );
        }

        if( aFilter.start_time_ms > 0 )
        {
            wxDateTime startDt( static_cast<time_t>( aFilter.start_time_ms / 1000 ) );
            std::string startStr = std::string( startDt.FormatISOCombined().utf8_str() );
            domain.push_back( nlohmann::json::array( { "date", ">=", startStr } ) );
        }

        if( aFilter.end_time_ms > 0 )
        {
            wxDateTime endDt( static_cast<time_t>( aFilter.end_time_ms / 1000 ) );
            std::string endStr = std::string( endDt.FormatISOCombined().utf8_str() );
            domain.push_back( nlohmann::json::array( { "date", "<=", endStr } ) );
        }

        domain.push_back( nlohmann::json::array(
                { "message_type", "in",
                  nlohmann::json::array( { "notification", "comment", "email" } ) } ) );

        nlohmann::json fields = nlohmann::json::array(
                { "date", "body", "author_id", "res_id", "model", "subtype_id",
                  "tracking_value_ids", "message_type" } );

        size_t limit = aFilter.limit > 0 ? aFilter.limit : 100;

        ODOO_RPC_RESULT result = m_rpcClient->SearchRead(
                wxS( "mail.message" ), domain, fields,
                static_cast<int>( limit ), static_cast<int>( aFilter.offset ),
                wxS( "date desc" ) );

        if( !result.success || !result.result.is_array() )
            return queryResult;

        for( const auto& record : result.result )
        {
            PLM_AUDIT_EVENT event;
            event.event_id = wxString::Format( wxS( "%d" ), record.value( "id", 0 ) );

            if( record.contains( "date" ) && record["date"].is_string() )
            {
                wxDateTime dt;

                if( dt.ParseISOCombined( wxString::FromUTF8(
                        record.value( "date", std::string() ) ) ) )
                {
                    event.timestamp_ms = static_cast<int64_t>( dt.GetTicks() ) * 1000;
                }
            }

            if( record.contains( "author_id" ) && record["author_id"].is_array()
                && record["author_id"].size() >= 2 )
            {
                event.user_id = wxString::Format(
                        wxS( "%d" ), record["author_id"][0].get<int>() );
                event.user_name = wxString::FromUTF8(
                        record["author_id"][1].get<std::string>() );
            }

            if( record.contains( "res_id" ) && record["res_id"].is_number() )
            {
                event.project_id = wxString::Format(
                        wxS( "%d" ), record["res_id"].get<int>() );
            }

            std::string messageType = record.value( "message_type", std::string() );
            bool hasTracking = record.contains( "tracking_value_ids" )
                               && record["tracking_value_ids"].is_array()
                               && !record["tracking_value_ids"].empty();

            if( hasTracking )
                event.event_type = PLM_AUDIT_EVENT_TYPE::ATTRIBUTE_MODIFIED;
            else if( messageType == "comment" )
                event.event_type = PLM_AUDIT_EVENT_TYPE::REVIEW_COMMENT_ADDED;
            else
                event.event_type = PLM_AUDIT_EVENT_TYPE::CUSTOM;

            if( record.contains( "body" ) && record["body"].is_string() )
                event.result = wxString::FromUTF8( record.value( "body", std::string() ) );

            event.success = true;
            queryResult.events.push_back( event );
        }

        queryResult.total_count = queryResult.events.size();
        queryResult.has_more = queryResult.events.size() >= limit;
    }
    catch( const nlohmann::json::exception& )
    {
        queryResult.error = PLM_ERROR_CODE::ERR_INTERNAL;
        queryResult.error_message = wxS( "Invalid server response" );
    }

    return queryResult;
}


PLM_AUDIT_QUERY_RESULT PLM_ODOO_PLUGIN::GetProjectAuditLog( const wxString& aProjectId,
                                                              size_t          aLimit )
{
    PLM_AUDIT_FILTER filter;
    filter.project_id = aProjectId;
    filter.limit = aLimit;
    return GetAuditLog( filter );
}


//-------------------------------------------------------------------------
// File Operations Extended Interface
//-------------------------------------------------------------------------

PLM_CHECKOUT_RESULT PLM_ODOO_PLUGIN::CreateFolder( const wxString& aParentFolderId,
                                                     const wxString& aName )
{
    std::lock_guard<std::recursive_mutex> lock( m_mutex );

    if( !m_isAuthenticated || !m_rpcClient )
        return PLM_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
                                             wxS( "Not authenticated" ) );

    try
    {
        nlohmann::json vals;
        vals["name"] = std::string( aName.utf8_str() );
        vals["type"] = "binary";
        vals["mimetype"] = "inode/directory";

        if( !aParentFolderId.IsEmpty() )
        {
            long parentId = 0;

            if( aParentFolderId.ToLong( &parentId ) )
                vals["res_id"] = parentId;
        }

        ODOO_RPC_RESULT createResult = m_rpcClient->Create(
                wxS( "ir.attachment" ), vals );

        if( !createResult.success || !createResult.result.is_number() )
        {
            return PLM_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                                 wxS( "Failed to create folder" ) );
        }

        int folderId = createResult.result.get<int>();
        wxString folderIdStr = wxString::Format( wxS( "%d" ), folderId );

        PLM_FILE_INFO info;
        info.id = folderIdStr;
        info.name = aName;

        return PLM_CHECKOUT_RESULT::Success( folderIdStr, info );
    }
    catch( const nlohmann::json::exception& )
    {
        return PLM_CHECKOUT_RESULT::Failure( PLM_ERROR_CODE::ERR_INTERNAL,
                                             wxS( "Invalid server response" ) );
    }
}


std::vector<wxString> PLM_ODOO_PLUGIN::GetModifiedFiles( const wxString& aLocalPath )
{
    return {};
}


//-------------------------------------------------------------------------
// Preview Cache Interface
//-------------------------------------------------------------------------

PLM_PREVIEW_CACHE* PLM_ODOO_PLUGIN::GetPreviewCache()
{
    return nullptr;
}


// Plugin entry points
PLM_PLUGIN_ENTRY_POINTS( PLM_ODOO_PLUGIN )
