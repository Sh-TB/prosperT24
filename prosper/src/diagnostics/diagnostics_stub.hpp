/**
 * Diagnostics Stub Implementation (Disabled Build)
 * 
 * CRITICAL: This provides REAL API signatures that match the enabled build.
 * When PROSPER_DIAGNOSTICS is NOT defined, all functions compile to no-ops
 * or return safe default values.
 * 
 * API CONTRACT RULES:
 * 1. Same function signatures as diagnostics_impl.hpp
 * 2. No ellipsis arguments - exact parameter types
 * 3. No hidden missing types - PluginInfo is defined in diagnostics.hpp
 * 4. Return values must be safe defaults (false, 0, empty, etc.)
 * 5. Must compile without errors when included
 * 
 * This fixes PR #2513 issue where disabled build had different API.
 */

#pragma once

#include "diagnostics.hpp"
#include <mutex>

namespace prosper {
namespace diagnostics {

// ============================================================================
// Global State (stub)
// ============================================================================

/**
 * Check if diagnostics is enabled.
 * Always returns false in stub mode.
 */
inline bool is_enabled() {
    return false;
}

/**
 * Initialize diagnostics system.
 * No-op in stub mode.
 */
inline bool initialize() {
    return true;  // Success (nothing to do)
}

/**
 * Shutdown diagnostics system.
 * No-op in stub mode.
 */
inline void shutdown() {
    // No-op
}

// ============================================================================
// Event Emission (stub)
// ============================================================================

/**
 * Emit a diagnostic event.
 * No-op in stub mode.
 */
inline void emit_event(
    const std::string& category,
    const std::string& message,
    Severity severity,
    const SourceLocation& location = SourceLocation())
{
    // Event dropped - diagnostics disabled
    // In a debug build, could optionally log to stderr
#ifdef PROSPER_DIAG_STUB_VERBOSE
    std::cerr << "[DIAG-STUB] " << severityToString(severity) 
              << " [" << category << "] " << message 
              << " @ " << location.toString() << "\n";
#endif
}

// ============================================================================
// PluginRegistry Stub (REAL SIGNATURES - PR #2513 FIX)
// ============================================================================

/**
 * PluginRegistry stub class.
 * 
 * CRITICAL: All methods have EXACT same signatures as enabled implementation.
 * This allows code to compile unconditionally with PluginInfo.
 * 
 * Example usage (compiles in BOTH modes):
 * @code
 *   PluginInfo info{"boot_state", "1.0", "Boot phase diagnostics"};
 *   bool success = plugin_registry().register_plugin(info);
 *   assert(success == false);  // Always false in stub mode
 *   assert(plugin_registry().plugin_count() == 0);  // Always 0
 * @endcode
 */
class PluginRegistry {
public:
    /**
     * Get singleton instance.
     */
    static PluginRegistry& instance() {
        static PluginRegistry instance_;
        return instance_;
    }
    
    /**
     * Register a plugin.
     * 
     * REAL SIGNATURE: Takes const PluginInfo& (not ellipsis)
     * ALWAYS RETURNS: false (no plugins registered in stub mode)
     * 
     * This fixes PR #2513 where disabled build had different signature.
     */
    bool register_plugin(const PluginInfo& info) {
        // Stub: accept the call but don't register
        // Return false to indicate "not registered"
        (void)info;  // Suppress unused warning
        return false;
    }
    
    /**
     * Unregister a plugin by name.
     * Returns false (plugin not found/registered).
     */
    bool unregister_plugin(const std::string& name) {
        (void)name;
        return false;
    }
    
    /**
     * Get plugin by name.
     * Returns nullptr (no plugins exist in stub mode).
     */
    PluginInstance* get_plugin(const std::string& name) {
        (void)name;
        return nullptr;
    }
    
    const PluginInstance* get_plugin(const std::string& name) const {
        (void)name;
        return nullptr;
    }
    
    /**
     * Check if plugin is registered.
     * Returns false (no plugins in stub mode).
     */
    bool has_plugin(const std::string& name) const {
        (void)name;
        return false;
    }
    
    /**
     * Get number of registered plugins.
     * Returns 0 (always empty in stub mode).
     */
    size_t plugin_count() const {
        return 0;
    }
    
    /**
     * Get all registered plugins.
     * Returns empty vector.
     */
    std::vector<PluginInstance> get_all_plugins() const {
        return {};
    }
    
    /**
     * Initialize all registered plugins.
     * No-op in stub mode.
     */
    bool initialize_all() {
        return true;  // Success (nothing to initialize)
    }
    
    /**
     * Shutdown all plugins (reverse order).
     * No-op in stub mode.
     */
    void shutdown_all() {
        // No-op
    }
    
