# KiCad PLM Plugin Author Guide

This guide explains how to create a PLM (Product Lifecycle Management) plugin for KiCad.
PLM plugins allow KiCad to integrate with external PLM systems for project management,
version control, and workflow automation.

## Overview

A PLM plugin is a shared library (.dll on Windows, .so on Linux, .dylib on macOS) that
implements the `PLM_PLUGIN` interface. KiCad discovers and loads plugins at runtime,
allowing users to connect to different PLM systems without modifying KiCad itself.

## Prerequisites

- C++20 compiler (GCC 10+, Clang 12+, MSVC 2019+)
- wxWidgets 3.2+
- KiCad development headers
- CMake 3.21+

## Getting Started

### Step 1: Create Plugin Project Structure

```
my-plm-plugin/
├── CMakeLists.txt
├── src/
│   ├── my_plm_plugin.h
│   └── my_plm_plugin.cpp
├── include/
│   └── my_plm_plugin_export.h
└── tests/
    └── test_my_plm_plugin.cpp
```

### Step 2: Implement the Plugin Class

Create a class that inherits from `PLM_PLUGIN`:

```cpp
#include <plm/plm_plugin.h>

class MY_PLM_PLUGIN : public PLM_PLUGIN
{
public:
    MY_PLM_PLUGIN();
    ~MY_PLM_PLUGIN() override;

    // Plugin identity
    PLM_PLUGIN_INFO GetPluginInfo() const override;
    PLM_CAPABILITIES GetCapabilities() const override;

    // Lifecycle
    bool OnLoad() override;
    void OnUnload() override;

    // Authentication
    PLM_AUTH_RESULT Authenticate( const PLM_CREDENTIALS& aCredentials ) override;
    PLM_AUTH_RESULT RefreshSession() override;
    bool Logout() override;
    bool IsAuthenticated() const override;
    PLM_SESSION GetCurrentSession() const override;

    // Navigation
    std::vector<PLM_FOLDER> GetRootFolders() override;
    std::vector<PLM_FOLDER> GetChildFolders( const wxString& aFolderId ) override;
    PLM_FOLDER_PATH GetFolderPath( const wxString& aFolderId ) override;
    PLM_LIST_RESULT<PLM_FILE_INFO> ListFiles( const wxString& aFolderId,
                                               const PLM_LIST_OPTIONS& aOptions ) override;

    // File operations
    PLM_CHECKOUT_RESULT Checkout( const wxString& aProjectId,
                                   const wxString& aLocalPath,
                                   const PLM_CHECKOUT_OPTIONS& aOptions ) override;
    PLM_CHECKIN_RESULT Checkin( const wxString& aLocalPath,
                                 const PLM_CHECKIN_OPTIONS& aOptions ) override;
    PLM_FILE_INFO GetFileInfo( const wxString& aProjectId ) override;
    PLM_CANCEL_CHECKOUT_RESULT CancelCheckout( const wxString& aLocalPath ) override;

    // Locking
    PLM_LOCK_RESULT Lock( const PLM_LOCK_REQUEST& aRequest ) override;
    PLM_LOCK_RESULT Unlock( const PLM_UNLOCK_REQUEST& aRequest ) override;
    PLM_LOCK_INFO GetLockInfo( const wxString& aProjectId ) override;

    // Revisions
    PLM_REVISION_HISTORY GetRevisionHistory( const wxString& aProjectId ) override;
    PLM_REVISION GetRevision( const wxString& aProjectId,
                               const wxString& aRevisionId ) override;
    bool DownloadRevision( const wxString& aProjectId,
                            const wxString& aRevisionId,
                            const wxString& aLocalPath ) override;
    PLM_REVISION_COMPARE CompareRevisions( const wxString& aProjectId,
                                            const wxString& aRevision1,
                                            const wxString& aRevision2 ) override;

    // Project management
    PLM_CHECKOUT_RESULT CreateProject( const wxString& aFolderId,
                                        const wxString& aName,
                                        const wxString& aTemplateId ) override;
    bool DeleteProject( const wxString& aProjectId ) override;

    // Schema and attributes
    PLM_SCHEMA GetSchema() override;
    PLM_ATTRIBUTE_SET GetAttributes( const wxString& aProjectId ) override;
    bool SetAttributes( const wxString& aProjectId,
                         const PLM_ATTRIBUTE_SET& aAttributes ) override;

    // Permissions
    PLM_PERMISSIONS GetPermissions( const wxString& aProjectId ) override;
    wxString GetUserDisplayName( const wxString& aUserId ) override;

private:
    PLM_SESSION m_session;
    bool m_authenticated = false;
    // Add your PLM client connection here
};
```

