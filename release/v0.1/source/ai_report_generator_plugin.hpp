#pragma once
#include "../core/diagnostic_interface.hpp"
#include "../core/event_bus.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_set>

/**
 * @file ai_report_generator_plugin.hpp
 * @brief AI Report Generator Plugin - Generates analysis reports
 * 
 * Phase 9.5 Diagnostic Plugin for Prosper PS4 Emulator
 * 
 * Features:
 * - Aggregates data from all other plugins
 * - Generates human-readable diagnostic report
 * - Produces hypotheses about potential issues
 * - Bounded memory usage (CRITICAL FIX: ring buffer / strict size limits)
 */

namespace prosper {
namespace diagnostics {

//=============================================================================
// Hypothesis Structure
//=============================================================================

struct Hypothesis {
    std::string description;
    double confidence{0.0};  // 0.0 to 1.0
    std::vector<std::string> evidence;
    std::string source_plugin;
    Severity severity{Severity::INFO};
    Timestamp generated_at{};
    std::string category;  // "performance", "correctness", "stability", "compatibility"
    std::vector<std::string> suggested_actions;
    bool is_confirmed{false};
    
    // Serialization
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"description\":\"" << escape_json(description) << "\",";
        oss << "\"confidence\":" << std::fixed << std::setprecision(2) confidence << ",";
        oss << "\"severity\":\"" << to_string(severity) << "\",";
        oss << "\"category\":\"" << escape_json(category) << "\",";
        oss << "\"source_plugin\":\"" << escape_json(source_plugin) << "\",";
        oss << "\"generated_at_ms\":" << timestamp_to_ms(generated_at) << ",";
        oss << "\"is_confirmed\":" << (is_confirmed ? "true" : "false") << ",";
        
        oss << "\"evidence\":[";
        for (size_t i = 0; i < evidence.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << escape_json(evidence[i]) << "\"";
        }
        oss << "],";
        
        oss << "\"suggested_actions\":[";
        for (size_t i = 0; i < suggested_actions.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << escape_json(suggested_actions[i]) << "\"";
        }
        oss << "]";
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
    
private:
    static const char* to_string(Severity s) {
        switch (s) {
            case Severity::DEBUG: return "debug";
            case Severity::INFO: return "info";
            case Severity::WARNING: return "warning";
            case Severity::ERROR: return "error";
            case Severity::CRITICAL: return "critical";
            default: return "unknown";
        }
    }
};

//=============================================================================
// Collected Data Summary from Plugins
//=============================================================================

struct PluginDataSummary {
    std::string plugin_name;
    size_t event_count{0};
    bool active{false};
    std::string status_message;
    std::string summary_json;  // Raw JSON from plugin's generate_report()
    
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"plugin_name\":\"" << Hypothesis::escape_json(plugin_name) << "\",";
        oss << "\"event_count\":" << event_count << ",";
        oss << "\"active\":" << (active ? "true" : "false") << ",";
        oss << "\"status_message\":\"" << Hypothesis::escape_json(status_message) << "\"";
        // Note: Not embedding summary_json to avoid deep nesting
        oss << "}";
        return oss.str();
    }
};

//=============================================================================
// Generated Report Structure
//=============================================================================

struct GeneratedReport {
    Timestamp generated_at{};
    std::string title;
    std::string summary;
    std::vector<Hypothesis> hypotheses;
    std::vector<PluginDataSummary> plugin_summaries;
    std::unordered_map<std::string, std::string> metadata;
    
    // Statistics
    size_t total_hypotheses{0};
    size_t high_confidence_count{0};  // confidence > 0.7
    size_t critical_issues{0};
    
    void compute_statistics() {
        total_hypotheses = hypotheses.size();
        high_confidence_count = 0;
        critical_issues = 0;
        
        for (const auto& h : hypotheses) {
            if (h.confidence > 0.7) high_confidence_count++;
            if (h.severity == Severity::CRITICAL || h.severity == Severity::ERROR) {
                critical_issues++;
            }
        }
    }
    
