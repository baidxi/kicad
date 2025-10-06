/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
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

/**
 * @file plm_api.h
 * @brief PLM Plugin API documentation entry point.
 *
 * This header provides the main documentation for the KiCad PLM Plugin API.
 * Include this header to access all PLM functionality.
 */

/**
 * @defgroup plm PLM Plugin System
 * @brief Product Lifecycle Management integration for KiCad.
 *
 * The PLM Plugin System provides an extensible framework for integrating KiCad
 * with external Product Lifecycle Management (PLM) systems. This allows design
 * teams to manage KiCad projects within their existing PLM infrastructure.
 *
 * ## Architecture Overview
 *
 * The PLM system consists of several layers:
 *
 * - **Plugin Interface** - Abstract base class PLM_PLUGIN that vendors implement
 * - **Plugin Manager** - Handles plugin discovery, loading, and lifecycle
 * - **Cache Layer** - Local caching for offline access and performance
 * - **UI Components** - Browser panel and dialogs for user interaction
 *
 * ## Key Concepts
 *
 * ### Authentication
 * PLM systems require authentication before accessing projects. The authentication
 * layer supports various methods including username/password, tokens, and OAuth2.
 * See PLM_CREDENTIALS and PLM_AUTH_RESULT for details.
 *
 * ### File Operations
 * The core workflow involves checking out projects (PLM_CHECKOUT_RESULT),
 * making changes in KiCad, and checking them back in (PLM_CHECKIN_RESULT).
 * Optimistic and pessimistic locking strategies are supported.
 *
 * ### State Management
 * Projects move through lifecycle states (PLM_STATE_CATEGORY) such as editable,
 * in-review, and released. State transitions are controlled by PLM policies.
 *
 * ### Plugin Development
 * To create a PLM plugin, inherit from PLM_PLUGIN and implement the required
 * virtual methods. See the Plugin Author Guide for detailed instructions.
 *
 * ## Module Structure
 *
 * - @ref plm_types "Data Types" - Core enumerations and structures
 * - @ref plm_auth "Authentication" - Session and credential management
 * - @ref plm_operations "Operations" - File checkout, checkin, and locking
 * - @ref plm_revision "Revisions" - Version history and comparison
 * - @ref plm_plugin_interface "Plugin Interface" - Base class for plugins
 *
 * @{
 */

/**
 * @defgroup plm_types PLM Data Types
 * @brief Core data structures for PLM operations.
 *
 * These types define the data exchanged between KiCad and PLM plugins.
 * All structures are designed for efficient serialization and network transfer.
 */

/**
 * @defgroup plm_auth PLM Authentication
 * @brief Authentication and session management.
 *
 * Handles user authentication, session lifecycle, and credential storage.
 * Supports multiple authentication methods through PLM_AUTH_METHOD.
 */

/**
 * @defgroup plm_operations PLM File Operations
 * @brief Checkout, checkin, and locking operations.
 *
 * File operations are the core workflow for PLM integration. Projects are
 * checked out for editing, modifications are saved locally, and changes
 * are checked back in to create new revisions.
 */

/**
 * @defgroup plm_revision PLM Revision Management
 * @brief Version history and revision comparison.
 *
 * Tracks the complete history of project changes with author attribution,
 * timestamps, and comments. Supports revision comparison for review workflows.
 */

/**
 * @defgroup plm_plugin_interface PLM Plugin Interface
 * @brief Base class and supporting types for plugin implementation.
 *
 * Plugin developers inherit from PLM_PLUGIN and implement the required
 * methods to connect KiCad with their PLM system.
 */

/** @} */ // end of plm group

#ifndef PLM_API_H
#define PLM_API_H

// Core types and enumerations
#include <plm/plm_types.h>

// Authentication and session management
#include <plm/plm_auth.h>
#include <plm/plm_session.h>
#include <plm/plm_credentials.h>

// File operations
#include <plm/plm_file_info.h>
#include <plm/plm_checkout.h>
#include <plm/plm_checkin.h>
#include <plm/plm_lock.h>

// Navigation
#include <plm/plm_folder.h>
#include <plm/plm_list.h>

// Revision management
#include <plm/plm_revision.h>

// State management
#include <plm/plm_state.h>
#include <plm/plm_state_mapping.h>

// Schema and attributes
#include <plm/plm_schema.h>
#include <plm/plm_attributes.h>

// Permissions
#include <plm/plm_permissions.h>

// Plugin interface
#include <plm/plm_plugin.h>
#include <plm/plm_plugin_info.h>
#include <plm/plm_capabilities.h>

// Plugin manager
#include <plm/plm_plugin_manager.h>

// Cache layer
#include <plm/plm_cache.h>
#include <plm/plm_part_cache.h>

// UI components (when available)
// #include <plm/plm_browser_panel.h>

/**
 * @example plm_plugin_example.cpp
 * @brief Example PLM plugin implementation.
 *
 * This example shows how to implement a basic PLM plugin that connects
 * to a REST API backend.
 *
 * @code{.cpp}
 * class MY_PLM_PLUGIN : public PLM_PLUGIN
 * {
 * public:
 *     PLM_PLUGIN_INFO GetPluginInfo() const override
 *     {
 *         PLM_PLUGIN_INFO info;
 *         info.plugin_id = wxS( "my-plm" );
 *         info.name = wxS( "My PLM System" );
 *         info.vendor = wxS( "My Company" );
 *         info.version = PLM_VERSION( 1, 0, 0 );
 *         return info;
 *     }
 *
 *     PLM_CAPABILITIES GetCapabilities() const override
 *     {
 *         PLM_CAPABILITIES caps;
 *         caps.supports_checkout = true;
 *         caps.supports_checkin = true;
 *         caps.supports_locking = true;
 *         caps.supports_revisions = true;
 *         return caps;
 *     }
 *
 *     PLM_AUTH_RESULT Authenticate( const PLM_CREDENTIALS& aCredentials ) override
 *     {
 *         // Implement authentication logic
 *         // Call your PLM system's REST API
 *         // Return PLM_AUTH_RESULT::Success() or PLM_AUTH_RESULT::Failure()
 *     }
 *
 *     // Implement remaining virtual methods...
 * };
 * @endcode
 */

#endif // PLM_API_H
