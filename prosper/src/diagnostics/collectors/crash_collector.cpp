// diagnostics/collectors/crash_collector.cpp — Crash collector implementation
#include "crash_collector.hpp"
#include <sstream>

namespace prosper {
namespace diagnostics {

const char* CrashCollector::crash_type_string(CrashType t) {
    switch (t) {
        case CrashType::SIGSEGV:     return "SIGSEGV";
        case CrashType::SIGABRT:     return "SIGABRT";
        case CrashType::SIGFPE:      return "SIGFPE";
        case CrashType::SIGBUS:      return "SIGBUS";
        case CrashType::TRAP:        return "TRAP";
        case CrashType::UNEXPECTED:  return "UNEXPECTED";
        default:                     return "UNKNOWN";
    }
}

void CrashCollector::record_crash(const CrashReport& crash) {
    has_crash_ = true;
    report_ = crash;
    
    emit_event(
        "CRASH",
        Severity::CRITICAL,
        std::string("Crash detected: ") + crash_type_string(crash.type) +
            (crash.signal_reason.empty() ? "" : (" - " + crash.signal_reason)),
        SourceLocation(__FILE__, __LINE__, __func__),
        [&](DiagnosticEvent& e) {
            e.add_string("crash_type", crash_type_string(crash.type));
            e.add_uint("fault_address", crash.fault_address);
            e.add_string("signal", crash.signal_name);
            if (!crash.suspected_cause.empty()) {
                e.add_string("suspected_cause", crash.suspected_cause);
                e.add_float("confidence", crash.confidence);
            }
            e.add_uint("stack_depth", crash.stack_trace.size());
        }
    );
}

void CrashCollector::record_crash(const CrashInfo& info) {
    // Convert lightweight signal-handler info to full CrashReport
    CrashReport crash;
    
    // Map signal number to CrashType
    switch (info.signal_number) {
        case 11: crash.type = CrashType::SIGSEGV; break;  // SIGSEGV
        case 6:  crash.type = CrashType::SIGABRT; break;  // SIGABRT
        case 8:  crash.type = CrashType::SIGFPE; break;   // SIGFPE
        case 7:  crash.type = CrashType::SIGBUS; break;   // SIGBUS
        case 4:  crash.type = CrashType::TRAP; break;    // SIGILL (treated as trap)
        default: crash.type = CrashType::UNKNOWN; break;
    }
    
    // Fill register state from available info
    crash.registers.rip = info.instruction_pointer;
    crash.registers.rsp = info.stack_pointer;
    crash.registers.rbp = info.frame_pointer;
    crash.fault_address = info.fault_address;
    crash.signal_name = crash_type_string(crash.type);
    crash.signal_reason = "Fatal signal during guest execution";
    crash.crash_time = std::chrono::steady_clock::now();
    
    // Add module info if available
    if (!info.module_name.empty()) {
        crash.loaded_modules.push_back(info.module_name);
    }
    
    // Basic analysis
    if (info.fault_address == 0) {
        crash.suspected_cause = "Null pointer dereference";
        crash.confidence = 0.85;
    } else if (info.fault_address < 0x10000) {
        crash.suspected_cause = "Low address access (null+offset)";
        crash.confidence = 0.8;
    } else {
        crash.suspected_cause = "Access to unmapped or protected memory";
        crash.confidence = 0.7;
    }
    
    record_crash(crash);
}

void CrashCollector::record_segfault(uint64_t fault_addr, const RegisterState& regs) {
    CrashReport crash;
    crash.type = CrashType::SIGSEGV;
    crash.registers = regs;
    crash.fault_address = fault_addr;
    crash.signal_name = "SIGSEGV";
    crash.crash_time = std::chrono::steady_clock::now();
    
    // Basic analysis of fault address
    if (fault_addr == 0) {
        crash.signal_reason = "null pointer dereference";
        crash.suspected_cause = "Null pointer dereference - likely uninitialized or freed pointer";
        crash.confidence = 0.85;
        crash.recommendations.push_back("Check for null pointer before dereference");
        crash.recommendations.push_back("Review recent memory operations");
    } else if (fault_addr < 0x10000) {
        crash.signal_reason = "low address access (likely null+offset)";
        crash.suspected_cause = "Null structure member access or buffer underread";
        crash.confidence = 0.8;
        crash.recommendations.push_back("Check structure field offsets");
    } else {
        crash.signal_reason = "invalid memory access";
        crash.suspected_cause = "Access to unmapped or protected memory region";
        crash.confidence = 0.7;
        crash.recommendations.push_back("Verify address is within mapped regions");
        crash.recommendations.push_back("Check array bounds and pointer arithmetic");
    }
    
    record_crash(crash);
}

void CrashCollector::record_abort(const RegisterState& regs, const std::string& reason) {
    CrashReport crash;
    crash.type = CrashType::SIGABRT;
    crash.registers = regs;
    crash.signal_name = "SIGABRT";
    crash.signal_reason = reason.empty() ? "abort() called" : reason;
    crash.crash_time = std::chrono::steady_clock::now();
    
    crash.suspected_cause = reason.empty() 
        ? "Assertion failure or explicit abort" 
        : reason;
    crash.confidence = 0.75;
    crash.recommendations.push_back("Find assertion that triggered abort");
    crash.recommendations.push_back("Check for invariant violations");
    
    record_crash(crash);
}

void CrashCollector::set_analysis(const std::string& cause, double confidence,
                                  const std::vector<std::string>& recommendations) {
    report_.suspected_cause = cause;
    report_.confidence = confidence;
    report_.recommendations = recommendations;
}

std::string CrashCollector::registers_to_json(const RegisterState& r) const {
    std::ostringstream ss;
    
    ss << "{\n";
    ss << "        \"rip\": "0x" << std::hex << r.rip << "\",\n";
    ss << "        \"rsp\": "0x" << std::hex << r.rsp << "\",\n";
    ss << "        \"rbp\": "0x" << std::hex << r.rbp << "\",\n";
    ss << "        \"rax\": "0x" << std::hex << r.rax << "\",\n";
    ss << "        \"rbx\": "0x" << std::hex << r.rbx << "\",\n";
    ss << "        \"rcx\": "0x" << std::hex << r.rcx << "\",\n";
    ss << "        \"rdx\": "0x" << std::hex << r.rdx << "\",\n";
    ss << "        \"rsi\": "0x" << std::hex << r.rsi << "\",\n";
    ss << "        \"rdi\": "0x" << std::hex << r.rdi << "\",\n";
    ss << "        \"r8\": "0x" << std::hex << r.r8 << "\",\n";
    ss << "        \"r9\": "0x\" << std::hex << r.r9 << "\",\n";
    ss << "        \"r10\": "0x\" << std::hex << r.r10 << "\",\n";
    ss << "        \"r11\": "0x\" << std::hex << r.r11 << "\",\n";
    ss << "        \"r12\": "0x\" << std::hex << r.r12 << "\",\n";
    ss << "        \"r13\": "0x\" << std::hex << r.r13 << "\",\n";
    ss << "        \"r14\": "0x\" << std::hex << r.r14 << "\",\n";
    ss << "        \"r15\": "0x\" << std::hex << r.r15 << "\",\n";
    ss << "        \"rflags\": "0x\" << std::hex << r.rflags << "\"\n";
    ss << "      }";
    
    return ss.str();
}

std::string CrashCollector::stack_trace_to_json() const {
    std::ostringstream ss;
    
    ss << "[\n";
    for (size_t i = 0; i < report_.stack_trace.size(); ++i) {
        const auto& frame = report_.stack_trace[i];
        ss << "        {\n";
        ss << "          \"return_addr\": "0x" << std::hex << frame.return_address << "\",\n";
        ss << "          \"frame_ptr\": "0x" << std::hex << frame.frame_pointer << "\",\n";
        ss << "          \"module\": \"" << frame.module_name << "\"";
        if (!frame.function_name.empty()) {
            ss << ",\n          \"function\": \"" << frame.function_name << "\"";
        }
        ss << "\n        }";
        if (i < report_.stack_trace.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "      ]";
    
    return ss.str();
}

std::string CrashCollector::generate_report() const {
    if (!has_crash_) return "{}";
    
    std::ostringstream ss;
    
    ss << "{\n";
    
    // Crash info
    ss << "  \"crash\": {\n";
    ss << "    \"type\": \"" << crash_type_string(report_.type) << "\",\n";
    ss << "    \"signal\": \"" << report_.signal_name << "\",\n";
    ss << "    \"reason\": \"" << report_.signal_reason << "\",\n";
    ss << "    \"fault_address\": "0x" << std::hex << report_.fault_address << "\",\n";
    ss << "    \"uptime_ms\": " << std::dec << report_.uptime_ms << "\n";
    ss << "  },\n";
    
    // Registers
    ss << "  \"registers\": " << registers_to_json(report_.registers) << ",\n";
    
    // Stack trace
    ss << "  \"stack_trace\": " << stack_trace_to_json() << ",\n";
    
    // Analysis
    ss << "  \"analysis\": {\n";
    ss << "    \"suspected_cause\": \"" << report_.suspected_cause << "\",\n";
    ss << "    \"confidence\": " << report_.confidence << ",\n";
    
    ss << "    \"recommendations\": [";
    for (size_t i = 0; i < report_.recommendations.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "\"" << report_.recommendations[i] << "\"";
    }
    ss << "]\n";
    
    ss << "  },\n";
    
    // Loaded modules at time of crash
    ss << "  \"loaded_modules\": [";
    for (size_t i = 0; i < report_.loaded_modules.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "\"" << report_.loaded_modules[i] << "\"";
    }
    ss << "]\n";
    
    ss << "}\n";
    
    return ss.str();
}

} // namespace diagnostics
} // namespace prosper
