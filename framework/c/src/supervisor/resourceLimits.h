//--------------------------------------------------------------------------------------------------
/** @file resourceLimits.h
 *
 * API for setting resource limits.
 *
 * Copyright (C) Sierra Wireless, Inc. 2013. All rights reserved. Use of this work is subject to license.
 */

#ifndef LEGATO_SRC_RESOURCE_LIMITS_INCLUDE_GUARD
#define LEGATO_SRC_RESOURCE_LIMITS_INCLUDE_GUARD


#include "app.h"
#include "proc.h"


//--------------------------------------------------------------------------------------------------
/**
 * Sets the resource limits for the specified application.
 *
 * @return
 *      LE_OK if successful.
 *      LE_FAULT if there was an error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t resLim_SetAppLimits
(
    app_Ref_t appRef                ///< [IN] The application to set resource limits for.
);


//--------------------------------------------------------------------------------------------------
/**
 * Sets the resource limits for the specified process.
 *
 * @return
 *      LE_OK if successful.
 *      LE_FAULT if there was an error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t resLim_SetProcLimits
(
    proc_Ref_t procRef              ///< [IN] The process to set resource limits for.
);


#endif  // LEGATO_SRC_RESOURCE_LIMITS_INCLUDE_GUARD
