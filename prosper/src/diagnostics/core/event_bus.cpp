// diagnostics/core/event_bus.cpp — EventBus implementation
#include "event_bus.hpp"
#include <cstdio>

namespace prosper {
namespace diagnostics {

void EventBus::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true))
        return;  // Already running
    
    dispatch_thread_ = std::thread([this]() {
        while (running_.load(std::memory_order_acquire)) {
            DiagnosticEvent event;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [&] {
                    return !event_queue_.empty() || !running_.load(std::memory_order_acquire);
                });
                if (event_queue_.empty() && !running_.load(std::memory_order_acquire))
                    break;
                event = std::move(event_queue_.front());
                event_queue_.pop();
            }
            // Async subscribers already handled in publish(); this thread
            // can be used for batch processing, rate limiting, etc.
        }
    });
}

void EventBus::stop() {
    running_.store(false, std::memory_order_release);
    queue_cv_.notify_all();
    if (dispatch_thread_.joinable()) {
        dispatch_thread_.join();
    }
}

uint64_t EventBus::subscribe(EventCallback callback) {
    uint64_t id = next_subscriber_id_.fetch_add(1, std::memory_order_acq_rel);
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    subscribers_.emplace_back(id, std::move(callback));
    return id;
}

void EventBus::unsubscribe(uint64_t id) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    subscribers_.erase(
        std::remove_if(subscribers_.begin(), subscribers_.end(),
            [id](const auto& p) { return p.first == id; }),
        subscribers_.end()
    );
}

void EventBus::dispatch(const DiagnosticEvent& event) {
    for (const auto& [id, cb] : subscribers_) {
        try {
            cb(event);
        } catch (const std::exception& e) {
            // Subscriber error should not crash the emitter
            fprintf(stderr, "[diagnostics] subscriber %zu error: %s\n",
                    (size_t)id, e.what());
        }
    }
}

void EventBus::flush() {
    // Wait for queue to drain
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_cv_.wait(lock, [this] { return event_queue_.empty(); });
}

} // namespace diagnostics
} // namespace prosper
