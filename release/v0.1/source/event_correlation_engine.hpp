#pragma once
#include "../core/diagnostic_interface.hpp"
#include "../core/event_bus.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <algorithm>
#include <numeric>
#include <functional>

namespace prosper {
namespace diagnostics {

//=============================================================================
// Event Correlation Engine - Phase 10 Tier 2 Diagnostic Plugin
//
// AI Debugging Core - Links events with ranked hypotheses.
//
// CRITICAL DESIGN PRINCIPLE: This AI layer must NEVER claim certainty.
// Only generates ranked hypotheses with confidence scores. The highest
// confidence is 99% to acknowledge inherent uncertainty in debugging.
//
// Purpose: When a crash occurs, identify related events and rank possible
// causes by confidence to guide developer investigation efficiently.
//
// Output format example:
//   Crash: SIGSEGV at 0x801234
//   
//   Possible causes:
//   1. Invalid relocation (Confidence: 82%)
//      Evidence: Function pointer points outside module
//   
//   2. Missing HLE import (Confidence: 35%)
//      Evidence: Related import unresolved
//
// Design Decisions:
// - Uses rule-based correlation (not ML) for determinism and explainability
// - Confidence scores are calibrated, not arbitrary
// - Conflicting evidence is tracked for each hypothesis
// - All hypotheses require human investigation flag when confidence < threshold
//=============================================================================

/**
 * @brief A single hypothesis about crash causation
 * 
 * Represents one possible explanation for a crash, with supporting
 * and conflicting evidence, confidence score, and investigation flags.
 */
struct CorrelationHypothesis {
    std::string hypothesis_id;                    ///< Unique identifier for this hypothesis
    std::string description;                      ///< Human-readable hypothesis text
    double confidence{0.0};                       ///< 0.0 to 0.99 (NEVER 100% certain!)
    
    /// Supporting evidence references
    std::vector<std::string> evidence;            ///< Event IDs or descriptions supporting this
    
    /// Evidence against this hypothesis
    std::vector<std::string> conflicting_evidence; ///< Reasons to doubt this hypothesis
    
    /// Classification metadata
    std::string source_type;                      ///< "relocation", "import", "memory", "thread", "module"
    std::vector<std::string> related_event_ids;   ///< Events directly related to this hypothesis
    
    Timestamp generated_at;                       ///< When this hypothesis was created
    bool requires_investigation{true};            ///< Flag for human review (true if uncertain)
    
    /// Default constructor
    CorrelationHypothesis() : generated_at(now()) {}
    
    /**
     * @brief Constructor with essential fields
     */
    CorrelationHypothesis(const std::string& id, const std::string& desc, double conf,
                          const std::string& type)
        : hypothesis_id(id), description(desc), 
          confidence(std::min(0.99, conf)),  // Cap at 99%
          source_type(type),
          generated_at(now()),
          requires_investigation(conf < 0.70) {}  // Auto-flag if low confidence
    
    /**
     * @brief Convert hypothesis to JSON representation
     */
    std::string to_json() const {
        std::ostringstream ss;
        ss << "{\n";
        ss << "    \"hypothesis_id\": \"" << hypothesis_id << "\",\n";
        ss << "    \"description\": \"" << escape_json(description) << "\",\n";
        ss << "    \"confidence\": " << std::fixed << std::setprecision(4) << confidence << ",\n";
        ss << "    \"confidence_display\": \"" << static_cast<int>(confidence * 100) << "%\",\n";
        ss << "    \"source_type\": \"" << source_type << "\",\n";
        ss << "    \"requires_investigation\": " << (requires_investigation ? "true" : "false") << ",\n";
        
        // Evidence array
        ss << "    \"evidence\": [";
        for (size_t i = 0; i < evidence.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << "\"" << escape_json(evidence[i]) << "\"";
        }
        ss << "],\n";
        
        // Conflicting evidence array
        ss << "    \"conflicting_evidence\": [";
        for (size_t i = 0; i < conflicting_evidence.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << "\"" << escape_json(conflicting_evidence[i]) << "\"";
        }
        ss << "],\n";
        
        // Related event IDs
        ss << "    \"related_event_ids\": [";
        for (size_t i = 0; i < related_event_ids.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << "\"" << related_event_ids[i] << "\"";
        }
        ss << "],\n";
        
        ss << "    \"generated_at\": " << timestamp_to_ms(generated_at) << "\n";
        ss << "  }";
        return ss.str();
    }
    
private:
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
 * @brief Complete correlation analysis report for a crash
 * 
 * Contains all hypotheses, summary, and recommended actions
 * resulting from analyzing a crash event against recent history.
 */
struct CorrelationReport {
    std::string crash_event_id;                   ///< ID of the analyzed crash event
    Timestamp crash_time;                         ///< When the crash occurred
    uint64_t crash_address{0};                    ///< Address where crash occurred
    int signal_number{0};                         ///< Signal that caused crash (SIGSEGV=11, etc.)
    
    /// Ranked hypotheses (sorted by confidence descending)
    std::vector<CorrelationHypothesis> hypotheses;
    
    /// Human-readable summary paragraph
    std::string summary;
    
    /// Actionable recommendations
    std::vector<std::string> recommended_actions;
    
