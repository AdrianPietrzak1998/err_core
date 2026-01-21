# Changelog

All notable changes to the err_core library will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2026-01-21

### ✨ Added
- **Warning System** with graduated error escalation
  - `WarningsToError` parameter for configurable warning thresholds
  - `TimeToResetWarning` parameter for warning timeout configuration
  - `WarningReg` 64-bit register for active warnings
  - `WarningCnt` counter for warning accumulation
  - `WarningPending` flag to prevent multiple warnings in single cycle
- Comprehensive unit test suite (22 tests, 100% pass rate)
- Detailed API documentation with usage examples
- Support for dual-error monitoring pattern (auto-recovery + manual intervention)

### 🔧 Changed
- **BREAKING:** Modified `EC_error_t` structure - added `TimeToResetWarning` and `WarningsToError` fields
- **BREAKING:** Modified `EC_runtimeData_t` structure - added `WarningCnt` and `WarningPending` fields
- **BREAKING:** Modified `EC_instance_t` structure - added `WarningReg` field
- Enhanced `EC_poll()` logic to handle warning detection and escalation
- Improved error state tracking with `WarningPending` flag reset on error clearance

### 🐛 Fixed
- Fixed warning accumulation logic - warnings now properly accumulate when errors recur quickly
- Fixed `WarningPending` blocking issue - errors can now be re-detected after clearing
- Fixed counter reset behavior - `WarningCnt` properly resets after `TimeToResetWarning`

### 📝 Documentation
- Added comprehensive README with usage examples
- Added detailed inline documentation for all structures and functions
- Added state machine diagrams and timing examples
- Added FAQ section covering common use cases

### 🎯 Use Cases
- Differentiation between isolated incidents and persistent problems
- Auto-recovery mechanism with escalation to manual intervention
- Network stability monitoring with retry logic
- Temperature monitoring with graduated response levels

## [1.0.0] - 2025-06-06

### 🎉 Initial Release
- Basic error detection with debouncing
- Support for up to 64 simultaneous error conditions per instance
- Configurable `TimeToErrorRegister` for error persistence validation
- 64-bit error register (`ErrorReg`)
- Multi-instance support for subsystem separation
- Flexible tick source (function or variable based)
- Custom time base type support (8/16/32/64-bit)
- Core API functions:
  - `EC_init()` - Instance initialization
  - `EC_poll()` - Periodic error checking
  - `EC_getErrors()` - Get all errors
  - `EC_getOneError()` - Get specific error state
  - `EC_checkError()` - Force immediate error check
  - `EC_clearErr()` - Clear all errors
- Runtime data tracking:
  - `LastReg` - Last registration timestamp
  - `LastNoErr` - Last error-free timestamp
- Minimal memory footprint optimized for embedded systems

---

## Migration Guide: v1.0.0 → v2.0.0

### Breaking Changes

The v2.0.0 warning system requires modifications to existing code:

#### 1. Update Error Definitions

**Before (v1.0.0):**
```c
typedef struct {
    EC_err_state_t (*ErrFunc)(uint16_t HelperNumber);
    uint16_t HelperNumber;
    EC_TIME_t TimeToErrorRegister;
} EC_error_t;

const EC_error_t errors[] = {
    {check_temp, 0, 1000}
};
```

**After (v2.0.0):**
```c
typedef struct {
    EC_err_state_t (*ErrFunc)(uint16_t HelperNumber);
    uint16_t HelperNumber;
    EC_TIME_t TimeToErrorRegister;
    EC_TIME_t TimeToResetWarning;    // NEW
    uint16_t WarningsToError;        // NEW
} EC_error_t;

const EC_error_t errors[] = {
    {check_temp, 0, 1000, 5000, 1}  // Add TimeToResetWarning and WarningsToError
};
```

#### 2. Maintain v1.0.0 Behavior

To keep the same behavior as v1.0.0 (immediate error registration):

```c
const EC_error_t errors[] = {
    // v1.0.0 equivalent: immediate error after debounce
    {check_function, 0, TimeToErrorRegister, 1000, 1}
    //                   ^                    ^    ^
    //                   |                    |    WarningsToError=1 (immediate)
    //                   |                    TimeToResetWarning (any value)
    //                   Original debounce time
};
```

#### 3. Runtime Data Structure

The runtime data structure has been extended (backward compatible in memory):

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
    uint8_t WarningCnt : 7;      // NEW
    uint8_t WarningPending : 1;  // NEW
} EC_runtimeData_t;
```

**Action Required:** Zero-initialize runtime data arrays (already recommended practice).

#### 4. Instance Structure

**v2.0.0 adds `WarningReg` field:**

```c
typedef struct {
    uint64_t ErrorReg;
    uint64_t WarningReg;         // NEW
    const EC_error_t *Errors;
    EC_runtimeData_t *RuntimeData;
    uint8_t NumberOfErrors;
} EC_instance_t;
```

**Action Required:** Zero-initialize instance structures (already recommended practice).

### New Features Available in v2.0.0

#### Warning Monitoring

```c
// Check if warnings are active
if (instance.WarningReg & 0x01) {
    uint8_t count = runtime[0].WarningCnt;
    printf("Warning level: %d\n", count);
}
```

#### Dual-Error Pattern

```c
// Auto-recovery + manual intervention pattern
const EC_error_t errors[] = {
    {check_fault, 0, 100, 500, 1},  // Immediate - triggers auto-recovery
    {check_fault, 0, 100, 500, 3}   // Persistent - requires manual fix
};
```

### Recommended Migration Path

1. **Minimal changes** (maintain v1.0.0 behavior):
   - Add `TimeToResetWarning` = any reasonable value (e.g., 1000)
   - Set `WarningsToError` = 1
   - No code logic changes needed

2. **Gradual adoption** (enable warnings for select errors):
   - Keep critical errors at `WarningsToError = 1`
   - Enable warnings for non-critical errors with `WarningsToError > 1`

3. **Full migration** (leverage warning system):
   - Analyze which errors benefit from warning escalation
   - Implement dual-error patterns where appropriate
   - Add warning monitoring to user interface

---

[2.0.0]: https://github.com/AdrianPietrzak1998/err_core/releases/tag/v2.0.0
[1.0.0]: https://github.com/AdrianPietrzak1998/err_core/releases/tag/v1.0.0
