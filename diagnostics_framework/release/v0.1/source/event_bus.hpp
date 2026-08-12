#pragma once

/**
 * @file event_bus.hpp
 * @brief Event Bus for Diagnostics Framework - Thread-safe pub/sub system
 */

#include "diagnostic_interface.hpp"
#include <queue>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <list>

namespace prosper {
namespace diagnostics {

//=============================================================================
// Event Subscription
//=============================================================================

using EventFilter = std::function<bool(const DiagnosticEvent&)>;
using EventHandler = std::function<void(const DiagnosticEvent&)>;

struct Subscription {
    std::string id;
    std::string subscriber_name;
    EventHandler handler;
    EventFilter filter;
    bool active{true};
    int priority{0};  // Higher priority handlers called first
    Timestamp created_at;
    
    Subscription(const std::string& name, EventHandler h, EventFilter f = nullptr, int prio = 0)
        : id(DiagnosticEvent::generate_event_id()), 
          subscriber_name(name), 
          handler(std::move(h)), 
          filter(std::move(f)),
          priority(prio),
          created_at(now()) {}
};

//=============================================================================
// Event Bus Implementation
//=============================================================================

class EventBus {
public:
    static EventBus& instance();
    
    // Singleton management
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    
    // Subscription management
    std::string subscribe(const std::string& subscriber_name, 
                         EventHandler handler,
                         EventFilter filter = nullptr,
                         int priority = 0);
    
    bool unsubscribe(const std::string& subscription_id);
    bool unsubscribe_all(const std::string& subscriber_name);
    
    // Publishing
    void publish(DiagnosticEvent event);
    void publish_sync(DiagnosticEvent event);  // Blocking publish
    
    // Querying
    std::vector<DiagnosticEvent> query_events(
        const std::string& event_type = "",
        const std::string& source = "",
        Severity min_severity = Severity::DEBUG,
        Timestamp since = Timestamp{},
        size_t limit = 100) const;
    
    std::vector<DiagnosticEvent> get_recent_events(size_t count = 100) const;
    
    // Statistics
    size_t total_events_published() const { return total_events_.load(); }
    size_t total_events_delivered() const { return total_delivered_.load(); }
    size_t active_subscriptions() const;
    size_t dropped_events() const { return dropped_.load(); }
    
    // Lifecycle
    void start_async_processing(size_t thread_count = 1);
    void stop_async_processing();
    bool is_processing() const { return processing_active_.load(); }
    
    // Flush remaining events
    void flush();
    
    // Memory management
    void set_max_buffer_size(size_t max_size);
    size_t current_buffer_size() const;
    void clear_buffer();
    
    // Export
    std::string export_events_json(Timestamp since = Timestamp{}) const;
    bool export_events_to_file(const std::string& path, Timestamp since = Timestamp{}) const;
    
private:
    EventBus();
    ~EventBus();
    
    // Internal processing
    void process_queue();
    void deliver_event(const DiagnosticEvent& event);
    
    // Data members
    mutable std::mutex subscriptions_mutex_;
    std::list<Subscription> subscriptions_;
    
    mutable std::mutex event_queue_mutex_;
    std::queue<DiagnosticEvent> event_queue_;
    std::vector<DiagnosticEvent> event_history_;  // Ring buffer for queries
    size_t max_buffer_size_{10000};
    
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> total_events_{0};
    std::atomic<size_t> total_delivered_{0};
    std::atomic<size_t> dropped_{0};
    
    // Async processing
    std::atomic<bool> processing_active_{false};
    std::vector<std::thread> processing_threads_;
    std::condition_variable queue_cv_;
    bool shutdown_requested_{false};
    
    // History ring buffer index
    mutable std::size_t history_index_{0};
};

//=============================================================================
// Inline Helper Functions
//=============================================================================

inline void emit_diagnostic_event(
    const std::string& source,
    const std::string& type,
    Severity severity,
    const std::string& message,
    const std::unordered_map<std::string, std::string>& metadata = {}) {
    
    if (!global::is_initialized()) return;
    
    DiagnosticEvent event;
    event.source_plugin = source;
    event.event_type = type;
    event.severity = severity;
    event.message = message;
    event.metadata = metadata;
    
    global::get_event_bus()->publish(std::move(event));
}

// Convenience macros for common patterns
#define DIAG_EMIT_INFO(src, type, msg) \
    prosper::diagnostics::emit_diagnostic_event(src, type, prosper::diagnostics::Severity::INFO, msg)

#define DIAG_EMIT_WARNING(src, type, msg) \
    prosper::diagnostics::emit_diagnostic_event(src, type, prosper::diagnostics::Severity::WARNING, msg)

#define DIAG_EMIT_ERROR(src, type, msg) \
    prosper::diagnostics::emit_diagnostic_event(src, type, prosper::diagnostics::Severity::ERROR, msg)

#define DIAG_EMIT_CRITICAL(src, type, msg) \
    prosper::diagnostics::emit_diagnostic_event(src, type, prosper::diagnostics::Severity::CRITICAL, msg)

} // namespace diagnostics
} // namespace prosper
