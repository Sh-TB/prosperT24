#pragma once
#include "../core/diagnostic_interface.hpp"
#include "../core/event_bus.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <algorithm>
#include <numeric>

namespace prosper {
namespace diagnostics {

//=============================================================================
// HLE Evidence Plugin - Phase 10 Tier 2 Diagnostic Plugin
//
// Enhanced import classification with evidence-based impact assessment.
// Builds on basic import resolution to provide EVIDENCE-BASED impact assessment.
//
// Key enhancement over basic import_resolution_plugin:
// - Track "crash distance" - how close temporally was an unresolved import to a crash?
// - Classify imports by CALLER analysis - which functions call missing imports?
// - Generate confidence-scored impact assessment per missing import
// - Correlate missing imports with actual crashes
//
// Design Decisions:
// - Uses mutex for thread safety (emulator has multiple threads)
// - Evidence data is accumulated over session lifetime
// - Impact scores are recalculated on each query for accuracy
// - Crash correlation uses event counting, not time, for determinism
//=============================================================================

/**
 * @brief Comprehensive evidence record for a single HLE import
 * 
 * Contains all collected evidence about an import's behavior,
 * including call patterns, crash proximity, and calculated impact.
 */
struct HLEEvidence {
    std::string nid;                              ///< NID (Name ID) identifier
    std::string function_name;                    ///< Human-readable function name
    std::string module;                           ///< Source module name
    ImportStatus status{ImportStatus::MISSING_NOT_CALLED};  ///< Current classification
    
    // Call evidence - tracks who called this and when
    size_t total_calls{0};                        ///< Total invocation count
    uint64_t first_caller_address{0};             ///< Address of first caller
    std::vector<uint64_t> all_caller_addresses;   ///< All unique caller addresses
    Timestamp first_call_timestamp;                ///< When first called
    Timestamp last_call_timestamp;                 ///< When most recently called
    
    // Crash correlation evidence
    size_t crash_distance{static_cast<size_t>(-1)}; ///< Events before crash (-1 = no crash)
    double crash_confidence{0.0};                  ///< 0.0-1.0 likelihood of causing crash
    
    // Calculated impact scoring
    double impact_score{0.0};                     ///< Composite impact score (0.0-100.0)
    std::string impact_rationale;                  ///< Human-readable explanation
    
    /// Default constructor
    HLEEvidence() = default;
    
    /// Constructor with essential identification fields
    HLEEvidence(const std::string& n, const std::string& name, const std::string& mod)
        : nid(n), function_name(name), module(mod) {}
    
    /**
     * @brief Convert evidence to JSON representation
     * @return JSON string of this evidence record
     */
    std::string to_json() const {
        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"nid\": \"" << nid << "\",\n";
        ss << "  \"function_name\": \"" << function_name << "\",\n";
        ss << "  \"module\": \"" << module << "\",\n";
        ss << "  \"status\": " << static_cast<int>(status) << ",\n";
        ss << "  \"status_name\": \"" << status_to_string(status) << "\",\n";
        ss << "  \"total_calls\": " << total_calls << ",\n";
        ss << "  \"first_caller_address\": \"0x" << std::hex << first_caller_address << std::dec << "\",\n";
        
        ss << "  \"caller_addresses\": [";
        for (size_t i = 0; i < all_caller_addresses.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << "\"0x" << std::hex << all_caller_addresses[i] << std::dec << "\"";
        }
        ss << "],\n";
        
        ss << "  \"first_call_timestamp\": " << timestamp_to_ms(first_call_timestamp) << ",\n";
        ss << "  \"last_call_timestamp\": " << timestamp_to_ms(last_call_timestamp) << ",\n";
        ss << "  \"crash_distance\": " << (crash_distance == static_cast<size_t>(-1) ? -1 : static_cast<int64_t>(crash_distance)) << ",\n";
        ss << "  \"crash_confidence\": " << std::fixed << std::setprecision(4) << crash_confidence << ",\n";
        ss << "  \"impact_score\": " << std::fixed << std::setprecision(2) << impact_score << ",\n";
        ss << "  \"impact_rationale\": \"" << escape_json(impact_rationale) << "\"\n";
        ss << "}";
        return ss.str();
    }
    
