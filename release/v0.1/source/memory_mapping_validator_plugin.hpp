#pragma once
#include "../core/diagnostic_interface.hpp"
#include "../core/event_bus.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <algorithm>
#include <set>

namespace prosper {
namespace diagnostics {

//=============================================================================
// Memory Mapping Validator Plugin - Phase 10 Tier 2 Diagnostic Plugin
//
// Pre-access validation for memory operations.
//
// Validates memory operations BEFORE they happen to detect violations early,
// preventing crashes and providing better error messages than raw SIGSEGV.
//
// Checks performed:
// - Region exists (mapped)
// - Permissions allow operation
// - Alignment is correct for operation type
// - Access is within region bounds (no overflow)
//
// Design Decisions:
// - Validation is opt-in (expensive, off by default in production)
// - All validation methods are const-correct where possible
// - Violations are logged with full context for debugging
// - Thread-safe: uses mutex for region map access
// - Supports overlapping regions (first match wins)
//=============================================================================

/**
 * @brief Result of a memory access validation check
 * 
 * Contains detailed information about whether an access is valid,
 * what type of violation occurred if any, and suggested fixes.
 */
struct ValidationResult {
    bool valid{true};                           ///< True if access would succeed
    ViolationType violation_type{ViolationType::READ_VIOLATION};  ///< Type of violation (if invalid)
    std::string message;                        ///< Human-readable description
    uint64_t address{0};                        ///< Address being accessed
    size_t access_size{1};                      ///< Size of access in bytes
    MemoryProtection required_prot{MemoryProtection::READ};  ///< Protection needed
    MemoryProtection actual_prot{MemoryProtection::NONE};     ///< Protection at address
    std::string region_owner;                   ///< Name of owning module/region
    
    bool would_crash{false};                    ///< Would this cause SIGSEGV?
    
    /// Suggested fix or workaround
    std::string suggestion;
    
    /// Default constructor - valid result
    ValidationResult() = default;
    
    /**
     * @brief Create a valid result
     */
    static ValidationResult ok(uint64_t addr, size_t size) {
        ValidationResult r;
        r.valid = true;
        r.address = addr;
        r.access_size = size;
        return r;
    }
    
    /**
     * @brief Create a violation result
     */
    static ValidationResult violation(ViolationType type, uint64_t addr, size_t size,
                                       MemoryProtection required, MemoryProtection actual,
                                       const std::string& owner, bool crash = false) {
        ValidationResult r;
        r.valid = false;
        r.violation_type = type;
        r.address = addr;
        r.access_size = size;
        r.required_prot = required;
        r.actual_prot = actual;
        r.region_owner = owner;
        r.would_crash = crash;
        
        // Generate appropriate message and suggestion
        generate_violation_details(r);
        
        return r;
    }
    
    /**
     * @brief Convert to JSON representation
     */
    std::string to_json() const {
        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"valid\": " << (valid ? "true" : "false") << ",\n";
        ss << "  \"address\": \"0x" << std::hex << address << std::dec << "\",\n";
        ss << "  \"access_size\": " << access_size << ",\n";
        
        if (!valid) {
            ss << "  \"violation_type\": " << static_cast<int>(violation_type) << ",\n";
            ss << "  \"violation_name\": \"" << violation_to_string(violation_type) << "\",\n";
            ss << "  \"message\": \"" << escape_json(message) << "\",\n";
            ss << "  \"required_protection\": " << static_cast<int>(required_prot) << ",\n";
            ss << "  \"actual_protection\": " << static_cast<int>(actual_prot) << ",\n";
            ss << "  \"region_owner\": \"" << escape_json(region_owner) << "\",\n";
            ss << "  \"would_crash\": " << (would_crash ? "true" : "false") << ",\n";
            ss << "  \"suggestion\": \"" << escape_json(suggestion) << "\"\n";
        } else {
            ss << "  \"required_protection\": " << static_cast<int>(required_prot) << "\n";
        }
        
        ss << "}";
        return ss.str();
    }
    
private:
    static std::string violation_to_string(ViolationType v) {
        switch (v) {
            case ViolationType::WRITE_VIOLATION:      return "WRITE_VIOLATION";
            case ViolationType::EXECUTE_VIOLATION:    return "EXECUTE_VIOLATION";
            case ViolationType::READ_VIOLATION:       return "READ_VIOLATION";
            case ViolationType::ALIGNMENT_VIOLATION:  return "ALIGNMENT_VIOLATION";
            case ViolationType::BOUNDARY_VIOLATION:   return "BOUNDARY_VIOLATION";
            case ViolationType::PERMISSION_VIOLATION: return "PERMISSION_VIOLATION";
            default: return "UNKNOWN";
        }
    }
    
