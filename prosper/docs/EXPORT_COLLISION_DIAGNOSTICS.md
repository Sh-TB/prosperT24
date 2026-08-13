# Export Collision Diagnostics Layer

## Overview

**This document describes a diagnostics enhancement layer, NOT collision detection implementation.**

Export collision detection is **already fully implemented** in the Prosper PS4 emulator via:

- **Issue:** #1635 (Export Collision Detection)
- **PR:** #1670 (Merged upstream)
- **Implementation:** `prosper/src/loader/linker.hpp` and `linker.cpp`
- **Existing tests:** `prosper/tests/test_plugin_autolink.cpp`

### What This Layer Adds

This diagnostics layer provides **observability and evidence reporting** for collision data that the existing linker infrastructure already collects. It is a pure observer/aggregator with zero modifications to loader behavior.

### Design Principles

1. **Observer-Only:** No changes to `link_program()`, module loading, or HLE behavior
2. **FAILURE ≠ EMPTY:** Analysis failure is semantically distinct from empty result sets
3. **Dual Output:** Human-readable text + machine-readable JSON (CLI-ready)
4. **Deterministic:** Same input always produces identical output
5. **Zero Dependencies:** Uses only C++17 standard library

---

## Architecture

### Relationship to Existing Code

```
┌─────────────────────────────────────────────────────────────┐
│                    Existing Infrastructure (#1670)          │
│  ┌─────────────────┐    ┌─────────────────┐                │
│  │   linker.hpp    │    │   linker.cpp    │                │
│  │ - ExportCollision│    │-find_export_   │                │
│  │ - SkippedModule │    │ collision()    │                │
│  │ - AliasedExport │    │-module_export_ │                │
│  │                 │    │ nids()         │                │
│  └────────┬────────┘    └────────┬────────┘                │
│           │                      │                          │
│           ▼                      ▼                          │
│  ┌─────────────────────────────────────────┐                │
│  │           Program (link result)          │                │
│  │  - skipped_modules: vector<SkippedModule>│                │
│  │  - aliased_exports: vector<AliasedExport>│                │
│  └──────────────────────┬──────────────────┘                │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          │ (read-only access)
                          ▼
┌─────────────────────────────────────────────────────────────┐
│              THIS DIAGNOSTICS LAYER (NEW)                   │
│  ┌──────────────────────────────────────────────────┐       │
│  │  export_collision_diagnostics.hpp/.cpp           │       │
│  │  - analyze()                                    │       │
│  │  - CollisionStats                               │       │
│  │  - DiagnosticResult                             │       │
│  │  - ModuleImpact                                 │       │
│  └──────────────────────────┬───────────────────────┘       │
│                             │                               │
│              ┌──────────────┴──────────────┐                │
│              ▼                             ▼                │
│  ┌─────────────────────┐      ┌─────────────────────┐      │
│  │   toText()          │      │   toJson()          │      │
│  │   Human-readable    │      │   Machine-readable  │      │
│  │   console output    │      │   JSON for CLI/AI   │      │
│  └─────────────────────┘      └─────────────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

### Integration Point

The diagnostics layer integrates at the **Program level**, after `link_program()` completes:

```cpp
#include "loader/export_collision_diagnostics.hpp"

// After link_program() returns successfully:
auto& program = /* your linked Program */;

prosper::export_collision_diagnostics::DiagnosticResult result =
    prosper::export_collision_diagnostics::analyze(
        program.skipped_modules,  // From existing infrastructure
        program.aliased_exports,  // From existing infrastructure
        total_modules_loaded,
        failed_module_count,
        empty_export_count
    );

// Generate reports
std::cout << result.toText();     // Human-readable
std::string json = result.toJson(); // Machine-readable
```

---

## FAILURE ≠ EMPTY Semantics

### Critical Design Decision

This layer explicitly distinguishes between **analysis failure** and **empty results**:

| Status | Meaning | Is Error? | Action |
|--------|---------|-----------|--------|
| `COMPLETE` | Analysis finished, may have 0+ findings | No | Use results normally |
| `PARTIAL` | Some modules unavailable, partial data | Warning | Use with caution |
| `FAILED` | Analysis could not complete | **Yes** | Check `errorMessage` |
| `EMPTY_INPUT` | No modules provided for analysis | No | Valid state, nothing to analyze |

### Why This Matters

Consider these real scenarios:

**Scenario A: Clean System (EMPTY_INPUT or COMPLETE with 0 collisions)**
```
Status: COMPLETE
Collision events: 0
→ System is healthy, no action needed
```

**Scenario B: Data Quality Issue (FAILED)**
```
Status: FAILED
Error: Failed module count exceeds total module count
→ Input data is corrupted, do not trust any statistics
```

**Scenario C: Legitimate Empty Exports (COMPLETE with high empty rate)**
```
Status: COMPLETE
Modules analyzed: 30
Successful: 30
Empty exports: 18  ← NOT failures, just data-only modules
Collisions: 12
→ Normal PS4 system, many modules have no exports
```

### Implementation

```cpp
// Correct usage - ALWAYS check status first
auto result = analyze(...);

