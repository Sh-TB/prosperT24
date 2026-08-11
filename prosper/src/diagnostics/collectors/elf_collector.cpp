// diagnostics/collectors/elf_collector.cpp — ELF collector implementation
#include "elf_collector.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace prosper {
namespace diagnostics {

void ElfCollector::record_elf_opened(const std::string& path) {
    emit_event(
        "ELF_OPENED",
        Severity::INFO,
        "ELF file opened: " + path,
        SourceLocation(__FILE__, __LINE__, __func__),
        [&](DiagnosticEvent& e) {
            e.add_string("path", path);
            e.add_bool("is_main_executable", path.find("eboot") != std::string::npos);
        }
    );
}

void ElfCollector::record_elf_parsed(const ElfHeaderInfo& header) {
    current_header_ = header;
    
    // Build message with proper hex formatting
    std::ostringstream msg_ss;
    msg_ss << "ELF parsed: " << header.total_segments << " segments, entry at 0x" 
           << std::hex << header.entry;
    
    emit_event(
        "ELF_PARSED",
        Severity::INFO,
        msg_ss.str(),
        SourceLocation(__FILE__, __LINE__, __func__),
        [&](DiagnosticEvent& e) {
            e.add_uint("machine", header.machine);
            e.add_uint("type", header.type);
            e.add_string("entry", "0x" + std::to_string(header.entry));
            e.add_uint("phnum", header.phnum);
            e.add_uint("text_segments", header.text_segments);
            e.add_uint("data_segments", header.data_segments);
            e.add_uint("total_segments", header.total_segments);
            e.add_string("image_base", "0x" + std::to_string(header.image_base));
            e.add_uint("total_memory", header.total_memory);
        }
    );
}

void ElfCollector::record_segment_mapped(const ElfSegmentInfo& segment, size_t index) {
    // Determine segment type name
    const char* type_name = "UNKNOWN";
    switch (segment.type) {
        case 1: type_name = "PT_LOAD"; break;
        case 2: type_name = "PT_DYNAMIC"; break;
        case 3: type_name = "PT_INTERP"; break;
        case 4: type_name = "PT_NOTE"; break;
        case 6: type_name = "PT_PHDR"; break;
        case 7: type_name = "PT_TLS"; break;
        case 0x6474e550: type_name = "PT_GNU_EH_FRAME"; break;
        case 0x6474e551: type_name = "PT_GNU_STACK"; break;
        case 0x6474e552: type_name = "PT_GNU_RELRO"; break;
        case 0x6474e553: type_name = "PT_GNU_PROPERTY"; break;
        default: type_name = "OTHER"; break;
    }
    
    // Build flags string
    std::string flags_str;
    if (segment.flags & 4) flags_str += 'R';
    if (segment.flags & 2) flags_str += 'W';
    if (segment.flags & 1) flags_str += 'X';
    
    // Build message with proper hex formatting
    std::ostringstream msg_ss;
    msg_ss << "Segment [" << index << "] mapped: " << type_name 
           << " " << flags_str << " 0x" << std::hex << segment.vaddr 
           << " size 0x" << segment.memsz;
    
    emit_event(
        "SEGMENT_MAPPED",
        Severity::DEBUG,
        msg_ss.str(),
        SourceLocation(__FILE__, __LINE__, __func__),
        [&, index](DiagnosticEvent& e) {
            e.add_uint("segment_index", index);
            e.add_string("type", type_name);
            e.add_uint("type_value", segment.type);
            e.add_string("flags", flags_str);
            e.add_string("vaddr", "0x" + std::to_string(segment.vaddr));
            e.add_string("filesz", "0x" + std::to_string(segment.filesz));
            e.add_string("memsz", "0x" + std::to_string(segment.memsz));
            e.add_string("mapped_addr", "0x" + std::to_string(segment.mapped_addr));
        }
    );
    
    current_header_.segments.push_back(segment);
}

void ElfCollector::record_mapping_complete(uint64_t total_time_ms) {
    current_header_.map_time_ms = total_time_ms;
    headers_.push_back(current_header_);
    
    emit_event(
        "MAPPING_COMPLETE",
        Severity::INFO,
        "All segments mapped in " + std::to_string(total_time_ms) + "ms",
        SourceLocation(__FILE__, __LINE__, __func__),
        [=](DiagnosticEvent& e) {
            e.add_uint("total_time_ms", total_time_ms);
            e.add_uint("total_headers", headers_.size());
        }
    );
}

std::string ElfCollector::header_to_json(const ElfHeaderInfo& h) const {
    std::ostringstream ss;
    
    ss << "{\n";
    ss << "      \"machine\": " << h.machine << ",\n";
    ss << "      \"type\": " << h.type << ",\n";
    ss << "      \"entry\": \"0x" << std::hex << h.entry << "\",\n";
    ss << "      \"mapped_entry\": \"0x" << h.mapped_entry << "\",\n";
    ss << std::dec;  // Reset to decimal
    ss << "      \"phnum\": " << h.phnum << ",\n";
    ss << "      \"shnum\": " << h.shnum << ",\n";
    ss << "      \"text_segments\": " << h.text_segments << ",\n";
    ss << "      \"data_segments\": " << h.data_segments << ",\n";
    ss << "      \"total_segments\": " << h.total_segments << ",\n";
    ss << "      \"image_base\": \"0x" << std::hex << h.image_base << "\",\n";
    ss << "      \"min_vaddr\": \"0x" << h.min_vaddr << "\",\n";
    ss << "      \"max_vaddr\": \"0x" << h.max_vaddr << "\",\n";
    ss << std::dec;
    ss << "      \"total_memory\": " << h.total_memory << ",\n";
    ss << "      \"parse_time_ms\": " << h.parse_time_ms << ",\n";
    ss << "      \"map_time_ms\": " << h.map_time_ms << ",\n";
    
    // Segments array
    ss << "      \"segments\": [\n";
    for (size_t i = 0; i < h.segments.size(); ++i) {
        ss << "        " << segment_to_json(h.segments[i]);
        if (i < h.segments.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "      ]\n";
    
    ss << "    }";
    
    return ss.str();
}

std::string ElfCollector::segment_to_json(const ElfSegmentInfo& s) const {
    std::ostringstream ss;
    
    ss << "{\n";
    ss << "          \"vaddr\": \"0x" << std::hex << s.vaddr << "\",\n";
    ss << "          \"paddr\": \"0x" << s.paddr << "\",\n";
    ss << "          \"filesz\": " << std::dec << s.filesz << ",\n";
    ss << "          \"memsz\": " << s.memsz << ",\n";
    ss << "          \"type\": " << s.type << ",\n";
    ss << "          \"flags\": " << s.flags << ",\n";
    ss << "          \"permissions\": \"";
    if (s.flags & 4) ss << 'R';
    if (s.flags & 2) ss << 'W';
    if (s.flags & 1) ss << 'X';
    ss << "\",\n";
    ss << "          \"mapped_addr\": \"0x" << std::hex << s.mapped_addr << "\"\n";
    ss << "        }";
    
    return ss.str();
}

std::string ElfCollector::generate_report() const {
    std::ostringstream ss;
    
    ss << "{\n";
    ss << "  \"main_executable\": \"" << main_executable_path_ << "\",\n";
    ss << "  \"total_images\": " << headers_.size() << ",\n";
    ss << "  \"images\": [\n";
    
    for (size_t i = 0; i < headers_.size(); ++i) {
        ss << "    " << header_to_json(headers_[i]);
        if (i < headers_.size() - 1) ss << ",";
        ss << "\n";
    }
    
    ss << "  ]\n";
    ss << "}\n";
    
    return ss.str();
}

} // namespace diagnostics
} // namespace prosper
