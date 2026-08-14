/**
 * Diagnostics Infrastructure for Prosper/SharpEmuT24
 * 
 * Core diagnostics types and interfaces.
 * Compatible with PR #2495/#2496 architecture.
 * 
 * DESIGN PRINCIPLES:
 * - Observer only: No runtime behavior modifications
 * - Zero-cost when disabled: Stub implementations compile to nothing
 * - API identical in both modes: Same signatures, same headers
 * - Upstream compatible: Matches existing EventBus/DiagnosticContext patterns
 * 
 * @version 2.0.0
 * @license MIT
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <chrono>
#include <memory>
#include <cstdint>
#include <atomic>
#include <sstream>
#include <algorithm>

namespace prosper {
namespace diagnostics {

// ============================================================================
// Forward Declarations (always available)
// ============================================================================

class EventBus;
class DiagnosticContext;
class PluginRegistry;

// ============================================================================
// Version & Metadata
// ============================================================================

constexpr const char* DIAGNOSTICS_VERSION = "2.0.0";
constexpr const char* DIAGNOSTICS_API_LEVEL = "2024.1";

// ============================================================================
// Severity Levels
// ============================================================================

enum class Severity : uint8_t {
    Debug = 0,
    Info,
    Warning,
    Error,
    Critical,
    Fatal
};

inline const char* severityToString(Severity s) {
    switch (s) {
        case Severity::Debug:    return "DEBUG";
        case Severity::Info:     return "INFO";
        case Severity::Warning:  return "WARNING";
        case Severity::Error:    return "ERROR";
        case Severity::Critical: return "CRITICAL";
        case Severity::Fatal:    return "FATAL";
    }
    return "UNKNOWN";
}

// ============================================================================
// Source Location (always available, zero overhead)
// ============================================================================

struct SourceLocation {
    const char* file{nullptr};
    int line{0};
    const char* function{nullptr};
    
    SourceLocation() = default;
    SourceLocation(const char* f, int l, const char* fn = nullptr)
        : file(f), line(l), function(fn) {}
    
    std::string toString() const {
        std::string result;
        if (file) result += file;
        result += ":";
        result += std::to_string(line);
        if (function) {
            result += " in ";
            result += function;
        }
        return result;
    }
};

// ============================================================================
// Diagnostic Event (core data structure)
// ============================================================================

struct DiagnosticEvent {
    std::string id;
    std::string category;
    std::string message;
    Severity severity{Severity::Info};
    SourceLocation location;
    
    // Timestamp
    std::chrono::system_clock::time_point timestamp;
    uint64_t frame_number{0};
    
    // Context data (key-value pairs for structured logging)
    std::map<std::string, std::string> context;
    
    // Correlation
    std::string correlation_id;
    std::string parent_event_id;
    
    static std::string generateId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "evt_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    DiagnosticEvent() 
        : id(generateId())
        , timestamp(std::chrono::system_clock::now()) {}
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"id\": \"" << id << "\",\n";
        json << "  \"category\": \"" << category << "\",\n";
        json << "  \"message\": \"" << message << "\",\n";
        json << "  \"severity\": \"" << severityToString(severity) << "\",\n";
        if (!location.toString().empty()) {
            json << "  \"location\": \"" << location.toString() << "\",\n";
        }
        json << "  \"frame\": " << frame_number << "\n";
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Plugin Info (MUST be available outside #ifdef PROSPER_DIAGNOSTICS)
// This is the key fix for PR #2513 API Contract Issue
// ============================================================================

/**
 * Plugin metadata structure.
 * 
 * CRITICAL: This struct MUST be defined outside any #ifdef guard.
 * Both enabled and disabled builds must see this exact definition.
 * 
 * Usage example (must compile in BOTH modes):
 * @code
 *   PluginInfo info{
 *       "boot_state",
 *       "1.0",
 *       "Boot phase diagnostics"
 *   };
 *   plugin_registry().register_plugin(info);
 * @endcode
 */
struct PluginInfo {
    std::string name;           ///< Unique plugin identifier
    std::string version;        ///< Semantic version string
    std::string description;    ///< Human-readable description
    std::string author{"Prosper Team"};  ///< Plugin author/maintainer
    
    // Optional metadata
    std::vector<std::string> dependencies;  ///< Required plugins
    std::map<std::string, std::string> config;  ///< Configuration options
    
    // Lifecycle callbacks (only used when PROSPER_DIAGNOSTICS enabled)
#ifdef PROSPER_DIAGNOSTICS
    std::function<bool()> on_initialize;
    std::function<void()> on_shutdown;
    std::function<bool()> on_health_check;
#endif
    
    PluginInfo() = default;
    
    PluginInfo(const std::string& n, const std::string& v, const std::string& d)
        : name(n), version(v), description(d) {}
    
