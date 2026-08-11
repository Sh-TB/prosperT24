// diagnostics/collectors/hle_collector.cpp — HLE collector implementation
#include "hle_collector.hpp"
#include <algorithm>
#include <sstream>

namespace prosper {
namespace diagnostics {

HleCallRecord* HleCollector::get_or_create_record(const std::string& library,
                                                   const std::string& function) {
    auto key = make_key(library, function);
    auto it = calls_.find(key);
    if (it != calls_.end()) return &it->second;
    
    HleCallRecord rec;
    rec.library = library;
    rec.function = function;
    calls_[key] = rec;
    return &calls_[key];
}

void HleCollector::record_call(const HleCallEvent& event) {
    auto* rec = get_or_create_record(event.library, event.function);
    
    // Update statistics
    rec->call_count++;
    if (event.is_error) rec->failed_count++;
    
    // Timing
    rec->total_time_ns += event.duration_ns;
    if (event.duration_ns < rec->min_time_ns) rec->min_time_ns = event.duration_ns;
    if (event.duration_ns > rec->max_time_ns) rec->max_time_ns = event.duration_ns;
    
    // Last call info
    rec->last_return_value = event.return_value;
    rec->last_args_summary = event.args_summary;
    if (!event.thread_name.empty()) rec->primary_thread = event.thread_name;
    
    // Add to recent events (ring buffer)
    recent_events_.push_back(event);
    if (recent_events_.size() > kMaxRecentEvents) {
        recent_events_.erase(recent_events_.begin());
    }
    
    // Emit event for significant calls (errors, slow calls)
    if (event.is_error || event.duration_ns > 10000000ULL) {  // >10ms
        emit_event(
            "HLE_CALL",
            event.is_error ? Severity::WARNING : Severity::DEBUG,
            event.is_error
                ? ("HLE error: " + event.library + "::" + event.function + 
                   " returned " + std::to_string(event.return_value))
                : ("HLE slow call: " + event.library + "::" + event.function +
                   " took " + std::to_string(event.duration_ns / 1000) + "us"),
            SourceLocation(__FILE__, __LINE__, __func__),
            [&](DiagnosticEvent& e) {
                e.add_string("library", event.library);
                e.add_string("function", event.function);
                e.add_int("return_value", event.return_value);
                e.add_uint("duration_ns", event.duration_ns);
                e.add_bool("is_error", event.is_error);
                e.add_string("thread", event.thread_name);
            }
        );
    }
}

void HleCollector::record_simple_call(const std::string& library,
                                       const std::string& function,
                                       int retval,
                                       bool is_error) {
    HleCallEvent event;
    event.library = library;
    event.function = function;
    event.return_value = retval;
    event.is_error = is_error;
    record_call(event);
}

uint64_t HleCollector::total_calls() const {
    uint64_t total = 0;
    for (const auto& [key, rec] : calls_) {
        total += rec.call_count;
    }
    return total;
}

const HleCallRecord* HleCollector::find_function(
    const std::string& library,
    const std::string& function
) const {
    auto it = calls_.find(make_key(library, function));
    if (it != calls_.end()) return &it->second;
    return nullptr;
}

std::vector<const HleCallRecord*> HleCollector::top_functions(size_t n) const {
    std::vector<const HleCallRecord*> result;
    for (const auto& [key, rec] : calls_) {
        result.push_back(&rec);
    }
    
    std::partial_sort(result.begin(), result.begin() + 
                      std::min(n, result.size()), result.end(),
                      [](const HleCallRecord* a, const HleCallRecord* b) {
                          return a->call_count > b->call_count;
                      });
    
    if (result.size() > n) result.resize(n);
    return result;
}

std::vector<const HleCallRecord*> HleCollector::failed_functions() const {
    std::vector<const HleCallRecord*> result;
    for (const auto& [key, rec] : calls_) {
        if (rec.failed_count > 0) result.push_back(&rec);
    }
    
    std::sort(result.begin(), result.end(),
              [](const HleCallRecord* a, const HleCallRecord* b) {
                  return a->failed_count > b->failed_count;
              });
    
    return result;
}

std::vector<const HleCallRecord*> HleCollector::slowest_functions(size_t n) const {
    std::vector<const HleCallRecord*> result;
    for (const auto& [key, rec] : calls_) {
        if (rec.call_count > 0) {  // Avoid division by zero
            result.push_back(&rec);
        }
    }
    
    std::partial_sort(result.begin(), result.begin() + 
                      std::min(n, result.size()), result.end(),
                      [](const HleCallRecord* a, const HleCallRecord* b) {
                          uint64_t avg_a = a->total_time_ns / a->call_count;
                          uint64_t avg_b = b->total_time_ns / b->call_count;
                          return avg_a > avg_b;
                      });
    
    if (result.size() > n) result.resize(n);
    return result;
}

std::vector<const HleCallRecord*> HleCollector::library_functions(
    const std::string& lib
) const {
    std::vector<const HleCallRecord*> result;
    for (const auto& [key, rec] : calls_) {
        if (rec.library == lib) result.push_back(&rec);
    }
    
    std::sort(result.begin(), result.end(),
              [](const HleCallRecord* a, const HleCallRecord* b) {
                  return a->call_count > b->call_count;
              });
    
    return result;
}

std::string HleCollector::record_to_json(const HleCallRecord& r) const {
    std::ostringstream ss;
    
    ss << "{\n";
    ss << "      \"library\": \"" << r.library << "\",\n";
    ss << "      \"function\": \"" << r.function << "\",\n";
    ss << "      \"calls\": " << r.call_count << ",\n";
    ss << "      \"failed\": " << r.failed_count << ",\n";
    
    // Average time in microseconds
    uint64_t avg_us = r.call_count > 0 ? (r.total_time_ns / r.call_count / 1000) : 0;
    ss << "      \"avg_time_us\": " << avg_us << ",\n";
    ss << "      \"min_time_us\": " << (r.min_time_ns == UINT64_MAX ? 0 : r.min_time_ns / 1000) << ",\n";
    ss << "      \"max_time_us\": " << r.max_time_ns / 1000 << ",\n";
    ss << "      \"last_return\": " << r.last_return_value << ",\n";
    ss << "      \"primary_thread\": \"" << r.primary_thread << "\"\n";
    ss << "    }";
    
    return ss.str();
}

std::string HleCollector::generate_report() const {
    std::ostringstream ss;
    
    ss << "{\n";
    ss << "  \"summary\": {\n";
    ss << "    \"unique_functions\": " << calls_.size() << ",\n";
    ss << "    \"total_calls\": " << total_calls() << ",\n";
    
    uint64_t total_errors = 0;
    for (const auto& [k, r] : calls_) total_errors += r.failed_count;
    ss << "    \"error_calls\": " << total_errors << "\n";
    ss << "  },\n";
    
    // Top 20 functions by call count
    auto top = top_functions(20);
    ss << "  \"top_functions\": [\n";
    for (size_t i = 0; i < top.size(); ++i) {
        ss << "    " << record_to_json(*top[i]);
        if (i < top.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    
    // Failed functions
    auto failed = failed_functions();
    ss << "  \"failed_functions\": [\n";
    for (size_t i = 0; i < failed.size(); ++i) {
        ss << "    " << record_to_json(*failed[i]);
        if (i < failed.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    
    // Slowest functions
    auto slow = slowest_functions(10);
    ss << "  \"slowest_functions\": [\n";
    for (size_t i = 0; i < slow.size(); ++i) {
        ss << "    " << record_to_json(*slow[i]);
        if (i < slow.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    
    ss << "}\n";
    
    return ss.str();
}

} // namespace diagnostics
} // namespace prosper
