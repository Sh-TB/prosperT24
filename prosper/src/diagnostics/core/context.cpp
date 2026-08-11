// diagnostics/core/context.cpp — DiagnosticContext implementation
#include "context.hpp"
#include "../collectors/collector.hpp"
#include "../storage/json_writer.hpp"
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/utsname.h>
#include <unistd.h>
#endif

namespace prosper {
namespace diagnostics {

DiagnosticContext& DiagnosticContext::instance() {
    static DiagnosticContext ctx;
    return ctx;
}

void DiagnosticContext::initialize(const DiagnosticsConfig& config) {
    if (initialized_) return;
    
    config_ = config;
    
    if (config_.enabled) {
        EventBus::instance().enable(true);
        EventBus::instance().start();
        
        // Auto-register storage subscriber
        // (Storage backend will be initialized when session begins)
        
        initialized_ = true;
        
        EventBus::instance().emit(
            "SYSTEM_INIT",
            Severity::INFO,
            Subsystem::CORE,
            "Diagnostics system initialized",
            SourceLocation(__FILE__, __LINE__, __func__)
        );
    }
}

void DiagnosticContext::shutdown() {
    if (!initialized_) return;
    
    end_session();
    
    EventBus::instance().stop();
    EventBus::instance().enable(false);
    
    collectors_.clear();
    evidence_.clear();
    timeline_.clear();
    
    initialized_ = false;
}

std::string DiagnosticContext::generate_session_id() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::gmtime(&time_t);
    
    std::ostringstream ss;
    ss << "diag_"
       << std::put_time(&tm, "%Y%m%d_%H%M%S")
       << "_"
       << std::setw(4) << std::setfill('0')
       << (std::random_device()() % 10000);
    
    return ss.str();
}

std::string DiagnosticContext::detect_git_revision() {
    // Try to get git revision from build info or environment
    const char* rev = getenv("PROSPER_GIT_REVISION");
    if (rev && rev[0]) return rev;
    
    // Could also read from build_revision.hpp at compile time
    return "unknown";
}

std::string DiagnosticContext::detect_platform() {
#ifdef _WIN32
    OSVERSIONINFOEX vi = { sizeof(OSVERSIONINFOEX) };
    GetVersionEx((OSVERSIONINFO*)&vi);
    return std::string("Windows ") + std::to_string(vi.dwMajorVersion) + "." 
           + std::to_string(vi.dwMinorVersion);
#else
    struct utsname uts;
    if (uname(&uts) == 0) {
        return std::string(uts.sysname) + " " + uts.release + " " + uts.machine;
    }
    return "unknown";
#endif
}

void DiagnosticContext::begin_session(
    const std::string& game_identifier,
    const std::string& binary_hash
) {
    session_ = DiagnosticSession();
    session_.session_id = generate_session_id();
    session_.game_identifier = game_identifier.empty() ? "unknown" : game_identifier;
    session_.binary_hash = binary_hash;
    session_.platform = detect_platform();
    session_.git_revision = detect_git_revision();
    session_.timestamp = std::chrono::system_clock::now();
    session_.stats.boot_start = std::chrono::steady_clock::now();
    
    // Build configuration summary
    std::ostringstream cfg_ss;
    cfg_ss << "--diagnostics";
    if (config_.ai_report) cfg_ss << " --ai-report";
    if (config_.trace_all) cfg_ss << " --trace-all";
    else {
        if (config_.trace_loader) cfg_ss << " --trace-loader";
        if (config_.trace_prx) cfg_ss << " --trace-prx";
        if (config_.trace_hle) cfg_ss << " --trace-hle";
        if (config_.trace_memory) cfg_ss << " --trace-memory";
        if (config_.trace_gpu) cfg_ss << " --trace-gpu";
        if (config_.trace_video) cfg_ss << " --trace-video";
        if (config_.trace_thread) cfg_ss << " --trace-thread";
        if (config_.trace_crash) cfg_ss << " --trace-crash";
    }
    session_.configuration = cfg_ss.str();
    
    // Set output directory
    if (config_.output_directory.empty()) {
        session_.output_directory = "./diagnostic_" + session_.session_id;
    } else {
        session_.output_directory = config_.output_directory;
    }
    
    // Create output directory
#ifdef _WIN32
    CreateDirectoryA(session_.output_directory.c_str(), nullptr);
#else
    mkdir(session_.output_directory.c_str(), 0755);
#endif
    
    current_phase_ = BootPhase::PROCESS_START;
    timeline_.clear();
    evidence_.clear();
    
    // Record initial timeline entry
    TimelineEntry entry;
    entry.phase = BootPhase::PROCESS_START;
    entry.start_time = std::chrono::steady_clock::now();
    entry.end_time = entry.start_time;
    entry.success = true;
    timeline_.push_back(entry);
    
    write_session_json();
    
    EventBus::instance().emit(
        "SESSION_START",
        Severity::INFO,
        Subsystem::CORE,
        "Diagnostic session started: " + session_.session_id,
        SourceLocation(__FILE__, __LINE__, __func__)
    );
}

void DiagnosticContext::end_session() {
    session_.stats.boot_end_or_now = std::chrono::steady_clock::now();
    session_.stats.final_phase = current_phase_;
    
    // Finalize last timeline entry
    if (!timeline_.empty()) {
        auto& last = timeline_.back();
        last.end_time = session_.stats.boot_end_or_now;
        last.duration_ms = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                last.end_time - last.start_time
            ).count()
        );
    }
    
    write_final_reports();
    
    EventBus::instance().emit(
        "SESSION_END",
        Severity::INFO,
        Subsystem::CORE,
        "Diagnostic session ended",
        SourceLocation(__FILE__, __LINE__, __func__)
    );
}