    static void generate_violation_details(ValidationResult& r) {
        switch (r.violation_type) {
            case ViolationType::WRITE_VIOLATION:
                r.message = "Write access to non-writable or unmapped memory at 0x" + 
                           to_hex(r.address);
                r.suggestion = "Ensure target region has WRITE permission. "
                              "Check if address is within mapped region bounds.";
                break;
                
            case ViolationType::EXECUTE_VIOLATION:
                r.message = "Execute from non-executable memory at 0x" + 
                           to_hex(r.address);
                r.suggestion = "Ensure code region has EXECUTE permission. "
                              "Check if address falls within executable segment.";
                break;
                
            case ViolationType::READ_VIOLATION:
                r.message = "Read from unmapped memory at 0x" + to_hex(r.address);
                r.suggestion = "Verify address is within mapped region. "
                              "Possible null/invalid pointer dereference.";
                break;
                
            case ViolationType::ALIGNMENT_VIOLATION:
                r.message = "Unaligned access at 0x" + to_hex(r.address) + 
                           " (size: " + std::to_string(r.access_size) + ")";
                r.suggestion = "Align access to " + std::to_string(r.access_size) + 
                              "-byte boundary. Use aligned load/store instructions.";
                break;
                
            case ViolationType::BOUNDARY_VIOLATION:
                r.message = "Access crosses region boundary at 0x" + to_hex(r.address) +
                           " (size: " + std::to_string(r.access_size) + ")";
                r.suggestion = "Split access or ensure buffer doesn't cross page/region boundary.";
                break;
                
            case ViolationType::PERMISSION_VIOLATION:
                r.message = "Permission violation at 0x" + to_hex(r.address) + 
                           ": requires " + prot_to_string(r.required_prot) +
                           " but has " + prot_to_string(r.actual_prot);
                r.suggestion = "Adjust region permissions or use appropriate access type.";
                break;
        }
    }
    
    static std::string to_hex(uint64_t v) {
        std::ostringstream ss;
        ss << std::hex << v;
        return ss.str();
    }
    
    static std::string prot_to_string(MemoryProtection p) {
        std::string s;
        if (has_flag(p, MemoryProtection::READ)) s += "R";
        if (has_flag(p, MemoryProtection::WRITE)) s += "W";
        if (has_flag(p, MemoryProtection::EXECUTE)) s += "X";
        if (s.empty()) s = "NONE";
        return s;
    }
    
    static std::string escape_json(const std::string& s) {
        std::string result;
        result.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:   result += c; break;
            }
        }
        return result;
    }
};

/**
 * @brief Aggregated statistics for validation operations
 */
struct ValidationStats {
    size_t total_validations{0};                 ///< Total number of validations performed
    size_t violations_detected{0};               ///< Total violations found
    
    /// Breakdown by violation type
    std::map<ViolationType, size_t> violations_by_type;
    
    /// Breakdown by region owner
    std::map<std::string, size_t> violations_by_region;
    
    /// Estimated crashes prevented
    size_t prevented_crashes{0};
    
    /**
     * @brief Convert stats to JSON
     */
    std::string to_json() const {
        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"total_validations\": " << total_validations << ",\n";
        ss << "  \"violations_detected\": " << violations_detected << ",\n";
        ss << "  \"prevented_crashes\": " << prevented_crashes << ",\n";
        ss << "  \"violation_rate\": " << std::fixed << std::setprecision(4);
        if (total_validations > 0) {
            ss << (static_cast<double>(violations_detected) / total_validations);
        } else {
            ss << "0.0";
        }
        ss << ",\n";
        
        ss << "  \"violations_by_type\": {\n";
        bool first = true;
        for (const auto& [type, count] : violations_by_type) {
            if (!first) ss << ",\n";
            ss << "    \"" << violation_name(type) << "\": " << count;
            first = false;
        }
        ss << "\n  },\n";
        
        ss << "  \"violations_by_region\": {\n";
        first = true;
        for (const auto& [region, count] : violations_by_region) {
            if (!first) ss << ",\n";
            ss << "    \"" << region << "\": " << count;
            first = false;
        }
        ss << "\n  }\n";
        
        ss << "}";
        return ss.str();
    }
    
private:
    static std::string violation_name(ViolationType v) {
        switch (v) {
            case ViolationType::WRITE_VIOLATION:      return "WRITE_VIOLATION";
            case ViolationType::EXECUTE_VIOLATION:    return "EXECUTE_VIOLATION";
            case ViolationType::READ_VIOLATION:       return "READ_VIOLATION";
            case ViolationType::ALIGNMENT_VIOLATION:  return "ALIGNMENT_VIOLATION";
            case ViolationType::BOUNDARY_VIOLATION:   return "BOUNDARY_VIOLATION";
            case ViolationType::PERMISSION_VIOLATION: return "PERMISSION_VIOLATION";
            default: return "UNKNOWN";
        }
    }
};

