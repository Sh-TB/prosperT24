/**
 * Memory Provenance Tracker - Priority 1
 * 
 * PROBLEM SOLVED:
 * The biggest missing answer in debugging: "Who wrote this bad value?"
 * 
 * Current debugging shows:
 *   - Crash location (symptom)
 *   - Invalid memory
 *   - Corrupted pointer
 *   
 * Required answer:
 *   - Which subsystem wrote to this address?
 *   - When did it happen?
 *   - What was the old value?
 *   - What was the new value?
 *   - What function caused it?
 * 
 * DESIGN:
 * - Observer only: Does not modify memory operations
 * - Optional: Must be explicitly enabled
 * - Ring buffer: Bounded memory usage
 * - Thread-safe: Can be called from any thread
 * 
 * USAGE:
 *   // Enable tracking for address range
 *   tracker.watchRange(0x2000000000, 0x10000);
 *   
 *   // On memory write (called by existing code)
 *   tracker.recordWrite(0x20000000100, value, size, source);
 *   
 *   // Query when corruption found
 *   auto writers = tracker.whoWrote(0x20000000100);
 */

#pragma once

#include "../core/foundation.hpp"
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <list>
#include <shared_mutex>
#include <thread>

namespace prosper_debug {
namespace memory {

// ============================================================================
// Memory Event Types
// ============================================================================

enum class MemoryEventType : uint8_t {
    Write,
    Read,
    Allocation,
    Free,
    Reallocation,
    ProtectionChange,
    CorruptionDetected,
    AccessViolation
};

inline std::string memoryEventTypeToString(MemoryEventType type) {
    switch (type) {
        case MemoryEventType::Write: return "Write";
        case MemoryEventType::Read: return "Read";
        case MemoryEventType::Allocation: return "Allocation";
        case MemoryEventType::Free: return "Free";
        case MemoryEventType::Reallocation: return "Reallocation";
        case MemoryEventType::ProtectionChange: return "ProtectionChange";
        case MemoryEventType::CorruptionDetected: return "CorruptionDetected";
        case MemoryEventType::AccessViolation: return "AccessViolation";
    }
    return "Unknown";
}

// ============================================================================
// Memory Value Representation
// ============================================================================

/**
 * Represents a memory value with type information
 */
struct MemoryValue {
    std::vector<uint8_t> bytes;
    size_t size{0};
    
    // Typed accessors
    uint8_t asUint8() const { return bytes.size() >= 1 ? bytes[0] : 0; }
    uint16_t asUint16() const { 
        return bytes.size() >= 2 ? 
            (static_cast<uint16_t>(bytes[1]) << 8) | bytes[0] : 0; 
    }
    uint32_t asUint32() const {
        if (bytes.size() < 4) return 0;
        return (static_cast<uint32_t>(bytes[3]) << 24) |
               (static_cast<uint32_t>(bytes[2]) << 16) |
               (static_cast<uint32_t>(bytes[1]) << 8) |
               bytes[0];
    }
    uint64_t asUint64() const {
        if (bytes.size() < 8) return asUint32();
        return (static_cast<uint64_t>(asUint32()) << 32) | asUint32();
    }
    
    /**
     * Convert to hex string for display/JSON
     */
    std::string toHex() const {
        std::stringstream ss;
        ss << "0x";
        // Print in reverse order (big-endian display)
        for (auto it = bytes.rbegin(); it != bytes.rend(); ++it) {
            ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(*it);
        }
        return ss.str();
    }
    
    static MemoryValue fromBytes(const void* data, size_t size) {
        MemoryValue val;
        val.size = size;
        val.bytes.assign(
            static_cast<const uint8_t*>(data),
            static_cast<const uint8_t*>(data) + size
        );
        return val;
    }
    
