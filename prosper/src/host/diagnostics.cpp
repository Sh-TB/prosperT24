// diagnostics.cpp — Diagnostics Plugin Registry Implementation
//
// Implements both enabled (PROSPER_DIAGNOSTICS) and disabled branches.
// The disabled branch is a no-op stub that compiles away to nothing.
#include "host/diagnostics.hpp"

#ifdef PROSPER_DIAGNOSTICS

namespace prosper {

bool PluginRegistry::register_plugin(const PluginInfo& info) {
    // Reject invalid inputs
    if (!info.name || !info.name[0]) return false;
    
    // Check for duplicate registration
    for (const auto& p : plugins_) {
        if (p.name && std::string(p.name) == info.name) {
            return false;  // Already registered
        }
    }
    
    plugins_.push_back(info);
    return true;
}

size_t PluginRegistry::plugin_count() const {
    return plugins_.size();
}

bool PluginRegistry::has_plugin(const std::string& name) const {
    for (const auto& p : plugins_) {
        if (p.name && std::string(p.name) == name) {
            return true;
        }
    }
    return false;
}

PluginRegistry& PluginRegistry::instance() {
    static PluginRegistry registry;
    return registry;
}

} // namespace prosper

#else  // !PROSPER_DIAGNOSTICS — disabled stub implementations

namespace prosper {

PluginRegistry& PluginRegistry::instance() {
    static PluginRegistry registry;
    return registry;
}

} // namespace prosper

#endif  // PROSPER_DIAGNOSTICS