    std::string to_markdown() const {
        std::ostringstream md;
        
        md << "# " << title << "\n\n";
        md << "**Generated:** " << format_timestamp(generated_at) << "\n\n";
        
        md << "## Summary\n\n";
        md << summary << "\n\n";
        
        md << "## Key Findings\n\n";
        md << "- **Total Hypotheses:** " << total_hypotheses << "\n";
        md << "- **High Confidence (>70%):** " << high_confidence_count << "\n";
        md << "- **Critical/Error Issues:** " << critical_issues << "\n\n";
        
        if (!hypotheses.empty()) {
            md << "## Detailed Hypotheses\n\n";
            
            // Group by severity
            auto sorted_hypotheses = hypotheses;
            std::sort(sorted_hypotheses.begin(), sorted_hypotheses.end(),
                     [](const Hypothesis& a, const Hypothesis& b) {
                         return static_cast<int>(a.severity) > static_cast<int>(b.severity);
                     });
            
            for (const auto& h : sorted_hypotheses) {
                std::string severity_str;
                switch (h.severity) {
                    case Severity::CRITICAL: severity_str = "🔴 CRITICAL"; break;
                    case Severity::ERROR: severity_str = "🟠 ERROR"; break;
                    case Severity::WARNING: severity_str = "🟡 WARNING"; break;
                    case Severity::INFO: severity_str = "🔵 INFO"; break;
                    default: severity_str = "⚪ DEBUG"; break;
                }
                
                md << "### " << severity_str << " (" << 
                   static_cast<int>(h.confidence * 100) << "% confidence)\n\n";
                md << h.description << "\n\n";
                
                if (!h.evidence.empty()) {
                    md << "**Evidence:**\n";
                    for (const auto& e : h.evidence) {
                        md << "- " << e << "\n";
                    }
                    md << "\n";
                }
                
                if (!h.suggested_actions.empty()) {
                    md << "**Suggested Actions:**\n";
                    for (const auto& a : h.suggested_actions) {
                        md << "- " << a << "\n";
                    }
                    md << "\n";
                }
                
                md << "*Source: " << h.source_plugin << "*\n\n";
                md << "---\n\n";
            }
        }
        
        if (!plugin_summaries.empty()) {
            md << "## Plugin Status\n\n";
            md << "| Plugin | Events | Status |\n";
            md << "|--------|--------|--------|\n";
            
            for (const auto& ps : plugin_summaries) {
                md << "| " << ps.plugin_name << " | " << ps.event_count << 
                       " | " << (ps.active ? "✅ Active" : "⚪ Inactive") << " |\n";
            }
            md << "\n";
        }
        
        return md.str();
    }
    
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"title\":\"" << Hypothesis::escape_json(title) << "\",";
        oss << "\"generated_at_ms\":" << timestamp_to_ms(generated_at) << ",";
        oss << "\"summary\":\"" << Hypothesis::escape_json(summary) << "\",";
        oss << "\"total_hypotheses\:" << total_hypotheses << ",";
        oss << "\"high_confidence_count\:" << high_confidence_count << ",";
        oss << "\"critical_issues\:" << critical_issues << ",";
        
        oss << "\"hypotheses\":[";
        for (size_t i = 0; i < hypotheses.size(); ++i) {
            if (i > 0) oss << ",";
            oss << hypotheses[i].to_json();
        }
        oss << "],";
        
        oss << "\"plugin_summaries\":[";
        for (size_t i = 0; i < plugin_summaries.size(); ++i) {
            if (i > 0) oss << ",";
            oss << plugin_summaries[i].to_json();
        }
        oss << "]";
        
        oss << "}";
        return oss.str();
    }
    
private:
    static std::string format_timestamp(Timestamp ts) {
        auto time_t_val = std::chrono::system_clock::to_time_t(
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ts - Timestamp{} + std::chrono::system_clock::now() - now()));
        std::tm tm_buf{};
#ifdef _WIN32
        gmtime_s(&tm_buf, &time_t_val);
#else
        gmtime_r(&time_t_val, &tm_buf);
#endif
        char buffer[64];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S UTC", &tm_buf);
        return std::string(buffer);
    }
};

//=============================================================================
// AI Report Generator Plugin Implementation
//=============================================================================

class AIReportGeneratorPlugin final : public DiagnosticPlugin {
public:
    AIReportGeneratorPlugin() = default;
    ~AIReportGeneratorPlugin() override { shutdown(); }
    
