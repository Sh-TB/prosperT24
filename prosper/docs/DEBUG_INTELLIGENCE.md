# Debug Intelligence Layer for Prosper/SharpEmuT24

**Version:** 1.0.0  
**Type:** Observer-Only Evidence Management System  
**Language:** C++17  
**Storage Format:** JSON (.exp.json)

---

## Overview

The Debug Intelligence Layer is an **evidence management and reasoning assistant** designed specifically for AI-assisted debugging workflows in the Prosper PS4 emulator project. It is **not a debugger replacement**—rather, it provides structured capture, organization, and analysis of debugging data to support both human investigators and AI assistants.

### Core Philosophy

1. **Observer Only**: Zero modifications to emulator behavior, runtime, loader, HLE, or GPU systems
2. **Evidence-Backed**: All conclusions require supporting data points
3. **AI-Friendly**: Structured JSON output optimized for LLM consumption
4. **Non-Invasive**: Can be added to any project without affecting existing functionality

---

## Architecture

### Module Structure

```
debug_intelligence/
├── debug_intelligence.hpp       # Core types, enums, data structures
├── experiment_recorder.hpp      # Build/env/log/crash capture, EXP generation
├── report_generator.hpp         # Root cause report generation (text + JSON)
├── history_search.hpp           # Duplicate detection, upstream fix awareness
├── cli_interface.hpp            # Command-line interface
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

## Version History

### v1.0.0 (Current)
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
