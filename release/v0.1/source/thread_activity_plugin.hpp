#pragma once
#include "../core/diagnostic_interface.hpp"
#include "../core/event_bus.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_set>

/**
 * @file thread_activity_plugin.hpp
 * @brief Thread Activity Plugin - Tracks thread lifecycle and synchronization
 * 
 * Phase 9.5 Diagnostic Plugin for Prosper PS4 Emulator
 * 
 * Features:
 * - Records thread creation/termination events
 * - Tracks thread states (running, blocked, sleeping, etc.)
 * - Monitors thread synchronization primitives (mutexes, semaphores, condvars)
 * - Detects potential deadlocks using timeout-based heuristics
 * - Provides comprehensive thread information queries
 */

namespace prosper {
namespace diagnostics {

//=============================================================================
// Thread Information Structure
//=============================================================================

struct ThreadInfo {
    uint32_t tid{0};
    std::string name;
    std::string state;  // "running", "blocked", "sleeping", "terminated", "unknown"
    Timestamp created{};
    Timestamp last_state_change{};
    uint64_t cpu_time_ms{0};
    std::vector<std::string> held_locks;
    std::string waiting_for_lock;
    size_t priority{0};
    std::string stack_info;
    
    // Serialization
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"tid\":" << tid << ",";
        oss << "\"name\":\"" << escape_json(name) << "\",";
        oss << "\"state\":\"" << state << "\",";
        oss << "\"created_ms\":" << std::fixed << std::setprecision(3) << timestamp_to_ms(created) << ",";
        oss << "\"last_state_change_ms\":" << timestamp_to_ms(last_state_change) << ",";
        oss << "\"cpu_time_ms\":" << cpu_time_ms << ",";
        oss << "\"priority\":" << priority << ",";
        
        oss << "\"held_locks\":[";
        for (size_t i = 0; i < held_locks.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << escape_json(held_locks[i]) << "\"";
        }
        oss << "],";
        
        oss << "\"waiting_for_lock\":\"" << escape_json(waiting_for_lock) << "\",";
        oss << "\"stack_info\":\"" << escape_json(stack_info) << "\"";
        oss << "}";
        return oss.str();
    }
    
    static std::string escape_json(const std::string& s) {
        std::string result;
        result.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c; break;
            }
        }
        return result;
    }
};

//=============================================================================
// Deadlock Detection Result
//=============================================================================

struct DeadlockDetectionResult {
    bool potential_deadlock{false};
    std::vector<uint32_t> involved_threads;
    std::vector<std::string> lock_cycle;
    double confidence{0.0};
    Timestamp detected_at{};
    std::string description;
    
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"potential_deadlock\":" << (potential_deadlock ? "true" : "false") << ",";
        oss << "\"confidence\":" << std::fixed << std::setprecision(2) << confidence << ",";
        oss << "\"detected_at_ms\":" << timestamp_to_ms(detected_at) << ",";
        oss << "\"description\":\"" << ThreadInfo::escape_json(description) << "\",";
        
        oss << \"involved_threads\":[";
        for (size_t i = 0; i < involved_threads.size(); ++i) {
            if (i > 0) oss << ",";
            oss << involved_threads[i];
        }
        oss << "],";
        