    static MemoryValue fromUint64(uint64_t value, size_t size = 8) {
        MemoryValue val;
        val.size = size;
        val.bytes.resize(size);
        for (size_t i = 0; i < size; ++i) {
            val.bytes[i] = (value >> (i * 8)) & 0xFF;
        }
        return val;
    }
};

// ============================================================================
// Memory Event - Single Memory Operation Record
// ============================================================================

struct MemoryEvent {
    std::string id;
    DebugTimestamp timestamp;
    MemoryEventType type;
    
    // Address info
    uint64_t address{0};
    size_t size{0};
    MemoryValue old_value;
    MemoryValue new_value;
    
    // Provenance
    Subsystem subsystem{Subsystem::Unknown};
    SourceLocation source;
    std::string thread_id;
    
    // Context
    std::map<std::string, std::string> metadata;
    
    // For allocations
    uint64_t allocation_base{0};
    size_t allocation_size{0};
    std::string allocation_tag;
    
    static std::string generateId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "mem_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    MemoryEvent() {
        id = generateId();
        timestamp = DebugTimestamp::now();
    }
    
    /**
     * Convert to JSON for export
     */
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"id\": \"" << JsonUtils::escapeJson(id) << "\",\n";
        json << "  \"timestamp\": \"" << timestamp.toISO8601() << "\",\n";
        json << "  \"frame\": " << timestamp.frame_number << ",\n";
        json << "  \"type\": \"" << memoryEventTypeToString(type) << "\",\n";
        json << "  \"address\": \"0x" << std::hex << address << std::dec << "\",\n";
        json << "  \"size\": " << size << ",\n";
        json << "  \"old_value\": \"" << JsonUtils::escapeJson(old_value.toHex()) << "\",\n";
        json << "  \"new_value\": \"" << JsonUtils::escapeJson(new_value.toHex()) << "\",\n";
        json << "  \"subsystem\": \"" << subsystemToString(subsystem) << "\",\n";
        json << "  \"source\": \"" << JsonUtils::escapeJson(source.toString()) << "\",\n";
        json << "  \"thread_id\": \"" << JsonUtils::escapeJson(thread_id) << "\"\n";
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Address Range Watch Configuration
// ============================================================================

struct AddressRange {
    uint64_t start{0};
    uint64_t end{0};  // Exclusive
    std::string description;
    bool enabled{true};
    
    bool contains(uint64_t addr, size_t size = 1) const {
        return addr >= start && (addr + size) <= end;
    }
    
    bool overlaps(uint64_t addr, size_t size) const {
        uint64_t range_start = addr;
        uint64_t range_end = addr + size;
        return range_start < end && range_end > start;
    }
    
    size_t length() const { return end - start; }
};

// ============================================================================
// Writer Information - Answering "Who Wrote This?"
// ============================================================================

struct WriterInfo {
    MemoryEvent event;
    
    // Computed context
    double seconds_before_crash{-1};
    int events_before_crash{-1};
    
    // Classification
    bool is_suspicious{false};
    std::string suspicion_reason;
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"event\": " << event.toJson() << ",\n";
        json << "  \"seconds_before_crash\": " << seconds_before_crash << ",\n";
        json << "  \"events_before_crash\": " << events_before_crash << ",\n";
        json << "  \"is_suspicious\": " << (is_suspicious ? "true" : "false") << ",\n";
        json << "  \"suspicion_reason\": \"" << JsonUtils::escapeJson(suspicion_reason) << "\"\n";
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Memory History Query Result
// ============================================================================

struct MemoryHistoryQuery {
    uint64_t target_address{0};
    size_t query_range{0};
    
    std::vector<WriterInfo> all_writers;
    std::vector<WriterInfo> all_readers;
    
    // Last state
    std::optional<MemoryValue> current_value;
    std::optional<WriterInfo> last_writer;
    
    // Timeline summary
    uint64_t first_access_frame{0};
    uint64_t last_access_frame{0};
    size_t total_writes{0};
    size_t total_reads{0};
    
    // Corruption analysis
    std::vector<WriterInfo> suspicious_writes;
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"target_address\": \"0x" << std::hex << target_address << std::dec << "\",\n";
        json << "  \"query_range\": " << query_range << ",\n";
        
