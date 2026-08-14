/**
 * State Timeline System - Priority 3
 * 
 * PROBLEM SOLVED:
 * Crash location is usually not corruption location.
 * Need historical state to trace back to root cause.
 * 
 * EXAMPLE:
 *   Current: "Crash at frame 1300"
 *   Required: 
 *     Object created:  frame 200
 *     Modified:        frame 600
 *     Invalidated:     frame 900
 *     Used incorrectly: frame 1250
 *     Crash:           frame 1300
 * 
 * DESIGN:
 * - Records state changes with frame-level precision
 * - Tracks object/resource lifecycles
 * - Enables time-travel queries ("what was state at frame X?")
 * - Supports multiple timeline streams (GPU, memory, HLE, etc.)
 */

#pragma once

#include "../core/foundation.hpp"
#include <mutex>
#include <unordered_map>
#include <deque>
#include <atomic>
#include <set>
#include <shared_mutex>
#include <thread>

namespace prosper_debug {
namespace timeline {

// ============================================================================
// Timeline Event Types
// ============================================================================

enum class TimelineEventType : uint8_t {
    // Lifecycle events
    ObjectCreated,
    ObjectDestroyed,
    ObjectModified,
    StateChanged,
    
    // Resource-specific
    ResourceAllocated,
    ResourceFreed,
    ResourceMapped,
    ResourceUnmapped,
    
    // GPU specific
    GpuCommandSubmitted,
    GpuCommandCompleted,
    GpuFenceCreated,
    GpuFenceSignaled,
    GpuMemoryAllocated,
    GpuMemoryFreed,
    TextureUploaded,
    RenderTargetBound,
    
    // Memory specific
    MemoryRegionAllocated,
    MemoryRegionFreed,
    MemoryProtectionChanged,
    MemoryCorrupted,
    
    // Thread/execution
    ThreadCreated,
    ThreadStarted,
    ThreadSuspended,
    ThreadResumed,
    ThreadTerminated,
    ContextSwitch,
    
    // Synchronization
    LockAcquired,
    LockReleased,
    WaitStarted,
    WaitCompleted,
    
    // Error/corruption
    ErrorDetected,
    CorruptionDetected,
    AssertionFailed,
    AnomalyDetected,
    
    // Markers (for manual annotation)
    FrameMarker,
    UserMarker,
    SectionStart,
    SectionEnd,
    
