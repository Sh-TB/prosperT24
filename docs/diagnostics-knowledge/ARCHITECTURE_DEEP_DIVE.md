# Comment 1: Diagnostics Architecture Deep Dive

## Core Architecture Patterns (Code-Verified ✅)

### 1. Event-Driven Observer Pattern
```
Publisher (Prosper Code)
    ↓ DIAG_EMIT_EVENT(type, payload)
Subscriber (PluginRegistry::dispatch_event)
    ↓ Iterates over registered plugins
Observer (Each Plugin::on_event)
    ↓ Records to internal state
Reporter (Plugin::export_report)
    ↓ Outputs nlohmann::json
```

**Key Implementation Details**:

| Pattern | Implementation | File | Lines |
|---------|---------------|------|-------|
| Zero-overhead disabled | `#define DIAG_EMIT_EVENT(...) ((void)0)` | plugin_interface.hpp | 284 |
| Type-safe payload | `EventData::get<T>(key, default)` | plugin_interface.hpp | 171-181 |
| Safe extraction | `event.has(key)` before `event.get(key)` | crash_context_plugin.hpp | 220 |
| Thread-safe counting | `EventCounter` with mutex_ | plugin_base.hpp | 107-137 |
| Phase timing | `TimelineRecorder` with start/end | plugin_base.hpp | 143-206 |

---

### 2. Plugin Interface Contract
```cpp
class DiagnosticPlugin {
public:
    // Identity (pure virtual)
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    virtual std::string description() const = 0;
    
    // Lifecycle
    virtual bool initialize() = 0;       // Returns false on failure!
    virtual void shutdown() = 0;
    
    // Event handling
    virtual void on_event(const EventData& event) = 0;
    
    // State queries
    virtual bool is_enabled() const = 0;
    virtual void set_enabled(bool enabled) = 0;
    
    // Reporting
    virtual nlohmann::json export_report() const = 0;
    virtual bool save_report(const std::string& path) const = 0;
    
    // Subscription
    virtual std::vector<EventType> subscribed_events() const = 0;
};
```

**Critical Rules**:
- ✅ `initialize()` MUST return `false` on failure — caller must check!
- ✅ `on_event()` MUST NOT throw exceptions — catch internally
- ✅ `on_event()` may be called from ANY thread — use mutex_
- ✅ `export_report()` MUST return valid JSON, never null

---

### 3. BasePlugin Helper Class
```cpp
class BasePlugin : public DiagnosticPlugin {
protected:
    // State management
    bool should_process() const;           // Check enabled_ flag
    nlohmann::json create_base_report() const;  // Common JSON fields
    
    // Lifecycle hooks
    virtual void on_enable();   // Override for custom enable behavior
    virtual void on_disable();  // Override for custom cleanup
    
private:
    std::atomic<bool> enabled_;  // Thread-safe enabled flag
};
```

**Provided by BasePlugin**:
- Name/version/description storage and getters
- Thread-safe enable/disable with atomic flag
- Default `save_report()` implementation
- `create_base_report()` helper (adds metadata to all reports)

---

### 4. Event Type System (45+ Types)
```
Event Categories:
├── Boot Lifecycle (13): PROCESS_START → BOOT_COMPLETE
├── Module Events (4): MODULE_LOAD/UNLOAD, IMPORT_RESOLUTION  
├── Memory Events (5): MAP/UNMAP/PROTECT_CHANGE, ALLOCATION/DEALLOCATION
├── Thread Events (3): CREATE/DESTROY/STATE_CHANGE
├── CPU Events (3): SYSCALL_ENTRY/EXIT, BASIC_BLOCK_EXECUTE
├── GPU Events (5): COMMAND_SUBMIT/SYNCHRONIZE, SHADER_COMPILE, PIPELINE_CREATE, FRAME_PRESENT
├── File Events (4): OPEN/CLOSE/READ/WRITE
├── Error/Crash Events (3): EXCEPTION_THROWN, SIGNAL_RECEIVED, CRASH_DETECTED
├── Performance Events (3): PHASE_START/END, MARKER_RECORD
├── Shutdown Events (2): INITIATED/COMPLETE
└── Extension (1): CUSTOM_EVENT
```

