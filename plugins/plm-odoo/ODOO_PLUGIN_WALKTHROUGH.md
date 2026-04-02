# KiCad PLM Odoo Plugin Walkthrough

This document provides an in-depth walkthrough of the OdooPLM plugin implementation,
explaining architectural decisions, implementation patterns, and lessons learned.
Use this as a reference when creating your own PLM plugins.

## Architecture Overview

The Odoo plugin maps PLM concepts to Odoo models:

| PLM Concept | Odoo Implementation |
|-------------|-------------------|
| Projects | product.product records |
| Folders | product.category records |
| Revisions | Not yet implemented (OdooPLM versioning) |
| Checkout | Product info retrieval (no file transfer yet) |
| Checkin | Product update (no file transfer yet) |
| Lock | Client-side only (needs server persistence) |
| Attributes | Product fields (name, default_code, description, etc.) |

### File Structure

```
plugins/plm-odoo/
├── CMakeLists.txt           # Build configuration
├── plm_odoo_plugin.h        # Main plugin class header
├── plm_odoo_plugin.cpp      # Core plugin implementation
├── odoo_rpc_client.h        # JSON-RPC client interface
├── odoo_rpc_client.cpp      # JSON-RPC implementation
├── odoo_error_mapper.h      # Error code mapping
└── odoo_error_mapper.cpp    # Error mapping implementation
```

### Design Decisions

#### Decision 1: JSON-RPC 2.0 Protocol

Odoo's External API uses JSON-RPC 2.0 over HTTP. We implement this using KiCad's
existing KICAD_CURL_EASY wrapper:

```cpp
nlohmann::json OdooRpcClient::BuildRpcRequest( const nlohmann::json& aParams )
{
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["method"] = "call";
    request["id"] = ++m_requestId;
    request["params"] = aParams;
    return request;
}
```

Pros:
- Standard protocol with good documentation
- Works with any Odoo installation
- Same API for both Community and Enterprise editions

Cons:
- Verbose requests/responses
- No streaming support for large files

#### Decision 2: Product-to-File Mapping

We map Odoo's `product.product` model to PLM files. This is simpler than using
OdooPLM's document model and works without the PLM module installed:

```cpp
void PLM_ODOO_PLUGIN::PopulateFileInfoFromProduct( PLM_FILE_INFO& aFile,
                                                     const nlohmann::json& aProduct )
{
    aFile.id = wxString::Format( wxS( "%d" ), aProduct["id"].get<int>() );

    // Combine code and name for display
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
    aFile.state_category = PLM_STATE_CATEGORY::EDITABLE;
    aFile.native_state = wxS( "draft" );

    // Map category to folder
    if( aProduct.contains( "categ_id" ) && aProduct["categ_id"].is_array() &&
        aProduct["categ_id"].size() >= 1 )
    {
        aFile.folder_id = wxString::Format( wxS( "%d" ), aProduct["categ_id"][0].get<int>() );
    }
}
```

#### Decision 3: URL-Based Database Selection

Odoo servers can host multiple databases. We encode the database in the URL path:

```cpp
void PLM_ODOO_PLUGIN::ParseServerUrl( const wxString& aServerUrl )
{
    wxURI uri( aServerUrl );
    wxString path = uri.GetPath();
    wxString scheme = uri.GetScheme();
    wxString server = uri.GetServer();

    if( scheme.IsEmpty() )
        scheme = wxS( "https" );

    m_serverUrl = scheme + wxS( "://" ) + server;

    if( uri.HasPort() )
        m_serverUrl += wxS( ":" ) + uri.GetPort();

    // Extract database from path: https://server.com/database
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
```

Example URLs:
- `https://mycompany.odoo.com/production` - Database "production"
- `https://localhost:8069/testdb` - Database "testdb" on local server

## Implementation Details

### Authentication

Odoo supports session-based authentication via `/web/session/authenticate`:

