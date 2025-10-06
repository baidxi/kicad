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
 * @file plm_permissions.h
 * @brief Permission and policy structures for the PLM client interface.
 *
 * This header defines structures for managing user permissions and
 * organizational policies in the PLM system. Permissions control what
 * actions a user can perform, while policies define organizational rules.
 */

#ifndef PLM_PERMISSIONS_H
#define PLM_PERMISSIONS_H

#include <wx/string.h>
#include <set>
#include <vector>


/**
 * User permissions for PLM operations.
 *
 * Defines what actions the current user is allowed to perform on files
 * managed by the PLM system. These permissions are typically returned
 * by the server and may vary per project or file.
 */
struct PLM_PERMISSIONS
{
    bool can_checkout = false;          ///< Can check out files for editing
    bool can_checkin = false;           ///< Can check in modified files
    bool can_lock = false;              ///< Can acquire locks on files
    bool can_unlock = false;            ///< Can release locks (own locks)
    bool can_force_unlock = false;      ///< Can release locks held by others
    bool can_delete = false;            ///< Can delete files from PLM
    bool can_modify_attributes = false; ///< Can modify file attributes
    bool can_create_revision = false;   ///< Can create new revisions
    bool can_revert = false;            ///< Can revert to previous revisions
    bool can_view_history = false;      ///< Can view revision history
    bool can_compare = false;           ///< Can compare revisions
    bool can_transition_state = false;  ///< Can perform lifecycle state transitions
    bool can_release = false;           ///< Can release designs / create releases
    bool can_create_branch = false;     ///< Can create deviation branches
    bool can_manage_variants = false;   ///< Can create/modify product variants
    bool can_submit_review = false;     ///< Can submit designs for review
    bool can_review = false;            ///< Can review and approve/reject designs
    bool can_merge_review = false;      ///< Can merge approved reviews

    std::set<wxString> editable_attributes;  ///< Specific attributes user can edit

    PLM_PERMISSIONS() = default;

    /**
     * Create permissions with all access granted.
     * @return Permissions with all flags set to true.
     */
    static PLM_PERMISSIONS FullAccess()
    {
        PLM_PERMISSIONS perms;
        perms.can_checkout = true;
        perms.can_checkin = true;
        perms.can_lock = true;
        perms.can_unlock = true;
        perms.can_force_unlock = true;
        perms.can_delete = true;
        perms.can_modify_attributes = true;
        perms.can_create_revision = true;
        perms.can_revert = true;
        perms.can_view_history = true;
        perms.can_compare = true;
        perms.can_transition_state = true;
        perms.can_release = true;
        perms.can_create_branch = true;
        perms.can_manage_variants = true;
        perms.can_submit_review = true;
        perms.can_review = true;
        perms.can_merge_review = true;
        return perms;
    }

    /**
     * Create permissions for read-only access.
     * @return Permissions with only viewing allowed.
     */
    static PLM_PERMISSIONS ReadOnly()
    {
        PLM_PERMISSIONS perms;
        perms.can_view_history = true;
        perms.can_compare = true;
        return perms;
    }

    /**
     * Create permissions for standard editing access.
     * @return Permissions typical for regular users.
     */
    static PLM_PERMISSIONS StandardEdit()
    {
        PLM_PERMISSIONS perms;
        perms.can_checkout = true;
        perms.can_checkin = true;
        perms.can_lock = true;
        perms.can_unlock = true;
        perms.can_modify_attributes = true;
        perms.can_view_history = true;
        perms.can_compare = true;
        perms.can_submit_review = true;
        perms.can_review = true;
        return perms;
    }

    /**
     * Check if user can edit a specific attribute.
     * @param aAttrId Attribute ID to check.
     * @return True if attribute modification is allowed.
     */
    bool CanEditAttribute( const wxString& aAttrId ) const
    {
        if( !can_modify_attributes )
            return false;

        if( editable_attributes.empty() )
            return true;

        return editable_attributes.find( aAttrId ) != editable_attributes.end();
    }

