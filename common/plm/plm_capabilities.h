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

/**
 * @file plm_capabilities.h
 * @brief Capability and plugin information structures for the PLM client interface.
 *
 * This header defines structures for describing PLM plugin capabilities
 * and metadata. Capabilities are runtime-queryable to allow the UI to
 * adapt to what features each PLM backend supports.
 */

#ifndef PLM_CAPABILITIES_H
#define PLM_CAPABILITIES_H

#include "plm_types.h"

#include <wx/string.h>
#include <map>
#include <vector>


/**
 * Custom state definition for PLM systems with non-standard workflows.
 *
 * Allows PLM plugins to define additional states beyond the standard
 * EDITABLE/LOCKED/RELEASED categories.
 */
struct PLM_CUSTOM_STATE
{
    wxString           state_id;      ///< Unique state identifier
    wxString           display_name;  ///< Human-readable name
    wxString           description;   ///< Help text for the state
    PLM_STATE_CATEGORY category;      ///< Mapping to standard category
    wxString           icon_name;     ///< Icon resource name (optional)
    int                sort_order = 0; ///< Order in state lists

    PLM_CUSTOM_STATE() : category( PLM_STATE_CATEGORY::EDITABLE ) {}

    PLM_CUSTOM_STATE( const wxString& aId, const wxString& aDisplayName,
                      PLM_STATE_CATEGORY aCategory ) :
            state_id( aId ),
            display_name( aDisplayName ),
            category( aCategory )
    {
    }
};


/**
 * Capabilities supported by a PLM plugin.
 *
 * Each capability flag indicates whether a particular feature is
 * available. The UI uses these to enable/disable menu items and
 * show/hide UI elements appropriately.
 */
struct PLM_CAPABILITIES
{
    // Core features
    bool supports_checkout = true;         ///< Can check out files for editing
    bool supports_locking = true;          ///< Supports file locking
    bool supports_revisions = true;        ///< Tracks revision history
    bool supports_attributes = true;       ///< Supports custom metadata attributes

    // Advanced features
    bool supports_eco = false;             ///< Engineering Change Order workflow
    bool supports_part_database = false;   ///< Integrated part/component database
    bool supports_bom_validation = false;  ///< Can validate BOMs against part DB
    bool supports_push_notifications = false; ///< Real-time update notifications
    bool supports_search = false;          ///< Server-side search functionality
    bool supports_preview_images = false;  ///< Thumbnail/preview generation
    bool supports_diff = false;            ///< Visual diff between revisions
    bool supports_merge = false;           ///< Three-way merge support
    bool supports_branching = false;       ///< Branch/variant management

    // Authentication
    bool supports_sso = false;             ///< Single sign-on integration
    bool supports_api_keys = false;        ///< API key authentication
    bool supports_oauth = false;           ///< OAuth 2.0 authentication

    // Offline capabilities
    bool supports_offline_mode = false;    ///< Can work without server connection
    bool supports_sync = false;            ///< Background synchronization

    // ECO/ECR lifecycle
    bool supports_eco_create = false;      ///< Can create ECOs and ECRs
    bool supports_eco_transitions = false; ///< Can transition ECO status
    bool supports_eco_items = false;       ///< Per-item tracking within ECOs
    bool supports_eco_impact = false;      ///< ECO impact assessment
    bool supports_ecr_to_eco = false;      ///< Can convert ECR to ECO

    // BOM and routing
    bool supports_bom_management = false;  ///< Full BOM CRUD operations
    bool supports_routing = false;         ///< Manufacturing routing management

    // Lifecycle state transitions
    bool supports_state_transitions = false; ///< Formal lifecycle state transitions

    // Review and forge
    bool supports_review = false;          ///< Design review workflows
    bool supports_forge_api = false;       ///< Git forge API integration

    // Part management
    bool supports_part_qualification = false; ///< Part qualification workflows
    bool supports_where_used = false;      ///< Where-used queries
    bool supports_vendor_management = false; ///< Vendor source management

    // Variants and release
    bool supports_variants = false;        ///< Product variant management
    bool supports_release_packaging = false; ///< Manufacturing release packaging

    // CI/CD and audit
    bool supports_ci_cd = false;           ///< CI/CD pipeline integration
    bool supports_audit_trail = false;     ///< Audit trail queries

    // File operations
    bool supports_folder_creation = false; ///< Can create folders in PLM
    bool supports_project_creation = false; ///< Can create new projects
    bool supports_cloning = false;         ///< Can clone projects
    bool supports_file_level_locking = false; ///< Per-file lock granularity
    bool supports_progress_reporting = false; ///< Progress callbacks for long operations