```cpp
ODOO_RPC_RESULT OdooRpcClient::Authenticate( const wxString& aUsername,
                                              const wxString& aPassword )
{
    nlohmann::json params;
    params["db"] = m_database.ToStdString();
    params["login"] = aUsername.ToStdString();
    params["password"] = aPassword.ToStdString();

    ODOO_RPC_RESULT result = SendRequest( wxS( "/web/session/authenticate" ), params );

    if( result.success && result.result.is_object() )
    {
        if( result.result.contains( "uid" ) && result.result["uid"].is_number() )
        {
            m_session.uid = result.result["uid"].get<int>();
            m_session.username = aUsername;
            m_session.database = m_database;
            m_session.is_authenticated = true;

            if( result.result.contains( "partner_display_name" ) )
                m_session.partner_display_name = wxString::FromUTF8(
                        result.result["partner_display_name"].get<std::string>() );
        }
        else if( result.result["uid"].is_boolean() && !result.result["uid"].get<bool>() )
        {
            result.success = false;
            result.error_code = 401;
            result.error_message = wxS( "Invalid username or password" );
        }
    }

    return result;
}
```

Session cookies are maintained automatically by the RPC client.

### Folder Navigation

Folders map to Odoo's `product.category` hierarchy:

```cpp
std::vector<PLM_FOLDER> PLM_ODOO_PLUGIN::GetChildFolders( const wxString& aFolderId )
{
    std::vector<PLM_FOLDER> folders;

    if( !m_isAuthenticated || !m_rpcClient || aFolderId.IsEmpty() )
        return folders;

    long parentId = 0;
    if( !aFolderId.ToLong( &parentId ) )
        return folders;

    // Build Odoo domain filter
    nlohmann::json domain = nlohmann::json::array();
    domain.push_back( nlohmann::json::array( { "parent_id", "=", static_cast<int>( parentId ) } ) );

    nlohmann::json fields = nlohmann::json::array( { "id", "name", "child_id" } );

    ODOO_RPC_RESULT result = m_rpcClient->SearchRead( wxS( "product.category" ),
                                                       domain, fields, 0, 0, wxS( "name" ) );

    if( result.success && result.result.is_array() )
    {
        for( const auto& cat : result.result )
        {
            PLM_FOLDER folder;
            folder.id = wxString::Format( wxS( "%d" ), cat["id"].get<int>() );
            folder.name = wxString::FromUTF8( cat["name"].get<std::string>() );
            folder.parent_id = aFolderId;
            folder.has_children = cat.contains( "child_id" ) && cat["child_id"].is_array() &&
                                  !cat["child_id"].empty();

            // Cache category name for path building
            m_categoryCache[cat["id"].get<int>()] = folder.name;

            folders.push_back( folder );
        }
    }

    return folders;
}
```

### Error Handling

The error mapper translates Odoo errors to PLM error codes:

```cpp
PLM_ERROR_CODE OdooErrorMapper::FromRpcResult( const ODOO_RPC_RESULT& aRpcResult )
{
    if( aRpcResult.success )
        return PLM_ERROR_CODE::SUCCESS;

    if( IsAuthError( aRpcResult ) )
        return PLM_ERROR_CODE::ERR_AUTH_FAILED;

    if( IsPermissionError( aRpcResult ) )
        return PLM_ERROR_CODE::ERR_AUTH_INSUFFICIENT;

    if( IsNotFoundError( aRpcResult ) )
        return PLM_ERROR_CODE::ERR_FILE_NOT_FOUND;

    if( IsNetworkError( aRpcResult ) )
        return PLM_ERROR_CODE::ERR_NETWORK_UNREACHABLE;

    return PLM_ERROR_CODE::ERR_INTERNAL;
}

bool OdooErrorMapper::IsAuthError( const ODOO_RPC_RESULT& aRpcResult )
{
    wxString msg = aRpcResult.error_message.Lower();
    wxString data = aRpcResult.error_data.Lower();

    return msg.Contains( wxS( "session expired" ) ) ||
           msg.Contains( wxS( "authentication" ) ) ||
           msg.Contains( wxS( "access denied" ) ) ||
           data.Contains( wxS( "accessdenied" ) );
}
```