        json << "  \"writers\": [\n";
        for (size_t i = 0; i < all_writers.size(); ++i) {
            json << "    " << all_writers[i].toJson();
            if (i < all_writers.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ],\n";
        
        json << "  \"last_writer\": ";
        if (last_writer) {
            json << last_writer->toJson() << "\n";
        } else {
            json << "null\n";
        }
        
        json << ",\n  \"statistics\": {\n";
        json << "    \"total_writes\": " << total_writes << ",\n";
        json << "    \"total_reads\": " << total_reads << ",\n";
        json << "    \"suspicious_writes\": " << suspicious_writes.size() << "\n";
        json << "  }\n";
        
        json << "}\n";
        return json.str();
    }
};

// ============================================================================
// Memory Provenance Tracker - Main Class
// ============================================================================

class MemoryProvenanceTracker {
public:
    struct Config {
        size_t max_events_per_address;
        size_t max_total_events;
        size_t max_watch_ranges;
        bool track_reads;
        bool auto_watch_allocations;
        bool detect_corruption_patterns;
        
        Config() 
            : max_events_per_address(1000)
            , max_total_events(1000000)
            , max_watch_ranges(100)
            , track_reads(false)
            , auto_watch_allocations(true)
            , detect_corruption_patterns(true) {}
        
        Config(size_t mea, size_t mte, size_t mwr, bool tr, bool awa, bool dcp)
            : max_events_per_address(mea)
            , max_total_events(mte)
            , max_watch_ranges(mwr)
            , track_reads(tr)
            , auto_watch_allocations(awa)
            , detect_corruption_patterns(dcp) {}
        
        static Config sensitive() {
            return Config(10000, 10000000, 500, true, true, true);
        }
        
        static Config minimal() {
            return Config(100, 10000, 10, false, false, false);
        }
    };
    
    explicit MemoryProvenanceTracker(const Config& config = Config())
        : m_config(config), m_enabled(false), m_total_events(0) {}
    
    // ========================================================================
    // Control Interface
    // ========================================================================
    
    /**
     * Enable/disable tracking
     * When disabled, recordXxx calls are no-ops (minimal overhead)
     */
    void enable() { m_enabled.store(true); }
    void disable() { m_enabled.store(false); }
    bool isEnabled() const { return m_enabled.load(); }
    
    /**
     * Clear all tracked data
     */
    void clear() {
        std::unique_lock lock(m_mutex);
        m_events.clear();
        m_address_index.clear();
        m_watch_ranges.clear();
        m_allocations.clear();
        m_total_events = 0;
    }
    
    // ========================================================================
    // Range Watching
    // ========================================================================
    
    /**
     * Watch an address range for changes
     */
    bool watchRange(uint64_t start, uint64_t end, const std::string& description = "") {
        std::unique_lock lock(m_mutex);
        
        if (m_watch_ranges.size() >= m_config.max_watch_ranges) {
            return false;
        }
        
        AddressRange range;
        range.start = start;
        range.end = end;
        range.description = description;
        
        m_watch_ranges.push_back(range);
        return true;
    }
    
    /**
     * Remove a watch range
     */
    bool unwatchRange(uint64_t start, uint64_t end) {
        std::unique_lock lock(m_mutex);
        
        auto it = std::find_if(m_watch_ranges.begin(), m_watch_ranges.end(),
            [start, end](const AddressRange& r) {
                return r.start == start && r.end == end;
            });
        
        if (it != m_watch_ranges.end()) {
            m_watch_ranges.erase(it);
            return true;
        }
        return false;
    }
    
    /**
     * Check if address is being watched
     */
    bool isWatched(uint64_t address, size_t size = 1) const {
        std::shared_lock lock(m_mutex);
        
        for (const auto& range : m_watch_ranges) {
            if (range.contains(address, size)) {
                return true;
            }
        }
        return false;
    }
    
    // ========================================================================
    // Event Recording (Call These From Existing Code)
    // ========================================================================
    
