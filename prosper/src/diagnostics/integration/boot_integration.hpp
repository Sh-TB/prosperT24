// diagnostics/integration/boot_integration.hpp — Boot sequence integration points
//
// This file defines where and how diagnostics hooks into the Prosper boot sequence.
// The integration is designed to be:
//   - Observer-only: No behavior changes when disabled
//   - Zero-overhead: All calls are gated on is_enabled()
//   - Non-intrusive: Original code flow is unchanged
#pragma once

// Only include what we need - avoid pulling in all collectors
#include "../core/types.hpp"
#include "../core/context.hpp"
#include "../core/event_bus.hpp"
#include "../collectors/prx_collector.hpp"

#include <sstream>  // For std::ostringstream in on_boot_complete

namespace prosper {
namespace diagnostics {

class BootIntegration {
public:
    // Called at the start of boot_program() before any work begins
    static void on_boot_start(const std::string& dump_root) {
        if (!DiagnosticContext::instance().is_enabled()) return;
        
        DiagnosticContext::instance().emit_phase_event(
            BootPhase::PROCESS_START, true, "", Subsystem::CORE
        );
        
        EventBus::instance().emit(
            "BOOT_START",
            Severity::INFO,
            Subsystem::CORE,
            "Boot started for: " + dump_root,
            SourceLocation(__FILE__, __LINE__, __func__)
        );
    }
    
    // Called before link_program()
    static void on_link_start(size_t module_count) {
        if (!DiagnosticContext::instance().is_enabled()) return;
        
        DiagnosticContext::instance().set_phase(BootPhase::ELF_OPENED);
        
        EventBus::instance().emit(
            "LINK_START",
            Severity::INFO,
            Subsystem::LINKER,
            "Starting link of " + std::to_string(module_count) + " modules",
            SourceLocation(__FILE__, __LINE__, __func__)
        );
    }
    
    // Called after link_program() succeeds (pass module count and import count)
    static void on_link_complete(size_t module_count, size_t import_count) {
        if (!DiagnosticContext::instance().is_enabled()) return;
        
        DiagnosticContext::instance().set_phase(BootPhase::ELF_PARSED);
        
        EventBus::instance().emit(
            "LINK_COMPLETE",
            Severity::INFO,
            Subsystem::LINKER,
            "Linked " + std::to_string(module_count) + " modules, " +
            std::to_string(import_count) + " imports",
            SourceLocation(__FILE__, __LINE__, __func__)
        );
    }
    
    // Called during/after image mapping
    static void on_segments_mapped() {
        if (!DiagnosticContext::instance().is_enabled()) return;
        
        DiagnosticContext::instance().set_phase(BootPhase::SEGMENTS_MAPPED);
        
        EventBus::instance().emit(
            "SEGMENTS_MAPPED_COMPLETE",
            Severity::INFO,
            Subsystem::MEMORY,
            "All segments mapped",
            SourceLocation(__FILE__, __LINE__, __func__)
        );
    }
    
    // Called for each PRX module load start
    static void on_prx_load_start(PrxCollector* prx_collector,
                                   const std::string& path, uint64_t base) {
        if (!DiagnosticContext::instance().is_enabled()) return;
        if (!prx_collector) return;
        
        prx_collector->record_prx_load_start(path, base);
    }
    
    // Called for each PRX module load complete
    static void on_prx_loaded(PrxCollector* prx_collector,
                               const std::string& path, uint64_t size,
                               bool success) {
        if (!DiagnosticContext::instance().is_enabled()) return;
        if (!prx_collector) return;
        
        prx_collector->record_prx_loaded(path, size, success);
    }
    
    // Called after all PRX modules loaded
    static void on_prx_loading_complete() {
        if (!DiagnosticContext::instance().is_enabled()) return;
        
        DiagnosticContext::instance().set_phase(BootPhase::PRX_LOADING);
        
        EventBus::instance().emit(
            "PRX_LOADING_COMPLETE",
            Severity::INFO,
            Subsystem::LOADER,
            "PRX loading phase complete",
            SourceLocation(__FILE__, __LINE__, __func__)
        );
    }
    