    //-------------------------------------------------------------------------
    // DiagnosticPlugin Interface
    //-------------------------------------------------------------------------
    
    std::string name() const override { return "ai_report_generator"; }
    std::string version() const override { return "1.0.0"; }
    std::string description() const override {
        return "Aggregates data from all plugins and generates analysis reports with hypotheses";
    }
    
    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // CRITICAL FIX: Use bounded containers to prevent unbounded memory growth
        collected_data_.clear();
        hypotheses_history_.clear();
        report_history_.clear();
        active_ = true;
        
        // Subscribe to all events for comprehensive collection
        if (global::is_initialized()) {
            auto* bus = global::get_event_bus();
            event_subscription_id_ = bus->subscribe(
                name(),
                [this](const DiagnosticEvent& evt) { on_event(evt); },
                nullptr  // No filter - collect everything
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
        
        // CRITICAL FIX: Clear all bounded containers on reset
        collected_data_.clear();
        hypotheses_history_.clear();
        report_history_.clear();
        event_count_ = 0;
    }
    
    void on_event(const DiagnosticEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return;
        
        // CRITICAL FIX: Strictly enforce event limits using ring buffer behavior
        enforce_data_limit();
        
        // Store event in bounded collection
        collected_data_.push_back(event);
        
        // Enforce maximum collection size immediately after adding
        while (collected_data_.size() > max_collected_events_) {
            collected_data_.pop_front();
        }
        
        event_count_++;
    }
    
    std::string generate_report() const override {
        // Generate a fresh report on demand
        auto report = generate_report_internal();
        return report.to_json();
    }
    
    void export_json(const std::string& path) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ofstream ofs(path);
        if (ofs.is_open()) {
            ofs << generate_report_internal().to_json();
        }
    }
    
    //-------------------------------------------------------------------------
    // Data Collection Methods
    //-------------------------------------------------------------------------
    
    /**
     * @brief Collect current data from all registered plugins
     * @return Map of plugin name -> data summary
     */
    std::vector<PluginDataSummary> collect_data() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<PluginDataSummary> summaries;
        
        if (!global::is_initialized()) {
            return summaries;
        }
        
        auto* registry = global::get_plugin_registry();
        if (!registry) {
            return summaries;
        }
        
        // Get all plugins and their data
        // Note: In a real implementation, this would iterate over registered plugins
        // For now, we'll work with what we've collected
        
        // Summarize our own collected data by source plugin
        std::unordered_map<std::string, size_t> counts_by_source;
        for (const auto& event : collected_data_) {
            counts_by_source[event.source_plugin]++;
        }
        
        for (const auto& pair : counts_by_source) {
            PluginDataSummary summary;
            summary.plugin_name = pair.first;
            summary.event_count = pair.second;
            summary.active = true;
            summary.status_message = "Collected " + std::to_string(pair.second) + " events";
            summaries.push_back(summary);
        }
        