    /**
     * Record a memory write event
     * Call this AFTER actual write completes, passing old and new values
     */
    void recordWrite(uint64_t address, const void* data, size_t size,
                     Subsystem subs = Subsystem::Unknown,
                     const SourceLocation& loc = SourceLocation(),
                     const std::string& thread_id = "") {
        
        if (!isEnabled()) return;
        if (!shouldTrack(address, size)) return;
        
        MemoryEvent event;
        event.type = MemoryEventType::Write;
        event.address = address;
        event.size = size;
        event.new_value = MemoryValue::fromBytes(data, size);
        event.subsystem = subs;
        event.source = loc;
        event.thread_id = thread_id.empty() ? getCurrentThreadId() : thread_id;
        
        // Try to get old value (may not always be available)
        // In real implementation, would read from actual memory
        event.old_value = MemoryValue::fromUint64(0, size);  // Placeholder
        
        addEvent(event);
    }
    
    /**
     * Record a memory write with known old value
     */
    void recordWriteWithOldValue(uint64_t address, 
                                  const MemoryValue& old_val,
                                  const MemoryValue& new_val,
                                  Subsystem subs = Subsystem::Unknown,
                                  const SourceLocation& loc = SourceLocation(),
                                  const std::string& thread_id = "") {
        
        if (!isEnabled()) return;
        if (!shouldTrack(address, new_val.size)) return;
        
        MemoryEvent event;
        event.type = MemoryEventType::Write;
        event.address = address;
        event.size = new_val.size;
        event.old_value = old_val;
        event.new_value = new_val;
        event.subsystem = subs;
        event.source = loc;
        event.thread_id = thread_id.empty() ? getCurrentThreadId() : thread_id;
        
        addEvent(event);
    }
    
    /**
     * Record a memory read event (only if reads are tracked)
     */
    void recordRead(uint64_t address, const void* data, size_t size,
                    Subsystem subs = Subsystem::Unknown,
                    const SourceLocation& loc = SourceLocation(),
                    const std::string& thread_id = "") {
        
        if (!isEnabled()) return;
        if (!m_config.track_reads) return;
        if (!shouldTrack(address, size)) return;
        
        MemoryEvent event;
        event.type = MemoryEventType::Read;
        event.address = address;
        event.size = size;
        event.new_value = MemoryValue::fromBytes(data, size);
        event.subsystem = subs;
        event.source = loc;
        event.thread_id = thread_id.empty() ? getCurrentThreadId() : thread_id;
        
        addEvent(event);
    }
    
    /**
     * Record a memory allocation
     */
    void recordAllocation(uint64_t address, size_t size,
                          const std::string& tag = "",
                          Subsystem subs = Subsystem::Unknown,
                          const SourceLocation& loc = SourceLocation()) {
        
        if (!isEnabled()) return;
        if (!m_config.auto_watch_allocations) return;
        
        MemoryEvent event;
        event.type = MemoryEventType::Allocation;
        event.address = address;
        event.size = size;
        event.allocation_base = address;
        event.allocation_size = size;
        event.allocation_tag = tag;
        event.subsystem = subs;
        event.source = loc;
        event.thread_id = getCurrentThreadId();
        
        addEvent(event);
        
        // Auto-watch this allocation
        std::unique_lock lock(m_mutex);
        m_allocations[address] = {address, address + size, tag, true};
    }
    
    /**
     * Record a memory free
     */
    void recordFree(uint64_t address,
                    Subsystem subs = Subsystem::Unknown,
                    const SourceLocation& loc = SourceLocation()) {
        
        if (!isEnabled()) return;
        if (!m_config.auto_watch_allocations) return;
        
        MemoryEvent event;
        event.type = MemoryEventType::Free;
        event.address = address;
        event.subsystem = subs;
        event.source = loc;
        event.thread_id = getCurrentThreadId();
        
        // Look up allocation info
        std::unique_lock lock(m_mutex);
        auto it = m_allocations.find(address);
        if (it != m_allocations.end()) {
            event.allocation_base = it->second.start;
            event.allocation_size = it->second.length();
            event.allocation_tag = it->second.description;
            m_allocations.erase(it);
        }
        
        addEvent(event);
    }
    
