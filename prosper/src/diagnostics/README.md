# Diagnostics Layer

**Purpose**: Optional observer-only diagnostics layer for compatibility debugging.

## Design Principles

1. **No runtime behavior changes when disabled** — All diagnostic calls are gated on `is_enabled()` checks. When `--diagnostics` is not passed, the emulator runs identically to a build without diagnostics code.

2. **No modification of emulator execution flow** — The diagnostics system observes only. It does not intercept, redirect, or alter any emulator operations. Collectors record events; they do not influence outcomes.

3. **Thread-safe event collection** — `EventBus` uses proper synchronization for concurrent event emission from multiple guest threads (main thread, SPURS workers, audio thread, etc.).

4. **AI-friendly reports** — Output formats (JSON + Markdown) are structured for both human readability and LLM/automated analysis consumption.

## CLI Usage

### Basic Diagnostics Session

```bash
./prosper-app --diagnostics <game>
```

Enables diagnostic session collection. Creates a `diagnostic_run_<timestamp>/` directory in the current working directory containing all reports.

### AI-Generated Summary

```bash
./prosper-app --diagnostics --ai-report <game>
```

Generates an additional AI-readable summary (`ai_context.md`) with:
- Boot status assessment (success/failure/incomplete)
- Failure point identification (where boot stopped)
- Root cause hypotheses with confidence scores
- Recommended investigation actions

### Flag Reference

| Flag | Description |
|------|-------------|
| `--diagnostics` | Enable diagnostic session collection |
| `--diagnostics=<dir>` | Enable diagnostics with custom output directory |
| `--ai-report` | Generate AI-readable summary alongside standard reports |
| `--trace-loader` | Trace ELF/PRX loading events (verbose) |
| `--trace-hle` | Trace HLE function calls (verbose) |
| `--trace-memory` | Trace memory operations (verbose) |
| `--trace-all` | Enable all trace subsystems |

## Architecture Overview

```
prosper/src/diagnostics/
├── core/              # Core infrastructure
│   ├── types.hpp      # Event types, phases, severity levels
│   ├── event_bus.hpp  # Thread-safe event pub/sub
│   └── context.hpp    # Central coordinator (DiagnosticContext)
├── collectors/        # Subsystem-specific observers
│   ├── collector.hpp       # Base interface
│   ├── elf_collector       # ELF loader tracking
│   ├── prx_collector       # PRX module tracking
│   ├── hle_collector       # HLE call recording
│   ├── memory_collector    # Memory operation logging
│   └── crash_collector     # Crash state capture
├── storage/           # Output generation
│   └── json_writer    # JSON + Markdown file writer
├── ai/                # AI-optimized analysis
│   └── ai_report      # Report generator for LLM consumption
├── cli/               # Command-line interface
│   └── cli            # Flag parsing (--diagnostics, --ai-report)
├── integration/       # Runtime hook points
│   ├── boot_integration   # Boot sequence hooks
│   └── boot_program_patch # Example integration
└── diagnostics.hpp    # Single include entry point
```

## Integration Example

```cpp
#include "diagnostics/diagnostics.hpp"

int main(int argc, char** argv) {
    // Initialize from CLI (parses --diagnostics, --ai-report, etc.)
    bool diag_enabled = prosper::diagnostics::DiagnosticsIntegration::initialize_from_cli(&argc, &argv);
    
    // ... existing emulator initialization ...
    
    // Begin session when game identifier is known
    if (diag_enabled) {
        prosper::diagnostics::DiagnosticsIntegration::begin_session("PPSA02929");
    }
    
    // ... run emulator ...
    
    // Shutdown writes all reports
    if (diag_enabled) {
        prosper::diagnostics::DiagnosticsIntegration::shutdown();
    }
}
```

## Emitting Events

```cpp
// Simple event (compiles to nothing when disabled)
DIAG_EVENT("CUSTOM_EVENT", Severity::INFO, Subsystem::LOADER, "Message");

// Boot phase transitions
DIAG_BOOT_PHASE(BootPhase::ELF_PARSED);
DIAG_BOOT_PHASE_FAIL(BootPhase::IMPORT_RESOLUTION, "Missing critical import");

// Direct collector access
if (auto* prx = prosper::diagnostics::DiagnosticsIntegration::prx()) {
    prx->record_prx_loaded(path, size, true);
}
```

## Output Structure

See [docs/diagnostics/sample_report.md](../../../docs/diagnostics/sample_report.md) for detailed output examples and field descriptions.

## Integration Status

See [docs/diagnostics/integration_status.md](../../../docs/diagnostics/integration_status.md) for implemented features and future roadmap.
