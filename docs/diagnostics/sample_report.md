# Diagnostics Sample Report

This document describes the output structure and contents of a diagnostics run.

## Output Directory

When diagnostics are enabled, a `diagnostic_run_<timestamp>/` directory is created:

```
diagnostic_run_20260812_143522/
├── session.json       # Session metadata
├── timeline.json      # Boot phase timeline
├── events.json        # Complete event log
├── elf_report.json    # ELF loading analysis
├── prx_report.json    # PRX module report
├── hle_report.json    # HLE call statistics
├── ai_context.md      # AI-readable summary (with --ai-report)
└── crash_report.json  # Crash analysis (if crash occurred, optional)
```

---

## session.json

Session metadata and overall statistics.

```json
{
  "session_id": "diag_20260812_143522",
  "game_identifier": "PPSA02929",
  "game_name": "Dreaming Sarah",
  "start_time": "2026-08-12T14:35:22Z",
  "end_time": "2026-08-12T14:35:48Z",
  "duration_ms": 26340,
  "platform": {
    "os": "Linux",
    "arch": "x86_64",
    "build_type": "Debug"
  },
  "config": {
    "ai_report": true,
    "trace_loader": false,
    "trace_hle": false,
    "trace_memory": false
  },
  "status": "completed",
  "final_phase": "BOOT_COMPLETE",
  "stats": {
    "total_events": 1247,
    "errors": 3,
    "warnings": 12
  }
}
```

| Field | Description |
|-------|-------------|
| `session_id` | Unique run identifier |
| `game_identifier` | Title ID (e.g., PPSA02929) |
| `duration_ms` | Wall-clock session duration |
| `status` | `completed`, `failed`, or `interrupted` |
| `final_phase` | Last boot phase reached |
| `stats.total_events` | Number of events recorded |

---

## timeline.json

Boot phase progression with timestamps and durations.

```json
{
  "phases": [
    {
      "phase": "PROCESS_START",
      "start_ms": 0,
      "end_ms": 2,
      "duration_ms": 2,
      "status": "success"
    },
    {
      "phase": "ELF_OPENED",
      "start_ms": 2,
      "end_ms": 15,
      "duration_ms": 13,
      "status": "success"
    },
    {
      "phase": "ELF_PARSED",
      "start_ms": 15,
      "end_ms": 89,
      "duration_ms": 74,
      "status": "success"
    },
    {
      "phase": "SEGMENTS_MAPPED",
      "start_ms": 89,
      "end_ms": 234,
      "duration_ms": 145,
      "status": "success"
    },
    {
      "phase": "PRX_LOADING",
      "start_ms": 234,
      "end_ms": 4567,
      "duration_ms": 4333,
      "status": "success",
      "detail": {
        "modules_loaded": 24,
        "modules_failed": 0
      }
    },
    {
      "phase": "IMPORT_RESOLUTION",
      "start_ms": 4567,
      "end_ms": 12890,
      "duration_ms": 8323,
      "status": "success",
      "detail": {
        "imports_resolved": 1847,
        "imports_stubbed": 23,
        "imports_missing": 0
      }
    },
    {
      "phase": "ENTRYPOINT_EXECUTED",
      "start_ms": 12890,
      "end_ms": 12945,
      "duration_ms": 55,
      "status": "success"
    },
    {
      "phase": "VIDEOOUT_INITIALIZED",
      "start_ms": 18900,
      "end_ms": 19234,
      "duration_ms": 334,
      "status": "success"
    },
    {
      "phase": "FIRST_FRAME_CAPTURED",
      "start_ms": 24120,
      "end_ms": 24156,
      "duration_ms": 36,
      "status": "success",
      "detail": {
        "resolution": { "width": 1920, "height": 1080 },
        "format": "RGBA8888"
      }
    },
    {
      "phase": "BOOT_COMPLETE",
      "start_ms": 26300,
      "end_ms": 26340,
      "duration_ms": 40,
      "status": "success"
    }
  ],
  "total_boot_time_ms": 26340
}
```

| Field | Description |
|-------|-------------|
| `phase` | Boot phase identifier |
| `start_ms` / `end_ms` | Relative timestamps from session start |
| `duration_ms` | Phase duration |
| `status` | `success` or `failure` |
| `detail` | Phase-specific data (module counts, resolution, etc.) |

---

## events.json

Complete ordered event log. Each event follows the universal schema.