    // Generic
    CustomEvent
};

inline std::string timelineEventTypeToString(TimelineEventType type) {
    switch (type) {
        case TimelineEventType::ObjectCreated: return "ObjectCreated";
        case TimelineEventType::ObjectDestroyed: return "ObjectDestroyed";
        case TimelineEventType::ObjectModified: return "ObjectModified";
        case TimelineEventType::StateChanged: return "StateChanged";
        case TimelineEventType::ResourceAllocated: return "ResourceAllocated";
        case TimelineEventType::ResourceFreed: return "ResourceFreed";
        case TimelineEventType::ResourceMapped: return "ResourceMapped";
        case TimelineEventType::ResourceUnmapped: return "ResourceUnmapped";
        case TimelineEventType::GpuCommandSubmitted: return "Gpu.CommandSubmitted";
        case TimelineEventType::GpuCommandCompleted: return "Gpu.CommandCompleted";
        case TimelineEventType::GpuFenceCreated: return "Gpu.FenceCreated";
        case TimelineEventType::GpuFenceSignaled: return "Gpu.FenceSignaled";
        case TimelineEventType::GpuMemoryAllocated: return "Gpu.MemoryAllocated";
        case TimelineEventType::GpuMemoryFreed: return "Gpu.MemoryFreed";
        case TimelineEventType::TextureUploaded: return "TextureUploaded";
        case TimelineEventType::RenderTargetBound: return "RenderTargetBound";
        case TimelineEventType::MemoryRegionAllocated: return "MemoryRegionAllocated";
        case TimelineEventType::MemoryRegionFreed: return "MemoryRegionFreed";
        case TimelineEventType::MemoryProtectionChanged: return "MemoryProtectionChanged";
        case TimelineEventType::MemoryCorrupted: return "MemoryCorrupted";
        case TimelineEventType::ThreadCreated: return "ThreadCreated";
        case TimelineEventType::ThreadStarted: return "ThreadStarted";
        case TimelineEventType::ThreadSuspended: return "ThreadSuspended";
        case TimelineEventType::ThreadResumed: return "ThreadResumed";
        case TimelineEventType::ThreadTerminated: return "ThreadTerminated";
        case TimelineEventType::ContextSwitch: return "ContextSwitch";
        case TimelineEventType::LockAcquired: return "LockAcquired";
        case TimelineEventType::LockReleased: return "LockReleased";
        case TimelineEventType::WaitStarted: return "WaitStarted";
        case TimelineEventType::WaitCompleted: return "WaitCompleted";
        case TimelineEventType::ErrorDetected: return "ErrorDetected";
        case TimelineEventType::CorruptionDetected: return "CorruptionDetected";
        case TimelineEventType::AssertionFailed: return "AssertionFailed";
        case TimelineEventType::AnomalyDetected: return "AnomalyDetected";
        case TimelineEventType::FrameMarker: return "FrameMarker";
        case TimelineEventType::UserMarker: return "UserMarker";
        case TimelineEventType::SectionStart: return "SectionStart";
        case TimelineEventType::SectionEnd: return "SectionEnd";
        case TimelineEventType::CustomEvent: return "CustomEvent";
    }
    return "Unknown";
}

// ============================================================================
// Timeline Event
// ============================================================================

struct TimelineEvent {
    std::string id;
    DebugTimestamp timestamp;
    uint64_t frame_number{0};
    TimelineEventType type;
    
    // What object/state?
    std::string object_id;       // Unique identifier for the object
    std::string object_type;     // Class/type of object
    std::string object_description;  // Human-readable description
    
    // What changed?
    std::string attribute_name;   // Which attribute changed
    std::string old_value;        // Previous value (JSON string)
    std::string new_value;        // New value (JSON string)
    
    // Provenance
    Subsystem subsystem{Subsystem::Unknown};
    SourceLocation source;
    std::string thread_id;
    
    // Additional data
    std::map<std::string, std::string> metadata;
    
    // Relationships
    std::string parent_event_id;  // Causal parent
    std::vector<std::string> related_event_ids;
    
    static std::string generateId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "tle_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    TimelineEvent() {
        id = generateId();
        timestamp = DebugTimestamp::now();
    }
    
    bool isErrorOrCorruption() const {
        return type == TimelineEventType::ErrorDetected ||
               type == TimelineEventType::CorruptionDetected ||
               type == TimelineEventType::AssertionFailed ||
               type == TimelineEventType::AnomalyDetected;
    }
    
    bool isLifecycleEvent() const {
        return type == TimelineEventType::ObjectCreated ||
               type == TimelineEventType::ObjectDestroyed ||
               type == TimelineEventType::ResourceAllocated ||
               type == TimelineEventType::ResourceFreed;
    }
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"id\": \"" << JsonUtils::escapeJson(id) << "\",\n";
        json << "  \"timestamp\": \"" << timestamp.toISO8601() << "\",\n";
        json << "  \"frame\": " << frame_number << ",\n";
        json << "  \"type\": \"" << timelineEventTypeToString(type) << "\",\n";
        json << "  \"object_id\": \"" << JsonUtils::escapeJson(object_id) << "\",\n";
        json << "  \"object_type\": \"" << JsonUtils::escapeJson(object_type) << "\",\n";
        if (!object_description.empty()) {
            json << "  \"description\": \"" << JsonUtils::escapeJson(object_description) << "\",\n";
        }
        if (!attribute_name.empty()) {
            json << "  \"attribute\": \"" << JsonUtils::escapeJson(attribute_name) << "\",\n";
            json << "  \"old_value\": \"" << JsonUtils::escapeJson(old_value) << "\",\n";
            json << "  \"new_value\": \"" << JsonUtils::escapeJson(new_value) << "\",\n";
        }
        json << "  \"subsystem\": \"" << subsystemToString(subsystem) << "\"\n";
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Object Lifecycle Tracker
// ============================================================================

struct ObjectLifecycle {
    std::string object_id;
    std::string object_type;
    