    // Called after import resolution (HLE registration)
    static void on_import_resolution_complete(PrxCollector* prx_collector) {
        if (!DiagnosticContext::instance().is_enabled()) return;
        
        DiagnosticContext::instance().set_phase(BootPhase::IMPORT_RESOLUTION);
        
        // Report missing imports if any
        if (prx_collector) {
            auto missing = prx_collector->get_missing_imports();
            if (!missing.empty()) {
                DiagnosticContext::instance().session().stats.errors += missing.size();
                
                EventBus::instance().emit(
                    "MISSING_IMPORTS_DETECTED",
                    Severity::ERROR,
                    Subsystem::LINKER,
                    std::to_string(missing.size()) + " unresolved imports detected",
                    SourceLocation(__FILE__, __LINE__, __func__)
                );
            }
        }
    }
    
    // Called after relocations applied / stubs installed
    static void on_relocations_complete() {
        if (!DiagnosticContext::instance().is_enabled()) return;
        
        DiagnosticContext::instance().set_phase(BootPhase::RELOCATIONS_APPLIED);
        
        EventBus::instance().emit(
            "RELOCATIONS_COMPLETE",
            Severity::INFO,
            Subsystem::LINKER,
            "Relocations applied successfully",
            SourceLocation(__FILE__, __LINE__, __func__)
        );
    }
    
    // Called before run_guest_inits
    static void on_init_start(size_t init_count) {
        if (!DiagnosticContext::instance().is_enabled()) return;
        
        DiagnosticContext::instance().set_phase(BootPhase::THREAD_CREATION);
        
        EventBus::instance().emit(
            "GUEST_INIT_START",
            Severity::INFO,
            Subsystem::THREAD,
            "Running " + std::to_string(init_count) + " init functions",
            SourceLocation(__FILE__, __LINE__, __func__)
        );
    }
    
    // Called after run_guest_inits completes
    static void on_init_complete(bool success) {
        if (!DiagnosticContext::instance().is_enabled()) return;
        
        DiagnosticContext::instance().set_phase(
            BootPhase::ENTRYPOINT_EXECUTED, 
            success,
            success ? "" : "Init function failed"
        );
        
        EventBus::instance().emit(
            success ? "GUEST_INIT_COMPLETE" : "GUEST_INIT_FAILED",
            success ? Severity::INFO : Severity::ERROR,
            Subsystem::THREAD,
            success ? "All init functions completed" : "Init function failed",
            SourceLocation(__FILE__, __LINE__, __func__)
        );
    }
    
    // Called when boot completes successfully
    static void on_boot_complete(uint64_t entry_point) {
        if (!DiagnosticContext::instance().is_enabled()) return;
        
        DiagnosticContext::instance().set_phase(BootPhase::RUNTIME_INITIALIZED);
        
        std::ostringstream ss;
        ss << "Boot complete, entry at 0x" << std::hex << entry_point;
        
        EventBus::instance().emit(
            "BOOT_COMPLETE",
            Severity::INFO,
            Subsystem::CORE,
            ss.str(),
            SourceLocation(__FILE__, __LINE__, __func__)
        );
    }
    
    // Called on boot failure
    static void on_boot_failed(const std::string& error, BootPhase failed_at) {
        if (!DiagnosticContext::instance().is_enabled()) return;
        
        DiagnosticContext::instance().set_phase(BootPhase::PHASE_FAILED, false, error);
        
        EventBus::instance().emit(
            "BOOT_FAILED",
            Severity::CRITICAL,
            Subsystem::CORE,
            "Boot failed at " + std::string(boot_phase_string(failed_at)) + ": " + error,
            SourceLocation(__FILE__, __LINE__, __func__)
        );
    }

private:
    BootIntegration() = delete;  // Static only
};

} // namespace diagnostics
} // namespace prosper
