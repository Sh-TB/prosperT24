/**
 * Diagnostics Implementation (Enabled Build)
 * 
 * Full diagnostics functionality when PROSPER_DIAGNOSTICS is defined.
 * Provides:
 * - Event bus with subscription/filtering
 * - Plugin registry with lifecycle management
 * - Boot phase tracking with timing
 * - Statistics and export
 * - Thread-safe operations
 */

#pragma once

#include "diagnostics.hpp"
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <algorithm>
#include <thread>

namespace prosper {
namespace diagnostics {

// ============================================================================
// Forward Declarations
// ============================================================================

class EventBus;
class PluginRegistry;

inline EventBus& event_bus();
inline PluginRegistry& plugin_registry();

// ============================================================================
// Global State
// ============================================================================

namespace detail {

static std::atomic<bool> g_initialized{false};
static std::atomic<bool> g_enabled{true};
static BootPhase g_current_phase{BootPhase::None};
static std::mutex g_state_mutex;

} // namespace detail

// ============================================================================
// Basic State Functions (needed by EventBus::publish)
// ============================================================================

inline bool is_enabled() {
    return detail::g_enabled.load(std::memory_order_relaxed);
}

// ============================================================================
// EventBus Implementation (must be defined before functions that use it)
// ============================================================================

class EventBus {
public:
    static EventBus& instance() {
        static EventBus instance_;
        return instance_;
    }
    
    void publish(const DiagnosticEvent& event) {
        if (!is_enabled()) return;
        
        // Store event
        {
            std::lock_guard<std::mutex> lock(events_mutex_);
            events_.push_back(event);
            
            // Limit history size
            while (events_.size() > max_events_) {
                events_.pop_front();
            }
        }
        
        // Notify subscribers
        {
            std::shared_lock<std::shared_mutex> lock(subscribers_mutex_);
            
            auto range = subscribers_.equal_range(event.category);
            for (auto it = range.first; it != range.second; ++it) {
                try {
                    if (it->second) {
                        it->second(event);
                    }
                } catch (...) {
                    // Subscriber error - don't propagate
                }
            }
            
            // Also notify wildcard subscribers
            auto wildcard_range = subscribers_.equal_range("*");
            for (auto it = wildcard_range.first; it != wildcard_range.second; ++it) {
                try {
                    if (it->second) {
                        it->second(event);
                    }
                } catch (...) {
                    // Subscriber error
                }
            }
        }
    }
    
    void subscribe(const std::string& category, EventCallback callback) {
        std::unique_lock<std::shared_mutex> lock(subscribers_mutex_);
        subscribers_.emplace(category, std::move(callback));
    }
    
    void unsubscribe(const std::string& category, EventCallback /*callback*/) {
        // Note: Can't easily compare std::function, so this clears all for category
        std::unique_lock<std::shared_mutex> lock(subscribers_mutex_);
        subscribers_.erase(category);
    }
    
    size_t subscriber_count(const std::string& category) const {
        std::shared_lock<std::shared_mutex> lock(subscribers_mutex_);
        return subscribers_.count(category);
    }
    
    /**
     * Get stored events (for querying).
     */
    std::deque<DiagnosticEvent> get_events() const {
        std::lock_guard<std::mutex> lock(events_mutex_);
        return events_;  // Copy
    }
    
private:
    mutable std::mutex events_mutex_;
    std::deque<DiagnosticEvent> events_;
    static constexpr size_t max_events_{10000};
    
    mutable std::shared_mutex subscribers_mutex_;
    std::unordered_multimap<std::string, EventCallback> subscribers_;
    
    EventBus() = default;
};

inline EventBus& event_bus() {
    return EventBus::instance();
}

// ============================================================================
// PluginRegistry Implementation (must be defined before functions that use it)
// ============================================================================

class PluginRegistry {
public:
    static PluginRegistry& instance() {
        static PluginRegistry instance_;
        return instance_;
    }
    