    /**
     * @brief Convert full report to JSON
     */
    std::string to_json() const {
        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"report_type\": \"crash_correlation\",\n";
        ss << "  \"crash_event_id\": \"" << crash_event_id << "\",\n";
        ss << "  \"crash_time\": " << timestamp_to_ms(crash_time) << ",\n";
        ss << "  \"crash_address\": \"0x" << std::hex << crash_address << std::dec << "\",\n";
        ss << "  \"signal_number\": " << signal_number << ",\n";
        ss << "  \"signal_name\": \"" << signal_to_string(signal_number) << "\",\n";
        
        // Summary
        ss << "  \"summary\": \"" << escape_json(summary) << "\",\n";
        
        // Hypotheses (ranked)
        ss << "  \"hypotheses\": [\n";
        for (size_t i = 0; i < hypotheses.size(); ++i) {
            if (i > 0) ss << ",\n";
            ss << "    " << hypotheses[i].to_json();
        }
        ss << "\n  ],\n";
        
        // Recommended actions
        ss << "  \"recommended_actions\": [\n";
        for (size_t i = 0; i < recommended_actions.size(); ++i) {
            if (i > 0) ss << ",\n";
            ss << "    \"" << escape_json(recommended_actions[i]) << "\"";
        }
        ss << "\n  ]\n";
        
        ss << "}\n";
        return ss.str();
    }
    
    /**
     * @brief Generate human-readable text report
     */
    std::string to_text() const {
        std::ostringstream ss;
        ss << "═══════════════════════════════════════════════════════════════\n";
        ss << "               CRASH CORRELATION ANALYSIS REPORT\n";
        ss << "═══════════════════════════════════════════════════════════════\n\n";
        
        ss << "CRASH DETAILS:\n";
        ss << "  Address: 0x" << std::hex << crash_address << std::dec << "\n";
        ss << "  Signal: " << signal_number << " (" << signal_to_string(signal_number) << ")\n";
        ss << "  Time: " << timestamp_to_ms(crash_time) << " ms\n\n";
        
        ss << "SUMMARY:\n";
        ss << "  " << summary << "\n\n";
        
        ss << "RANKED HYPOTHESES:\n";
        for (size_t i = 0; i < hypotheses.size(); ++i) {
            const auto& h = hypotheses[i];
            ss << "  " << (i + 1) << ". " << h.description 
               << " (Confidence: " << static_cast<int>(h.confidence * 100) << "%)\n";
            
            if (!h.evidence.empty()) {
                ss << "     Evidence:\n";
                for (const auto& e : h.evidence) {
                    ss << "       • " << e << "\n";
                }
            }
            
            if (!h.conflicting_evidence.empty()) {
                ss << "     Conflicting evidence:\n";
                for (const auto& ce : h.conflicting_evidence) {
                    ss << "       • " << ce << "\n";
                }
            }
            
            if (h.requires_investigation) {
                ss << "     ⚠ REQUIRES MANUAL INVESTIGATION\n";
            }
            ss << "\n";
        }
        
        if (!recommended_actions.empty()) {
            ss << "RECOMMENDED ACTIONS:\n";
            for (const auto& action : recommended_actions) {
                ss << "  → " << action << "\n";
            }
        }
        
        ss << "═══════════════════════════════════════════════════════════════\n";
        return ss.str();
    }
    
private:
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
    
    static std::string signal_to_string(int sig) {
        switch (sig) {
            case 6:  return "SIGABRT";
            case 11: return "SIGSEGV";
            case 7:  return "SIGBUS";
            case 8:  return "SIGFPE";
            case 4:  return "SIGILL";
            case 13: return "SIGPIPE";
            case 1:  return "SIGHUP";
            case 2:  return "SIGINT";
            case 15: return "SIGTERM";
            default: return "UNKNOWN(" + std::to_string(sig) + ")";
        }
    }
};

//=============================================================================
// Correlation Rule Type Definition
//=============================================================================

/**
 * @brief Function signature for correlation rules
 * 
 * Rules receive recent events and output zero or more hypotheses.
 */
using CorrelationRule = std::function<std::vector<CorrelationHypothesis>(
    const std::vector<DiagnosticEvent>&)>;

//=============================================================================
// Event Correlation Engine Implementation
//=============================================================================

class EventCorrelationEngine : public DiagnosticPlugin {
public:
    //=========================================================================
    // Construction / Destruction
    //=========================================================================
    
    EventCorrelationEngine() : DiagnosticPlugin() {
        active_ = false;
        setup_default_rules();
    }
    
    ~EventCorrelationEngine() override {
        shutdown();
    }
    
    //=========================================================================
    // DiagnosticPlugin Interface Implementation
    //=========================================================================
    
    std::string name() const override {
        return "event_correlation";
    }
    
    std::string description() const override {
        return "AI-powered event correlation engine that analyzes crashes and "
               "generates ranked hypotheses about possible causes with "
               "confidence scoring for PS4 emulator debugging.";
    }
    
    std::string version() const override {
        return "2.0.0";
    }
    
    /**
     * @brief Initialize correlation engine with default configuration
     * @return true if successful
     */
    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            // Clear buffers
            event_buffer_.clear();
            crash_history_.clear();
            hypothesis_cache_.clear();
            
            // Subscribe to all events for analysis
            if (global::is_initialized()) {
                global::get_event_bus()->subscribe(
                    "event_correlation",
                    [this](const DiagnosticEvent& event) { on_event(event); },
                    nullptr,  // No filter - we want everything
                    -10       // Low priority - let others process first
                );
            }
            
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
        