### Attribute Schema

The plugin defines a static schema based on Odoo's product fields:

```cpp
PLM_SCHEMA PLM_ODOO_PLUGIN::GetSchema()
{
    PLM_SCHEMA schema( wxS( "1.0" ) );

    PLM_ATTRIBUTE_SCHEMA name;
    name.attr_id = wxS( "name" );
    name.display_name = wxS( "Product Name" );
    name.type = PLM_ATTR_TYPE::STRING;
    name.required = true;
    name.editable_in_kicad = true;
    schema.attributes.push_back( name );

    PLM_ATTRIBUTE_SCHEMA defaultCode;
    defaultCode.attr_id = wxS( "default_code" );
    defaultCode.display_name = wxS( "Internal Reference" );
    defaultCode.type = PLM_ATTR_TYPE::STRING;
    defaultCode.required = false;
    defaultCode.editable_in_kicad = true;
    schema.attributes.push_back( defaultCode );

    // ... additional attributes

    return schema;
}
```

## Security Considerations

### Credential Protection

Passwords and API keys are redacted from trace logs:

```cpp
if( wxLog::IsAllowedTraceMask( traceOdooRpc ) )
{
    nlohmann::json logRequest = rpcRequest;

    if( logRequest.contains( "params" ) )
    {
        if( logRequest["params"].contains( "password" ) )
            logRequest["params"]["password"] = "***REDACTED***";

        if( logRequest["params"].contains( "api_key" ) )
            logRequest["params"]["api_key"] = "***REDACTED***";
    }

    wxLogTrace( traceOdooRpc, wxS( "Request to %s: %s" ), aEndpoint, logRequest.dump() );
}
```

## Current Limitations

### Not Yet Implemented

1. **File Transfer** - Checkout/checkin don't transfer actual files
2. **Server-Side Locking** - Locks are client-side only
3. **Revision History** - Returns static "revision 1"
4. **Actual OdooPLM Integration** - Uses base Odoo, not PLM module

### Capability Flags

The plugin honestly reports what's implemented:

```cpp
PLM_CAPABILITIES PLM_ODOO_PLUGIN::GetCapabilities() const
{
    PLM_CAPABILITIES caps;

    caps.supports_checkout = true;      // Workflow works, no file transfer
    caps.supports_locking = false;      // Disabled until server-side
    caps.supports_revisions = false;    // Disabled until real history
    caps.supports_attributes = true;    // Fully working
    caps.supports_search = true;        // Fully working
    caps.supports_api_keys = true;      // Fully working

    return caps;
}
```

## Future Improvements

1. **Attachment Handling** - Download/upload via `ir.attachment` model
2. **OdooPLM Documents** - Integrate with `plm.document` for versioning
3. **Server-Side Locks** - Store lock state in product custom fields
4. **Real Revision History** - Query Odoo's chatter/activity for history
5. **Custom Fields** - Dynamically discover and map custom product fields
6. **Multi-Company Support** - Handle Odoo's company-based access control

## Testing

To test against a local Odoo instance:

```bash
# Start Odoo with Docker
docker run -d -p 8069:8069 --name odoo odoo:17

# Configure KiCad to connect
# Server URL: http://localhost:8069/database_name
# Username: admin
# Password: admin (or configured password)
```

## References

- [KiCad PLM Plugin Author Guide](../../common/plm/PLM_PLUGIN_AUTHOR_GUIDE.md)
- [Odoo External API](https://www.odoo.com/documentation/18.0/developer/reference/external_api.html)
- [OdooPLM GitHub](https://github.com/OdooPLM/odooplm)
