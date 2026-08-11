// diagnostics/collectors/crash_collector.hpp — Crash intelligence collector
//
// Captures detailed crash information including register state, stack trace,
// loaded modules, recent events, and generates root cause analysis.
#pragma once

#include "collector.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

namespace prosper {
namespace diagnostics {

enum class CrashType : uint8_t {
    SIGSEGV,     // Segmentation fault
    SIGABRT,     // Abort
    SIGFPE,      // Floating point exception
    SIGBUS,      // Bus error
    TRAP,        // Trap/breakpoint
    UNEXPECTED,  // Unexpected exit
    UNKNOWN,
};

struct RegisterState {
    uint64_t rip = 0;   // Instruction pointer
    uint64_t rsp = 0;   // Stack pointer
    uint64_t rbp = 0;   // Frame pointer
    
    // General purpose registers
    uint64_t rax = 0, rbx = 0, rcx = 0, rdx = 0;
    uint64_t rsi = 0, rdi = 0, r8 = 0, r9 = 0;
    uint64_t r10 = 0, r11 = 0, r12 = 0, r13 = 0;
    uint64_t r14 = 0, r15 = 0;
    
    // Flags
    uint64_t rflags = 0;
    
    // Segment registers (for completeness)
    uint16_t cs = 0, ds = 0, es = 0, fs = 0, gs = 0, ss = 0;
};

struct StackFrame {
    uint64_t        return_address = 0;
    uint64_t        frame_pointer = 0;
    std::string     module_name;     // Which module this address is in
    std::string     function_name;   // If known (symbolicated)
    uint64_t        offset = 0;      // Offset within module/function
};

struct CrashReport {
    CrashType           type = CrashType::UNKNOWN;
    RegisterState       registers;
    std::vector<StackFrame> stack_trace;
    
    // Context
    std::string         signal_name;      // e.g., "SIGSEGV"
    std::string         signal_reason;    // e.g., "address not mapped"
    uint64_t            fault_address = 0;
    
    // Timing
    std::chrono::steady_clock::time_point crash_time;
    uint64_t            uptime_ms = 0;   // Time since boot start
    
    // Module state at crash
    std::vector<std::string> loaded_modules;
    
    // Recent events before crash (for correlation)
    size_t              events_before_crash = 0;
    
    // Analysis
    std::string         suspected_cause;
    double              confidence = 0.0;
    std::vector<std::string> recommendations;
};

class CrashCollector : public Collector {
public:
    CrashCollector() = default;
    
    const char* name() const override { return "crash"; }
    Subsystem subsystem() const override { return Subsystem::CRASH; }
    
    bool initialize() override {
        has_crash_ = false;
        report_ = CrashReport();
        return true;
    }
    
    // --- Recording Interface -------------------------------------------------
    
    // Record a crash with full context
    void record_crash(const CrashReport& crash);
    
    // Lightweight record for signal-handler context (minimal info, no allocation)
    // Used by exec_image_linux.cpp fault_handler where full stack trace is unsafe
    struct CrashInfo {
        int         signal_number = 0;
        uint64_t    fault_address = 0;
        uint64_t    instruction_pointer = 0;
        uint64_t    stack_pointer = 0;
        uint64_t    frame_pointer = 0;
        std::string module_name;
        uint64_t    module_offset = 0;
    };
    void record_crash(const CrashInfo& info);
    
    // Convenience: record a simple segfault
    void record_segfault(uint64_t fault_addr, const RegisterState& regs);
    
    // Convenience: record an abort
    void record_abort(const RegisterState& regs, const std::string& reason = "");
    
    // Set suspected cause after analysis
    void set_analysis(const std::string& cause, double confidence,
                     const std::vector<std::string>& recommendations);
    
    // --- Query Interface ----------------------------------------------------
    
    bool has_crash() const { return has_crash_; }
    const CrashReport& report() const { return report_; }
    
    // Get crash type as string
    static const char* crash_type_string(CrashType t);
    
    std::string generate_report() const override;

private:
    bool has_crash_ = false;
    CrashReport report_;
    
    std::string registers_to_json(const RegisterState& r) const;
    std::string stack_trace_to_json() const;
};

} // namespace diagnostics
} // namespace prosper
