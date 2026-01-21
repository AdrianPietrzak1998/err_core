/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Author: Adrian Pietrzak
 * GitHub: https://github.com/AdrianPietrzak1998
 * Created: Jun 06, 2025
 */

/**
 * @file err_core.h
 * @brief Error Core - Advanced error management system with debouncing and warning escalation
 *
 * @details
 * Error Core provides a sophisticated error handling mechanism for embedded systems,
 * featuring:
 * - Debouncing: Errors must persist for a defined time before registration
 * - Warning System: Graduated escalation from warnings to errors
 * - Flexible Configuration: Support for up to 64 simultaneous error conditions
 * - Timestamp Tracking: Complete history of error state changes
 * - Minimal Memory Footprint: Optimized for resource-constrained systems
 *
 * Typical Use Cases:
 * - Sensor validation and fault detection
 * - Communication timeout handling
 * - System health monitoring
 * - Safety-critical error management
 *
 * @example Basic Usage
 * @code
 * // Define error check function
 * EC_err_state_t check_temperature(uint16_t sensor_id) {
 *     return (temperature[sensor_id] > MAX_TEMP) ? EC_ERR : EC_NERR;
 * }
 *
 * // Configure errors
 * const EC_error_t errors[] = {
 *     {check_temperature, 0, 1000, 5000, 3}  // Sensor 0: 1s debounce, 5s warning reset, 3 warnings
 * };
 *
 * // Initialize
 * EC_runtimeData_t runtime[1] = {0};
 * EC_instance_t instance = {0};
 * EC_tick_variable_register(&system_tick);
 * EC_init(&instance, errors, runtime, 1);
 *
 * // Poll in main loop
 * while(1) {
 *     EC_poll(&instance);
 *     if (EC_getOneError(&instance, 0) == EC_ERR) {
 *         handle_temperature_error();
 *     }
 * }
 * @endcode
 *
 * @version 1.0.0
 * @date Jun 06, 2025
 */

#ifndef ERR_CORE_ERR_CORE_H_
#define ERR_CORE_ERR_CORE_H_

#include "limits.h"
#include "stddef.h"
#include "stdint.h"

/*******************************************************************************
 * CONFIGURATION MACROS
 ******************************************************************************/

/**
 * @def EC_TICK_FROM_FUNC
 * @brief Selects the system tick source method
 *
 * When set to 1, the system tick is retrieved by calling a user-registered function.
 * When set to 0, the system tick is read directly from a user-registered variable.
 *
 * Function-based approach:
 * - Pros: Thread-safe, can include additional logic
 * - Cons: Function call overhead
 *
 * Variable-based approach:
 * - Pros: Faster, direct memory access
 * - Cons: Must ensure variable is updated atomically
 *
 * @note Default: 0 (variable-based)
 */
#define EC_TICK_FROM_FUNC 0

/*******************************************************************************
 * TIME BASE CONFIGURATION
 ******************************************************************************/

/**
 * @brief System time base type configuration
 *
 * By default, the library uses `volatile uint32_t` as the time base type, providing
 * a maximum timeout of UINT32_MAX ticks.
 *
 * To use a custom time base type:
 * 1. Define EC_TIME_BASE_TYPE_CUSTOM as your desired type
 * 2. Define the corresponding EC_TIME_BASE_TYPE_CUSTOM_IS_* macro
 *
 * @example Custom 16-bit time base
 * @code
 * #define EC_TIME_BASE_TYPE_CUSTOM uint16_t
 * #define EC_TIME_BASE_TYPE_CUSTOM_IS_UINT16
 * #include "err_core.h"
 * @endcode
 *
 * Supported types:
 * - UINT8:  8-bit unsigned  (max timeout: 255)
 * - UINT16: 16-bit unsigned (max timeout: 65535)
 * - UINT32: 32-bit unsigned (max timeout: 4294967295) [DEFAULT]
 * - UINT64: 64-bit unsigned (max timeout: 18446744073709551615)
 * - INT8:   8-bit signed    (max timeout: 127)
 * - INT16:  16-bit signed   (max timeout: 32767)
 * - INT32:  32-bit signed   (max timeout: 2147483647)
 * - INT64:  64-bit signed   (max timeout: 9223372036854775807)
 */

#ifndef EC_TIME_BASE_TYPE_CUSTOM

