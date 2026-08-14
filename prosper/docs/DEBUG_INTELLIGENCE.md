# Debug Intelligence Platform for Prosper/SharpEmuT24

**Version:** 2.0.0 (Next Generation)  
**Type:** Observer-Only Evidence Management & Reasoning System  
**Language:** C++17  
**Storage Format:** JSON (.exp.json, .replay.json)

---

## Overview

The Debug Intelligence Platform is a **comprehensive evidence management and reasoning assistant** designed specifically for AI-assisted debugging workflows in the Prosper PS4 emulator project. Based on analysis of **3000+ investigation history entries**, it addresses the biggest time losses in debugging: missing observability, repeated investigations, and inability to trace root causes across subsystem boundaries.

### Core Philosophy (Golden Rules)

1. **Observer Only**: Zero modifications to emulator behavior, runtime, loader, HLE, GPU, CPU, or memory allocator
2. **Evidence-Backed**: All conclusions require supporting data points
3. **AI-Friendly**: Structured JSON output optimized for LLM consumption
4. **Root Cause Focus**: Crash location ≠ corruption location—track provenance across frames
5. **Temporal Awareness**: When matters as much as what—frame-level precision tracking
6. **Non-Invasive**: Can be added to any project without affecting existing functionality
7. **Performance Safe**: Zero overhead unless explicitly enabled

---

## Architecture

### Module Structure (v2.0 - 7 Priority Modules)

```
debug_intelligence/
├── core/
│   └── foundation.hpp           # P0: Core types, timestamps, JSON utils
├── memory/
│   └── provenance_tracker.hpp   # P1: Memory write tracking (Who wrote this value?)
├── contracts/
│   └── hle_contract_auditor.hpp # P2: HLE side-effect validation
├── timeline/
│   └── state_timeline.hpp       # P3: Frame-level object lifecycle tracking
├── diagnostics/
│   └── quality_analyzer.hpp     # P4: Diagnostic reachability analysis
├── history/
│   └── intelligence_database.hpp# P5: Searchable experiment/hypothesis DB
├── graph/
│   └── causal_dependency_graph.hpp # P6: Cross-layer dependency mapping
├── replay/
│   └── replay_package.hpp       # P7: Deterministic investigation packages
├── debug_intelligence.hpp       # Legacy v1.0 types (backward compatible)
├── experiment_recorder.hpp      # Build/env/log/crash capture
├── report_generator.hpp         # Root cause report generation
├── history_search.hpp           # Duplicate detection, upstream fix awareness
├── cli_interface.hpp            # Command-line interface (fork-only)
├── main.cpp                     # Executable entry point
└── CMakeLists.txt               # Build configuration
```

### Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    CLI Interface                            │
│  (Commands: init, capture, hypothesize, report, search)     │
└─────────────┬───────────────────┬───────────────────────────┘
              │                   │
    ┌─────────▼─────────┐ ┌──────▼──────────┐
    │ Experiment Recorder│ │ History Search   │
    │ - Build config    │ │ - Indexing       │
    │ - Environment     │ │ - Duplicate check│
    │ - Logs/Screenshots│ │ - Upstream fixes │
    │ - Crash state     │ │ - Search         │
    │ - EXP package I/O │ └──────────────────┘
    └─────────┬─────────┘
              │
    ┌─────────▼─────────┐
    │ Report Generator   │
    │ - Timeline build   │
    │ - Evidence summary │
    │ - Hypothesis track │
    │ - Recommendations  │
    └─────────┬─────────┘
              │
    ┌─────────▼─────────┐
    │ JSON Storage       │
    │ .exp.json format   │
    └───────────────────┘
```

### Data Flow

```
1. INIT: Create experiment → Capture build/env baseline
2. CAPTURE: Add evidence (logs, screenshots, crash dumps)
3. INVESTIGATE: Form hypotheses, link evidence
4. RESOLVE: Confirm/reject hypotheses based on evidence
5. REPORT: Generate root cause analysis with timeline
6. EXPORT: Save as .exp.json package for sharing/archival
7. SEARCH: Query history to avoid duplicate work
```

---

## Quick Start

### Building

```bash
# From the prosper root directory
mkdir -p build && cd build
cmake .. -DBUILD_TESTS=ON
make -j$(nproc)
```

### Basic Workflow

```bash
# Set experiment storage directory (optional, defaults to ./experiments)
export DEBUG_INTEL_DIR=/path/to/debug/sessions

# Start new investigation
./bin/debug-intel init "IL2CPP Parser Crash Investigation"