```json
{
  "events": [
    {
      "id": 1,
      "timestamp_ms": 0,
      "type": "BOOT_PHASE",
      "severity": "INFO",
      "subsystem": "boot",
      "thread": "main",
      "source": "boot_integration.hpp:42",
      "message": "Phase transition: PROCESS_START",
      "data": {
        "phase": "PROCESS_START",
        "status": "success"
      }
    },
    {
      "id": 2,
      "timestamp_ms": 2,
      "type": "ELF_OPEN",
      "severity": "INFO",
      "subsystem": "loader",
      "thread": "main",
      "source": "elf_loader.cpp:156",
      "message": "Opening executable: /app0/eboot.bin",
      "data": {
        "path": "/app0/eboot.bin",
        "size": 2458624
      }
    },
    {
      "id": 3,
      "timestamp_ms": 15,
      "type": "ELF_PARSE",
      "severity": "INFO",
      "subsystem": "loader",
      "thread": "main",
      "source": "elf_parser.cpp:89",
      "message": "ELF parsed successfully",
      "data": {
        "type": "EXEC",
        "machine": "AMD64",
        "segments": 9,
        "entry_point": "0x2000100"
      }
    },
    {
      "id": 447,
      "timestamp_ms": 234,
      "type": "PRX_LOAD",
      "severity": "INFO",
      "subsystem": "loader",
      "thread": "main",
      "source": "prx_loader.cpp:312",
      "message": "Module loaded: libSceLibcInternal.prx",
      "data": {
        "path": "/app0/sce_module/libSceLibcInternal.prx",
        "size": 1572864,
        "loaded": true
      }
    },
    {
      "id": 892,
      "timestamp_ms": 12450,
      "type": "HLE_CALL",
      "severity": "DEBUG",
      "subsystem": "hle",
      "thread": "main",
      "source": "hle_dispatch.cpp:78",
      "message": "scePthreadCreate",
      "data": {
        "nid": "0x12345678",
        "module": "libkernel",
        "args_count": 4,
        "return_value": 0
      }
    },
    {
      "id": 1247,
      "timestamp_ms": 24156,
      "type": "VIDEOOUT_FRAME",
      "severity": "INFO",
      "subsystem": "videoout",
      "thread": "render",
      "source": "videoout.cpp:445",
      "message": "First frame presented",
      "data": {
        "flip_index": 1,
        "resolution": { "width": 1920, "height": 1080 },
        "format": "RGBA8888"
      }
    }
  ],
  "total_count": 1247
}
```

| Field | Description |
|-------|-------------|
| `id` | Sequential event number |
| `timestamp_ms` | Relative time from session start |
| `type` | Event category (see Event Types) |
| `severity` | `DEBUG`, `INFO`, `WARNING`, `ERROR` |
| `subsystem` | Source subsystem |
| `thread` | Guest thread name |
| `source` | Source file:line |
| `data` | Event-specific payload |

### Event Types

| Type | Subsystem | Description |
|------|-----------|-------------|
| `BOOT_PHASE` | boot | Phase transition |
| `ELF_OPEN` | loader | Executable opened |
| `ELF_PARSE` | loader | ELF structure parsed |
| `PRX_LOAD` | loader | PRX module loaded |
| `PRX_IMPORT` | linker | Import resolved |
| `HLE_CALL` | hle | HLE function invoked |
| `HLE_ERROR` | hle | HLE call failed/unimplemented |
| `MEMORY_ALLOC` | memory | Memory allocation |
| `MEMORY_VIOLATION` | memory | Access violation |
| `VIDEOOUT_FRAME` | videoout | Frame presented |
| `CRASH` | crash | Crash detected |

---

## elf_report.json

Detailed ELF binary analysis.

```json
{
  "executable": {
    "path": "/app0/eboot.bin",
    "size": 2458624,
    "hash_sha256": "a1b2c3d4..."
  },
  "header": {
    "type": "EXEC",
    "machine": "AMD64",
    "version": 1,
    "entry_point": "0x2000100",
    "flags": 0
  },
  "segments": [
    {
      "index": 0,
      "type": "PT_LOAD",
      "vaddr": "0x0000000",
      "filesz": 8192,
      "memsz": 8192,
      "flags": "R+E"
    },
    {
      "index": 1,
      "type": "PT_LOAD",
      "vaddr": "0x1000000",
      "filesz": 2048000,
      "memsz": 2097152,
      "flags": "R+W+E"
    }
  ],
  "dynamic": {
    "needed_libraries": ["libSceLibcInternal.prx", "libSceGnmDriver.prx"],
    "init_array_entries": 3,
    "fini_array_entries": 1
  },
  "analysis": {
    "total_segments": 9,
    "code_segments": 3,
    "data_segments": 4,
    "unknown_segments": 2
  }
}
```

---

## prx_report.json

PRX module loading summary with dependency graph.