    /**
     * Register a plugin.
     * Returns true if registration succeeded, false if:
     * - Plugin name is empty (invalid PluginInfo)
     * - Plugin with same name already exists
     */
    bool register_plugin(const PluginInfo& info) {
        if (!info.isValid()) {
            return false;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check for duplicate
        if (plugins_.find(info.name) != plugins_.end()) {
            return false;  // Duplicate name
        }
        
        // Create instance
        PluginInstance instance;
        instance.info = info;
        instance.state = PluginState::Registered;
        instance.registration_time = std::chrono::system_clock::now();
        
        plugins_[info.name] = instance;
        
        // Emit registration event
        DiagnosticEvent event;
        event.category = "plugin";
        event.message = "Plugin registered: " + info.name;
        event.severity = Severity::Info;
        event.context["plugin_name"] = info.name;
        event.context["version"] = info.version;
        event_bus().publish(event);
        
        return true;
    }
    
    /**
     * Unregister a plugin by name.
     * Shuts down plugin first if active.
     */
    bool unregister_plugin(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = plugins_.find(name);
        if (it == plugins_.end()) {
            return false;
        }
        
        // Call shutdown callback if exists
#ifdef PROSPER_DIAGNOSTICS
        if (it->second.info.on_shutdown) {
            try {
                it->second.info.on_shutdown();
            } catch (...) {
                // Ignore shutdown errors during unregister
            }
        }
#endif
        
        plugins_.erase(it);
        
        DiagnosticEvent event;
        event.category = "plugin";
        event.message = "Plugin unregistered: " + name;
        event.severity = Severity::Info;
        event.context["plugin_name"] = name;
        event_bus().publish(event);
        
        return true;
    }
    
    /**
     * Get plugin by name (mutable).
     */
    PluginInstance* get_plugin(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = plugins_.find(name);
        if (it == plugins_.end()) {
            return nullptr;
        }
        return &it->second;
    }
    
    /**
     * Get plugin by name (const).
     */
    const PluginInstance* get_plugin(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = plugins_.find(name);
        if (it == plugins_.end()) {
            return nullptr;
        }
        return &it->second;
    }
    
    /**
     * Check if plugin exists.
     */
    bool has_plugin(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return plugins_.find(name) != plugins_.end();
    }
    
    /**
     * Get number of registered plugins.
     */
    size_t plugin_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return plugins_.size();
    }
    
    /**
     * Get all plugins (copy).
     */
    std::vector<PluginInstance> get_all_plugins() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<PluginInstance> result;
        result.reserve(plugins_.size());
        
        for (const auto& pair : plugins_) {
            result.push_back(pair.second);
        }
        
        return result;
    }
    
    /**
     * Initialize all registered plugins.
     * Calls on_initialize callback for each plugin.
     * Returns true if all initialized successfully.
     */
    bool initialize_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        bool all_success = true;
        
        for (auto& pair : plugins_) {
            auto& instance = pair.second;
            
            if (instance.state != PluginState::Registered) {
                continue;  // Already initialized or in other state
            }
            
#ifdef PROSPER_DIAGNOSTICS
            if (instance.info.on_initialize) {
                try {
                    bool success = instance.info.on_initialize();
                    if (success) {
                        instance.state = PluginState::Initialized;
                        instance.state = PluginState::Active;
                    } else {
                        instance.state = PluginState::Error;
                        instance.last_error = "Initialization callback returned false";
                        all_success = false;
                    }
                } catch (const std::exception& e) {
                    instance.state = PluginState::Error;
                    instance.last_error = e.what();
                    all_success = false;
                }
            } else {
                // No initializer, mark as active
                instance.state = PluginState::Active;
            }
#else
            instance.state = PluginState::Active;
#endif
        }
        
        return all_success;
    }
    
    /**
     * Shutdown all plugins in reverse registration order.
     */
    void shutdown_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Collect names in reverse order
        std::vector<std::string> names;
        names.reserve(plugins_.size());
        
        for (const auto& pair : plugins_) {
            names.push_back(pair.first);
        }
        std::reverse(names.begin(), names.end());
        
        // Shutdown each
        for (const auto& name : names) {
            auto it = plugins_.find(name);
            if (it != plugins_.end() && it->second.state == PluginState::Active) {
                
#ifdef PROSPER_DIAGNOSTICS
                if (it->second.info.on_shutdown) {
                    try {
                        it->second.info.on_shutdown();
                    } catch (...) {
                        // Ignore shutdown errors
                    }
                }
#endif
                
                it->second.state = PluginState::Shutdown;
            }
        }
    }
    
    /**
     * Export registry as JSON.
     */
    std::string export_json() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::stringstream json;
        json << "{\n";
        json << "  \"plugins\": [\n";
        
        bool first = true;
        for (const auto& pair : plugins_) {
            if (!first) json << ",\n";
            first = false;
            
            json << "    " << pair.second.toJson();
        }
        
        json << "\n  ],\n";
        json << "  \"count\": " << plugins_.size() << "\n";
        json << "}\n";
        
        return json.str();
    }
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, PluginInstance> plugins_;
    
    PluginRegistry() = default;
    ~PluginRegistry() = default;
    PluginRegistry(const PluginRegistry&) = delete;
    PluginRegistry& operator=(const PluginRegistry&) = delete;
};

inline PluginRegistry& plugin_registry() {
    return PluginRegistry::instance();
}

// ============================================================================
// Initialization/Shutdown Functions
// ============================================================================