# Capture current state
./bin/debug-intel capture-build
./bin/debug-intel capture-env

# Add evidence from log file
./bin/debug-intel capture-log /tmp/prosper.log --description="Runtime error log"

# Register screenshot if available
./bin/debug-intel capture-screenshot /tmp/crash_screenshot.png

# Parse and capture crash dump
./bin/debug-intel capture-crash /tmp/crash.log

# Formulate hypotheses
./bin/debug-intel add-hypothesis "Null pointer in string lookup" \
  --description="Parser doesn't validate filename before use"

# Link supporting evidence
./bin/debug-intel link-evidence hyp_xxx evd_yyy

# Reject incorrect hypothesis
./bin/debug-intel reject hyp_zzz --reason="Memory analysis showed no corruption"

# Confirm root cause
./bin/debug-intel confirm hyp_xxx

# Generate reports
./bin/debug-intel report --format=txt --output=investigation_report.txt
./bin/debug-intel report --format=json --output=report_for_ai.json

# Export complete EXP package
./bin/debug-intel export

# Check for similar past investigations before starting new one
./bin/debug-intel check-duplicate "Potential metadata parsing issue"

# Search all previous experiments
./bin/debug-intel search "metadata parser"
```

---

## Command Reference

### Experiment Management

| Command | Description | Example |
|---------|-------------|---------|
| `init [title]` | Create new experiment | `init "Crash investigation"` |
| `status` | Show current status | `status` |
| `complete` | Mark as completed | `complete` |
| `abort` | Abort experiment | `abort` |
| `load <file>` | Load existing EXP | `load session.exp.json` |

### Evidence Capture

| Command | Description | Example |
|---------|-------------|---------|
| `capture-build` | Capture compiler/git state | `capture-build` |
| `capture-env` | Snapshot environment | `capture-env --full` |
| `capture-log <file>` | Import log file | `capture-log app.log` |
| `capture-screenshot <img>` | Register image | `capture-screenshot screen.png` |
| `capture-crash <log>` | Parse crash dump | `capture-crash core.dump` |
| `add-evidence` | Custom evidence entry | `add-evidence --type=Custom` |

### Hypothesis Management

| Command | Description | Example |
|---------|-------------|---------|
| `add-hypothesis "title"` | Create hypothesis | `add-hypothesis "Null ptr"` |
| `list-hypotheses` | Show all hypotheses | `list-hypotheses` |
| `confirm <id>` | Confirm as root cause | `confirm hyp_123` |
| `reject <id>` | Reject hypothesis | `reject hyp_456` |
| `link-evidence <hyp> <evd>` | Attach evidence | `link-evidence h1 e1` |

### Reports & Export

| Command | Description | Example |
|---------|-------------|---------|
| `report` | Generate report | `report --format=json` |
| `export` | Export EXP package | `export` |

### History & Search

| Command | Description | Example |
|---------|-------------|---------|
| `search "query"` | Find experiments | `search "parser"` |
| `check-duplicate "title"` | Detect duplicates | `check-duplicate "Bug title"` |
| `history` | List all experiments | `history` |

---

## Data Structures

### Experiment Record

The central data structure containing all investigation data:

```cpp
struct ExperimentRecord {
    std::string id;                  // Unique identifier (exp_timestamp_counter)
    std::string title;               // Investigation title
    std::string description;         // Detailed description
    std::string created_at;          // ISO 8601 timestamp
    std::string updated_at;          // Last modification time
    std::string completed_at;        // Completion time (if applicable)
    
    BuildConfiguration build_config; // Compiler, git, defines info
    EnvironmentSnapshot environment; // OS, hardware, variables
    
    EvidenceCollection evidences;    // All captured evidence
    HypothesisTracker hypotheses;    // Investigation hypotheses
    
    std::string issue_reference;     // External tracking (GitHub, JIRA)
    std::vector<std::string> tags;   // Categorization tags
    std::string status;              // running/completed/failed/aborted
};
```

### Evidence Types

| Type | Description | Use Case |
|------|-------------|----------|
| `LogEntry` | Text log content | Runtime logs, console output |
| `Screenshot` | Image file reference | Visual state capture |
| `CrashDump` | Crash state data | Segfaults, assertions, panics |
| `MemorySnapshot` | Memory region dump | Buffer analysis |
| `Configuration` | Config file/settings | CMake cache, ini files |
| `EnvironmentVariable` | Env var value | PATH, library paths |
| `BuildInfo` | Compiler/build state | Reproducibility info |
| `GitCommit` | Version control state | Exact code version |
| `UserObservation` | Manual notes | Investigator notes |
| `MetricMeasurement` | Numeric data | Performance, counts |
| `CodeDiff` | Source changes | Patches, modifications |
| `NetworkCapture` | Network traffic | API calls, packets |
| `Custom` | User-defined | Anything else |

### Hypothesis Lifecycle

```
Open → InProgress → Confirmed (root cause found)
                 ↘ Rejected (ruled out by evidence)
                 ↘ Blocked (needs more info)
                 ↘ Superseded (merged into other hypothesis)