    /**
     * Record detected corruption
     */
    void recordCorruption(uint64_t address, const MemoryValue& corrupt_value,
                           const std::string& detection_method,
                           Severity severity = Severity::Critical) {
        
        // Always record corruption, even if not in watched range
        MemoryEvent event;
        event.type = MemoryEventType::CorruptionDetected;
        event.address = address;
        event.size = corrupt_value.size;
        event.new_value = corrupt_value;
        event.subsystem = Subsystem::DiagnosticSystem;
        event.metadata["detection_method"] = detection_method;
        
        addEvent(event);
    }
    
    // ========================================================================
    // Query Interface - Answer "Who Wrote This?"
    // ========================================================================
    
    /**
     * Find who wrote to a specific address
     */
    std::vector<WriterInfo> whoWrote(uint64_t address) const {
        std::shared_lock lock(m_mutex);
        
        std::vector<WriterInfo> writers;
        auto events = getEventsForAddress(address);
        
        for (const auto& event : events) {
            if (event.type == MemoryEventType::Write || 
                event.type == MemoryEventType::Allocation) {
                WriterInfo writer;
                writer.event = event;
                
                // Analyze suspicion
                analyzeSuspicion(writer);
                
                writers.push_back(writer);
            }
        }
        
        return writers;
    }
    
    /**
     * Get the last writer to an address
     */
    std::optional<WriterInfo> lastWriter(uint64_t address) const {
        auto writers = whoWrote(address);
        if (writers.empty()) return std::nullopt;
        return writers.back();  // Most recent
    }
    
    /**
     * Get complete history for an address or range
     */
    MemoryHistoryQuery memoryHistory(uint64_t address, size_t range = 1) const {
        MemoryHistoryQuery query;
        query.target_address = address;
        query.query_range = range;
        
        std::shared_lock lock(m_mutex);
        
        // Collect all events in range
        for (uint64_t addr = address; addr < address + range; ++addr) {
            auto events = getEventsForAddress(addr);
            
            for (const auto& event : events) {
                WriterInfo info;
                info.event = event;
                
                if (event.type == MemoryEventType::Write ||
                    event.type == MemoryEventType::Allocation) {
                    query.all_writers.push_back(info);
                    query.total_writes++;
                    analyzeSuspicion(info);
                    if (info.is_suspicious) {
                        query.suspicious_writes.push_back(info);
                    }
                    query.last_writer = info;
                    query.current_value = event.new_value;
                    
                    if (query.first_access_frame == 0 || 
                        event.timestamp.frame_number < query.first_access_frame) {
                        query.first_access_frame = event.timestamp.frame_number;
                    }
                    if (event.timestamp.frame_number > query.last_access_frame) {
                        query.last_access_frame = event.timestamp.frame_number;
                    }
                } else if (event.type == MemoryEventType::Read) {
                    query.all_readers.push_back(info);
                    query.total_reads++;
                }
            }
        }
        
        return query;
    }
    
    /**
     * Find all addresses written by a specific subsystem
     */
    std::vector<uint64_t> addressesWrittenBy(Subsystem subs) const {
        std::shared_lock lock(m_mutex);
        
        std::set<uint64_t> addresses;
        for (const auto& [addr, events] : m_address_index) {
            for (const auto& event : events) {
                if (event.subsystem == subs && 
                    (event.type == MemoryEventType::Write ||
                     event.type == MemoryEventType::Allocation)) {
                    addresses.insert(addr);
                    break;
                }
            }
        }
        
        return std::vector<uint64_t>(addresses.begin(), addresses.end());
    }
    