### Step 3: Implement Plugin Info

```cpp
PLM_PLUGIN_INFO MY_PLM_PLUGIN::GetPluginInfo() const
{
    PLM_PLUGIN_INFO info;
    info.plugin_id = wxS( "my-plm-system" );
    info.name = wxS( "My PLM System" );
    info.vendor = wxS( "My Company" );
    info.version = PLM_VERSION( 1, 0, 0 );
    info.api_version = PLM_VERSION( PLM_API_VERSION_MAJOR, PLM_API_VERSION_MINOR, 0 );
    info.description = wxS( "Integration with My PLM System" );
    info.icon_name = wxS( "my_plm_icon" );
    return info;
}
```

### Step 4: Declare Capabilities

```cpp
PLM_CAPABILITIES MY_PLM_PLUGIN::GetCapabilities() const
{
    PLM_CAPABILITIES caps;

    // Core operations
    caps.supports_checkout = true;
    caps.supports_checkin = true;
    caps.supports_locking = true;
    caps.supports_revisions = true;

    // Extended features
    caps.supports_attributes = true;
    caps.supports_search = true;
    caps.supports_offline = false;

    // Locking model
    caps.lock_model = PLM_LOCK_MODEL::PESSIMISTIC;

    // Authentication methods
    caps.supported_auth_methods = {
        PLM_AUTH_METHOD::USERNAME_PASSWORD,
        PLM_AUTH_METHOD::TOKEN
    };

    return caps;
}
```

### Step 5: Implement Authentication

```cpp
PLM_AUTH_RESULT MY_PLM_PLUGIN::Authenticate( const PLM_CREDENTIALS& aCredentials )
{
    // Connect to your PLM system
    // Example using a REST API:

    try
    {
        wxString token = CallPLMLoginAPI(
            aCredentials.server_url,
            aCredentials.username,
            aCredentials.password.Get()
        );

        // Store session info
        m_session.token = PLM_SECURE_STRING( token );
        m_session.user_info.user_id = aCredentials.username;
        m_session.user_info.display_name = GetUserDisplayNameFromAPI( aCredentials.username );
        m_session.expires_at = wxDateTime::Now() + wxTimeSpan::Hours( 8 );
        m_authenticated = true;

        return PLM_AUTH_RESULT::Success( token, m_session.expires_at );
    }
    catch( const std::exception& e )
    {
        return PLM_AUTH_RESULT::Failure(
            PLM_ERROR_CODE::ERR_AUTH_FAILED,
            wxString::FromUTF8( e.what() )
        );
    }
}
```

### Step 6: Implement File Operations

