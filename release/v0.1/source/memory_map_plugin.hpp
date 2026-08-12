#pragma once
#include "../core/diagnostic_interface.hpp"
#include "../core/event_bus.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <map>

namespace prosper {
namespace diagnostics {

//=============================================================================
// Memory Map Plugin - Phase 9.5 Diagnostic Plugin
//
// Tracks memory mappings for PS4 emulator diagnostics.
// Records all allocations/mappings, tracks protection flags per region,
// detects overlapping mappings, and validates access patterns.
//=============================================================================

/**
 * @brief Represents a single memory region/mapping
 */
struct MemoryRegion {
    uint64_t base{0};                       ///< Base address of the region
    size_t size{0};                         ///< Size of the region in bytes
    MemoryProtection prot{MemoryProtection::NONE};  ///< Protection flags
    std::string owner;                      ///< What owns this region (module name, "heap", etc.)
    bool allocated{false};                  ///< Whether this region is currently allocated
    Timestamp allocated_at;                 ///< When this region was allocated
    Timestamp freed_at;                     ///< When this region was freed (min if still allocated)
    
    /// Default constructor
    MemoryRegion() : freed_at(Timestamp::min()) {}
    
    /// Full constructor for allocation
    MemoryRegion(uint64_t b, size_t s, MemoryProtection p, const std::string& o)
        : base(b), size(s), prot(p), owner(o), allocated(true),
          allocated_at(now()), freed_at(Timestamp::min()) {}
    
    /// Get end address (one past last byte)
    uint64_t end() const { return base + size; }
    
    /// Check if an address falls within this region
    bool contains(uint64_t addr) const {
        return allocated && addr >= base && addr < end();
    }
    
    /// Check if another region overlaps with this one
    bool overlaps(const MemoryRegion& other) const {
        if (!allocated || !other.allocated) return false;
        return base < other.end() && other.base < end();
    }
    
    /// Get protection flags as string
    std::string protection_string() const {
        std::string result;
        if (has_flag(prot, MemoryProtection::READ)) result += 'R';
        else result += '-';
        if (has_flag(prot, MemoryProtection::WRITE)) result += 'W';
        else result += '-';
        if (has_flag(prot, MemoryProtection::EXECUTE)) result += 'X';
        else result += '-';
        return result;
    }
    
    /// Serialize to JSON string
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"base\":\"0x" << std::hex << base << std::dec << "\",";
        oss << "\"end\":\"0x" << std::hex << end() << std::dec << "\",";
        oss << "\"size\":" << size << ",";
        oss << "\"size_kb\":" << (size / 1024) << ",";
        oss << "\"protection\":" << static_cast<int>(prot) << ",";
        oss << "\"protection_str\":\"" << protection_string() << "\",";
        oss << "\"owner\":\"" << owner << "\",";
        oss << "\"allocated\":" << (allocated ? "true" : "false") << ",";
        
        if (allocated_at.time_since_epoch().count() != 0) {
            oss << "\"allocated_ms\":" << std::fixed << std::setprecision(3)
                << timestamp_to_ms(allocated_at) << ",";
        } else {
            oss << "\"allocated_ms\":null,";
        }
        
        if (freed_at.time_since_epoch().count() != 0) {
            oss << "\"freed_ms\":" << std::fixed << std::setprecision(3)
                << timestamp_to_ms(freed_at);
        } else {
            oss << "\"freed_ms\":null";
        }
        
        oss << "}";
        return oss.str();
    }
};

/**
 * @brief Result of a memory access validation check
 */
struct AccessValidationResult {
    bool valid{true};
    uint64_t address{0};
    size_t access_size{0};
    bool read_access{true};
    bool write_access{false};
    bool execute_access{false};
    ViolationType violation{ViolationType::PERMISSION_VIOLATION};
    const MemoryRegion* region{nullptr};
    std::string message;
    
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"valid\":" << (valid ? "true" : "false") << ",";
        oss << "\"address\":\"0x" << std::hex << address << std::dec << "\",";
        oss << "\"access_size\":" << access_size << ",";
        oss << "\"read_access\":" << (read_access ? "true" : "false") << ",";
        oss << "\"write_access\":" << (write_access ? "true" : "false") << ",";
        oss << "\"execute_access\":" << (execute_access ? "true" : "false") << ",";
        