if (result.status == AnalysisStatus::FAILED) {
    std::cerr << "Analysis failed: " << result.errorMessage << std::endl;
    return 1;
}

// EMPTY_INPUT is valid - just means no modules to analyze
if (result.status == AnalysisStatus::EMPTY_INPUT) {
    std::cout << "No modules provided" << std::endl;
    return 0;
}

// Now safe to use statistics
std::cout << "Found " << result.stats.collisionEvents << " collisions" << std::endl;
```

---

## API Reference

### CollisionStats

Comprehensive statistics for diagnostic analysis:

```cpp
struct CollisionStats {
    // Input metrics
    size_t totalModulesChecked = 0;    // Total modules submitted
    
    // Success/Failure/Empty (mutually exclusive per module)
    size_t successfulModules = 0;      // Successfully analyzed
    size_t failedModules = 0;          // FAILED analysis (error state)
    size_t emptyExportModules = 0;     // Valid EMPTY export sets
    
    // Collision metrics
    size_t collisionEvents = 0;        // Total occurrences (with dups)
    size_t uniqueCollisionNIDs = 0;    // Distinct NIDs in collisions
    size_t skippedModules = 0;         // Modules skipped due to collision
    size_t aliasedExports = 0;         // Shadowed exports
    
    // Derived calculations
    double collisionRate() const;      // Events per analyzable module
    double failureRate() const;        // Failures per total module
    double emptyExportRate() const;    // Empty per total module
    
    bool hasCollisions() const noexcept;
    bool isSuccess() const noexcept;
    
    std::string toString() const;      // Debug string
    std::string toJson() const;        // JSON fragment
};
```

### DiagnosticResult

Complete analysis output:

```cpp
struct DiagnosticResult {
    AnalysisStatus status = AnalysisStatus::EMPTY_INPUT;
    CollisionStats stats;
    std::vector<ModuleImpact> affectedModules;
    std::vector<std::string> warnings;
    std::string errorMessage;          // Set if FAILED
    
    std::string toText() const;        // Full human report
    std::string toJson() const;        // Full JSON report
    bool isClean() const noexcept;     // True if no collisions
};
```

### Core Functions

```cpp
// Main analysis entry point
DiagnosticResult analyze(
    const std::vector<tuple<path, nid, owner>>& skippedModules,
    const std::vector<tuple<nid, winner, loser, wAddr, lAddr>>& aliasedExports,
    size_t totalModules,
    size_t failedCount = 0,
    size_t emptyExportCount = 0
);

// Factory functions with distinct semantics
DiagnosticResult emptyResult();                  // Valid empty (NOT error)
DiagnosticResult failedResult(string message);   // Explicit error state
```

---

## Output Formats

### Text Report (toText)

Human-readable format suitable for console output:

```
Export Collision Diagnostic Report
====================================

Status:
  COMPLETE

Modules analyzed:
  30

Successful analysis:
  30

Failed analysis:
  0

Modules with zero exports:
  18

Collision events:
  41

Unique conflicting NIDs:
  21

Skipped modules:
  5

Aliased exports:
  36

Rates:
  Collision rate: 266.67%
  Failure rate: 0.00%
  Empty export rate: 60.00%

Affected modules:
  /system/common/libfmod.prx
    Collisions: 8
    Status: HAS_ALIASED_EXPORTS
  /system/common/libfmodL.prx
    Collisions: 8
    Status: HAS_ALIASED_EXPORTS

Warnings:
  ⚠ Aliased exports detected—behavior may be load-order dependent
  ⚠ More than half of analyzed modules have zero exports

Severity assessment:
  🔶 ELEVATED - Aliased exports detected (load-order dependent)