/** @brief Maximum timeout value for default time base */
#define EC_MAX_TIMEOUT UINT32_MAX

/** @brief Default time base type - 32-bit unsigned volatile */
typedef volatile uint32_t EC_TIME_t;

#else

/** @brief Custom time base type */
typedef EC_TIME_BASE_TYPE_CUSTOM EC_TIME_t;

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

/*******************************************************************************
 * TYPE DEFINITIONS
 ******************************************************************************/

/**
 * @enum EC_err_state_t
 * @brief Error state enumeration
 *
 * Represents the current state of an error condition.
 */
typedef enum
{
    EC_NERR = 0, /**< No error - condition is normal */
    EC_ERR = 1   /**< Error detected - condition is abnormal */
} EC_err_state_t;

/**
 * @struct EC_runtimeData_t
 * @brief Runtime data for error tracking
 *
 * This structure contains all runtime state information for a single error condition.
 * It is automatically managed by the library and should not be modified directly.
 *
 * Memory Layout (3 bytes + padding):
 * - LastReg: 4 bytes
 * - LastNoErr: 4 bytes
 * - WarningCnt: 7 bits
 * - WarningPending: 1 bit
 *
 * @note All fields are updated automatically by EC_poll()
 */
typedef struct
{
    EC_TIME_t LastReg; /**< Timestamp when error/warning was last registered
                            Used to calculate TimeToResetWarning timeout */

    EC_TIME_t LastNoErr; /**< Timestamp when error was last NOT present
                              Used to calculate TimeToErrorRegister timeout */

    uint8_t WarningCnt : 7; /**< Warning accumulator (0-127)
                                 Increments each time error persists for TimeToErrorRegister
                                 Resets when error is cleared for TimeToResetWarning
                                 Escalates to error when reaching WarningsToError threshold */

    uint8_t WarningPending : 1; /**< Warning pending flag (0 or 1)
                                     Set when warning is registered
                                     Prevents multiple warnings in single detection cycle
                                     Cleared after TimeToResetWarning elapses */
} EC_runtimeData_t;

/**
 * @struct EC_error_t
 * @brief Error definition structure
 *
 * Defines a single error condition including its check function, timing parameters,
 * and escalation behavior.
 *
 * @note This structure should be declared as const and stored in flash memory
 *
 * @example Temperature sensor error with 3-stage warning
 * @code
 * EC_err_state_t check_temp_sensor(uint16_t sensor_id) {
 *     return (sensor_readings[sensor_id] > LIMIT) ? EC_ERR : EC_NERR;
 * }
 *
 * const EC_error_t temp_errors[] = {
 *     {
 *         .ErrFunc = check_temp_sensor,
 *         .HelperNumber = 0,              // Sensor ID
 *         .TimeToErrorRegister = 1000,    // 1 second debounce
 *         .TimeToResetWarning = 5000,     // 5 second warning reset
 *         .WarningsToError = 3            // 3 warnings before error
 *     }
 * };
 * @endcode
 */