```

Each hypothesis tracks:
- Supporting evidence IDs
- Refuting evidence IDs  
- Confidence score (0.0–1.0)
- Investigation notes

---

## JSON Format (EXP Package)

### Structure

```json
{
  "id": "exp_1234567890_1",
  "title": "Investigation Title",
  "description": "Detailed description",
  "status": "completed",
  "created_at": "2025-01-15T10:30:00Z",
  "updated_at": "2025-01-15T14:22:00Z",
  "completed_at": "2025-01-15T14:22:00Z",
  "issue_reference": "PR-2527",
  
  "build_config": {
    "compiler": "g++",
    "compiler_version": "g++ (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0",
    "build_type": "Debug",
    "git_branch": "fix/il2cpp-metadata-runtime-parser",
    "git_commit_hash": "abc123...",
    "is_dirty": "false"
  },
  
  "environment": {
    "os_name": "Ubuntu 22.04.3 LTS",
    "architecture": "x86_64",
    "hostname": "dev-machine",
    "cpu_count": 12,
    "total_memory_mb": 32768
  },
  
  "evidences": [
    {
      "id": "evd_123...",
      "type": "CrashDump",
      "description": "Segfault in parser",
      "severity": "Critical",
      "verified": true
    }
  ],
  
  "hypotheses": [
    {
      "id": "hyp_456...",
      "title": "Null input handling",
      "status": "Confirmed",
      "confidence_score": 1.0,
      "supporting_evidence_count": 3,
      "refuting_evidence_count": 0
    }
  ],
  
  "tags": ["il2cpp", "parser", "crash"],
  
  "metadata": {
    "generator_version": "1.0.0",
    "generated_at": "2025-01-15T14:23:00Z"
  }
}
```

### Design Rationale for JSON

1. **Human Readable**: Can be inspected/edited with any text editor
2. **AI Consumable**: LLMs can parse and reason about JSON natively
3. **Version Control Friendly**: Diff-friendly for tracking changes
4. **Language Agnostic**: Any tool can read/write the format
5. **Extensible**: New fields can be added without breaking parsers

---

## Report Formats

### Text Report (Human-Readable)

Generated with `--format=txt` (default):

```
========================================================================
  ROOT CAUSE ANALYSIS REPORT
  Experiment: exp_1234567890_1
  Generated: 2025-01-15T14:23:00Z
========================================================================

EXECUTIVE SUMMARY
----------------------------------------
Root cause identified: Null input handling missing. Investigation 
completed with 3 hypotheses tested, 1 rejected.

ROOT CAUSE
----------------------------------------
The parser doesn't validate filename before using it.
Confidence Level: 100%
Confirmed Hypothesis ID: hyp_456...

INVESTIGATION TIMELINE
----------------------------------------
[2025-01-15T10:30:00Z] [experiment_started] Experiment started
[2025-01-15T10:31:00Z] [evidence_added] Evidence captured: Segfault...
[2025-01-15T11:00:00Z] [hypothesis_created] Hypothesis proposed: Null input...
[2025-01-15T14:00:00Z] [hypothesis_confirmed] Root cause confirmed: Null input...

KEY EVIDENCE
----------------------------------------
1. [Critical] Segfault in parser
   Content: Signal SIGSEGV at address 0x000000000000
   Source: /var/log/crash.log
   Verified: Yes

HYPOTHESIS ANALYSIS
----------------------------------------
[CONFIRMED] Null input handling
  Description: Parser doesn't validate filename before use
  Confidence: 100%
  Supporting Evidence: 3 items

REJECTED HYPOTHESES
----------------------------------------
[REJECTED] Memory corruption
  Reason: Memory analysis showed no corruption
  Refuting Evidence: 1 item

RECOMMENDATIONS
----------------------------------------
Add null-check validation at start of MetadataParser::parse().

VERIFICATION STEPS:
1. Apply fix addressing: Null input handling
2. Rebuild with configuration matching this experiment
3. Reproduce original test case
4. Verify issue is resolved
5. Run regression tests
6. Document findings in EXP package

