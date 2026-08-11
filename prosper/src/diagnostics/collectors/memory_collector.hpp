// diagnostics/collectors/memory_collector.hpp — Memory operation diagnostics
//
#pragma once
#include "collector.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace prosper {
namespace diagnostics {

enum class MemoryOperation : uint8_t {
    ALLOC,
    FREE,
    MAP,
    UNMAP,
    PROTECT,  // Protection change
    ACCESS_VIOLATION,  // Invalid access detected
};

struct MemoryEvent {
    MemoryOperation  operation;
    uint64_t        address = 0;
    uint64_t        size = 0;
    uint32_t        permissions = 0;  // R/W/X
    bool            success = true;
    std::string     error_message;
    std::string     region_name;     // e.g., "heap", "stack", "module text"
};

struct MemoryRegion {
    uint64_t        base = 0;
    uint64_t        size = 0;
    uint32_t        permissions = 0;
    std::string     name;
    std::string     owner;  // Which module/owns this
};

class MemoryCollector : public Collector {
public:
    MemoryCollector() = default;
    
    const char* name() const override { return "memory"; }
    Subsystem subsystem() const override { return Subsystem::MEMORY; }
    
    bool initialize() override {
        events_.clear();
        regions_.clear();
        total_allocated_ = 0;
        total_freed_ = 0;
        violation_count_ = 0;
        return true;
    }
    
    // --- Recording Interface -------------------------------------------------
    
    void record_alloc(uint64_t addr, uint64_t size, const std::string& region = "");
    void record_free(uint64_t addr, bool success = true);
    void record_map(uint64_t addr, uint64_t size, uint32_t perms, 
                   const std::string& name = "");
    void record_unmap(uint64_t addr, uint64_t size);
    void record_protect_change(uint64_t addr, uint64_t size, 
                               uint32_t old_perms, uint32_t new_perms);
    void record_violation(uint64_t addr, uint32_t access_type, 
                          const std::string& context = "");
    
    // Register a known memory region
    void add_region(const MemoryRegion& region);
    
    // --- Query Interface ----------------------------------------------------
    
    const std::vector<MemoryEvent>& events() const { return events_; }
    const std::vector<MemoryRegion>& regions() const { return regions_; }
    
    uint64_t total_allocated() const { return total_allocated_; }
    uint64_t total_freed() const { return total_freed_; }
    uint64_t current_usage() const { return total_allocated_ - total_freed_; }
    uint64_t violation_count() const { return violation_count_; }
    
    // Find which region contains an address
    const MemoryRegion* find_region(uint64_t addr) const;
    
    std::string generate_report() const override;

private:
    std::vector<MemoryEvent> events_;
    std::vector<MemoryRegion> regions_;
    uint64_t total_allocated_ = 0;
    uint64_t total_freed_ = 0;
    uint64_t violation_count_ = 0;
    
    const char* op_string(MemoryOperation op) const;
    std::string event_to_json(const MemoryEvent& e) const;
};

inline const char* MemoryCollector::op_string(MemoryOperation op) const {
    switch (op) {
        case MemoryOperation::ALLOC:           return "ALLOC";
        case MemoryOperation::FREE:            return "FREE";
        case MemoryOperation::MAP:             return "MAP";
        case MemoryOperation::UNMAP:           return "UNMAP";
        case MemoryOperation::PROTECT:         return "PROTECT";
        case MemoryOperation::ACCESS_VIOLATION: return "VIOLATION";
    }
    return "UNKNOWN";
}

} // namespace diagnostics
} // namespace prosper
