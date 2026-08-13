# Case-Correct Path Resolution for Prosper

## Overview

This module provides case-insensitive path resolution for the Prosper PS4 emulator, enabling games to load correctly on Linux hosts with case-sensitive filesystems even when game assets use inconsistent file naming conventions.

## Problem Statement (Issue #1006)

### The Bug

Unity-based PS4 games ship IL2CPP modules with **inconsistent casing**:

| Game | On-Disk Filename |
|------|------------------|
| The Messenger | `Il2cppUserAssemblies.prx` (lowercase 'c') |
| Blasphemous 2 | `Il2CppUserAssemblies.prx` (uppercase 'C') |
| Evergate | `Il2CppUserAssemblies.prx` (uppercase 'C') |

Prosper's boot code hardcodes one casing in its preload list. When the actual file differs:

1. `fopen(".../Il2CppUserAssemblies.prx")` fails silently
2. Module is dropped as "absent"
3. Guest's `sceKernelLoadStartModule()` returns ENOENT
4. Runtime null-jumps to address 0 → **SIGSEGV (crash)**

### Why It "Worked" Before

- **Windows (NTFS)**: Case-insensitive → wrong spelling still opens
- **macOS (APFS)**: Default is case-insensitive → same behavior  
- **Linux (ext4/xfs)**: **Case-sensitive** → reveals the bug

## Solution Design

### Algorithm

```
resolve_host_path_case(want):
    if want.empty() or exists(want):
        return want                          // Fast path: already works
    
    parent = dirname(want)
    base = basename(want)
    
    if parent == want:                       // No more ancestors
        return want
    
    dir = resolve_host_path_case(parent)     // Recurse: fix parents first
    
    for entry in listdir(dir):              // Search for case match
        if case_insensitive_eq(entry.name, base):
            return dir / entry.real_name
    
    // No match: return best effort (parent may be corrected)
    if dir != original_parent:
        return dir / base                   # Corrected parent, original leaf
    return want                             # Nothing changed
```

### Key Properties

| Property | Guarantee |
|----------|-----------|
| Never throws | Uses `error_code` overloads throughout |
| Idempotent | Calling twice gives same result |
| Preserves separators | Returns host-native format, not rewritten |
| Absent-safe | Missing files returned unchanged |

### Behavior by Filesystem Type

| Filesystem | Example | Behavior |
|------------|---------|----------|
| Case-sensitive | Linux ext4 | Corrects mismatched components |
| Case-insensitive | Windows NTFS | Returns input unchanged |
| Mixed | WSL DrvFs | Depends on mount options |

## API Reference

### Primary Function

```cpp
namespace prosper {
    std::string resolve_host_path_case(const std::string& want);
}
```

**Parameters:**
- `want`: Requested path (may have incorrect casing)

**Returns:** Path with corrected casing, or original if no correction possible

**Throws:** Never

### Plugin Discovery

```cpp
std::vector<std::string> discover_extra_plugin_modules(
    const std::string& dump_root,
    const std::vector<std::string>& listed_basenames);
```

**Parameters:**
- `dump_root`: Game dump root directory
- `listed_basenames`: Already-known module names (lowercase)

**Returns:** Paths to undiscovered `.prx` files, sorted descending by name

## Usage Examples

### Basic Usage

```cpp
#include "host/path_case_resolution.hpp"

// In boot_program.cpp or similar:
std::string modulePath = dumpRoot + "/Media/Modules/Il2cppUserAssemblies.prx";
std::string resolved = resolve_host_path_case(modulePath);

// resolved now has correct casing for actual filesystem
FILE* f = fopen(resolved.c_str(), "rb");
if (f) {
    // Load module...
}
```

### With Plugin Auto-Discovery

```cpp
std::vector<std::string> listed;
for (const auto& m : fixedModules) {
    listed.push_back(lowercase(basename(m.path)));
}

auto extraPlugins = discover_extra_plugin_modules(dumpRoot, listed);
for (const auto& plugin : extraPlugins) {
    link_module(plugin);  // Auto-link newly discovered plugins
}
```

## Testing

The test suite covers **30+ test cases** across categories:

### Test Categories

| Category | Tests | Description |
|----------|-------|-------------|
| Exact Match | 2 | No correction needed |
| #1006 Scenario | 3 | Wrong-case filename resolution |
| Directory Correction | 2 | Wrong-case intermediate dirs |
| Absent Files | 2 | Graceful handling of missing files |
| Length Mismatch | 2 | Don't match different-length names |
| #1236 Fix | 1 | Absent leaf with corrected parent |
| Edge Cases | 4 | Empty input, unicode, root path |
| Plugin Discovery | 7 | Extra module finding |
| Comparison Helper | 7 | Unit tests for string comparison |
| Real Scenarios | 1 | Messenger game structure |