    // Shared library management (subscription, version pinning, distribution)
    bool supports_shared_libraries = false; ///< Plugin implements GetAvailableLibraries / GetLibraryVersions / DownloadLibrary

    // Custom states beyond standard EDITABLE/LOCKED/RELEASED
    std::vector<PLM_CUSTOM_STATE> custom_states;

    PLM_CAPABILITIES() = default;

    /**
     * Check if a capability is supported.
     * @param aCapability Capability name to check.
     * @return True if the capability is enabled.
     */
    bool HasCapability( const wxString& aCapability ) const
    {
        if( aCapability == wxS( "checkout" ) )          return supports_checkout;
        if( aCapability == wxS( "locking" ) )           return supports_locking;
        if( aCapability == wxS( "revisions" ) )         return supports_revisions;
        if( aCapability == wxS( "attributes" ) )        return supports_attributes;
        if( aCapability == wxS( "eco" ) )               return supports_eco;
        if( aCapability == wxS( "part_database" ) )     return supports_part_database;
        if( aCapability == wxS( "bom_validation" ) )    return supports_bom_validation;
        if( aCapability == wxS( "push_notifications" ) ) return supports_push_notifications;
        if( aCapability == wxS( "search" ) )            return supports_search;
        if( aCapability == wxS( "preview_images" ) )    return supports_preview_images;
        if( aCapability == wxS( "diff" ) )              return supports_diff;
        if( aCapability == wxS( "merge" ) )             return supports_merge;
        if( aCapability == wxS( "branching" ) )         return supports_branching;
        if( aCapability == wxS( "sso" ) )               return supports_sso;
        if( aCapability == wxS( "api_keys" ) )          return supports_api_keys;
        if( aCapability == wxS( "oauth" ) )             return supports_oauth;
        if( aCapability == wxS( "offline_mode" ) )      return supports_offline_mode;
        if( aCapability == wxS( "sync" ) )              return supports_sync;
        if( aCapability == wxS( "eco_create" ) )        return supports_eco_create;
        if( aCapability == wxS( "eco_transitions" ) )   return supports_eco_transitions;
        if( aCapability == wxS( "eco_items" ) )         return supports_eco_items;
        if( aCapability == wxS( "eco_impact" ) )        return supports_eco_impact;
        if( aCapability == wxS( "ecr_to_eco" ) )        return supports_ecr_to_eco;
        if( aCapability == wxS( "bom_management" ) )    return supports_bom_management;
        if( aCapability == wxS( "routing" ) )           return supports_routing;
        if( aCapability == wxS( "state_transitions" ) ) return supports_state_transitions;
        if( aCapability == wxS( "review" ) )            return supports_review;
        if( aCapability == wxS( "forge_api" ) )         return supports_forge_api;
        if( aCapability == wxS( "part_qualification" ) ) return supports_part_qualification;
        if( aCapability == wxS( "where_used" ) )        return supports_where_used;
        if( aCapability == wxS( "vendor_management" ) ) return supports_vendor_management;
        if( aCapability == wxS( "variants" ) )          return supports_variants;
        if( aCapability == wxS( "release_packaging" ) ) return supports_release_packaging;
        if( aCapability == wxS( "ci_cd" ) )             return supports_ci_cd;
        if( aCapability == wxS( "audit_trail" ) )       return supports_audit_trail;
        if( aCapability == wxS( "folder_creation" ) )   return supports_folder_creation;
        if( aCapability == wxS( "project_creation" ) )  return supports_project_creation;
        if( aCapability == wxS( "cloning" ) )           return supports_cloning;
        if( aCapability == wxS( "file_level_locking" ) ) return supports_file_level_locking;
        if( aCapability == wxS( "progress_reporting" ) ) return supports_progress_reporting;
        if( aCapability == wxS( "shared_libraries" ) )   return supports_shared_libraries;

        return false;
    }