typedef struct
{
    /**
     * @brief Error check function pointer
     *
     * This function is called by EC_poll() to determine if the error condition exists.
     *
     * @param HelperNumber User-defined parameter (e.g., sensor ID, channel number)
     * @return EC_ERR if error condition is present, EC_NERR otherwise
     *
     * @note Function should execute quickly (< 100µs recommended)
     * @note Function must be reentrant if used in multi-threaded environment
     */
    EC_err_state_t (*ErrFunc)(uint16_t HelperNumber);

    /**
     * @brief Helper parameter passed to error check function
     *
     * Typical uses:
     * - Sensor/channel index
     * - Peripheral identifier
     * - Threshold value
     * - Any user-defined parameter
     */
    uint16_t HelperNumber;

    /**
     * @brief Debounce time - ticks error must persist before warning/registration
     *
     * Error condition must be continuously present for this duration before:
     * - First detection: Warning counter increments
     * - Subsequent detections: Additional warnings after WarningPending clears
     *
     * Typical Values:
     * - Fast sensors: 100-500 ticks (100-500ms @ 1kHz)
     * - Slow sensors: 1000-5000 ticks (1-5s @ 1kHz)
     * - Communication: 3000-10000 ticks (3-10s @ 1kHz)
     *
     * @note Set to 0 for immediate detection (not recommended for noisy signals)
     */
    EC_TIME_t TimeToErrorRegister;

    /**
     * @brief Warning reset time - ticks after which WarningPending flag clears
     *
     * After a warning is registered, this timeout determines when:
     * - WarningPending flag is cleared (allows next warning detection)
     * - WarningReg bit is cleared (visual warning indicator off)
     *
     * Important: WarningCnt is NOT reset by this timeout!
     * WarningCnt only resets when error clears for >= TimeToResetWarning
     *
     * Typical Values:
     * - Quick retry: 1000-2000 ticks (1-2s)
     * - Normal: 5000-10000 ticks (5-10s)
     * - Slow: 30000-60000 ticks (30-60s)
     */
    EC_TIME_t TimeToResetWarning;

    /**
     * @brief Warning escalation threshold
     *
     * Number of warnings that must accumulate before escalating to registered error.
     *
     * Behavior:
     * - 1: First warning immediately becomes error (simple debouncing)
     * - 2-127: Warning must occur N times before error registration
     *
     * Use Cases:
     * - 1: Critical errors (immediate action required)
     * - 2-3: Important errors (allow brief recovery attempts)
     * - 5-10: Non-critical errors (tolerate intermittent issues)
     *
     * @note Set to 1 for traditional debounced error detection
     * @note Values > 1 enable graduated warning system
     */
    uint16_t WarningsToError;

} EC_error_t;

/**
 * @struct EC_instance_t
 * @brief Error management instance
 *
 * Main structure managing a collection of error conditions. Multiple instances
 * can be created for different subsystems.
 *
 * @note Initialize to zero before calling EC_init()
 *
 * @example Multi-instance setup
 * @code
 * // Sensor subsystem
 * EC_instance_t sensor_errors = {0};
 * EC_init(&sensor_errors, sensor_err_defs, sensor_runtime, 4);
 *
 * // Communication subsystem
 * EC_instance_t comm_errors = {0};
 * EC_init(&comm_errors, comm_err_defs, comm_runtime, 2);
 *
 * // Poll both in main loop
 * while(1) {
 *     EC_poll(&sensor_errors);
 *     EC_poll(&comm_errors);
 * }
 * @endcode
 */
typedef struct
{
    /**
     * @brief Error register - 64-bit bitfield of registered errors
     *
     * Each bit represents one error:
     * - Bit 0: Error 0
     * - Bit 1: Error 1
     * - ...
     * - Bit 63: Error 63
     *
     * Bit Value:
     * - 0: Error not registered (normal operation)
     * - 1: Error registered (action required)
     *
     * @note Updated automatically by EC_poll()
     * @note Can be cleared with EC_clearErr()
     */
    uint64_t ErrorReg;

    /**
     * @brief Warning register - 64-bit bitfield of active warnings
     *
     * Similar to ErrorReg, but for warnings that haven't escalated to errors yet.
     *
     * Bit Value:
     * - 0: No active warning
     * - 1: Warning active (not yet an error)
     *
     * @note Warnings automatically clear after TimeToResetWarning
     * @note Warnings escalate to errors based on WarningsToError threshold
     */
    uint64_t WarningReg;

    /**
     * @brief Pointer to error definition array
     *
     * Must point to a const array of EC_error_t structures.
     * Array should be stored in flash memory to save RAM.
     */
    const EC_error_t *Errors;

    /**
     * @brief Pointer to runtime data array
     *
     * Must point to an array of EC_runtimeData_t structures.
     * Array must be in RAM and initialized to zero.
     * Array size must match NumberOfErrors.
     */
    EC_runtimeData_t *RuntimeData;

    /**
     * @brief Number of errors in this instance
     *
     * Valid range: 1-64
     * Must match size of Errors and RuntimeData arrays.
     */
    uint8_t NumberOfErrors;

} EC_instance_t;

/*******************************************************************************
 * FUNCTION PROTOTYPES
 ******************************************************************************/

#if EC_TICK_FROM_FUNC