inline bool initialize() {
    std::lock_guard<std::mutex> lock(detail::g_state_mutex);
    
    if (detail::g_initialized.load(std::memory_order_acquire)) {
        return true;  // Already initialized
    }
    
    detail::g_initialized.store(true, std::memory_order_release);
    detail::g_enabled.store(true, std::memory_order_release);
    
    // Emit initialization event
    DiagnosticEvent init_event;
    init_event.category = "system";
    init_event.message = "Diagnostics system initialized";
    init_event.severity = Severity::Info;
    event_bus().publish(init_event);
    
    return true;
}

inline void shutdown() {
    std::lock_guard<std::mutex> lock(detail::g_state_mutex);
    
    if (!detail::g_initialized.load(std::memory_order_acquire)) {
        return;  // Not initialized
    }
    
    // Shutdown plugins first
    plugin_registry().shutdown_all();
    
    // Emit shutdown event
    DiagnosticEvent shutdown_event;
    shutdown_event.category = "system";
    shutdown_event.message = "Diagnostics system shutting down";
    shutdown_event.severity = Severity::Info;
    event_bus().publish(shutdown_event);
    
    detail::g_enabled.store(false, std::memory_order_release);
    detail::g_initialized.store(false, std::memory_order_release);
}

// ============================================================================
// Event Emission
// ============================================================================

inline void emit_event(
    const std::string& category,
    const std::string& message,
    Severity severity,
    const SourceLocation& location)
{
    if (!is_enabled()) return;
    
    DiagnosticEvent event;
    event.category = category;
    event.message = message;
    event.severity = severity;
    event.location = location;
    
    event_bus().publish(event);
}

// ============================================================================
// DiagnosticContext Implementation
// ============================================================================

class DiagnosticContext {
public:
    explicit DiagnosticContext(const std::string& name)
        : context_name_(name)
        , correlation_id_(generate_correlation_id())
    {
        push_context(this);
    }
    
    ~DiagnosticContext() {
        pop_context(this);
    }
    
    void set_correlation_id(const std::string& id) {
        correlation_id_ = id;
    }
    
    std::string get_correlation_id() const {
        return correlation_id_;
    }
    
    void push_scope(const std::string& scope) {
        scopes_.push_back(scope);
    }
    
    void pop_scope() {
        if (!scopes_.empty()) {
            scopes_.pop_back();
        }
    }
    
    std::string get_current_scope() const {
        if (scopes_.empty()) return context_name_;
        
        std::string result = context_name_;
        for (const auto& scope : scopes_) {
            result += "/" + scope;
        }
        return result;
    }
    
private:
    std::string context_name_;
    std::string correlation_id_;
    std::vector<std::string> scopes_;
    
    static std::string generate_correlation_id() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "ctx_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    static void push_context(DiagnosticContext* ctx) {
        // Could track context stack globally if needed
        (void)ctx;
    }
    
    static void pop_context(DiagnosticContext* ctx) {
        (void)ctx;
    }
};

// ============================================================================
// Boot Phase Recording
// ============================================================================

namespace detail {

static std::vector<BootPhaseRecord> g_boot_phases;
static std::chrono::system_clock::time_point g_boot_start_time;
static std::mutex g_boot_mutex;

} // namespace detail

inline void record_boot_phase(
    BootPhase phase,
    const SourceLocation& location,
    const std::string& message)
{
    if (!is_enabled()) return;
    
    std::lock_guard<std::mutex> lock(detail::g_boot_mutex);
    
    BootPhaseRecord record;
    record.phase = phase;
    record.timestamp = std::chrono::system_clock::now();
    record.location = location;
    record.message = message;
    
    // Calculate duration from previous phase
    if (!detail::g_boot_phases.empty()) {
        auto prev_time = detail::g_boot_phases.back().timestamp;
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            record.timestamp - prev_time);
        detail::g_boot_phases.back().duration = duration;
    } else {
        // First phase - record start time
        detail::g_boot_start_time = record.timestamp;
    }
    
    // Handle special phases
    if (phase == BootPhase::Error) {
        record.success = false;
    } else if (phase == BootPhase::Ready) {
        record.success = true;
    }
    
    detail::g_current_phase = phase;
    detail::g_boot_phases.push_back(record);
    
    // Emit boot phase event
    DiagnosticEvent event;
    event.category = "boot";
    event.message = message.empty() ? bootPhaseToString(phase) : message;
    event.severity = (phase == BootPhase::Error) ? Severity::Error : Severity::Info;
    event.context["phase"] = bootPhaseToString(phase);
    event.context["phase_num"] = std::to_string(static_cast<int>(phase));
    event.location = location;
    event_bus().publish(event);
}

inline BootPhase get_current_boot_phase() {
    return detail::g_current_phase;
}

inline std::vector<BootPhaseRecord> get_boot_phase_history() {
    std::lock_guard<std::mutex> lock(detail::g_boot_mutex);
    return detail::g_boot_phases;  // Copy
}

