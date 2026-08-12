#pragma once

/**
 * @file plugin_registry.hpp
 * @brief Plugin Registry for Diagnostics Framework - Manages plugin lifecycle
 */

#include "diagnostic_interface.hpp"
#include "event_bus.hpp"
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace prosper {
namespace diagnostics {

//=============================================================================
// Plugin Registry Implementation
//=============================================================================

class PluginRegistry {
public:
    static PluginRegistry& instance();
    
    // Singleton management
    PluginRegistry(const PluginRegistry&) = delete;
    PluginRegistry& operator=(const PluginRegistry&) = delete;
    
    // Registration
    bool register_plugin(PluginInfo info);
    bool unregister_plugin(const std::string& name);
    
    // Lifecycle management
    bool initialize_all(const DiagnosticsConfig& config);
    bool shutdown_all();
    bool reset_all();
    
    // Individual plugin control
    bool enable_plugin(const std::string& name, bool enabled = true);
    bool is_plugin_enabled(const std::string& name) const;
    bool is_plugin_loaded(const std::string& name) const;
    
    // Access
    DiagnosticPlugin* get_plugin(const std::string& name) const;
    std::vector<std::string> list_plugins() const;
    std::vector<std::string> list_enabled_plugins() const;
    std::vector<PluginInfo> get_all_plugin_info() const;
    
    // Statistics
    size_t total_plugins() const { return plugins_.size(); }
    size_t enabled_plugins_count() const;
    PluginStatistics get_statistics(const std::string& name) const;
    std::vector<PluginStatistics> get_all_statistics() const;
    
    // Bulk operations
    std::string generate_combined_report() const;
    bool export_all_reports(const std::string& directory) const;
    
    // Validation
    std::vector<std::string> validate_plugins() const;  // Returns validation errors
    
private:
    PluginRegistry();
    ~PluginRegistry();
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, PluginInfo> registered_plugins_;
    std::unordered_map<std::string, std::unique_ptr<DiagnosticPlugin>> active_plugins_;
    std::unordered_map<std::string, bool> plugin_enabled_state_;
};

//=============================================================================
// Auto-Registration Helper (for static registration)
//=============================================================================

class PluginRegistrar {
public:
    PluginRegistrar(const std::string& name, 
                   const std::string& description,
                   PluginFactory factory,
                   bool default_enabled = true,
                   int priority = 100) {
        PluginInfo info(name, description, factory, default_enabled, priority);
        // Registry may not exist yet during static init - defer registration
        pending_registrations_.push_back(std::move(info));
    }
    
    static void flush_pending_registrations();
    
private:
    static std::vector<PluginInfo> pending_registrations_;
};

//=============================================================================
// Macro for easy plugin registration
//=============================================================================

#define REGISTER_DIAGNOSTIC_PLUGIN(class_name, plugin_name, description, ...) \
    static prosper::diagnostics::PluginRegistrar _reg_##class_name( \
        plugin_name, \
        description, \
        []() -> std::unique_ptr<prosper::diagnostics::DiagnosticPlugin> { \
            return std::make_unique<class_name>(); \
        }, \
        __VA_ARGS__ \
    )

} // namespace diagnostics
} // namespace prosper
