// diagnostics/collectors/elf_collector.hpp — ELF loader diagnostics collector
//
// Tracks ELF loading, parsing, segment mapping, and entry point resolution.
// Outputs elf_report.json with complete ELF analysis.
#pragma once

#include "collector.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace prosper {
namespace diagnostics {

struct ElfSegmentInfo {
    uint64_t    vaddr = 0;
    uint64_t    paddr = 0;
    uint64_t    filesz = 0;
    uint64_t    memsz = 0;
    uint32_t    type = 0;      // PT_LOAD, PT_GNU_STACK, etc.
    uint32_t    flags = 0;     // PF_R, PF_W, PF_X
    uint64_t    mapped_addr = 0;  // Actual mapped address (after relocation)
};

struct ElfHeaderInfo {
    // Basic identification
    uint16_t    machine = 0;       // EM_X86_64 = 62
    uint16_t    type = 0;          // ET_EXEC or ET_DYN
    
    // Entry point
    uint64_t    entry = 0;         // Original entry point from ELF
    uint64_t    mapped_entry = 0;  // Entry after base address adjustment
    
    // Program headers
    uint32_t    phnum = 0;         // Number of program headers
    uint32_t    shnum = 0;         // Number of section headers (if available)
    
    // Segment summary
    size_t      text_segments = 0;
    size_t      data_segments = 0;
    size_t      total_segments = 0;
    
    // Memory layout
    uint64_t    image_base = 0;    // Where the image was loaded
    uint64_t    min_vaddr = 0;
    uint64_t    max_vaddr = 0;
    uint64_t    total_memory = 0;  // Total virtual memory consumed
    
    // Detailed segments
    std::vector<ElfSegmentInfo> segments;
    
    // Timing
    uint64_t    parse_time_ms = 0;
    uint64_t    map_time_ms = 0;
};

class ElfCollector : public Collector {
public:
    ElfCollector() = default;
    
    const char* name() const override { return "elf"; }
    Subsystem subsystem() const override { return Subsystem::LOADER; }
    
    bool initialize() override {
        // Reset state for new session
        headers_.clear();
        current_header_ = {};
        main_executable_path_.clear();
        return true;
    }
    
    // --- Recording Interface -------------------------------------------------
    
    // Called when ELF file is opened
    void record_elf_opened(const std::string& path);
    
    // Called after ELF is parsed but before mapping
    void record_elf_parsed(const ElfHeaderInfo& header);
    
    // Called when a segment is mapped
    void record_segment_mapped(const ElfSegmentInfo& segment, size_t index);
    
    // Called when all segments are mapped
    void record_mapping_complete(uint64_t total_time_ms);
    
    // Set the main executable path (for report context)
    void set_main_executable(const std::string& path) { 
        main_executable_path_ = path; 
    }
    
    // Access recorded data
    const std::vector<ElfHeaderInfo>& headers() const { return headers_; }
    const ElfHeaderInfo* main_header() const { 
        return headers_.empty() ? nullptr : &headers_[0]; 
    }
    
    // Generate JSON report
    std::string generate_report() const override;

private:
    std::vector<ElfHeaderInfo> headers_;
    ElfHeaderInfo current_header_;
    std::string main_executable_path_;
    
    std::string header_to_json(const ElfHeaderInfo& h) const;
    std::string segment_to_json(const ElfSegmentInfo& s) const;
};

} // namespace diagnostics
} // namespace prosper