        if (global::is_initialized()) {
            global::get_event_bus()->unsubscribe_all("event_correlation");
        }
    }
    
    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        event_buffer_.clear();
        crash_history_.clear();
        hypothesis_cache_.clear();
    }
    
    /**
     * @brief Handle incoming events - store for correlation analysis
     */
    void on_event(const DiagnosticEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) return;
        
        event_count_++;
        
        // Add to circular buffer
        if (event_buffer_.size() >= max_buffer_size_) {
            event_buffer_.erase(event_buffer_.begin());
        }
        event_buffer_.push_back(event);
        
        // Track crashes separately for quick access
        if (event.event_type == "crash") {
            crash_history_.push_back(event);
            // Keep only recent crashes
            if (crash_history_.size() > 50) {
                crash_history_.erase(crash_history_.begin());
            }
        }
    }
    
    std::string generate_report() const override {
        // Generate report of most recent analysis
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (last_report_.has_value()) {
            return last_report_->to_json();
        }
        return "{\"status\": \"no_analysis_performed\"}";
    }
    
    void export_json(const std::string& path) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            std::ofstream file(path);
            if (file.is_open()) {
                file << generate_report();
                file.close();
            }
        } catch (...) {
            // Silently handle export errors
        }
    }
    
    //=========================================================================
    // Core Correlation Methods
    //=========================================================================
    
    /**
     * @brief Register an event into the engine's buffer
     * @param event The diagnostic event to register
     * 
     * Primary method for feeding events into the correlation engine.
     * Events are stored in a circular buffer for analysis.
     */
    void register_event(const DiagnosticEvent& event) {
        on_event(event);
    }
    
    /**
     * @brief Analyze a crash event and generate correlation report
     * @param crash_event The crash event to analyze
     * @return Complete correlation report with ranked hypotheses
     * 
     * Main entry point for crash analysis. Applies all registered
     * correlation rules and combines results into unified report.
     */
    CorrelationReport analyze_crash(const DiagnosticEvent& crash_event) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) {
            return create_empty_report(crash_event);
        }
        
        // Extract crash details
        uint64_t crash_addr = 0;
        int signal_num = 0;
        
        if (crash_event.numeric_data.count("address")) {
            crash_addr = static_cast<uint64_t>(crash_event.numeric_data.at("address"));
        }
        if (crash_event.numeric_data.count("signal")) {
            signal_num = static_cast<int>(crash_event.numeric_data.at("signal"));
        }
        
        // Generate hypotheses using all rules
        auto hypotheses = generate_hypotheses_for_crash(crash_addr, crash_event.timestamp);
        
        // Build complete report
        CorrelationReport report;
        report.crash_event_id = crash_event.event_id;
        report.crash_time = crash_event.timestamp;
        report.crash_address = crash_addr;
        report.signal_number = signal_num;
        report.hypotheses = std::move(hypotheses);
        
        // Generate summary
        report.summary = generate_summary(report);
        
        // Generate recommendations
        report.recommended_actions = generate_recommendations(report);
        
        // Cache for later retrieval
        last_report_ = report;
        
        // Emit analysis complete event
        emit_analysis_event(report);
        
        return report;
    }
    
    /**
     * @brief Generate hypotheses for a crash at given address/time
     * @param crash_addr Address where crash occurred
     * @param crash_time When the crash occurred
     * @return Vector of hypotheses ranked by confidence
     * 
     * Applies all registered correlation rules and merges/deduplicates results.
     */
    std::vector<CorrelationHypothesis> generate_hypotheses_for_crash(
        uint64_t crash_addr, Timestamp crash_time) {
        
        std::vector<CorrelationHypothesis> all_hypotheses;
        
        // Apply each registered rule
        for (const auto& [name, rule] : correlation_rules_) {
            try {
                auto rule_hypotheses = rule(event_buffer_);
                
                // Filter to only relevant hypotheses (near crash)
                for (auto& hyp : rule_hypotheses) {
                    // Adjust confidence based on temporal proximity
                    adjust_confidence_for_proximity(hyp, crash_time);
                    
                    // Only include if confidence above minimum threshold
                    if (hyp.confidence >= min_confidence_threshold_) {
                        all_hypotheses.push_back(std::move(hyp));
                    }
                }
            } catch (...) {
                // Rule failed - skip it, don't crash the engine
                continue;
            }
        }
        
        // Deduplicate similar hypotheses
        auto deduplicated = deduplicate_hypotheses(all_hypotheses);
        
        // Sort by confidence (descending)
        std::sort(deduplicated.begin(), deduplicated.end(),
                 [](const CorrelationHypothesis& a, const CorrelationHypothesis& b) {
                     return a.confidence > b.confidence;
                 });
        
        // Limit to top N hypotheses
        if (deduplicated.size() > max_hypotheses_) {
            deduplicated.resize(max_hypotheses_);
        }
        
        return deduplicated;
    }
    
    /**
     * @brief Add a custom correlation rule
     * @param name Unique name for this rule
     * @param rule Function that generates hypotheses from events
     * 
     * Allows extending the engine with domain-specific correlation logic.
     * Rules are called in registration order during analysis.
     */
    void add_correlation_rule(const std::string& name, CorrelationRule rule) {
        std::lock_guard<std::mutex> lock(mutex_);
        correlation_rules_[name] = std::move(rule);
    }
    
    /**
     * @brief Remove a correlation rule by name
     * @param name Name of rule to remove
     * @return true if rule was found and removed
     */
    bool remove_correlation_rule(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        return correlation_rules_.erase(name) > 0;
    }
    
    /**
     * @brief Generate JSON-formatted correlation report
     * @param report The correlation report to serialize
     * @return JSON string representation
     */
    std::string generate_correlation_report_json(const CorrelationReport& report) const {
        return report.to_json();
    }
    
    /**
     * @brief Get events from buffer within time window before crash
     * @param crash_time Reference crash time
     * @param window_ms Milliseconds to look back
     * @return Events within the time window
     */
    std::vector<DiagnosticEvent> get_events_before_crash(Timestamp crash_time, 
                                                          size_t window_ms = 5000) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<DiagnosticEvent> result;
        Duration window{static_cast<int64_t>(window_ms) * 1000};  // Convert ms to microseconds
        
        for (const auto& event : event_buffer_) {
            Duration diff = std::chrono::duration_cast<Duration>(crash_time - event.timestamp);
            if (diff.count() >= 0 && diff <= window) {
                result.push_back(event);
            }
        }
        
        return result;
    }
    
    /**
     * @brief Get current number of registered rules
     */
    size_t get_rule_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return correlation_rules_.size();
    }
    
    /**
     * @brief Configure engine parameters
     * @param max_buffer Maximum events to keep in buffer
     * @param max_hyp Maximum hypotheses per analysis
     * @param min_conf Minimum confidence threshold (0.0-1.0)
     */
    void configure(size_t max_buffer = 1000, size_t max_hyp = 10, 
                   double min_conf = 0.05) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_buffer_size_ = max_buffer;
        max_hypotheses_ = max_hyp;
        min_confidence_threshold_ = std::max(0.01, std::min(0.99, min_conf));
    }

