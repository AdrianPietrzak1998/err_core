# Migration Guide: err_core v1.0.0 → v2.0.0

## Overview

Version 2.0.0 introduces a **warning system** that adds graduated error escalation capability. This guide helps you migrate existing v1.0.0 code to v2.0.0.

## What's New in v2.0.0?

### Warning System
- Errors can generate warnings before escalating to registered errors
- Configurable threshold: how many warnings trigger an error
- Configurable timeout: how long before warnings reset
- Separate warning register for monitoring

### Use Case: Auto-Recovery with Escalation
```c
// Two errors monitoring the same condition:
// Error 0: Triggers immediate auto-recovery (WarningsToError=1)
// Error 1: Escalates after 3 failed recoveries (WarningsToError=3)

const EC_error_t errors[] = {
    {check_fault, 0, 100, 500, 1},  // Auto-recovery trigger
    {check_fault, 0, 100, 500, 3}   // Manual intervention needed
};
```

## Breaking Changes

### 1. EC_error_t Structure

**v1.0.0:**
```c
typedef struct {
    EC_err_state_t (*ErrFunc)(uint16_t HelperNumber);
    uint16_t HelperNumber;
    EC_TIME_t TimeToErrorRegister;
} EC_error_t;
```

**v2.0.0:**
```c
typedef struct {
    EC_err_state_t (*ErrFunc)(uint16_t HelperNumber);
    uint16_t HelperNumber;
    EC_TIME_t TimeToErrorRegister;
    EC_TIME_t TimeToResetWarning;    // NEW: Warning timeout
    uint16_t WarningsToError;        // NEW: Warning threshold
} EC_error_t;
```

### 2. EC_runtimeData_t Structure

**v1.0.0:**
```c
typedef struct {
    EC_TIME_t LastReg;
    EC_TIME_t LastNoErr;
} EC_runtimeData_t;
```

**v2.0.0:**
```c
typedef struct {
    EC_TIME_t LastReg;
    EC_TIME_t LastNoErr;
    uint8_t WarningCnt : 7;      // NEW: Warning counter (0-127)
    uint8_t WarningPending : 1;  // NEW: Warning pending flag
} EC_runtimeData_t;
```

### 3. EC_instance_t Structure

**v1.0.0:**
```c
typedef struct {
    uint64_t ErrorReg;
    const EC_error_t *Errors;
    EC_runtimeData_t *RuntimeData;
    uint8_t NumberOfErrors;
} EC_instance_t;
```

**v2.0.0:**
```c
typedef struct {
    uint64_t ErrorReg;
    uint64_t WarningReg;             // NEW: Warning register
    const EC_error_t *Errors;
    EC_runtimeData_t *RuntimeData;
    uint8_t NumberOfErrors;
} EC_instance_t;
```

## Migration Steps

### Step 1: Update Error Definitions

Add two new parameters to each error definition:

```c
// v1.0.0 code:
const EC_error_t errors[] = {
    {check_temperature, 0, 1000},
    {check_voltage,     0, 500},
    {check_communication, 0, 3000}
};

// v2.0.0 equivalent (same behavior):
const EC_error_t errors[] = {
    {check_temperature,   0, 1000, 1000, 1},  // +TimeToResetWarning, +WarningsToError
    {check_voltage,       0,  500, 1000, 1},
    {check_communication, 0, 3000, 1000, 1}
};
```

**Guidelines:**
- `TimeToResetWarning`: Can be same as `TimeToErrorRegister` for simplicity
- `WarningsToError = 1`: Behaves exactly like v1.0.0 (immediate error)

### Step 2: Ensure Zero-Initialization

Both runtime data and instances should be zero-initialized (already best practice):

```c
// This is correct and works in both versions
EC_runtimeData_t runtime[3] = {0};
EC_instance_t instance = {0};
```

### Step 3: Optional - Add Warning Monitoring

If you want to leverage the new warning system:

```c
void error_monitor(void) {
    EC_poll(&instance);
    
    // NEW: Check warnings
    if (instance.WarningReg & 0x01) {
        uint8_t level = runtime[0].WarningCnt;
        printf("Temperature warning level: %d\n", level);
    }
    
    // Existing: Check errors
    if (instance.ErrorReg & 0x01) {
        printf("Temperature ERROR!\n");
        handle_temperature_error();
    }
}
```

## Migration Patterns

### Pattern 1: Maintain v1.0.0 Behavior (No Warnings)

Set `WarningsToError = 1` for all errors:

```c
const EC_error_t errors[] = {
    // Parameters: {function, helper, debounce, reset, threshold}
    {check_sensor_1, 0, 1000, 1000, 1},  // Immediate error after debounce
    {check_sensor_2, 0, 500,  500,  1},  // Immediate error after debounce
};
```

**Result:** Identical behavior to v1.0.0

### Pattern 2: Add Warnings to Non-Critical Errors

Keep critical errors at threshold=1, add warnings to others:

```c
const EC_error_t errors[] = {
    // Critical - immediate action
    {check_overvoltage, 0, 100, 1000, 1},  // No warnings, immediate error
    
    // Non-critical - allow transient faults
    {check_communication, 0, 1000, 5000, 3},  // 3 warnings before error
    {check_temperature,   0, 2000, 10000, 5}  // 5 warnings before error
};
```

### Pattern 3: Dual-Error Auto-Recovery Pattern

Monitor same condition with two thresholds:

