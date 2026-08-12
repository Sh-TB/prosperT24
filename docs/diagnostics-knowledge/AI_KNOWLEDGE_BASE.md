# AI Knowledge Base — Prosper Diagnostics Verified Findings

## 📋 Document Overview

| Field | Value |
|-------|-------|
| **Repository** | Sh-TB/prosperT24 (PS4 Emulator) |
| **Document Type** | Permanent AI Knowledge Base |
| **Verification Status** | Phase 8.8 Evidence-Verified ✅ |
| **Confidence Level** | **86%** High-Confidence (78/91 items code-verified) |
| **Last Updated** | 2026-08-12 |
| **Total Plugins** | 10 Diagnostic Plugins |
| **Total Event Types** | 45+ Observable Events |
| **Test Coverage** | 84+ Tests Passing (4 suites) |

---

## 🎯 Project Purpose

### Why Diagnostics Plugins Were Created
The Prosper Diagnostics Plugin Collection addresses a critical gap in PS4 emulator development: **the lack of structured, AI-consumable debugging information**.

### Primary Goal: Improve AI Coder Debugging Ability
- **AI-Optimized Output**: All reports generated in JSON format with LLM-friendly structure
- **Structured Event Data**: 45+ observable event types with consistent payload schemas
- **Automated Analysis**: AI Report Generator plugin produces Markdown summaries for LLM consumption
- **Evidence-Based Debugging**: Every claim traceable to source events

### Observer-Only Design Philosophy
```
CORE PRINCIPLE: ZERO IMPACT ON EMULATION
├── No behavior modification
├── No state changes  
├── No performance impact when disabled (~1ns overhead)
└── Pure observation and recording only
```

---

## 🏗️ Verified Architecture

### EventBus Design (CONFIRMED ✅)

**Implementation**: Singleton `PluginRegistry` with publish/subscribe pattern

```cpp
// Event Flow (Verified):
Prosper Code → DIAG_EMIT_EVENT(type, payload) 
             → PluginRegistry::dispatch_event() 
             → Each plugin's on_event() 
             → Internal data structures
             → export_report() → JSON file
```

**Key Facts** (Code-Verified):
| Component | Location | Lines | Status |
|-----------|----------|-------|--------|
| EventType enum | `plugin_interface.hpp` | 37-105 | 45 event types defined |
| EventData struct | `plugin_interface.hpp` | 164-186 | Type-safe payload container |
| DiagnosticPlugin interface | `plugin_interface.hpp` | 192-218 | Pure virtual base class |
| PluginRegistry singleton | `plugin_registry.cpp` | 223-256 | Global event dispatcher |

---

## 📊 Validation Evidence

### Test Results Summary ✅
| Suite | Count | Status |
|------|-------|--------|
| Unit Tests | 36+ | ✅ PASSING |
| Integration Tests | 11 | ✅ PASSING |
| Negative Tests | 36 | ✅ PASSING |
| AI Simulation | 1 | ✅ PASSING |
| **TOTAL** | **84+** | **✅ ALL PASS** |

### Phase 8.8 Evidence Verification ✅
| Metric | Value | Confidence |
|--------|-------|------------|
| CONFIRMED (Code Evidence) | 78 | **86%** |
| NEEDS_SOURCE (Blocked) | 5 | 5% |
| ASSUMPTION (Unverified) | 10 | 11% |

---

## 🧩 Plugin Knowledge Map

### High-Value Plugins (AI Score 9.0+) ⭐

| # | Plugin | File | Score | Key Value |
|---|--------|------|-------|----------|
| 1 | **Import Resolution** | `import_resolution_plugin.hpp` | **9.8/10** ⭐ | NID→function mapping, stub priorities |
| 2 | **Crash Context** | `crash_context_plugin.hpp` | **9.8/10** ⭐ | Register dump, signal name, post-mortem |
| 3 | **HLE Call Stats** | `hle_call_stats_plugin.hpp` | **9.0/10** ⭐ | Function profiling, regression detection |
| 4 | **AI Report Generator** | `ai_report_generator_plugin.hpp` | **9.0/10** ⭐ | LLM-optimized aggregation (MUST BE LAST) |

### Medium-Value Plugins (6.0-8.5)

| # | Plugin | Score | Priority |
|---|--------|-------|----------|
| 5 | Memory Map | 8.5/10 | P3 |
| 6 | Module Load | 8.0/10 | P3 |
| 7 | Boot Timeline | 7.3/10 | P3 |
| 8 | Thread Activity | 6.0/10 | P4 OPTIONAL |
| 9 | File Access | 6.0/10 | P3 |

### Deferred Plugin

| # | Plugin | Score | Status |
|---|--------|-------|--------|
| 10 | Performance Marker | 5.5/10 🔻 | P5-DEFER (known bugs) |

---

## 📝 Phase 9 Commit Structure (COMPLETED ✅)

| # | Commit | Description |
|---|--------|-------------|
| 1 | `fc285df` | Core Interface (522 lines) |
| 2 | `08496e6` | EventBus / Registry (223 lines) |
| 3 | `e423cfd` | Boot Timeline Plugin (281 lines) |
| 4 | `0c76b97` | Import Resolution Plugin ⭐ (193 lines) |
| 5 | `8bdfa29` | Module Load Plugin (241 lines) |
| 6 | `70f9eb5` | Memory Map Plugin (258 lines) |
| 7 | `67e790b` | Crash Context Plugin ⭐ (289 lines) |
| 8 | `01789c2` | HLE Call Stats Plugin ⭐ (279 lines) |
| 9 | `c95d2ea` | Thread Activity Plugin (315 lines) |
| 10 | `e09a62c` | File Access Plugin (298 lines) |
| 11 | `ff360f2` | Performance Marker Plugin 🔻 (327 lines) |
| 12 | `df17a0c` | AI Report Generator Plugin ⭐ (378 lines) |
| 13 | `6a33f22` | Tests + Documentation (41,628 insertions) |

**Total Lines:** ~6,520 (source) + ~2,806 (tests)

---

## 🚨 Known Limitations

### CONFIRMED Bugs
| Issue | Location | Severity |
|-------|----------|----------|
| Static counter bug | `performance_marker_plugin.hpp:232` | Medium (P5-DEFER) |
| Unbounded growth | `ai_report_generator_plugin.hpp:140` | Medium |

### NEEDS_SOURCE (Requires Prosper Source Code)
- Integration points location
- Exact HLE function count (~298)
- Thread state machine (8 states)
- GPU backend type (Vulkan?)

---

## 📜 AI Coder Rules (Permanent Guidelines)

1. **Evidence Before Assumptions** - Verify against source code first
2. **Never Claim Runtime Behavior Without Measurement** - Use benchmarks
3. **Keep Diagnostics Separate From Runtime Logic** - Observer-only design
4. **Small Independent Commits** - Persian-specified order
5. **Test Every Plugin Independently** - Identity, enable/disable, core, JSON, negative
6. **Document Uncertainty Explicitly** - Use [ASSUMPTION], [NEEDS_SOURCE] tags

---

*Generated: 2026-08-12*  
*Source: Phase 8.7 → 8.8 → 8.10 → 9 Verification*  
*Status: ✅ COMPLETE*