        return summaries;
    }
    
    //-------------------------------------------------------------------------
    // Hypothesis Generation Methods
    //-------------------------------------------------------------------------
    
    /**
     * @brief Generate hypotheses based on collected data
     * @return Vector of generated hypotheses
     */
    std::vector<Hypothesis> generate_hypotheses() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<Hypothesis> new_hypotheses;
        
        if (collected_data_.empty()) {
            return new_hypotheses;
        }
        
        // Analyze patterns in collected data
        
        // 1. Check for error/critical events
        analyze_error_patterns(new_hypotheses);
        
        // 2. Check for performance issues
        analyze_performance_patterns(new_hypotheses);
        
        // 3. Check for unusual patterns
        analyze_unusual_patterns(new_hypotheses);
        
        // 4. Store hypotheses in bounded history
        for (auto& h : new_hypotheses) {
            h.generated_at = now();
            hypotheses_history_.push_back(h);
            
            // CRITICAL FIX: Enforce strict size limit on hypothesis storage
            while (hypotheses_history_.size() > MAX_HYPOTHESES_STORED) {
                hypotheses_history_.pop_front();
            }
        }
        
        return new_hypotheses;
    }
    
    /**
     * @brief Get all stored hypotheses
     * @param min_confidence Minimum confidence threshold (0.0 to 1.0)
     * @param max_results Maximum number of results
     * @return Vector of hypotheses matching criteria
     */
    std::vector<Hypothesis> get_hypotheses(double min_confidence = 0.0,
                                           size_t max_results = 100) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<Hypothesis> result;
        
        for (auto it = hypotheses_history_.rbegin(); 
             it != hypotheses_history_.rend() && result.size() < max_results; ++it) {
            if (it->confidence >= min_confidence) {
                result.push_back(*it);
            }
        }
        
        return result;
    }
    
    //-------------------------------------------------------------------------
    // Report Generation Methods
    //-------------------------------------------------------------------------
    
    /**
     * @brief Generate a complete diagnostic report
     * @param include_raw_data Whether to include raw event data
     * @return Complete generated report structure
     */
    GeneratedReport generate_report(bool include_raw_data = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        return generate_report_internal(include_raw_data);
    }
    
    /**
     * @brief Export report as Markdown file
     * @param path Output file path
     * @return true if export succeeded
     */
    bool export_markdown(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto report = generate_report_internal();
        std::string markdown = report.to_markdown();
        
        std::ofstream ofs(path);
        if (!ofs.is_open()) {
            return false;
        }
        
        ofs << markdown;
        return true;
    }
    
    /**
     * @brief Get recent reports from history
     * @param count Number of recent reports to retrieve
     * @return Vector of past reports
     */
    std::vector<GeneratedReport> get_report_history(size_t count = 10) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<GeneratedReport> result;
        
        for (auto it = report_history_.rbegin(); 
             it != report_history_.rend() && result.size() < count; ++it) {
            result.push_back(*it);
        }
        
        return result;
    }