    // Key lifecycle events
    std::optional<TimelineEvent> created;
    std::optional<TimelineEvent> destroyed;
    std::vector<TimelineEvent> modifications;
    
    // Current state (reconstructed from events)
    std::map<std::string, std::string> current_state;
    
    // Statistics
    uint64_t creation_frame{0};
    uint64_t destruction_frame{0};  // 0 if still alive
    uint64_t frames_before_crash{0};  // For crash analysis
    size_t total_modifications{0};
    uint64_t last_access_frame{0};
    
    bool isAlive() const { return !destroyed.has_value(); }
    uint64_t lifetime() const { 
        return destruction_frame > 0 ? destruction_frame - creation_frame : 0;
    }
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"object_id\": \"" << JsonUtils::escapeJson(object_id) << "\",\n";
        json << "  \"object_type\": \"" << JsonUtils::escapeJson(object_type) << "\",\n";
        json << "  \"alive\": " << (isAlive() ? "true" : "false") << ",\n";
        json << "  \"created_frame\": " << creation_frame << ",\n";
        json << "  \"modifications\": " << total_modifications << ",\n";
        json << "  \"lifetime_frames\": " << lifetime() << "\n";
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Timeline Query Results
// ============================================================================

struct TimelineQueryResult {
    std::vector<TimelineEvent> events;
    
    // Time range of results
    uint64_t start_frame{0};
    uint64_t end_frame{0};
    
    // Filtering applied
    std::vector<TimelineEventType> event_types_filter;
    Subsystem subsystem_filter{Subsystem::Unknown};
    std::string object_id_filter;
    
    // Aggregated statistics
    size_t total_events{0};
    size_t error_events{0};
    size_t lifecycle_events{0};
    
    // Object lifecycles found (if querying by object)
    std::map<std::string, ObjectLifecycle> object_lifecycles;
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"total_events\": " << total_events << ",\n";
        json << "  \"error_events\": " << error_events << ",\n";
        json << "  \"frame_range\": [" << start_frame << ", " << end_frame << "],\n";
        
        json << "  \"events\": [\n";
        for (size_t i = 0; i < events.size() && i < 100; ++i) {  // Limit output
            json << "    " << events[i].toJson();
            if (i < events.size() - 1 && i < 99) json << ",";
            json << "\n";
        }
        if (events.size() > 100) {
            json << "    ... and " << (events.size() - 100) << " more\n";
        }
        json << "  ]\n";
        
        json << "}\n";
        return json.str();
    }
};

// ============================================================================
// State Snapshot at a Point in Time
// ============================================================================

struct StateSnapshot {
    uint64_t target_frame;
    DebugTimestamp approximate_time;
    
    // All active objects at this point
    std::map<std::string, ObjectLifecycle> active_objects;
    
    // Recent events leading to this state
    std::vector<TimelineEvent> preceding_events;
    
    // Any errors/corruptions before this point
    std::vector<TimelineEvent> prior_errors;
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"frame\": " << target_frame << ",\n";
        json << "  \"active_objects\": " << active_objects.size() << ",\n";
        json << "  \"prior_errors\": " << prior_errors.size() << "\n";
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Timeline System - Main Class
// ============================================================================

class StateTimelineSystem {
public:
    struct Config {
        bool enabled;
        size_t max_events;          // Global event limit
        size_t max_events_per_object; // Per-object limit
        bool track_all_objects;       // Track everything or just watched
        bool auto_track_gpu;           // Auto-track GPU resources
        bool auto_track_memory;        // Auto-track major allocations
        bool record_full_state_changes; // Record old+new values (expensive)
        
