// diagnostics/ai/ai_report.cpp — AI Report Generator implementation
#include "ai_report.hpp"
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>

namespace prosper {
namespace diagnostics {

AiReport AiReportGenerator::generate(
    const DiagnosticSession& session,
    const std::vector<TimelineEntry>& timeline,
    const std::vector<EvidenceItem>& evidence,
    size_t total_events,
    const std::string& elf_report,
    const std::string& prx_report,
    const std::string& imports_report,
    const std::string& hle_report,
    const std::string& crash_report
) {
    AiReport report;
    
    auto start = std::chrono::steady_clock::now();
    
    // Basic info
    report.game_identifier = session.game_identifier;
    report.final_phase = session.stats.final_phase;
    report.boot_success = (session.stats.final_phase == BootPhase::BOOT_COMPLETE);
    report.total_events = total_events;
    report.error_count = session.stats.errors;
    report.warning_count = session.stats.warnings;
    report.generated_at = current_timestamp();
    
    // Analyze each data source
    analyze_timeline(report, timeline);
    analyze_imports(report, imports_report);
    analyze_hle(report, hle_report);
    analyze_crash(report, crash_report);
    
    // Sort root causes by confidence
    std::sort(report.root_causes.begin(), report.root_causes.end(),
              [](const RootCause& a, const RootCause& b) {
                  return a.confidence > b.confidence;
              });
    
    auto end = std::chrono::steady_clock::now();
    report.generation_time_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
    );
    