//=============================================================================
// Memory Region Definition (Internal)
//=============================================================================

/**
 * @brief Represents a mapped memory region for validation
 */
struct MemoryRegion {
    uint64_t base_address;                         ///< Start address (inclusive)
    size_t size;                                   ///< Region size in bytes
    MemoryProtection protection;                   ///< Access permissions
    std::string owner;                             ///< Owner identifier (module name, etc.)
    Timestamp created_at;                          ///< When region was registered
    
    uint64_t end_address() const { return base_address + size - 1; }
    
    /**
     * @brief Check if address falls within this region
     */
    bool contains(uint64_t addr) const {
        return addr >= base_address && addr <= end_address();
    }
    
    /**
     * @brief Check if range [addr, addr+size-1] fits entirely in region
     */
    bool contains_range(uint64_t addr, size_t range_size) const {
        return contains(addr) && (addr + range_size - 1) <= end_address();
    }
};

//=============================================================================
// Memory Mapping Validator Plugin Implementation
//=============================================================================

class MemoryMappingValidatorPlugin : public DiagnosticPlugin {
public:
    //=========================================================================
    // Construction / Destruction
    //=========================================================================
    
    MemoryMappingValidatorPlugin() : DiagnosticPlugin() {
        active_ = false;
    }
    
    ~MemoryMappingValidatorPlugin() override {
        shutdown();
    }
    
    //=========================================================================
    // DiagnosticPlugin Interface Implementation
    //=========================================================================
    
    std::string name() const override {
        return "memory_mapping_validator";
    }
    
    std::string description() const override {
        return "Pre-access memory operation validator that detects violations "
               "(unmapped, wrong permissions, alignment issues, boundary overflows) "
               "before they cause crashes in PS4 emulation.";
    }
    
    std::string version() const override {
        return "2.0.0";
    }
    
    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            regions_.clear();
            recent_violations_.clear();
            stats_ = ValidationStats{};
            