        oss << "\"lock_cycle\":[";
        for (size_t i = 0; i < lock_cycle.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << ThreadInfo::escape_json(lock_cycle[i]) << "\"";
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

//=============================================================================
// Lock Information Structure
//=============================================================================

struct LockInfo {
    std::string lock_id;
    std::string lock_type;  // "mutex", "semaphore", "spinlock", "rwlock", "condvar"
    uint32_t owner_tid{0};
    Timestamp acquired_at{};
    std::vector<uint32_t> waiting_threads;
    size_t acquisition_count{0};
    
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"lock_id\":\"" << ThreadInfo::escape_json(lock_id) << "\",";
        oss << "\"lock_type\":\"" << lock_type << "\",";
        oss << "\"owner_tid\":" << owner_tid << ",";
        oss << "\"acquired_at_ms\":" << timestamp_to_ms(acquired_at) << ",";
        oss << "\"acquisition_count\":" << acquisition_count << ",";
        
        oss << "\"waiting_threads\":[";
        for (size_t i = 0; i < waiting_threads.size(); ++i) {
            if (i > 0) oss << ",";
            oss << waiting_threads[i];
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

//=============================================================================
// Thread Activity Plugin Implementation
//=============================================================================

class ThreadActivityPlugin final : public DiagnosticPlugin {
public:
    ThreadActivityPlugin() = default;
    ~ThreadActivityPlugin() override { shutdown(); }
    
    //-------------------------------------------------------------------------
    // DiagnosticPlugin Interface
    //-------------------------------------------------------------------------
    
    std::string name() const override { return "thread_activity"; }
    std::string version() const override { return "1.0.0"; }
    std::string description() const override {
        return "Tracks thread lifecycle, states, and detects potential deadlocks";
    }
    
    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        threads_.clear();
        locks_.clear();
        state_history_.clear();
        deadlock_results_.clear();
        active_ = true;
        
        // Subscribe to relevant events
        if (global::is_initialized()) {
            auto* bus = global::get_event_bus();
            event_subscription_id_ = bus->subscribe(
                name(),
                [this](const DiagnosticEvent& evt) { on_event(evt); },
                [](const DiagnosticEvent& evt) {
                    return evt.event_type.find("thread") != std::string::npos ||
                           evt.event_type.find("sync") != std::string::npos ||
                           evt.event_type.find("lock") != std::string::npos;
                }
            );
        }
        
        return true;
    }
    
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        active_ = false;
        
        if (global::is_initialized() && !event_subscription_id_.empty()) {
            global::get_event_bus()->unsubscribe(event_subscription_id_);
            event_subscription_id_.clear();
        }
    }
    
    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        threads_.clear();
        locks_.clear();
        state_history_.clear();
        deadlock_results_.clear();
        event_count_ = 0;
        next_tid_ = 1000;  // Reset TID counter
    }
    
    void on_event(const DiagnosticEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return;
        
        // Check event limit
        if (global::get_config()) {
            if (event_count_ >= global::get_config()->max_events_per_plugin) {
                return;  // Drop event if at limit
            }
        }
        
        // Process thread-related events
        if (event.event_type == "thread_created") {
            uint32_t tid = 0;
            auto it = event.numeric_data.find("tid");
            if (it != event.numeric_data.end()) tid = static_cast<uint32_t>(it->second);
            
            std::string tname = event.metadata.count("thread_name") ? 
                                event.metadata.at("thread_name") : "unnamed";
            
            ThreadInfo info;
            info.tid = tid ? tid : generate_tid();
            info.name = tname;
            info.state = "running";
            info.created = event.timestamp;
            info.last_state_change = event.timestamp;
            
            threads_[info.tid] = info;
            event_count_++;
        }
        else if (event.event_type == "thread_terminated") {
            uint32_t tid = 0;
            auto it = event.numeric_data.find("tid");
            if (it != event.numeric_data.end()) tid = static_cast<uint32_t>(it->second);
            
            auto thread_it = threads_.find(tid);
            if (thread_it != threads_.end()) {
                thread_it->second.state = "terminated";
                thread_it->second.last_state_change = event.timestamp;
                
                // Release all held locks
                for (const auto& lock_id : thread_it->second.held_locks) {
                    release_lock_internal(lock_id);
                }
                thread_it->second.held_locks.clear();
                
                record_state_change(tid, "terminated");
                event_count_++;
            }
        }
        else if (event.event_type == "lock_acquired") {
            std::string lock_id = event.metadata.count("lock_id") ?
                                  event.metadata.at("lock_id") : "";
            uint32_t tid = 0;
            auto it = event.numeric_data.find("tid");
            if (it != event.numeric_data.end()) tid = static_cast<uint32_t>(it->second);
            std::string lock_type = event.metadata.count("lock_type") ?
                                    event.metadata.at("lock_type") : "mutex";
            
            acquire_lock_internal(lock_id, tid, lock_type, event.timestamp);
            event_count_++;
        }
        else if (event.event_type == "lock_released") {
            std::string lock_id = event.metadata.count("lock_id") ?
                                  event.metadata.at("lock_id") : "";
            release_lock_internal(lock_id);
            event_count_++;
        }
    }
    
    std::string generate_report() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ostringstream oss;
        oss << "{";
        oss << "\"plugin\":\"" << name() << "\",";
        oss << "\"version\":\"" << version() << "\",";
        oss << "\"generated_at_ms\":" << timestamp_to_ms(now()) << ",";
        oss << "\"total_threads_recorded\":" << threads_.size() << ",";
        oss << "\"active_thread_count\:" << count_active_threads() << ",";
        oss << "\"total_locks_tracked\":" << locks_.size() << ",";
        oss << "\"deadlock_detections\":" << deadlock_results_.size() << ",";
        
        // Threads summary
        oss << "\"threads\":[";
        bool first = true;
        for (const auto& pair : threads_) {
            if (!first) oss << ",";
            first = false;
            oss << pair.second.to_json();
        }
        oss << "],";
        
        // Locks summary
        oss << "\"locks\":[";
        first = true;
        for (const auto& pair : locks_) {
            if (!first) oss << ",";
            first = false;
            oss << pair.second.to_json();
        }
        oss << "],";
        
        // Recent deadlock detections
        oss << "\"recent_deadlocks\":[";
        size_t count = 0;
        for (auto it = deadlock_results_.rbegin(); 
             it != deadlock_results_.rend() && count < 10; ++it, ++count) {
            if (count > 0) oss << ",";
            oss << it->to_json();
        }
        oss << "]";
        oss << "}";
        
        return oss.str();
    }
    