```json
{
  "summary": {
    "total_modules": 24,
    "loaded": 24,
    "failed": 0,
    "skipped": 0
  },
  "modules": [
    {
      "name": "libSceLibcInternal.prx",
      "path": "/app0/sce_module/libSceLibcInternal.prx",
      "size": 1572864,
      "status": "loaded",
      "load_order": 1,
      "exports": 342,
      "imports": 12,
      "init_function": "0x10200"
    },
    {
      "name": "libSceGnmDriver.prx",
      "path": "/sys/internal/libSceGnmDriver.prx",
      "size": 2097152,
      "status": "loaded",
      "load_order": 5,
      "exports": 567,
      "imports": 45,
      "init_function": "0x50000"
    }
  ],
  "import_resolution": {
    "total_imports": 1870,
    "resolved": 1847,
    "stubbed": 23,
    "missing": 0,
    "stubbed_nids": [
      "0xAABBCCDD",
      "0x11223344"
    ]
  }
}
```

---

## hle_report.json

HLE function call statistics and frequency analysis.

```json
{
  "summary": {
    "total_calls": 15847,
    "unique_functions": 234,
    "errors": 3,
    "unimplemented_called": 0
  },
  "top_functions": [
    {
      "name": "scePthreadMutexLock",
      "nid": "0x11111111",
      "module": "libkernel",
      "call_count": 3421,
      "avg_time_us": 2.3,
      "max_time_us": 145,
      "errors": 0
    },
    {
      "name": "sceKernelGetProcessId",
      "nid": "0x22222222",
      "module": "libkernel",
      "call_count": 2156,
      "avg_time_us": 0.1,
      "max_time_us": 5,
      "errors": 0
    }
  ],
  "by_module": {
    "libkernel": { "calls": 8934, "functions": 89 },
    "libSceLibcInternal": { "calls": 4521, "functions": 67 },
    "libSceGnmDriver": { "calls": 2392, "functions": 78 }
  },
  "error_calls": [
    {
      "name": "sceVideoOutSetBufferAttribute",
      "nid": "0x33333333",
      "error_code": "0x805A0001",
      "count": 2,
      "first_occurrence_ms": 18234
    }
  ]
}
```

---

## ai_context.md

Human-readable AI-generated summary (produced with `--ai-report` flag).

```markdown
# Diagnostics Summary: PPSA02929 / Dreaming Sarah

**Status**: ✅ **Boot Successful**

**Total boot time**: 26.3s

**Events captured**: 1,247

## Timeline Summary

| Phase | Duration | Status |
|-------|----------|--------|
| ELF Loading | 89ms | ✅ |
| Segment Mapping | 145ms | ✅ |
| PRX Loading (24 modules) | 4.3s | ✅ |
| Import Resolution (1,870 imports) | 8.3s | ✅ |
| Entry Point Execution | 55ms | ✅ |
| VideoOut Initialization | 334ms | ✅ |
| First Frame Capture | 36ms | ✅ |

## Key Observations

1. **All 24 PRX modules loaded successfully** — No missing or failed dependencies.

2. **Import resolution complete** — 1,847 of 1,870 imports resolved to real implementations.
   - 23 imports use stub implementations (expected for optional features).

3. **First frame at 24.1s** — VideoOut presented 1920×1080 RGBA8888 frame.

4. **Minor warnings** — 3 HLE calls returned error codes (non-critical).

## Issues Detected

| Severity | Count | Description |
|----------|-------|-------------|
| Warning | 12 | Non-critical (stub imports, timing) |
| Error | 3 | HLE return errors (handled gracefully) |

## Recommendation

Title boots correctly. No action required. Stub imports can be implemented later for full feature parity.
```

---

## crash_report.json (Optional)

Crash analysis report — only present when a crash is captured.

```json
{
  "crashed": true,
  "crash_time_ms": 15234,
  "crash_phase": "ENTRYPOINT_EXECUTED",
  "signal": 11,
  "signal_name": "SIGSEGV",
  "fault_address": "0xDEADBEEF",
  "registers": {
    "rip": "0x10023456",
    "rsp": "0x7FFF12340000",
    "rbp": "0x7FFF1233FF00"
  },
  "stack_trace": [
    {
      "address": "0x10023456",
      "symbol": "game_main+0x156",
      "module": "eboot.bin"
    },
    {
      "address": "0x10020000",
      "symbol": "_start+0x0",
      "module": "eboot.bin"
    }
  ],
  "suspected_causes": [
    {
      "cause": "Null pointer dereference",
      "confidence": 0.85,
      "evidence": ["Fault address in low memory range", "RAX=0 before crash"]
    }
  ],
  "recent_events": [
    {"id": 720, "type": "HLE_CALL", "message": "sceKernelMapMemory"},
    {"id": 721, "type": "MEMORY_ALLOC", "message": "Allocated 4096 bytes"},
    {"id": 722, "type": "CRASH", "message": "SIGSEGV at 0xDEADBEEF"}
  ]
}
```
