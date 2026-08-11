// diagnostics/collectors/hle_collector.hpp — HLE function call diagnostics
//
// Tracks all HLE (High-Level Emulation) function calls including arguments,
// return values, execution time, and frequency analysis.
#pragma once

#include "collector.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace prosper {
namespace diagnostics {

struct HleCallRecord {
    std::string     library;        // e.g., "libkernel", "sceVideoOut"
    std::string     function;       // Function name
    uint64_t        call_count = 0;
    uint64_t        failed_count = 0;   // Return value indicated error
    
    // Timing statistics
    uint64_t        total_time_ns = 0;
    uint64_t        min_time_ns = UINT64_MAX;
    uint64_t        max_time_ns = 0;
    
    // Last call info (for debugging)
    int             last_return_value = 0;
    std::string     last_args_summary;  // Truncated argument summary
    
    // Thread that most often calls this
    std::string     primary_thread;
};

struct HleCallEvent {
    std::string     library;
    std::string     function;
    int             return_value = 0;
    uint64_t        duration_ns = 0;
    std::string     thread_name;
    std::string     args_summary;
    bool            is_error = false;  // Return indicates error condition
};

class HleCollector : public Collector {
public:
    HleCollector() = default;
    
    const char* name() const override { return "hle"; }
    Subsystem subsystem() const override { return Subsystem::HLE; }
    
    bool initialize() override {
        calls_.clear();
        recent_events_.clear();
        return true;
    }
    
    // --- Recording Interface -------------------------------------------------
    
    // Record an HLE function call
    void record_call(const HleCallEvent& event);
    
    // Convenience: record a simple call with minimal info
    void record_simple_call(
        const std::string& library,
        const std::string& function,
        int retval,
        bool is_error = false
    );
    
    // --- Query Interface ----------------------------------------------------
    
    // Get total number of unique functions called
    size_t unique_functions() const { return calls_.size(); }
    
    // Get total call count across all functions
    uint64_t total_calls() const;
    
    // Find a specific function's record
    const HleCallRecord* find_function(const std::string& library,
                                        const std::string& function) const;
    
    // Get top N functions by call count (for "hot path" analysis)
    std::vector<const HleCallRecord*> top_functions(size_t n) const;
    
    // Get functions that failed (returned errors)
    std::vector<const HleCallRecord*> failed_functions() const;
    
    // Get slowest functions by average time
    std::vector<const HleCallRecord*> slowest_functions(size_t n) const;
    
    // Get all functions from a library
    std::vector<const HleCallRecord*> library_functions(const std::string& lib) const;
    
    // Recent events (for crash context)
    const std::vector<HleCallEvent>& recent_events() const { return recent_events_; }
    
    // Generate JSON report
    std::string generate_report() const override;

private:
    // Key is "library::function"
    std::unordered_map<std::string, HleCallRecord> calls_;
    std::vector<HleCallEvent> recent_events_;  // Ring buffer of last N events
    static constexpr size_t kMaxRecentEvents = 500;
    
    HleCallRecord* get_or_create_record(const std::string& library,
                                        const std::string& function);
    std::string make_key(const std::string& library, 
                         const std::string& function) const {
        return library + "::" + function;
    }
    
    std::string record_to_json(const HleCallRecord& r) const;
};

} // namespace diagnostics
} // namespace prosper