    bool isValid() const {
        return !name.empty() && !version.empty();
    }
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"name\": \"" << name << "\",\n";
        json << "  \"version\": \"" << version << "\",\n";
        json << "  \"description\": \"" << description << "\"\n";
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Plugin State (for tracking registered plugins)
// ============================================================================

enum class PluginState : uint8_t {
    Unregistered = 0,
    Registered,
    Initialized,
    Active,
    Suspended,
    Error,
    Shutdown
};

inline const char* pluginStateToString(PluginState s) {
    switch (s) {
        case PluginState::Unregistered: return "Unregistered";
        case PluginState::Registered:   return "Registered";
        case PluginState::Initialized:  return "Initialized";
        case PluginState::Active:       return "Active";
        case PluginState::Suspended:    return "Suspended";
        case PluginState::Error:        return "Error";
        case PluginState::Shutdown:     return "Shutdown";
    }
    return "Unknown";
}

struct PluginInstance {
    PluginInfo info;
    PluginState state{PluginState::Registered};
    std::chrono::system_clock::time_point registration_time;
    size_t event_count{0};
    std::string last_error;
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"info\": " << info.toJson() << ",\n";
        json << "  \"state\": \"" << pluginStateToString(state) << "\",\n";
        json << "  \"event_count\": " << event_count << "\n";
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Boot Phase Tracking (preserves existing record_boot_phase interface)
// ============================================================================

enum class BootPhase : uint8_t {
    None = 0,
    Initialization,
    ConfigLoading,
    ModuleLoading,
    HLESetup,
    KernelInit,
    GpuInit,
    AudioInit,
    InputInit,
    FileSystemInit,
    NetworkInit,
    ApplicationStart,
    Ready,
    Shutdown,
    Error
};

inline const char* bootPhaseToString(BootPhase phase) {
    switch (phase) {
        case BootPhase::None:             return "None";
        case BootPhase::Initialization:   return "Initialization";
        case BootPhase::ConfigLoading:    return "ConfigLoading";
        case BootPhase::ModuleLoading:    return "ModuleLoading";
        case BootPhase::HLESetup:         return "HLESetup";
        case BootPhase::KernelInit:       return "KernelInit";
        case BootPhase::GpuInit:          return "GpuInit";
        case BootPhase::AudioInit:        return "AudioInit";
        case BootPhase::InputInit:        return "InputInit";
        case BootPhase::FileSystemInit:   return "FileSystemInit";
        case BootPhase::NetworkInit:      return "NetworkInit";
        case BootPhase::ApplicationStart: return "ApplicationStart";
        case BootPhase::Ready:            return "Ready";
        case BootPhase::Shutdown:         return "Shutdown";
        case BootPhase::Error:            return "Error";
    }
    return "Unknown";
}

struct BootPhaseRecord {
    BootPhase phase;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::microseconds duration{0};
    bool success{true};
    std::string message;
    SourceLocation location;
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"phase\": \"" << bootPhaseToString(phase) << "\",\n";
        json << "  \"success\": " << (success ? "true" : "false") << ",\n";
        json << "  \"duration_us\": " << duration.count() << "\n";
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Event Callback Types
// ============================================================================

using EventCallback = std::function<void(const DiagnosticEvent&)>;
using FilterFunction = std::function<bool(const DiagnosticEvent&)>;

// ============================================================================
// Statistics
// ============================================================================

struct DiagnosticsStats {
    size_t total_events{0};
    size_t events_by_severity[6]{0};  // Index by Severity enum
    size_t plugins_registered{0};
    size_t plugins_active{0};
    size_t boot_phases_recorded{0};
    std::chrono::system_clock::time_point start_time;
    std::chrono::microseconds uptime{0};
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"total_events\": " << total_events << ",\n";
        json << "  \"plugins_registered\": " << plugins_registered << ",\n";
        json << "  \"plugins_active\": " << plugins_active << ",\n";
        json << "  \"boot_phases_recorded\": " << boot_phases_recorded << "\n";
        json << "}";
        return json.str();
    }
};

} // namespace diagnostics
} // namespace prosper

// ============================================================================
// Include Implementation (conditional on PROSPER_DIAGNOSTICS)
// ============================================================================

#ifdef PROSPER_DIAGNOSTICS
#include "diagnostics_impl.hpp"
#else
#include "diagnostics_stub.hpp"
#endif

// ============================================================================
// Convenience Macros (work in both modes)
// ============================================================================

#define PROSPER_DIAG_HERE() ::prosper::diagnostics::SourceLocation(__FILE__, __LINE__, __func__)

#define PROSPER_DIAG_EMIT(category, message, severity) \
    do { \
        if (::prosper::diagnostics::is_enabled()) { \
            ::prosper::diagnostics::emit_event( \
                (category), (message), (severity), \
                PROSPER_DIAG_HERE() \
            ); \
        } \
    } while(0)

#define PROSPER_RECORD_BOOT_PHASE(phase, ...) \
    do { \
        ::prosper::diagnostics::record_boot_phase( \
            (phase), PROSPER_DIAG_HERE(), ##__VA_ARGS__ \
        ); \
    } while(0)