    /**
     * Export registry state as JSON.
     * Returns empty object.
     */
    std::string export_json() const {
        return "{\"plugins\": [], \"count\": 0}";
    }
    
private:
    PluginRegistry() = default;
    ~PluginRegistry() = default;
    PluginRegistry(const PluginRegistry&) = delete;
    PluginRegistry& operator=(const PluginRegistry&) = delete;
};

/**
 * Convenience function to get global PluginRegistry instance.
 * Works in both enabled and disabled builds.
 */
inline PluginRegistry& plugin_registry() {
    return PluginRegistry::instance();
}

// ============================================================================
// EventBus Stub
// ============================================================================

class EventBus {
public:
    static EventBus& instance() {
        static EventBus instance_;
        return instance_;
    }
    
    void publish(const DiagnosticEvent& event) {
        (void)event;  // Dropped
    }
    
    void subscribe(const std::string& category, EventCallback callback) {
        (void)category;
        (void)callback;  // Ignored
    }
    
    void unsubscribe(const std::string& category, EventCallback callback) {
        (void)category;
        (void)callback;  // Ignored
    }
    
    size_t subscriber_count(const std::string& category) const {
        (void)category;
        return 0;
    }
    
private:
    EventBus() = default;
};

inline EventBus& event_bus() {
    return EventBus::instance();
}

// ============================================================================
// DiagnosticContext Stub
// ============================================================================

class DiagnosticContext {
public:
    explicit DiagnosticContext(const std::string& name)
        : context_name(name) {}
    
    ~DiagnosticContext() = default;
    
    void set_correlation_id(const std::string& id) {
        (void)id;
    }
    
    std::string get_correlation_id() const {
        return "";
    }
    
    void push_scope(const std::string& scope) {
        (void)scope;
    }
    
    void pop_scope() {
        // No-op
    }
    
    std::string get_current_scope() const {
        return "";
    }
    
private:
    std::string context_name;
};

// ============================================================================
// Boot Phase Recording (preserves existing interface)
// ============================================================================

/**
 * Record a boot phase transition.
 * No-op in stub mode but preserves call site compatibility.
 * 
 * Existing record_boot_phase() calls continue to work without modification.
 */
inline void record_boot_phase(
    BootPhase phase,
    const SourceLocation& location = SourceLocation(),
    const std::string& message = "")
{
    (void)phase;
    (void)location;
    (void)message;
    // Phase recording dropped - diagnostics disabled
    
#ifdef PROSPER_DIAG_STUB_VERBOSE
    std::cerr << "[DIAG-STUB] Boot phase: " << bootPhaseToString(phase);
    if (!message.empty()) {
        std::cerr << " - " << message;
    }
    std::cerr << "\n";
#endif
}

/**
 * Get current boot phase.
 * Returns BootPhase::None (not tracking in stub mode).
 */
inline BootPhase get_current_boot_phase() {
    return BootPhase::None;
}

/**
 * Get boot phase history.
 * Returns empty vector.
 */
inline std::vector<BootPhaseRecord> get_boot_phase_history() {
    return {};
}

/**
 * Check if boot completed successfully.
 * Returns true (assume success when not tracking).
 */
inline bool boot_completed_successfully() {
    return true;
}

// ============================================================================
// Statistics (stub values)
// ============================================================================

inline DiagnosticsStats get_stats() {
    return DiagnosticsStats{};  // All zeros
}

inline void reset_stats() {
    // No-op
}

// ============================================================================
// Query Interface (stub - returns empty)
// ============================================================================

inline std::vector<DiagnosticEvent> query_events(
    FilterFunction filter = nullptr,
    size_t limit = 0)
{
    (void)filter;
    (void)limit;
    return {};
}

inline std::vector<DiagnosticEvent> query_events_by_category(
    const std::string& category,
    size_t limit = 0)
{
    (void)category;
    (void)limit;
    return {};
}

inline std::vector<DiagnosticEvent> query_events_by_severity(
    Severity min_severity,
    size_t limit = 0)
{
    (void)min_severity;
    (void)limit;
    return {};
}

// ============================================================================
// Export (stub - returns empty)
// ============================================================================

inline std::string export_events_json(size_t limit = 0) {
    (void)limit;
    return "{\"events\": [], \"count\": 0}";
}

inline std::string export_boot_phases_json() {
    return "{\"phases\": [], \"count\": 0}";
}

inline std::string export_full_report_json() {
    return "{\"diagnostics\": {\"enabled\": false}, \"events\": [], \"boot_phases\": []}";
}

} // namespace diagnostics
} // namespace prosper