        if (!valid) {
            oss << "\"violation\":" << static_cast<int>(violation) << ",";
            oss << "\"message\":\"" << message << "\"";
        } else {
            oss << "\"region_owner\":\"" << (region ? region->owner : "unknown") << "\"";
        }
        
        oss << "}";
        return oss.str();
    }
};

/**
 * @brief Overlap detection result
 */
struct OverlapDetectionResult {
    bool has_overlaps{false};
    size_t overlap_count{0};
    struct OverlapInfo {
        uint64_t address;
        const MemoryRegion* region1;
        const MemoryRegion* region2;
        size_t overlap_size;
    };
    std::vector<OverlapInfo> overlaps;
    
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"has_overlaps\":" << (has_overlaps ? "true" : "false") << ",";
        oss << "\"overlap_count\":" << overlap_count << ",";
        
        oss << "\"overlaps\":[";
        for (size_t i = 0; i < overlaps.size(); ++i) {
            oss << "{";
            oss << "\"address\":\"0x" << std::hex << overlaps[i].address << std::dec << "\",";
            oss << "\"region1\":\"" << (overlaps[i].region1 ? overlaps[i].region1->owner : "?") << "\",";
            oss << "\"region2\":\"" << (overlaps[i].region2 ? overlaps[i].region2->owner : "?") << "\",";
            oss << "\"overlap_size\":" << overlaps[i].overlap_size;
            oss << "}";
            if (i < overlaps.size() - 1) oss << ",";
        }
        oss << "]";
        
        oss << "}";
        return oss.str();
    }
};

/**
 * @brief Memory Map Plugin implementation
 * 
 * This plugin provides comprehensive memory tracking:
 * - Records all memory allocations and mappings with full metadata
 * - Tracks protection flags (R/W/X) per region
 * - Detects overlapping or conflicting mappings
 * - Validates memory accesses against known regions
 * - Generates detailed memory layout reports
 */
class MemoryMapPlugin : public DiagnosticPlugin {
public:
    //=========================================================================
    // Constructor / Destructor
    //=========================================================================
    
    MemoryMapPlugin()
        : total_allocated_bytes_(0),
          total_regions_created_(0),
          max_events_(10000),
          detect_overlaps_(true) {}
    
    virtual ~MemoryMapPlugin() override {
        shutdown();
    }
    
    //=========================================================================
    // DiagnosticPlugin Interface Implementation
    //=========================================================================
    
    std::string name() const override { return "MemoryMap"; }
    
    std::string version() const override { return "1.5.0"; }
    
    std::string description() const override {
        return "Tracks memory mappings, protection, and validates access patterns";
    }
    
    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        regions_.clear();
        addr_index_.clear();
        total_allocated_bytes_ = 0;
        total_regions_created_ = 0;
        
        active_ = true;
        
        // Parse configuration
        auto it = config_.find("max_events");
        if (it != config_.end()) {
            try { max_events_ = static_cast<size_t>(std::stoull(it->second)); } catch (...) {}
        }
        
        it = config_.find("detect_overlaps");
        if (it != config_.end()) {
            detect_overlaps_ = (it->second == "true" || it->second == "1");
        }
        
