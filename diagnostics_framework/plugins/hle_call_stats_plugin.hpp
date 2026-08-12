#pragma once
#include "../core/diagnostic_interface.hpp"
#include "../core/event_bus.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_set>
#include <algorithm>
#include <cmath>

/**
 * @file hle_call_stats_plugin.hpp
 * @brief HLE Call Stats Plugin - Statistics on High-Level Emulation calls
 * 
 * Phase 9.5 Diagnostic Plugin for Prosper PS4 Emulator
 * 
 * Features:
 * - Counts calls per HLE function
 * - Tracks timing statistics (min, max, avg, total) per function
 * - Identifies most-called and slowest functions
 * - Provides performance profiling data for HLE layer optimization
 */

namespace prosper {
namespace diagnostics {

//=============================================================================
// HLE Call Statistics Structure
//=============================================================================

struct HLECallStats {
    std::string function_name;
    size_t call_count{0};
    double total_us{0.0};   // Total time in microseconds
    double min_us{0.0};     // Minimum call duration
    double max_us{0.0};     // Maximum call duration
    double avg_us{0.0};     // Average call duration (computed)
    Timestamp last_call{};
    
    // Additional metadata
    std::string module_name;  // Which module this function belongs to (e.g., "libkernel", "libSceLibc")
    uint32_t nid{0};         // NID if applicable
    bool is_stub{false};     // Is this a stub implementation?
    size_t error_count{0};   // Number of calls that returned errors
    
    // Percentile tracking (approximate)
    std::vector<double> recent_durations;  // For percentile calculation
    static constexpr size_t MAX_RECENT_DURATIONS = 1000;
    
    void record_call(double duration_us, bool success = true) {
        call_count++;
        total_us += duration_us;
        last_call = now();
        
        if (call_count == 1) {
            min_us = duration_us;
            max_us = duration_us;
        } else {
            min_us = std::min(min_us, duration_us);
            max_us = std::max(max_us, duration_us);
        }
        
        avg_us = total_us / call_count;
        
        // Track recent durations for percentiles
        recent_durations.push_back(duration_us);
        while (recent_durations.size() > MAX_RECENT_DURATIONS) {
            recent_durations.erase(recent_durations.begin());
        }
        
        if (!success) {
            error_count++;
        }
    }
    
    /**
     * @brief Calculate approximate percentile from recent samples
     */
    double get_percentile(double percentile) const {
        if (recent_durations.empty()) return avg_us;
        
        std::vector<double> sorted = recent_durations;
        std::sort(sorted.begin(), sorted.end());
        
        double index = (percentile / 100.0) * (sorted.size() - 1);
        size_t lower = static_cast<size_t>(std::floor(index));
        size_t upper = static_cast<size_t>(std::ceil(index));
        
        if (lower == upper) return sorted[lower];
        
        double fraction = index - lower;
        return sorted[lower] + fraction * (sorted[upper] - sorted[lower]);
    }
    
    double get_p50() const { return get_percentile(50.0); }
    double get_p90() const { return get_percentile(90.0); }
    double get_p99() const { return get_percentile(99.0); }
    
    // Serialization
    std::string to_json(bool detailed = false) const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"function_name\":\"" << escape_json(function_name) << "\",";
        oss << "\"module\":\"" << escape_json(module_name) << "\",";
        oss << "\"nid\":\"0x" << std::hex << nid << std::dec << "\",";
        oss << "\"call_count\":" << call_count << ",";
        oss << "\"total_us\":" << std::fixed << std::setprecision(2) << total_us << ",";
        oss << "\"total_ms\":" << (total_us / 1000.0) << ",";
        oss << "\"min_us\:" << min_us << ",";
        oss << "\"max_us\":" << max_us << ",";
        oss << "\"avg_us\":" << avg_us << ",";
        oss << "\"p50_us\":" << get_p50() << ",";
        oss << "\"p90_us\":" << get_p90() << ",";
        oss << "\"p99_us\":" << get_p99() << ",";
        oss << "\"last_call_ms\":" << timestamp_to_ms(last_call) << ",";
        oss << "\"is_stub\":" << (is_stub ? "true" : "false") << ",";
        oss << "\"error_count\":" << error_count;
        