        Config()
            : enabled(true)
            , max_events(1000000)
            , max_events_per_object(10000)
            , track_all_objects(false)
            , auto_track_gpu(true)
            , auto_track_memory(true)
            , record_full_state_changes(false) {}
        
        Config(bool en, size_t me, size_t mepo, bool tao, bool atg, bool atm, bool rfsc)
            : enabled(en)
            , max_events(me)
            , max_events_per_object(mepo)
            , track_all_objects(tao)
            , auto_track_gpu(atg)
            , auto_track_memory(atm)
            , record_full_state_changes(rfsc) {}
        
        static Config minimal() {
            return Config(true, 100000, 1000, false, false, false, false);
        }
        
        static Config comprehensive() {
            return Config(true, 10000000, 50000, true, true, true, true);
        }
    };
    
    explicit StateTimelineSystem(const Config& config = Config())
        : m_config(config), m_enabled(config.enabled), m_current_frame(0) {}
    
    // ========================================================================
    // Control Interface
    // ========================================================================
    
    void enable() { m_enabled.store(true); }
    void disable() { m_enabled.store(false); }
    bool isEnabled() const { return m_enabled.load(); }
    
    void clear() {
        std::unique_lock lock(m_mutex);
        m_events.clear();
        m_object_lifecycles.clear();
        m_watch_list.clear();
        m_current_frame = 0;
    }
    
    // ========================================================================
    // Frame Tracking
    // ========================================================================
    
    /**
     * Advance to next frame (call once per emulated frame)
     */
    void advanceFrame() {
        if (!isEnabled()) return;
        
        ++m_current_frame;
        
        // Optionally emit frame marker
        if (m_config.track_all_objects) {
            TimelineEvent marker;
            marker.type = TimelineEventType::FrameMarker;
            marker.frame_number = m_current_frame;
            marker.object_description = "Frame " + std::to_string(m_current_frame);
            
            addEventInternal(marker);
        }
    }
    
    void setFrame(uint64_t frame) {
        m_current_frame = frame;
    }
    
    uint64_t getCurrentFrame() const { return m_current_frame.load(); }
    
    // ========================================================================
    // Object Watching
    // ========================================================================
    
    /**
     * Watch a specific object for all state changes
     */
    void watchObject(const std::string& object_id, const std::string& object_type = "") {
        std::unique_lock lock(m_mutex);
        m_watch_list[object_id] = object_type;
    }
    
    void unwatchObject(const std::string& object_id) {
        std::unique_lock lock(m_mutex);
        m_watch_list.erase(object_id);
    }
    
    bool isWatching(const std::string& object_id) const {
        std::shared_lock lock(m_mutex);
        return m_watch_list.count(object_id) > 0 || m_config.track_all_objects;
    }
    
    // ========================================================================
    // Event Recording Interface
    // ========================================================================
    
    /**
     * Record generic timeline event
     */
    void recordEvent(
        TimelineEventType type,
        const std::string& object_id,
        const SourceLocation& loc = DEBUG_HERE(),
        Subsystem subs = Subsystem::Unknown) {
        
        if (!isEnabled()) return;
        if (!shouldTrack(object_id)) return;
        
        TimelineEvent event;
        event.type = type;
        event.object_id = object_id;
        event.frame_number = m_current_frame;
        event.subsystem = subs;
        event.source = loc;
        event.thread_id = getCurrentThreadId();
        
        addEventInternal(event);
    }
    
