# IL2CPP Symbolication Tools for Unity/IL2CPP PS5 Titles

Recover C# method names and addresses from IL2CPP-compiled Unity games running on PS5, enabling breakpoint setting on managed logic (scene loading, boot state machines, game state transitions) during emulator debugging.

## Overview

These tools convert PlayStation 5 SELF/PRX modules into a format compatible with [Il2CppDumper](https://github.com/Perfare/Il2CppDumper), then symbolicate runtime addresses back to C# method names.

### Workflow Summary

```
PRX Module → prx_to_elf.py → Flattened ELF → Il2CppDumper → script.json → resolve.py → Method Names
```

### Components

| Tool | Purpose | Input | Output |
|------|---------|-------|--------|
| `prx_to_elf.py` | Flatten PRX to loadable ELF | `*.prx` | `*.elf` |
| `resolve.py` | Symbolicate addresses | `script.json` + addresses | Method names |

## Prerequisites

- Python 3.6+ (uses only standard library: `struct`, `json`, `bisect`, `re`)
- [Il2CppDumper](https://github.com/Perfare/Il2CppDumper) (dotnet SDK required)
- Unencrypted game dump with `Media/Modules/` directory

## Quick Start

### Step 1: Flatten the IL2CPP PRX

Convert the compiled-C# PRX module into a loadable ELF file:

```bash
python3 prx_to_elf.py <dump>/Media/Modules/Il2CppUserAssemblies.prx /tmp/asm.elf
```

**Optional:** Add section headers for disassembly support:

```bash
python3 prx_to_elf.py <dump>/Media/Modules/Il2CppUserAssemblies.prx /tmp/asm.elf --sections
```

The `--sections` flag synthesizes a section header table so `objdump -d` can disassemble the result. This is opt-in—the default output is byte-identical to the original behavior.

### Step 2: Obtain Il2CppDumper

Download and extract Il2CppDumper (not preinstalled in the environment):

```bash
cd /tmp
curl -sSL https://github.com/Perfare/Il2CppDumper/releases/download/v6.7.46/Il2CppDumper-net7-v6.7.46.zip -o d.zip
python3 -c "import zipfile; zipfile.ZipFile('d.zip').extractall('dumper')"
```

### Step 3: Run Il2CppDumper

Generate method address database from the flattened ELF and metadata:

```bash
cd /tmp/dumper && DOTNET_ROLL_FORWARD=LatestMajor \
  sh -c 'printf "0\n0\n0\n" | dotnet Il2CppDumper.dll /tmp/asm.elf \
    <dump>/Media/Metadata/global-metadata.dat /tmp/out'
```

**Notes:**
- The ELF starts at vaddr 0, so answer `0` to "input il2cpp dump address" prompt
- This forces registration auto-scan
- Headless operation pipes answers via `printf`
- Harmless `Cannot read keys` warning at end—ignore it (outputs already written)

### Step 4: Examine Output

Il2CppDumper produces several files in `/tmp/out/`:

| File | Format | Content |
|------|--------|---------|
| `dump.cs` | C# source | Every method with `// RVA: 0x...` comments |
| `script.json` | JSON | Machine-readable `{Address, Name}` list |
| `il2cpp.h` | C header | Struct definitions and offsets |

The **runtime address** of a method = `module_load_base + RVA` (from script.json).

### Step 5: Symbolicate Addresses

Resolve runtime addresses or backtrace chains to C# method names:

```bash
# Single address (RVA format)
python3 resolve.py /tmp/out/script.json il+0x1764ce2
# Output: il+0x1764ce2  ->  Unity.PSN.PS5.Async.WorkerThread$$RunProc (+0xa2)

# Multiple addresses
python3 resolve.py /tmp/out/script.json 0x16b981 0x11e63c

# Backtrace chain from prosper [btrace] output
echo '[btrace] ... chain=il+0xde92e9,il+0xde9159' | python3 resolve.py /tmp/out/script.json -

# Absolute runtime address (with known base)
python3 resolve.py /tmp/out/script.json --base 0x440000000 0x4402140d0
```

## Usage Details

### prx_to_elf.py

Flattens an unencrypted PS5 SELF/PRX into a loadable ET_DYN ELF where `p_offset == p_vaddr`.

**Syntax:**
```bash
python3 prx_to_elf.py <input.prx> <output.elf> [--sections]
```

**Arguments:**
- `input.prx`: Path to unencrypted PRX module (typically `Il2CppUserAssemblies.prx`)
- `output.elf`: Path for output ELF file
- `--sections`: (Optional) Synthesize section headers for objdump compatibility

**Algorithm:**
1. Parse SELF header to locate segment table and embedded ELF
2. Extract program headers from embedded ELF
3. For each PT_LOAD segment, copy data from SELF segment to flattened buffer
4. Set `e_type = ET_DYN`, clear section header fields (`e_shoff`, `e_shnum`, `e_shstrndx`)
5. Optionally synthesize section table with `--sections`

**Output Properties:**
- ELF64, little-endian, ET_DYN
- Program headers preserved and corrected (`p_offset := p_vaddr`)
- No section headers by default (opt-in with `--sections`)
- Suitable for Il2CppDumper input

### resolve.py

Maps runtime addresses to C# method names using Il2CppDumper's `script.json`.

**Syntax:**
```bash
python3 resolve.py <script.json> [options] <addresses...>
python3 resolve.py <script.json> -  # Read addresses from stdin
```

**Options:**
- `--base <addr>`: Subtract this base from absolute addresses (default: 0)
- `-`: Read tokens from stdin (parses `[btrace]` format)

**Address Formats Supported:**
| Format | Example | Interpretation |
|--------|---------|----------------|
| Bare hex | `0x2140d0` | Treated as RVA (or absolute if `--base` set) |
| IL2CPP offset | `il+0x1764ce2` | Extract hex, treat as RVA |
| Eboot offset | `eb+0xada254` | Marked as native (not managed) |

**Resolution Logic:**
1. Load and sort methods by address from `script.json`
2. For each input address, find nearest method start via binary search
3. If within 32KB (`0x8000`) tolerance, report method name + offset
4. Otherwise report as runtime/internal address

## Technical Details

### ELF Flattening Algorithm

The flattening process transforms a PS5 SELF/PRX (which has segmented data referenced by program headers) into a contiguous ELF image where file offsets match virtual addresses.

**Key Transformations:**

| Field | Original | Flattened | Rationale |
|-------|----------|-----------|-----------|
| `e_type` | Varies | `ET_DYN` (3) | Position-independent executable |
| `e_phoff` | Embedded offset | `0x40` | Standard location |
| `e_shoff` | Stale value | `0` | No section table |
| `e_shnum` | Stale value | `0` | No sections |
| `e_shstrndx` | Stale index | `SHN_UNDEF` (0) | No string table |
| `p_offset` | File offset | `p_vaddr` | Flat layout |

**Why Clear Section Headers?**
PS5 modules keep stale section header values after stripping:
- `e_shoff`: Points past EOF (table doesn't exist)
- `e_shstrndx`: Invalid index (41-48 observed across titles)
- `e_shnum`: Zero or large value (43-48 observed)

Binutils rejects files where these describe non-existent tables. All three must be cleared—clearing only one is insufficient (verified by A/B testing all four combinations on real modules).

### Section Synthesis (--sections)

When `--sections` is specified, one `SHT_PROGBITS` section per PT_LOAD segment is created:

```
.shstrtab  →  SHT_STRTAB (section name strings)
.textN     →  SHT_PROGBITS + SHF_EXECINSTR (executable segments)
.dataN     →  SHT_PROGBITS + SHF_WRITE (writable segments)
.rodataN   →  SHT_PROGBITS (read-only segments)
.bssN      →  SHT_NOBITS (zero-filled segments)
```

Properties:
- `sh_addr == sh_offset == p_vaddr` (directly from flattening)
- No derivation or guessing—each section restates its segment
- Enables `objdump -d` with correct virtual addresses

### Address Resolution Tolerance

The 32KB (`0x8000`) tolerance window accounts for:
- Method prologue instructions before first meaningful instruction
- IL2CPP thunk/trampoline code
- Compiler-generated preamble

Addresses outside any method are reported as `<il2cpp runtime / no managed method at this offset>`.

## Integration with Prosper Emulator

### Backtrace Symbolication

Prosper's validated unwinder (`[btrace]`) outputs frames as:

```
[btrace] ... chain=il+0x1764ce2,il+0xde92e9,il+0xde9159
```

Pipe directly to `resolve.py`:

```bash
echo '[btrace] ... chain=il+0xde92e9,il+0xde9159' | python3 resolve.py script.json -
```

**Important:** The unwinder must accept INDIRECT call sites (`0xFF` at v-2/-3/-6/-7), not just `0xE8` (call rel32). IL2CPP dispatches managed methods indirectly, so an `0xE8`-only filter drops every managed frame.

### GDB Debugging

When running prosper under GDB, always configure signal handling:

```
handle SIGSEGV SIGILL SIGBUS nostop noprint pass
```

Prosper installs a SIGSEGV handler for lazy memory commit. Without this, GDB stops on normal lazy-commit faults and corrupts execution—a healthy boot appears to "crash" at a null dereference when it actually does not.

## Troubleshooting

### Common Issues

| Symptom | Cause | Solution |
|---------|-------|----------|
| `file format not recognized` (objdump) | Stale section headers | Use default output (no `--sections`) or verify `prx_to_elf.py` ran correctly |
| `Cannot read keys` (Il2CppDumper) | Normal in headless mode | Ignore—outputs written successfully before this message |
| No methods found | Wrong metadata file | Ensure path to `global-metadata.dat` is correct |
| All addresses unresolved | Wrong base address | Try `--base 0` (treat as RVAs) or determine actual load base |
| `<il2cpp runtime>` for valid methods | Address tolerance exceeded | Method may be in thunk/trampoline; check disassembly |

### Verification Steps

1. **ELF Validity:**
   ```bash
   objdump -f /tmp/asm.elf  # Should show ELF headers
   objdump -p /tmp/asm.elf  # Should show program headers
   ```

2. **Section Synthesis (if used):**
   ```bash
   objdump -h /tmp/asm.elf  # Should show .text*, .data*, etc.
   objdump -d --start-address=0x2140d0 /tmp/asm.elf  # Should disassemble
   ```

3. **Script.json Content:**
   ```bash
   python3 -c "import json; d=json.load(open('/tmp/out/script.json')); print(len(d['ScriptMethod']), 'methods')"
   ```

### Known Limitations

- **Game-Specific Base Addresses:** The IL2CPP PRX load base varies by title (observed: `0x440000000` for some titles). Check your specific title's memory map.
- **Metadata Version:** Il2CppDumper version must match game's IL2CPP generation. Version mismatches produce empty or corrupted output.
- **Encrypted Modules:** These tools require unencrypted dumps. Encrypted PRX cannot be flattened.
- **Single-Title Focus:** Currently optimized for `Il2CppUserAssemblies.prx`. Other IL2CPP modules may need adjustment.

## Testing

### Running Tests

```bash
# Run dedicated test suite
python3 test_il2cpp_tools.py -v

# Run existing regression tests
python3 test_prx_to_elf.py -v
```

### Test Coverage

| Suite | File | Coverage |
|-------|------|----------|
| `test_il2cpp_tools.py` | This repo | ELF conversion, resolution, edge cases, integration |
| `test_prx_to_elf.py` | Upstream | ELF header validation, section synthesis regression |

## Contributing

### Code Style

- Python 3.6+ compatibility (type hints optional)
- Standard library only (no pip dependencies)
- Docstrings for public functions
- Comments for non-obvious algorithm steps

### Submitting Changes

1. Test changes with `python3 test_il2cpp_tools.py -v`
2. Verify no regression: `python3 test_prx_to_elf.py -v`
3. Update this README if behavior changes
4. Do not commit Il2CppDumper output (`dump.cs`, `script.json`, `il2cpp.h`)

## References

- [Il2CppDumper](https://github.com/Perfare/Il2CppDumper) — External tool dependency
- [Unity IL2CPP Documentation](https://docs.unity3d.com/Manual/IL2CPP.html) — Background on IL2CPP compilation
- Upstream issues: #2155 (ELF header fix), #2308 (sections flag), #2151 (GC hypothesis)

---

*Do not commit dumper output (dump.cs / script.json / il2cpp.h) — it is derived from the gitignored game dump.*
