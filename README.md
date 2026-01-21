# Error Core Library

**Advanced Error Management System for Embedded Systems**

[![License: MPL 2.0](https://img.shields.io/badge/License-MPL%202.0-brightgreen.svg)](https://opensource.org/licenses/MPL-2.0)
[![Version](https://img.shields.io/badge/Version-2.0.0-blue.svg)](CHANGELOG.md)
[![Language: C](https://img.shields.io/badge/Language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Platform: Embedded](https://img.shields.io/badge/Platform-Embedded-orange.svg)]()
[![Tests](https://img.shields.io/badge/Tests-22%2F22%20passing-success.svg)]()

## Overview

Error Core is a sophisticated error handling library designed for resource-constrained embedded systems. It provides intelligent error detection with debouncing, graduated warning escalation, and comprehensive state tracking.

### Key Features

- **Debouncing**: Filters transient faults with configurable persistence time
- **Warning System**: Graduated escalation from warnings to critical errors
- **Multi-Instance**: Support for multiple independent error management subsystems
- **Efficient**: Minimal memory footprint, optimized for 8/16/32-bit microcontrollers
- **Flexible**: Up to 64 simultaneous error conditions per instance
- **Type-Safe**: Strongly typed API with compile-time checks
- **Well-Tested**: Comprehensive unit test suite included

## Table of Contents

- [Quick Start](#quick-start)
- [Core Concepts](#core-concepts)
- [API Reference](#api-reference)
- [Configuration](#configuration)
- [Usage Examples](#usage-examples)
- [Best Practices](#best-practices)
- [Memory Requirements](#memory-requirements)
- [FAQ](#faq)

## Quick Start

### Basic Example

```c
#include "err_core.h"

// Step 1: Define error check functions
EC_err_state_t check_temperature(uint16_t sensor_id) {
    return (temperature[sensor_id] > MAX_TEMP) ? EC_ERR : EC_NERR;
}

EC_err_state_t check_communication(uint16_t timeout_ms) {
    return (comm_timeout > timeout_ms) ? EC_ERR : EC_NERR;
}

// Step 2: Configure error definitions (const, in flash)
const EC_error_t errors[] = {
    // {check_func, helper, debounce_ms, warning_reset_ms, warnings_to_error}
    {check_temperature,   0, 1000, 5000, 3},  // Temperature: 3 warnings before error
    {check_communication, 0,  500, 2000, 1}   // Communication: immediate error
};

// Step 3: Allocate runtime data (RAM, zero-initialized)
EC_runtimeData_t runtime[2] = {0};
EC_instance_t instance = {0};

// Step 4: System tick variable
volatile uint32_t system_tick = 0;

void SysTick_Handler(void) {
    system_tick++;
}

// Step 5: Initialize
void init(void) {
    EC_tick_variable_register((EC_TIME_t*)&system_tick);
    EC_init(&instance, errors, runtime, 2);
}

// Step 6: Poll in main loop
void main(void) {
    init();
    
    while(1) {
        EC_poll(&instance);  // Check all errors
        
        // Handle errors
        if (EC_getOneError(&instance, 0) == EC_ERR) {
            handle_temperature_error();
        }
        
        if (EC_getOneError(&instance, 1) == EC_ERR) {
            handle_communication_error();
        }
        
        delay_ms(10);  // 10ms poll rate
    }
}
```

## Core Concepts

### Error State Machine

Each error transitions through these states:

```
   [NORMAL]
       |
       | Error detected for TimeToErrorRegister
       v
   [WARNING 1]
       |
       | Error clears briefly, returns quickly (< TimeToResetWarning)
       v
   [WARNING 2]
       |
       | Error clears briefly, returns quickly
       v
   [WARNING 3]  (if WarningsToError = 3)
       |
       v
   [ERROR REGISTERED]
       |
       | EC_clearErr() called
       v
   [NORMAL]

Alternative path:
   [WARNING N]
       |
       | TimeToResetWarning elapses
       v
   [NORMAL]  (counter reset, starts fresh)
```

### Key Parameters

| Parameter | Description | Typical Range |
|-----------|-------------|---------------|
| **TimeToErrorRegister** | Debounce time - how long error must persist | 100ms - 10s |
| **TimeToResetWarning** | Time until warning pending flag clears | 1s - 60s |
| **WarningsToError** | Number of warnings before escalation | 1 - 127 |

### Warning Accumulation Example

```
Time:    0ms   100ms  150ms  200ms  300ms  350ms  400ms  500ms
         |      |      |      |      |      |      |      |
Error:   ------██████---████████---████████-----------
         
State:   NORM  WARN1  clr   WARN2  clr   WARN3 → ERROR!
                ↑            ↑            ↑
              +1 cnt       +1 cnt       +1 cnt → Error
              
Timeline:
  t=100ms:  Error present for 100ms → WarningCnt=1, WarningPending=1
  t=150ms:  Error clears → WarningPending=0 (allows next detection)
  t=200ms:  Error returns (only 100ms since LastReg, < 500ms timeout)
  t=300ms:  Error present for 100ms → WarningCnt=2 (accumulated!)
  t=350ms:  Error clears → WarningPending=0
  t=400ms:  Error returns again
  t=500ms:  Error present for 100ms → WarningCnt=3 → ERROR REGISTERED!

If there was a long gap:
  t=100ms:  WarningCnt=1
  t=700ms:  TimeToResetWarning elapsed (700-100 > 500ms)
            → WarningCnt=0 (RESET!)
  t=800ms:  Error returns → WarningCnt=1 (starts fresh, not 2!)
```

### Important: WarningCnt Behavior

**WarningCnt resets with TimeToResetWarning:**
- ✅ Increments when error persists for TimeToErrorRegister
- ✅ Resets to 0 after TimeToResetWarning elapses
- ✅ This means warnings only accumulate if error returns QUICKLY (before timeout)
- ✅ Also resets when error escalates (WarningCnt >= WarningsToError)

**Why this matters:**
```
TimeToResetWarning = 500ms, WarningsToError = 3

Scenario A - Fast recurring error (accumulates):
t=0:   Error appears
t=100: Warning 1
t=150: Error clears
t=200: Error returns (only 100ms since last warning < 500ms)
t=300: Warning 2 (accumulated!)
t=350: Error clears
t=400: Error returns (only 100ms since last warning < 500ms)
t=500: Warning 3 → ERROR!

Scenario B - Slow recurring error (resets):
t=0:    Error appears
t=100:  Warning 1
t=150:  Error clears
t=800:  Error returns (700ms since last warning > 500ms)
        → Counter RESET!
t=900:  Warning 1 (starts fresh, not 2!)
```

## API Reference

### Initialization Functions

#### `EC_tick_variable_register()`
```c
void EC_tick_variable_register(EC_TIME_t *Variable);
```
Registers the system tick variable (when `EC_TICK_FROM_FUNC = 0`).

**Parameters:**
- `Variable`: Pointer to volatile tick counter

**Example:**
```c
volatile uint32_t system_tick = 0;
EC_tick_variable_register((EC_TIME_t*)&system_tick);
```

---

#### `EC_init()`
```c
void EC_init(EC_instance_t *Instance, const EC_error_t *Errors, 
             EC_runtimeData_t *Timestamps, uint8_t NumberOfErrors);
```
Initializes an error management instance.

**Parameters:**
- `Instance`: Pointer to instance struct (must be zero-initialized)
- `Errors`: Pointer to const error definitions array
- `Timestamps`: Pointer to runtime data array (must be zero-initialized)
- `NumberOfErrors`: Number of errors (1-64)

**Example:**
```c
const EC_error_t errors[2] = {...};
EC_runtimeData_t runtime[2] = {0};
EC_instance_t instance = {0};

EC_init(&instance, errors, runtime, 2);
```

### Runtime Functions

#### `EC_poll()`
```c
void EC_poll(EC_instance_t *Instance);
```
Polls all errors and updates state. **Must be called periodically.**

**Call Frequency:** Determines timing resolution. Recommended: 10-100ms

**Example:**
```c
while(1) {
    EC_poll(&instance);
    // ... other tasks ...
    delay_ms(10);
}
```

---

#### `EC_getErrors()`
```c
uint64_t EC_getErrors(EC_instance_t *Instance);
```
Returns 64-bit error register.

**Returns:** Bitfield where bit N represents error N (1 = error present)

**Example:**
```c
uint64_t errors = EC_getErrors(&instance);
if (errors & 0x03) {
    // Error 0 or 1 is active
}
```

---

#### `EC_getOneError()`
```c
EC_err_state_t EC_getOneError(EC_instance_t *Instance, uint8_t ErrorNumber);
```
Checks if specific error is registered.

**Parameters:**
- `ErrorNumber`: Error index (0-63)

**Returns:** `EC_ERR` if error registered, `EC_NERR` otherwise

**Example:**
```c
if (EC_getOneError(&instance, 0) == EC_ERR) {
    handle_temperature_fault();
}
```

---

#### `EC_checkError()`
```c
EC_err_state_t EC_checkError(EC_instance_t *Instance, uint8_t ErrorNumber);
```
Force-checks error, bypassing debouncing. **Use sparingly!**

**Warning:** Bypasses all timing and warning logic.

**Example:**
```c
// Emergency check before critical operation
if (EC_checkError(&instance, ERROR_CRITICAL) == EC_ERR) {
    abort_operation();
}
```

---

#### `EC_clearErr()`
```c
void EC_clearErr(EC_instance_t *Instance);
```
Clears all errors and warnings. Resets system to initial state.

**Example:**
```c
void user_acknowledged_errors(void) {
    EC_clearErr(&instance);
    log_message("Errors cleared by user");
}
```

## Configuration

### Time Base Configuration

By default, the library uses `volatile uint32_t` for timing. To use a custom type:

```c
// In your config file BEFORE including err_core.h:
#define EC_TIME_BASE_TYPE_CUSTOM uint16_t
#define EC_TIME_BASE_TYPE_CUSTOM_IS_UINT16
#include "err_core.h"
```

Supported types: `UINT8`, `UINT16`, `UINT32`, `UINT64`, `INT8`, `INT16`, `INT32`, `INT64`

### Tick Source Configuration

**Variable-based (default, faster):**
```c
#define EC_TICK_FROM_FUNC 0  // Before including header

volatile uint32_t tick = 0;
EC_tick_variable_register(&tick);
```

**Function-based (thread-safe):**
```c
#define EC_TICK_FROM_FUNC 1  // Before including header

EC_TIME_t get_tick(void) {
    return xTaskGetTickCount();  // FreeRTOS example
}

EC_tick_function_register(get_tick);
```

## Usage Examples

### Example 1: Dual Error Detection - Auto-Recovery vs Manual Intervention

```c
// Scenario: System has auto-recovery mechanism
// Error 1: Triggers auto-recovery immediately (WarningsToError=1)
// Error 2: Detects if problem is serious/persistent (WarningsToError=3)
//          Only escalates if auto-recovery fails 3 times quickly

EC_err_state_t check_system_fault(uint16_t unused) {
    return (system_has_fault()) ? EC_ERR : EC_NERR;
}

const EC_error_t errors[] = {
    // Error 0: Immediate detection + auto-recovery
    {check_system_fault, 0, 100, 500, 1},
    
    // Error 1: Persistent problem detection (requires manual intervention)
    {check_system_fault, 0, 100, 500, 3}
};

EC_runtimeData_t runtime[2] = {0};
EC_instance_t monitor = {0};

void init_error_monitor(void) {
    EC_tick_variable_register(&system_tick);
    EC_init(&monitor, errors, runtime, 2);
}

void monitor_loop(void) {
    EC_poll(&monitor);
    
    // Error 0: Immediate response with auto-recovery
    if (EC_getOneError(&monitor, 0) == EC_ERR) {
        log_warning("Fault detected - attempting auto-recovery");
        attempt_auto_recovery();
        EC_clearErr(&monitor);  // Clear after recovery attempt
    }
    
    // Error 1: Manual intervention required (auto-recovery failed 3x)
    if (EC_getOneError(&monitor, 1) == EC_ERR) {
        log_critical("PERSISTENT FAULT - auto-recovery failed 3 times!");
        log_critical("Manual intervention required!");
        disable_auto_recovery();
        alert_operator();
    }
}

/* Timeline example:
 * t=0:   Fault occurs
 * t=100: Error 0 = ERR (immediate), Error 1 = WARNING (1/3)
 *        → Auto-recovery triggered, fault cleared
 * 
 * t=200: Fault returns (only 100ms later, < 500ms timeout)
 * t=300: Error 0 = ERR, Error 1 = WARNING (2/3)
 *        → Auto-recovery triggered again
 * 
 * t=400: Fault returns AGAIN (persistent problem!)
 * t=500: Error 0 = ERR, Error 1 = ERROR! (3/3)
 *        → Manual intervention required
 *
 * Alternative: If fault stayed away > 500ms between occurrences,
 *              Error 1 counter would reset - isolated incidents OK
 */
```

### Example 2: Communication Timeout with Retry Logic

```c
// Scenario: Network communication monitoring
// - First timeout: Automatic reconnect attempt
// - If timeouts happen quickly (< 3s apart): Network is unstable
// - After 2 quick timeouts: Switch to backup connection

volatile uint32_t last_message_time = 0;

EC_err_state_t check_comm_timeout(uint16_t timeout_ms) {
    uint32_t elapsed = system_tick - last_message_time;
    return (elapsed > timeout_ms) ? EC_ERR : EC_NERR;
}

const EC_error_t errors[] = {
    {check_comm_timeout, 5000, 5000, 3000, 2}  
    // 5s timeout detection
    // 3s warning reset (if timeouts > 3s apart, they're isolated incidents)
    // 2 warnings = switch to backup
};

EC_runtimeData_t runtime[1] = {0};
EC_instance_t comm_monitor = {0};

void on_message_received(void) {
    last_message_time = system_tick;
    
    // If we were in error state, clear it
    if (comm_monitor.ErrorReg & 0x01) {
        EC_clearErr(&comm_monitor);
        log_info("Communication restored");
    }
}

void comm_monitor_task(void) {
    EC_poll(&comm_monitor);
    
    // Warning: timeout detected, try to reconnect
    if (comm_monitor.WarningReg & 0x01) {
        if (runtime[0].WarningCnt == 1) {
            log_warning("Communication timeout - reconnecting...");
            attempt_reconnect();
        }
    }
    
    // Error: Repeated timeouts (network unstable)
    if (EC_getOneError(&comm_monitor, 0) == EC_ERR) {
        log_error("Network unstable - switching to backup");
        switch_to_backup_connection();
        EC_clearErr(&comm_monitor);
    }
}

/* How it works:
 * Isolated timeouts (> 3s apart):
 *   - Each triggers reconnect attempt
 *   - Counter resets between incidents
 *   - Never escalates to error
 * 
 * Rapid timeouts (< 3s apart):
 *   - First: Reconnect attempt, WarningCnt=1
 *   - Second (within 3s): WarningCnt=2 → ERROR!
 *   - Switches to backup immediately
 */
```

### Example 3: Multi-Instance System

```c
// Scenario: Separate error monitoring for different subsystems

// Sensor errors
const EC_error_t sensor_errors[] = {
    {check_temp_sensor, 0, 1000, 5000, 3},
    {check_pressure_sensor, 0, 1000, 5000, 3}
};
EC_runtimeData_t sensor_runtime[2] = {0};
EC_instance_t sensor_instance = {0};

// Communication errors
const EC_error_t comm_errors[] = {
    {check_uart_timeout, 0, 3000, 10000, 2},
    {check_can_timeout, 0, 3000, 10000, 2}
};
EC_runtimeData_t comm_runtime[2] = {0};
EC_instance_t comm_instance = {0};

void init_error_system(void) {
    EC_tick_variable_register(&system_tick);
    
    EC_init(&sensor_instance, sensor_errors, sensor_runtime, 2);
    EC_init(&comm_instance, comm_errors, comm_runtime, 2);
}

void main_loop(void) {
    while(1) {
        EC_poll(&sensor_instance);
        EC_poll(&comm_instance);
        
        // Handle sensor errors
        uint64_t sensor_err = EC_getErrors(&sensor_instance);
        if (sensor_err) handle_sensor_faults(sensor_err);
        
        // Handle comm errors
        uint64_t comm_err = EC_getErrors(&comm_instance);
        if (comm_err) handle_comm_faults(comm_err);
        
        delay_ms(10);
    }
}
```

### Example 4: Error Logging and Diagnostics

```c
void print_error_status(EC_instance_t *instance, const char *subsystem) {
    printf("=== %s Error Status ===\n", subsystem);
    
    uint64_t errors = EC_getErrors(instance);
    uint64_t warnings = instance->WarningReg;
    
    printf("Errors:   0x%016llX\n", errors);
    printf("Warnings: 0x%016llX\n", warnings);
    
    for (uint8_t i = 0; i < instance->NumberOfErrors; i++) {
        if (errors & ((uint64_t)1 << i)) {
            printf("  [ERROR %d] Registered\n", i);
        } else if (warnings & ((uint64_t)1 << i)) {
            printf("  [WARN %d] Count: %d\n", i, instance->RuntimeData[i].WarningCnt);
        }
    }
}

// Usage
print_error_status(&sensor_instance, "Sensors");
print_error_status(&comm_instance, "Communication");
```

## Best Practices

### 1. Error Check Functions

**DO:**
- Keep functions fast (< 100µs)
- Make functions reentrant
- Return immediately
- Use HelperNumber for parameterization

**DON'T:**
- Perform I/O operations
- Block or delay
- Access shared resources without protection
- Modify global state

```c
// GOOD
EC_err_state_t check_voltage(uint16_t channel) {
    return (adc_values[channel] > threshold[channel]) ? EC_ERR : EC_NERR;
}

// BAD - Too slow!
EC_err_state_t check_voltage_bad(uint16_t channel) {
    float voltage = read_adc_blocking(channel);  // I/O operation
    delay_ms(10);  // Delay!
    return (voltage > MAX_VOLTAGE) ? EC_ERR : EC_NERR;
}
```

### 2. Timing Configuration

**Debounce Time (TimeToErrorRegister):**
- Digital signals: 50-200ms
- Analog sensors: 500-2000ms
- Communication: 2000-10000ms

**Warning Reset (TimeToResetWarning):**
- Quick recovery: 1000-3000ms
- Normal: 5000-10000ms
- Slow systems: 30000-60000ms

**General Rule:** `TimeToResetWarning > TimeToErrorRegister`

### 3. Warning Thresholds

| WarningsToError | Use Case |
|-----------------|----------|
| 1 | Critical errors requiring immediate action |
| 2-3 | Important errors with brief recovery window |
| 5-10 | Non-critical errors, tolerate intermittent faults |

### 4. Memory Organization

```c
// Store error definitions in flash (const)
const EC_error_t errors[] = {
    // definitions...
} __attribute__((section(".rodata")));

// Runtime data in RAM (zero-initialized)
EC_runtimeData_t runtime[NUM_ERRORS] = {0};
EC_instance_t instance = {0};
```

### 5. Poll Frequency

| Application | Poll Rate | Resolution |
|-------------|-----------|------------|
| Fast control loops | 1-10ms | High precision |
| Standard monitoring | 10-100ms | Good balance |
| Slow sensors | 100-1000ms | Low overhead |

**Rule of Thumb:** Poll at least 10x faster than your shortest timeout

## Memory Requirements

### Per Instance

```
Instance structure:    20 bytes (fixed)
Error definition:      16 bytes per error (const, flash)
Runtime data:          12 bytes per error (RAM)
```

### Example Calculation

System with 10 errors:
- Instance: 20 bytes
- Definitions: 10 × 16 = 160 bytes (flash)
- Runtime: 10 × 12 = 120 bytes (RAM)
- **Total RAM: 140 bytes**
- **Total Flash: 160 bytes** (+ code)

### Code Size

Typical compiled size (ARM Cortex-M, -Os):
- Core logic: ~800 bytes
- With all functions: ~1200 bytes

## FAQ

### Q: Can I use this in an RTOS?

**A:** Yes! Error Core is RTOS-friendly:
- Thread-safe if error check functions are reentrant
- Can be called from different tasks
- Use function-based tick source for best compatibility

```c
EC_TIME_t get_tick(void) {
    return (EC_TIME_t)xTaskGetTickCount();
}
EC_tick_function_register(get_tick);
```

### Q: What happens if my tick counter wraps around?

**A:** The library handles wraparound correctly using unsigned arithmetic. Works seamlessly across 32-bit boundary.

### Q: Can I have different poll rates for different instances?

**A:** Yes, each instance is independent:

```c
void task_fast(void) {
    while(1) {
        EC_poll(&critical_errors);
        delay_ms(10);  // 100 Hz
    }
}

void task_slow(void) {
    while(1) {
        EC_poll(&noncritical_errors);
        delay_ms(1000);  // 1 Hz
    }
}
```

### Q: How do I debug why an error isn't registering?

**A:** Check the runtime data:

```c
printf("LastNoErr: %u\n", runtime[0].LastNoErr);
printf("LastReg: %u\n", runtime[0].LastReg);
printf("WarningCnt: %u\n", runtime[0].WarningCnt);
printf("WarningPending: %u\n", runtime[0].WarningPending);
printf("Current tick: %u\n", system_tick);

// Check if error function is being called
if (errors[0].ErrFunc(errors[0].HelperNumber) == EC_ERR) {
    printf("Error condition IS present\n");
} else {
    printf("Error condition NOT present\n");
}
```

### Q: Can warnings reset while error is still present?

**A:** Yes, after TimeToResetWarning elapses! This is the intended behavior:

```c
// Timeline with persistent error:
t=0:    Error appears
t=100:  WarningCnt=1, WarningPending=1, LastReg=100

// Error stays continuously present...

t=600:  TimeToResetWarning elapsed (600-100 >= 500)
        → WarningCnt=0, WarningPending=0, WarningReg cleared
        
// Error STILL present, but counter reset!

t=700:  Error still present for 100ms from LastNoErr
        → WarningCnt=1 again (fresh count)
```

**Why?** This distinguishes between:
- **Persistent errors** (continuous) → Each timeout resets the count
- **Recurring errors** (intermittent but frequent) → Counts accumulate before timeout

### Q: What's the maximum number of errors I can have?

**A:** 64 errors per instance. Need more? Create additional instances:

```c
EC_instance_t errors_0_63 = {0};    // Errors 0-63
EC_instance_t errors_64_127 = {0};  // Errors 64-127
```

## Contributing

Found a bug? Have a feature request? Please open an issue on GitHub!

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for detailed version history.

### Latest Release: v2.0.0 (2026-01-21)
- ✨ Added warning system with graduated error escalation
- ✨ Added comprehensive unit test suite (22 tests)
- 🐛 Fixed warning accumulation and reset logic
- 📝 Added detailed documentation and examples

### Previous Release: v1.0.0 (2025-06-06)
- 🎉 Initial release with basic error detection and debouncing

## License

Mozilla Public License 2.0 - see LICENSE file for details.

## Author

**Adrian Pietrzak**
- GitHub: [@AdrianPietrzak1998](https://github.com/AdrianPietrzak1998)

---

**Error Core** - Reliable error management for embedded systems 🛡️