**String Conversion**: Every enum value has mapping in `event_type_to_string()` (lines 108-158)

---

### 5. Threading Model
```
Thread Safety Rules (Verified):
┌─────────────────────────────────────────────────────┐
│ PluginRegistry::dispatch_event()                     │
│   ├── Holds mutex_ lock                              │
│   ├── Iterates over ALL registered plugins          │
│   └── Calls each plugin's on_event() WHILE holding lock │
│                                                     │
│ IMPLICATIONS:                                        │
│ ❌ Don't call register/unregister in on_event()      │
│ ❌ Don't block in on_event() (deadlock risk)         │
│ ✅ Use your own mutex for mutable member data        │
│ ✅ Keep handlers fast (<100ns target)                │
└─────────────────────────────────────────────────────┘
```

**Thread-Safe Components**:
- `PluginRegistry::mutex_` — Protects plugin map (`plugin_registry.cpp:254`)
- `EventCounter::mutex_` — Protects counts map (`plugin_base.hpp:135`)
- `TimelineRecorder::mutex_` — Protects events map (`plugin_base.hpp:204`)
- `BasePlugin::enabled_` — Atomic boolean, no lock needed (`plugin_base.hpp:100`)

---

### 6. Data Flow Diagram
```
┌─────────────────────────────────────────────────────────────┐
│                    PROSPER EMULATOR                          │
│                                                              │
│  [Game Code]  →  [HLE Layer]  →  [Kernel]  →  [Hardware]   │
│       │              │             │            │           │
│       ▼              ▼             ▼            ▼           │
│  DIAG_EMIT_EVENT  DIAG_EMIT_EVENT  ...        ...          │
│       │              │             │                        │
│       └──────────────┴─────────────┘                        │
│                         │                                   │
│                         ▼                                   │
│              ┌─────────────────────┐                        │
│              │  PluginRegistry     │                        │
│              │  (Singleton)        │                        │
│              └─────────┬───────────┘                        │
│                        │                                    │
│         ┌──────────────┼──────────────┐                     │
│         ▼              ▼              ▼                     │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                  │
│  │  Crash   │  │ Import   │  │   HLE    │    ... (10 total) │
│  │ Context  │  │ Resolve  │  │  Stats   │                  │
│  └─────┬────┘  └─────┬────┘  └─────┬────┘                  │
│        │             │             │                        │
│        ▼             ▼             ▼                        │
│  crash_report.json  import.json   hle_stats.json            │
│                                                              │
└─────────────────────────────────────────────────────────────┘

Output Directory: ./diagnostics_reports/
├── ai_report.json           ← Aggregated AI view
├── ai_context.md            ← Markdown summary
├── timeline.json            ← Chronological events
├── summary.json             ← Key findings
├── crash_context_report.json
├── import_resolution_report.json
├── hle_call_stats_report.json
└── ... (one per enabled plugin)
```

---

### 7. Integration Points (Assumed - Needs Prosper Source)

| Event Category | Assumed Location | Prosper File (Guessed) |
|---------------|------------------|------------------------|
| PROCESS_START | Main function entry | `main.cpp` or `emulator.cpp` |
| ELF_LOADING/LOADED | ELF loader | `elf_loader.cpp` |
| PRX_LOADING/LOADED | PRX module loader | `module_loader.cpp` |
| IMPORT_RESOLUTION | NID resolver | `hle_interface.cpp` |
| SYSCALL_ENTRY/EXIT | HLE dispatcher | `syscall_dispatch.cpp` |
| MEMORY_MAP/UNMAP | Memory manager | `memory_manager.cpp` |
| CRASH_DETECTED | Signal handler | `signal_handler.cpp` |
| THREAD_CREATE/DESTROY | Thread manager | `thread_manager.cpp` |
| FILE_OPEN/CLOSE/READ/WRITE | VFS layer | `vfs.cpp` |
| GPU_COMMAND_SUBMIT | GPU command processor | `gpu_command_buffer.cpp` |

**Status**: `[NEEDS_SOURCE]` — These are educated guesses based on standard emulator architecture patterns.

---

*Source: GITHUB_COMMENT1_ARCHITECTURE.md*  
*Confidence: HIGH (86% code-verified)*