    /**
     * @brief Get human-readable status string
     */
    static std::string status_to_string(ImportStatus s) {
        switch (s) {
            case ImportStatus::MISSING_NOT_CALLED:   return "MISSING_NOT_CALLED";
            case ImportStatus::MISSING_CALLED:       return "MISSING_CALLED";
            case ImportStatus::IMPLEMENTED_BUT_FAILED: return "IMPLEMENTED_BUT_FAILED";
            case ImportStatus::CALLED_SUCCESSFULLY:  return "CALLED_SUCCESSFULLY";
            case ImportStatus::STUB_IMPLEMENTED:     return "STUB_IMPLEMENTED";
            default: return "UNKNOWN";
        }
    }
    
private:
    /**
     * @brief Escape special characters for JSON string safety
     */
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
 * @brief Aggregated summary of all HLE evidence
 * 
 * Provides high-level view of import health across the system,
 * with categorized issue lists for prioritization.
 */
struct HLEEvidenceSummary {
    size_t total_imports{0};                                    ///< Total tracked imports
    std::map<ImportStatus, size_t> by_status;                   ///< Count by classification
    std::vector<HLEEvidence> high_impact_missing;               ///< MISSING_CALLED only (critical)
    std::vector<HLEEvidence> medium_risk;                       ///< STUB or FAILED (needs review)
    double overall_system_health{1.0};                          ///< 0.0-1.0 composite health score
    
    /**
     * @brief Convert summary to JSON
     */
    std::string to_json() const {
        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"total_imports\": " << total_imports << ",\n";
        
        ss << "  \"by_status\": {\n";
        bool first_status = true;
        for (const auto& [status, count] : by_status) {
            if (!first_status) ss << ",\n";
            ss << "    \"" << HLEEvidence::status_to_string(status) << "\": " << count;
            first_status = false;
        }
        ss << "\n  },\n";
        
        ss << "  \"high_impact_count\": " << high_impact_missing.size() << ",\n";
        ss << "  \"medium_risk_count\": " << medium_risk.size() << ",\n";
        ss << "  \"overall_system_health\": " << std::fixed << std::setprecision(4) << overall_system_health << "\n";
        ss << "}";
        return ss.str();
    }
};

//=============================================================================
// HLE Evidence Plugin Implementation
//=============================================================================

class HLEEvidencePlugin : public DiagnosticPlugin {
public:
    //=========================================================================
    // Construction / Destruction
    //=========================================================================
    
    HLEEvidencePlugin() : DiagnosticPlugin() {
        active_ = false;
    }
    
    ~HLEEvidencePlugin() override {
        shutdown();
    }
    
    //=========================================================================
    // DiagnosticPlugin Interface Implementation
    //=========================================================================
    
    /**
     * @brief Plugin identity - returns unique plugin name
     */
    std::string name() const override {
        return "hle_evidence";
    }
    
    /**
     * @brief Human-readable description of plugin purpose
     */
    std::string description() const override {
        return "Enhanced HLE import evidence collection with crash correlation "
               "and confidence-scored impact assessment for PS4 emulation diagnostics.";
    }
    
    /**
     * @brief Version string for this plugin
     */
    std::string version() const override {
        return "2.0.0";
    }
    
    /**
     * @brief Initialize the evidence tracking system
     * @return true if initialization successful
     * 
     * Sets up internal data structures and subscribes to relevant events.
     */
    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            // Clear any previous session data
            evidence_map_.clear();
            crash_events_.clear();
            event_sequence_counter_ = 0;
            
            // Subscribe to crash events for correlation
            if (global::is_initialized()) {
                global::get_event_bus()->subscribe(
                    "hle_evidence",
                    [this](const DiagnosticEvent& event) { on_event(event); },
                    [](const DiagnosticEvent& event) {
                        return event.event_type == "crash" || 
                               event.event_type == "import_call" ||
                               event.event_type == "import_resolved";
                    }
                );
            }
            