    /**
     * Get list of all enabled capabilities.
     * @return Vector of capability names that are enabled.
     */
    std::vector<wxString> GetEnabledCapabilities() const
    {
        std::vector<wxString> result;

        if( supports_checkout )          result.push_back( wxS( "checkout" ) );
        if( supports_locking )           result.push_back( wxS( "locking" ) );
        if( supports_revisions )         result.push_back( wxS( "revisions" ) );
        if( supports_attributes )        result.push_back( wxS( "attributes" ) );
        if( supports_eco )               result.push_back( wxS( "eco" ) );
        if( supports_part_database )     result.push_back( wxS( "part_database" ) );
        if( supports_bom_validation )    result.push_back( wxS( "bom_validation" ) );
        if( supports_push_notifications ) result.push_back( wxS( "push_notifications" ) );
        if( supports_search )            result.push_back( wxS( "search" ) );
        if( supports_preview_images )    result.push_back( wxS( "preview_images" ) );
        if( supports_diff )              result.push_back( wxS( "diff" ) );
        if( supports_merge )             result.push_back( wxS( "merge" ) );
        if( supports_branching )         result.push_back( wxS( "branching" ) );
        if( supports_sso )               result.push_back( wxS( "sso" ) );
        if( supports_api_keys )          result.push_back( wxS( "api_keys" ) );
        if( supports_oauth )             result.push_back( wxS( "oauth" ) );
        if( supports_offline_mode )      result.push_back( wxS( "offline_mode" ) );
        if( supports_sync )              result.push_back( wxS( "sync" ) );
        if( supports_eco_create )        result.push_back( wxS( "eco_create" ) );
        if( supports_eco_transitions )   result.push_back( wxS( "eco_transitions" ) );
        if( supports_eco_items )         result.push_back( wxS( "eco_items" ) );
        if( supports_eco_impact )        result.push_back( wxS( "eco_impact" ) );
        if( supports_ecr_to_eco )        result.push_back( wxS( "ecr_to_eco" ) );
        if( supports_bom_management )    result.push_back( wxS( "bom_management" ) );
        if( supports_routing )           result.push_back( wxS( "routing" ) );
        if( supports_state_transitions ) result.push_back( wxS( "state_transitions" ) );
        if( supports_review )            result.push_back( wxS( "review" ) );
        if( supports_forge_api )         result.push_back( wxS( "forge_api" ) );
        if( supports_part_qualification ) result.push_back( wxS( "part_qualification" ) );
        if( supports_where_used )        result.push_back( wxS( "where_used" ) );
        if( supports_vendor_management ) result.push_back( wxS( "vendor_management" ) );
        if( supports_variants )          result.push_back( wxS( "variants" ) );
        if( supports_release_packaging ) result.push_back( wxS( "release_packaging" ) );
        if( supports_ci_cd )             result.push_back( wxS( "ci_cd" ) );
        if( supports_audit_trail )       result.push_back( wxS( "audit_trail" ) );
        if( supports_folder_creation )   result.push_back( wxS( "folder_creation" ) );
        if( supports_project_creation )  result.push_back( wxS( "project_creation" ) );
        if( supports_cloning )           result.push_back( wxS( "cloning" ) );
        if( supports_file_level_locking ) result.push_back( wxS( "file_level_locking" ) );
        if( supports_progress_reporting ) result.push_back( wxS( "progress_reporting" ) );
        if( supports_shared_libraries )  result.push_back( wxS( "shared_libraries" ) );

        return result;
    }

    /**
     * Find a custom state by ID.
     * @param aStateId State identifier to find.
     * @return Pointer to custom state, or nullptr if not found.
     */
    const PLM_CUSTOM_STATE* FindCustomState( const wxString& aStateId ) const
    {
        for( const auto& state : custom_states )
        {
            if( state.state_id == aStateId )
                return &state;
        }

        return nullptr;
    }

    /**
     * Create capabilities for a basic PLM system.
     * @return Capabilities with core features only.
     */
    static PLM_CAPABILITIES Basic()
    {
        return PLM_CAPABILITIES();
    }

    /**
     * Create capabilities for a full-featured PLM system.
     * @return Capabilities with all features enabled.
     */
    static PLM_CAPABILITIES Full()
    {
        PLM_CAPABILITIES caps;
        caps.supports_eco = true;
        caps.supports_part_database = true;
        caps.supports_bom_validation = true;
        caps.supports_push_notifications = true;
        caps.supports_search = true;
        caps.supports_preview_images = true;
        caps.supports_diff = true;
        caps.supports_merge = true;
        caps.supports_branching = true;
        caps.supports_sso = true;
        caps.supports_api_keys = true;
        caps.supports_oauth = true;
        caps.supports_offline_mode = true;
        caps.supports_sync = true;
        caps.supports_eco_create = true;
        caps.supports_eco_transitions = true;
        caps.supports_eco_items = true;
        caps.supports_eco_impact = true;
        caps.supports_ecr_to_eco = true;
        caps.supports_bom_management = true;
        caps.supports_routing = true;
        caps.supports_state_transitions = true;
        caps.supports_review = true;
        caps.supports_forge_api = true;
        caps.supports_part_qualification = true;
        caps.supports_where_used = true;
        caps.supports_vendor_management = true;
        caps.supports_variants = true;
        caps.supports_release_packaging = true;
        caps.supports_ci_cd = true;
        caps.supports_audit_trail = true;
        caps.supports_folder_creation = true;
        caps.supports_project_creation = true;
        caps.supports_cloning = true;
        caps.supports_file_level_locking = true;
        caps.supports_progress_reporting = true;
        return caps;
    }
};