```

### JSON Output (toJson)

Machine-readable format for tooling consumption:

```json
{
  "status": "COMPLETE",
  "statistics": {
    "total_modules_checked": 30,
    "successful_modules": 30,
    "failed_modules": 0,
    "empty_export_modules": 18,
    "collision_events": 41,
    "unique_collision_nids": 21,
    "skipped_modules": 5,
    "aliased_exports": 36,
    "collision_rate": 2.6667,
    "failure_rate": 0.0000,
    "empty_export_rate": 0.6000
  },
  "affected_modules": [
    {
      "path": "/system/common/libfmod.prx",
      "collision_count": 8,
      "was_skipped": false,
      "has_aliased_exports": true
    },
    {
      "path": "/system/common/libfmodL.prx",
      "collision_count": 8,
      "was_skipped": false,
      "has_aliased_exports": true
    }
  ],
  "warnings": [
    "Aliased exports detected—behavior may be load-order dependent",
    "More than half of analyzed modules have zero exports"
  ]
}
```

### JSON Schema Stability

**All JSON field names are documented as STABLE for long-term compatibility:**

| Field Path | Type | Stability | Description |
|------------|------|-----------|-------------|
| `status` | string | STABLE | One of: COMPLETE, PARTIAL, FAILED, EMPTY_INPUT |
| `statistics.total_modules_checked` | integer | STABLE | Total input count |
| `statistics.successful_modules` | integer | STABLE | Successfully analyzed |
| `statistics.failed_modules` | integer | STABLE | Analysis failures (error state) |
| `statistics.empty_export_modules` | integer | STABLE | Valid empty export sets |
| `statistics.collision_events` | integer | STABLE | Total collision occurrences |
| `statistics.unique_collision_nids` | integer | STABLE | Distinct conflicting NIDs |
| `statistics.skipped_modules` | integer | STABLE | Modules skipped by loader |
| `statistics.aliased_exports` | integer | STABLE | Shadowed exports |
| `statistics.collision_rate` | float | STABLE | Events per analyzable (4 decimal precision) |
| `statistics.failure_rate` | float | STABLE | Failure ratio (4 decimal precision) |
| `statistics.empty_export_rate` | float | STABLE | Empty ratio (4 decimal precision) |
| `affected_modules[]` | array | STABLE | Per-module impact details |
| `affected_modules[].path` | string | STABLE | Module filesystem path |
| `affected_modules[].collision_count` | integer | STABLE | Collisions involving this module |
| `affected_modules[].was_skipped` | boolean | STABLE | If module was fully skipped |
| `affected_modules[].has_aliased_exports` | boolean | STABLE | If module has shadowed exports |
| `warnings[]` | array of strings | STABLE | Non-fatal contextual warnings |
| `error_message` | string (optional) | STABLE | Present only when status=FAILED |

**Stability Guarantees:**
- Field names will use `snake_case` permanently
- Numeric types will not change (int stays int, float stays float)
- New optional fields may be added; existing fields will not be removed
- Array order is deterministic (sorted by module path)

---

## Usage Examples

### Basic Console Reporting

```cpp
#include "loader/export_collision_diagnostics.hpp"

void report_collisions(const Program& program) {
    using namespace prosper::export_collision_diagnostics;
    
    auto result = analyze(
        program.skipped_modules,
        program.aliased_exports,
        program.modules.size()
    );
    
    // Always output to console
    std::cout << result.toText() << std::endl;
    
    // Exit with error code if analysis failed
    if (result.status == AnalysisStatus::FAILED) {
        std::exit(1);
    }
}
```

### CI/CD Integration

```cpp
#include "loader/export_collision_diagnostics.hpp"
#include <fstream>

void generate_ci_artifact(const Program& program) {
    using namespace prosper::export_collision_diagnostics;
    
    auto result = analyze(
        program.skipped_modules,
        program.aliased_exports,
        program.modules.size()
    );
    
    // Write JSON artifact for CI consumption
    std::ofstream out("collision_report.json");
    out << result.toJson();
    out.close();
    
    // Fail CI if critical issues
    if (result.stats.failureRate() > 0.2) {
        std::cerr << "CRITICAL: High failure rate detected" << std::endl;
        std::exit(1);
    }
}
```

### Conditional Alerting

```cpp
#include "loader/export_collision_diagnostics.hpp"

