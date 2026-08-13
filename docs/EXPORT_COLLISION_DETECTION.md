# Export Collision Detection for Prosper

## Overview

Export Collision Detection is a critical subsystem of Prosper's dynamic linker that prevents ambiguous symbol resolution when multiple PS4 modules export identical NIDs (Name/ID hashes). This document describes the problem, solution, and API for collision detection and handling.

## Problem Statement

### The Core Issue

PS4 games may ship **multiple builds of the same library** with identical exports:

| Game | Modules | Conflict |
|------|---------|----------|
| Evergate (PPSA01885) | `libfmod.prx` + `libfmodL.prx` | FMOD release + logging builds |
| Multiple titles | `PSNCore.prx` + `PSNCommon.prx` | 4 shared NIDs |
| Audio titles | `libfmod.prx` + `libfmodstudio.prx` | 16 shared FMOD NIDs |
| Wwise titles | `AkMotion` + `AkSoundEngine` + `AkVorbisHwAccelerator` | 1 three-way collision |

### What Happens Without Detection

1. Both modules are linked, mapped, and initialized
2. Global export table uses **first-definition-wins** (silent)
3. `sceKernelDlsym()` returns different addresses depending on module handle
4. Runtime behavior becomes **load-order dependent**
5. Potential for:
   - Double initialization (two init_arrays run)
   - Inconsistent function pointer resolution
   - Subtle, hard-to-debug race conditions

### Real-World Impact

A census over 30 local game dumps found:
- **41 aliased NIDs** across 7 titles
- All among modules linked by name (not auto-discovered)
- **Current severity: None** (no title imports these aliased NIDs)
- **Future risk: High** (new titles might import them)

## Solution Architecture

### Design Principles

1. **Deterministic**: Same input → same output every time
2. **First-definition-wins**: Simple, predictable policy
3. **Diagnostic**: Report all collisions, don't just silently handle
4. **Opt-in skip**: Optional modules can be skipped on collision
5. **Per-module tables**: `sceKernelDlsym` checks handle's own exports first (#147)

### Data Flow

```
Boot Sequence:
┌─────────────┐
│ Link Inputs │ (ordered list of modules to load)
└──────┬──────┘
       ▼
┌─────────────────┐
│ For each module │◄──────────────────┐
└──────┬──────────┘                   │
       ▼                              │
┌──────────────────┐    ┌─────────────┴─────────────┐
│ Check Collisions │───▶│ Collision found?          │
└──────┬───────────┘    └──────┬──────────────┬────┘
       │                       │              │
       ▼                  Yes  │              │ No
┌──────────────┐             ▼              ▼
│ Claim Exports │    ┌─────────────┐  ┌──────────────┐
│ (add to map)  │    │ Skip Module │  │ Accept Module│
└──────────────┘    │ (optional)  │  │ Claim NIDs   │
                    └─────────────┘  └──────────────┘
```

### Key Data Structures

```cpp
// Track which NIDs are already claimed and by whom
std::unordered_map<std::string, std::string> claimed;  // nid -> owner_path

// Record of skipped modules
struct SkippedModule {
    std::string path;        // Which module was skipped
    std::string nid;         // Which NID caused the collision
    std::string owner_path;  // Which module already owns it
};

// Record of aliased exports (accepted but shadowed)
struct AliasedExport {
    std::string nid;           // The aliased NID
    std::string winner_path;   // First definition (wins)
    std::string loser_path;    // Later definition (shadowed)
    uint64_t winner;           // Winner's guest address
    uint64_t loser;            // Loser's guest address
};
```

## API Reference

### Core Functions

#### `module_export_nids()`

```cpp
std::vector<std::string> module_export_nids(const Module& m);
```

Get all exported NIDs from a module. Returns defined (non-import), NID-bearing symbols with nonzero values.

#### `find_export_collision()`

```cpp
ExportCollision find_export_collision(
    const Module& m,
    const std::unordered_map<std::string, string>& claimed);
```

Find first colliding export. Scans in file order for determinism.

**Returns:** `ExportCollision` with details, or empty if no collision.

### Diagnostic Functions

#### `generateCollisionReport()`

```cpp
namespace export_collision {
    std::string generateCollisionReport(
        const std::vector<SkippedModule>& skipped,
        const std::vector<AliasedExport>& aliased,
        size_t totalModules);
}
```

Generate human-readable collision report for logging.

#### `calculateStats()`

```cpp
CollisionStats calculateStats(
    const std::vector<SkippedModule>& skipped,
    const std::vector<AliasedExport>& aliased,
    size_t totalModules);
```

Calculate summary statistics.

### Statistics Structure

```cpp
struct CollisionStats {
    size_t totalModulesChecked;
    size_t modulesSkipped;
    size_t uniqueCollisions;
    size_t aliasedExports;
    size_t totalExportsClaimed;
    
    double skipRate();  // modulesSkipped / totalModulesChecked
    std::string toString();
};
```

## Usage Examples

### Basic Collision Detection

```cpp
#include "loader/export_collision.hpp"

// During boot sequence:
std::unordered_map<std::string, std::string> claimedNIDs;

for (const auto& input : linkInputs) {
    Module* mod = load_module(input.path);
    
    // Check for collisions before accepting
    auto collision = find_export_collision(*mod, claimedNIDs);
    if (!collision.isEmpty()) {
        // Handle collision
        if (input.skip_on_export_collision) {
            program.skipped_modules.push_back({
                input.path, collision.nid, collision.owner_path
            });
            continue;  // Skip this module
        }
    }
    
    // Accept module: claim its exports
    auto nids = module_export_nids(*mod);
    for (const auto& nid : nids) {
        claimedNIDs[nid] = input.path;
    }
    
    program.mods.push_back(mod);
}
```