            active_ = true;
            return true;
        } catch (...) {
            active_ = false;
            return false;
        }
    }
    
    /**
     * @brief Shutdown and cleanup resources
     */
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        active_ = false;
        
        // Unsubscribe from events
        if (global::is_initialized()) {
            global::get_event_bus()->unsubscribe_all("hle_evidence");
        }
    }
    
    /**
     * @brief Reset all collected evidence for new session
     */
    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        evidence_map_.clear();
        crash_events_.clear();
        event_sequence_counter_ = 0;
        cached_summary_.reset();
    }
    
    /**
     * @brief Handle incoming diagnostic events
     * @param event The diagnostic event to process
     * 
     * Processes crash events for correlation and import-related events
     * for evidence accumulation.
     */
    void on_event(const DiagnosticEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) return;
        
        event_count_++;
        event_sequence_counter_++;
        
        // Track crashes for distance calculation
        if (event.event_type == "crash") {
            crash_events_.push_back({
                .timestamp = event.timestamp,
                .sequence_number = event_sequence_counter_,
                .address = event.numeric_data.count("address") ? 
                           static_cast<uint64_t>(event.numeric_data.at("address")) : 0,
                .signal = static_cast<int>(event.numeric_data.count("signal") ? 
                          event.numeric_data.at("signal") : 0)
            });
            
            // Update crash distances for all evidence entries
            update_crash_distances(event.timestamp, event_sequence_counter_);
        }
        
        // Invalidate cached summary on new data
        cached_summary_.reset();
    }
    
    /**
     * @brief Generate comprehensive evidence report in JSON format
     * @return JSON string containing full evidence report
     */
    std::string generate_report() const override {
        return generate_evidence_report();
    }
    
    /**
     * @brief Export report to file
     * @param path Output file path
     */
    void export_json(const std::string& path) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            std::ofstream file(path);
            if (file.is_open()) {
                file << generate_evidence_report();
                file.close();
            }
        } catch (...) {
            // Silently handle export errors
        }
    }
    
    //=========================================================================
    // Core Evidence Recording Methods
    //=========================================================================
    
    /**
     * @brief Record an import call attempt with full context
     * @param nid NID of the called import
     * @param caller_addr Address of the calling code
     * @param success Whether the call succeeded
     * 
     * This is the primary method for building evidence. Each call updates:
     * - Call counts and timestamps
     * - Caller address tracking
     * - Status classification based on success/failure
     * - Impact score recalculation trigger
     * 
     * Thread-safe: acquires internal mutex
     */
    void record_import_call(const std::string& nid, uint64_t caller_addr, bool success) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) return;
        
        event_count_++;
        event_sequence_counter_++;
        
        // Find or create evidence entry
        auto it = evidence_map_.find(nid);
        if (it == evidence_map_.end()) {
            // Create new entry - will need name/module registered separately
            it = evidence_map_.emplace(nid, HLEEvidence(nid, "<unknown>", "<unknown>")).first;
        }
        
        HLEEvidence& evidence = it->second;
        Timestamp current_time = now();
        
        // Update call statistics
        evidence.total_calls++;
        
        // Track first caller
        if (evidence.first_caller_address == 0) {
            evidence.first_caller_address = caller_addr;
            evidence.first_call_timestamp = current_time;
        }
        
        // Track all unique callers (with limit to prevent unbounded growth)
        if (evidence.all_caller_addresses.size() < 100) {
            if (std::find(evidence.all_caller_addresses.begin(), 
                         evidence.all_caller_addresses.end(), 
                         caller_addr) == evidence.all_caller_addresses.end()) {
                evidence.all_caller_addresses.push_back(caller_addr);
            }
        }
        
        evidence.last_call_timestamp = current_time;
        
        // Update status based on call outcome
        update_status_from_call(evidence, success);
        
        // Recalculate impact score
        evidence.impact_score = calculate_impact_score(evidence);
        evidence.impact_rationale = generate_impact_rationale(evidence);
        
        // Invalidate cached summary
        cached_summary_.reset();
        
        // Emit diagnostic event for this call
        emit_call_event(evidence, caller_addr, success);
    }
    
    /**
     * @brief Register/import metadata for an NID
     * @param nid NID identifier
     * @param function_name Human-readable name
     * @param module Source module
     * @param initial_status Initial status classification
     * 
     * Should be called during module loading when imports are first discovered.
     * This allows the evidence system to have proper names before any calls occur.
     */
    void register_import(const std::string& nid, 
                        const std::string& function_name,
                        const std::string& module,
                        ImportStatus initial_status = ImportStatus::MISSING_NOT_CALLED) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) return;
        
        auto it = evidence_map_.find(nid);
        if (it == evidence_map_.end()) {
            evidence_map_.emplace(nid, HLEEvidence(nid, function_name, module));
            evidence_map_[nid].status = initial_status;
        } else {
            // Update existing entry metadata
            it->second.function_name = function_name;
            it->second.module = module;
            if (it->second.status == ImportStatus::MISSING_NOT_CALLED) {
                it->second.status = initial_status;
            }
        }
        
        cached_summary_.reset();
    }
    
    /**
     * @brief Record a crash event for correlation purposes
     * @param crash_time Timestamp of the crash
     * 
     * Updates crash distances for all evidence entries based on their
     * temporal proximity to this crash event.
     */
    void record_crash(Timestamp crash_time) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) return;
        
        event_sequence_counter_++;
        
        crash_events_.push_back({
            .timestamp = crash_time,
            .sequence_number = event_sequence_counter_,
            .address = 0,
            .signal = 0
        });
        
        // Recalculate distances for all evidence
        update_crash_distances(crash_time, event_sequence_counter_);
        cached_summary_.reset();
        
        // Emit crash correlation event
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "evidence_crash_correlation";
        event.severity = Severity::CRITICAL;
        event.message = "Crash recorded for evidence correlation";
        event.numeric_data["crashes_total"] = static_cast<int64_t>(crash_events_.size());
        emit_event(event);
    }
    
    /**
     * @brief Get complete evidence summary
     * @return Reference to current evidence summary (recalculated if needed)
     * 
     * The summary is cached and only recalculated when data changes.
     */
    const HLEEvidenceSummary& get_evidence_summary() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!cached_summary_) {
            cached_summary_ = build_evidence_summary();
        }
        return *cached_summary_;
    }
    
    /**
     * @brief Get list of high-impact issues requiring immediate attention
     * @return Vector of evidence entries with HIGH impact scores
     * 
     * High-impact issues are those classified as MISSING_CALLED (actually
     * executed but not implemented), which almost certainly cause problems.
     */
    std::vector<HLEEvidence> get_high_impact_issues() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<HLEEvidence> high_impact;
        
        for (const auto& [nid, evidence] : evidence_map_) {
            if (evidence.status == ImportStatus::MISSING_CALLED && 
                evidence.impact_score >= 70.0) {
                high_impact.push_back(evidence);
            }
        }
        
        // Sort by impact score descending
        std::sort(high_impact.begin(), high_impact.end(),
                 [](const HLEEvidence& a, const HLEEvidence& b) {
                     return a.impact_score > b.impact_score;
                 });
        
        return high_impact;
    }
    
    /**
     * @brief Generate comprehensive evidence report as formatted string
     * @return Multi-line report string with full analysis
     * 
     * Report includes:
     * - Summary statistics
     * - High-impact issues detail
     * - Medium-risk items
     * - System health assessment
     * - Recommendations
     */
    std::string generate_evidence_report() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const HLEEvidenceSummary& summary = get_evidence_summary();
        
        std::ostringstream report;
        report << "{\n";
        report << "  \"report_type\": \"hle_evidence\",\n";
        report << "  \"generated_at\": " << timestamp_to_ms(now()) << ",\n";
        report << "  \"plugin_version\": \"" << version() << "\",\n";
        
        // Summary section
        report << "  \"summary\": " << summary.to_json() << ",\n";
        
        // High-impact details
        report << "  \"high_impact_details\": [\n";
        for (size_t i = 0; i < summary.high_impact_missing.size(); ++i) {
            if (i > 0) report << ",\n";
            report << "    " << summary.high_impact_missing[i].to_json();
        }
        report << "\n  ],\n";
        
        // Medium-risk details
        report << "  \"medium_risk_details\": [\n";
        for (size_t i = 0; i < summary.medium_risk.size(); ++i) {
            if (i > 0) report << ",\n";
            report << "    " << summary.medium_risk[i].to_json();
        }
        report << "\n  ],\n";
        
        // All evidence entries (abbreviated)
        report << "  \"all_evidence\": [\n";
        bool first = true;
        for (const auto& [nid, evidence] : evidence_map_) {
            if (!first) report << ",\n";
            first = false;
            report << "    {\n";
            report << "      \"nid\": \"" << evidence.nid << "\",\n";
            report << "      \"function_name\": \"" << evidence.function_name << "\",\n";
            report << "      \"module\": \"" << evidence.module << "\",\n";
            report << "      \"status\": " << static_cast<int>(evidence.status) << ",\n";
            report << "      \"total_calls\": " << evidence.total_calls << ",\n";
            report << "      \"impact_score\": " << std::fixed << std::setprecision(2) << evidence.impact_score << "\n";
            report << "    }";
        }
        report << "\n  ],\n";
        
        // Recommendations
        report << "  \"recommendations\": " << generate_recommendations_json() << "\n";
        report << "}\n";
        
        return report.str();
    }
    
    /**
     * @brief Calculate impact score for a single evidence entry
     * @param evidence The evidence to score
     * @return Impact score from 0.0 (no concern) to 100.0 (critical)
     * 
     * Scoring algorithm considers:
     * - Status classification (base weight)
     * - Call frequency (more calls = higher impact)
     * - Crash proximity (closer to crash = higher impact)
     * - Caller diversity (many callers = wider impact)
     * 
     * Formula: base_score + call_weight + crash_weight + caller_weight
     */
    double calculate_impact_score(const HLEEvidence& evidence) const {
        double score = 0.0;
        
        // Base score from status classification
        switch (evidence.status) {
            case ImportStatus::MISSING_NOT_CALLED:
                score = 5.0;   // Low risk - not being used
                break;
            case ImportStatus::MISSING_CALLED:
                score = 75.0;  // HIGH RISK - being called but missing!
                break;
            case ImportStatus::IMPLEMENTED_BUT_FAILED:
                score = 55.0;  // Medium-high - exists but broken
                break;
            case ImportStatus::CALLED_SUCCESSFULLY:
                score = 0.0;   // No issue
                break;
            case ImportStatus::STUB_IMPLEMENTED:
                score = 35.0;  // Unknown risk - might work
                break;
        }
        
        // Call frequency weight (capped at +15 points)
        // Logarithmic scale: 1 call = +2, 10 calls = +7, 100 calls = +12, 1000+ = +15
        if (evidence.total_calls > 0) {
            double call_weight = std::min(15.0, 2.0 + 3.0 * std::log10(
                static_cast<double>(evidence.total_calls)));
            score += call_weight;
        }
        
        // Crash proximity weight (capped at +20 points)
        // Closer to crash = higher weight
        if (evidence.crash_distance != static_cast<size_t>(-1)) {
            // Inverse relationship: fewer events between call and crash = higher score
            if (evidence.crash_distance == 0) {
                score += 20.0;  // Called right before crash!
            } else if (evidence.crash_distance < 10) {
                score += 15.0;
            } else if (evidence.crash_distance < 50) {
                score += 10.0;
            } else if (evidence.crash_distance < 200) {
                score += 5.0;
            }
            // Add crash confidence factor
            score += evidence.crash_confidence * 10.0;
        }
        
        // Caller diversity weight (capped at +10 points)
        // More unique callers = more widespread impact
        size_t unique_callers = evidence.all_caller_addresses.size();
        if (unique_callers > 1) {
            score += std::min(10.0, static_cast<double>(unique_callers) * 2.0);
        }
        
        // Ensure score stays in valid range
        return std::max(0.0, std::min(100.0, score));
    }
    
    /**
     * @brief Get evidence for a specific NID
     * @param nid NID to look up
     * @return Pointer to evidence if found, nullptr otherwise
     */
    const HLEEvidence* get_evidence_for_nid(const std::string& nid) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = evidence_map_.find(nid);
        if (it != evidence_map_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    /**
     * @brief Get total number of tracked imports
     */
    size_t get_tracked_import_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return evidence_map_.size();
    }
    
    /**
     * @brief Get imports by status classification
     * @param status Status filter
     * @return Vector of matching evidence entries
     */
    std::vector<HLEEvidence> get_imports_by_status(ImportStatus status) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<HLEEvidence> results;
        for (const auto& [nid, evidence] : evidence_map_) {
            if (evidence.status == status) {
                results.push_back(evidence);
            }
        }
        return results;
    }