LESSONS LEARNED
----------------------------------------
Investigation ruled out 1 incorrect hypothesis before finding root cause.
- Memory integrity was not the issue
- Input validation is the actual problem area

========================================================================
End of Report | Debug Intelligence Layer v1.0.0
```

### JSON Report (AI-Optimized)

Generated with `--format=json`:

```json
{
  "report_version": "1.0",
  "experiment_id": "exp_1234567890_1",
  "summary": {
    "executive_summary": "...",
    "root_cause": "...",
    "confidence_level": 1.0,
    "confirmed_hypothesis_id": "hyp_456..."
  },
  "timeline": [...],
  "key_evidence": [...],
  "hypotheses": {
    "confirmed": 1,
    "rejected": 1,
    "open": 0,
    "total": 3
  },
  "recommendations": {...},
  "metadata": {...}
}
```

---

## Integration Guide

### As Standalone Tool

```bash
# Build and use independently
cd prosper/src/debug_intelligence
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build

# Now available system-wide
debug-intel init "My investigation"
```

### Embedded in C++ Code

```cpp
#include "debug_intelligence/experiment_recorder.hpp"
#include "debug_intelligence/report_generator.hpp"

using namespace debug_intelligence;

void investigateIssue() {
    // Initialize recorder
    ExperimentRecorder recorder("./debug_sessions");
    recorder.initialize();
    
    // Create experiment
    ExperimentRecord exp;
    exp.title = "Automated investigation";
    exp.build_config = recorder.captureBuildConfig();
    
    // Capture evidence automatically on crash
    try {
        riskyOperation();
    } catch (const std::exception& e) {
        Evidence evd;
        evd.type = EvidenceType::CrashDump;
        evd.description = "Exception caught";
        evd.content = e.what();
        evd.severity = Severity::Critical;
        exp.evidences.add(evd);
        
        // Generate and save
        auto path = recorder.generateExpPackage(exp);
        if (path) {
            std::cout << "Debug package saved: " << path->string() << std::endl;
        }
    }
}
```

### With CI/CD Pipeline

```yaml
# .github/workflows/debug-capture.yml
name: Capture Debug Info on Failure

on:
  workflow_run:
    workflows: ['Tests']
    types: [completed]