            active_ = true;
            return true;
        } catch (...) {
            active_ = false;
            return false;
        }
    }
    
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        active_ = false;
        regions_.clear();
    }
    
    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        regions_.clear();
        recent_violations_.clear();
        stats_ = ValidationStats{};
    }
    
    void on_event(const DiagnosticEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) return;
        
        // Track region mapping/unmapping events
        if (event.event_type == "region_mapped" || event.event_type == "memory_mapped") {
            if (event.numeric_data.count("base") && event.numeric_data.count("size")) {
                uint64_t base = static_cast<uint64_t>(event.numeric_data.at("base"));
                size_t size = static_cast<size_t>(event.numeric_data.at("size"));
                MemoryProtection prot = MemoryProtection::RWX;
                
                if (event.numeric_data.count("protection")) {
                    prot = static_cast<MemoryProtection>(
                        event.numeric_data.at("protection"));
                }
                
                std::string owner = event.metadata.count("owner") ?
                                   event.metadata.at("owner") : "unknown";
                
                register_region_internal(base, size, prot, owner);
            }
        } else if (event.event_type == "region_unmapped" || event.event_type == "memory_unmapped") {
            if (event.numeric_data.count("base")) {
                uint64_t base = static_cast<uint64_t>(event.numeric_data.at("base"));
                unregister_region_internal(base);
            }
        }
        
        event_count_++;
    }
    
    std::string generate_report() const override {
        return generate_validation_report();
    }
    
    void export_json(const std::string& path) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            std::ofstream file(path);
            if (file.is_open()) {
                file << generate_validation_report();
                file.close();
            }
        } catch (...) {
            // Silently handle errors
        }
    }
    
    //=========================================================================
    // Core Validation Methods
    //=========================================================================
    
    /**
     * @brief Validate a read operation before execution
     * @param addr Target address
     * @param size Number of bytes to read
     * @param alignment Required alignment (default: 1 = byte-aligned)
     * @return Validation result with details
     * 
     * Checks:
     * 1. Address is within a mapped region
     * 2. Region has READ permission
     * 3. Access is properly aligned
     * 4. Full range stays within region bounds
     */
    ValidationResult validate_read(uint64_t addr, size_t size = 1, size_t alignment = 1) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) {
            return ValidationResult::ok(addr, size);
        }
        
        stats_.total_validations++;
        
        // Check alignment first (fast fail)
        if (alignment > 1 && (addr % alignment != 0)) {
            return record_violation(ValidationResult::violation(
                ViolationType::ALIGNMENT_VIOLATION, addr, size,
                MemoryProtection::READ, MemoryProtection::NONE,
                "", true  // Unaligned access can crash on some architectures
            ));
        }
        
        // Find containing region
        const MemoryRegion* region = find_region(addr);
        
        if (!region) {
            return record_violation(ValidationResult::violation(
                ViolationType::READ_VIOLATION, addr, size,
                MemoryProtection::READ, MemoryProtection::NONE,
                "", true  // Reading unmapped = SIGSEGV
            ));
        }
        
        // Check bounds
        if (!region->contains_range(addr, size)) {
            return record_violation(ValidationResult::violation(
                ViolationType::BOUNDARY_VIOLATION, addr, size,
                MemoryProtection::READ, region->protection,
                region->owner, true  // Boundary violation = potential crash
            ));
        }
        
        // Check permission
        if (!has_flag(region->protection, MemoryProtection::READ)) {
            return record_violation(ValidationResult::violation(
                ViolationType::PERMISSION_VIOLATION, addr, size,
                MemoryProtection::READ, region->protection,
                region->owner, true  // No read perm = crash
            ));
        }
        
        // All checks passed
        return ValidationResult::ok(addr, size);
    }
    
    /**
     * @brief Validate a write operation before execution
     * @param addr Target address
     * @param size Number of bytes to write
     * @param alignment Required alignment (default: 1)
     * @return Validation result with details
     */
    ValidationResult validate_write(uint64_t addr, size_t size = 1, size_t alignment = 1) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) {
            return ValidationResult::ok(addr, size);
        }
        
        stats_.total_validations++;
        
        // Check alignment
        if (alignment > 1 && (addr % alignment != 0)) {
            return record_violation(ValidationResult::violation(
                ViolationType::ALIGNMENT_VIOLATION, addr, size,
                MemoryProtection::WRITE, MemoryProtection::NONE,
                "", true
            ));
        }
        
        // Find region
        const MemoryRegion* region = find_region(addr);
        
        if (!region) {
            return record_violation(ValidationResult::violation(
                ViolationType::WRITE_VIOLATION, addr, size,
                MemoryProtection::WRITE, MemoryProtection::NONE,
                "", true  // Writing unmapped = SIGSEGV
            ));
        }
        
        // Check bounds
        if (!region->contains_range(addr, size)) {
            return record_violation(ValidationResult::violation(
                ViolationType::BOUNDARY_VIOLATION, addr, size,
                MemoryProtection::WRITE, region->protection,
                region->owner, true
            ));
        }
        
        // Check write permission
        if (!has_flag(region->protection, MemoryProtection::WRITE)) {
            return record_violation(ValidationResult::violation(
                ViolationType::WRITE_VIOLATION, addr, size,
                MemoryProtection::WRITE, region->protection,
                region->owner, true  // No write perm = SIGSEGV
            ));
        }
        
        return ValidationResult::ok(addr, size);
    }
    
    /**
     * @brief Validate an execute (code fetch) operation
     * @param addr Instruction address
     * @return Validation result with details
     */
    ValidationResult validate_execute(uint64_t addr) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) {
            return ValidationResult::ok(addr, 4);  // Assume 4-byte instruction
        }
        
        stats_.total_validations++;
        
        // Execute should be at least 4-byte aligned for x86-64
        if (addr % 4 != 0) {
            return record_violation(ValidationResult::violation(
                ViolationType::ALIGNMENT_VIOLATION, addr, 4,
                MemoryProtection::EXECUTE, MemoryProtection::NONE,
                "", true  // Misaligned execute = crash on x86
            ));
        }
        
        // Find region
        const MemoryRegion* region = find_region(addr);
        
        if (!region) {
            return record_violation(ValidationResult::violation(
                ViolationType::EXECUTE_VIOLATION, addr, 4,
                MemoryProtection::EXECUTE, MemoryProtection::NONE,
                "", true  // Executing unmapped = SIGSEGV
            ));
        }
        
        // Check execute permission
        if (!has_flag(region->protection, MemoryProtection::EXECUTE)) {
            return record_violation(ValidationResult::violation(
                ViolationType::EXECUTE_VIOLATION, addr, 4,
                MemoryProtection::EXECUTE, region->protection,
                region->owner, true  // NX bit violation = SIGSEGV
            ));
        }
        
        return ValidationResult::ok(addr, 4);
    }
    
    //=========================================================================
    // Region Management Methods
    //=========================================================================
    
    /**
     * @brief Register a new memory region for tracking
     * @param base Base address of region
     * @param size Size in bytes
     * @param prot Access permissions
     * @param owner Owner identifier string
     * 
     * Called when memory is mapped to track it for future validations.
     * Overlapping regions are allowed (first match during lookup).
     */
    void register_region(uint64_t base, size_t size, MemoryProtection prot, 
                        const std::string& owner) {
        std::lock_guard<std::mutex> lock(mutex_);
        register_region_internal(base, size, prot, owner);
    }
    
    /**
     * @brief Unregister a previously registered region
     * @param base Base address of region to remove
     * @return true if region was found and removed
     */
    bool unregister_region(uint64_t base) {
        std::lock_guard<std::mutex> lock(mutex_);
        return unregister_region_internal(base);
    }
    
    /**
     * @brief Get current validation statistics
     * @return Reference to current stats structure
     */
    const ValidationStats& get_stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
    
    /**
     * @brief Generate comprehensive validation report
     * @return JSON-formatted report string
     */
    std::string generate_validation_report() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ostringstream report;
        report << "{\n";
        report << "  \"report_type\": \"memory_validation\",\n";
        report << "  \"generated_at\": " << timestamp_to_ms(now()) << ",\n";
        report << "  \"plugin_version\": \"" << version() << "\",\n";
        
        // Statistics
        report << "  \"statistics\": " << stats_.to_json() << ",\n";
        
        // Registered regions summary
        report << "  \"registered_regions\": {\n";
        report << "    \"count\": " << regions_.size() << ",\n";
        report << "    \"total_mapped_bytes\": " << calculate_total_mapped() << ",\n";
        report << "    \"regions\": [\n";
        
        bool first_region = true;
        for (const auto& region : regions_) {
            if (!first_region) report << ",\n";
            first_region = false;
            
            report << "      {\n";
            report << "        \"base\": \"0x" << std::hex << region.base_address << std::dec << "\",\n";
            report << "        \"size\": " << region.size << ",\n";
            report << "        \"end\": \"0x" << std::hex << region.end_address() << std::dec << "\",\n";
            report << "        \"protection\": " << static_cast<int>(region.protection) << ",\n";
            report << "        \"owner\": \"" << region.owner << "\"\n";
            report << "      }";
        }
        report << "\n    ]\n";
        report << "  },\n";
        
        // Recent violations
        report << "  \"recent_violations\": [\n";
        for (size_t i = 0; i < recent_violations_.size(); ++i) {
            if (i > 0) report << ",\n";
            report << "    " << recent_violations_[i].to_json();
        }
        report << "\n  ]\n";
        
        report << "}\n";
        return report.str();
    }
    
    /**
     * @brief Get list of recent violations
     * @param count Maximum number to return (default: 50)
     * @return Vector of most recent violations
     */
    std::vector<ValidationResult> get_recent_violations(size_t count = 50) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (count >= recent_violations_.size()) {
            return recent_violations_;
        }
        
        return std::vector<ValidationResult>(
            recent_violations_.end() - count,
            recent_violations_.end()
        );
    }
    
    /**
     * @brief Check if an address is mapped
     * @param addr Address to check
     * @return true if address falls within any registered region
     */
    bool is_address_mapped(uint64_t addr) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return find_region(addr) != nullptr;
    }
    
    /**
     * @brief Get protection flags for an address
     * @param addr Address to query
     * @return Protection flags, or NONE if not mapped
     */
    MemoryProtection get_address_protection(uint64_t addr) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const MemoryRegion* region = find_region(addr);
        return region ? region->protection : MemoryProtection::NONE;
    }
    
    /**
     * @brief Get number of registered regions
     */
    size_t get_region_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return regions_.size();
    }