/**
 * @brief Registers system tick source function
 *
 * Call this once during initialization when EC_TICK_FROM_FUNC is 1.
 * The registered function will be called by EC_poll() to get current time.
 *
 * @param[in] Function Pointer to function returning current tick count
 *
 * @pre Function must not be NULL
 * @post All subsequent EC_poll() calls will use this function for timing
 *
 * @note Function should execute in < 10µs
 * @note Function must be reentrant if EC_poll() can be called from interrupts
 *
 * @example FreeRTOS tick function
 * @code
 * EC_TIME_t get_system_tick(void) {
 *     return (EC_TIME_t)xTaskGetTickCount();
 * }
 *
 * void init(void) {
 *     EC_tick_function_register(get_system_tick);
 * }
 * @endcode
 */
void EC_tick_function_register(EC_TIME_t (*Function)(void));

#else

/**
 * @brief Registers system tick source variable
 *
 * Call this once during initialization when EC_TICK_FROM_FUNC is 0.
 * EC_poll() will read this variable directly to get current time.
 *
 * @param[in] Variable Pointer to volatile variable holding current tick count
 *
 * @pre Variable must not be NULL
 * @pre Variable must be updated regularly (e.g., in SysTick ISR)
 * @post All subsequent EC_poll() calls will read this variable for timing
 *
 * @note Variable should be updated atomically
 * @note Variable must remain valid for lifetime of error system
 *
 * @example SysTick-based timing
 * @code
 * volatile uint32_t system_tick = 0;
 *
 * void SysTick_Handler(void) {
 *     system_tick++;
 * }
 *
 * void init(void) {
 *     EC_tick_variable_register((EC_TIME_t*)&system_tick);
 * }
 * @endcode
 */
void EC_tick_variable_register(EC_TIME_t *Variable);

#endif

/**
 * @brief Initializes error control instance
 *
 * Must be called once for each EC_instance_t before using EC_poll().
 * Sets up the instance with error definitions and runtime data arrays.
 *
 * @param[out] Instance     Pointer to instance to initialize (must be zeroed)
 * @param[in]  Errors       Pointer to const array of error definitions
 * @param[in]  Timestamps   Pointer to runtime data array (must be zeroed)
 * @param[in]  NumberOfErrors Size of Errors and Timestamps arrays (1-64)
 *
 * @pre Instance must be zero-initialized
 * @pre Errors array must be valid and const
 * @pre Timestamps array must be zero-initialized and in RAM
 * @pre NumberOfErrors must be 1-64
 * @pre Arrays must remain valid for lifetime of instance
 *
 * @post Instance is ready for EC_poll() calls
 * @post All error/warning registers are cleared
 *
 * @example Initialization
 * @code
 * // Error definitions (const, in flash)
 * const EC_error_t errors[] = {
 *     {check_temp, 0, 1000, 5000, 3},
 *     {check_voltage, 0, 500, 2000, 1}
 * };
 *
 * // Runtime data (RAM, zero-initialized)
 * EC_runtimeData_t runtime[2] = {0};
 *
 * // Instance (RAM, zero-initialized)
 * EC_instance_t instance = {0};
 *
 * // Initialize
 * EC_init(&instance, errors, runtime, 2);
 * @endcode
 */
void EC_init(EC_instance_t *Instance, const EC_error_t *Errors, EC_runtimeData_t *Timestamps, uint8_t NumberOfErrors);

/**
 * @brief Polls all errors and updates state
 *
 * This function must be called periodically (e.g., in main loop or timer ISR).
 * It checks all error conditions, manages debouncing, handles warning escalation,
 * and updates error/warning registers.
 *
 * @param[in,out] Instance Pointer to initialized error instance
 *
 * @pre Instance must be initialized with EC_init()
 * @pre System tick source must be registered
 *
 * @post Error and warning registers updated
 * @post Runtime data timestamps updated
 * @post Warning counters updated
 *
 * @note Call frequency determines timing resolution
 * @note Execution time: O(n) where n = NumberOfErrors
 * @note Safe to call from interrupts if error functions are reentrant
 *
 * @example Typical usage
 * @code
 * void main(void) {
 *     init_system();
 *     EC_init(&errors, ...);
 *
 *     while(1) {
 *         EC_poll(&errors);  // Poll errors
 *
 *         // Check for critical errors
 *         if (errors.ErrorReg & 0x01) {
 *             handle_critical_error();
 *         }
 *
 *         delay_ms(10);  // 10ms poll rate = 100Hz
 *     }
 * }
 * @endcode
 */
void EC_poll(EC_instance_t *Instance);