```cpp
PLM_CHECKOUT_RESULT MY_PLM_PLUGIN::Checkout( const wxString& aProjectId,
                                              const wxString& aLocalPath,
                                              const PLM_CHECKOUT_OPTIONS& aOptions )
{
    if( !m_authenticated )
    {
        return PLM_CHECKOUT_RESULT::Failure(
            PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
            wxS( "Not authenticated" )
        );
    }

    try
    {
        // Acquire lock if requested
        if( aOptions.acquire_lock )
        {
            if( !AcquireLockFromAPI( aProjectId ) )
            {
                return PLM_CHECKOUT_RESULT::Failure(
                    PLM_ERROR_CODE::ERR_LOCK_CONFLICT,
                    wxS( "Could not acquire lock" )
                );
            }
        }

        // Download files
        DownloadProjectFilesFromAPI( aProjectId, aLocalPath );

        // Return success
        PLM_FILE_INFO fileInfo = GetFileInfo( aProjectId );
        return PLM_CHECKOUT_RESULT::Success( aLocalPath, fileInfo );
    }
    catch( const std::exception& e )
    {
        return PLM_CHECKOUT_RESULT::Failure(
            PLM_ERROR_CODE::ERR_CHECKOUT_FAILED,
            wxString::FromUTF8( e.what() )
        );
    }
}

PLM_CHECKIN_RESULT MY_PLM_PLUGIN::Checkin( const wxString& aLocalPath,
                                            const PLM_CHECKIN_OPTIONS& aOptions )
{
    if( !m_authenticated )
    {
        return PLM_CHECKIN_RESULT::Failure(
            PLM_ERROR_CODE::ERR_AUTH_REQUIRED,
            wxS( "Not authenticated" )
        );
    }

    try
    {
        // Upload files to PLM
        wxString projectId = GetProjectIdForLocalPath( aLocalPath );
        wxString newRevision = UploadFilesToAPI( projectId, aLocalPath, aOptions.revision_comment );

        // Release lock if requested
        if( aOptions.release_lock )
            ReleaseLockFromAPI( projectId );

        PLM_FILE_INFO fileInfo = GetFileInfo( projectId );
        fileInfo.revision = newRevision;

        return PLM_CHECKIN_RESULT::Success( newRevision, fileInfo );
    }
    catch( const std::exception& e )
    {
        return PLM_CHECKIN_RESULT::Failure(
            PLM_ERROR_CODE::ERR_CHECKIN_FAILED,
            wxString::FromUTF8( e.what() )
        );
    }
}
```

### Step 7: Export Plugin Entry Point

Create an export header and entry point function:

```cpp
// my_plm_plugin_export.h
#ifdef _WIN32
    #define PLM_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define PLM_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

// At the end of my_plm_plugin.cpp
extern "C" PLM_PLUGIN_EXPORT PLM_PLUGIN* CreatePLMPlugin()
{
    return new MY_PLM_PLUGIN();
}

extern "C" PLM_PLUGIN_EXPORT void DestroyPLMPlugin( PLM_PLUGIN* aPlugin )
{
    delete aPlugin;
}

extern "C" PLM_PLUGIN_EXPORT int GetPLMAPIVersion()
{
    return PLM_API_VERSION_MAJOR;
}
```

### Step 8: Create CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.21)
project(my-plm-plugin VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base)
find_package(KiCad REQUIRED COMPONENTS common)

add_library(my-plm-plugin SHARED
    src/my_plm_plugin.cpp
)

target_include_directories(my-plm-plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${KiCad_INCLUDE_DIRS}
)

target_link_libraries(my-plm-plugin PRIVATE
    ${wxWidgets_LIBRARIES}
    ${KiCad_LIBRARIES}
)

# Set output name without lib prefix on Linux
set_target_properties(my-plm-plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "plm_my_system"
)

# Install to KiCad plugins directory
install(TARGETS my-plm-plugin
    LIBRARY DESTINATION ${KiCad_PLUGIN_DIR}/plm
    RUNTIME DESTINATION ${KiCad_PLUGIN_DIR}/plm
)
```

## Error Handling

Use the provided error codes for consistent error reporting:

```cpp
// Common error codes
PLM_ERROR_CODE::SUCCESS              // Operation succeeded
PLM_ERROR_CODE::ERR_AUTH_REQUIRED    // Not authenticated
PLM_ERROR_CODE::ERR_AUTH_FAILED      // Authentication failed
PLM_ERROR_CODE::ERR_FILE_NOT_FOUND   // File/project not found
PLM_ERROR_CODE::ERR_LOCK_CONFLICT    // File locked by another user
PLM_ERROR_CODE::ERR_LOCK_NOT_HELD    // Cannot unlock - not lock owner
PLM_ERROR_CODE::ERR_CHECKOUT_FAILED  // Checkout operation failed
PLM_ERROR_CODE::ERR_CHECKIN_FAILED   // Checkin operation failed
PLM_ERROR_CODE::ERR_NETWORK_TIMEOUT  // Network timeout
PLM_ERROR_CODE::ERR_NETWORK_UNREACHABLE // Network unreachable
PLM_ERROR_CODE::ERR_PERMISSION_DENIED // Access denied
```

## Testing Your Plugin

Use the conformance test framework to verify your plugin:

```cpp
#include <boost/test/unit_test.hpp>
#include "plm_conformance_framework.h"
#include "my_plm_plugin.h"

