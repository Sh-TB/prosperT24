# IL2CPP Tools for Prosper PS4 Emulator

A collection of utilities for working with Unity IL2CPP-based PS4 games, enabling symbolication of managed method calls during emulation debugging.

## Overview

When PS4 games are built with Unity's IL2CPP scripting backend, C# code is compiled to native C++. This makes debugging difficult because:
- Stack traces show raw addresses instead of method names
- Breakpoints require knowing exact RVAs (Relative Virtual Addresses)
- The relationship between C# source and native code is obscured

These tools recover that information by parsing Unity's `global-metadata.dat` file and cross-referencing it with the game's compiled modules.

## Tools

### 1. prx_to_elf.py

Converts an unencrypted PS5 SELF/PRX module into a loadable ELF file suitable for analysis with `Il2CppDumper`.

**Purpose:** Il2CppDumper requires a standard ELF input, but PS5 modules use the SELF format with custom program headers.

**Usage:**
```bash
# Basic conversion (for dumping symbols)
python3 prx_to_elf.py <input.prx> <output.elf>

# With section headers (for disassembly)
python3 prx_to_elf.py <input.prx> <output.elf> --sections
```

**Output:**
- Flattened ELF with `p_offset == p_vaddr` (file offset = virtual address)
- Optional: Section header table for `objdump -d` compatibility

**Example:**
```bash
python3 prx_to_elf.py Media/Modules/Il2CppUserAssemblies.prx /tmp/il2cpp.elf
```

### 2. resolve.py

Maps runtime addresses or backtrace frames to C# method names using Il2CppDumper output.

**Purpose:** Translates prosper's `[btrace]` output into readable method names.

**Usage:**
```bash
# Resolve individual addresses (RVAs)
python3 resolve.py script.json 0x16b981 0x11e63c

# Resolve btrace chain
python3 resolve.py script.json il+0x16b981,il+0x11e63c

# Resolve from stdin (pipe)
echo '[btrace] ... chain=il+0x1e23b8,il+0xde92e9' | python3 resolve.py script.json -

# With explicit base address
python3 resolve.py script.json --base 0x440000000 0x4402140d0
```

**Input Formats:**
| Format | Example | Description |
|--------|---------|-------------|
| Bare RVA | `0x16b981` | Relative virtual address |
| Btrace frame | `il+0x16b981` | From prosper `[btrace]` output |
| Eboot frame | `eb+0xada254` | Native eboot address (noted as such) |
| Absolute | `0x4402140d0` | Runtime address (with `--base`) |

**Output:**
```
il+0x1764ce2        Unity.PSN.PS5.Async.WorkerThread$$RunProc (+0xa2)
il+0xde9159         UnityEngine.SceneManagement.LoadScene$$MoveNext (+0x12)
eb+0xada254         (eboot / Unity native — not managed)
```

## Complete Workflow

### Prerequisites