    /**
     * Check if user has any write permissions.
     * @return True if any modification action is allowed.
     */
    bool HasWriteAccess() const
    {
        return can_checkout || can_checkin || can_lock || can_modify_attributes ||
               can_create_revision || can_delete || can_transition_state || can_release;
    }
};


/**
 * Organizational policy for PLM usage.
 *
 * Defines rules and restrictions that apply to all users within an
 * organization. Policies are typically configured on the PLM server
 * and enforced by both server and client.
 */
struct PLM_POLICY
{
    bool mandatory_server = false;       ///< All projects must use PLM (no local-only)
    bool allow_local_projects = true;    ///< Users can create non-PLM projects
    bool require_eco = false;            ///< Changes require ECO approval
    bool require_lock_before_edit = false;  ///< Must lock before modifying
    bool require_comment_on_checkin = false; ///< Checkin requires description
    bool allow_force_unlock = false;     ///< Users can break others' locks
    bool require_approval_for_release = false; ///< Release needs approval

    std::set<wxString> locked_settings;  ///< Settings that users cannot change

    wxString default_server_url;         ///< Default PLM server URL
    wxString organization_name;          ///< Organization identifier

    int session_timeout_minutes = 480;   ///< Session timeout (default 8 hours)
    int lock_timeout_hours = 24;         ///< Lock auto-release timeout

    PLM_POLICY() = default;

    /**
     * Check if a setting is locked by policy.
     * @param aSettingId Setting identifier to check.
     * @return True if the setting cannot be modified by users.
     */
    bool IsSettingLocked( const wxString& aSettingId ) const
    {
        return locked_settings.find( aSettingId ) != locked_settings.end();
    }

    /**
     * Check if local-only projects are permitted.
     * @return True if users can work without PLM.
     */
    bool AllowsLocalProjects() const
    {
        return allow_local_projects && !mandatory_server;
    }

    /**
     * Create a permissive policy with minimal restrictions.
     * @return Policy allowing most operations.
     */
    static PLM_POLICY Permissive()
    {
        PLM_POLICY policy;
        policy.allow_local_projects = true;
        policy.allow_force_unlock = true;
        return policy;
    }

    /**
     * Create a restrictive policy for controlled environments.
     * @return Policy with strict controls enabled.
     */
    static PLM_POLICY Restrictive()
    {
        PLM_POLICY policy;
        policy.mandatory_server = true;
        policy.allow_local_projects = false;
        policy.require_eco = true;
        policy.require_lock_before_edit = true;
        policy.require_comment_on_checkin = true;
        policy.allow_force_unlock = false;
        policy.require_approval_for_release = true;
        return policy;
    }
};


/**
 * Combined permission and policy context.
 *
 * Represents the effective permissions and policies for the current
 * user session, combining user-specific permissions with organizational
 * policies.
 */
struct PLM_PERMISSION_CONTEXT
{
    PLM_PERMISSIONS user_permissions;  ///< User's direct permissions
    PLM_POLICY      org_policy;        ///< Organizational policy

    PLM_PERMISSION_CONTEXT() = default;

    PLM_PERMISSION_CONTEXT( const PLM_PERMISSIONS& aPerms, const PLM_POLICY& aPolicy ) :
            user_permissions( aPerms ),
            org_policy( aPolicy )
    {
    }

    /**
     * Check if checkout is effectively allowed.
     * Combines user permission with policy requirements.
     */
    bool CanCheckout() const
    {
        return user_permissions.can_checkout;
    }

    /**
     * Check if modification requires locking first.
     * @return True if lock-before-edit is required.
     */
    bool RequiresLockBeforeEdit() const
    {
        return org_policy.require_lock_before_edit;
    }

    /**
     * Check if checkin requires a comment.
     * @return True if description is mandatory.
     */
    bool RequiresCheckinComment() const
    {
        return org_policy.require_comment_on_checkin;
    }

    /**
     * Check if user can force-unlock another user's lock.
     * @return True if force unlock is allowed by both permissions and policy.
     */
    bool CanForceUnlock() const
    {
        return user_permissions.can_force_unlock && org_policy.allow_force_unlock;
    }
};


#endif // PLM_PERMISSIONS_H