    void export_json(const std::string& path) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ofstream ofs(path);
        if (ofs.is_open()) {
            ofs << generate_report();
        }
    }
    
    //-------------------------------------------------------------------------
    // Thread Lifecycle Methods
    //-------------------------------------------------------------------------
    
    /**
     * @brief Record a new thread creation
     * @param tid Thread ID (0 to auto-generate)
     * @param name Thread name
     * @param priority Thread priority
     * @return The assigned thread ID
     */
    uint32_t on_thread_created(uint32_t tid = 0, const std::string& name = "unnamed",
                               size_t priority = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return 0;
        
        // Check event limit
        enforce_event_limit();
        
        ThreadInfo info;
        info.tid = tid ? tid : generate_tid();
        info.name = name.empty() ? "thread_" + std::to_string(info.tid) : name;
        info.state = "running";
        info.created = now();
        info.last_state_change = info.created;
        info.priority = priority;
        
        threads_[info.tid] = info;
        event_count_++;
        
        // Emit diagnostic event
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "thread_created";
        event.severity = Severity::INFO;
        event.message = "Thread created: " + info.name + " (TID=" + std::to_string(info.tid) + ")";
        event.category = "thread_lifecycle";
        event.numeric_data["tid"] = info.tid;
        event.numeric_data["priority"] = priority;
        event.metadata["thread_name"] = info.name;
        
        emit_event(event);
        
        return info.tid;
    }
    
    /**
     * @brief Record thread termination
     * @param tid Thread ID to terminate
     * @param exit_code Optional exit code
     */
    void on_thread_terminated(uint32_t tid, int exit_code = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return;
        
        enforce_event_limit();
        
        auto it = threads_.find(tid);
        if (it == threads_.end()) return;
        
        it->second.state = "terminated";
        it->second.last_state_change = now();
        
        // Release all held locks
        for (const auto& lock_id : it->second.held_locks) {
            release_lock_internal(lock_id);
        }
        it->second.held_locks.clear();
        
        record_state_change(tid, "terminated");
        event_count_++;
        
        // Emit diagnostic event
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "thread_terminated";
        event.severity = Severity::INFO;
        event.message = "Thread terminated: " + it->second.name + " (TID=" + std::to_string(tid) + ")";
        event.category = "thread_lifecycle";
        event.numeric_data["tid"] = tid;
        event.numeric_data["exit_code"] = exit_code;
        event.metadata["thread_name"] = it->second.name;
        
        emit_event(event);
    }
    
    /**
     * @brief Record a thread state change
     * @param tid Thread ID
     * @param new_state New state ("running", "blocked", "sleeping", "waiting")
     */
    void on_state_change(uint32_t tid, const std::string& new_state) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return;
        
        enforce_event_limit();
        
        auto it = threads_.find(tid);
        if (it == threads_.end()) return;
        
        std::string old_state = it->second.state;
        it->second.state = new_state;
        it->second.last_state_change = now();
        
        record_state_change(tid, new_state);
        event_count_++;
        
        // Emit diagnostic event for significant state changes
        if (new_state == "blocked" || new_state == "sleeping") {
            DiagnosticEvent event;
            event.source_plugin = name();
            event.event_type = "state_change";
            event.severity = new_state == "blocked" ? Severity::WARNING : Severity::DEBUG;
            event.message = "Thread " + it->second.name + " state: " + old_state + " -> " + new_state;
            event.category = "thread_state";
            event.numeric_data["tid"] = tid;
            event.metadata["old_state"] = old_state;
            event.metadata["new_state"] = new_state;
            event.metadata["thread_name"] = it->second.name;
            
            emit_event(event);
        }
    }
    
    //-------------------------------------------------------------------------
    // Synchronization Primitive Tracking
    //-------------------------------------------------------------------------
    
    /**
     * @brief Record lock acquisition
     * @param lock_id Unique identifier for the lock
     * @param tid Acquiring thread ID
     * @param lock_type Type of lock ("mutex", "semaphore", "spinlock", "rwlock")
     */
    void on_lock_acquired(const std::string& lock_id, uint32_t tid,
                          const std::string& lock_type = "mutex") {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return;
        
        enforce_event_limit();
        acquire_lock_internal(lock_id, tid, lock_type, now());
        event_count_++;
    }
    
    /**
     * @brief Record lock release
     * @param lock_id Unique identifier for the lock
     */
    void on_lock_released(const std::string& lock_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return;
        
        enforce_event_limit();
        release_lock_internal(lock_id);
        event_count_++;
    }
    
    /**
     * @brief Record that a thread is waiting for a lock
     * @param tid Waiting thread ID
     * @param lock_id Lock being waited on
     */
    void on_lock_wait(uint32_t tid, const std::string& lock_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return;
        
        enforce_event_limit();
        
        auto it = threads_.find(tid);
        if (it != threads_.end()) {
            it->second.waiting_for_lock = lock_id;
            it->second.state = "blocked";
            it->second.last_state_change = now();
        }
        
        auto lock_it = locks_.find(lock_id);
        if (lock_it != locks_.end()) {
            lock_it->second.waiting_threads.push_back(tid);
        }
        
        event_count_++;
    }
    
    //-------------------------------------------------------------------------
    // Deadlock Detection
    //-------------------------------------------------------------------------
    
    /**
     * @brief Detect potential deadlocks using timeout-based heuristic
     * @param block_timeout_ms Consider threads blocked longer than this as potentially deadlocked
     * @return Detection result with details about any found issues
     */
    DeadlockDetectionResult detect_deadlock(double block_timeout_ms = 5000.0) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        DeadlockDetectionResult result;
        result.detected_at = now();
        
        Timestamp current = now();
        
        // Find threads that have been blocked for too long
        std::vector<uint32_t> long_blocked_threads;
        
        for (const auto& pair : threads_) {
            const ThreadInfo& info = pair.second;
            
            if (info.state == "blocked" && !info.waiting_for_lock.empty()) {
                double blocked_ms = timestamp_to_ms(current) - 
                                   timestamp_to_ms(info.last_state_change);
                
                if (blocked_ms >= block_timeout_ms) {
                    long_blocked_threads.push_back(info.tid);
                    
                    // Check if this thread holds locks that other blocked threads are waiting for
                    const std::string& waited_lock = info.waiting_for_lock;
                    auto waited_lock_it = locks_.find(waited_lock);
                    
                    if (waited_lock_it != locks_.end() && 
                        waited_lock_it->second.owner_tid != 0) {
                        
                        // The owner of the lock we're waiting for
                        uint32_t owner_tid = waited_lock_it->second.owner_tid;
                        auto owner_it = threads_.find(owner_tid);
                        
                        // Check if owner is also blocked and waiting for one of our locks
                        if (owner_it != threads_.end() && 
                            owner_it->second.state == "blocked") {
                            
                            for (const auto& held_lock : info.held_locks) {
                                auto held_lock_it = locks_.find(held_lock);
                                if (held_lock_it != locks_.end()) {
                                    for (uint32_t waiter : held_lock_it->second.waiting_threads) {
                                        if (waiter == owner_tid) {
                                            // Found circular wait condition!
                                            result.potential_deadlock = true;
                                            result.involved_threads.push_back(info.tid);
                                            result.involved_threads.push_back(owner_tid);
                                            
                                            result.lock_cycle.push_back(held_lock + 
                                                " (held by T" + std::to_string(info.tid) + 
                                                ", waited by T" + std::to_string(owner_tid) + ")");
                                            result.lock_cycle.push_back(waited_lock + 
                                                " (held by T" + std::to_string(owner_tid) + 
                                                ", waited by T" + std::to_string(info.tid) + ")");
                                            
                                            result.confidence = std::min(1.0, 
                                                blocked_ms / block_timeout_ms);
                                            result.description = "Circular dependency detected between T" +
                                                std::to_string(info.tid) + " (" + info.name + 
                                                ") and T" + std::to_string(owner_tid) + 
                                                " (" + owner_it->second.name + ")";
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        if (result.potential_deadlock) {
            deadlock_results_.push_back(result);
            
            // Keep only recent results (bounded storage)
            while (deadlock_results_.size() > 100) {
                deadlock_results_.pop_front();
            }
            
            // Emit critical event
            DiagnosticEvent event;
            event.source_plugin = name();
            event.event_type = "potential_deadlock";
            event.severity = Severity::CRITICAL;
            event.message = result.description;
            event.category = "deadlock_detection";
            event.float_data["confidence"] = result.confidence;
            
            emit_event(event);
        }
        
        // If no circular wait but long-blocked threads exist, report lower confidence warning
        if (!result.potential_deadlock && !long_blocked_threads.empty()) {
            result.confidence = 0.3;  // Low confidence - might just be slow operation
            result.involved_threads = long_blocked_threads;
            result.description = std::to_string(long_blocked_threads.size()) + 
                " threads blocked for more than " + std::to_string(block_timeout_ms) + "ms";
        }
        
        return result;
    }
    
    //-------------------------------------------------------------------------
    // Query Methods
    //-------------------------------------------------------------------------
    
    /**
     * @brief Get information about a specific thread
     * @param tid Thread ID
     * @return Thread information, or default if not found
     */
    ThreadInfo get_thread_info(uint32_t tid) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = threads_.find(tid);
        if (it != threads_.end()) {
            return it->second;
        }
        return ThreadInfo{};  // Return default/empty info
    }
    
    /**
     * @brief Get all tracked threads
     * @return Vector of all thread information
     */
    std::vector<ThreadInfo> get_all_threads() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<ThreadInfo> result;
        result.reserve(threads_.size());
        
        for (const auto& pair : threads_) {
            result.push_back(pair.second);
        }
        
        return result;
    }
    
    /**
     * @brief Get threads in a specific state
     * @param state State to filter by
     * @return Vector of matching threads
     */
    std::vector<ThreadInfo> get_threads_by_state(const std::string& state) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<ThreadInfo> result;
        
        for (const auto& pair : threads_) {
            if (pair.second.state == state) {
                result.push_back(pair.second);
            }
        }
        
        return result;
    }
    
    /**
     * @brief Get information about a specific lock
     * @param lock_id Lock identifier
     * @return Lock information, or default if not found
     */
    LockInfo get_lock_info(const std::string& lock_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = locks_.find(lock_id);
        if (it != locks_.end()) {
            return it->second;
        }
        return LockInfo{};
    }
    
    /**
     * @brief Get all tracked locks
     * @return Vector of all lock information
     */
    std::vector<LockInfo> get_all_locks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<LockInfo> result;
        result.reserve(locks_.size());
        
        for (const auto& pair : locks_) {
            result.push_back(pair.second);
        }
        
        return result;
    }
    
    /**
     * @brief Get recent deadlock detection results
     * @param max_results Maximum number of results to return
     * @return Vector of detection results
     */
    std::vector<DeadlockDetectionResult> get_recent_deadlocks(size_t max_results = 10) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<DeadlockDetectionResult> result;
        
        for (auto it = deadlock_results_.rbegin(); 
             it != deadlock_results_.rend() && result.size() < max_results; ++it) {
            result.push_back(*it);
        }
        
        return result;
    }
    
    /**
     * @brief Get thread count statistics
     * @return Map of state -> count
     */
    std::unordered_map<std::string, size_t> get_thread_counts() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::unordered_map<std::string, size_t> counts;
        
        for (const auto& pair : threads_) {
            counts[pair.second.state]++;
        }
        
        return counts;
    }

private:
    //-------------------------------------------------------------------------
    // Internal Data Structures
    //-------------------------------------------------------------------------
    
    std::unordered_map<uint32_t, ThreadInfo> threads_;
    std::unordered_map<std::string, LockInfo> locks_;
    std::deque<DeadlockDetectionResult> deadlock_results_;
    
    // State change history (bounded ring buffer)
    struct StateChangeRecord {
        uint32_t tid;
        std::string state;
        Timestamp time;
    };
    std::deque<StateChangeRecord> state_history_;
    static constexpr size_t MAX_STATE_HISTORY = 10000;
    
    std::string event_subscription_id_;
    uint32_t next_tid_{1000};  // Auto-generated TIDs start here
    
    //-------------------------------------------------------------------------
    // Internal Helper Methods
    //-------------------------------------------------------------------------
    
    uint32_t generate_tid() {
        return next_tid_++;
    }
    
    size_t count_active_threads() const {
        size_t count = 0;
        for (const auto& pair : threads_) {
            if (pair.second.state != "terminated") {
                count++;
            }
        }
        return count;
    }
    
    void record_state_change(uint32_t tid, const std::string& state) {
        StateChangeRecord record;
        record.tid = tid;
        record.state = state;
        record.time = now();
        
        state_history_.push_back(record);
        
        // Enforce bounded history
        while (state_history_.size() > MAX_STATE_HISTORY) {
            state_history_.pop_front();
        }
    }
    
    void acquire_lock_internal(const std::string& lock_id, uint32_t tid,
                               const std::string& lock_type, Timestamp time) {
        auto it = locks_.find(lock_id);
        if (it == locks_.end()) {
            LockInfo info;
            info.lock_id = lock_id;
            info.lock_type = lock_type;
            info.owner_tid = tid;
            info.acquired_at = time;
            info.acquisition_count = 1;
            locks_[lock_id] = info;
        } else {
            it->second.owner_tid = tid;
            it->second.acquired_at = time;
            it->second.acquisition_count++;
            it->second.waiting_threads.clear();  // Clear waiters on successful acquire
        }
        
        // Update thread's held locks
        auto thread_it = threads_.find(tid);
        if (thread_it != threads_.end()) {
            // Avoid duplicates
            if (std::find(thread_it->second.held_locks.begin(), 
                         thread_it->second.held_locks.end(), 
                         lock_id) == thread_it->second.held_locks.end()) {
                thread_it->second.held_locks.push_back(lock_id);
            }
            thread_it->second.waiting_for_lock.clear();
            thread_it->second.state = "running";
        }
    }
    
    void release_lock_internal(const std::string& lock_id) {
        auto it = locks_.find(lock_id);
        if (it == locks_.end()) return;
        
        uint32_t prev_owner = it->second.owner_tid;
        it->second.owner_tid = 0;
        it->second.acquired_at = {};
        
        // Update thread's held locks
        auto thread_it = threads_.find(prev_owner);
        if (thread_it != threads_.end()) {
            auto lock_it = std::find(thread_it->second.held_locks.begin(),
                                     thread_it->second.held_locks.end(),
                                     lock_id);
            if (lock_it != thread_it->second.held_locks.end()) {
                thread_it->second.held_locks.erase(lock_it);
            }
        }
    }
    
    void enforce_event_limit() {
        if (global::get_config()) {
            size_t max_events = global::get_config()->max_events_per_plugin;
            if (event_count_ >= max_events) {
                // Remove oldest entries to make room
                // For threads, we could remove terminated ones
                std::vector<uint32_t> terminated_tids;
                for (const auto& pair : threads_) {
                    if (pair.second.state == "terminated") {
                        terminated_tids.push_back(pair.first);
                    }
                }
                
                for (uint32_t tid : terminated_tids) {
                    if (event_count_ < max_events / 2) break;
                    threads_.erase(tid);
                    event_count_--;
                }
            }
        }
    }
};

} // namespace diagnostics
} // namespace prosper