        emit_info("MemoryMap initialized successfully");
        return true;
    }
    
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        active_ = false;
        emit_info("MemoryMap shut down");
    }
    
    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        regions_.clear();
        addr_index_.clear();
        total_allocated_bytes_ = 0;
        total_regions_created_ = 0;
        event_count_ = 0;
    }
    
    void on_event(const DiagnosticEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) return;
        if (event_count_ >= max_events_) return;
        
        // Process memory-related events
        if (event.event_type == "memory_mapped" || event.event_type == "memory_allocated") {
            auto base_it = event.numeric_data.find("base_addr");
            auto size_it = event.numeric_data.find("size");
            
            if (base_it != event.numeric_data.end() && size_it != event.numeric_data.end()) {
                uint64_t base = static_cast<uint64_t>(base_it->second);
                size_t size = static_cast<size_t>(size_it->second);
                
                MemoryProtection prot = MemoryProtection::RWX;
                auto prot_it = event.numeric_data.find("protection");
                if (prot_it != event.numeric_data.end()) {
                    prot = static_cast<MemoryProtection>(prot_it->second);
                }
                
                std::string owner = "unknown";
                auto owner_it = event.metadata.find("owner");
                if (owner_it != event.metadata.end()) {
                    owner = owner_it->second;
                }
                
                on_map_allocated_internal(base, size, prot, owner, event.timestamp);
            }
        }
        else if (event.event_type == "memory_unmapped" || event.event_type == "memory_freed") {
            auto base_it = event.numeric_data.find("base_addr");
            if (base_it != event.numeric_data.end()) {
                on_map_freed_internal(static_cast<uint64_t>(base_it->second), event.timestamp);
            }
        }
        
        event_count_++;
    }
    
    std::string generate_report() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return generate_report_internal();
    }
    
    void export_json(const std::string& path) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ofstream file(path);
        if (!file.is_open()) return;
        
        file << generate_report_internal();
        file.close();
    }
    
    //=========================================================================
    // Public API Methods
    //=========================================================================
    
    /**
     * @brief Record that a memory region was allocated/mapped
     * @param base Base address of the new mapping
     * @param size Size of the region in bytes
     * @param prot Protection flags for the region
     * @param owner Owner identifier (module name, "heap", "stack", etc.)
     * @param timestamp Optional timestamp (defaults to now)
     */
    void on_map_allocated(uint64_t base,
                          size_t size,
                          MemoryProtection prot = MemoryProtection::RW,
                          const std::string& owner = "unknown",
                          Timestamp timestamp = Timestamp{}) {
        std::lock_guard<std::mutex> lock(mutex_);
        on_map_allocated_internal(base, size, prot, owner,
                                  timestamp.time_since_epoch().count() == 0 ? now() : timestamp);
    }
    
    /**
     * @brief Record that a memory region was freed/unmapped
     * @param base Base address of the region to free
     * @param timestamp Optional timestamp (defaults to now)
     */
    void on_map_freed(uint64_t base, Timestamp timestamp = Timestamp{}) {
        std::lock_guard<std::mutex> lock(mutex_);
        on_map_freed_internal(base,
                              timestamp.time_since_epoch().count() == 0 ? now() : timestamp);
    }
    
    /**
     * @brief Validate a memory access against known regions
     * @param address Address being accessed
     * @param size Size of the access in bytes
     * @param read Whether this is a read access
     * @param write Whether this is a write access
     * @param execute Whether this is an execute access
     * @return AccessValidationResult with details about validity
     */
    AccessValidationResult validate_access(uint64_t address,
                                           size_t size = 1,
                                           bool read = true,
                                           bool write = false,
                                           bool execute = false) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return validate_access_internal(address, size, read, write, execute);
    }
    
    /**
     * @brief Get information about a specific memory region by address
     * @param address Any address within the region
     * @return Pointer to MemoryRegion or nullptr if not found
     */
    const MemoryRegion* get_region_info(uint64_t address) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return find_region_containing(address);
    }
    
    /**
     * @brief Find region by exact base address
     * @param base Base address to look up
     * @return Pointer to MemoryRegion or nullptr if not found
     */
    const MemoryRegion* get_region_by_base(uint64_t base) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = addr_index_.find(base);
        if (it != addr_index_.end()) {
            return &regions_[it->second];
        }
        return nullptr;
    }
    
    /**
     * @brief Detect overlapping memory regions
     * @return OverlapDetectionResult with details about any overlaps found
     */
    OverlapDetectionResult detect_overlaps() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return detect_overlaps_internal();
    }
    
    /**
     * @brief Get all currently allocated regions
     */
    std::vector<MemoryRegion> get_allocated_regions() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<MemoryRegion> result;
        for (const auto& region : regions_) {
            if (region.allocated) {
                result.push_back(region);
            }
        }
        return result;
    }
    
    /**
     * @brief Get total currently allocated memory in bytes
     */
    size_t total_allocated_memory() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_allocated_bytes_;
    }
    
    /**
     * @brief Get number of currently active regions
     */
    size_t active_region_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t count = 0;
        for (const auto& region : regions_) {
            if (region.allocated) count++;
        }
        return count;
    }
    
    /**
     * @brief Update protection flags for a region
     * @param base Base address of the region
     * @param new_prot New protection flags
     * @return True if update succeeded
     */
    bool update_protection(uint64_t base, MemoryProtection new_prot) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = addr_index_.find(base);
        if (it != addr_index_.end() && regions_[it->second].allocated) {
            regions_[it->second].prot = new_prot;
            return true;
        }
        return false;
    }
    
    /**
     * @brief Check if an address is within any allocated region
     */
    bool is_address_mapped(uint64_t address) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return find_region_containing(address) != nullptr;
    }