        if (detailed && !recent_durations.empty()) {
            oss << ",\"recent_samples\":[";
            for (size_t i = 0; i < recent_durations.size() && i < 20; ++i) {
                if (i > 0) oss << ",";
                oss << recent_durations[i];
            }
            oss << "]";
        }
        
        oss << "}";
        return oss.str();
    }
    
    static std::string escape_json(const std::string& s) {
        std::string result;
        result.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c; break;
            }
        }
        return result;
    }
};

//=============================================================================
// Function Ranking Entry
//=============================================================================

struct FunctionRankingEntry {
    std::string function_name;
    size_t rank{0};
    double value{0.0};
    std::string metric;  // "call_count", "total_time", "avg_time", "max_time"
    
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"rank\":" << rank << ",";
        oss << "\"function_name\":\"" << HLECallStats::escape_json(function_name) << "\",";
        oss << "\"metric\":\"" << metric << "\",";
        oss << "\"value\":" << std::fixed << std::setprecision(2) << value;
        oss << "}";
        return oss.str();
    }
};

//=============================================================================
// Module Summary Structure
//=============================================================================

struct ModuleSummary {
    std::string module_name;
    size_t function_count{0};
    size_t total_calls{0};
    double total_time_us{0.0};
    size_t stub_count{0};
    size_t error_count{0};
    
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"module_name\":\"" << HLECallStats::escape_json(module_name) << "\",";
        oss << "\"function_count\":" << function_count << ",";
        oss << "\"total_calls\":" << total_calls << ",";
        oss << "\"total_time_us\":" << std::fixed << std::setprecision(2) << total_time_us << ",";
        oss << "\"stub_count\":" << stub_count << ",";
        oss << "\"error_count\":" << error_count;
        oss << "}";
        return oss.str();
    }
};

//=============================================================================
// HLE Call Stats Plugin Implementation
//=============================================================================

class HLECallStatsPlugin final : public DiagnosticPlugin {
public:
    HLECallStatsPlugin() = default;
    ~HLECallStatsPlugin() override { shutdown(); }
    
    //-------------------------------------------------------------------------
    // DiagnosticPlugin Interface
    //-------------------------------------------------------------------------
    
    std::string name() const override { return "hle_call_stats"; }
    std::string version() const override { return "1.0.0"; }
    std::string description() const override {
        return "Collects statistics and timing data for HLE function calls";
    }
    
    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        function_stats_.clear();
        active_ = true;
        total_calls_ = 0;
        total_time_us_ = 0.0;
        
        // Subscribe to relevant events
        if (global::is_initialized()) {
            auto* bus = global::get_event_bus();
            event_subscription_id_ = bus->subscribe(
                name(),
                [this](const DiagnosticEvent& evt) { on_event(evt); },
                [](const DiagnosticEvent& evt) {
                    return evt.event_type.find("hle_") != std::string::npos ||
                           evt.event_type == "function_call" ||
                           evt.category == "hle";
                }
            );
        }
        
