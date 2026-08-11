// diagnostics/diagnostics.hpp — Main include for AI-Oriented Diagnostics Platform
//
// This is the single entry point for integrating diagnostics into Prosper.
// Include this file to get access to all diagnostic functionality.
//
// Usage:
//   1. Call DiagnosticsIntegration::initialize(config) early in main()
//   2. Use DIAG_EMIT() macros or collector->record_*() methods
//   3. Call DiagnosticsIntegration::shutdown() before exit
//
// The system is zero-overhead when disabled - all calls are gated on
// DiagnosticContext::is_enabled().
#pragma once

// Core types (always available, lightweight)
#include "core/types.hpp"
#include "core/event_bus.hpp"
#include "core/context.hpp"

// Collectors
#include "collectors/collector.hpp"
#include "collectors/elf_collector.hpp"
#include "collectors/prx_collector.hpp"
#include "collectors/hle_collector.hpp"
#include "collectors/memory_collector.hpp"
#include "collectors/crash_collector.hpp"

// Storage
#include "storage/json_writer.hpp"

// AI
#include "ai/ai_report.hpp"

// CLI
#include "cli/cli.hpp"

// Integration hooks
#include "integration/boot_integration.hpp"

namespace prosper {
namespace diagnostics {

// --- Integration Helper Class -----------------------------------------------
//
// Provides a simple interface for initializing and managing the entire
// diagnostics system. Use this in main.cpp or boot_program.cpp.
class DiagnosticsIntegration {
public:
    // Initialize diagnostics from CLI arguments.
    // Parses --diagnostics flags, sets up all collectors, starts session.
    // Returns true if diagnostics were enabled.
    static bool initialize_from_cli(int* argc, char*** argv,
                                     const std::string& game_path = "");
    
    // Initialize with explicit configuration.
    static bool initialize(const DiagnosticsConfig& config);
    
    // Start a new diagnostic session for a specific game.
    static void begin_session(const std::string& game_identifier,
                               const std::string& binary_hash = "");
    
    // End current session and write reports.
    static void end_session();
    
    // Full shutdown - call before process exit.
    static void shutdown();
    
    // Quick check: is diagnostics enabled?
    static bool enabled() { 
        return DiagnosticContext::instance().is_enabled(); 
    }
    
    // Get specific collectors (for direct recording)
    static ElfCollector* elf() {
        return DiagnosticContext::instance().get_collector<ElfCollector>();
    }
    static PrxCollector* prx() {
        return DiagnosticContext::instance().get_collector<PrxCollector>();
    }
    static HleCollector* hle() {
        return DiagnosticContext::instance().get_collector<HleCollector>();
    }
    static MemoryCollector* memory() {
        return DiagnosticContext::instance().get_collector<MemoryCollector>();
    }
    static CrashCollector* crash() {
        return DiagnosticContext::instance().get_collector<CrashCollector>();
    }
    
