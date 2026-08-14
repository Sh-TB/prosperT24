/**
 * Production Integration Example
 * 
 * Shows how to integrate Diagnostics Infrastructure into Prosper's boot flow.
 * This file demonstrates the REAL call site for PluginRegistry.
 * 
 * Location: Near existing HLE initialization / boot phase tracking
 * 
 * INTEGRATION POINTS:
 * 1. Early in boot: initialize() + register "boot_state" plugin
 * 2. During phases: record_boot_phase() calls
 * 3. On shutdown: shutdown()
 */

#include "diagnostics/diagnostics.hpp"

namespace prosper {
namespace boot {

/**
 * Initialize diagnostics and register core plugins.
 * Call this EARLY in boot sequence, after basic subsystems are ready.
 */
bool initialize_diagnostics() {
#ifdef PROSPER_DIAGNOSTICS
    // Initialize the diagnostics system
    if (!prosper::diagnostics::initialize()) {
        return false;
    }
    
    // Register the "boot_state" plugin
    // This is the PRODUCTION CALL SITE required by PR #2513
    prosper::diagnostics::PluginInfo boot_plugin{
        "boot_state",           // name
        "1.0.0",               // version
        "Boot phase diagnostics and state tracking"  // description
    };
    
    boot_plugin.author = "Prosper Team";
    
    // Optional: Add lifecycle callbacks
    boot_plugin.on_initialize = []() -> bool {
        // Boot state plugin initialization logic
        prosper::diagnostics::emit_event(
            "plugin",
            "Boot state plugin initialized",
            prosper::diagnostics::Severity::Info,
            PROSPER_DIAG_HERE()
        );
        return true;
    };
    
    boot_plugin.on_shutdown = []() -> void {
        // Cleanup on shutdown
        prosper::diagnostics::emit_event(
            "plugin",
            "Boot state plugin shutting down",
            prosper::diagnostics::Severity::Info,
            PROSPER_DIAG_HERE()
        );
    };
    
    bool registered = prosper::diagnostics::plugin_registry().register_plugin(boot_plugin);
    
    if (!registered) {
        // Plugin registration failed (duplicate?)
        return false;
    }
    
#endif // PROSPER_DIAGNOSTICS
    
    return true;  // Success (or no-op in disabled mode)
}

/**
 * Record boot phase transitions.
 * Call this at each major boot milestone.
 * 
 * Example usage in boot_program.cpp or equivalent:
 * 
 *   // After config loading:
 *   record_boot_phase(BootPhase::ConfigLoading, PROSPER_DIAG_HERE(), "Config loaded");
 *   
 *   // After HLE setup:
 *   record_boot_phase(BootPhase::HLESetup, PROSPER_DIAG_HERE(), "HLE initialized");
 */
void record_boot_phase_with_diagnostics(
    prosper::diagnostics::BootPhase phase,
    const char* message = nullptr)
{
    std::string msg = message ? message : "";
    
#ifdef PROSPER_DIAGNOSTICS
    prosper::diagnostics::record_boot_phase(phase, PROSPER_DIAG_HERE(), msg);
#else
    // In disabled mode, still call the stub (no-op but preserves API)
    (void)phase;
    (void)msg;
#endif
}

/**
 * Shutdown diagnostics system.
 * Call this during clean shutdown.
 */
void shutdown_diagnostics() {
#ifdef PROSPER_DIAGNOSTICS
    // Mark boot as complete before shutdown
    if (!prosper::diagnostics::boot_completed_successfully()) {
        // Record that we're shutting down without successful boot
        prosper::diagnostics::record_boot_phase(
            prosper::diagnostics::BootPhase::Error,
            PROSPER_DIAG_HERE(),
            "Shutdown without successful boot completion"
        );
    }
    
    // Export final report before shutdown
    std::string report = prosper::diagnostics::export_full_report_json();
    // Could write to file here for post-mortem analysis
    
    // Shutdown all plugins and diagnostics
    prosper::diagnostics::shutdown();
#endif
}

} // namespace boot
} // namespace prosper

// ============================================================================
// EXAMPLE: How this integrates into actual boot flow
// ============================================================================

/*
// In boot_program.cpp or equivalent:

extern "C" int prosper_main(int argc, char* argv[]) {
    // ========================================
    // VERY EARLY BOOT - Initialize Diagnostics
    // ========================================
    
    if (!prosper::boot::initialize_diagnostics()) {
        // Diagnostics init failed (only in enabled mode)
        // Continue anyway - diagnostics is optional
    }
    
    // Record initial boot phase
    prosper::boot::record_boot_phase_with_diagnostics(
        prosper::diagnostics::BootPhase::Initialization,
        "Starting Prosper emulator"
    );
    
    // ========================================
    // CONFIG LOADING PHASE
    // ========================================
    
    // ... load configuration ...
    
    prosper::boot::record_boot_phase_with_diagnostics(
        prosper::diagnostics::BootPhase::ConfigLoading,
        "Configuration loaded successfully"
    );
    
    // ========================================
    // MODULE LOADING PHASE
    // ========================================
    
    // ... load modules ...
    
    prosper::boot::record_boot_phase_with_diagnostics(
        prosper::diagnostics::BootPhase::ModuleLoading,
        "All modules loaded"
    );
    
    // ========================================
    // HLE SETUP PHASE
    // ========================================
    
    // ... initialize HLE functions ...
    
    // THIS IS WHERE THE "boot_state" PLUGIN IS MOST VALUABLE
    // It tracks whether HLE contracts are being violated
    
    prosper::boot::record_boot_phase_with_diagnostics(
        prosper::diagnostics::BootPhase::HLESetup,
        "HLE layer initialized"
    );
    
    // ========================================
    // KERNEL INITIALIZATION
    // ========================================
    
    // ... initialize kernel simulation ...
    
    prosper::boot::record_boot_phase_with_diagnostics(
        prosper::diagnostics::BootPhase::KernelInit,
        "Kernel subsystem ready"
    );
    
    // ========================================
    // GPU INITIALIZATION
    // ========================================
    
    // ... initialize GPU ...
    
    prosper::boot::record_boot_phase_with_diagnostics(
        prosper::diagnostics::BootPhase::GpuInit,
        "GPU subsystem initialized"
    );
    
    // ========================================
    // READY STATE
    // ========================================
    
    prosper::boot::record_boot_phase_with_diagnostics(
        prosper::diagnostics::BootPhase::Ready,
        "System fully initialized and ready"
    );
    
    // ========================================
    // MAIN LOOP
    // ========================================
    
    // ... run emulation loop ...
    
    // ========================================
    // SHUTDOWN
    // ========================================
    
    prosper::boot::shutdown_diagnostics();
    
    return 0;
}
*/