        return true;
    }
    
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        active_ = false;
        
        if (global::is_initialized() && !event_subscription_id_.empty()) {
            global::get_event_bus()->unsubscribe(event_subscription_id_);
            event_subscription_id_.clear();
        }
    }
    
    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        function_stats_.clear();
        active_ = true;
        total_calls_ = 0;
        total_time_us_ = 0.0;
        event_count_ = 0;
    }
    
    void on_event(const DiagnosticEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return;
        
        enforce_event_limit();
        
        // Process HLE call events
        if (event.event_type == "hle_call" || event.event_type == "function_call") {
            std::string func_name = event.metadata.count("function") ?
                                    event.metadata.at("function") : "";
            
            if (func_name.empty()) {
                func_name = event.message;
            }
            
            double duration_us = event.float_data.count("duration_us") ?
                                event.float_data.at("duration_us") : 0.0;
            
            bool success = !event.numeric_data.count("error") ||
                          event.numeric_data.at("error") == 0;
            
            std::string module = event.metadata.count("module") ?
                                event.metadata.at("module") : "unknown";
            
            uint32_t nid = event.numeric_data.count("nid") ?
                          static_cast<uint32_t>(event.numeric_data.at("nid")) : 0;
            
            bool is_stub = event.numeric_data.count("stub") &&
                          event.numeric_data.at("stub") != 0;
            
            record_call_internal(func_name, duration_us, success, module, nid, is_stub);
        }
    }
    
    std::string generate_report() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ostringstream oss;
        oss << "{";
        oss << "\"plugin\":\"" << name() << "\",";
        oss << "\"version\":\"" << version() << "\",";
        oss << "\"generated_at_ms\":" << timestamp_to_ms(now()) << ",";
        oss << "\"total_functions_tracked\":" << function_stats_.size() << ",";
        oss << "\"total_calls_recorded\":" << total_calls_ << ",";
        oss << "\"total_time_us\":" << std::fixed << std::setprecision(2) << total_time_us_ << ",";
        oss << "\"total_time_ms\":" << (total_time_us_ / 1000.0) << ",";
        
        // Top functions by call count
        auto top_by_calls = get_top_functions("call_count", 10);
        oss << "\"top_by_calls\":[";
        for (size_t i = 0; i < top_by_calls.size(); ++i) {
            if (i > 0) oss << ",";
            oss << top_by_calls[i].to_json();
        }
        oss << "],";
        
        // Top functions by total time
        auto top_by_time = get_top_functions("total_time", 10);
        oss << "\"top_by_total_time\":[";
        for (size_t i = 0; i < top_by_time.size(); ++i) {
            if (i > 0) oss << ",";
            oss << top_by_time[i].to_json();
        }
        oss << "],";
        
        // Slowest functions (by average time)
        auto slowest = get_top_functions("avg_time", 10);
        oss << "\"slowest_functions\":[";
        for (size_t i = 0; i < slowest.size(); ++i) {
            if (i > 0) oss << ",";
            oss << slowest[i].to_json();
        }
        oss << "],";
        
        // Module summary
        auto modules = get_module_summaries();
        oss << "\"modules\":[";
        for (size_t i = 0; i < modules.size(); ++i) {
            if (i > 0) oss << ",";
            oss << modules[i].to_json();
        }
        oss << "]";
        
        oss << "}";
        return oss.str();
    }
    
    void export_json(const std::string& path) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ofstream ofs(path);
        if (ofs.is_open()) {
            ofs << generate_report();
        }
    }
    
    //-------------------------------------------------------------------------
    // Call Recording Methods
    //-------------------------------------------------------------------------
    
    /**
     * @brief Record an HLE function call with timing
     * @param function_name Name of the called function
     * @param duration_us Duration of the call in microseconds
     * @param success Whether the call succeeded
     * @param module Module the function belongs to
     * @param nid NID (Name ID) of the function
     * @param is_stub Whether this is a stub implementation
     */
    void on_hle_call(const std::string& function_name, double duration_us,
                     bool success = true, const std::string& module = "unknown",
                     uint32_t nid = 0, bool is_stub = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return;
        
        enforce_event_limit();
        record_call_internal(function_name, duration_us, success, module, nid, is_stub);
    }
    
    /**
     * @brief Record an HLE function call start (returns a handle for end timing)
     * @param function_name Name of the called function
     * @param module Module the function belongs to
     * @param nid NID of the function
     * @return A unique call ID to be used with end_call()
     */
    uint64_t begin_call(const std::string& function_name,
                        const std::string& module = "unknown",
                        uint32_t nid = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return 0;
        
        enforce_event_limit();
        
        uint64_t call_id = next_call_id_++;
        
        ActiveCall call;
        call.call_id = call_id;
        call.function_name = function_name;
        call.module = module;
        call.nid = nid;
        call.start_time = now();
        
        active_calls_[call_id] = call;
        
        return call_id;
    }
    
    /**
     * @brief End a previously started call and record its timing
     * @param call_id The call ID returned by begin_call()
     * @param success Whether the call succeeded
     * @param is_stub Whether this was a stub implementation
     */
    void end_call(uint64_t call_id, bool success = true, bool is_stub = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return;
        
        auto it = active_calls_.find(call_id);
        if (it == active_calls_.end()) return;
        
        Timestamp end = now();
        double duration_us = std::chrono::duration<double, std::micro>(
            end - it->second.start_time).count();
        
        record_call_internal(it->second.function_name, duration_us, success,
                            it->second.module, it->second.nid, is_stub);
        
        active_calls_.erase(it);
    }
    
    //-------------------------------------------------------------------------
    // Query Methods
    //-------------------------------------------------------------------------
    
    /**
     * @brief Get call count for a specific function
     * @param function_name Function name to query
     * @return Number of times the function was called, or 0 if not tracked
     */
    size_t get_call_count(const std::string& function_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = function_stats_.find(function_name);
        if (it != function_stats_.end()) {
            return it->second.call_count;
        }
        return 0;
    }
    
    /**
     * @brief Get total number of all recorded calls
     * @return Total call count across all functions
     */
    size_t get_total_call_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_calls_;
    }
    
    /**
     * @brief Get timing statistics for a specific function
     * @param function_name Function name to query
     * @return Timing statistics, or default if not found
     */
    HLECallStats get_timing_stats(const std::string& function_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = function_stats_.find(function_name);
        if (it != function_stats_.end()) {
            return it->second;
        }
        return HLECallStats{};
    }
    
    /**
     * @brief Get all timing statistics
     * @return Map of function name -> stats
     */
    std::unordered_map<std::string, HLECallStats> get_all_timing_stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return function_stats_;
    }
    
    /**
     * @brief Get top functions by various metrics
     * @param metric Metric to sort by: "call_count", "total_time", "avg_time", "max_time", "error_count"
     * @param limit Maximum number of results
     * @return Vector of ranked function entries
     */
    std::vector<FunctionRankingEntry> get_top_functions(const std::string& metric,
                                                        size_t limit = 20) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<FunctionRankingEntry> entries;
        entries.reserve(function_stats_.size());
        
        for (const auto& pair : function_stats_) {
            FunctionRankingEntry entry;
            entry.function_name = pair.first;
            entry.metric = metric;
            
            if (metric == "call_count") {
                entry.value = static_cast<double>(pair.second.call_count);
            } else if (metric == "total_time") {
                entry.value = pair.second.total_us;
            } else if (metric == "avg_time") {
                entry.value = pair.second.avg_us;
            } else if (metric == "max_time") {
                entry.value = pair.second.max_us;
            } else if (metric == "error_count") {
                entry.value = static_cast<double>(pair.second.error_count);
            } else {
                entry.value = static_cast<double>(pair.second.call_count);
            }
            
            entries.push_back(entry);
        }
        
        // Sort by value descending
        std::sort(entries.begin(), entries.end(),
                 [](const FunctionRankingEntry& a, const FunctionRankingEntry& b) {
                     return a.value > b.value;
                 });
        
        // Assign ranks and limit results
        std::vector<FunctionRankingEntry> result;
        for (size_t i = 0; i < entries.size() && i < limit; ++i) {
            entries[i].rank = i + 1;
            result.push_back(entries[i]);
        }
        
        return result;
    }
    
    /**
     * @brief Get summary statistics grouped by module
     * @return Vector of module summaries
     */
    std::vector<ModuleSummary> get_module_summaries() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::unordered_map<std::string, ModuleSummary> module_map;
        
        for (const auto& pair : function_stats_) {
            const HLECallStats& stats = pair.second;
            
            auto it = module_map.find(stats.module_name);
            if (it == module_map.end()) {
                ModuleSummary summary;
                summary.module_name = stats.module_name;
                module_map[stats.module_name] = summary;
                it = module_map.find(stats.module_name);
            }
            
            it->second.function_count++;
            it->second.total_calls += stats.call_count;
            it->second.total_time_us += stats.total_us;
            if (stats.is_stub) it->second.stub_count++;
            it->second.error_count += stats.error_count;
        }
        
        std::vector<ModuleSummary> result;
        result.reserve(module_map.size());
        
        for (auto& pair : module_map) {
            result.push_back(pair.second);
        }
        
        // Sort by total calls descending
        std::sort(result.begin(), result.end(),
                 [](const ModuleSummary& a, const ModuleSummary& b) {
                     return a.total_calls > b.total_calls;
                 });
        
        return result;
    }
    
    /**
     * @brief Find stub functions that have been called
     * @return Vector of stub function names and their call counts
     */
    std::vector<std::pair<std::string, size_t>> get_called_stubs() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::pair<std::string, size_t>> result;
        
        for (const auto& pair : function_stats_) {
            if (pair.second.is_stub && pair.second.call_count > 0) {
                result.emplace_back(pair.first, pair.second.call_count);
            }
        }
        
        // Sort by call count descending
        std::sort(result.begin(), result.end(),
                 [](const std::pair<std::string, size_t>& a,
                    const std::pair<std::string, size_t>& b) {
                     return a.second > b.second;
                 });
        
        return result;
    }
    
    /**
     * @brief Find functions with high error rates
     * @param min_error_rate Minimum error rate threshold (0.0 to 1.0)
     * @return Vector of function names with their error rates
     */
    std::vector<std::tuple<std::string, size_t, size_t, double>> 
    get_high_error_functions(double min_error_rate = 0.1) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::tuple<std::string, size_t, size_t, double>> result;
        
        for (const auto& pair : function_stats_) {
            const HLECallStats& stats = pair.second;
            if (stats.call_count > 0 && stats.error_count > 0) {
                double error_rate = static_cast<double>(stats.error_count) / 
                                   static_cast<double>(stats.call_count);
                
                if (error_rate >= min_error_rate) {
                    result.emplace_back(pair.first, stats.call_count, 
                                       stats.error_count, error_rate);
                }
            }
        }
        
        // Sort by error rate descending
        std::sort(result.begin(), result.end(),
                 [](const std::tuple<std::string, size_t, size_t, double>& a,
                    const std::tuple<std::string, size_t, size_t, double>& b) {
                     return std::get<3>(a) > std::get<3>(b);
                 });
        
        return result;
    }

