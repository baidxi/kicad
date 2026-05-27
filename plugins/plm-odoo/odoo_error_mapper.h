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
 * @file odoo_error_mapper.h
 * @brief Error mapping utilities for OdooPLM plugin.
 *
 * Maps Odoo JSON-RPC errors and HTTP status codes to PLM error codes,
 * providing consistent error handling across all plugin operations.
 */

#ifndef ODOO_ERROR_MAPPER_H
#define ODOO_ERROR_MAPPER_H

#include <plm/plm_types.h>
#include "odoo_rpc_client.h"


/**
 * Utility class for mapping Odoo errors to PLM error codes.
 */
class OdooErrorMapper
{
public:
    /**
     * Map an HTTP status code to a PLM error code.
     * @param aHttpStatus HTTP status code.
     * @return Corresponding PLM error code.
     */
    static PLM_ERROR_CODE FromHttpStatus( int aHttpStatus );

    /**
     * Map an Odoo RPC error to a PLM error code.
     * @param aRpcResult The RPC result containing error information.
     * @return Corresponding PLM error code.
     */
    static PLM_ERROR_CODE FromRpcResult( const ODOO_RPC_RESULT& aRpcResult );

    /**
     * Map a CURL error code to a PLM error code.
     * @param aCurlError CURL error code from Perform().
     * @return Corresponding PLM error code.
     */
    static PLM_ERROR_CODE FromCurlError( int aCurlError );

    /**
     * Get a user-friendly error message for an RPC result.
     * @param aRpcResult The RPC result.
     * @return Formatted error message suitable for display.
     */
    static wxString FormatErrorMessage( const ODOO_RPC_RESULT& aRpcResult );

    /**
     * Check if an RPC result indicates an authentication error.
     * @param aRpcResult The RPC result to check.
     * @return True if the error is authentication-related.
     */
    static bool IsAuthError( const ODOO_RPC_RESULT& aRpcResult );

    /**
     * Check if an RPC result indicates a network error.
     * @param aRpcResult The RPC result to check.
     * @return True if the error is network-related.
     */
    static bool IsNetworkError( const ODOO_RPC_RESULT& aRpcResult );

    /**
     * Check if an RPC result indicates a permission error.
     * @param aRpcResult The RPC result to check.
     * @return True if the error is permission-related.
     */
    static bool IsPermissionError( const ODOO_RPC_RESULT& aRpcResult );

    /**
     * Check if an RPC result indicates a not-found error.
     * @param aRpcResult The RPC result to check.
     * @return True if the requested resource was not found.
     */
    static bool IsNotFoundError( const ODOO_RPC_RESULT& aRpcResult );

    /**
     * Format a user-friendly permission denied message for a specific action.
     * @param aAction Description of the attempted action (e.g. "check in files").
     * @param aRpcResult The RPC result containing error details.
     * @return Formatted message suitable for display.
     */
    static wxString FormatPermissionError( const wxString&        aAction,
                                           const ODOO_RPC_RESULT& aRpcResult );
};

#endif // ODOO_ERROR_MAPPER_H
