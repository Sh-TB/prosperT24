// diagnostics/storage/json_writer.cpp — JSON writer implementation
#include "json_writer.hpp"
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <direct.h>
#define mkdir(d, p) _mkdir(d)
#else
#include <sys/stat.h>
#endif

namespace prosper {
namespace diagnostics {

// --- JsonWriter Implementation -----------------------------------------------

JsonWriter::JsonWriter(const std::string& output_dir)
    : output_dir_(output_dir)
{
    ensure_directory();
}

void JsonWriter::ensure_directory() {
    if (output_dir_.empty()) return;
#ifdef _WIN32
    _mkdir(output_dir_.c_str());
#else
    ::mkdir(output_dir_.c_str(), 0755);
#endif
}

std::string JsonWriter::make_path(const std::string& filename) const {
    if (output_dir_.empty()) return filename;
    char sep = '/';
#ifdef _WIN32
    sep = '\\';
#endif
    // Ensure trailing separator
    std::string dir = output_dir_;
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') {
        dir += sep;
    }
    return dir + filename;
}

std::string JsonWriter::escape_json(const std::string& s) const {
    std::ostringstream ss;
    for (char c : s) {
        switch (c) {
            case '"':  ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)(unsigned char)c;
                } else {
                    ss << c;
                }
        }
    }
    return ss.str();
}