    return report;
}

void AiReportGenerator::analyze_timeline(AiReport& report,
                                          const std::vector<TimelineEntry>& timeline) {
    if (timeline.empty()) {
        report.failure_point = "No boot phases recorded";
        report.likely_cause = "Diagnostics system did not capture any boot activity";
        report.root_causes.push_back(make_root_cause(
            "No boot activity captured", 0.7,
            "Verify diagnostics integration is correctly hooked into boot sequence"
        ));
        return;
    }
    
    // Find first failure
    const TimelineEntry* first_failure = nullptr;
    for (const auto& entry : timeline) {
        if (!entry.success) {
            first_failure = &entry;
            break;
        }
    }
    
    if (report.boot_success) {
        report.failure_point = "Boot completed successfully";
        report.likely_cause = "No issues detected";
        
        // Still add recommendations for optimization
        report.investigation_steps.push_back(
            "Review HLE call frequency for potential optimizations"
        );
        report.investigation_steps.push_back(
            "Check GPU report for rendering anomalies"
        );
        
    } else if (first_failure) {
        BootPhase failed_phase = first_failure->phase;
        report.failure_point = "Boot stopped at: " + phase_to_human(failed_phase);
        
        // Generate likely cause based on phase
        switch (failed_phase) {
            case BootPhase::ELF_OPENED:
            case BootPhase::ELF_PARSED:
                report.likely_cause = "ELF file could not be opened or parsed";
                report.root_causes.push_back(make_root_cause(
                    "Corrupted or missing executable", 0.9,
                    "Verify eboot.bin exists and is a valid PS5 ELF/SELF"
                ));
                break;
                
            case BootPhase::SEGMENTS_MAPPED:
                report.likely_cause = "Memory mapping failed";
                report.root_causes.push_back(make_root_cause(
                    "Insufficient address space or invalid segment", 0.85,
                    "Check for address conflicts in elf_report.json"
                ));
                break;
                
            case BootPhase::RELOCATIONS_APPLIED:
                report.likely_cause = "Relocation processing error";
                report.root_causes.push_back(make_root_cause(
                    "Invalid relocation target or corrupted relocation table", 0.8,
                    "Review relocations.json for out-of-range targets"
                ));
                break;
                
            case BootPhase::PRX_LOADING:
                report.likely_cause = "PRX module loading failed";
                report.root_causes.push_back(make_root_cause(
                    "Missing or corrupted PRX module", 0.85,
                    "Check prx_report.json for which module failed to load"
                ));
                report.immediate_actions.push_back(
                    "Verify all required PRX files exist in dump directory"
                );
                break;
                
            case BootPhase::IMPORT_RESOLUTION:
                report.likely_cause = "Import resolution failed - missing exports";
                report.root_causes.push_back(make_root_cause(
                    "Unresolved imports preventing boot", 0.92,
                    "Implement missing HLE exports listed in imports.json"
                ));
                report.immediate_actions.push_back(
                    "Review imports.json for missing NIDs"
                );
                report.immediate_actions.push_back(
                    "Prioritize imports called before the failure point"
                );
                break;
                
            case BootPhase::ENTRYPOINT_EXECUTED:
                report.likely_cause = "Crash during early execution";
                report.root_causes.push_back(make_root_cause(
                    "Entry point faulted - possibly missing critical init", 0.88,
                    "Check crash.json for register state and stack trace"
                ));
                break;
                
            default:
                report.likely_cause = "Boot stopped at unexpected phase";
                report.root_causes.push_back(make_root_cause(
                    "Unknown failure at " + phase_to_human(failed_phase), 0.6,
                    "Review full event log for context around failure"
                ));
                break;
        }
        
    } else {
        // No explicit failure but didn't complete
        report.failure_point = "Boot incomplete at: " + 
                               phase_to_human(timeline.back().phase);
        report.likely_cause = "Process exited or hung without completing boot";
        
        report.root_causes.push_back(make_root_cause(
            "Incomplete boot without recorded failure", 0.7,
            "Check for external signals, resource exhaustion, or infinite wait"
        ));
    }
}

void AiReportGenerator::analyze_imports(AiReport& report,
                                         const std::string& imports_json) {
    // Simple analysis of import status
    // In production, this would parse the JSON properly
    
    bool has_missing = imports_json.find("\"MISSING\"") != std::string::npos;
    bool has_stubs = imports_json.find("\"STUB\"") != std::string::npos;
    
    if (has_missing) {
        report.immediate_actions.push_back(
            "Implement missing HLE functions (see imports.json)"
        );
    }
    
    if (has_stubs && !has_missing) {
        report.investigation_steps.push_back(
            "Some imports are stubbed - may need real implementations for this title"
        );
    }
}

void AiReportGenerator::analyze_hle(AiReport& report,
                                     const std::string& hle_json) {
    // Look for patterns in HLE calls that indicate problems
    bool has_errors = hle_json.find("\"failed\"") != std::string::npos &&
                      hle_json.find("0") != std::string::npos;  // has non-zero failures
    
    if (has_errors) {
        report.investigation_steps.push_back(
            "Some HLE functions returned errors - check hle_report.json"
        );
    }
}

void AiReportGenerator::analyze_crash(AiReport& report,
                                       const std::string& crash_json) {
    if (!crash_json.empty() && crash_json != "{}" && crash_json != "null") {
        // Crash data exists
        report.root_causes.push_back(make_root_cause(
            "Runtime crash detected", 0.95,
            "Analyze crash.json for register state, stack trace, and crash location"
        ));
        report.immediate_actions.push_back(
            "Examine crash.json to identify crash location and cause"
        );
    }
}

RootCause AiReportGenerator::make_root_cause(const std::string& cause,
                                              double confidence,
                                              const std::string& recommendation) const {
    RootCause rc;
    rc.cause = cause;
    rc.confidence = confidence;
    rc.recommendation = recommendation;
    return rc;
}

std::string AiReportGenerator::phase_to_human(BootPhase phase) const {
    switch (phase) {
        case BootPhase::PROCESS_START:        return "process initialization";
        case BootPhase::ELF_OPENED:           return "opening ELF file";
        case BootPhase::ELF_PARSED:           return "parsing ELF structure";
        case BootPhase::SEGMENTS_MAPPED:      return "mapping memory segments";
        case BootPhase::RELOCATIONS_APPLIED:  return "applying relocations";
        case BootPhase::PRX_LOADING:          return "loading PRX modules";
        case BootPhase::IMPORT_RESOLUTION:    return "resolving imports";
        case BootPhase::THREAD_CREATION:      return "creating threads";
        case BootPhase::ENTRYPOINT_EXECUTED:  return "executing entry point";
        case BootPhase::RUNTIME_INITIALIZED:  return "runtime initialization";
        case BootPhase::VIDEOOUT_INITIALIZED: return "VideoOut initialization";
        case BootPhase::FIRST_FRAME_ATTEMPT:  return "first frame attempt";
        case BootPhase::FIRST_FRAME_CAPTURED: return "first frame capture";
        case BootPhase::BOOT_COMPLETE:        return "boot completion";
        case BootPhase::PHASE_FAILED:         return "phase failure";
        default: return "unknown phase (" + std::to_string(static_cast<int>(phase)) + ")";
    }
}

std::string AiReportGenerator::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::gmtime(&time_t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string AiReportGenerator::to_json(const AiReport& report) const {
    std::ostringstream ss;
    
    ss << "{\n";
    
    // Summary
    ss << "  \"summary\": {\n";
    ss << "    \"game\": \"" << report.game_identifier << "\",\n";
    ss << "    \"boot_success\": " << (report.boot_success ? "true" : "false") << ",\n";
    ss << "    \"final_phase\": \"" << boot_phase_string(report.final_phase) << "\",\n";
    ss << "    \"failure_point\": \"" << report.failure_point << "\",\n";
    ss << "    \"likely_cause\": \"" << report.likely_cause << "\"\n";
    ss << "  },\n";
    
    // Statistics
    ss << "  \"statistics\": {\n";
    ss << "    \"total_events\": " << report.total_events << ",\n";
    ss << "    \"errors\": " << report.error_count << ",\n";
    ss << "    \"warnings\": " << report.warning_count << "\n";
    ss << "  },\n";
    
    // Root causes
    ss << "  \"root_causes\": [\n";
    for (size_t i = 0; i < report.root_causes.size(); ++i) {
        const auto& rc = report.root_causes[i];
        ss << "    {\n";
        ss << "      \"cause\": \"" << rc.cause << "\",\n";
        ss << "      \"confidence\": " << rc.confidence << ",\n";
        ss << "      \"recommendation\": \"" << rc.recommendation << "\"\n";
        ss << "    }";
        if (i < report.root_causes.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    
    // Recommendations
    ss << "  \"recommendations\": {\n";
    
    ss << "    \"immediate_actions\": [";
    for (size_t i = 0; i < report.immediate_actions.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "\"" << report.immediate_actions[i] << "\"";
    }
    ss << "],\n";
    
    ss << "    \"investigation_steps\": [";
    for (size_t i = 0; i < report.investigation_steps.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "\"" << report.investigation_steps[i] << "\"";
    }
    ss << "],\n";
    
    ss << "    \"long_term_fixes\": [";
    for (size_t i = 0; i < report.long_term_fixes.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "\"" << report.long_term_fixes[i] << "\"";
    }
    ss << "]\n";
    
    ss << "  },\n";
    
    // Metadata
    ss << "  \"metadata\": {\n";
    ss << "    \"generated_at\": \"" << report.generated_at << "\",\n";
    ss << "    \"generation_time_ms\": " << report.generation_time_ms << "\n";
    ss << "  }\n";
    
    ss << "}\n";
    
    return ss.str();
}

std::string AiReportGenerator::to_markdown(const AiReport& report) const {
    std::ostringstream ss;
    
    ss << "# AI Diagnostics Report\n\n";
    ss << "**Game**: `" << report.game_identifier << "`\n\n";
    ss << "**Status**: " << (report.boot_success ? "✅ **BOOT SUCCESS**" : "❌ **BOOT FAILED**") << "\n\n";
    
    if (!report.boot_success) {
        ss << "## Failure Analysis\n\n";
        ss << "**Where it stopped**: " << report.failure_point << "\n\n";
        ss << "**Likely cause**: " << report.likely_cause << "\n\n";
        
        if (!report.root_causes.empty()) {
            ss << "### Suspected Root Causes\n\n";
            for (size_t i = 0; i < report.root_causes.size(); ++i) {
                const auto& rc = report.root_causes[i];
                ss << (i + 1) << ". **" << rc.cause << "** (confidence: " 
                   << static_cast<int>(rc.confidence * 100) << "%)\n";
                ss << "   - " << rc.recommendation << "\n\n";
            }
        }
    }
    
    // Recommendations
    bool has_recommendations = !report.immediate_actions.empty() ||
                              !report.investigation_steps.empty() ||
                              !report.long_term_fixes.empty();
    
    if (has_recommendations) {
        ss << "## Recommendations\n\n";
        
        if (!report.immediate_actions.empty()) {
            ss << "### Immediate Actions\n\n";
            for (const auto& action : report.immediate_actions) {
                ss << "- [ ] " << action << "\n";
            }
            ss << "\n";
        }
        
        if (!report.investigation_steps.empty()) {
            ss << "### Investigation Steps\n\n";
            for (const auto& step : report.investigation_steps) {
                ss << "- [ ] " << step << "\n";
            }
            ss << "\n";
        }
        
        if (!report.long_term_fixes.empty()) {
            ss << "### Long-term Fixes\n\n";
            for (const auto& fix : report.long_term_fixes) {
                ss << "- [ ] " << fix << "\n";
            }
            ss << "\n";
        }
    }
    
    return ss.str();
}

} // namespace diagnostics
} // namespace prosper
