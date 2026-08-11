// diagnostics/ai/ai_report.hpp — AI Report Generator
//
// Generates an analysis report optimized for LLM consumption.
// Synthesizes all diagnostic data into actionable insights about
// boot failures, missing imports, and recommended fixes.
#pragma once

#include "../core/types.hpp"
#include <string>
#include <vector>

namespace prosper {
namespace diagnostics {

struct RootCause {
    std::string     cause;           // Description of suspected root cause
    double          confidence = 0.0; // 0.0 - 1.0
    std::vector<std::string> evidence;  // Supporting evidence items
    std::string     recommendation;  // What to do about it
};

struct AiReport {
    // Summary
    std::string     game_identifier;
    BootPhase       final_phase;
    bool            boot_success = false;
    
    // Analysis
    std::string     failure_point;   // Where boot stopped, in human terms
    std::string     likely_cause;    // Most probable cause of failure
    
    // Root causes (ordered by confidence)
    std::vector<RootCause> root_causes;
    
    // Evidence summary
    size_t          total_events = 0;
    size_t          error_count = 0;
    size_t          warning_count = 0;
    
    // Recommendations
    std::vector<std::string> immediate_actions;      // Do first
    std::vector<std::string> investigation_steps;    // Look into
    std::vector<std::string> long_term_fixes;        // Implement properly
    
    // Metadata
    std::string     generated_at;
    uint64_t        generation_time_ms = 0;
};

class AiReportGenerator {
public:
    AiReportGenerator() = default;
    
    // Generate complete AI report from diagnostic session data
    AiReport generate(
        const DiagnosticSession& session,
        const std::vector<TimelineEntry>& timeline,
        const std::vector<EvidenceItem>& evidence,
        size_t total_events,
        
        // Subsystem reports (as JSON strings for flexibility)
        const std::string& elf_report,
        const std::string& prx_report,
        const std::string& imports_report,
        const std::string& hle_report,
        const std::string& crash_report
    );
    
    // Convert report to JSON string
    std::string to_json(const AiReport& report) const;
    
    // Convert report to markdown (for ai_context.md)
    std::string to_markdown(const AiReport& report) const;

private:
    void analyze_timeline(AiReport& report, 
                          const std::vector<TimelineEntry>& timeline);
    void analyze_imports(AiReport& report,
                         const std::string& imports_report);
    void analyze_hle(AiReport& report,
                     const std::string& hle_report);
    void analyze_crash(AiReport& report,
                       const std::string& crash_report);
    
    RootCause make_root_cause(const std::string& cause, double confidence,
                              const std::string& recommendation) const;
    
    std::string phase_to_human(BootPhase phase) const;
    std::string current_timestamp() const;
};

} // namespace diagnostics
} // namespace prosper