```c
EC_err_state_t check_system_fault(uint16_t unused) {
    return system_has_fault() ? EC_ERR : EC_NERR;
}

const EC_error_t errors[] = {
    // Error 0: Triggers auto-recovery attempt
    {check_system_fault, 0, 1000, 5000, 1},
    
    // Error 1: Detects persistent problem (auto-recovery failing)
    {check_system_fault, 0, 1000, 5000, 3}
};

void monitor(void) {
    EC_poll(&instance);
    
    // Auto-recovery
    if (EC_getOneError(&instance, 0) == EC_ERR) {
        attempt_auto_recovery();
        EC_clearErr(&instance);  // Clear after recovery
    }
    
    // Manual intervention
    if (EC_getOneError(&instance, 1) == EC_ERR) {
        alert_operator("Auto-recovery failed 3 times!");
        disable_auto_recovery();
    }
}
```

## Common Questions

### Q: Will my v1.0.0 code still work?

**A:** Your code will **not compile** without modifications because the `EC_error_t` structure changed. However, migration is straightforward - just add the two new parameters.

### Q: Do I need to change my logic?

**A:** No! If you set `WarningsToError = 1`, the behavior is identical to v1.0.0.

### Q: What values should I use for the new parameters?

**A:** For v1.0.0 behavior:
- `TimeToResetWarning`: Any reasonable value (e.g., same as `TimeToErrorRegister`)
- `WarningsToError`: Set to `1`

### Q: Can I enable warnings gradually?

**A:** Yes! You can:
1. Migrate all errors with `WarningsToError = 1` (v1.0.0 behavior)
2. Gradually change selected errors to `WarningsToError > 1`
3. Add warning monitoring code as needed

### Q: What happens to the warning counter?

**A:** The warning counter (`WarningCnt`):
- Increments each time error persists for `TimeToErrorRegister`
- Resets when `TimeToResetWarning` elapses
- Resets when error escalates to registered error
- Allows warnings to accumulate only if error returns quickly

## Example: Complete Migration

### Before (v1.0.0)

```c
#include "err_core.h"

// Error definitions
const EC_error_t errors[] = {
    {check_temp, 0, 1000},
    {check_comm, 0, 3000}
};

EC_runtimeData_t runtime[2] = {0};
EC_instance_t monitor = {0};

void init(void) {
    EC_tick_variable_register(&system_tick);
    EC_init(&monitor, errors, runtime, 2);
}

void loop(void) {
    EC_poll(&monitor);
    
    if (monitor.ErrorReg & 0x01) {
        handle_temp_error();
    }
    
    if (monitor.ErrorReg & 0x02) {
        handle_comm_error();
    }
}
```

### After (v2.0.0 - Minimal Changes)

```c
#include "err_core.h"

// Error definitions - ADDED TWO PARAMETERS
const EC_error_t errors[] = {
    {check_temp, 0, 1000, 1000, 1},  // +TimeToResetWarning, +WarningsToError
    {check_comm, 0, 3000, 3000, 1}   // +TimeToResetWarning, +WarningsToError
};

EC_runtimeData_t runtime[2] = {0};  // No change needed (zero-init)
EC_instance_t monitor = {0};        // No change needed (zero-init)

void init(void) {
    EC_tick_variable_register(&system_tick);
    EC_init(&monitor, errors, runtime, 2);
}

void loop(void) {
    EC_poll(&monitor);
    
    if (monitor.ErrorReg & 0x01) {
        handle_temp_error();
    }
    
    if (monitor.ErrorReg & 0x02) {
        handle_comm_error();
    }
}
```

**Result:** Identical behavior to v1.0.0!

### After (v2.0.0 - With Warning System)

```c
#include "err_core.h"

// Use warning system for communication
const EC_error_t errors[] = {
    {check_temp, 0, 1000, 1000, 1},    // Critical - immediate error
    {check_comm, 0, 3000, 5000, 2}     // Non-critical - 2 warnings before error
};

EC_runtimeData_t runtime[2] = {0};
EC_instance_t monitor = {0};

void init(void) {
    EC_tick_variable_register(&system_tick);
    EC_init(&monitor, errors, runtime, 2);
}

void loop(void) {
    EC_poll(&monitor);
    
    // Temperature - unchanged
    if (monitor.ErrorReg & 0x01) {
        handle_temp_error();
    }
    
    // Communication - NEW: warning handling
    if (monitor.WarningReg & 0x02) {
        if (runtime[1].WarningCnt == 1) {
            log_info("Comm timeout - retrying...");
            attempt_reconnect();
        }
    }
    
    if (monitor.ErrorReg & 0x02) {
        log_error("Comm failed - switching to backup");
        switch_to_backup();
    }
}
```

## Testing After Migration

1. **Verify compilation:**
   ```bash
   gcc -c err_core.c -o err_core.o
   gcc your_code.c err_core.o -o your_app
   ```

2. **Test error detection:**
   - Verify errors still trigger as expected
   - Check error register bits

3. **Test warning system (if enabled):**
   - Verify warnings increment correctly
   - Verify warnings escalate to errors at threshold
   - Verify warnings reset after timeout

4. **Run existing tests:**
   - All existing functionality should work unchanged
   - Error detection timing should be identical

## Need Help?

- Check [README.md](README.md) for detailed API documentation
- See [CHANGELOG.md](CHANGELOG.md) for complete list of changes
- Run the test suite for reference implementation examples

---

**Migration Summary:**
- ✅ Add two parameters to error definitions
- ✅ Set `WarningsToError = 1` for v1.0.0 behavior
- ✅ Optionally add warning monitoring code
- ✅ Test and deploy!