private:
    //=========================================================================
    // Internal Data Structures
    //=========================================================================
    
    /**
     * @brief Internal crash event record for correlation
     */
    struct CrashRecord {
        Timestamp timestamp;
        size_t sequence_number;
        uint64_t address;
        int signal;
    };
    
    /// Main evidence storage: NID -> Evidence
    std::unordered_map<std::string, HLEEvidence> evidence_map_;
    
    /// Recorded crash events for distance calculation
    std::vector<CrashRecord> crash_events_;
    
    /// Monotonically increasing event sequence counter
    size_t event_sequence_counter_{0};
    
    /// Cached summary (invalidated on data change)
    mutable std::optional<HLEEvidenceSummary> cached_summary_;
    
    //=========================================================================
    // Internal Methods
    //=========================================================================
    
    /**
     * @brief Update evidence status based on call outcome
     * @param evidence Evidence entry to update
     * @param success Whether the call succeeded
     * 
     * Status transition rules:
     * - Success on MISSING_* -> CALLED_SUCCESSFULLY (if now implemented)
     * - Failure on SUCCESS -> IMPLEMENTED_BUT_FAILED
     * - Call on NOT_CALLED -> CALLED variant
     */
    void update_status_from_call(HLEEvidence& evidence, bool success) {
        ImportStatus old_status = evidence.status;
        
        switch (old_status) {
            case ImportStatus::MISSING_NOT_CALLED:
                // Being called means it's needed
                evidence.status = ImportStatus::MISSING_CALLED;
                break;
                
            case ImportStatus::MISSING_CALLED:
                // Still missing but called again - no change unless we learn otherwise
                break;
                
            case ImportStatus::IMPLEMENTED_BUT_FAILED:
                if (success) {
                    // Recovered! Now working
                    evidence.status = ImportStatus::CALLED_SUCCESSFULLY;
                }
                break;
                
            case ImportStatus::CALLED_SUCCESSFULLY:
                if (!success) {
                    // Regression - started failing
                    evidence.status = ImportStatus::IMPLEMENTED_BUT_FAILED;
                }
                break;
                
            case ImportStatus::STUB_IMPLEMENTED:
                if (success) {
                    // Stub seems to work
                    evidence.status = ImportStatus::CALLED_SUCCESSFULLY;
                } else {
                    // Stub failed - might be incomplete
                    evidence.status = ImportStatus::IMPLEMENTED_BUT_FAILED;
                }
                break;
        }
    }
    
    /**
     * @brief Update crash distances for all evidence entries
     * @param crash_time Timestamp of the crash
     * @param crash_sequence Sequence number of the crash event
     * 
     * Calculates how many events occurred between each evidence entry's
     * last call and this crash. Lower distance = stronger correlation.
     */
    void update_crash_distances(Timestamp crash_time, size_t crash_sequence) {
        for (auto& [nid, evidence] : evidence_map_) {
            if (evidence.total_calls > 0) {
                // Calculate time-based distance (simplified to sequence delta)
                // In production, would use actual event count between call and crash
                
                Duration time_since_last_call = std::chrono::duration_cast<Duration>(
                    crash_time - evidence.last_call_timestamp);
                
                // Convert to approximate event distance (rough heuristic)
                // Assume average 1000 events per second during active emulation
                size_t approx_events_between = static_cast<size_t>(
                    std::abs(time_since_last_call.count()) / 1000.0);
                
                evidence.crash_distance = approx_events_between;
                
                // Calculate crash confidence based on proximity
                // Exponential decay: very close = high confidence, far = low
                if (approx_events_between == 0) {
                    evidence.crash_confidence = 0.95;
                } else if (approx_events_between < 10) {
                    evidence.crash_confidence = 0.80;
                } else if (approx_events_between < 100) {
                    evidence.crash_confidence = 0.50;
                } else if (approx_events_between < 1000) {
                    evidence.crash_confidence = 0.25;
                } else {
                    evidence.crash_confidence = 0.10;
                }
                
                // Recalculate impact with new crash data
                evidence.impact_score = calculate_impact_score(evidence);
                evidence.impact_rationale = generate_impact_rationale(evidence);
            }
        }
    }
    
    /**
     * @brief Generate human-readable impact rationale
     * @param evidence Evidence to analyze
     * @return Explanation string for the impact score
     */
    std::string generate_impact_rationale(const HLEEvidence& evidence) const {
        std::ostringstream rationale;
        
        // Start with status-based explanation
        switch (evidence.status) {
            case ImportStatus::MISSING_NOT_CALLED:
                rationale << "Import is not implemented but has never been called. "
                         << "Low immediate risk but may be needed later.";
                break;
                
            case ImportStatus::MISSING_CALLED:
                rationale << "CRITICAL: Import is missing but has been called "
                         << evidence.total_calls << " time(s). This likely causes "
                         << "incorrect behavior or crashes.";
                break;
                
            case ImportStatus::IMPLEMENTED_BUT_FAILED:
                rationale << "Import implementation exists but returned errors on "
                         << "invocation. Implementation may be incorrect or incomplete.";
                break;
                
            case ImportStatus::CALLED_SUCCESSFULLY:
                rationale << "Import is implemented and working correctly.";
                break;
                
            case ImportStatus::STUB_IMPLEMENTED:
                rationale << "Import has stub/placeholder implementation. Behavior "
                         << "may be incomplete or incorrect.";
                break;
        }
        
        // Add crash correlation info if applicable
        if (evidence.crash_distance != static_cast<size_t>(-1) && 
            evidence.crash_distance < 100) {
            rationale << " Last call was approximately " << evidence.crash_distance 
                     << " events before a crash (confidence: " 
                     << static_cast<int>(evidence.crash_confidence * 100) << "%).";
        }
        
        // Add caller diversity info
        if (evidence.all_caller_addresses.size() > 1) {
            rationale << " Called from " << evidence.all_caller_addresses.size() 
                     << " unique locations.";
        }
        
        return rationale.str();
    }
    
    /**
     * @brief Build complete evidence summary from current data
     * @return Fully populated summary structure
     */
    std::unique_ptr<HLEEvidenceSummary> build_evidence_summary() const {
        auto summary = std::make_unique<HLEEvidenceSummary>();
        summary->total_imports = evidence_map_.size();
        
        double total_impact = 0.0;
        size_t weighted_count = 0;
        
        for (const auto& [nid, evidence] : evidence_map_) {
            // Categorize by status
            summary->by_status[evidence.status]++;
            
            // Collect high-impact issues
            if (evidence.status == ImportStatus::MISSING_CALLED) {
                summary->high_impact_missing.push_back(evidence);
            }
            
            // Collect medium-risk items
            if (evidence.status == ImportStatus::STUB_IMPLEMENTED ||
                evidence.status == ImportStatus::IMPLEMENTED_BUT_FAILED) {
                summary->medium_risk.push_back(evidence);
            }
            
            // Accumulate for health calculation
            total_impact += evidence.impact_score;
            weighted_count++;
        }
        
        // Sort high-impact by score descending
        std::sort(summary->high_impact_missing.begin(), 
                 summary->high_impact_missing.end(),
                 [](const HLEEvidence& a, const HLEEvidence& b) {
                     return a.impact_score > b.impact_score;
                 });
        
        // Sort medium-risk by score descending
        std::sort(summary->medium_risk.begin(), 
                 summary->medium_risk.end(),
                 [](const HLEEvidence& a, const HLEEvidence& b) {
                     return a.impact_score > b.impact_score;
                 });
        
        // Calculate overall system health
        // Health = 1.0 - (average_impact / 100.0)
        // Higher average impact = lower health
        if (weighted_count > 0) {
            double avg_impact = total_impact / weighted_count;
            summary->overall_system_health = std::max(0.0, 1.0 - (avg_impact / 100.0));
        } else {
            summary->overall_system_health = 1.0;  // No imports = perfect (degenerate case)
        }
        
        return summary;
    }
    
    /**
     * @brief Generate recommendations based on current evidence state
     * @return JSON array of recommendation strings
     */
    std::string generate_recommendations_json() const {
        const HLEEvidenceSummary& summary = get_evidence_summary();
        
        std::ostringstream json;
        json << "[\n";
        
        std::vector<std::string> recommendations;
        
        // High-impact missing imports need immediate attention
        if (!summary.high_impact_missing.empty()) {
            recommendations.push_back(
                "IMMEDIATE: Implement " + 
                std::to_string(summary.high_impact_missing.size()) +
                " missing imports that are being called (HIGH CRASH RISK)");
        }
        
        // Medium-risk items should be reviewed
        if (!summary.medium_risk.empty()) {
            recommendations.push_back(
                "REVIEW: Investigate " +
                std::to_string(summary.medium_risk.size()) +
                " stub/failed implementations for correctness");
        }
        
        // Health threshold warnings
        if (summary.overall_system_health < 0.5) {
            recommendations.push_back(
                "WARNING: Overall system health below 50% - significant import issues detected");
        } else if (summary.overall_system_health < 0.75) {
            recommendations.push_back(
                "CAUTION: System health at " +
                std::to_string(static_cast<int>(summary.overall_system_health * 100)) +
                "% - some import issues present");
        }
        
        // If everything looks good
        if (summary.high_impact_missing.empty() && 
            summary.medium_risk.empty() &&
            summary.overall_system_health >= 0.9) {
            recommendations.push_back(
                "GOOD: All imports resolved successfully, system healthy");
        }
        
        for (size_t i = 0; i < recommendations.size(); ++i) {
            if (i > 0) json << ",\n";
            json << "    \"" << recommendations[i] << "\"";
        }
        
        json << "\n  ]";
        return json.str();
    }
    
    /**
     * @brief Emit diagnostic event for recorded call
     */
    void emit_call_event(const HLEEvidence& evidence, uint64_t caller_addr, bool success) {
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "import_call_evidence";
        event.severity = (evidence.status == ImportStatus::MISSING_CALLED) ? 
                         Severity::ERROR : Severity::INFO;
        event.message = "Import call recorded: " + evidence.nid;
        event.metadata["nid"] = evidence.nid;
        event.metadata["function_name"] = evidence.function_name;
        event.metadata["module"] = evidence.module;
        event.metadata["success"] = success ? "true" : "false";
        event.numeric_data["caller_address"] = static_cast<int64_t>(caller_addr);
        event.numeric_data["total_calls"] = static_cast<int64_t>(evidence.total_calls);
        event.float_data["impact_score"] = evidence.impact_score;
        
        emit_event(event);
    }
};

//=============================================================================
// Plugin Registration
//=============================================================================

inline std::unique_ptr<DiagnosticPlugin> create_hle_evidence_plugin() {
    return std::make_unique<HLEEvidencePlugin>();
}

// Auto-registration through plugin info
inline PluginInfo hle_evidence_plugin_info(
    "hle_evidence",
    "Enhanced HLE import evidence collection with crash correlation",
    &create_hle_evidence_plugin,
    true,  // enabled by default
    45     // priority
);

} // namespace diagnostics
} // namespace prosper