BOOST_AUTO_TEST_SUITE( MyPLMPluginTests )

BOOST_AUTO_TEST_CASE( PluginInfoValid )
{
    MY_PLM_PLUGIN plugin;
    PLM_PLUGIN_INFO info = plugin.GetPluginInfo();

    BOOST_CHECK( !info.plugin_id.IsEmpty() );
    BOOST_CHECK( !info.name.IsEmpty() );
    BOOST_CHECK( info.version.IsValid() );
}

BOOST_AUTO_TEST_CASE( AuthenticationFlow )
{
    MY_PLM_PLUGIN plugin;
    plugin.OnLoad();

    BOOST_CHECK( !plugin.IsAuthenticated() );

    PLM_CREDENTIALS creds;
    creds.server_url = wxS( "https://test-plm.example.com" );
    creds.username = wxS( "testuser" );
    creds.password = PLM_SECURE_STRING( wxS( "testpass" ) );

    PLM_AUTH_RESULT result = plugin.Authenticate( creds );

    // Check based on your test environment
    if( result.success )
    {
        BOOST_CHECK( plugin.IsAuthenticated() );
        BOOST_CHECK( plugin.Logout() );
        BOOST_CHECK( !plugin.IsAuthenticated() );
    }

    plugin.OnUnload();
}

BOOST_AUTO_TEST_SUITE_END()
```

## Distribution

### Plugin Naming Convention

Name your plugin library file with the prefix `plm_`:
- Windows: `plm_my_system.dll`
- Linux: `plm_my_system.so`
- macOS: `plm_my_system.dylib`

### Installation Location

Install plugins to KiCad's PLM plugin directory:
- Windows: `C:\Program Files\KiCad\<version>\share\kicad\plugins\plm\`
- Linux: `/usr/share/kicad/plugins/plm/` or `~/.local/share/kicad/plugins/plm/`
- macOS: `/Applications/KiCad/KiCad.app/Contents/SharedSupport/plugins/plm/`

### Package Contents

Include in your distribution:
- Plugin shared library
- README with installation instructions
- License file
- Configuration template (if needed)

## Best Practices

1. **Handle Network Errors Gracefully** - PLM systems are network-dependent. Always handle
   timeouts, connection failures, and retry logic.

2. **Cache Frequently Accessed Data** - User display names, folder structures, and file
   metadata can be cached to improve responsiveness.

3. **Secure Credential Handling** - Never log or store passwords in plain text. Use
   `PLM_SECURE_STRING` for sensitive data.

4. **Thread Safety** - Plugin methods may be called from multiple threads. Protect shared
   state with appropriate synchronization.

5. **Version Compatibility** - Check `PLM_API_VERSION_MAJOR` to ensure compatibility with
   the KiCad version loading your plugin.

6. **Meaningful Error Messages** - Return descriptive error messages that help users
   understand and resolve issues.

## API Reference

See the Doxygen-generated documentation for complete API details:
- `PLM_PLUGIN` - Base class with all virtual methods
- `PLM_CAPABILITIES` - Capability flags structure
- `PLM_AUTH_RESULT`, `PLM_CHECKOUT_RESULT`, etc. - Result types

Include `<plm/plm_api.h>` for access to all PLM types and interfaces.