private:
    //=========================================================================
    // Internal Implementation Methods
    //=========================================================================
    
    /**
     * @ Internal map allocation handler (assumes lock held)
     */
    void on_map_allocated_internal(uint64_t base,
                                   size_t size,
                                   MemoryProtection prot,
                                   const std::string& owner,
                                   Timestamp timestamp) {
        // Check for existing region at this address
        auto existing = addr_index_.find(base);
        if (existing != addr_index_.end()) {
            // Update existing region
            auto& region = regions_[existing->second];
            
            // Adjust totals if previously allocated
            if (region.allocated && total_allocated_bytes_ >= region.size) {
                total_allocated_bytes_ -= region.size;
            }
            
            region.base = base;
            region.size = size;
            region.prot = prot;
            region.owner = owner;
            region.allocated = true;
            region.allocated_at = timestamp;
            region.freed_at = Timestamp::min();
            
            total_allocated_bytes_ += size;
            
            emit_warning("Region re-allocated at 0x" + addr_to_hex(base));
            
            // Check for overlaps if enabled
            if (detect_overlaps_) {
                check_and_emit_overlaps(region);
            }
            return;
        }
        
        // Create new region
        MemoryRegion region(base, size, prot, owner);
        region.allocated_at = timestamp;
        
        size_t index = regions_.size();
        regions_.push_back(std::move(region));
        addr_index_[base] = index;
        
        total_allocated_bytes_ += size;
        total_regions_created_++;
        
        // Emit diagnostic event
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "region_tracked";
        event.severity = Severity::INFO;
        event.message = "Memory mapped: 0x" + addr_to_hex(base) + 
                        " size=" + std::to_string(size) + " owner=" + owner;
        event.metadata["owner"] = owner;
        event.numeric_data["base_addr"] = static_cast<int64_t>(base);
        event.numeric_data["size"] = static_cast<int64_t>(size);
        event.numeric_data["total_allocated"] = static_cast<int64_t>(total_allocated_bytes_);
        event.numeric_data["region_count"] = static_cast<int64_t>(active_region_count_locked());
        
        emit_event(event);
        
        // Check for overlaps if enabled
        if (detect_overlaps_) {
            check_and_emit_overlaps(regions_[index]);
        }
    }
    
    /**
     * @brief Internal map free handler (assumes lock held)
     */
    void on_map_freed_internal(uint64_t base, Timestamp timestamp) {
        auto it = addr_index_.find(base);
        if (it == addr_index_.end()) {
            emit_warning("Attempted to free unknown region: 0x" + addr_to_hex(base));
            return;
        }
        
        auto& region = regions_[it->second];
        if (!region.allocated) {
            emit_warning("Attempted to free already-freed region: 0x" + addr_to_hex(base));
            return;
        }
        
        region.allocated = false;
        region.freed_at = timestamp;
        
        if (total_allocated_bytes_ >= region.size) {
            total_allocated_bytes_ -= region.size;
        }
        
        // Emit diagnostic event
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "region_freed";
        event.severity = Severity::INFO;
        event.message = "Memory freed: 0x" + addr_to_hex(base) + " owner=" + region.owner;
        event.metadata["owner"] = region.owner;
        event.numeric_data["base_addr"] = static_cast<int64_t>(base);
        event.numeric_data["size"] = static_cast<int64_t>(region.size);
        event.numeric_data["total_allocated"] = static_cast<int64_t>(total_allocated_bytes_);
        
        emit_event(event);
    }
    
    /**
     * @brief Validate memory access (assumes lock held)
     */
    AccessValidationResult validate_access_internal(uint64_t address,
                                                    size_t size,
                                                    bool read,
                                                    bool write,
                                                    bool execute) const {
        AccessValidationResult result;
        result.address = address;
        result.access_size = size;
        result.read_access = read;
        result.write_access = write;
        result.execute_access = execute;
        
        // Find containing region
        const MemoryRegion* region = find_region_containing(address);
        
        if (!region) {
            result.valid = false;
            result.violation = ViolationType::BOUNDARY_VIOLATION;
            result.message = "Address 0x" + addr_to_hex(address) + 
                            " is not within any mapped region";
            return result;
        }
        
        result.region = region;
        
        // Check if access extends beyond region
        if (address + size > region->end()) {
            result.valid = false;
            result.violation = ViolationType::BOUNDARY_VIOLATION;
            result.message = "Access at 0x" + addr_to_hex(address) + 
                            " with size " + std::to_string(size) +
                            " extends beyond region end 0x" + addr_to_hex(region->end());
            return result;
        }
        
        // Check alignment (optional warning)
        if ((address & 0x7) != 0 && size >= 8) {
            // Not critical, just note it
        }
        
        // Check permission violations
        if (read && !has_flag(region->prot, MemoryProtection::READ)) {
            result.valid = false;
            result.violation = ViolationType::READ_VIOLATION;
            result.message = "Read access to non-readable region owned by " + region->owner;
            return result;
        }
        
        if (write && !has_flag(region->prot, MemoryProtection::WRITE)) {
            result.valid = false;
            result.violation = ViolationType::WRITE_VIOLATION;
            result.message = "Write access to non-writable region owned by " + region->owner;
            return result;
        }
        
        if (execute && !has_flag(region->prot, MemoryProtection::EXECUTE)) {
            result.valid = false;
            result.violation = ViolationType::EXECUTE_VIOLATION;
            result.message = "Execute access to non-executable region owned by " + region->owner;
            return result;
        }
        
        result.valid = true;
        return result;
    }
    
    /**
     * @brief Find region containing an address (assumes lock held)
     */
    const MemoryRegion* find_region_containing(uint64_t address) const {
        // Simple linear search - could be optimized with interval tree for large numbers of regions
        for (const auto& region : regions_) {
            if (region.allocated && region.contains(address)) {
                return &region;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Detect overlapping regions (assumes lock held)
     */
    OverlapDetectionResult detect_overlaps_internal() const {
        OverlapDetectionResult result;
        
        // Compare all pairs of allocated regions
        for (size_t i = 0; i < regions_.size(); ++i) {
            if (!regions_[i].allocated) continue;
            
            for (size_t j = i + 1; j < regions_.size(); ++j) {
                if (!regions_[j].allocated) continue;
                
                if (regions_[i].overlaps(regions_[j])) {
                    result.has_overlaps = true;
                    result.overlap_count++;
                    
                    uint64_t overlap_start = std::max(regions_[i].base, regions_[j].base);
                    uint64_t overlap_end = std::min(regions_[i].end(), regions_[j].end());
                    
                    OverlapDetectionResult::OverlapInfo info;
                    info.address = overlap_start;
                    info.region1 = &regions_[i];
                    info.region2 = &regions_[j];
                    info.overlap_size = static_cast<size_t>(overlap_end - overlap_start);
                    
                    result.overlaps.push_back(info);
                }
            }
        }
        
        return result;
    }
    
    /**
     * @brief Check for overlaps with a new region and emit events
     */
    void check_and_emit_overlaps(const MemoryRegion& new_region) {
        for (const auto& region : regions_) {
            if (&region == &new_region) continue;
            if (!region.allocated) continue;
            
            if (new_region.overlaps(region)) {
                uint64_t overlap_start = std::max(new_region.base, region.base);
                uint64_t overlap_end = std::min(new_region.end(), region.end);
                
                DiagnosticEvent event;
                event.source_plugin = name();
                event.event_type = "overlap_detected";
                event.severity = Severity::WARNING;
                event.message = "Memory overlap detected between " + new_region.owner +
                                " and " + region.owner + " at 0x" + addr_to_hex(overlap_start);
                event.metadata["owner1"] = new_region.owner;
                event.metadata["owner2"] = region.owner;
                event.numeric_data["overlap_address"] = static_cast<int64_t>(overlap_start);
                event.numeric_data["overlap_size"] = static_cast<int64_t>(overlap_end - overlap_start);
                
                emit_event(event);
            }
        }
    }
    
    /**
     * @brief Generate comprehensive report (assumes lock held)
     */
    std::string generate_report_internal() const {
        std::ostringstream json;
        
        json << "{\n";
        json << "  \"plugin\": \"" << name() << "\",\n";
        json << "  \"version\": \"" << version() << "\",\n";
        json << "  \"generated_at_ms\": " << std::fixed << std::setprecision(3)
             << timestamp_to_ms(now()) << ",\n";
        
        // Statistics
        json << "  \"statistics\": {\n";
        json << "    \"total_regions_created\": " << total_regions_created_ << ",\n";
        json << "    \"active_regions\": " << active_region_count_locked() << ",\n";
        json << "    \"total_allocated_bytes\": " << total_allocated_bytes_ << ",\n";
        json << "    \"total_allocated_mb\": " << std::fixed << std::setprecision(2)
             << (total_allocated_bytes_ / (1024.0 * 1024.0)) << "\n";
        json << "  },\n";
        
        // Regions array
        json << "  \"regions\": [\n";
        size_t exported = 0;
        for (size_t i = 0; i < regions_.size() && exported < 2000; ++i) {
            if (regions_[i].allocated) {
                json << "    " << regions_[i].to_json();
                if (exported < active_region_count_locked() - 1 && exported < 1999) json << ",";
                json << "\n";
                exported++;
            }
        }
        if (active_region_count_locked() > 2000) {
            json << "    ,\"... (" << (active_region_count_locked() - 2000) << " more regions truncated)\"\n";
        }
        json << "  ],\n";
        
        // Overlap detection
        json << "  \"overlap_detection\": " << detect_overlaps_internal().to_json() << "\n";
        
        json << "}\n";
        
        return json.str();
    }
    
    /**
     * @brief Active region count (assumes lock held)
     */
    size_t active_region_count_locked() const {
        size_t count = 0;
        for (const auto& region : regions_) {
            if (region.allocated) count++;
        }
        return count;
    }
    
    /**
     * @brief Convert address to hex string helper
     */
    static std::string addr_to_hex(uint64_t addr) {
        std::ostringstream oss;
        oss << std::hex << std::setw(16) << std::setfill('0') << addr;
        return oss.str();
    }
    
    /**
     * @brief Emit an info-level diagnostic event
     */
    void emit_info(const std::string& message) {
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "info";
        event.severity = Severity::INFO;
        event.message = message;
        emit_event(event);
    }
    
    /**
     * @brief Emit a warning diagnostic event
     */
    void emit_warning(const std::string& message) {
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "warning";
        event.severity = Severity::WARNING;
        event.message = message;
        emit_event(event);
    }
    
    //=========================================================================
    // Member Variables
    //=========================================================================
    
    std::vector<MemoryRegion> regions_;              ///< All tracked regions (including freed)
    std::unordered_map<uint64_t, size_t> addr_index_; ///< Base address -> index in regions_
    
    size_t total_allocated_bytes_;
    size_t total_regions_created_;
    size_t max_events_;
    bool detect_overlaps_;
};

} // namespace diagnostics
} // namespace prosper
