# Comment 2: AI Debugging Workflow Knowledge

## How AI Agents Use This Diagnostic System

### Workflow Overview
```
┌─────────────────────────────────────────────────────────────┐
│                AI DEBUGGING WORKFLOW                         │
│                                                              │
│  1. PROBLEM DETECTION                                        │
│     User reports: "Game crashes after boot"                  │
│     OR Automated: Crash Context Plugin detects SIGSEGV       │
│                                                              │
│  2. DATA COLLECTION (Automatic)                              │
│     ├── crash_context_report.json → Register dump, signal    │
│     ├── boot_timeline_report.json → Phase durations         │
│     ├── import_resolution_report.json → Unresolved imports  │
│     └── ai_context.md → LLM-optimized summary               │
│                                                              │
│  3. AI ANALYSIS (Using this knowledge base)                  │
│     ├── Cross-reference crash address with loaded modules   │
│     ├── Check if unresolved imports exist in crashed module │
│     ├── Identify which boot phase was active at crash time  │
│     └── Generate hypothesis with confidence score           │
│                                                              │
│  4. RECOMMENDATION GENERATION                                │
│     "Likely cause: Unresolved import sceFunctionName in      │
│      libModule.prx (NID: 0x123456789ABCDEF)                 │
│      Confidence: HIGH (crash in import resolution path)      │
└─────────────────────────────────────────────────────────────┘
```

---

## Debugging Scenario Examples

### Scenario 1: Boot Failure Diagnosis

**User Report**: "Emulator hangs during boot, never reaches game"

**AI Analysis Steps**:
```
STEP 1: Check Boot Timeline Report
- Query: Which phase was last to complete?
- Possible Findings:
  - LOADER_START but no LOADER_END → Loader hanging
  - ELF_LOADED but no PRX_LOADING → PRX discovery failure
  - MEMORY_MAPPING phase very long → Memory bottleneck

STEP 2: Cross-Reference with Module Load Report
- Query: Were any modules partially loaded?
- Look for: Modules with status != "COMPLETE"

STEP 3: Generate Diagnosis
OUTPUT FORMAT (for LLM consumption):
## Boot Failure Analysis
**Last Completed Phase**: ELF_LOADING
**Hanging Phase**: PRX_LOADING
**Root Cause Hypothesis**: PRX file discovery timeout or I/O error
**Confidence**: MEDIUM
**Recommended Action**: Enable File Access plugin, check for missing .prx files
```

---

### Scenario 2: Crash Post-Mortem

**Trigger**: Crash Context Plugin detects `CRASH_DETECTED` event

**AI Analysis Steps**:
```
INPUT DATA:
From crash_context_report.json:
{
  "exception_type": "SIGSEGV",
  "instruction_address": "0x00007FF123456789",
  "active_module": "libSceLibcInternal.prx",
  "boot_phase": "GUEST_INITIALIZATION",
  "registers": { "rip": "...", "rax": "0x0", ... }
}

ANALYSIS WORKFLOW:

1. MODULE IDENTIFICATION
   - Active module: libSceLibcInternal.prx (SYSTEM library)
   - High probability of HLE implementation bug

2. ADDRESS ANALYSIS  
   - RIP: 0x00007FF123456789
   - If in valid module range → Bug in library code or HLE stub
   - If NO → Memory corruption or buffer overflow

3. REGISTER HEURISTICS
   - RAX = 0x0 → Common after NULL dereference
   - RSP looks valid → Stack not corrupted

4. CROSS-REFERENCE WITH IMPORTS
   - Filter: module_name == "libSceLibcInternal.prx"
   - If crash address near unresolved import → STUB NEEDED

OUTPUT:
## Crash Analysis Report
**Signal**: SIGSEGV (Segmentation Fault)
**Location**: libSceLibcInternal.prx + offset
**Phase**: Guest Initialization (late boot)

**Hypothesis #1 (HIGH confidence)**:
Call to unimplemented HLE function
- Import was unresolved (stub missing)
- Stub returned NULL, code dereferenced it

**Action Required**:
1. Check Import Resolution report for unresolved imports
2. Implement missing HLE stubs
3. Add null-check in stub return values
```

---

### Scenario 3: Performance Regression Detection

**Use Case**: "After code change, game runs slower"

**AI Detection Algorithm**:
```
For each function:
  delta_avg = (after.avg - before.avg) / before.avg
  
IF delta_avg > 0.5 (50% slower):
  FLAG as "REGRESSION"
  
IF delta_max > 2.0 (200% slower max):
  FLAG as "SEVERE REGRESSION" (tail latency)

OUTPUT:
## Performance Regression Report
**Regressed Functions**: 3 of 147
**Most Severe**: scePthreadCreate (+98%)

| Function | Before (avg) | After (avg) | Δ |
|----------|-------------|-------------|---|
| pthreadC | 4.5μs | 8.9μs | +98% |
| mutexLock | 1.2μs | 2.1μs | +75% |

**Hypothesis**: Lock contention introduced by recent change
```

---

## Common Debugging Patterns (AI-Learned)

### Pattern 1: The "Unresolved Import Crash"
```
SYMPTOMS:
- Crash in system library (libSce*.prx)
- SIGSEGV or SIGBUS
- RAX/RDI/RSI often 0x0 (NULL)
- Boot phase: GUEST_INIT or later

ROOT CAUSE:
Game calls function via import table → Import unresolved → 
Stub returns NULL/0 → Code uses return value without check → CRASH

FIX PRIORITY: HIGH
Implement the missing HLE stub for the NID
```

### Pattern 2: The "Memory Map Conflict"
```
SYMPTOMS:
- Crash during MEMORY_MAPPING phase
- Signal: SIGBUS (alignment) or SIGSEGV (permission)

ROOT CAUSE:
Two modules mapped to overlapping regions
Or protection flags wrong (RW vs RO vs EXEC)
```

### Pattern 3: The "Thread Explosion"
```
SYMPTOMS:
- Emulator very slow
- High memory usage
- Many THREAD_CREATE events (>100)

ROOT CAUSE:
Game spawning worker threads rapidly
Or thread creation in loop without limit
```

---

## AI Prompt Engineering for Diagnostics

### Effective Prompts

**Template 1: Crash Investigation**
```
Analyze the following PS4 emulator diagnostic data and identify 
the most likely root cause of the crash:

## Crash Context
{paste crash_context_report.json}

## Import Status  
{paste import_resolution_report.json}

Provide:
1. Root cause hypothesis with confidence level
2. Supporting evidence from the data
3. Recommended fix or investigation step
```

**Template 2: Performance Review**
```
Compare these two HLE statistics reports and identify performance regressions:

## Baseline (before changes)
{paste baseline_hle_stats.json}

## Current (after changes)  
{paste current_hle_stats.json}

For each regressed function:
- Calculate percentage degradation
- Assess severity
- Suggest likely cause category
```

---

## AI Confidence Calibration

| Condition | Confidence | Reason |
|-----------|------------|--------|
| Direct evidence in crash report | HIGH | Factual data |
| Import NID matches crash module/address | HIGH | Correlated signals |
| Multiple plugins agree | HIGH | Cross-validated |
| Single anomalous data point | MEDIUM | Could be outlier |
| Absence of expected event | MEDIUM | Could be timing |
| No errors anywhere | LOW | Puzzle needs more data |

---

*Source: GITHUB_COMMENT2_WORKFLOW.md*  
*Practical patterns for AI-assisted PS4 emulator debugging*