    /**
     * Find corruption events
     */
    std::vector<MemoryEvent> findCorruptions() const {
        std::shared_lock lock(m_mutex);
        
        std::vector<MemoryEvent> corruptions;
        for (const auto& [addr, events] : m_address_index) {
            for (const auto& event : events) {
                if (event.type == MemoryEventType::CorruptionDetected) {
                    corruptions.push_back(event);
                }
            }
        }
        
        return corruptions;
    }
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    struct Stats {
        size_t total_events{0};
        size_t write_events{0};
        size_t read_events{0};
        size_t allocation_events{0};
        size_t corruption_events{0};
        size_t tracked_addresses{0};
        size_t watch_ranges{0};
        size_t active_allocations{0};
    };
    
    Stats getStats() const {
        std::shared_lock lock(m_mutex);
        
        Stats stats;
        stats.total_events = m_total_events;
        stats.tracked_addresses = m_address_index.size();
        stats.watch_ranges = m_watch_ranges.size();
        stats.active_allocations = m_allocations.size();
        
        for (const auto& [addr, events] : m_address_index) {
            for (const auto& event : events) {
                switch (event.type) {
                    case MemoryEventType::Write: stats.write_events++; break;
                    case MemoryEventType::Read: stats.read_events++; break;
                    case MemoryEventType::Allocation: 
                    case MemoryEventType::Free:
                    case MemoryEventType::Reallocation:
                        stats.allocation_events++; break;
                    case MemoryEventType::CorruptionDetected:
                        stats.corruption_events++; break;
                    default: break;
                }
            }
        }
        
        return stats;
    }
    
    // ========================================================================
    // Export
    // ========================================================================
    