    // Convenience: emit a phase transition event
    static void phase(BootPhase p, bool success = true, 
                      const std::string& error = "") {
        if (enabled()) {
            DiagnosticContext::instance().set_phase(p, success, error);
        }
    }

private:
    static void register_all_collectors();
};

// --- Inline Implementation ---------------------------------------------------

inline bool DiagnosticsIntegration::initialize_from_cli(int* argc, char*** argv,
                                                        const std::string& game_path) {
    auto parsed = DiagnosticsCli::parse(argc, argv);
    
    if (!parsed.config.enabled) return false;
    
    if (parsed.help_requested) {
        DiagnosticsCli::print_usage();
        return false;
    }
    
    if (!parsed.error_message.empty()) {
        fprintf(stderr, "[diagnostics] CLI error: %s\n", parsed.error_message.c_str());
        return false;
    }
    
    std::string validation_error;
    if (!DiagnosticsCli::validate(parsed.config, &validation_error)) {
        fprintf(stderr, "[diagnostics] Configuration error: %s\n", validation_error.c_str());
        return false;
    }
    
    if (!initialize(parsed.config)) return false;
    
    // Start session immediately if we have a game path
    if (!game_path.empty()) {
        begin_session(game_path);
    }
    
    return true;
}

inline bool DiagnosticsIntegration::initialize(const DiagnosticsConfig& config) {
    auto& ctx = DiagnosticContext::instance();
    ctx.initialize(config);
    
    if (!ctx.is_initialized()) return false;
    
    register_all_collectors();
    
    return true;
}

inline void DiagnosticsIntegration::register_all_collectors() {
    auto& ctx = DiagnosticContext::instance();
    
    // Register core collectors
    ctx.register_collector(std::make_unique<ElfCollector>());
    ctx.register_collector(std::make_unique<PrxCollector>());
    ctx.register_collector(std::make_unique<HleCollector>());
    ctx.register_collector(std::make_unique<MemoryCollector>());
    ctx.register_collector(std::make_unique<CrashCollector>());
}

inline void DiagnosticsIntegration::begin_session(const std::string& game_identifier,
                                                   const std::string& binary_hash) {
    if (!enabled()) return;
    
    DiagnosticContext::instance().begin_session(game_identifier, binary_hash);
}

inline void DiagnosticsIntegration::end_session() {
    if (!enabled()) return;
    
    auto& ctx = DiagnosticContext::instance();
    
    // Generate subsystem reports
    JsonWriter writer(ctx.session().output_directory);
    
    if (auto* e = elf()) writer.write_elf_report(e->generate_report());
    if (auto* p = prx()) writer.write_prx_report(p->generate_report());
    if (auto* p = prx()) writer.write_imports_report(p->generate_report());  // Imports in PRX report
    if (auto* h = hle()) writer.write_hle_report(h->generate_report());
    if (auto* m = memory()) writer.write_memory_report(m->generate_report());
    if (auto* c = crash()) writer.write_crash_report(c->generate_report());
    
    // Write common outputs
    writer.write_session(ctx.session());
    writer.write_timeline(ctx.timeline());
    writer.write_evidence_list(ctx.evidence());
    
    // Generate AI report if requested
    if (ctx.config().ai_report) {
        AiReportGenerator gen;
        auto report = gen.generate(
            ctx.session(),
            ctx.timeline(),
            ctx.evidence(),
            EventBus::instance().event_count(),
            elf() ? elf()->generate_report() : "{}",
            prx() ? prx()->generate_report() : "{}",
            prx() ? prx()->generate_report() : "{}",  // Would be imports-specific
            hle() ? hle()->generate_report() : "{}",
            crash() ? crash()->generate_report() : ""
        );
        
        writer.write_ai_report(gen.to_json(report));
        
        // Also write markdown version
        AiContextWriter ai_writer(ctx.session().output_directory);
        ai_writer.write(
            ctx.session(),
            ctx.timeline(),
            ctx.evidence(),
            EventBus::instance().event_count(),
            ctx.session().stats.errors,
            ctx.session().stats.warnings,
            ctx.current_phase()
        );
    }
    
    // Write summary
    writer.write_summary(
        ctx.session(),
        ctx.timeline(),
        ctx.evidence(),
        EventBus::instance().event_count()
    );
    
    ctx.end_session();
}

inline void DiagnosticsIntegration::shutdown() {
    if (!enabled()) return;
    
    end_session();
    DiagnosticContext::instance().shutdown();
}

} // namespace diagnostics
} // namespace prosper

// --- Convenience Macros ------------------------------------------------------
//
// These macros make it easy to add diagnostic events anywhere in the codebase.
// They compile to nothing when diagnostics is disabled.

#ifdef PROSPER_DIAGNOSTICS_ENABLED
// Full diagnostics - events are always emitted
#define DIAG_EVENT(type, sev, subsys, msg) \
    prosper::diagnostics::EventBus::instance().emit( \
        (type), (sev), (subsys), (msg), \
        __FILE__, __LINE__, __func__)

#define DIAG_PHASE_TRANSITION(phase, success, err) \
    prosper::diagnostics::DiagnosticContext::instance().emit_phase_event( \
        (phase), (success), (err))

#else
// Diagnostics may or may not be enabled - check at runtime
#define DIAG_EVENT(type, sev, subsys, msg) \
    do { \
        if (prosper::diagnostics::DiagnosticContext::instance().is_enabled()) { \
            prosper::diagnostics::EventBus::instance().emit( \
                (type), (sev), (subsys), (msg), \
                __FILE__, __LINE__, __func__); \
        } \
    } while (0)

#define DIAG_PHASE_TRANSITION(phase, success, err) \
    do { \
        if (prosper::diagnostics::DiagnosticContext::instance().is_enabled()) { \
            prosper::diagnostics::DiagnosticContext::instance().emit_phase_event( \
                (phase), (success), (err)); \
        } \
    } while (0)

#endif

// Boot phase convenience macros
#define DIAG_BOOT_PHASE(phase) DIAG_PHASE_TRANSITION((phase), true, "")
#define DIAG_BOOT_PHASE_FAIL(phase, err) DIAG_PHASE_TRANSITION((phase), false, (err))