void DiagnosticContext::set_phase(BootPhase phase, bool success, const std::string& error) {
    if (!timeline_.empty()) {
        // Close current phase
        auto& prev = timeline_.back();
        prev.end_time = std::chrono::steady_clock::now();
        prev.success = success && (phase > current_phase_ || phase == BootPhase::PHASE_FAILED);  // Don't mark failed unless explicitly told
        prev.duration_ms = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                prev.end_time - prev.start_time
            ).count()
        );
        if (!success && !error.empty()) {
            prev.error_message = error;
            session_.stats.errors++;
        }
    }
    
    current_phase_ = phase;
    
    // Open new phase entry
    TimelineEntry entry;
    entry.phase = phase;
    entry.from_phase = timeline_.empty() ? BootPhase::PROCESS_START : timeline_.back().phase;
    entry.start_time = std::chrono::steady_clock::now();
    entry.success = true;  // Assume success until marked otherwise
    timeline_.push_back(entry);
    
    session_.stats.final_phase = phase;
}

TimelineEntry& DiagnosticContext::current_timeline_entry() {
    if (timeline_.empty()) {
        static TimelineEntry empty;
        return empty;
    }
    return timeline_.back();
}

void DiagnosticContext::emit_phase_event(
    BootPhase phase,
    bool success,
    const std::string& error,
    Subsystem subsystem
) {
    set_phase(phase, success, error);
    
    std::ostringstream msg;
    msg << "Boot phase: " << boot_phase_string(phase);
    if (!success) msg << " [FAILED: " << error << "]";
    else msg << " [OK]";
    
    EventBus::instance().emit(
        "PHASE_TRANSITION",
        success ? Severity::INFO : Severity::ERROR,
        subsystem,
        msg.str(),
        SourceLocation(__FILE__, __LINE__, __func__),
        [phase](DiagnosticEvent& e) {
            e.add_string("phase", boot_phase_string(phase));
            e.add_uint("phase_value", static_cast<uint64_t>(phase));
        }
    );
}

void DiagnosticContext::register_collector(std::unique_ptr<Collector> collector) {
    if (collector) {
        collectors_.push_back(std::move(collector));
    }
}

uint64_t DiagnosticContext::add_evidence(EvidenceItem&& evidence) {
    evidence.id = next_evidence_id_++;
    evidence.captured_at = std::chrono::steady_clock::now();
    evidence_.push_back(std::move(evidence));
    session_.stats.total_evidence = evidence_.size();
    return evidence.id;
}

const EvidenceItem* DiagnosticContext::get_evidence(uint64_t id) const {
    for (const auto& e : evidence_) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

const std::vector<DiagnosticEvent>* DiagnosticContext::recent_events(size_t max_count) const {
    // This would require event storage - for now return null
    // Real implementation would use a ring buffer or query storage backend
    return nullptr;
}

void DiagnosticContext::write_session_json() {
    // Session JSON is written by storage backend
    // This is a placeholder that will be connected to real storage
}

void DiagnosticContext::write_final_reports() {
    // Reports are written by their respective generators
    // AI report, summary, etc.
}

} // namespace diagnostics
} // namespace prosper
