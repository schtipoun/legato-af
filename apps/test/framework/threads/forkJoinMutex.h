// -------------------------------------------------------------------------------------------------
// Header file for thread creating and joining plus mutex tests.  These functions are called by the
// main module (main.c).
//
// Copyright (C) 2013, Sierra Wireless Inc.
// -------------------------------------------------------------------------------------------------

#ifndef LE_FORK_JOIN_MUTEX_TEST_H_INCLUSION_GUARD
#define LE_FORK_JOIN_MUTEX_TEST_H_INCLUSION_GUARD

// -------------------------------------------------------------------------------------------------
/**
 * Initializes the Create/Join/Mutex tests.
 *
 * Increments the use count on a given memory pool object, then releases it when the test is
 * complete.
 */
// -------------------------------------------------------------------------------------------------
void fjm_Init
(
    void* objPtr    ///< [in] Pointer to the object whose reference count is used to signal
                    ///       the completion of the test.
);

// -------------------------------------------------------------------------------------------------
/**
 * Checks the completion status of the Create/Join/Mutex tests.
 *
 * @return  LE_OK if everything looks good.
 */
// -------------------------------------------------------------------------------------------------
le_result_t fjm_CheckResults
(
    void
);

#endif // LE_FORK_JOIN_MUTEX_TEST_H_INCLUSION_GUARD