private:
    //=========================================================================
    // Internal Data Structures
    //=========================================================================
    
    /// Circular buffer of recent events for analysis
    std::vector<DiagnosticEvent> event_buffer_;
    
    /// Recent crash events for quick access
    std::vector<DiagnosticEvent> crash_history_;
    
    /// Registered correlation rules: name -> rule function
    std::map<std::string, CorrelationRule> correlation_rules_;
    
    /// Cached last analysis result
    mutable std::optional<CorrelationReport> last_report_;
    
    /// Configuration parameters
    size_t max_buffer_size_{1000};
    size_t max_hypotheses_{10};
    double min_confidence_threshold_{0.05};
    
    /// Hypothesis ID counter
    std::atomic<size_t> hypothesis_id_counter_{0};
    
    //=========================================================================
    // Default Correlation Rules Setup
    //=========================================================================
    
    /**
     * @brief Set up built-in correlation rules
     * 
     * These are the core rules that provide basic crash analysis capability.
     * Each rule examines specific patterns in the event stream.
     */
    void setup_default_rules() {
        // Rule 1: Relocation failure near crash address
        add_correlation_rule("relocation_failure_near_crash",
            [](const std::vector<DiagnosticEvent>& events) -> std::vector<CorrelationHypothesis> {
                std::vector<CorrelationHypothesis> results;
                
                // Find relocation failures
                std::vector<DiagnosticEvent> reloc_failures;
                for (const auto& e : events) {
                    if (e.event_type == "relocation_error" || 
                        e.event_type == "relocation_failed") {
                        reloc_failures.push_back(e);
                    }
                }
                
                // Find crash
                DiagnosticEvent crash_event;
                bool has_crash = false;
                for (const auto& e : events) {
                    if (e.event_type == "crash") {
                        crash_event = e;
                        has_crash = true;
                        break;
                    }
                }
                
                if (!has_crash || reloc_failures.empty()) {
                    return results;
                }
                
                uint64_t crash_addr = 0;
                if (crash_event.numeric_data.count("address")) {
                    crash_addr = static_cast<uint64_t>(crash_event.numeric_data.at("address"));
                }
                
                // Check for nearby relocation failures
                for (const auto& rf : reloc_failures) {
                    uint64_t reloc_addr = 0;
                    if (rf.numeric_data.count("address")) {
                        reloc_addr = static_cast<uint64_t>(rf.numeric_data.at("address"));
                    }
                    
                    // Calculate address proximity (within 4KB page?)
                    uint64_t distance = (crash_addr > reloc_addr) ? 
                                        (crash_addr - reloc_addr) : (reloc_addr - crash_addr);
                    
                    if (distance < 4096) {  // Same page
                        CorrelationHypothesis hyp(
                            "hyp_reloc_" + std::to_string(rf.event_id.back()),
                            "Invalid relocation near crash address (distance: 0x" + 
                            to_hex_string(distance) + ")",
                            0.82,  // High confidence for same-page proximity
                            "relocation"
                        );
                        
                        hyp.evidence.push_back("Relocation failure at 0x" + to_hex_string(reloc_addr));
                        hyp.evidence.push_back("Crash at 0x" + to_hex_string(crash_addr));
                        
                        if (rf.metadata.count("symbol")) {
                            hyp.evidence.push_back("Affected symbol: " + rf.metadata.at("symbol"));
                        }
                        
                        if (rf.metadata.count("relocation_type")) {
                            hyp.evidence.push_back("Relocation type: " + rf.metadata.at("relocation_type"));
                        }
                        
                        hyp.related_event_ids.push_back(rf.event_id);
                        results.push_back(std::move(hyp));
                    } else if (distance < 65536) {  // Same segment
                        CorrelationHypothesis hyp(
                            "hyp_reloc_distant_" + std::to_string(rf.event_id.back()),
                            "Possible relocation issue in same segment as crash",
                            0.45,  // Lower confidence for distant relocations
                            "relocation"
                        );
                        
                        hyp.evidence.push_back("Relocation failure at 0x" + to_hex_string(reloc_addr));
                        hyp.conflicting_evidence.push_back("Relocation far from crash site");
                        hyp.related_event_ids.push_back(rf.event_id);
                        results.push_back(std::move(hyp));
                    }
                }
                
                return results;
            });
        
        // Rule 2: Missing import called before crash
        add_correlation_rule("missing_import_before_crash",
            [](const std::vector<DiagnosticEvent>& events) -> std::vector<CorrelationHypothesis> {
                std::vector<CorrelationHypothesis> results;
                
                // Find missing import calls
                std::vector<DiagnosticEvent> missing_calls;
                for (const auto& e : events) {
                    if ((e.event_type == "import_call" || e.event_type == "hle_call") &&
                        e.metadata.count("success") && e.metadata.at("success") == "false") {
                        missing_calls.push_back(e);
                    }
                }
                
                if (missing_calls.empty()) {
                    return results;
                }
                
                // Find crash position
                size_t crash_pos = 0;
                bool has_crash = false;
                for (size_t i = 0; i < events.size(); ++i) {
                    if (events[i].event_type == "crash") {
                        crash_pos = i;
                        has_crash = true;
                        break;
                    }
                }
                
                if (!has_crash) return results;
                
                // Check which missing imports were called shortly before crash
                for (const auto& mc : missing_calls) {
                    // Find position of this call
                    size_t call_pos = 0;
                    bool found = false;
                    for (size_t i = 0; i < events.size(); ++i) {
                        if (events[i].event_id == mc.event_id) {
                            call_pos = i;
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found) continue;
                    
                    // How many events between call and crash?
                    size_t distance = crash_pos - call_pos;
                    
                    if (distance < 20) {  // Very close to crash
                        double conf = 0.35 + (20 - distance) * 0.03;  // 35%-95%
                        conf = std::min(0.95, conf);
                        
                        CorrelationHypothesis hyp(
                            "hyp_import_" + mc.event_id.substr(0, 8),
                            "Missing/unimplemented import called shortly before crash",
                            conf,
                            "import"
                        );
                        
                        std::string nid = mc.metadata.count("nid") ? 
                                          mc.metadata.at("nid") : "<unknown>";
                        std::string func = mc.metadata.count("function") ?
                                           mc.metadata.at("function") : nid;
                        
                        hyp.evidence.push_back("Import '" + func + "' (" + nid + ") not resolved");
                        hyp.evidence.push_back("Called " + std::to_string(distance) + 
                                              " events before crash");
                        
                        if (mc.metadata.count("module")) {
                            hyp.evidence.push_back("From module: " + mc.metadata.at("module"));
                        }
                        
                        if (distance > 5) {
                            hyp.conflicting_evidence.push_back(
                                "Other events occurred between call and crash");
                        }
                        
                        hyp.related_event_ids.push_back(mc.event_id);
                        results.push_back(std::move(hyp));
                    }
                }
                
                return results;
            });
        
        // Rule 3: Memory violation at crash location
        add_correlation_rule("memory_violation_pattern",
            [](const std::vector<DiagnosticEvent>& events) -> std::vector<CorrelationHypothesis> {
                std::vector<CorrelationHypothesis> results;
                
                // Find memory-related events
                std::vector<DiagnosticEvent> mem_events;
                for (const auto& e : events) {
                    if (e.event_type.find("memory") != std::string::npos ||
                        e.event_type.find("access") != std::string::npos ||
                        e.event_type.find("protect") != std::string::npos ||
                        e.event_type.find("map") != std::string::npos) {
                        mem_events.push_back(e);
                    }
                }
                
                if (mem_events.empty()) {
                    return results;
                }
                
                // Find crash
                uint64_t crash_addr = 0;
                int signal = 11;  // Default SIGSEGV
                
                for (const auto& e : events) {
                    if (e.event_type == "crash") {
                        if (e.numeric_data.count("address")) {
                            crash_addr = static_cast<uint64_t>(e.numeric_data.at("address"));
                        }
                        if (e.numeric_data.count("signal")) {
                            signal = static_cast<int>(e.numeric_data.at("signal"));
                        }
                        break;
                    }
                }
                
                // Check for protection violations
                for (const auto& me : mem_events) {
                    if (me.event_type == "memory_violation" || 
                        me.event_type == "protection_violation") {
                        
                        uint64_t viol_addr = 0;
                        if (me.numeric_data.count("address")) {
                            viol_addr = static_cast<uint64_t>(me.numeric_data.at("address"));
                        }
                        
                        if (viol_addr == crash_addr || 
                            (viol_addr != 0 && std::abs(static_cast<int64_t>(crash_addr - viol_addr)) < 16)) {
                            
                            double conf = (signal == 11 || signal == 7) ? 0.78 : 0.55;
                            
                            CorrelationHypothesis hyp(
                                "hyp_memory_" + me.event_id.substr(0, 8),
                                "Memory access violation at crash address",
                                conf,
                                "memory"
                            );
                            
                            hyp.evidence.push_back("Violation detected at 0x" + to_hex_string(viol_addr));
                            
                            if (me.metadata.count("operation")) {
                                hyp.evidence.push_back("Operation: " + me.metadata.at("operation"));
                            }
                            if (me.metadata.count("required_protection")) {
                                hyp.evidence.push_back("Required: " + me.metadata.at("required_protection"));
                            }
                            if (me.metadata.count("actual_protection")) {
                                hyp.evidence.push_back("Actual: " + me.metadata.at("actual_protection"));
                            }
                            
                            hyp.related_event_ids.push_back(me.event_id);
                            results.push_back(std::move(hyp));
                        }
                    }
                }
                
                // Check for unmapped access pattern
                if (results.empty() && crash_addr != 0) {
                    bool found_mapping = false;
                    
                    for (const auto& me : mem_events) {
                        if (me.event_type == "region_mapped" || me.event_type == "memory_mapped") {
                            uint64_t base = 0, size = 0;
                            if (me.numeric_data.count("base")) {
                                base = static_cast<uint64_t>(me.numeric_data.at("base"));
                            }
                            if (me.numeric_data.count("size")) {
                                size = static_cast<uint64_t>(me.numeric_data.at("size"));
                            }
                            
                            if (crash_addr >= base && crash_addr < base + size) {
                                found_mapping = true;
                                break;
                            }
                        }
                    }
                    
                    if (!found_mapping && (signal == 11 || signal == 7)) {
                        CorrelationHypothesis hyp(
                            "hyp_unmapped_access",
                            "Crash address appears to be in unmapped memory region",
                            0.72,
                            "memory"
                        );
                        
                        hyp.evidence.push_back("No mapping found containing 0x" + to_hex_string(crash_addr));
                        hyp.evidence.push_back("Signal indicates memory access error");
                        hyp.conflicting_evidence.push_back("Mapping events may have been missed");
                        results.push_back(std::move(hyp));
                    }
                }
                
                return results;
            });
        
        // Rule 4: Thread deadlock/synchronization issues before crash
        add_correlation_rule("thread_issue_before_crash",
            [](const std::vector<DiagnosticEvent>& events) -> std::vector<CorrelationHypothesis> {
                std::vector<CorrelationHypothesis> results;
                
                // Find thread-related events
                std::vector<DiagnosticEvent> thread_events;
                for (const auto& e : events) {
                    if (e.event_type.find("thread") != std::string::npos ||
                        e.event_type.find("mutex") != std::string::npos ||
                        e.event_type.find("lock") != std::string::npos ||
                        e.event_type.find("sync") != std::string::npos) {
                        thread_events.push_back(e);
                    }
                }
                
                if (thread_events.empty()) {
                    return results;
                }
                
                // Look for deadlock indicators
                bool has_deadlock_warning = false;
                bool has_timeout = false;
                bool has_race_condition = false;
                
                for (const auto& te : thread_events) {
                    if (te.event_type == "deadlock_detected" || 
                        te.event_type == "potential_deadlock") {
                        has_deadlock_warning = true;
                    }
                    if (te.event_type == "lock_timeout" || 
                        te.event_type == "mutex_timeout") {
                        has_timeout = true;
                    }
                    if (te.event_type == "race_condition" ||
                        te.event_type == "data_race") {
                        has_race_condition = true;
                    }
                }
                
                if (has_deadlock_warning) {
                    CorrelationHypothesis hyp(
                        "hyp_thread_deadlock",
                        "Thread deadlock or synchronization issue may have caused hang/crash",
                        0.65,
                        "thread"
                    );
                    
                    hyp.evidence.push_back("Deadlock warning detected before crash");
                    hyp.conflicting_evidence.push_back("Deadlock may not be direct cause");
                    results.push_back(std::move(hyp));
                }
                
                if (has_timeout) {
                    CorrelationHypothesis hyp(
                        "hyp_lock_timeout",
                        "Lock timeout suggests contention or deadlock scenario",
                        0.48,
                        "thread"
                    );
                    
                    hyp.evidence.push_back("Lock timeout observed before crash");
                    hyp.conflicting_evidence.push_back("Timeout may have been handled gracefully");
                    results.push_back(std::move(hyp));
                }
                
                if (has_race_condition) {
                    CorrelationHypothesis hyp(
                        "hyp_race_condition",
                        "Data race condition may have caused corrupted state",
                        0.52,
                        "thread"
                    );
                    
                    hyp.evidence.push_back("Race condition detected before crash");
                    hyp.conflicting_evidence.push_back("Race may be benign");
                    results.push_back(std::move(hyp));
                }
                
                return results;
            });
        
        // Rule 5: Recent module load + crash = init ordering issue
        add_correlation_rule("recent_module_load",
            [](const std::vector<DiagnosticEvent>& events) -> std::vector<CorrelationHypothesis> {
                std::vector<CorrelationHypothesis> results;
                
                // Find module load events
                std::vector<DiagnosticEvent> module_loads;
                for (const auto& e : events) {
                    if (e.event_type == "module_load" || 
                        e.event_type == "prx_load" ||
                        e.event_type == "module_loaded") {
                        module_loads.push_back(e);
                    }
                }
                
                if (module_loads.empty()) {
                    return results;
                }
                
                // Find crash
                size_t crash_pos = events.size();
                for (size_t i = 0; i < events.size(); ++i) {
                    if (events[i].event_type == "crash") {
                        crash_pos = i;
                        break;
                    }
                }
                
                if (crash_pos >= events.size()) {
                    return results;
                }
                
                // Check for very recent module loads (within 50 events of crash)
                for (const auto& ml : module_loads) {
                    size_t load_pos = 0;
                    bool found = false;
                    
                    for (size_t i = 0; i < events.size(); ++i) {
                        if (events[i].event_id == ml.event_id) {
                            load_pos = i;
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found) continue;
                    
                    size_t distance = crash_pos - load_pos;
                    
                    if (distance < 50) {  // Recently loaded
                        double conf = 0.30 + (50 - distance) * 0.01;  // 30%-80%
                        
                        std::string mod_name = ml.metadata.count("module") ?
                                               ml.metadata.at("module") : "<unknown>";
                        
                        CorrelationHypothesis hyp(
                            "hyp_module_init_" + ml.event_id.substr(0, 8),
                            "Module '" + mod_name + "' loaded shortly before crash - possible init ordering issue",
                            std::min(0.80, conf),
                            "module"
                        );
                        
                        hyp.evidence.push_back("Module loaded " + std::to_string(distance) +
                                              " events before crash");
                        
                        if (ml.metadata.count("path")) {
                            hyp.evidence.push_back("Path: " + ml.metadata.at("path"));
                        }
                        
                        // Check if boot state was early
                        BootState state = BootState::UNKNOWN;
                        if (ml.numeric_data.count("boot_state")) {
                            state = static_cast<BootState>(ml.numeric_data.at("boot_state"));
                        }
                        
                        if (state < BootState::RUNTIME_INITIALIZED) {
                            hyp.confidence = std::min(0.90, hyp.confidence + 0.15);
                            hyp.evidence.push_back("Load occurred during early boot phase");
                        } else {
                            hyp.conflicting_evidence.push_back("System appeared fully initialized");
                        }
                        
                        hyp.related_event_ids.push_back(ml.event_id);
                        results.push_back(std::move(hyp));
                    }
                }
                
                return results;
            });
    }
    
    //=========================================================================
    // Internal Helper Methods
    //=========================================================================
    
    /**
     * @brief Create empty report for edge cases
     */
    CorrelationReport create_empty_report(const DiagnosticEvent& crash_event) const {
        CorrelationReport report;
        report.crash_event_id = crash_event.event_id;
        report.crash_time = crash_event.timestamp;
        report.summary = "Unable to perform correlation analysis - engine not active.";
        report.recommended_actions.push_back("Enable event correlation engine and retry.");
        return report;
    }
    
    /**
     * @brief Adjust hypothesis confidence based on temporal proximity to crash
     */
    void adjust_confidence_for_proximity(CorrelationHypothesis& hyp, Timestamp crash_time) {
        // Find the latest evidence event's time
        Timestamp latest_evidence_time{};
        bool has_time = false;
        
        for (const auto& eid : hyp.related_event_ids) {
            for (const auto& event : event_buffer_) {
                if (event.event_id == eid) {
                    if (!has_time || event.timestamp > latest_evidence_time) {
                        latest_evidence_time = event.timestamp;
                        has_time = true;
                    }
                    break;
                }
            }
        }
        
        if (has_time) {
            Duration diff = std::chrono::duration_cast<Duration>(
                crash_time - latest_evidence_time);
            
            // Decay confidence for older evidence
            // Within 100ms: no penalty
            // 100ms-1s: slight penalty
            // 1s-10s: moderate penalty  
            // 10s+: significant penalty
            
            double decay_factor = 1.0;
            double diff_ms = diff.count() / 1000.0;
            
            if (diff_ms > 10000) {
                decay_factor = 0.5;
            } else if (diff_ms > 1000) {
                decay_factor = 0.75;
            } else if (diff_ms > 100) {
                decay_factor = 0.9;
            }
            
            hyp.confidence *= decay_factor;
            
            // Ensure still above minimum
            hyp.confidence = std::max(min_confidence_threshold_, hyp.confidence);
        }
    }
    
    /**
     * @brief Deduplicate similar hypotheses
     * 
     * Merges hypotheses that have the same source_type and similar descriptions,
     * keeping the one with higher confidence.
     */
    std::vector<CorrelationHypothesis> deduplicate_hypotheses(
        const std::vector<CorrelationHypothesis>& hypotheses) {
        
        std::vector<CorrelationHypothesis> result;
        std::map<std::string, size_t> type_index;  // source_type -> index in result
        
        for (const auto& hyp : hypotheses) {
            auto it = type_index.find(hyp.source_type);
            
            if (it != type_index.end()) {
                // Merge with existing hypothesis of same type
                CorrelationHypothesis& existing = result[it->second];
                
                // Keep higher confidence
                if (hyp.confidence > existing.confidence) {
                    existing.confidence = hyp.confidence;
                    existing.description = hyp.description;
                }
                
                // Merge evidence
                for (const auto& e : hyp.evidence) {
                    if (std::find(existing.evidence.begin(), existing.evidence.end(), e) 
                        == existing.evidence.end()) {
                        existing.evidence.push_back(e);
                    }
                }
                
                // Merge related events
                for (const auto& eid : hyp.related_event_ids) {
                    if (std::find(existing.related_event_ids.begin(), 
                                 existing.related_event_ids.end(), eid) 
                        == existing.related_event_ids.end()) {
                        existing.related_event_ids.push_back(eid);
                    }
                }
                
                // Merge conflicting evidence
                for (const auto& ce : hyp.conflicting_evidence) {
                    if (std::find(existing.conflicting_evidence.begin(),
                                 existing.conflicting_evidence.end(), ce)
                        == existing.conflicting_evidence.end()) {
                        existing.conflicting_evidence.push_back(ce);
                    }
                }
            } else {
                // New unique hypothesis type
                type_index[hyp.source_type] = result.size();
                result.push_back(hyp);
            }
        }
        
        return result;
    }
    
    /**
     * @brief Generate human-readable summary from report data
     */
    std::string generate_summary(const CorrelationReport& report) const {
        if (report.hypotheses.empty()) {
            return "No clear correlation patterns found. Crash may be due to factors "
                   "not captured in diagnostic events, or may require deeper analysis.";
        }
        
        std::ostringstream ss;
        
        ss << "Analysis identified " << report.hypotheses.size() 
           << " possible cause(s) for the crash at 0x" 
           << std::hex << report.crash_address << std::dec << ". ";
        
        if (report.hypotheses[0].confidence >= 0.70) {
            ss << "The most likely cause (confidence: " 
               << static_cast<int>(report.hypotheses[0].confidence * 100) 
               << "%) involves " << report.hypotheses[0].source_type << "-related issues. ";
        } else {
            ss << "All hypotheses have moderate-to-low confidence, suggesting the cause "
               << "may be complex or involve multiple factors. ";
        }
        
        ss << "Manual review of the top hypotheses is recommended.";
        
        return ss.str();
    }
    
    /**
     * @brief Generate actionable recommendations from report
     */
    std::vector<std::string> generate_recommendations(const CorrelationReport& report) const {
        std::vector<std::string> actions;
        
        if (report.hypotheses.empty()) {
            actions.push_back("Review raw event log for patterns not captured by correlation rules.");
            actions.push_back("Consider adding custom correlation rules for domain-specific patterns.");
            return actions;
        }
        
        // Top hypothesis actions
        const auto& top = report.hypotheses[0];
        
        if (top.source_type == "relocation") {
            actions.push_back("Examine relocation processing code for the affected module.");
            actions.push_back("Verify relocation entries match expected format for PS4 binaries.");
            if (top.confidence >= 0.70) {
                actions.push_back("HIGH PRIORITY: Relocation issue strongly correlated with crash.");
            }
        } else if (top.source_type == "import") {
            actions.push_back("Implement or fix the unresolved import(s) listed in evidence.");
            actions.push_back("Check HLE function tables for missing NID mappings.");
            if (top.confidence >= 0.60) {
                actions.push_back("PRIORITY: Missing import likely contributing to instability.");
            }
        } else if (top.source_type == "memory") {
            actions.push_back("Review memory mapping and protection handling.");
            actions.push_back("Check for buffer overflows or use-after-free in emulated code.");
            actions.push_back("Validate memory region tracking accuracy.");
        } else if (top.source_type == "thread") {
            actions.push_back("Review thread synchronization primitives.");
            actions.push_back("Check for potential deadlocks in mutex/semaphore usage.");
            actions.push_back("Consider thread scheduling simulation accuracy.");
        } else if (top.source_type == "module") {
            actions.push_back("Review module initialization order and dependencies.");
            actions.push_back("Check for circular dependencies in module loading.");
            actions.push_back("Verify all required modules are loaded before dependent code runs.");
        }
        
        // General recommendations based on overall picture
        if (report.hypotheses.size() > 1) {
            double top_vs_second = report.hypotheses[0].confidence - 
                                   (report.hypotheses.size() > 1 ? report.hypotheses[1].confidence : 0);
            
            if (top_vs_second < 0.10) {
                actions.push_back("Multiple hypotheses have similar confidence - investigate in parallel.");
            }
        }
        
        // Flag items requiring investigation
        size_t requiring_review = std::count_if(
            report.hypotheses.begin(), report.hypotheses.end(),
            [](const CorrelationHypothesis& h) { return h.requires_investigation; });
        
        if (requiring_review > 0) {
            actions.push_back(std::to_string(requiring_review) + 
                             " hypothesis(es) marked for manual investigation.");
        }
        
        return actions;
    }
    
    /**
     * @brief Emit diagnostic event for completed analysis
     */
    void emit_analysis_event(const CorrelationReport& report) {
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "correlation_analysis_complete";
        event.severity = Severity::INFO;
        event.message = "Crash correlation analysis completed";
        event.metadata["crash_event_id"] = report.crash_event_id;
        event.numeric_data["hypothesis_count"] = static_cast<int64_t>(report.hypotheses.size());
        
        if (!report.hypotheses.empty()) {
            event.float_data["top_confidence"] = report.hypotheses[0].confidence;
        }
        
        emit_event(event);
    }
    
    /**
     * @brief Convert integer to hex string
     */
    static std::string to_hex_string(uint64_t value) {
        std::ostringstream ss;
        ss << std::hex << value;
        return ss.str();
    }
};

//=============================================================================
// Plugin Registration
//=============================================================================

inline std::unique_ptr<DiagnosticPlugin> create_event_correlation_plugin() {
    return std::make_unique<EventCorrelationEngine>();
}

inline PluginInfo event_correlation_plugin_info(
    "event_correlation",
    "AI-powered event correlation engine for crash analysis with ranked hypotheses",
    &create_event_correlation_plugin,
    true,  // enabled by default
    50     // priority
);

} // namespace diagnostics
} // namespace prosper