- Python 3.6+
- [Il2CppDumper](https://github.com/Perfare/Il2CppDumper) (v6.7.46 or compatible)
- .NET SDK (for running Il2CppDumper)

### Step-by-Step Guide

#### 1. Extract Module from Game Dump

```bash
# Locate the IL2CPP module (name varies by game)
ls <dump>/Media/Modules/
# Common names: Il2CppUserAssemblies.prx, Il2cppUserAssemblies.prx
```

#### 2. Convert to ELF

```bash
python3 tools/il2cpp/prx_to_elf.py \
    <dump>/Media/Modules/Il2CppUserAssemblies.prx \
    /tmp/il2cpp.elf
```

#### 3. Run Il2CppDumper

```bash
# Download if needed
cd /tmp
curl -sSL https://github.com/Perfare/Il2CppDumper/releases/download/v6.7.46/Il2CppDumper-net7-v6.7.46.zip -o d.zip
python3 -c "import zipfile; zipfile.ZipFile('d.zip').extractall('dumper')"

# Execute (answer "0" to prompts for auto-scan)
cd /tmp/dumper && DOTNET_ROLL_FORWARD=LatestMajor \
  sh -c 'printf "0\n0\n0\n" | dotnet Il2CppDumper.dll /tmp/il2cpp.elf \
    <dump>/Media/Metadata/global-metadata.dat /tmp/il2cpp_out'
```

**Note:** The tool may print `Cannot read keys` at the end—this is harmless and can be ignored.

#### 4. Symbolicate Addresses

```bash
# Single address
python3 tools/il2cpp/resolve.py /tmp/il2cpp_out/script.json il+0x2140d0
# Output: DoFirstLogin  (+0x0)

# Backtrace chain
echo '[btrace] chain=il+0x1764ce2,il+0xde92e9' | \
  python3 tools/il2cpp/resolve.py /tmp/il2cpp_out/script.json -
```

## Understanding Addresses

### Address Layout

For IL2CPP modules loaded by Prosper:

```
Runtime Address = Module Base + RVA
                 = 0x440000000 + script.json["Address"]
```

**Common Module Bases:**
| Module | Typical Base |
|--------|--------------|
| Il2CppUserAssemblies.prx | `0x440000000` |
| eboot.bin | `0x400000000` |

### prosper Backtrace Format

Prosper's validated unwinder outputs frames as:

```
[btrace] ... chain=il+0x<offset>,il+0x<offset>,...
```

Where each `offset` is already an RVA from the module base.

**Important:** The unwinder must accept **indirect call sites** (`0xFF` at v-2/-3/-6/-7), not just `0xE8` (direct `call rel32`). IL2CPP dispatches managed methods indirectly, so an `0xE8`-only filter drops every managed frame.

## Advanced Usage

### Disassembly with Sections

The `--sections` flag synthesizes section headers for binutils compatibility:

```bash
# Convert with sections
python3 prx_to_elf.py module.prx module.elf --sections

# Now objdump can disassemble
objdump -d --start-address=0x2140d0 --stop-address=0x214100 module.elf
```

**Why sections matter:**
- Without sections: `objdump -f/-p/-T` work, but `-d` prints nothing
- With sections: Full disassembly with real virtual addresses
- Default output (without `--sections`) is byte-identical to original

### GDB Integration

When debugging under GDB, always set:

```
handle SIGSEGV SIGILL SIGBUS nostop noprint pass
```

This ensures recoverable guest faults reach Prosper's handler rather than being caught by GDB.

## File Reference

### Input Files

| File | Source | Description |
|------|--------|-------------|
| `*.prx` | Game dump | Unencrypted PS5 module |
| `global-metadata.dat` | Game dump | Unity type/metadata database |

### Output Files

| File | Source | Description |
|------|--------|-------------|
| `*.elf` | prx_to_elf.py | Flattened loadable ELF |
| `script.json` | Il2CppDumper | Machine-readable method list |
| `dump.cs` | Il2CppDumper | Human-readable method dump (C#) |
| `il2cpp.h` | Il2CppDumper | C++ header with method declarations |

**⚠️ Do not commit** `dump.cs`, `script.json`, or `il2cpp.h` to version control—they are derived from game dumps and should be gitignored.

## Troubleshooting

### Common Issues

| Problem | Cause | Solution |
|---------|-------|----------|
| `file format not recognized` | Missing `--sections` or wrong ELF fields | Use `--sections` for disassembly; default for dumping |
| `Cannot read keys` at end | Harmless Il2CppDumper message | Ignore—all output is written before this |
| No methods found | Wrong metadata file | Ensure correct `global-metadata.dat` path |
| `<no managed method>` | Address not in IL2CPP range | Check base address; may be native code |

### Verifying Conversion

```bash
# Compare sizes (should be non-zero)
ls -la /tmp/il2cpp.elf

# Check ELF headers
objdump -f /tmp/il2cpp.elf

# Verify dynamic symbols (Sony NIDs)
objdump -T /tmp/il2cpp.elf
```

## Technical Details

### ELF Flattening Algorithm

The converter works by:

1. Parsing SELF program headers to find data segments
2. For each `PT_LOAD` segment: copying data to `p_vaddr` offset in output
3. Setting `p_offset = p_vaddr` (flattened layout)
4. Clearing section header fields (`e_shoff=0, e_shnum=0, e_shstrndx=0`)
5. Optionally: Synthesizing section headers per PT_LOAD

### Binutils Compatibility

Tested configurations:

| Tool | Works without `--sections` | Works with `--sections` |
|------|---------------------------|------------------------|
| `objdump -f` | ✅ | ✅ |
| `objdump -p` | ✅ | ✅ |
| `objdump -T` | ✅ | ✅ |
| `objdump -d` | ❌ | ✅ |
| `readelf -a` | ⚠️ Partial | ✅ |

## Contributing

### Adding Support for New Games

If a game uses different conventions:

1. Check the actual module name in `Media/Modules/`
2. Verify the module base address (check prosper boot output)
3. Update documentation if new patterns emerge

### Code Style

- Python 3.6+ features allowed
- Type hints appreciated but not required
- Comments should explain "why" not "what"
- Keep scripts under 200 lines when possible

## License

MIT License - see repository LICENSE file for details.

## See Also

- [Il2CppDumper](https://github.com/Perfare/Il2CppDumper) - External dependency
- [Unity IL2CPP Documentation](https://docs.unity3d.com/Manual/IL2CPP.html)
- [prosper docs/IL2CPP_METADATA_RUNTIME.md](../../docs/IL2CPP_METADATA_RUNTIME.md) - Our metadata parser
- Issue #2154 - Section header synthesis feature
