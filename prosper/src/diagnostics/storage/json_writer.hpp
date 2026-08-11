// diagnostics/storage/json_writer.hpp — JSON output for diagnostic data
//
// Writes diagnostic events, timeline, evidence, and reports as JSON files.
// Output format is optimized for both human readability and LLM parsing.
#pragma once

#include "../core/types.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <memory>

namespace prosper {
namespace diagnostics {

class JsonWriter {
public:
    explicit JsonWriter(const std::string& output_dir);
    ~JsonWriter() = default;
    
    // --- Session -------------------------------------------------------------
    void write_session(const DiagnosticSession& session);
    
    // --- Events --------------------------------------------------------------
    void write_events(const std::vector<DiagnosticEvent>& events, bool append = false);
    void write_single_event(const DiagnosticEvent& event);
    
    // --- Timeline ------------------------------------------------------------
    void write_timeline(const std::vector<TimelineEntry>& timeline);
    
    // --- Evidence ------------------------------------------------------------
    void write_evidence_list(const std::vector<EvidenceItem>& evidence);
    
    // --- Reports -------------------------------------------------------------
    void write_ai_report(const std::string& report);
    void write_summary(
        const DiagnosticSession& session,
        const std::vector<TimelineEntry>& timeline,
        const std::vector<EvidenceItem>& evidence,
        size_t total_events
    );
    
    // --- Subsystem-specific reports -----------------------------------------
    void write_elf_report(const std::string& json);
    void write_prx_report(const std::string& json);
    void write_imports_report(const std::string& json);
    void write_relocations_report(const std::string& json);
    void write_memory_report(const std::string& json);
    void write_threads_report(const std::string& json);
    void write_gpu_report(const std::string& json);
    void write_video_report(const std::string& json);
    void write_crash_report(const std::string& json);
    void write_hle_report(const std::string& json);
    void write_syscalls_report(const std::string& json);
    
    // Utility: Write raw string to named file
    void write_raw(const std::string& filename, const std::string& content);
    
    const std::string& output_directory() const { return output_dir_; }
    
    // Public utility methods (shared with AiContextWriter)
    std::string format_timestamp(std::chrono::system_clock::time_point tp) const;
    std::string format_duration_ms(uint64_t ms) const;
    
private:
    std::string output_dir_;
    
    void ensure_directory();
    std::string make_path(const std::string& filename) const;
    
    // JSON helpers (minimal - avoid dependency on full JSON library)
    std::string escape_json(const std::string& s) const;
};

// --- AI-Optimized Markdown Output -------------------------------------------
//
// The AI context file is a structured markdown document optimized for LLM analysis.
// It summarizes all diagnostic data into a format that's easy for AI to parse.
class AiContextWriter {
public:
    explicit AiContextWriter(const std::string& output_dir);
    
    void write(
        const DiagnosticSession& session,
        const std::vector<TimelineEntry>& timeline,
        const std::vector<EvidenceItem>& evidence,
        size_t total_events,
        uint64_t errors,
        uint64_t warnings,
        BootPhase final_phase
    );
    
private:
    std::string output_dir_;
    
    std::string generate_boot_status(BootPhase phase, bool had_errors) const;
    std::string generate_phase_details(const std::vector<TimelineEntry>& timeline) const;
    std::string generate_recommendations(
        BootPhase final_phase,
        const std::vector<TimelineEntry>& timeline,
        uint64_t errors
    ) const;
};

} // namespace diagnostics
} // namespace prosper
