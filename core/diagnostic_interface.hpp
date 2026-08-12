#pragma once

/**
 * @file diagnostic_interface.hpp
 * @brief Core Diagnostic Interface for Prosper PS4 Emulator Diagnostics Framework
 * 
 * Phase 11 Enhanced - Evidence-Driven Diagnostics System
 * 
 * Design Principles:
 * - Observation-only: No modification to emulator runtime behavior
 * - Zero-overhead when disabled: All diagnostics behind feature flags
 * - Thread-safe: All operations safe from emulator threads
 * - Minimal dependencies: Self-contained where possible
 */

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <chrono>
#include <any>
#include <mutex>
#include <unordered_map>
#include <atomic>

namespace prosper {
namespace diagnostics {

//=============================================================================
// Forward Declarations
//=============================================================================

class EventBus;
class PluginRegistry;
struct DiagnosticEvent;

//=============================================================================
// Enums & Constants
//=============================================================================

enum class Severity : uint8_t {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

enum class BootState : uint8_t {
    POWER_ON = 0,
    ELF_LOADED = 1,
    PRX_LOADED = 2,
    SEGMENTS_MAPPED = 3,
    RELOCATIONS_APPLIED = 4,
    IMPORTS_RESOLVED = 5,
    RUNTIME_INITIALIZED = 6,
    THREAD_STARTED = 7,
    MAIN_ENTRY = 8,
    FIRST_RENDER = 9,
    BOOT_COMPLETE = 10,
    CRASHED = 255,
    UNKNOWN = 254
};

enum class RelocationType : uint32_t {
    R_X86_64_NONE = 0,
    R_X86_64_64 = 1,
    R_X86_64_PC32 = 2,
    R_X86_64_GOT32REL = 3,
    R_X86_64_PLT32 = 4,
    R_X86_64_RELATIVE = 8,
    R_X86_64_GOTPCREL = 9,
    R_X86_64_GOTPCRELX = 41,
    R_X86_64_REX_GOTPCRELX = 42
};

enum class ImportStatus : uint8_t {
    MISSING_NOT_CALLED = 0,      // Import not implemented, never called (low risk)
    MISSING_CALLED = 1,          // Import not implemented, but called (HIGH RISK)
    IMPLEMENTED_BUT_FAILED = 2,  // Implementation exists but returned error
    CALLED_SUCCESSFULLY = 3,     // Working correctly
    STUB_IMPLEMENTED = 4         // Stub/placeholder implementation
};

enum class MemoryProtection : uint8_t {
    NONE = 0,
    READ = 1 << 0,
    WRITE = 1 << 1,
    EXECUTE = 1 << 2,
    RW = READ | WRITE,
    RX = READ | EXECUTE,
    RWX = READ | WRITE | EXECUTE
};

enum class ViolationType : uint8_t {
    WRITE_VIOLATION,
    EXECUTE_VIOLATION,
    READ_VIOLATION,
    ALIGNMENT_VIOLATION,
    BOUNDARY_VIOLATION,
    PERMISSION_VIOLATION
};

inline MemoryProtection operator|(MemoryProtection a, MemoryProtection b) {
    return static_cast<MemoryProtection>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline bool has_flag(MemoryProtection value, MemoryProtection flag) {
    return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
}

//=============================================================================
// Timestamp Utility
//=============================================================================

using Timestamp = std::chrono::high_resolution_clock::time_point;
using Duration = std::chrono::microseconds;

inline Timestamp now() {
    return std::chrono::high_resolution_clock::now();
}

inline double timestamp_to_ms(Timestamp ts) {
    auto epoch = ts.time_since_epoch();
    return std::chrono::duration<double, std::milli>(epoch).count();
}

inline Timestamp from_ms(double ms) {
    return Timestamp(std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(
        std::chrono::milliseconds(static_cast<int64_t>(ms))));
}

//=============================================================================
// Diagnostic Event Structure
//=============================================================================

struct DiagnosticEvent {
    std::string source_plugin;
    std::string event_type;
    Severity severity;
    Timestamp timestamp;
    
    // Core data fields
    std::string message;
    std::string category;
    
    // Structured data for machine processing
    std::unordered_map<std::string, std::string> metadata;
    std::unordered_map<std::string, int64_t> numeric_data;
    std::unordered_map<std::string, double> float_data;
    
    // Optional context
    std::any context_data;
    std::string stack_trace;
    
    // Correlation
    std::string correlation_id;
    std::vector<std::string> related_event_ids;
    
    // Serialization helper
    std::string to_json() const;
    static DiagnosticEvent from_json(const std::string& json);
    
    // Generate unique ID
    static std::string generate_event_id();
    
    std::string event_id;  // Unique identifier
    
    DiagnosticEvent() 
        : severity(Severity::INFO), timestamp(now()) {
        event_id = generate_event_id();
    }
};

//=============================================================================
// Plugin Interface
//=============================================================================

class DiagnosticPlugin {
public:
    virtual ~DiagnosticPlugin() = default;
    
    // Identity
    virtual std::string name() const = 0;
    virtual std::string version() const { return "1.0.0"; }
    virtual std::string description() const { return ""; }
    
    // Lifecycle
    virtual bool initialize() { return true; }
    virtual void shutdown() {}
    virtual void reset() {}
    
    // Event handling
    virtual void on_event(const DiagnosticEvent& event) { (void)event; }
    
    // Reporting
    virtual std::string generate_report() const { return "{}"; }
    virtual void export_json(const std::string& path) const { (void)path; }
    
    // State queries
    virtual bool is_active() const { return active_; }
    virtual size_t event_count() const { return event_count_; }
    
    // Configuration
    virtual void configure(const std::unordered_map<std::string, std::string>& config) {
        config_ = config;
    }
    
protected:
    bool active_{false};
    size_t event_count_{0};
    std::unordered_map<std::string, std::string> config_;
    mutable std::mutex mutex_;
    
    void emit_event(DiagnosticEvent& event);
};

//=============================================================================
// Plugin Registration Helper
//=============================================================================

using PluginFactory = std::function<std::unique_ptr<DiagnosticPlugin>()>;

struct PluginInfo {
    std::string name;
    std::string description;
    PluginFactory factory;
    bool enabled_by_default;
    int priority;  // Lower = higher priority
    
    PluginInfo(const std::string& n, const std::string& d, PluginFactory f, 
               bool enabled = true, int p = 100)
        : name(n), description(d), factory(f), enabled_by_default(enabled), priority(p) {}
};

//=============================================================================
// Configuration Structures
//=============================================================================

struct DiagnosticsConfig {
    // Global settings
    bool enabled{true};
    bool verbose{false};
    bool json_output{true};
    
    // Performance limits
    size_t max_events_per_plugin{10000};  // Prevent unbounded memory growth
    size_t max_memory_mb{256};
    
    // Output paths
    std::string output_directory{"./diagnostics"};
    std::string crash_snapshot_path{"./diagnostics/crash_snapshots"};
    
    // Feature flags
    bool boot_state_machine_enabled{true};
    bool relocation_diagnostics_enabled{true};
    bool crash_replay_enabled{true};
    bool import_evidence_enabled{true};
    bool memory_validation_enabled{false};  // Expensive, off by default
    bool event_correlation_enabled{true};
    bool deterministic_mode{false};
    
    // Deterministic mode
    bool record_mode{false};
    bool replay_mode{false};
    std::string recording_path{"./diagnostics/recording.json"};
    
    // Performance targets
    double max_overhead_percent{5.0};
    size_t event_buffer_size{500};
    
    static DiagnosticsConfig production() {
        DiagnosticsConfig cfg;
        cfg.verbose = false;
        cfg.memory_validation_enabled = false;
        cfg.event_correlation_enabled = false;
        return cfg;
    }
    
    static DiagnosticsConfig debugging() {
        DiagnosticsConfig cfg;
        cfg.verbose = true;
        cfg.memory_validation_enabled = true;
        cfg.event_correlation_enabled = true;
        cfg.max_events_per_plugin = 50000;
        return cfg;
    }
    
    static DiagnosticsConfig minimal() {
        DiagnosticsConfig cfg;
        cfg.boot_state_machine_enabled = true;
        cfg.relocation_diagnostics_enabled = true;
        cfg.crash_replay_enabled = true;
        cfg.import_evidence_enabled = false;
        cfg.memory_validation_enabled = false;
        cfg.event_correlation_enabled = false;
        return cfg;
    }
};

//=============================================================================
// Statistics Tracking
//=============================================================================

struct PluginStatistics {
    std::string plugin_name;
    size_t events_processed{0};
    size_t events_emitted{0};
    size_t errors{0};
    double cpu_time_us{0.0};
    size_t peak_memory_bytes{0};
    Timestamp last_activity;
    
    void record_processing(double time_us) {
        events_processed++;
        cpu_time_us += time_us;
        last_activity = now();
    }
    
    std::string to_json() const;
};

//=============================================================================
// Global State Access
//=============================================================================

namespace global {
    EventBus* get_event_bus();
    PluginRegistry* get_plugin_registry();
    DiagnosticsConfig* get_config();
    
    bool initialize(const DiagnosticsConfig& config = DiagnosticsConfig{});
    void shutdown();
    
    bool is_initialized();
}

} // namespace diagnostics
} // namespace prosper