std::string JsonWriter::format_timestamp(std::chrono::system_clock::time_point tp) const {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto tm = *std::gmtime(&time_t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string JsonWriter::format_duration_ms(uint64_t ms) const {
    if (ms < 1000) return std::to_string(ms) + "ms";
    if (ms < 60000) return std::to_string(ms / 1000) + "." + std::to_string((ms % 1000) / 10) + "s";
    auto mins = ms / 60000;
    auto secs = (ms % 60000) / 1000;
    return std::to_string(mins) + "m " + std::to_string(secs) + "s";
}

void JsonWriter::write_session(const DiagnosticSession& session) {
    std::ofstream out(make_path("session.json"));
    if (!out.is_open()) return;
    
    out << "{\n";
    out << "  \"session_id\": \"" << escape_json(session.session_id) << "\",\n";
    out << "  \"timestamp\": \"" << format_timestamp(session.timestamp) << "\",\n";
    out << "  \"game_identifier\": \"" << escape_json(session.game_identifier) << "\",\n";
    out << "  \"binary_hash\": \"" << escape_json(session.binary_hash) << "\",\n";
    out << "  \"configuration\": \"" << escape_json(session.configuration) << "\",\n";
    out << "  \"platform\": \"" << escape_json(session.platform) << "\",\n";
    out << "  \"git_revision\": \"" << escape_json(session.git_revision) << "\",\n";
    out << "  \"prosper_version\": \"" << escape_json(session.prosper_version) << "\",\n";
    out << "  \"output_directory\": \"" << escape_json(session.output_directory) << "\",\n";
    
    // Stats
    out << "  \"statistics\": {\n";
    out << "    \"total_events\": " << session.stats.total_events << ",\n";
    out << "    \"total_evidence\": " << session.stats.total_evidence << ",\n";
    out << "    \"errors\": " << session.stats.errors << ",\n";
    out << "    \"warnings\": " << session.stats.warnings << ",\n";
    out << "    \"final_phase\": \"" << boot_phase_string(session.stats.final_phase) << "\"\n";
    out << "  }\n";
    
    out << "}\n";
}

void JsonWriter::write_events(const std::vector<DiagnosticEvent>& events, bool append) {
    auto mode = std::ios::out;
    if (append) mode |= std::ios::app;
    
    std::ofstream out(make_path("events.json"), mode);
    if (!out.is_open()) return;
    
    if (!append || events.empty()) {
        out << "[\n";
    }
    
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        
        out << "  {\n";
        out << "    \"id\": " << e.event_id.id << ",\n";
        // Convert steady_clock to milliseconds since session start would be better,
        // but for now use a simple representation
        out << "    \"time_ms\": " << e.event_id.id << ",\n";  // Monotonic proxy
        out << "    \"type\": \"" << escape_json(e.type) << "\",\n";
        out << "    \"severity\": \"" << severity_string(e.severity) << "\",\n";
        out << "    \"subsystem\": \"" << subsystem_string(e.subsystem) << "\",\n";
        out << "    \"thread\": \"" << escape_json(e.thread_name) << "\",\n";
        out << "    \"source\": \"" << escape_json(e.source.file) << ":" 
            << e.source.line << "\",\n";
        out << "    \"message\": \"" << escape_json(e.message) << "\",\n";
        
        // Data fields
        out << "    \"data\": {";
        for (size_t j = 0; j < e.data.size(); ++j) {
            if (j > 0) out << ",";
            const auto& d = e.data[j];
            out << "\n      \"" << escape_json(d.key) << "\": ";
            switch (d.type) {
                case DiagnosticEvent::DataField::STRING:
                    out << "\"" << escape_json(d.string_val) << "\"";
                    break;
                case DiagnosticEvent::DataField::INT:
                    out << d.int_val;
                    break;
                case DiagnosticEvent::DataField::UINT:
                    out << d.uint_val;
                    break;
                case DiagnosticEvent::DataField::FLOAT:
                    out << d.float_val;
                    break;
                case DiagnosticEvent::DataField::BOOL:
                    out << (d.bool_val ? "true" : "false");
                    break;
                case DiagnosticEvent::DataField::ARRAY:
                    out << "[...]";  // Simplified
                    break;
            }
        }
        if (!e.data.empty()) out << "\n    ";
        out << "},\n";
        
        // Evidence references
        if (!e.evidence_refs.empty()) {
            out << "    \"evidence_refs\": [";
            for (size_t j = 0; j < e.evidence_refs.size(); ++j) {
                if (j > 0) out << ", ";
                out << e.evidence_refs[j];
            }
            out << "],\n";
        }
        
        // Confidence
        out << "    \"confidence\": " << e.confidence << "\n";
        
        out << "  }";
        if (i < events.size() - 1) out << ",";
        out << "\n";
    }
    
    // Note: We don't close the array here in append mode - caller handles that
    if (!append) {
        out << "]\n";
    }
}

void JsonWriter::write_single_event(const DiagnosticEvent& event) {
    write_events({event}, true);
}

void JsonWriter::write_timeline(const std::vector<TimelineEntry>& timeline) {
    std::ofstream out(make_path("timeline.json"));
    if (!out.is_open()) return;
    
    out << "{\n";
    out << "  \"phases\": [\n";
    
    for (size_t i = 0; i < timeline.size(); ++i) {
        const auto& entry = timeline[i];
        
        out << "    {\n";
        out << "      \"phase\": \"" << boot_phase_string(entry.phase) << "\",\n";
        out << "      \"phase_value\": " << static_cast<uint64_t>(entry.phase) << ",\n";
        if (entry.from_phase != BootPhase::PROCESS_START || i > 0) {
            out << "      \"from_phase\": \"" << boot_phase_string(entry.from_phase) << "\",\n";
        }
        out << "      \"duration_ms\": " << entry.duration_ms << ",\n";
        out << "      \"status\": " << (entry.success ? "\"OK\"" : "\"FAILED\"") << ",\n";
        if (!entry.error_message.empty()) {
            out << "      \"error\": \"" << escape_json(entry.error_message) << "\",\n";
        }
        out << "      \"event_id\": " << entry.event_id << "\n";
        
        out << "    }";
        if (i < timeline.size() - 1) out << ",";
        out << "\n";
    }
    
    out << "  ]\n";
    out << "}\n";
}

void JsonWriter::write_evidence_list(const std::vector<EvidenceItem>& evidence) {
    std::ofstream out(make_path("evidence.json"));
    if (!out.is_open()) return;
    
    out << "{\n";
    out << "  \"items\": [\n";
    
    for (size_t i = 0; i < evidence.size(); ++i) {
        const auto& item = evidence[i];
        
        out << "    {\n";
        out << "      \"id\": " << item.id << ",\n";
        out << "      \"type\": ";
        switch (item.type) {
            case EvidenceType::SCREENSHOT:     out << "\"SCREENSHOT\""; break;
            case EvidenceType::MEMORY_DUMP:    out << "\"MEMORY_DUMP\""; break;
            case EvidenceType::REGISTER_STATE: out << "\"REGISTER_STATE\""; break;
            case EvidenceType::STACK_TRACE:    out << "\"STACK_TRACE\""; break;
            case EvidenceType::LOG_FRAGMENT:   out << "\"LOG_FRAGMENT\""; break;
            case EvidenceType::METRIC:         out << "\"METRIC\""; break;
            default:                           out << "\"CUSTOM\""; break;
        }
        out << ",\n";
        out << "      \"description\": \"" << escape_json(item.description) << "\",\n";
        out << "      \"file\": \"" << escape_json(item.file_path) << "\",\n";
        out << "      \"mime_type\": \"" << escape_json(item.mime_type) << "\"\n";
        
        out << "    }";
        if (i < evidence.size() - 1) out << ",";
        out << "\n";
    }
    
    out << "  ]\n";
    out << "}\n";
}

void JsonWriter::write_ai_report(const std::string& report) {
    write_raw("ai_report.json", report);
}

void JsonWriter::write_summary(
    const DiagnosticSession& session,
    const std::vector<TimelineEntry>& timeline,
    const std::vector<EvidenceItem>& evidence,
    size_t total_events
) {
    std::ofstream out(make_path("summary.json"));
    if (!out.is_open()) return;
    
    out << "{\n";
    out << "  \"session_id\": \"" << escape_json(session.session_id) << "\",\n";
    out << "  \"game\": \"" << escape_json(session.game_identifier) << "\",\n";
    
    // Boot status
    bool had_errors = session.stats.errors > 0;
    auto final_phase = session.stats.final_phase;
    
    out << "  \"boot_status\": ";
    if (final_phase == BootPhase::BOOT_COMPLETE) {
        out << "\"COMPLETE\"";
    } else if (had_errors) {
        out << "\"FAILED\"";
    } else {
        out << "\"INCOMPLETE\"";
    }
    out << ",\n";
    
    out << "  \"last_phase\": \"" << boot_phase_string(final_phase) << "\",\n";
    out << "  \"total_phases\": " << timeline.size() << ",\n";
    out << "  \"completed_phases\": " << std::count_if(timeline.begin(), timeline.end(),
        [](const TimelineEntry& e) { return e.success; }) << ",\n";
    out << "  \"failed_phases\": " << std::count_if(timeline.begin(), timeline.end(),
        [](const TimelineEntry& e) { return !e.success; }) << ",\n";
    out << "  \"total_events\": " << total_events << ",\n";
    out << "  \"total_evidence\": " << evidence.size() << ",\n";
    out << "  \"total_errors\": " << session.stats.errors << ",\n";
    out << "  \"total_warnings\": " << session.stats.warnings << "\n";
    
    out << "}\n";
}

void JsonWriter::write_elf_report(const std::string& json) {
    write_raw("elf_report.json", json);
}
void JsonWriter::write_prx_report(const std::string& json) {
    write_raw("prx_report.json", json);
}
void JsonWriter::write_imports_report(const std::string& json) {
    write_raw("imports.json", json);
}
void JsonWriter::write_relocations_report(const std::string& json) {
    write_raw("relocations.json", json);
}
void JsonWriter::write_memory_report(const std::string& json) {
    write_raw("memory_report.json", json);
}
void JsonWriter::write_threads_report(const std::string& json) {
    write_raw("threads.json", json);
}
void JsonWriter::write_gpu_report(const std::string& json) {
    write_raw("gpu_report.json", json);
}
void JsonWriter::write_video_report(const std::string& json) {
    write_raw("video_report.json", json);
}
void JsonWriter::write_crash_report(const std::string& json) {
    write_raw("crash.json", json);
}
void JsonWriter::write_hle_report(const std::string& json) {
    write_raw("hle_report.json", json);
}
void JsonWriter::write_syscalls_report(const std::string& json) {
    write_raw("syscalls.json", json);
}

void JsonWriter::write_raw(const std::string& filename, const std::string& content) {
    std::ofstream out(make_path(filename));
    if (out.is_open()) {
        out << content;
    }
}


// --- AiContextWriter Implementation ------------------------------------------

AiContextWriter::AiContextWriter(const std::string& output_dir)
    : output_dir_(output_dir)
{
}

void AiContextWriter::write(
    const DiagnosticSession& session,
    const std::vector<TimelineEntry>& timeline,
    const std::vector<EvidenceItem>& evidence,
    size_t total_events,
    uint64_t errors,
    uint64_t warnings,
    BootPhase final_phase
) {
    JsonWriter json_helper(output_dir_);
    std::string path = output_dir_.empty() ? "ai_context.md" : output_dir_ + "/ai_context.md";
    std::ofstream out(path);
    if (!out.is_open()) return;
    
    out << "# Prosper Diagnostics — AI Analysis Context\n\n";
    out << "**Generated**: " << json_helper.format_timestamp(std::chrono::system_clock::now()) << "\n\n";
    
    // Game identification
    out << "## Game\n\n";
    out << "- **Identifier**: `" << session.game_identifier << "`\n";
    out << "- **Platform**: " << session.platform << "\n";
    out << "- **Git Revision**: `" << session.git_revision << "`\n";
    out << "- **Configuration**: " << session.configuration << "\n\n";
    
    // Boot status summary
    out << "## Boot Status\n\n";
    out << generate_boot_status(final_phase, errors > 0) << "\n\n";
    
    // Phase details
    out << "## Boot Phases\n\n";
    out << generate_phase_details(timeline) << "\n\n";
    
    // Statistics
    out << "## Statistics\n\n";
    out << "| Metric | Value |\n";
    out << "|--------|-------|\n";
    out << "| Total Events | " << total_events << " |\n";
    out << "| Errors | " << errors << " |\n";
    out << "| Warnings | " << warnings << " |\n";
    out << "| Evidence Items | " << evidence.size() << " |\n";
    out << "| Final Phase | `" << boot_phase_string(final_phase) << "` |\n\n";
    
    // Recommendations
    out << "## Recommended Actions\n\n";
    out << generate_recommendations(final_phase, timeline, errors) << "\n\n";
    
    // Available files
    out << "## Available Data Files\n\n";
    out << "```\n";
    out << "diagnostic_run/\n";
    out << "├── session.json       # Session metadata\n";
    out << "├── timeline.json      # Boot phase timeline\n";
    out << "├── events.json        # All diagnostic events\n";
    out << "├── evidence.json      # Evidence item index\n";
    out << "├── elf_report.json    # ELF analysis\n";
    out << "├── prx_report.json    # PRX module details\n";
    out << "├── imports.json       # Import/export status\n";
    out << "├── relocations.json   # Relocation records\n";
    out << "├── memory_report.json # Memory operations\n";
    out << "├── threads.json       # Thread activity\n";
    out << "├── gpu_report.json    # GPU pipeline events\n";
    out << "├── video_report.json  # VideoOut/presentation\n";
    out << "├── hle_report.json    # HLE function calls\n";
    out << "├── syscalls.json      # System call trace\n";
    out << "├── crash.json         # Crash analysis (if applicable)\n";
    out << "├── ai_report.json     # AI-generated analysis\n";
    out << "└── ai_context.md      # This file\n";
    out << "```\n\n";
}

std::string AiContextWriter::generate_boot_status(BootPhase phase, bool had_errors) const {
    std::ostringstream ss;
    
    switch (phase) {
        case BootPhase::BOOT_COMPLETE:
            ss << "**Status**: ✅ Boot completed successfully\n";
            break;
        case BootPhase::FIRST_FRAME_CAPTURED:
            ss << "**Status**: 🟡 First frame captured, finalizing boot\n";
            break;
        case BootPhase::FIRST_FRAME_ATTEMPT:
            ss << "**Status**: 🟡 Attempting first frame presentation\n";
            break;
        case BootPhase::VIDEOOUT_INITIALIZED:
            ss << "**Status**: 🟡 VideoOut initialized, awaiting frame\n";
            break;
        case BootPhase::RUNTIME_INITIALIZED:
            ss << "**Status**: 🟡 Runtime initialized, entering main loop\n";
            break;
        case BootPhase::ENTRYPOINT_EXECUTED:
            ss << "**Status**: 🟠 Entry point executed, initializing runtime\n";
            break;
        case BootPhase::THREAD_CREATION:
            ss << "**Status**: 🟠 Creating threads\n";
            break;
        case BootPhase::IMPORT_RESOLUTION:
            ss << "**Status**: 🟠 Resolving imports\n";
            if (had_errors) ss << " (**Missing imports detected**)";
            break;
        case BootPhase::PRX_LOADING:
            ss << "**Status**: 🟠 Loading PRX modules\n";
            break;
        case BootPhase::RELOCATIONS_APPLIED:
            ss << "**Status**: 🟡 Applying relocations\n";
            break;
        case BootPhase::SEGMENTS_MAPPED:
            ss << "**Status**: 🟡 Mapping memory segments\n";
            break;
        case BootPhase::ELF_PARSED:
            ss << "**Status**: 🟢 ELF parsed successfully\n";
            break;
        case BootPhase::ELF_OPENED:
            ss << "**Status**: 🟢 ELF opened\n";
            break;
        case BootPhase::PROCESS_START:
            ss << "**Status**: ⚪ Process starting\n";
            break;
        case BootPhase::PHASE_FAILED:
            ss << "**Status**: ❌ **Boot failed**\n";
            break;
        default:
            ss << "**Status**: Unknown phase " << static_cast<uint64_t>(phase) << "\n";
    }
    
    return ss.str();
}

std::string AiContextWriter::generate_phase_details(const std::vector<TimelineEntry>& timeline) const {
    std::ostringstream ss;
    JsonWriter json_helper(output_dir_);
    
    ss << "| Phase | Status | Duration | Notes |\n";
    ss << "|-------|--------|----------|-------|\n";
    
    for (const auto& entry : timeline) {
        ss << "| `" << boot_phase_string(entry.phase) << "` | ";
        ss << (entry.success ? "✅ OK" : "❌ FAIL");
        ss << " | " << json_helper.format_duration_ms(entry.duration_ms) << " | ";
        if (!entry.error_message.empty()) {
            ss << entry.error_message;
        }
        ss << " |\n";
    }
    
    return ss.str();
}

std::string AiContextWriter::generate_recommendations(
    BootPhase final_phase,
    const std::vector<TimelineEntry>& timeline,
    uint64_t errors
) const {
    std::ostringstream ss;
    
    // Find first failed phase
    const TimelineEntry* first_failure = nullptr;
    for (const auto& entry : timeline) {
        if (!entry.success) {
            first_failure = &entry;
            break;
        }
    }
    
    if (final_phase == BootPhase::BOOT_COMPLETE) {
        ss << "1. Boot completed successfully. Review warnings for optimization opportunities.\n";
        ss << "2. Check `gpu_report.json` and `video_report.json` for rendering issues.\n";
        ss << "3. Analyze performance data in `events.json` for bottlenecks.\n";
    } else if (first_failure) {
        ss << "1. **Investigate phase failure**: `" << boot_phase_string(first_failure->phase) 
           << "` failed.\n";
        if (!first_failure->error_message.empty()) {
            ss << "   Error: " << first_failure->error_message << "\n";
        }
        
        // Specific recommendations based on phase
        switch (first_failure->phase) {
            case BootPhase::IMPORT_RESOLUTION:
                ss << "2. Check `imports.json` for unresolved imports.\n";
                ss << "3. Missing HLE exports may need implementation.\n";
                break;
            case BootPhase::PRX_LOADING:
                ss << "2. Verify PRX module files exist in dump directory.\n";
                ss << "3. Check `prx_report.json` for module load failures.\n";
                break;
            case BootPhase::SEGMENTS_MAPPED:
                ss << "2. Memory mapping failed — check address space layout.\n";
                ss << "3. Review `memory_report.json` for conflicts.\n";
                break;
            case BootPhase::RELOCATIONS_APPLIED:
                ss << "2. Relocation processing error — check `relocations.json`.\n";
                ss << "3. Invalid relocation targets may indicate corrupted binary.\n";
                break;
            case BootPhase::ENTRYPOINT_EXECUTED:
                ss << "2. Entry point execution faulted — check `crash.json`.\n";
                ss << "3. Review stack trace for crash location.\n";
                break;
            default:
                ss << "2. Review relevant subsystem report for details.\n";
                ss << "3. Examine events before failure for context.\n";
                break;
        }
    } else {
        ss << "1. Boot stopped at `" << boot_phase_string(final_phase) << "` without explicit failure.\n";
        ss << "2. The process may have exited or hung at this point.\n";
        ss << "3. Check system logs for external signals or resource issues.\n";
    }
    
    return ss.str();
}

} // namespace diagnostics
} // namespace prosper