    /**
     * Export all data as JSON
     */
    std::string exportToJson() const {
        std::shared_lock lock(m_mutex);
        
        std::stringstream json;
        json << "{\n";
        json << "  \"export_timestamp\": \"" << DebugTimestamp::now().toISO8601() << "\",\n";
        json << "  \"config\": {\n";
        json << "    \"max_events_per_address\": " << m_config.max_events_per_address << ",\n";
        json << "    \"track_reads\": " << (m_config.track_reads ? "true" : "false") << "\n";
        json << "  },\n";
        
        json << "  \"watch_ranges\": [\n";
        for (size_t i = 0; i < m_watch_ranges.size(); ++i) {
            const auto& range = m_watch_ranges[i];
            json << "    {\"start\": \"0x" << std::hex << range.start 
                 << "\", \"end\": \"0x" << range.end << std::dec
                 << "\", \"description\": \"" << JsonUtils::escapeJson(range.description) << "\"}";
            if (i < m_watch_ranges.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ],\n";
        
        json << "  \"events_by_address\": {\n";
        bool first_addr = true;
        for (const auto& [addr, events] : m_address_index) {
            if (!first_addr) json << ",\n";
            first_addr = false;
            
            json << "    \"0x" << std::hex << addr << std::dec << "\": [\n";
            size_t i = 0;
            for (const auto& event : events) {
                json << "      " << event.toJson();
                if (i < events.size() - 1) json << ",";
                json << "\n";
                ++i;
            }
            json << "    ]";
        }
        json << "\n  }\n";
        
        json << "}\n";
        return json.str();
    }

private:
    Config m_config;
    std::atomic<bool> m_enabled;
    mutable std::shared_mutex m_mutex;
    
    // Storage: address -> list of events (ring buffer behavior via size limit)
    std::map<uint64_t, std::list<MemoryEvent>> m_address_index;
    
    // Event storage for export
    std::list<MemoryEvent> m_events;
    
    // Watch ranges
    std::vector<AddressRange> m_watch_ranges;
    
    // Active allocations (for auto-unwatch on free)
    std::map<uint64_t, AddressRange> m_allocations;
    
    // Counters
    size_t m_total_events;
    
    // ========================================================================
    // Internal Methods
    // ========================================================================
    
    bool shouldTrack(uint64_t address, size_t size) const {
        // Always check watched ranges first
        for (const auto& range : m_watch_ranges) {
            if (range.overlaps(address, size)) {
                return true;
            }
        }
        
        // Check if address is in a tracked allocation
        auto it = m_allocations.lower_bound(address);
        if (it != m_allocations.end()) {
            if (it->second.contains(address, size)) {
                return true;
            }
        }
        
        return false;
    }
    
    void addEvent(const MemoryEvent& event) {
        std::unique_lock lock(m_mutex);
        
        // Check global limit
        if (m_total_events >= m_config.max_total_events) {
            return;  // Or could evict oldest
        }
        
        // Add to address index
        auto& events = m_address_index[event.address];
        events.push_back(event);
        
        // Enforce per-address limit (ring buffer)
        while (events.size() > m_config.max_events_per_address) {
            events.pop_front();
        }
        
        m_total_events++;
        
        // Auto-detect corruption patterns if enabled
        if (m_config.detect_corruption_patterns &&
            event.type == MemoryEventType::Write) {
            detectCorruptionPatterns(event, events);
        }
    }
    
    std::list<MemoryEvent> getEventsForAddress(uint64_t address) const {
        auto it = m_address_index.find(address);
        if (it != m_address_index.end()) {
            return it->second;
        }
        return {};
    }
    
    void analyzeSuspicion(WriterInfo& writer) const {
        // Pattern: Writing to recently freed memory
        // Pattern: Writing outside allocation bounds
        // Pattern: GPU writing to CPU memory (or vice versa)
        // Pattern: Large value change in single write
        
        const auto& event = writer.event;
        
        // Check if writing to freed memory
        auto alloc_it = m_allocations.lower_bound(event.address);
        if (alloc_it != m_allocations.begin()) {
            --alloc_it;
            if (alloc_it != m_allocations.end()) {
                const auto& alloc = alloc_it->second;
                if (event.address >= alloc.end) {
                    writer.is_suspicious = true;
                    writer.suspicion_reason = "Write to potentially freed memory region";
                }
            }
        }
        
        // Check for large value changes
        if (event.old_value.size == event.new_value.size &&
            event.old_value.size >= 4) {
            uint64_t old_val = event.old_value.asUint64();
            uint64_t new_val = event.new_value.asUint64();
            
            // If most bits changed, might be corruption
            uint64_t xor_result = old_val ^ new_val;
            int changed_bits = __builtin_popcountll(xor_result);
            
            if (changed_bits > (sizeof(uint64_t) * 8 * 0.7)) {  // 70%+ bits changed
                writer.is_suspicious = true;
                writer.suspicion_reason = "Large value change (" + 
                    std::to_string(changed_bits) + " of " + 
                    std::to_string(sizeof(uint64_t) * 8) + " bits changed)";
            }
        }
        
        // Cross-subsystem writes can be suspicious
        if (event.subsystem == Subsystem::GPU &&
            event.address < 0x100000000ULL) {  // Low memory (likely CPU)
            writer.is_suspicious = true;
            writer.suspicion_reason = "GPU write to low memory address space";
        }
    }
    
    void detectCorruptionPatterns(const MemoryEvent& new_event,
                                   const std::list<MemoryEvent>& existing_events) {
        // Look for pattern: write, write-back different, write again
        // This can indicate race conditions or use-after-free
        
        if (existing_events.size() < 3) return;
        
        auto it = existing_events.rbegin();
        ++it;  // Skip the event we just added
        
        // Check recent writes for inconsistencies
        MemoryValue prev_value;
        int write_count = 0;
        
        for (; it != existing_events.rend() && write_count < 5; ++it) {
            if (it->type == MemoryEventType::Write) {
                write_count++;
                
                if (!prev_value.bytes.empty()) {
                    // Values oscillating between different states?
                    if (prev_value.bytes != it->new_value.bytes &&
                        it->new_value.bytes != new_event.new_value.bytes) {
                        
                        // Could be normal, flag for review
                        // (In production, would have more sophisticated heuristics)
                    }
                }
                
                prev_value = it->new_value;
            }
        }
    }
    
    static std::string getCurrentThreadId() {
        std::stringstream ss;
        ss << std::this_thread::get_id();
        return ss.str();
    }
};

} // namespace memory
} // namespace prosper_debug
