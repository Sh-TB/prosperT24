// diagnostics.hpp — Diagnostics Plugin Registry API Contract
//
// PUBLIC API CONTRACT (upstream-reviewed):
//   - PluginInfo is ALWAYS visible (outside #ifdef) so both branches compile
//   - register_plugin(const PluginInfo&) has IDENTICAL signature in both branches
//   - Disabled stub returns false; enabled implementation returns true on success
//   - No ellipsis usage, no signature mismatch
//
// Usage:
//   #include "host/diagnostics.hpp"
//   prosper::PluginInfo info{ "name", "1.0", "description" };
//   bool ok = prosper::plugin_registry().register_plugin(info);
#pragma once

#include <string>
#include <vector>

namespace prosper {

// --- PluginInfo: ALWAYS visible (outside #ifdef) ---
// Both enabled and disabled branches MUST see this definition.
// This fixes the upstream review issue where disabled-mode code could not
// construct PluginInfo objects.
struct PluginInfo {
    const char* name;        // Short identifier (e.g., "boot_state", "crash_context")
    const char* version;     // Semantic version (e.g., "1.0", "2.1.3")
    const char* description; // Human-readable purpose (one line)
};

#ifdef PROSPER_DIAGNOSTICS

// --- ENABLED BRANCH: Full diagnostics registry ---

class PluginRegistry {
public:
    // Register a diagnostic plugin. Returns true if registered successfully.
    // Contract: Same signature as disabled stub (const PluginInfo&, no ellipsis).
    bool register_plugin(const PluginInfo& info);

    // Query current plugin count
    size_t plugin_count() const;

    // Check if a named plugin is registered
    bool has_plugin(const std::string& name) const;

    // Singleton access
    static PluginRegistry& instance();
private:
    PluginRegistry() = default;
    std::vector<PluginInfo> plugins_;
};

// Convenience accessor for the singleton
inline PluginRegistry& plugin_registry() {
    return PluginRegistry::instance();
}

#else  // !PROSPER_DIAGNOSTICS

// --- DISABLED BRANCH: Stub with MATCHING signature ---
//
// CRITICAL: This stub MUST have the EXACT same signature as the enabled branch:
//   bool register_plugin(const PluginInfo&)  ← takes const ref, returns bool
//
// Previous defect: disabled branch used ellipsis (...) or different parameter type,
// causing compile errors when caller code constructed PluginInfo and passed it.
//
// Fixed: Stub now accepts const PluginInfo& and returns false (registration denied).

class PluginRegistry {
public:
    // Stub: Always returns false. Signature matches enabled branch exactly.
    bool register_plugin(const PluginInfo& /*info*/) {
        return false;  // Diagnostics disabled; registration rejected
    }

    // Stub: No plugins in disabled mode
    size_t plugin_count() const {
        return 0;
    }

    // Stub: No plugins exist
    bool has_plugin(const std::string& /*name*/) const {
        return false;
    }

    static PluginRegistry& instance();
};

inline PluginRegistry& plugin_registry() {
    return PluginRegistry::instance();
}

#endif  // PROSPER_DIAGNOSTICS

} // namespace prosper