jobs:
  capture:
    if: ${{ github.event.workflow_run.conclusion == 'failure' }}
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install Debug Intelligence
        run: |
          cd prosper/src/debug_intelligence
          cmake -B build && cmake --build build
      
      - name: Capture failure state
        run: |
          export DEBUG_INTEL_DIR=./debug_artifacts
          ./build/bin/debug-intel init "CI Test Failure - $GITHUB_RUN_ID"
          ./build/bin/debug-intel capture-build
          ./build/bin/debug-intel capture-log test_output.log
          ./build/bin/debug-intel export
      
      - name: Upload debug artifacts
        uses: actions/upload-artifact@v3
        with:
          name: debug-package-${{ github.run_id }}
          path: debug_artifacts/*.exp.json
```

---

## AI-Assisted Workflows

### For LLM Assistants

The JSON format is designed for AI consumption:

1. **Context Loading**: Feed `.exp.json` files as context
2. **Query Understanding**: LLM can search/filter evidence
3. **Hypothesis Generation**: Suggest new hypotheses based on patterns
4. **Evidence Correlation**: Identify connections between evidence items
5. **Report Summarization**: Generate executive summaries
6. **Root Cause Prediction**: Rank hypotheses by probability

### Example AI Prompts

```
Based on the following debugging session (EXP package), what is the most 
likely root cause? Consider all evidence and rejected hypotheses.

[Paste JSON or attach .exp.json file]
```

```
I'm investigating [issue]. Before I proceed, search my debug history for 
similar investigations to avoid duplicate work.

Search query: "[brief description]"
```

```
Generate 3 possible hypotheses for this crash, given:
- Crash type: SIGSEGV
- Location: MetadataParser::getString()
- Recent changes: IL2CPP parser refactor
```

---

## Safety Guarantees

### What This System Does NOT Do

❌ Modify emulator runtime behavior  
❌ Change memory state during execution  
❌ Hook into loader or HLE functions  
❌ Access GPU registers or state  
❌ Modify game compatibility layers  
❌ Inject code into running processes  

### What This System DOES Do

✅ Observe and record external state  
✅ Parse existing log/crash files  
✅ Organize investigator notes  
✅ Generate reports from collected data  
✅ Search historical investigations  
✅ Suggest based on patterns  

### Security Considerations

- **Sensitive Environment Variables**: Automatically redacted (PASSWORD, SECRET, KEY, TOKEN, etc.)
- **No Network Access**: All operations are local
- **No Privilege Escalation**: Runs at user level
- **Data Ownership**: All files stored in user-specified directory

---

## Testing

### Running Tests

```bash
# Build with tests enabled
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Run all tests
./build/test_debug_intelligence

# Run specific test
./build/test_debug_intelligence --gtest_filter="*EvidenceCollection*"
```

### Test Categories

| Category | Count | Coverage |
|----------|-------|----------|
| Timestamp Utilities | 3 | Format validation, uniqueness |
| Evidence System | 7 | CRUD operations, type conversion |
| Hypothesis Tracker | 6 | Lifecycle, similarity detection |
| Experiment Recorder | 10 | Capture, serialization, I/O |
| Report Generator | 5 | Generation, formats, config |
| History Search | 5 | Indexing, search, duplicates |
| Integration | 2 | Full workflow, round-trip |

**Total: ~38 test cases**

---

## Extensibility

### Adding New Evidence Types

```cpp
// In your code
enum class CustomEvidenceType : int {
    EmulatorState = static_cast<int>(EvidenceType::Custom) + 1,
    HLECallTrace,
    GPURegisterSnapshot
};

// Use as regular evidence with custom type stored in tags
Evidence evd;
evd.type = EvidenceType::Custom;
evd.tags["custom_type"] = "emulator_state";
evd.content = serializeEmulatorState();
```

### Custom Report Generators

```cpp
class CustomReportGenerator : public ReportGenerator {
public:
    std::string generateMarkdown(const RootCauseReport& report) {
        // Implement Markdown output
    }
    
    std::string generateHtml(const RootCauseReport& report) {
        // Implement HTML output
    }
};
```

### Plugin Architecture (Future)

The modular design supports future plugin development:

- **Evidence Sources**: Custom capture modules (e.g., GPU trace capture)
- **Report Formats**: Additional output formats (Markdown, HTML, PDF)
- **Search Backends**: Elasticsearch, SQLite for large histories
- **AI Integrations**: Direct hooks to OpenAI, Claude, local models

---

## Troubleshooting

### Common Issues

**Problem**: `Failed to initialize recorder`  
**Solution**: Ensure base directory exists and is writable

**Problem**: `Cannot find GTest`  
**Solution**: Install Google Test or disable tests: `-DBUILD_TESTS=OFF`

**Problem**: Git info empty in build config  
**Solution**: Run from within a git repository

**Problem**: Sensitive variables showing as `[NOT SET]`  
**Solution**: Variable genuinely not set; set it before capturing

### Debug Mode

Enable verbose logging:

```bash
export DEBUG_INTEL_VERBOSE=1
./bin/debug-intel init "Test"
```

---

## Next Generation Modules (v2.0)

Based on analysis of **3000+ investigation history entries**, these 7 priority modules address the biggest time losses in emulator debugging:

### P1: Memory Provenance Tracker (Highest Priority)

**Problem Solved:** "Who wrote this bad value?"

```cpp
#include "debug_intelligence/memory/provenance_tracker.hpp"

prosper_debug::memory::MemoryProvenanceTracker tracker;
tracker.enable();

// Watch address range
tracker.watchRange(0x2000000000, 0x10000);

// On memory write (instrument existing code)
uint64_t value = 0xDEADBEEF;
tracker.recordWrite(0x20000000100, &value, sizeof(value), 
                   prosper_debug::Subsystem::GPU, DEBUG_HERE());

// When corruption detected:
auto writers = tracker.whoWrote(0x20000000100);  // All writers
auto last = tracker.lastWriter(0x20000000100);   // Most recent
auto history = tracker.getMemoryHistory(0x20000000100, 100); // Last 100 events

// Export for AI analysis
std::string json = tracker.exportToJson();
```

**Key Features:**
- Observer-only: No memory allocation changes
- Ring buffer: Bounded memory usage (configurable)
- Thread-safe: Can be called from any thread
- Pattern detection: Auto-detects suspicious write sequences
- JSON export: AI-ready output with full provenance

**Output Format:**
```json
{
  "address": "0x20000000100",
  "writers": [
    {
      "timestamp": "2024-01-15T10:30:45.123Z",
      "frame": 1250,
      "old_value": "0x0000000000000000",
      "new_value": "0xDEADBEEFCAFEBABE",
      "subsystem": "GPU",
      "caller": "gpu_release_mem.cpp:150",
      "thread": "GPU Cmd Buffer"
    }
  ]
}
```

---

### P2: HLE Contract Auditor

**Problem Solved:** HLE functions return success without performing required side effects.

```cpp
#include "debug_intelligence/contracts/hle_contract_auditor.hpp"

prosper_debug::contracts::HLEContractAuditor auditor;
auditor.enable();

// Register expected contract
prosper_debug::contracts::HLEContract contract;
contract.function_name = "sceKernelFtruncate";
contract.expected_effects.push_back({
    prosper_debug::contracts::SideEffectType::FileSizeChanged,
    true  // required
});
auditor.registerContract(contract);

// Around HLE call:
auditor.startCall("sceKernelFtruncate", "call_123", "libkernel", DEBUG_HERE());
// ... actual HLE call happens ...
auditor.completeCall("call_123", 0);  // return value

// Check for violations
auto violations = auditor.getViolations();
for (const auto& v : violations) {
    std::cout << "VIOLATION: " << v.function_name 
              << " missing: " << v.missing_side_effects.size() << "\n";
}
```

**Supported Side Effects:**
- File system: Create, Delete, SizeChange, PositionChange
- Memory: Allocate, Free, Modify, HandleCreate/Destroy
- Sync: Mutex, Semaphore, ConditionVariable operations
- Thread: Create, Start, Terminate, PriorityChange
- GPU: Resource, Command, Fence, Texture operations

---

### P3: State Timeline System

**Problem Solved:** Crash location ≠ corruption location. Need historical state.

```cpp
#include "debug_intelligence/timeline/state_timeline.hpp"

prosper_debug::timeline::StateTimelineSystem timeline;
timeline.enable();

// Track object lifecycle
timeline.watchObject("texture_001");

// Record events (called from instrumentation points)
timeline.recordEvent(
    prosper_debug::timeline::TimelineEventType::ObjectCreated,
    "texture_001", DEBUG_HERE(), prosper_debug::Subsystem::GPU
);
timeline.setFrame(200);

timeline.recordEvent(
    prosper_debug::timeline::TimelineEventType::ObjectModified,
    "texture_001", DEBUG_HERE(), prosper_debug::Subsystem::GPU
);
timeline.setFrame(600);

// Query at crash time (frame 1300)
auto lifecycle = timeline.getObjectLifecycle("texture_001");
if (lifecycle) {
    std::cout << "Created: frame " << lifecycle->created_frame << "\n";
    std::cout << "Last modified: frame " << lifecycle->last_modified_frame << "\n";
    std::cout << "Invalidated: " << (lifecycle->invalidated ? "yes" : "no") << "\n";
}

// Find objects destroyed before crash
auto stale = timeline.findObjectsDestroyedBeforeFrame(1250);
```

**Event Types:**
- Lifecycle: ObjectCreated, ObjectDestroyed, ObjectModified, StateChanged
- Resource: Allocated, Freed, Mapped, Unmapped
- GPU: CommandSubmitted, CommandCompleted, FenceCreated, TextureUploaded
- Memory: RegionAllocated, RegionFreed, ProtectionChanged

---

### P4: Diagnostic Quality Analyzer

**Problem Solved:** Diagnostics added but never executed (dead code paths).

```cpp
#include "debug_intelligence/diagnostics/quality_analyzer.hpp"

prosper_debug::diagnostics::DiagnosticQualityAnalyzer analyzer;
analyzer.enable();

// Register diagnostic
prosper_debug::diagnostics::DiagnosticDefinition def;
def.id = "diag_nullptr_check";
def.location = {"memory_manager.cpp", 250, "allocate"};
def.description = "Check for nullptr before dereference";
def.condition = "ptr != nullptr";
analyzer.registerDiagnostic(def);

// Mark when diagnostic code path is reached
if (ptr != nullptr) {
    analyzer.executeDiagnostic("diag_nullptr_check");
    // ... diagnostic logic ...
}

// Check health
auto summary = analyzer.getDiagnosticSummary("diag_nullptr_check");
if (summary) {
    std::cout << "Executed: " << summary->execution_count << " times\n";
    std::cout << "Last reached: " << summary->last_execution.toISO8601() << "\n";
    
    if (summary->status == prosper_debug::diagnostics::DiagnosticStatus::NotReached) {
        std::cout << "WARNING: Diagnostic code path not executed!\n";
        std::cout << "Possible reasons: " << summary->not_reached_reason << "\n";
    }
}

// Generate quality report
auto report = analyzer.generateReport();
```

**Analysis Features:**
- Tracks registration vs execution count
- Detects unreachable diagnostics ("NOT REACHED" reporting)
- Identifies suspicious patterns (never-executed diagnostics)
- Provides reasons why diagnostic might not execute
- Health scoring for diagnostic coverage

---

### P5: Evidence/Hypothesis Intelligence Database

**Problem Solved:** Thousands of experiments created knowledge, but it was lost.

```cpp
#include "debug_intelligence/history/intelligence_database.hpp"

prosper_debug::history::EvidenceIntelligenceDatabase db;

// Create experiment
std::string exp_id = db.createExperiment(
    "GPU Memory Corruption Investigation",
    "Crash in allocator after GPU ReleaseMem",
    {"gpu", "memory", "crash", "allocator"}
);

// Add hypothesis
std::string hyp_id = db.createHypothesis(exp_id,
    "GPU wrote to freed memory",
    "ReleaseMem command target already freed"
);

// Link evidence
db.linkEvidence(hyp_id, "evd_memory_dump_001");
db.linkEvidence(hyp_id, "evd_gpu_trace_042");

// Confirm hypothesis
db.confirmHypothesis(hyp_id, "Memory provenance shows GPU write to freed page");

// Search for similar past investigations
auto similar = db.searchText("GPU memory corruption");
for (const auto& exp : similar) {
    std::cout << "Similar: " << exp.title 
              << " Status: " << experimentStatusToString(exp.status) << "\n";
}

// Find known upstream fixes
auto fixes = db.findRelevantUpstreamFixes("allocator");
```

**Search Capabilities:**
- Full-text search across experiments/hypotheses
- Similarity detection (bigram overlap)
- Tag-based filtering
- Status-based queries (Running, Completed, Failed)
- Upstream fix correlation

---

### P6: Causal Dependency Graph

**Problem Solved:** Emulator debugging has multiple layers; crashes propagate across boundaries.

```
Guest Application
       ↓
HLE (High-Level Emulation)
       ↓
Kernel
       ↓
Memory ← Crash here (symptom)
       ↑
CPU → GPU ← Actual cause (wrote to freed memory 20s ago)
       ↓
Host System
```

```cpp
#include "debug_intelligence/graph/causal_dependency_graph.hpp"

prosper_debug::graph::CausalDependencyGraph graph;

// Build dependency model
std::string guest = graph.addNode(
    prosper_debug::graph::NodeType::Object,
    "game_main",
    prosper_debug::graph::EmulationLayer::Guest,
    DEBUG_HERE()
);

std::string gpu_op = graph.addNode(
    prosper_debug::graph::NodeType::FunctionCall,
    "gpu_release_mem",
    prosper_debug::graph::EmulationLayer::GPU,
    DEBUG_HERE(),
    prosper_debug::Subsystem::GPU
);

std::string mem_alloc = graph.addNode(
    prosper_debug::graph::NodeType::Operation,
    "memory_allocate",
    prosper_debug::graph::EmulationLayer::Memory,
    DEBUG_HERE(),
    prosper_debug::Subsystem::Memory
);

// Define relationships
graph.addEdge(guest, gpu_op, prosper_debug::graph::EdgeType::Calls);
graph.addEdge(gpu_op, mem_alloc, prosper_debug::graph::EdgeType::WritesTo);

// When crash occurs, trace root cause
auto analysis = graph.analyzeRootCause(mem_alloc, "allocation_failure");
for (const auto& candidate : analysis.ranked_candidates) {
    std::cout << "Candidate: " << candidate.node_id 
              << " Score: " << candidate.confidence_score << "\n";
    std::cout << "Path: ";
    for (const auto& node : candidate.causality_path) {
        std::cout << node << " → ";
    }
    std::cout << "\n";
}
```

**Layer Types:** Guest, HLE, Kernel, Memory, CPU, GPU, Audio, Input, FileSystem, Network, Host

**Edge Types:** Calls, ReadsFrom, WritesTo, DependsOn, Locks, Signals, AllocatesFor

---

### P7: Replay Debug Package

**Problem Solved:** Many bugs require reproducing exact previous state.

```cpp
#include "debug_intelligence/replay/replay_package.hpp"

prosper_debug::replay::ReplayPackageBuilder builder;
builder.initialize("GPU Corruption Investigation", "Full evidence package");

// Set build environment
prosper_debug::replay::CommitInfo commit;
commit.hash = "a1b2c3d4";
commit.branch = "feature/memory-fix";
commit.message="Fix GPU memory tracking";
builder.setCommitInfo(commit);

// Set system environment
prosper_debug::replay::EnvironmentSnapshot env;
env.os_name = "Linux";
env.architecture = "x86_64";
env.cpu_count = 8;
env.total_memory_mb = 32768;
builder.setEnvironmentSnapshot(env);

// Add component data from other modules
builder.addComponentFile("memory_provenance", tracker.exportToJson());
builder.addComponentFile("hle_contracts", auditor.exportToJson());
builder.addComponentFile("state_timeline", timeline.exportAllToJson());

// Add crash record
prosper_debug::replay::CrashRecord crash;
crash.type = prosper_debug::replay::CrashRecord::CrashType::Segfault;
crash.fault_address = "0x20000000100";
crash.frame_number = 1300;
builder.addCrashRecord(crash);

// Include logs and screenshots
builder.addLogFile("/tmp/prosper.log");
builder.addScreenshot("/tmp/crash_state.png");

// Build complete package
auto manifest_path = builder.buildPackage();
// Creates: debug_replay/package_20240115_103045/
//   ├── manifest.json
//   ├── commit.json
//   ├── environment.json
//   ├── memory_provenance.json
//   ├── hle_contracts.json
//   ├── state_timeline.json
//   ├── crashes.json
//   ├── logs/prosper.log
//   └── screenshots/crash_state.png
```

**Loading a Package Later:**
```cpp
// Months later, reopen investigation
auto meta = prosper_debug::replay::ReplayPackageLoader::loadManifest(package_dir);
std::cout << "Package: " << meta.title << " Created: " << meta.created_at.toISO8601();

auto memory_data = prosper_debug::replay::ReplayPackageLoader::loadComponent(
    package_dir, "memory_provenance");
```

---

## Compilation & Testing

### Build All Modules

```bash
# Test compilation of individual modules
g++ -std=c++17 -fsyntax-only -I src \
    src/debug_intelligence/core/foundation.hpp
g++ -std=c++17 -fsyntax-only -I src \
    src/debug_intelligence/memory/provenance_tracker.hpp
# ... etc for all modules

# Run comprehensive test suite
g++ -std=c++17 -I src tests/test_compile_check.cpp -o test_platform
./test_platform
```

### Test Results (Current)

```
=== Debug Intelligence Platform - Compilation & Basic Test ===
Test 1: Core Foundation Types... ✅ PASSED
Test 2: Memory Provenance Tracker (P1)... ✅ PASSED
Test 3: HLE Contract Auditor (P2)... ✅ PASSED
Test 4: State Timeline System (P3)... ✅ PASSED
Test 5: Diagnostic Quality Analyzer (P4)... ✅ PASSED
Test 6: Intelligence Database (P5)... ✅ PASSED
Test 7: Causal Dependency Graph (P6)... ✅ PASSED
Test 8: Replay Debug Package (P7)... ✅ PASSED

=== SUMMARY ===
Passed: 8/8 🎉
```

---

## Version History

### v2.0.0 (Next Generation - Current)
- **P1**: Memory Provenance Tracker - Answer "Who wrote this value?"
- **P2**: HLE Contract Auditor - Validate side-effect contracts
- **P3**: State Timeline System - Frame-level object lifecycle tracking
- **P4**: Diagnostic Quality Analyzer - Detect unreachable diagnostics
- **P5**: Evidence/Hypothesis Intelligence Database - Searchable history
- **P6**: Causal Dependency Graph - Cross-layer root cause tracing
- **P7**: Replay Debug Package - Deterministic investigation snapshots
- Refactored core types into foundation.hpp
- Full C++17 compliance
- Thread-safe design throughout
- JSON-first data format for AI consumption

### v1.0.0 (Legacy)
- Initial release
- Core evidence management
- Hypothesis tracking
- Report generation (text + JSON)
- History search with duplicate detection
- CLI interface
- Comprehensive test suite

---

## License

MIT License - See LICENSE file for details

---

## Contributing

When contributing to the Debug Intelligence Layer:

1. **Maintain Observer-Only Principle**: No runtime modifications
2. **Add Tests**: Cover new functionality with unit tests
3. **Update Documentation**: Keep this doc current
4. **Follow Existing Patterns**: Match code style and architecture
5. **JSON Compatibility**: Ensure backward compatibility for .exp.json format

---

*Part of the Prosper/SharpEmuT24 Debug Infrastructure*  
*Designed for AI-Assisted Debugging Workflows*
