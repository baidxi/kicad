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
 * @file plm_policy_handler.h
 * @brief Handler for PLM organizational policy files.
 *
 * This handler manages system-wide PLM policy files that define organizational
 * rules and restrictions. Policy files are loaded from well-known system paths
 * and applied to enforce mandatory settings.
 */

#ifndef PLM_POLICY_HANDLER_H
#define PLM_POLICY_HANDLER_H

#include "plm_permissions.h"
#include "plm_types.h"

#include <wx/string.h>
#include <functional>
#include <memory>
#include <vector>


/**
 * Result of loading a policy file.
 */
struct PLM_POLICY_LOAD_RESULT
{
    bool        success = false;        ///< True if policy loaded successfully
    wxString    error_message;          ///< Error message if loading failed
    wxString    source_path;            ///< Path policy was loaded from
    PLM_POLICY  policy;                 ///< The loaded policy

    static PLM_POLICY_LOAD_RESULT Success( const wxString& aPath, const PLM_POLICY& aPolicy )
    {
        PLM_POLICY_LOAD_RESULT result;
        result.success = true;
        result.source_path = aPath;
        result.policy = aPolicy;
        return result;
    }

    static PLM_POLICY_LOAD_RESULT Failure( const wxString& aError )
    {
        PLM_POLICY_LOAD_RESULT result;
        result.success = false;
        result.error_message = aError;
        return result;
    }

    static PLM_POLICY_LOAD_RESULT NoPolicy()
    {
        PLM_POLICY_LOAD_RESULT result;
        result.success = true;
        return result;
    }
};


/**
 * Validation result for policy settings.
 */
struct PLM_POLICY_VALIDATION
{
    bool                     valid = true;      ///< True if validation passed
    std::vector<wxString>    errors;            ///< Validation error messages
    std::vector<wxString>    warnings;          ///< Validation warnings

    void AddError( const wxString& aError )
    {
        errors.push_back( aError );
        valid = false;
    }

    void AddWarning( const wxString& aWarning )
    {
        warnings.push_back( aWarning );
    }

    bool HasIssues() const
    {
        return !errors.empty() || !warnings.empty();
    }
};


/**
 * Callback for UI settings that should be locked by policy.
 *
 * @param aSettingId The setting identifier being locked.
 * @param aLocked True to lock the setting, false to unlock.
 */
using PLM_SETTING_LOCK_CALLBACK = std::function<void( const wxString& aSettingId, bool aLocked )>;


/**
 * Handler for PLM organizational policy files.
 *
 * Manages loading, validation, and enforcement of organizational PLM policies.
 * Policies are loaded from system configuration paths and define mandatory
 * settings that users cannot override.
 *
 * Policy files are JSON format with the following structure:
 * @code
 * {
 *     "organization": "Company Name",
 *     "policy_version": "1.0",
 *     "mandatory_server": true,
 *     "default_server_url": "https://plm.company.com",
 *     "require_eco": true,
 *     "require_lock_before_edit": true,
 *     "locked_settings": ["server_url", "plugin_id"],
 *     ...
 * }
 * @endcode
 *
 * Usage:
 * @code
 * PLM_POLICY_HANDLER handler;
 *
 * // Set callback to lock UI settings
 * handler.SetSettingLockCallback( [&]( const wxString& id, bool locked ) {
 *     myUI.SetControlEnabled( id, !locked );
 * });
 *
 * // Load and apply policy
 * auto result = handler.LoadPolicy();
 * if( result.success )
 * {
 *     handler.ApplyPolicy();
 *     PLM_POLICY policy = handler.GetPolicy();
 * }
 * @endcode
 */
class PLM_POLICY_HANDLER
{
public:
    PLM_POLICY_HANDLER();
    ~PLM_POLICY_HANDLER();

    /**
     * Load policy from standard system paths.
     *
     * Searches for policy files in order of precedence:
     * 1. KiCad installation config directory
     * 2. System-wide config directory (/etc/kicad, %ProgramData%/KiCad)
     * 3. User config directory
     *
     * @return Result containing the loaded policy or error information.
     */
    PLM_POLICY_LOAD_RESULT LoadPolicy();

    /**
     * Load policy from a specific file path.
     *
     * @param aPath Path to the policy file.
     * @return Result containing the loaded policy or error information.
     */
    PLM_POLICY_LOAD_RESULT LoadPolicyFromFile( const wxString& aPath );

    /**
     * Apply the currently loaded policy.
     *
     * Invokes setting lock callbacks for all locked settings in the policy.
     */
    void ApplyPolicy();

    /**
     * Get the current policy.
     *
     * @return The loaded policy, or default policy if none loaded.
     */
    const PLM_POLICY& GetPolicy() const { return m_policy; }

    /**
     * Check if a policy has been loaded.
     *
     * @return True if a policy file was successfully loaded.
     */
    bool HasPolicy() const { return m_policyLoaded; }

    /**
     * Get the path where the policy was loaded from.
     *
     * @return Path to the policy file, or empty if no policy loaded.
     */
    const wxString& GetPolicyPath() const { return m_policyPath; }

    /**
     * Check if a specific setting is locked by policy.
     *
     * @param aSettingId Setting identifier to check.
     * @return True if the setting is locked and cannot be changed.
     */
    bool IsSettingLocked( const wxString& aSettingId ) const;

    /**
     * Get all locked settings.
     *
     * @return Set of setting identifiers that are locked.
     */
    std::set<wxString> GetLockedSettings() const;

    /**
     * Validate the current policy for consistency.
     *
     * Checks for conflicting settings or invalid values.
     *
     * @return Validation result with any errors or warnings.
     */
    PLM_POLICY_VALIDATION ValidatePolicy() const;

    /**
     * Set callback for locking UI settings.
     *
     * @param aCallback Callback to invoke when settings should be locked/unlocked.
     */
    void SetSettingLockCallback( PLM_SETTING_LOCK_CALLBACK aCallback );

    /**
     * Get the standard policy search paths.
     *
     * @return Vector of paths that will be searched for policy files.
     */
    std::vector<wxString> GetPolicySearchPaths() const;

    /**
     * Get the policy file name.
     *
     * @return Standard name of the policy file (kicad_plm_policy.json).
     */
    static wxString GetPolicyFileName();

    /**
     * Check if a setting value is allowed by policy.
     *
     * @param aSettingId Setting identifier.
     * @param aValue Proposed value.
     * @return True if the value is allowed.
     */
    bool IsSettingValueAllowed( const wxString& aSettingId, const wxString& aValue ) const;

    /**
     * Get the mandatory value for a locked setting.
     *
     * @param aSettingId Setting identifier.
     * @return The mandatory value, or empty string if setting is not locked.
     */
    wxString GetMandatoryValue( const wxString& aSettingId ) const;

private:
    /**
     * Parse policy JSON data.
     *
     * @param aJsonData JSON string to parse.
     * @return Result with parsed policy or error.
     */
    PLM_POLICY_LOAD_RESULT parsePolicy( const wxString& aJsonData );

    /**
     * Find the first existing policy file in search paths.
     *
     * @return Path to policy file, or empty if none found.
     */
    wxString findPolicyFile() const;

    PLM_POLICY                  m_policy;           ///< Current policy
    bool                        m_policyLoaded;     ///< True if policy was loaded
    wxString                    m_policyPath;       ///< Path policy was loaded from
    PLM_SETTING_LOCK_CALLBACK   m_lockCallback;     ///< Callback for locking settings
    std::map<wxString, wxString> m_mandatoryValues; ///< Forced values for settings
};


#endif // PLM_POLICY_HANDLER_H