/**
 * @brief Returns current error register
 *
 * Retrieves the complete 64-bit error register showing all registered errors.
 *
 * @param[in] Instance Pointer to error instance
 * @return 64-bit bitfield where each bit represents one error state
 *
 * @retval 0        No errors registered
 * @retval non-zero One or more errors registered (bit 1 = error present)
 *
 * @pre Instance must be initialized
 *
 * @example Check multiple errors
 * @code
 * uint64_t errors = EC_getErrors(&instance);
 *
 * if (errors == 0) {
 *     // All OK
 * } else if (errors & 0x0F) {
 *     // At least one of first 4 errors is active
 * }
 *
 * // Count total errors
 * int count = 0;
 * for (int i = 0; i < 64; i++) {
 *     if (errors & ((uint64_t)1 << i)) count++;
 * }
 * @endcode
 */
uint64_t EC_getErrors(EC_instance_t *Instance);

/**
 * @brief Returns state of specific error
 *
 * Queries whether a particular error is currently registered.
 *
 * @param[in] Instance    Pointer to error instance
 * @param[in] ErrorNumber Index of error to query (0-63)
 *
 * @return Error state
 * @retval EC_NERR Error not registered (normal)
 * @retval EC_ERR  Error registered (fault condition)
 *
 * @pre Instance must be initialized
 * @pre ErrorNumber must be 0-63
 *
 * @example Check specific error
 * @code
 * if (EC_getOneError(&instance, 0) == EC_ERR) {
 *     printf("Temperature sensor error!\n");
 *     activate_cooling();
 * }
 *
 * if (EC_getOneError(&instance, 1) == EC_ERR) {
 *     printf("Communication timeout!\n");
 *     reconnect();
 * }
 * @endcode
 */
EC_err_state_t EC_getOneError(EC_instance_t *Instance, uint8_t ErrorNumber);

/**
 * @brief Force-checks and registers error immediately
 *
 * Bypasses debouncing and warning system to immediately check and register
 * an error if the condition is present. Useful for critical errors that
 * need immediate attention.
 *
 * @param[in,out] Instance    Pointer to error instance
 * @param[in]     ErrorNumber Index of error to force-check (0-63)
 *
 * @return Current error state after check
 * @retval EC_NERR Error condition not present
 * @retval EC_ERR  Error condition present and now registered
 *
 * @pre Instance must be initialized
 * @pre ErrorNumber must be 0-63
 * @pre Error check function must not be NULL
 *
 * @post If error present, ErrorReg bit is set immediately
 * @post WarningReg and warning counters are NOT updated
 *
 * @warning Bypasses all timing and warning logic
 * @warning Use sparingly for truly critical errors only
 *
 * @example Manual error check
 * @code
 * // User pressed emergency stop button
 * if (emergency_stop_pressed()) {
 *     // Force immediate error registration
 *     EC_checkError(&instance, ERROR_EMERGENCY_STOP);
 *     shutdown_system();
 * }
 *
 * // Verify sensor before critical operation
 * if (EC_checkError(&instance, ERROR_SENSOR) == EC_ERR) {
 *     abort_operation();
 * }
 * @endcode
 */
EC_err_state_t EC_checkError(EC_instance_t *Instance, uint8_t ErrorNumber);

/**
 * @brief Clears all errors and warnings
 *
 * Resets the error management system to initial state:
 * - Clears all error register bits
 * - Clears all warning register bits
 * - Resets warning counters
 * - Updates timestamps to current time
 *
 * @param[in,out] Instance Pointer to error instance
 *
 * @pre Instance must be initialized
 *
 * @post ErrorReg = 0
 * @post WarningReg = 0
 * @post All WarningCnt = 0
 * @post All WarningPending = 0
 * @post All LastNoErr = current tick
 *
 * @note Does NOT clear error definitions or configuration
 * @note Errors will be re-detected on next EC_poll() if conditions persist
 *
 * @example Error acknowledgment
 * @code
 * void acknowledge_all_errors(void) {
 *     EC_clearErr(&instance);
 *     log_message("All errors cleared by user");
 * }
 *
 * void reset_after_maintenance(void) {
 *     perform_maintenance();
 *     EC_clearErr(&instance);  // Fresh start
 *     log_message("System reset after maintenance");
 * }
 * @endcode
 */
void EC_clearErr(EC_instance_t *Instance);

#endif /* ERR_CORE_ERR_CORE_H_ */