private:
    //=========================================================================
    // Internal Data Structures
    //=========================================================================
    
    /// All registered memory regions
    std::vector<MemoryRegion> regions_;
    
    /// Ring buffer of recent violations
    std::vector<ValidationResult> recent_violations_;
    static constexpr size_t MAX_RECENT_VIOLATIONS = 100;
    
    /// Accumulated statistics
    ValidationStats stats_;

    //=========================================================================
    // Internal Methods
    //=========================================================================
    
    /**
     * @brief Internal region registration (assumes lock held)
     */
    void register_region_internal(uint64_t base, size_t size, MemoryProtection prot,
                                  const std::string& owner) {
        // Check for existing region at same base (update)
        for (auto& region : regions_) {
            if (region.base_address == base) {
                region.size = size;
                region.protection = prot;
                region.owner = owner;
                region.created_at = now();
                return;
            }
        }
        
        // Add new region
        regions_.push_back({
            .base_address = base,
            .size = size,
            .protection = prot,
            .owner = owner,
            .created_at = now()
        });
        
        // Emit registration event
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "region_registered";
        event.severity = Severity::DEBUG;
        event.message = "Memory region registered for validation";
        event.metadata["owner"] = owner;
        event.numeric_data["base"] = static_cast<int64_t>(base);
        event.numeric_data["size"] = static_cast<int64_t>(size);
        event.numeric_data["protection"] = static_cast<int64_t>(prot);
        emit_event(event);
    }
    
    /**
     * @brief Internal region unregistration (assumes lock held)
     */
    bool unregister_region_internal(uint64_t base) {
        auto it = std::find_if(regions_.begin(), regions_.end(),
            [base](const MemoryRegion& r) { return r.base_address == base; });
        
        if (it != regions_.end()) {
            regions_.erase(it);
            return true;
        }
        return false;
    }
    
    /**
     * @brief Find region containing address (assumes lock held)
     * @return Pointer to region, or nullptr if not found
     * 
     * Uses simple linear search. For production with many regions,
     * consider interval tree or sorted vector with binary search.
     */
    const MemoryRegion* find_region(uint64_t addr) const {
        // Search in reverse order so newer registrations take priority
        // (handles overlapping regions correctly)
        for (auto it = regions_.rbegin(); it != regions_.rend(); ++it) {
            if (it->contains(addr)) {
                return &(*it);
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Record a violation and update statistics
     */
    ValidationResult record_violation(ValidationResult&& result) {
        stats_.violations_detected++;
        stats_.violations_by_type[result.violation_type]++;
        
        if (!result.region_owner.empty()) {
            stats_.violations_by_region[result.region_owner]++;
        }
        
        if (result.would_crash) {
            stats_.prevented_crashes++;
        }
        
        // Add to recent violations ring buffer
        recent_violations_.push_back(result);
        if (recent_violations_.size() > MAX_RECENT_VIOLATIONS) {
            recent_violations_.erase(recent_violations_.begin());
        }
        
        // Emit violation event
        emit_violation_event(result);
        
        return result;
    }
    
    /**
     * @brief Emit diagnostic event for violation
     */
    void emit_violation_event(const ValidationResult& result) {
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "memory_validation_violation";
        event.severity = result.would_crash ? Severity::CRITICAL : Severity::WARNING;
        event.message = result.message;
        event.metadata["violation_type"] = std::to_string(static_cast<int>(result.violation_type));
        event.metadata["region_owner"] = result.region_owner;
        event.metadata["suggestion"] = result.suggestion;
        event.numeric_data["address"] = static_cast<int64_t>(result.address);
        event.numeric_data["access_size"] = static_cast<int64_t>(result.access_size);
        event.numeric_data["required_prot"] = static_cast<int64_t>(result.required_prot);
        event.numeric_data["actual_prot"] = static_cast<int64_t>(result.actual_prot);
        event.float_data["would_crash"] = result.would_crash ? 1.0 : 0.0;
        
        emit_event(event);
    }
    
    /**
     * @brief Calculate total mapped bytes across all regions
     */
    size_t calculate_total_mapped() const {
        size_t total = 0;
        for (const auto& region : regions_) {
            total += region.size;
        }
        return total;
    }
};

//=============================================================================
// Plugin Registration
//=============================================================================

inline std::unique_ptr<DiagnosticPlugin> create_memory_validator_plugin() {
    return std::make_unique<MemoryMappingValidatorPlugin>();
}

inline PluginInfo memory_mapping_validator_plugin_info(
    "memory_mapping_validator",
    "Pre-access memory operation validator for detecting violations before crashes",
    &create_memory_validator_plugin,
    false,  // NOT enabled by default (performance impact)
    60      // priority
);

} // namespace diagnostics
} // namespace prosper
