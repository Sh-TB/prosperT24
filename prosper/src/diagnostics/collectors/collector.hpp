// diagnostics/collectors/collector.hpp — Base collector interface
//
// All diagnostic collectors inherit from this interface. Collectors observe
// (but do not modify) runtime behavior and emit structured events.
#pragma once

#include "../core/types.hpp"
#include "../core/event_bus.hpp"
#include <string>
#include <memory>

namespace prosper {
namespace diagnostics {

class Collector {
public:
    virtual ~Collector();
    
    // --- Lifecycle -----------------------------------------------------------
    
    // Initialize the collector. Called when diagnostics system starts.
    // Return false if initialization fails (collector will be disabled).
    virtual bool initialize() { return true; }
    
    // Shutdown the collector. Flush any pending data.
    virtual void shutdown() {}
    
    // --- Reporting -----------------------------------------------------------
    
    // Generate collector-specific report as JSON string.
    // Called at session end to write subsystem-specific output files.
    virtual std::string generate_report() const { return "{}"; }
    
    // Collector name (used for file naming and identification)
    virtual const char* name() const = 0;
    
    // Subsystem this collector monitors
    virtual Subsystem subsystem() const = 0;
    
protected:
    // Helper to emit events with this collector's context
    void emit_event(
        const std::string& type,
        Severity severity,
        const std::string& message,
        const SourceLocation& source = {},
        const std::function<void(DiagnosticEvent&)>& enrich = nullptr
    ) {
        DiagnosticEvent event;
        event.type = type;
        event.severity = severity;
        event.subsystem = subsystem();
        event.message = message;
        event.source = source;
        
        if (enrich) enrich(event);
        
        EventBus::instance().publish(std::move(event));
    }
};

// RAII helper for automatic collector registration
template<typename T>
std::unique_ptr<T> make_collector() {
    auto col = std::make_unique<T>();
    if (col->initialize()) {
        return col;
    }
    return nullptr;
}

} // namespace diagnostics
} // namespace prosper
