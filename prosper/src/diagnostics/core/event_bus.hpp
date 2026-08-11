// diagnostics/core/event_bus.hpp — Thread-safe event bus for diagnostic events
//
// Central hub for all diagnostic events. Collectors publish events here,
// subscribers (storage, AI analyzer, etc.) consume them.
// Zero-overhead when disabled: all calls are gated on is_enabled().
#pragma once

#include "types.hpp"
#include <functional>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <thread>

namespace prosper {
namespace diagnostics {

// Forward declarations
class EventSubscriber;

using EventCallback = std::function<void(const DiagnosticEvent&)>;

class EventBus {
public:
    static EventBus& instance();
    
    // --- Lifecycle -----------------------------------------------------------
    
    // Enable/disable event collection. When disabled, publish() returns immediately
    // without any locking or allocation - truly zero overhead.
    void enable(bool enabled = true) { enabled_.store(enabled, std::memory_order_relaxed); }
    bool is_enabled() const { return enabled_.load(std::memory_order_relaxed); }
    
    // Start/stop the background processing thread (for async subscribers)
    void start();
    void stop();
    
    // --- Event Publishing ----------------------------------------------------
    
    // Publish an event. Thread-safe. No-op when disabled.
    void publish(DiagnosticEvent&& event);
    
    // Convenience: build and publish an event in one call
    void emit(
        const std::string& type,
        Severity severity,
        Subsystem subsystem,
        const std::string& message,
        const SourceLocation& source = {},
        const std::function<void(DiagnosticEvent&)>& enrich = nullptr
    );
    
    // --- Subscription --------------------------------------------------------
    
    // Add a subscriber callback (called synchronously on publish thread)
    uint64_t subscribe(EventCallback callback);
    
    // Remove a subscriber by ID
    void unsubscribe(uint64_t id);
    
    // --- Query ---------------------------------------------------------------
    
    // Get current event count
    uint64_t event_count() const { return event_count_.load(std::memory_order_relaxed); }
    
    // Wait for all pending events to be processed (call before shutdown)
    void flush();
    
private:
    EventBus() = default;
    ~EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    
    void dispatch(const DiagnosticEvent& event);
    
    std::atomic<bool>       enabled_{false};
    std::atomic<uint64_t>   event_count_{0};
    std::atomic<uint64_t>   next_subscriber_id_{1};
    
    mutable std::mutex              subscribers_mutex_;
    std::vector<std::pair<uint64_t, EventCallback>> subscribers_;
    
    // Background processing
    std::atomic<bool>       running_{false};
    std::thread             dispatch_thread_;
    std::mutex              queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<DiagnosticEvent> event_queue_;
};

// RAII helper to subscribe for a scope's lifetime
class ScopedSubscription {
public:
    explicit ScopedSubscription(EventCallback cb)
        : id_(EventBus::instance().subscribe(std::move(cb))) {}
    ~ScopedSubscription() { EventBus::instance().unsubscribe(id_); }
    ScopedSubscription(const ScopedSubscription&) = delete;
    ScopedSubscription& operator=(const ScopedSubscription&) = delete;
    
    uint64_t id() const { return id_; }
    
private:
    uint64_t id_;
};

// --- Inline Implementations -------------------------------------------------

inline EventBus& EventBus::instance() {
    static EventBus bus;
    return bus;
}

inline void EventBus::publish(DiagnosticEvent&& event) {
    if (!is_enabled()) return;
    
    event.event_id.id = event_count_.fetch_add(1, std::memory_order_acq_rel);
    event.event_id.timestamp = std::chrono::steady_clock::now();
    
    // Synchronous dispatch to subscribers (keeps ordering guarantees)
    {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        dispatch(event);
    }
    
    // Also queue for async processing if running
    if (running_.load(std::memory_order_relaxed)) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            event_queue_.push(std::move(event));
        }
        queue_cv_.notify_one();
    }
}

inline void EventBus::emit(
    const std::string& type,
    Severity severity,
    Subsystem subsystem,
    const std::string& message,
    const SourceLocation& source,
    const std::function<void(DiagnosticEvent&)>& enrich
) {
    if (!is_enabled()) return;
    
    DiagnosticEvent event;
    event.type = type;
    event.severity = severity;
    event.subsystem = subsystem;
    event.message = message;
    event.source = source;
    
    if (enrich) enrich(event);
    
    publish(std::move(event));
}

} // namespace diagnostics
} // namespace prosper