    /**
     * Record object creation
     */
    void recordCreation(
        const std::string& object_id,
        const std::string& object_type,
        const std::string& description = "",
        Subsystem subs = Subsystem::Unknown,
        const SourceLocation& loc = DEBUG_HERE()) {
        
        if (!isEnabled()) return;
        if (!shouldTrack(object_id)) return;
        
        TimelineEvent event;
        event.type = TimelineEventType::ObjectCreated;
        event.object_id = object_id;
        event.object_type = object_type;
        event.object_description = description;
        event.frame_number = m_current_frame;
        event.subsystem = subs;
        event.source = loc;
        event.thread_id = getCurrentThreadId();
        
        addEventInternal(event);
    }
    
    /**
     * Record object destruction
     */
    void recordDestruction(
        const std::string& object_id,
        Subsystem subs = Subsystem::Unknown,
        const SourceLocation& loc = DEBUG_HERE()) {
        
        if (!isEnabled()) return;
        if (!shouldTrack(object_id)) return;
        
        TimelineEvent event;
        event.type = TimelineEventType::ObjectDestroyed;
        event.object_id = object_id;
        event.frame_number = m_current_frame;
        event.subsystem = subs;
        event.source = loc;
        event.thread_id = getCurrentThreadId();
        
        addEventInternal(event);
    }
    
    /**
     * Record state change on an object
     */
    void recordStateChange(
        const std::string& object_id,
        const std::string& attribute,
        const std::string& old_value,
        const std::string& new_value,
        Subsystem subs = Subsystem::Unknown,
        const SourceLocation& loc = DEBUG_HERE()) {
        
        if (!isEnabled()) return;
        if (!shouldTrack(object_id)) return;
        if (!m_config.record_full_state_changes) return;  // Skip if not recording details
        
        TimelineEvent event;
        event.type = TimelineEventType::StateChanged;
        event.object_id = object_id;
        event.attribute_name = attribute;
        event.old_value = old_value;
        event.new_value = new_value;
        event.frame_number = m_current_frame;
        event.subsystem = subs;
        event.source = loc;
        event.thread_id = getCurrentThreadId();
        
        addEventInternal(event);
    }
    
    /**
     * Record error/corruption event (always recorded even for unwatched objects)
     */
    void recordError(
        TimelineEventType error_type,
        const std::string& object_id,
        const std::string& description = "",
        Severity severity = Severity::Error,
        Subsystem subs = Subsystem::Unknown,
        const SourceLocation& loc = DEBUG_HERE()) {
        
        if (!isEnabled()) return;
        
        TimelineEvent event;
        event.type = error_type;
        event.object_id = object_id;
        event.object_description = description;
        event.frame_number = m_current_frame;
        event.subsystem = subs;
        event.source = loc;
        event.thread_id = getCurrentThreadId();
        event.metadata["severity"] = severityToString(severity);
        
        // Always add errors regardless of watch status
        addEventInternal(event);
    }
    
    /**
     * Add user-defined marker for manual annotation
     */
    void addMarker(const std::string& message, const std::map<std::string, std::string>& tags = {}) {
        if (!isEnabled()) return;
        
        TimelineEvent event;
        event.type = TimelineEventType::UserMarker;
        event.object_description = message;
        event.frame_number = m_current_frame;
        event.subsystem = Subsystem::DiagnosticSystem;
        event.metadata = tags;
        
        addEventInternal(event);
    }
    
    // ========================================================================
    // Query Interface - Time Travel
    // ========================================================================
    
    /**
     * Get all events in a frame range
     */
    TimelineQueryResult queryByFrameRange(uint64_t start_frame, uint64_t end_frame) const {
        TimelineQueryResult result;
        result.start_frame = start_frame;
        result.end_frame = end_frame;
        
        std::shared_lock lock(m_mutex);
        
        for (const auto& event : m_events) {
            if (event.frame_number >= start_frame && event.frame_number <= end_frame) {
                result.events.push_back(event);
                
                if (event.isErrorOrCorruption()) result.error_events++;
                if (event.isLifecycleEvent()) result.lifecycle_events++;
                result.total_events++;
            }
        }
        
        return result;
    }
    
