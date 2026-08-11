// diagnostics/collectors/memory_collector.cpp — Memory collector implementation
#include "memory_collector.hpp"
#include <algorithm>
#include <sstream>

namespace prosper {
namespace diagnostics {

void MemoryCollector::record_alloc(uint64_t addr, uint64_t size, 
                                    const std::string& region) {
    MemoryEvent e;
    e.operation = MemoryOperation::ALLOC;
    e.address = addr;
    e.size = size;
    e.region_name = region;
    events_.push_back(e);
    total_allocated_ += size;
    
    // Build message with proper hex formatting
    std::ostringstream msg_ss;
    msg_ss << "Allocated 0x" << std::hex << size << " bytes at 0x" << addr;
    
    emit_event(
        "MEMORY_ALLOC",
        Severity::DEBUG,
        msg_ss.str(),
        SourceLocation(__FILE__, __LINE__, __func__),
        [=](DiagnosticEvent& de) {
            de.add_string("operation", "ALLOC");
            de.add_uint("address", addr);
            de.add_uint("size", size);
            if (!region.empty()) de.add_string("region", region);
        }
    );
}

void MemoryCollector::record_free(uint64_t addr, bool success) {
    MemoryEvent e;
    e.operation = MemoryOperation::FREE;
    e.address = addr;
    e.success = success;
    events_.push_back(e);
    
    // Build message with proper hex formatting
    std::ostringstream msg_ss;
    if (success) {
        msg_ss << "Freed 0x" << std::hex << addr;
    } else {
        msg_ss << "Free failed: 0x" << std::hex << addr;
    }
    
    emit_event(
        "MEMORY_FREE",
        success ? Severity::DEBUG : Severity::WARNING,
        msg_ss.str(),
        SourceLocation(__FILE__, __LINE__, __func__),
        [=](DiagnosticEvent& de) {
            de.add_string("operation", "FREE");
            de.add_uint("address", addr);
            de.add_bool("success", success);
        }
    );
}

void MemoryCollector::record_map(uint64_t addr, uint64_t size, uint32_t perms,
                                 const std::string& name) {
    MemoryEvent e;
    e.operation = MemoryOperation::MAP;
    e.address = addr;
    e.size = size;
    e.permissions = perms;
    e.region_name = name;
    events_.push_back(e);
    
    // Add to regions
    MemoryRegion reg;
    reg.base = addr;
    reg.size = size;
    reg.permissions = perms;
    reg.name = name.empty() ? "mapped_" + std::to_string(regions_.size()) : name;
    regions_.push_back(reg);
    
    // Build message with proper hex formatting
    std::ostringstream msg_ss;
    msg_ss << "Mapped 0x" << std::hex << size << " bytes at 0x" << addr;
    if (!name.empty()) {
        msg_ss << " (" << name << ")";
    }
    
    emit_event(
        "MEMORY_MAP",
        Severity::INFO,
        msg_ss.str(),
        SourceLocation(__FILE__, __LINE__, __func__),
        [=](DiagnosticEvent& de) {
            de.add_string("operation", "MAP");
            de.add_uint("address", addr);
            de.add_uint("size", size);
            de.add_uint("permissions", perms);
            if (!name.empty()) de.add_string("name", name);
        }
    );
}

void MemoryCollector::record_unmap(uint64_t addr, uint64_t size) {
    MemoryEvent e;
    e.operation = MemoryOperation::UNMAP;
    e.address = addr;
    e.size = size;
    events_.push_back(e);
    
    // Remove from regions
    regions_.erase(
        std::remove_if(regions_.begin(), regions_.end(),
            [addr, size](const MemoryRegion& r) {
                return r.base == addr && r.size == size;
            }),
        regions_.end()
    );
    
    // Build message with proper hex formatting
    std::ostringstream msg_ss;
    msg_ss << "Unmapped 0x" << std::hex << size << " at 0x" << addr;
    
    emit_event(
        "MEMORY_UNMAP",
        Severity::INFO,
        msg_ss.str(),
        SourceLocation(__FILE__, __LINE__, __func__)
    );
}

void MemoryCollector::record_protect_change(uint64_t addr, uint64_t size,
                                            uint32_t old_perms, uint32_t new_perms) {
    MemoryEvent e;
    e.operation = MemoryOperation::PROTECT;
    e.address = addr;
    e.size = size;
    e.permissions = new_perms;
    events_.push_back(e);
    
    // Build message with proper hex formatting
    std::ostringstream msg_ss;
    msg_ss << "Protection changed at 0x" << std::hex << addr << ": " 
           << std::dec << old_perms << " -> " << new_perms;
    
    emit_event(
        "MEMORY_PROTECT",
        Severity::DEBUG,
        msg_ss.str(),
        SourceLocation(__FILE__, __LINE__, __func__)
    );
}

void MemoryCollector::record_violation(uint64_t addr, uint32_t access_type,
                                       const std::string& context) {
    violation_count_++;
    
    MemoryEvent e;
    e.operation = MemoryOperation::ACCESS_VIOLATION;
    e.address = addr;
    e.permissions = access_type;
    e.error_message = context;
    e.success = false;
    events_.push_back(e);
    
    // Build message with proper hex formatting
    std::ostringstream msg_ss;
    msg_ss << "Access violation at 0x" << std::hex << addr;
    if (!context.empty()) {
        msg_ss << " (" << context << ")";
    }
    
    emit_event(
        "MEMORY_VIOLATION",
        Severity::ERROR,
        msg_ss.str(),
        SourceLocation(__FILE__, __LINE__, __func__),
        [=](DiagnosticEvent& de) {
            de.add_string("operation", "VIOLATION");
            de.add_uint("address", addr);
            de.add_uint("access_type", access_type);
            if (!context.empty()) de.add_string("context", context);
        }
    );
}

void MemoryCollector::add_region(const MemoryRegion& region) {
    regions_.push_back(region);
}

const MemoryRegion* MemoryCollector::find_region(uint64_t addr) const {
    for (const auto& r : regions_) {
        if (addr >= r.base && addr < r.base + r.size) return &r;
    }
    return nullptr;
}

std::string MemoryCollector::event_to_json(const MemoryEvent& e) const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "      \"op\": \"" << op_string(e.operation) << "\",\n";
    ss << "      \"addr\": \"0x" << std::hex << e.address << "\",\n";
    ss << "      \"size\": " << std::dec << (e.size > 0 ? std::to_string(e.size) : "null") << ",\n";
    ss << "      \"success\": " << (e.success ? "true" : "false");
    if (!e.region_name.empty()) ss << ",\n      \"region\": \"" << e.region_name << "\"";
    if (!e.error_message.empty()) ss << ",\n      \"error\": \"" << e.error_message << "\"";
    ss << "\n    }";
    return ss.str();
}

std::string MemoryCollector::generate_report() const {
    std::ostringstream ss;
    
    ss << "{\n";
    ss << "  \"summary\": {\n";
    ss << "    \"total_operations\": " << events_.size() << ",\n";
    ss << "    \"total_regions\": " << regions_.size() << ",\n";
    ss << "    \"current_usage\": " << current_usage() << ",\n";
    ss << "    \"violations\": " << violation_count_ << "\n";
    ss << "  },\n";
    
    // Regions
    ss << "  \"regions\": [\n";
    for (size_t i = 0; i < regions_.size(); ++i) {
        const auto& r = regions_[i];
        ss << "    {\n";
        ss << "      \"base\": \"0x" << std::hex << r.base << "\",\n";
        ss << "      \"size\": " << std::dec << r.size << ",\n";
        ss << "      \"permissions\": " << r.permissions << ",\n";
        ss << "      \"name\": \"" << r.name << "\"\n";
        ss << "    }";
        if (i < regions_.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    
    // Recent violations (last 20)
    auto viol_count = std::count_if(events_.begin(), events_.end(),
        [](const MemoryEvent& e) { return e.operation == MemoryOperation::ACCESS_VIOLATION; });
    
    ss << "  \"violation_count\": " << viol_count << "\n";
    ss << "}\n";
    
    return ss.str();
}

} // namespace diagnostics
} // namespace prosper