private:
    //-------------------------------------------------------------------------
    // Internal Data Structures
    //-------------------------------------------------------------------------
    
    std::unordered_map<std::string, HLECallStats> function_stats_;
    
    struct ActiveCall {
        uint64_t call_id;
        std::string function_name;
        std::string module;
        uint32_t nid;
        Timestamp start_time;
    };
    std::unordered_map<uint64_t, ActiveCall> active_calls_;
    
    std::string event_subscription_id_;
    uint64_t next_call_id_{1};
    
    // Aggregate counters
    size_t total_calls_{0};
    double total_time_us_{0.0};
    
    //-------------------------------------------------------------------------
    // Internal Helper Methods
    //-------------------------------------------------------------------------
    
    void record_call_internal(const std::string& function_name, double duration_us,
                              bool success, const std::string& module,
                              uint32_t nid, bool is_stub) {
        auto it = function_stats_.find(function_name);
        if (it == function_stats_.end()) {
            HLECallStats stats;
            stats.function_name = function_name;
            stats.module_name = module;
            stats.nid = nid;
            stats.is_stub = is_stub;
            stats.record_call(duration_us, success);
            function_stats_[function_name] = stats;
        } else {
            it->second.record_call(duration_us, success);
        }
        
        total_calls_++;
        total_time_us_ += duration_us;
        event_count_++;
        
        // Emit diagnostic event for slow calls (> 10ms)
        if (duration_us > 10000.0) {
            DiagnosticEvent event;
            event.source_plugin = name();
            event.event_type = "slow_hle_call";
            event.severity = duration_us > 100000.0 ? Severity::WARNING : Severity::INFO;
            event.message = "Slow HLE call: " + function_name + 
                           " took " + std::to_string(duration_us / 1000.0) + "ms";
            event.category = "performance";
            event.metadata["function"] = function_name;
            event.metadata["module"] = module;
            event.float_data["duration_us"] = duration_us;
            
            emit_event(event);
        }
    }
    
    void enforce_event_limit() {
        if (global::get_config()) {
            size_t max_events = global::get_config()->max_events_per_plugin;
            
            // If we're approaching the limit, remove functions with lowest call counts
            while (function_stats_.size() > max_events / 10) {  // Keep at most max/10 unique functions
                // Find function with lowest call count
                auto min_it = function_stats_.begin();
                for (auto it = function_stats_.begin(); it != function_stats_.end(); ++it) {
                    if (it->second.call_count < min_it->second.call_count) {
                        min_it = it;
                    }
                }
                
                if (min_it != function_stats_.end() && min_it->second.call_count == 0) {
                    total_calls_ -= min_it->second.call_count;
                    total_time_us_ -= min_it->second.total_us;
                    function_stats_.erase(min_it);
                } else {
                    break;  // All remaining functions have been called
                }
            }
        }
    }
};

} // namespace diagnostics
} // namespace prosper
