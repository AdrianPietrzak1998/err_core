/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Author: Adrian Pietrzak
 * GitHub: https://github.com/AdrianPietrzak1998
 * Created: Jun 06, 2025
 */

#ifndef ERR_CORE_ERR_CORE_H_
#define ERR_CORE_ERR_CORE_H_

#include "limits.h"
#include "stddef.h"
#include "stdint.h"


/**
 * @def EC_TICK_FROM_FUNC
 * @brief Enables system tick retrieval via a function call.
 *
 * If set to 1, the system time base is obtained by calling a function
 * that returns the current tick count.
 * If set to 0, the system time base is read directly from a variable.
 */
#define EC_TICK_FROM_FUNC 0

/**
 * @brief System time base type and maximum timeout definition.
 *
 * By default, the system tick type is `volatile uint32_t`.
 * If you want to use a different type, define `CC_TIME_BASE_TYPE_CUSTOM` as that type
 * and also define the corresponding `_IS_` macro
 * (e.g., `EC_TIME_BASE_TYPE_CUSTOM_IS_UINT16`) to properly set `EC_MAX_TIMEOUT`.
 */

#ifndef EC_TIME_BASE_TYPE_CUSTOM

#define EC_MAX_TIMEOUT UINT32_MAX
typedef volatile uint32_t EC_TIME_t;

#else

typedef EC_TIME_BASE_TYPE_CUSTOM CC_TIME_t;

#if defined(EC_TIME_BASE_TYPE_CUSTOM_IS_UINT8)
#define EC_MAX_TIMEOUT UINT8_MAX
#elif defined(EC_TIME_BASE_TYPE_CUSTOM_IS_UINT16)
#define EC_MAX_TIMEOUT UINT16_MAX
#elif defined(EC_TIME_BASE_TYPE_CUSTOM_IS_UINT32)
#define EC_MAX_TIMEOUT UINT32_MAX
#elif defined(EC_TIME_BASE_TYPE_CUSTOM_IS_UINT64)
#define EC_MAX_TIMEOUT UINT64_MAX
#elif defined(EC_TIME_BASE_TYPE_CUSTOM_IS_INT8)
#define EC_MAX_TIMEOUT INT8_MAX
#elif defined(EC_TIME_BASE_TYPE_CUSTOM_IS_INT16)
#define EC_MAX_TIMEOUT INT16_MAX
#elif defined(EC_TIME_BASE_TYPE_CUSTOM_IS_INT32)
#define EC_MAX_TIMEOUT INT32_MAX
#elif defined(EC_TIME_BASE_TYPE_CUSTOM_IS_INT64)
#define EC_MAX_TIMEOUT INT64_MAX
#else
#error "EC_MAX_TIMEOUT: Unknown EC_TIME_BASE_TYPE_CUSTOM or missing _IS_* define"
#endif

#endif

/**
 * @enum EC_err_state_t
 * @brief Indicates whether an error condition is currently present.
 *
 * EC_NERR - No error is present.
 * EC_ERR  - An error has been detected.
 */
typedef enum
{
    EC_NERR = 0,
    EC_ERR = 1
} EC_err_state_t;

/**
 * @struct EC_timestamps_t
 * @brief Structure for storing timestamps related to an error's state.
 *
 * LastReg   - Timestamp when the error condition was last confirmed and registered.
 * LastNoErr - Timestamp when the error condition was last not present (no error).
 */
typedef struct
{
    EC_TIME_t LastReg;
    EC_TIME_t LastNoErr;
} EC_timestamps_t;

/**
 * @struct EC_error_t
 * @brief Definition of a single error condition.
 *
 * ErrFunc            - Function pointer returning current error state.
 * HelperNumber       - Argument passed to the error function (e.g., sensor index).
 * TimeToErrorRegister - Time (in ticks) the error condition must persist to be registered.
 * Timestamp          - Runtime timestamps for error registration and last non-error state.
 */
typedef struct
{
    EC_err_state_t (*ErrFunc)(uint16_t HelperNumber);
    uint16_t HelperNumber;
    EC_TIME_t TimeToErrorRegister;

//    EC_timestamps_t Timestamp;

} EC_error_t;

/**
 * @struct EC_instance_t
 * @brief Instance managing a set of error conditions.
 *
 * ErrorRegs      - Bitfield (up to 64 bits) representing current error states.
 * Errors         - Pointer to an array of error definitions.
 * Timestamp      - Pointer to an array of timing variables.
 * NumberOfErrors - Size of the Errors array.
 */
typedef struct
{
    uint64_t ErrorRegs;
    const EC_error_t *Errors;
    EC_timestamps_t *Timestamp;
    uint8_t NumberOfErrors;
} EC_instance_t;

#if EC_TICK_FROM_FUNC
/**
 * @brief Registers the system tick source function.
 *
 * When EC_TICK_FROM_FUNC is set to 1, this function registers a user-provided
 * function that returns the current system tick value.
 *
 * @param Function Pointer to a function that returns the current system tick (EC_TIME_t).
 */
void EC_tick_function_register(EC_TIME_t (*Function)(void));
#else
/**
 * @brief Registers the system tick source variable.
 *
 * When EC_TICK_FROM_FUNC is set to 0, this function registers a pointer
 * to a variable that holds the current system tick value.
 *
 * @param Variable Pointer to a volatile variable of type EC_TIME_t representing the system tick.
 */
void EC_tick_variable_register(EC_TIME_t *Variable);
#endif

/**
 * @brief Initializes an error control instance.
 *
 * @param Instance Pointer to the instance to be initialized.
 * @param Errors Pointer to the array of error definitions.
 * @param Time Stamps Pointer to the array of timing variables.
 * @param NumberOfErrors Number of errors in the array.
 */
void EC_init(EC_instance_t *Instance, const EC_error_t *Errors, EC_timestamps_t *Timestamps, uint8_t NumberOfErrors);

/**
 * @brief Should be called periodically in the main loop to check and register errors.
 *
 * @param Instance Pointer to the error control instance.
 */
void EC_poll(EC_instance_t *Instance);

/**
 * @brief Returns the current error register as a 64-bit bitfield.
 *
 * Each bit represents the state of a specific error.
 *
 * @param Instance Pointer to the error control instance.
 * @return 64-bit bitfield with the status of all errors.
 */
uint64_t EC_getErrors(EC_instance_t *Instance);

/**
 * @brief Returns the state of a specific error.
 *
 * @param Instance Pointer to the error control instance.
 * @param ErrorNumber Index of the error to query.
 * @return EC_ERR if the error is present, EC_NERR otherwise.
 */
EC_err_state_t EC_getOneError(EC_instance_t *Instance, uint8_t ErrorNumber);

/**
 * @brief Forces a check and registration of a specific error, bypassing the registration delay.
 *
 * Useful for immediate error validation regardless of the configured registration time.
 *
 * @param Instance Pointer to the error control instance.
 * @param ErrorNumber Index of the error to force-check.
 * @return EC_ERR if the error is present during the forced check, EC_NERR otherwise.
 */
EC_err_state_t EC_checkError(EC_instance_t *Instance, uint8_t ErrorNumber);

/**
 * @brief Clears all errors in the error register.
 *
 * This function resets the internal 64-bit register holding the error states.
 *
 * @param Instance Pointer to the error control instance.
 */
void EC_clearErr(EC_instance_t *Instance);

#endif /* ERR_CORE_ERR_CORE_H_ */