private:
    //-------------------------------------------------------------------------
    // Internal Data Structures (All Bounded!)
    //-------------------------------------------------------------------------
    
    // CRITICAL FIX: Ring-buffer style deque with strict size limit
    // This prevents unbounded memory growth that was causing issues
    std::deque<DiagnosticEvent> collected_data_;
    size_t max_collected_events_{10000};  // STRICT LIMIT - never exceeded
    
    // Bounded hypothesis storage
    std::deque<Hypothesis> hypotheses_history_;
    static constexpr size_t MAX_HYPOTHESES_STORED = 500;
    
    // Bounded report history
    std::deque<GeneratedReport> report_history_;
    static constexpr size_t MAX_REPORTS_STORED = 50;
    
    std::string event_subscription_id_;
    
    //-------------------------------------------------------------------------
    // Internal Helper Methods
    //-------------------------------------------------------------------------
    
    GeneratedReport generate_report_internal(bool include_raw_data = false) const {
        GeneratedReport report;
        report.generated_at = now();
        report.title = "Prosper PS4 Emulator Diagnostic Report";
        report.metadata["generator_version"] = version();
        
        // Collect plugin data
        // (Note: In real implementation, would query actual plugin registry)
        std::unordered_map<std::string, size_t> source_counts;
        size_t total_errors = 0;
        size_t total_warnings = 0;
        size_t total_criticals = 0;
        
        for (const auto& event : collected_data_) {
            source_counts[event.source_plugin]++;
            
            switch (event.severity) {
                case Severity::CRITICAL: total_criticals++; break;
                case Severity::ERROR: total_errors++; break;
                case Severity::WARNING: total_warnings++; break;
                default: break;
            }
        }
        
        // Build plugin summaries
        for (const auto& pair : source_counts) {
            PluginDataSummary ps;
            ps.plugin_name = pair.first;
            ps.event_count = pair.second;
            ps.active = true;
            ps.status_message = "Collected " + std::to_string(pair.second) + " events";
            report.plugin_summaries.push_back(ps);
        }
        
        // Copy existing hypotheses
        report.hypotheses.assign(hypotheses_history_.begin(), hypotheses_history_.end());
        
        // Generate summary
        std::ostringstream summary_ss;
        summary_ss << "Diagnostic session captured " << collected_data_.size() << " events ";
        summary_ss << "from " << source_counts.size() << " sources. ";
        
        if (total_criticals > 0) {
            summary_ss << total_criticals << " critical issues detected. ";
        }
        if (total_errors > 0) {
            summary_ss << total_errors << " errors recorded. ";
        }
        if (total_warnings > 0) {
            summary_ss << total_warnings << " warnings noted. ";
        }
        
        if (total_criticals == 0 && total_errors == 0 && total_warnings == 0) {
            summary_ss << "No significant issues detected.";
        }
        
        report.summary = summary_ss.str();
        
        // Compute statistics
        report.compute_statistics();
        
        // Store in bounded history (mutable operation - need to cast away const carefully)
        // Actually, we shouldn't modify state in a const method, so we skip storing here
        // The non-const version handles history storage
        
        return report;
    }
    
    // Non-const version that also stores to history
    GeneratedReport generate_report_internal(bool include_raw_data = false) {
        auto report = generate_report_internal_const(include_raw_data);
        
        // Store in bounded history
        report_history_.push_back(report);
        while (report_history_.size() > MAX_REPORTS_STORED) {
            report_history_.pop_front();
        }
        
        return report;
    }
    
    // Const version for internal use
    GeneratedReport generate_report_internal_const(bool include_raw_data = false) const {
        // Same implementation as above but truly const
        GeneratedReport report;
        report.generated_at = now();
        report.title = "Prosper PS4 Emulator Diagnostic Report";
        report.metadata["generator_version"] = version();
        
        std::unordered_map<std::string, size_t> source_counts;
        size_t total_errors = 0;
        size_t total_warnings = 0;
        size_t total_criticals = 0;
        
        for (const auto& event : collected_data_) {
            source_counts[event.source_plugin]++;
            
            switch (event.severity) {
                case Severity::CRITICAL: total_criticals++; break;
                case Severity::ERROR: total_errors++; break;
                case Severity::WARNING: total_warnings++; break;
                default: break;
            }
        }
        
        for (const auto& pair : source_counts) {
            PluginDataSummary ps;
            ps.plugin_name = pair.first;
            ps.event_count = pair.second;
            ps.active = true;
            ps.status_message = "Collected " + std::to_string(pair.second) + " events";
            report.plugin_summaries.push_back(ps);
        }
        
        report.hypotheses.assign(hypotheses_history_.begin(), hypotheses_history_.end());
        
        std::ostringstream summary_ss;
        summary_ss << "Diagnostic session captured " << collected_data_.size() << " events ";
        summary_ss << "from " << source_counts.size() << " sources. ";
        
        if (total_criticals > 0) {
            summary_ss << total_criticals << " critical issues detected. ";
        }
        if (total_errors > 0) {
            summary_ss << total_errors << " errors recorded. ";
        }
        if (total_warnings > 0) {
            summary_ss << total_warnings << " warnings noted. ";
        }
        
        if (total_criticals == 0 && total_errors == 0 && total_warnings == 0) {
            summary_ss << "No significant issues detected.";
        }
        
        report.summary = summary_ss.str();
        report.compute_statistics();
        
        return report;
    }
    
    //-------------------------------------------------------------------------
    // Analysis Methods for Hypothesis Generation
    //-------------------------------------------------------------------------
    
    void analyze_error_patterns(std::vector<Hypothesis>& hypotheses) {
        // Count errors by type/source
        std::unordered_map<std::string, size_t> errors_by_type;
        std::unordered_map<std::string, size_t> errors_by_source;
        std::vector<const DiagnosticEvent*> critical_events;
        
        for (const auto& event : collected_data_) {
            if (event.severity >= Severity::ERROR) {
                errors_by_type[event.event_type]++;
                errors_by_source[event.source_plugin]++;
                
                if (event.severity == Severity::CRITICAL) {
                    critical_events.push_back(&event);
                }
            }
        }
        
        // Generate hypothesis for frequent errors
        for (const auto& pair : errors_by_type) {
            if (pair.second >= 5) {  // Threshold for reporting
                Hypothesis h;
                h.description = "Frequent error pattern detected: '" + pair.first + 
                               "' occurred " + std::to_string(pair.second) + " times";
                h.confidence = std::min(0.95, 0.5 + (pair.second * 0.05));
                h.source_plugin = name();
                h.category = "stability";
                h.severity = pair.second >= 20 ? Severity::ERROR : Severity::WARNING;
                
                h.evidence.push_back("Event type '" + pair.first + "' seen " + 
                                    std::to_string(pair.second) + " times");
                
                h.suggested_actions.push_back("Investigate root cause of '" + pair.first + "' errors");
                h.suggested_actions.push_back("Check related system logs for additional context");
                
                hypotheses.push_back(h);
            }
        }
        
        // Critical issue hypothesis
        if (!critical_events.empty()) {
            Hypothesis h;
            h.description = std::to_string(critical_events.size()) + 
                           " critical event(s) detected requiring immediate attention";
            h.confidence = 1.0;
            h.source_plugin = name();
            h.category = "stability";
            h.severity = Severity::CRITICAL;
            
            size_t shown = 0;
            for (const auto* evt : critical_events) {
                if (shown >= 5) {
                    h.evidence.push_back("... and " + 
                        std::to_string(critical_events.size() - shown) + " more");
                    break;
                }
                h.evidence.push_back("[" + evt->source_plugin + "] " + evt->message);
                shown++;
            }
            
            h.suggested_actions.push_back("Review critical events immediately");
            h.suggested_actions.push_back("Consider pausing emulation if unstable");
            
            hypotheses.push_back(h);
        }
    }
    
    void analyze_performance_patterns(std::vector<Hypothesis>& hypotheses) {
        // Look for performance-related events
        size_t slow_frame_count = 0;
        size_t spike_count = 0;
        double total_frame_time = 0.0;
        size_t frame_count = 0;
        
        for (const auto& event : collected_data_) {
            if (event.event_type == "frame_spike" || 
                event.event_type == "frame_end_slow") {
                spike_count++;
                
                auto it = event.float_data.find("frame_time_ms");
                if (it != event.float_data.end()) {
                    total_frame_time += it->second;
                    frame_count++;
                    
                    if (it->second > 50.0) {  // > 50ms is very slow
                        slow_frame_count++;
                    }
                }
            }
        }
        
        // Frame spike hypothesis
        if (spike_count >= 3) {
            Hypothesis h;
            h.description = "Performance instability detected: " + 
                           std::to_string(spike_count) + " frame spikes recorded";
            h.confidence = std::min(0.9, 0.4 + (spike_count * 0.08));
            h.source_plugin = name();
            h.category = "performance";
            h.severity = slow_frame_count > 10 ? Severity::WARNING : Severity::INFO;
            
            h.evidence.push_back(std::to_string(spike_count) + " frame spikes detected");
            if (frame_count > 0) {
                double avg_spike_ms = total_frame_time / frame_count;
                h.evidence.push_back("Average spike duration: " + 
                    std::to_string(avg_spike_ms, 1) + "ms");
            }
            if (slow_frame_count > 0) {
                h.evidence.push_back(std::to_string(slow_frame_count) + 
                    " extremely slow frames (>50ms)");
            }
            
            h.suggested_actions.push_back("Check GPU driver settings and thermal throttling");
            h.suggested_actions.push_back("Consider reducing resolution or graphical quality");
            h.suggested_actions.push_back("Profile CPU/GPU usage during spikes");
            
            hypotheses.push_back(h);
        }
        
        // HLE performance hypothesis
        size_t slow_hle_calls = 0;
        std::string slowest_function;
        double slowest_duration = 0.0;
        
        for (const auto& event : collected_data_) {
            if (event.event_type == "slow_hle_call") {
                slow_hle_calls++;
                
                auto it = event.float_data.find("duration_us");
                if (it != event.float_data.end() && it->second > slowest_duration) {
                    slowest_duration = it->second;
                    auto func_it = event.metadata.find("function");
                    if (func_it != event.metadata.end()) {
                        slowest_function = func_it->second;
                    }
                }
            }
        }
        
        if (slow_hle_calls >= 5) {
            Hypothesis h;
            h.description = "HLE layer performance concerns: " + 
                           std::to_string(slow_hle_calls) + " slow function calls detected";
            h.confidence = std::min(0.85, 0.4 + (slow_hle_calls * 0.06));
            h.source_plugin = name();
            h.category = "performance";
            h.severity = Severity::INFO;
            
            h.evidence.push_back(std::to_string(slow_hle_calls) + " slow HLE calls recorded");
            if (!slowest_function.empty()) {
                h.evidence.push_back("Slowest function: " + slowest_function + 
                    " (" + std::to_string(slowest_duration / 1000.0, 1) + "ms)");
            }
            
            h.suggested_actions.push_back("Profile and optimize frequently-called HLE functions");
            h.suggested_actions.push_back("Consider caching results for expensive operations");
            
            hypotheses.push_back(h);
        }
    }
    
    void analyze_unusual_patterns(std::vector<Hypothesis>& hypotheses) {
        // File access anomalies
        size_t file_errors = 0;
        std::unordered_set<std::string> missing_files;
        
        for (const auto& event : collected_data_) {
            if (event.event_type == "access_anomaly") {
                file_errors++;
                
                auto path_it = event.metadata.find("path");
                auto type_it = event.metadata.find("anomaly_type");
                
                if (type_it != event.metadata.end() && type_it->second == "missing_file") {
                    if (path_it != event.metadata.end()) {
                        missing_files.insert(path_it->second);
                    }
                }
            }
        }
        
        if (file_errors >= 3 || !missing_files.empty()) {
            Hypothesis h;
            h.description = "File system anomalies detected";
            h.confidence = std::min(0.9, 0.5 + (file_errors * 0.08) + 
                                   (missing_files.size() * 0.1));
            h.source_plugin = name();
            h.category = "compatibility";
            h.severity = !missing_files.empty() ? Severity::WARNING : Severity::INFO;
            
            h.evidence.push_back(std::to_string(file_errors) + " file access anomalies");
            
            if (!missing_files.empty()) {
                h.evidence.push_back(std::to_string(missing_files.size()) + 
                    " missing files detected");
                for (const auto& f : missing_files) {
                    if (h.evidence.size() < 6) {
                        h.evidence.push_back("Missing: " + f);
                    }
                }
            }
            
            h.suggested_actions.push_back("Verify game files are correctly installed");
            h.suggested_actions.push_back("Check file path mappings in configuration");
            
            hypotheses.push_back(h);
        }
        
        // Thread deadlock indicators
        size_t deadlock_warnings = 0;
        
        for (const auto& event : collected_data_) {
            if (event.event_type == "potential_deadlock") {
                deadlock_warnings++;
            }
        }
        
        if (deadlock_warnings > 0) {
            Hypothesis h;
            h.description = "Potential thread synchronization issues detected: " + 
                           std::to_string(deadlock_warnings) + " deadlock warning(s)";
            h.confidence = std::min(0.95, 0.6 + (deadlock_warnings * 0.15));
            h.source_plugin = name();
            h.category = "stability";
            h.severity = Severity::WARNING;
            
            h.evidence.push_back(std::to_string(deadlock_warnings) + 
                " potential deadlock scenarios detected");
            
            h.suggested_actions.push_back("Review threading model for race conditions");
            h.suggested_actions.push_back("Check lock acquisition ordering consistency");
            h.suggested_actions.push_back("Consider adding timeout to blocking operations");
            
            hypotheses.push_back(h);
        }
    }
    
    //-------------------------------------------------------------------------
    // Memory Enforcement (CRITICAL FIX Implementation)
    //-------------------------------------------------------------------------
    
    void enforce_data_limit() {
        // CRITICAL FIX: Always maintain strict bounds on stored data
        // This is called before every insert to prevent unbounded growth
        
        // Check global config for custom limits
        if (global::get_config()) {
            max_collected_events_ = global::get_config()->max_events_per_plugin;
        }
        
        // Enforce collected data limit
        while (collected_data_.size() >= max_collected_events_) {
            collected_data_.pop_front();
        }
        
        // Enforce hypothesis history limit
        while (hypotheses_history_.size() >= MAX_HYPOTHESES_STORED) {
            hypotheses_history_.pop_front();
        }
        
        // Enforce report history limit
        while (report_history_.size() >= MAX_REPORTS_STORED) {
            report_history_.pop_front();
        }
    }
};

} // namespace diagnostics
} // namespace prosper