### Generating Reports

```cpp
// After linking complete:
auto stats = calculateStats(
    program.skipped_modules,
    program.aliased_exports,
    linkInputs.size()
);

if (stats.modulesSkipped > 0 || stats.aliasedExports > 0) {
    std::string report = generateCollisionReport(
        program.skipped_modules,
        program.aliased_exports,
        linkInputs.size()
    );
    
    printf("Collision Report:\n%s\n", report.c_str());
    printf("Stats: %zu/%zu modules skipped (%.1f%%)\n",
           stats.modulesSkipped, stats.totalModulesChecked,
           stats.skipRate() * 100);
}
```

### Example Output

```
=== Export Collision Report ===
Total modules processed: 15

Skipped Modules (export collision):
  [SKIP] /Media/Plugins/libfmodL.prx
         Colliding NID: FMOD_System_Create
         Already owned by: /Media/Plugins/libfmod.prx

Aliased Exports (shadowed):
  [ALIAS] NID PSN_PrxInitialize
         Winner: /Media/Plugins/PSNCore.prx (0x4a0000100)
         Loser: /Media/Plugins/PSNCommon.prx (0x4b0000200)

Summary:
  Modules checked: 15
  Modules skipped: 1
  Unique collisions: 1
  Aliased exports: 4
  Total exports claimed: 847
  Skip rate: 6.7%
```

## Integration Points

### Boot Sequence (`boot_program.cpp`)

The primary integration is during module loading:

```cpp
// For optional/auto-linked plugins:
in.insert(in.begin() + slot, { path, base, true });  // true = skip_on_export_collision
```

When `skip_on_export_collision=true`, the linker will:
1. Check if any exported NID is already claimed
2. If yes, skip the module (don't link, don't init)
3. Record in `program.skipped_modules` for reporting

### Linker (`linker.cpp`)

The linker implements the actual detection:

```cpp
// Pseudocode from linker.cpp:
for (const auto& input : inputs) {
    Module mod = parse_elf(input.path);
    auto collision = find_export_collision(mod, claimed);
    
    if (!collision.isEmpty() && input.skip_on_export_collision) {
        out.skipped_modules.push_back({input.path, collision.nid, collision.owner_path});
        continue;  // Skip
    }
    
    // Accept: add exports to global table
    for (auto& nid : module_export_nids(mod)) {
        auto [it, inserted] = out.exports.emplace(nid, address);
        if (!inserted) {
            // This is an alias (not a skip - module still linked)
            out.aliased_exports.push_back({nid, it->second.owner, input.path, ...});
        }
    }
}
```

## Testing

The test suite includes **35+ tests** covering:

| Category | Count | Description |
|----------|-------|-------------|
| Structure Tests | 12 | Data structure construction, toString |
| Stats Tests | 5 | Calculation, edge cases |
| Report Tests | 4 | Generation, formatting |
| Scenario Tests | 3 | Real game patterns (Evergate, etc.) |
| Edge Cases | 6 | Empty inputs, large data, duplicates |
| Workflow Tests | 2 | Full boot simulation |

### Running Tests

```bash
cmake -B build -DPROSPER_TESTS=ON
cmake --build build --target test_export_collision
./build/tests/test_export_collision
```

## Configuration Options

### Environment Variables

| Variable | Effect | Default |
|----------|--------|---------|
| (none) | Collision detection always active | — |

### Compile-Time Options

| Option | Effect |
|--------|--------|
| `PROSPER_STRICT_COLLISIONS` | Treat aliases as errors (not implemented) |

## Known Limitations

1. **First-wins policy**: May not always pick the "best" implementation
2. **No version awareness**: Can't distinguish compatible vs incompatible duplicates
3. **Order dependency**: Results depend on module processing order
4. **Memory overhead**: Stores full skip/alias lists for diagnostics

## Performance Characteristics

### Time Complexity

- Per module: O(E) where E = number of exports
- Lookup: O(1) average (hash table)
- Total: O(M × E) where M = modules, E = avg exports/module

### Memory Usage

- `claimed` map: O(total unique NIDs across all modules)
- `skipped_modules`: O(skipped modules)
- `aliased_exports`: O(aliased NIDs)

Typical values for a game:
- ~50-100 modules
- ~500-2000 unique NIDs
- 0-10 skipped modules
- 0-50 aliased exports

## Related Issues

| Issue | Title | Status |
|-------|-------|--------|
| #1609 | Plugin auto-link discovery | Fixed |
| #1635 | Aliased export reporting | Fixed |
| #147 | Per-module dlsym tables | Fixed |

## Future Enhancements (Deferred)

These are explicitly not part of this upstream candidate:

- [ ] Collision resolution strategies (newest-wins, largest-wins, etc.)
- [ ] Semantic version comparison for duplicate libraries
- [ ] Warning mode (report but don't skip)
- [ ] Graph visualization of module dependencies
- [ ] Heuristic detection of debug vs release builds

## References

- [tools/re/dup_exports.py](../tools/re/dup_exports.py) - Census script for finding duplicates
- [linker.hpp](../src/loader/linker.hpp) - Primary integration point
- [Issue #1609](https://github.com/mattias800/prosper/issues/1609) - Original feature request

## Changelog

### v1.0.0 (Upstream Candidate)
- Extracted to dedicated header (`export_collision.hpp`)
- Comprehensive test suite (35+ tests)
- Full documentation
- Diagnostic report generation
- Statistics calculation utilities
- Real-world scenario tests (Evergate FMOD, PSN variants)
