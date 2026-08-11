// diagnostics/core/context.hpp — DiagnosticContext: Central coordinator for diagnostics
//
// Single point of access for all diagnostic functionality. Collectors register here,
// the session is managed here, and this is where integration hooks connect.
#pragma once

#include "types.hpp"
#include "event_bus.hpp"
#include <memory>
#include <string>
#include <vector>

namespace prosper {
namespace diagnostics {

// Forward declarations
class Collector;
class StorageBackend;
class AiReportGenerator;

struct DiagnosticsConfig {
    bool        enabled = false;
    bool        ai_report = false;
    std::string output_directory;
    
    // Per-subsystem tracing flags
    bool trace_loader  = false;
    bool trace_prx     = false;
    bool trace_hle     = false;
    bool trace_memory  = false;
    bool trace_gpu     = false;
    bool trace_video   = false;
    bool trace_thread  = false;
    bool trace_crash   = false;
    bool trace_all     = false;  // Overrides individual flags
    
    // Output format options
    bool json_output   = true;
    bool markdown_ai_context = true;  // Generate AI-friendly summary
    
    static DiagnosticsConfig from_cli(int* argc, char*** argv);
};

class DiagnosticContext {
public:
    static DiagnosticContext& instance();
    
    // --- Lifecycle -----------------------------------------------------------
    
    // Initialize diagnostics with the given configuration.
    // Must be called before any collector activity.
    void initialize(const DiagnosticsConfig& config);
    
    // Shutdown: flush all events, write final reports, cleanup.
    void shutdown();
    
    bool is_initialized() const { return initialized_; }
    bool is_enabled() const { return config_.enabled; }
    const DiagnosticsConfig& config() const { return config_; }
    DiagnosticSession& session() { return session_; }
    const DiagnosticSession& session() const { return session_; }
    
    // --- Session Management --------------------------------------------------
    
    // Start a new diagnostic session (generates session_id, sets up output dir)
    void begin_session(
        const std::string& game_identifier,
        const std::string& binary_hash = ""
    );
    
    // End current session (writes final reports)
    void end_session();
    
    // --- Phase Tracking (Boot Timeline) -------------------------------------
    
    // Transition to a new boot phase
    void set_phase(BootPhase phase, bool success = true, const std::string& error = "");
    BootPhase current_phase() const { return current_phase_; }
    TimelineEntry& current_timeline_entry();
    const std::vector<TimelineEntry>& timeline() const { return timeline_; }
    
    // Convenience phase transition with event emission
    void emit_phase_event(
        BootPhase phase,
        bool success = true,
        const std::string& error = "",
        Subsystem subsystem = Subsystem::CORE
    );
    
    // --- Collector Management -----------------------------------------------
    
    // Register a collector (called during init or by subsystem initialization)
    void register_collector(std::unique_ptr<Collector> collector);
    
    // Get a registered collector by type T
    template<typename T>
    T* get_collector() {
        for (auto& c : collectors_) {
            if (auto* ptr = dynamic_cast<T*>(c.get()))
                return ptr;
        }
        return nullptr;
    }
    
    // --- Evidence Management -------------------------------------------------
    
    // Register a new evidence item, returns its ID
    uint64_t add_evidence(EvidenceItem&& evidence);
    
    // Get evidence by ID
    const EvidenceItem* get_evidence(uint64_t id) const;
    const std::vector<EvidenceItem>& evidence() const { return evidence_; }
    
    // --- Direct Event Access -------------------------------------------------
    
    EventBus& event_bus() { return EventBus::instance(); }
    
    // Get all events (for report generation)
    // Note: For large event sets, use storage backend instead
    const std::vector<DiagnosticEvent>* recent_events(size_t max_count = 1000) const;
    
private:
    DiagnosticContext() = default;
    ~DiagnosticContext() = default;
    DiagnosticContext(const DiagnosticContext&) = delete;
    DiagnosticContext& operator=(const DiagnosticContext&) = delete;
    
    void write_session_json();
    void write_final_reports();
    std::string generate_session_id();
    std::string detect_git_revision();
    std::string detect_platform();
    
    bool                        initialized_ = false;
    DiagnosticsConfig           config_;
    DiagnosticSession           session_;
    BootPhase                   current_phase_ = BootPhase::PROCESS_START;
    std::vector<TimelineEntry>  timeline_;
    std::vector<EvidenceItem>   evidence_;
    std::vector<std::unique_ptr<Collector>> collectors_;
    uint64_t                    next_evidence_id_ = 1;
};

// RAII helper for scoped phase tracking
class ScopedPhase {
public:
    ScopedPhase(BootPhase phase, const char* name = nullptr)
        : phase_(phase) {
        DiagnosticContext::instance().emit_phase_event(phase, true);
    }
    
    ~ScopedPhase() {
        if (std::uncaught_exceptions()) {
            DiagnosticContext::instance().set_phase(phase_, false, "exception thrown");
        }
    }
    
    void mark_failed(const std::string& error) {
        DiagnosticContext::instance().set_phase(phase_, false, error);
    }
    
private:
    BootPhase phase_;
};

// Convenience macros for emitting diagnostic events
#define DIAG_EMIT(type, severity, subsystem, message) \
    do { \
        if (prosper::diagnostics::DiagnosticContext::instance().is_enabled()) { \
            prosper::diagnostics::EventBus::instance().emit( \
                (type), (severity), (subsystem), (message), \
                __FILE__, __LINE__, __func__); \
        } \
    } while (0)

#define DIAG_EMIT_ENRICH(type, severity, subsystem, message, enrich_fn) \
    do { \
        if (prosper::diagnostics::DiagnosticContext::instance().is_enabled()) { \
            prosper::diagnostics::EventBus::instance().emit( \
                (type), (severity), (subsystem), (message), \
                __FILE__, __LINE__, __func__, (enrich_fn)); \
        } \
    } while (0)

#define DIAG_PHASE(phase) \
    prosper::diagnostics::ScopedPhase _diag_phase_##__LINE__((phase), #phase)

} // namespace diagnostics
} // namespace prosper