### Running Tests

```bash
# Build
cmake -B build -DPROSPER_TESTS=ON
cmake --build build --target test_path_case_resolution

# Run
./build/tests/test_path_case_resolution

# With verbose output
./build/tests/test_path_case_resolution --gtest_print_time=1
```

### Expected Output

```
[==========] Running 31 tests from 8 test suites.
[----------] Global test environment set-up.
[ PASSED ] PathCaseResolutionTest.ExactCasePathReturnedUnchanged
[ PASSED ] PathCaseResolutionTest.WrongCaseFilename_ResolvesToExistingFile
[ PASSED ] PathCaseResolutionTest.AbsentFile_ReturnedUnchanged
...
[==========] 31 tests ran. (12 ms total)
[  PASSED  ] 31 tests.
```

## Integration Points

### Boot Sequence

The primary integration is in `boot_program.cpp`:

```cpp
// For each module in the preload list:
for (size_t i = in.size(); i-- > 1; ) {
    std::string resolved = resolve_host_path_case(in[i].path);
    if (resolved != in[i].path) {
        printf("module path case-corrected: %s -> %s\n",
               in[i].path.c_str(), resolved.c_str());
        in[i].path = std::move(resolved);
    }
    // Then check existence and load...
}
```

### Plugin Auto-Link

Used when discovering extra plugins not in the fixed list:

```cpp
const std::string pluginsDir = resolve_host_path_case(dump_root + "/Media/Plugins");
// Scan pluginsDir for .prx files...
```

## Cross-Platform Notes

### Windows

On NTFS/APFS, `std::filesystem::exists()` returns true for wrong-case paths, so the function's fast path returns immediately without scanning directories. This is correct behavior—no correction needed.

### macOS

Default APFS format is case-insensitive. Can be formatted as case-sensitive ("APFS (Case-sensitive)") which will exercise the correction logic.

### Linux

All common filesystems (ext4, xfs, btrfs) are case-sensitive by default. This is where the fix is critical.

### WSL (Windows Subsystem for Linux)

Depends on `/drvfs` mount options:
- `/mnt/c` usually mounted as case-insensitive (DrvFs default)
- Linux filesystems (`/home`) are case-sensitive

## Performance Characteristics

### Time Complexity

- **Best case** (O(1)): Exact match or case-insensitive FS
- **Worst case** (O(d × n)): d=directory depth, n=entries per directory
- **Typical**: 2-3 directory scans for game paths

### Optimization Opportunities (Deferred)

- [ ] Cache directory listings during boot sequence
- [ ] Hash table lookup for large directories
- [ ] Early-exit if FS confirmed case-insensitive

## Known Limitations

1. **Symlinks**: Follows symlinks; may behave unexpectedly with cyclic links
2. **Race conditions**: Directory could change between exists() check and iteration
3. **Network filesystems**: May be slow due to latency per directory entry
4. **Very long paths**: Limited by PATH_MAX (typically 4096 on Linux)

## Related Issues

| Issue | Title | Status |
|-------|-------|--------|
| #1006 | Case-sensitive host drops modules | Fixed |
| #1236 | Absent leaf under corrected parent | Fixed |
| #1609 | Plugin auto-link discovery | Fixed |
| #2199 | Single definition of link set | Fixed |

## Future Work

These enhancements are explicitly deferred:

- [ ] Case correction cache for repeated lookups
- [ ] Configurable strictness mode (warn vs. silent)
- [ ] Statistics output (how many corrections applied per boot)
- [ ] Support for Unicode normalization (NFC vs NFD)
- [ ] Case folding per locale (Turkish İ/ı special case)

## References

- [POSIX filesystem case sensitivity](https://pubs.opengroup.org/onlinepubs/9699919799/)
- [C++17 std::filesystem](https://en.cppreference.com/w/cpp/filesystem)
- [Original issue #1006](https://github.com/mattias800/prosper/issues/1006)

## Changelog

### v1.1.0 (Upstream Candidate)
- Extracted to dedicated header (`path_case_resolution.hpp`)
- Enhanced test suite (30+ tests, up from 8)
- Added comprehensive documentation
- Plugin discovery tests included
- Edge case coverage improved

### v1.0.0 (Original Implementation)
- Initial `resolve_host_path_case()` in boot_program.cpp
- Basic test coverage (8 tests)
- Fixes #1006 and #1236