    /**
     * Get complete lifecycle of an object
     */
    std::optional<ObjectLifecycle> getObjectLifecycle(const std::string& object_id) const {
        std::shared_lock lock(m_mutex);
        
        auto it = m_object_lifecycles.find(object_id);
        if (it != m_object_lifecycles.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    /**
     * Get state snapshot at a specific frame
     */
    StateSnapshot getSnapshotAtFrame(uint64_t target_frame) const {
        StateSnapshot snapshot;
        snapshot.target_frame = target_frame;
        
        std::shared_lock lock(m_mutex);
        
        // Find all objects that existed at this frame
        for (const auto& [obj_id, lifecycle] : m_object_lifecycles) {
            if (lifecycle.creation_frame <= target_frame &&
                (lifecycle.destruction_frame == 0 || lifecycle.destruction_frame > target_frame)) {
                snapshot.active_objects[obj_id] = lifecycle;
            }
        }
        
        // Get recent events leading up to this frame
        int count = 0;
        for (auto it = m_events.rbegin(); it != m_events.rend() && count < 50; ++it) {
            if (it->frame_number <= target_frame) {
                snapshot.preceding_events.push_back(*it);
                count++;
                
                if (it->isErrorOrCorruption()) {
                    snapshot.prior_errors.push_back(*it);
                }
            }
        }
        
        return snapshot;
    }
    
    /**
     * Find what happened to an object around a specific frame
     */
    std::vector<TimelineEvent> getObjectEventsAroundFrame(
        const std::string& object_id,
        uint64_t center_frame,
        uint64_t window_size = 100) const {
        
        std::vector<TimelineEvent> result;
        uint64_t start = window_size > center_frame ? 0 : center_frame - window_size;
        uint64_t end = center_frame + window_size;
        
        std::shared_lock lock(m_mutex);
        
        for (const auto& event : m_events) {
            if (event.object_id == object_id &&
                event.frame_number >= start &&
                event.frame_number <= end) {
                result.push_back(event);
            }
        }
        
        return result;
    }
    
    /**
     * Find all errors/corruptions in a frame range
     */
    std::vector<TimelineEvent> findErrorsInFrameRange(
        uint64_t start_frame,
        uint64_t end_frame) const {
        
        std::vector<TimelineEvent> errors;
        
        std::shared_lock lock(m_mutex);
        
        for (const auto& event : m_events) {
            if (event.isErrorOrCorruption() &&
                event.frame_number >= start_frame &&
                event.frame_number <= end_frame) {
                errors.push_back(event);
            }
        }
        
        return errors;
    }
    
    /**
     * Find objects that were destroyed shortly before a crash
     * (Common pattern: use-after-free)
     */
    std::vector<ObjectLifecycle> findObjectsDestroyedBeforeFrame(
        uint64_t crash_frame,
        uint64_t lookback_window = 1000) const {
        
        std::vector<ObjectLifecycle> recently_destroyed;
        uint64_t start = lookback_window > crash_frame ? 0 : crash_frame - lookback_window;
        
        std::shared_lock lock(m_mutex);
        
        for (const auto& [obj_id, lifecycle] : m_object_lifecycles) {
            if (lifecycle.destruction_frame > 0 &&
                lifecycle.destruction_frame >= start &&
                lifecycle.destruction_frame < crash_frame) {
                
                ObjectLifecycle lt;
                lt.object_id = obj_id;
                lt.object_type = lifecycle.object_type;
                lt.destruction_frame = lifecycle.destruction_frame;
                lt.frames_before_crash = crash_frame - lifecycle.destruction_frame;
                
                recently_destroyed.push_back(lt);
            }
        }
        
        // Sort by proximity to crash
        std::sort(recently_destroyed.begin(), recently_destroyed.end(),
            [](const ObjectLifecycle& a, const ObjectLifecycle& b) {
                return a.frames_before_crash < b.frames_before_crash;
            });
        
        return recently_destroyed;
    }
    
    // ========================================================================
    // Statistics
    // ============================================================================
    
    struct Stats {
        size_t total_events{0};
        uint64_t current_frame{0};
        size_t tracked_objects{0};
        size_t watched_objects{0};
        size_t active_objects{0};
        size_t total_errors{0};
        size_t total_corruptions{0};
        
        std::map<TimelineEventType, size_t> events_by_type;
        std::map<Subsystem, size_t> events_by_subsystem;
    };
    
    Stats getStats() const {
        std::shared_lock lock(m_mutex);
        
        Stats stats;
        stats.current_frame = m_current_frame;
        stats.tracked_objects = m_object_lifecycles.size();
        stats.watched_objects = m_watch_list.size();
        
        for (const auto& [obj_id, lifecycle] : m_object_lifecycles) {
            if (lifecycle.isAlive()) stats.active_objects++;
        }
        
        for (const auto& event : m_events) {
            stats.total_events++;
            stats.events_by_type[event.type]++;
            stats.events_by_subsystem[event.subsystem]++;
            
            if (event.type == TimelineEventType::ErrorDetected) stats.total_errors++;
            if (event.type == TimelineEventType::CorruptionDetected) stats.total_corruptions++;
        }
        
        return stats;
    }

private:
    Config m_config;
    std::atomic<bool> m_enabled;
    std::atomic<uint64_t> m_current_frame;
    mutable std::shared_mutex m_mutex;
    
    // Event storage (ordered by insertion)
    std::deque<TimelineEvent> m_events;
    
    // Object lifecycle tracking
    std::map<std::string, ObjectLifecycle> m_object_lifecycles;
    
    // Watch list
    std::map<std::string, std::string> m_watch_list;  // object_id -> type
    
    // ========================================================================
    // Internal Methods
    // ========================================================================
    
    bool shouldTrack(const std::string& object_id) const {
        if (m_config.track_all_objects) return true;
        return m_watch_list.count(object_id) > 0;
    }
    
    void addEventInternal(const TimelineEvent& event) {
        std::unique_lock lock(m_mutex);
        
        // Check global limit
        if (m_events.size() >= m_config.max_events) {
            m_events.pop_front();  // Evict oldest
        }
        
        m_events.push_back(event);
        
        // Update object lifecycle tracking
        updateObjectLifecycle(event);
    }
    
    void updateObjectLifecycle(const TimelineEvent& event) {
        // Get or create lifecycle entry
        auto& lifecycle = m_object_lifecycles[event.object_id];
        
        if (lifecycle.object_id.empty()) {
            lifecycle.object_id = event.object_id;
            lifecycle.object_type = event.object_type;
        }
        
        switch (event.type) {
            case TimelineEventType::ObjectCreated:
            case TimelineEventType::ResourceAllocated:
                if (!lifecycle.created.has_value()) {
                    lifecycle.created = event;
                    lifecycle.creation_frame = event.frame_number;
                }
                break;
                
            case TimelineEventType::ObjectDestroyed:
            case TimelineEventType::ResourceFreed:
                if (!lifecycle.destroyed.has_value()) {
                    lifecycle.destroyed = event;
                    lifecycle.destruction_frame = event.frame_number;
                }
                break;
                
            case TimelineEventType::ObjectModified:
            case TimelineEventType::StateChanged:
                lifecycle.modifications.push_back(event);
                lifecycle.total_modifications++;
                
                // Update current state
                if (!event.attribute_name.empty()) {
                    lifecycle.current_state[event.attribute_name] = event.new_value;
                }
                break;
                
            default:
                break;
        }
        
        // Update last access
        if (event.frame_number > lifecycle.last_access_frame) {
            lifecycle.last_access_frame = event.frame_number;
        }
    }
    
    static std::string getCurrentThreadId() {
        std::stringstream ss;
        ss << std::this_thread::get_id();
        return ss.str();
    }
};

} // namespace timeline
} // namespace prosper_debug