inline bool boot_completed_successfully() {
    std::lock_guard<std::mutex> lock(detail::g_boot_mutex);
    
    if (detail::g_boot_phases.empty()) return false;
    
    const auto& last = detail::g_boot_phases.back();
    return last.phase == BootPhase::Ready && last.success;
}

// ============================================================================
// Statistics
// ============================================================================

inline DiagnosticsStats get_stats() {
    DiagnosticsStats stats;
    
    stats.start_time = detail::g_boot_start_time;
    
    if (auto now = std::chrono::system_clock::now(); now > stats.start_time) {
        stats.uptime = std::chrono::duration_cast<std::chrono::microseconds>(
            now - stats.start_time);
    }
    
    // Count events by severity
    auto events = event_bus().get_events();
    stats.total_events = events.size();
    
    for (const auto& evt : events) {
        int idx = static_cast<int>(evt.severity);
        if (idx >= 0 && idx < 6) {
            stats.events_by_severity[idx]++;
        }
    }
    
    // Plugin stats
    stats.plugins_registered = plugin_registry().plugin_count();
    
    // Boot phase stats
    {
        std::lock_guard<std::mutex> lock(detail::g_boot_mutex);
        stats.boot_phases_recorded = detail::g_boot_phases.size();
    }
    
    return stats;
}

inline void reset_stats() {
    std::lock_guard<std::mutex> lock(detail::g_boot_mutex);
    detail::g_boot_phases.clear();
    detail::g_current_phase = BootPhase::None;
}

// ============================================================================
// Query Interface
// ============================================================================

inline std::vector<DiagnosticEvent> query_events(
    FilterFunction filter,
    size_t limit)
{
    auto events = event_bus().get_events();
    
    std::vector<DiagnosticEvent> result;
    
    for (const auto& evt : events) {
        if (!filter || filter(evt)) {
            result.push_back(evt);
            if (limit > 0 && result.size() >= limit) {
                break;
            }
        }
    }
    
    return result;
}

inline std::vector<DiagnosticEvent> query_events_by_category(
    const std::string& category,
    size_t limit)
{
    return query_events(
        [category](const DiagnosticEvent& evt) {
            return evt.category == category;
        },
        limit
    );
}

inline std::vector<DiagnosticEvent> query_events_by_severity(
    Severity min_severity,
    size_t limit)
{
    return query_events(
        [min_severity](const DiagnosticEvent& evt) {
            return evt.severity >= min_severity;
        },
        limit
    );
}

// ============================================================================
// Export Functions
// ============================================================================

inline std::string export_events_json(size_t limit) {
    auto events = query_events(nullptr, limit);
    
    std::stringstream json;
    json << "{\n";
    json << "  \"events\": [\n";
    
    bool first = true;
    for (const auto& evt : events) {
        if (!first) json << ",\n";
        first = false;
        json << "    " << evt.toJson();
    }
    
    json << "\n  ],\n";
    json << "  \"count\": " << events.size() << "\n";
    json << "}\n";
    
    return json.str();
}

inline std::string export_boot_phases_json() {
    auto phases = get_boot_phase_history();
    
    std::stringstream json;
    json << "{\n";
    json << "  \"phases\": [\n";
    
    bool first = true;
    for (const auto& phase : phases) {
        if (!first) json << ",\n";
        first = false;
        json << "    " << phase.toJson();
    }
    
    json << "\n  ],\n";
    json << "  \"count\": " << phases.size() << ",\n";
    json << "  \"completed_successfully\": " << (boot_completed_successfully() ? "true" : "false") << "\n";
    json << "}\n";
    
    return json.str();
}

inline std::string export_full_report_json() {
    std::stringstream json;
    json << "{\n";
    json << "  \"diagnostics\": {\n";
    json << "    \"enabled\": " << (is_enabled() ? "true" : "false") << ",\n";
    json << "    \"version\": \"" << DIAGNOSTICS_VERSION << "\"\n";
    json << "  },\n";
    json << "  \"stats\": " << get_stats().toJson() << ",\n";
    json << "  \"plugins\": " << plugin_registry().export_json() << ",\n";
    
    // Events (last 100)
    json << "  \"recent_events\": ";
    {
        auto events = event_bus().get_events();
        size_t start = (events.size() > 100) ? events.size() - 100 : 0;
        
        json << "[\n";
        bool first = true;
        for (size_t i = start; i < events.size(); ++i) {
            if (!first) json << ",\n";
            first = false;
            json << "    " << events[i].toJson();
        }
        json << "\n  ]";
    }
    
    json << ",\n";
    json << "  \"boot_phases\": " << export_boot_phases_json() << "\n";
    json << "}\n";
    
    return json.str();
}

} // namespace diagnostics
} // namespace prosper