void check_collision_health(const Program& program) {
    using namespace prosper::export_collision_diagnostics;
    
    auto result = analyze(
        program.skipped_modules,
        program.aliased_exports,
        program.modules.size()
    );
    
    // Only alert on actionable findings
    if (!result.isClean()) {
        if (result.stats.aliasedExports > 0) {
            log_warning("Load-order dependent aliases detected");
        }
        
        if (result.stats.failureRate() > 0.1) {
            log_error("High analysis failure rate");
        }
    }
}
```

---

## Testing Strategy

### Test File: `test_export_collision_diagnostics.cpp`

**10 Test Categories (~40 test cases):**

1. **AnalysisStatus Enum Validation**
   - toString() correctness for all 4 states
   
2. **CollisionStats Structure Tests**
   - Default construction (all zeros)
   - hasCollisions() / isSuccess() predicates
   - Rate calculations (edge cases)

3. **FAILURE ≠ EMPTY Semantics (CRITICAL)**
   - emptyResult() vs failedResult() are distinct
   - Status propagation through analyze()
   - Error message handling
   - 8 dedicated tests for this semantic

4. **Core analyze() Scenarios**
   - Empty input handling
   - Single module
   - Multiple modules with mixed outcomes
   - Data integrity validation

5. **Module Impact Tracking**
   - Per-module collision counting
   - Skip/alias flag accuracy
   - Deterministic sorting

6. **Text Report Generation**
   - Format structure verification
   - Severity assessment logic
   - Warning inclusion criteria

7. **JSON Output Stability**
   - Field name consistency
   - Type correctness
   - Numeric precision

8. **Deterministic Output Guarantee**
   - Same input → same output (run multiple times)
   - Array ordering stability

9. **Edge Cases & Boundary Conditions**
   - Zero divisions protected
   - Empty vectors handled
   - Large input scaling

10. **Warning Generation Triggers**
    - High empty rate (>50%)
    - High failure rate (>10%)
    - Load-order dependency detection

### Running Tests

```bash
# Run only diagnostics tests
./prosper_tests --gtest_filter="*CollisionDiagnostics*"

# Run with verbose output
./prosper_tests --gtest_filter="*CollisionDiagnostics*" --gtest_print_time=1
```

---

## Risk Assessment

### What This Changes

| Component | Change Type | Risk Level |
|-----------|-------------|------------|
| `linker.hpp` | **NONE** | ✅ Safe |
| `linker.cpp` | **NONE** | ✅ Safe |
| `link_program()` behavior | **NONE** | ✅ Safe |
| Module loading policy | **NONE** | ✅ Safe |
| HLE/Runtime | **NONE** | ✅ Safe |
| New header file | Additive only | ✅ Low risk |
| New source file | Additive only | ✅ Low risk |
| New test file | Additive only | ✅ No risk |
| Documentation | Additive only | ✅ No risk |

### Dependencies

- **C++17 Standard Library** (only)
- **Google Test** (for tests only, existing dependency)
- **Zero new external dependencies**

### Backwards Compatibility

- **100% backwards compatible** - no existing APIs modified
- **Purely additive** - all new code in new files
- **No build system changes** required (files can be added to existing targets)

---

## Comparison: Detection vs Diagnostics

| Aspect | #1670 Detection (Existing) | This Diagnostics Layer (New) |
|--------|---------------------------|------------------------------|
| **Purpose** | Find duplicate NID exports | Report on found duplicates |
| **When it runs** | During `link_program()` | After linking completes |
| **What it modifies** | May skip/alias modules | Nothing (read-only) |
| **Output** | Internal data structures | Text + JSON reports |
| **Failure handling** | Loader-level decisions | Analysis status tracking |
| **Test coverage** | In `test_plugin_autolink.cpp` | Standalone test suite |
| **Dependencies** | Linker internals | Only standard library |

---

## Future Considerations (Not Implemented)

These are potential enhancements explicitly **NOT included** in this PR:

1. **CLI Tool Wrapper** - Command-line interface for standalone execution
2. **WebSocket Streaming** - Real-time collision monitoring
3. **Historical Tracking** - Persistence of collision trends over time
4. **Visual Dashboard** - Web-based collision visualization
5. **Automated Remediation** - Suggested fixes for collision patterns

These are mentioned here to document the design space without expanding scope.

---

## Related Issues

- **#1635** - Original export collision detection issue
- **#1670** - Upstream PR implementing detection (MERGED)
- **#1609** - Related plugin loading improvements

---

## File Manifest

| File | Lines | Purpose |
|------|-------|---------|
| `src/loader/export_collision_diagnostics.hpp` | ~463 | API definition |
| `src/loader/export_collision_diagnostics.cpp` | ~211 | Implementation |
| `tests/test_export_collision_diagnostics.cpp` | ~718 | Comprehensive tests |
| `docs/EXPORT_COLLISION_DIAGNOSTICS.md` | ~493 | This documentation |
| **Total** | **~1885** | Complete diagnostics layer |