/**
 * Semantic version for plugin compatibility checking.
 */
struct PLM_VERSION
{
    int major = 0;  ///< Major version (breaking changes)
    int minor = 0;  ///< Minor version (new features)
    int patch = 0;  ///< Patch version (bug fixes)

    PLM_VERSION() = default;

    PLM_VERSION( int aMajor, int aMinor, int aPatch ) :
            major( aMajor ),
            minor( aMinor ),
            patch( aPatch )
    {
    }

    /**
     * Parse version from string (e.g., "1.2.3").
     * @param aVersionStr Version string to parse.
     * @return Parsed version, or 0.0.0 on parse failure.
     */
    static PLM_VERSION Parse( const wxString& aVersionStr )
    {
        PLM_VERSION ver;
        wxString str = aVersionStr;

        long val;

        int dotPos = str.Find( '.' );

        if( dotPos != wxNOT_FOUND )
        {
            if( str.Left( dotPos ).ToLong( &val ) )
                ver.major = static_cast<int>( val );

            str = str.Mid( dotPos + 1 );
            dotPos = str.Find( '.' );

            if( dotPos != wxNOT_FOUND )
            {
                if( str.Left( dotPos ).ToLong( &val ) )
                    ver.minor = static_cast<int>( val );

                if( str.Mid( dotPos + 1 ).ToLong( &val ) )
                    ver.patch = static_cast<int>( val );
            }
            else
            {
                if( str.ToLong( &val ) )
                    ver.minor = static_cast<int>( val );
            }
        }
        else
        {
            if( str.ToLong( &val ) )
                ver.major = static_cast<int>( val );
        }

        return ver;
    }

    /**
     * Convert to string representation.
     * @return Version as "major.minor.patch".
     */
    wxString ToString() const
    {
        return wxString::Format( wxS( "%d.%d.%d" ), major, minor, patch );
    }

    /**
     * Check if this version is compatible with a required version.
     * @param aRequired Minimum required version.
     * @return True if this version meets or exceeds the requirement.
     */
    bool IsCompatibleWith( const PLM_VERSION& aRequired ) const
    {
        if( major != aRequired.major )
            return major > aRequired.major;

        if( minor != aRequired.minor )
            return minor > aRequired.minor;

        return patch >= aRequired.patch;
    }

    bool operator==( const PLM_VERSION& aOther ) const
    {
        return major == aOther.major && minor == aOther.minor && patch == aOther.patch;
    }

    bool operator<( const PLM_VERSION& aOther ) const
    {
        if( major != aOther.major )
            return major < aOther.major;

        if( minor != aOther.minor )
            return minor < aOther.minor;

        return patch < aOther.patch;
    }
};


/**
 * Information about a PLM plugin.
 *
 * Provides metadata for plugin identification, compatibility checking,
 * and display in the UI.
 */
struct PLM_PLUGIN_INFO
{
    wxString        plugin_id;       ///< Unique plugin identifier
    wxString        display_name;    ///< Human-readable plugin name
    wxString        description;     ///< Plugin description
    wxString        vendor;          ///< Plugin vendor/author
    wxString        vendor_url;      ///< Vendor website
    wxString        support_url;     ///< Support/documentation URL
    wxString        icon_name;       ///< Icon resource name
    PLM_VERSION     version;         ///< Plugin version
    PLM_VERSION     min_kicad_version; ///< Minimum KiCad version required
    PLM_CAPABILITIES capabilities;   ///< Supported features

    PLM_PLUGIN_INFO() = default;

    PLM_PLUGIN_INFO( const wxString& aId, const wxString& aDisplayName,
                     const PLM_VERSION& aVersion ) :
            plugin_id( aId ),
            display_name( aDisplayName ),
            version( aVersion )
    {
    }

    /**
     * Get full version string including plugin name.
     * @return String like "PluginName 1.2.3".
     */
    wxString GetVersionString() const
    {
        return display_name + wxS( " " ) + version.ToString();
    }

    /**
     * Check if plugin is compatible with a KiCad version.
     * @param aKicadVersion Current KiCad version.
     * @return True if plugin can run on this KiCad version.
     */
    bool IsCompatibleWithKiCad( const PLM_VERSION& aKicadVersion ) const
    {
        return aKicadVersion.IsCompatibleWith( min_kicad_version );
    }
};


#endif // PLM_CAPABILITIES_H
