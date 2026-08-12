#pragma once
#include "../core/diagnostic_interface.hpp"
#include "../core/event_bus.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace prosper {
namespace diagnostics {

//=============================================================================
// Boot Timeline Plugin - Phase 9.5 Diagnostic Plugin
// 
// Tracks boot sequence timing for PS4 emulator diagnostics.
// Records timestamps for major boot events, calculates durations between phases,
// detects boot stalls and timeouts, and generates timeline reports.
//=============================================================================

/**
 * @brief Represents a single phase in the boot sequence with timing data
 */
struct BootPhase {
    std::string name;                    ///< Human-readable phase name (e.g., "ELF_LOAD", "PRX_LOAD")
    Timestamp start;                     ///< When this phase started
    Timestamp end;                       ///< When this phase completed (or max if ongoing)
    Duration duration;                   ///< Calculated duration of this phase
    
    /// Default constructor
    BootPhase() : duration(0) {}
    
    /// Full constructor
    BootPhase(const std::string& n, Timestamp s, Timestamp e)
        : name(n), start(s), end(e) {
        duration = std::chrono::duration_cast<Duration>(e - s);
    }
    
    /// Calculate duration in milliseconds
    double duration_ms() const {
        return std::chrono::duration<double, std::milli>(duration).count();
    }
    
    /// Serialize to JSON string
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"name\":\"" << name << "\",";
        oss << "\"start_ms\":" << std::fixed << std::setprecision(3) << timestamp_to_ms(start) << ",";
        oss << "\"end_ms\":" << std::fixed << std::setprecision(3) << timestamp_to_ms(end) << ",";
        oss << "\"duration_us\":" << duration.count() << ",";
        oss << "\"duration_ms\":" << std::fixed << std::setprecision(3) << duration_ms();
        oss << "}";
        return oss.str();
    }
};

/**
 * @brief Stall detection result structure
 */
struct StallDetectionResult {
    bool stalled{false};
    std::string stalled_phase;
    double stall_duration_ms{0.0};
    double timeout_threshold_ms{0.0};
    Severity severity{Severity::WARNING};
    std::string message;
    
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"stalled\":" << (stalled ? "true" : "false") << ",";
        oss << "\"stalled_phase\":\"" << stalled_phase << "\",";
        oss << "\"stall_duration_ms\":" << std::fixed << std::setprecision(3) << stall_duration_ms << ",";
        oss << "\"timeout_threshold_ms\":" << std::fixed << std::setprecision(3) << timeout_threshold_ms << ",";
        oss << "\"severity\":" << static_cast<int>(severity) << ",";
        oss << "\"message\":\"" << message << "\"";
        oss << "}";
        return oss.str();
    }
};

/**
 * @brief Boot Timeline Plugin implementation
 * 
 * This plugin provides comprehensive boot sequence tracking and analysis:
 * - Records timestamps for all major boot events (ELF load, PRX load, init, etc.)
 * - Calculates durations between consecutive phases
 * - Detects boot stalls and timeouts based on configurable thresholds
 * - Generates detailed timeline reports with phase durations and statistics
 */
class BootTimelinePlugin : public DiagnosticPlugin {
public:
    //=========================================================================
    // Constructor / Destructor
    //=========================================================================
    
    BootTimelinePlugin()
        : boot_start_time_(Timestamp::min()),
          current_boot_state_(BootState::POWER_ON),
          stall_timeout_ms_(5000.0),
          total_boot_duration_(0) {}
    
    virtual ~BootTimelinePlugin() override {
        shutdown();
    }
    
    //=========================================================================
    // DiagnosticPlugin Interface Implementation
    //=========================================================================
    
    std::string name() const override { return "BootTimeline"; }
    
    std::string version() const override { return "1.5.0"; }
    
    std::string description() const override {
        return "Tracks boot sequence timing, detects stalls, and generates timeline reports";
    }
    
    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Reset state for fresh boot tracking
        phases_.clear();
        boot_start_time_ = now();
        current_boot_state_ = BootState::POWER_ON;
        total_boot_duration_ = 0;
        
        // Record initial power-on event
        record_boot_phase_internal("POWER_ON", boot_start_time_);
        
        active_ = true;
        
        // Parse configuration
        auto it = config_.find("stall_timeout_ms");
        if (it != config_.end()) {
            try { stall_timeout_ms_ = std::stod(it->second); } catch (...) {}
        }
        
        it = config_.find("max_events");
        if (it != config_.end()) {
            try { max_events_ = static_cast<size_t>(std::stoull(it->second)); } catch (...) {}
        }
        
        // Emit initialization event
        emit_info("BootTimeline initialized successfully");
        
        return true;
    }
    
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Finalize any open phase
        if (!phases_.empty() && phases_.back().duration.count() == 0) {
            Timestamp end_time = now();
            phases_.back().end = end_time;
            phases_.back().duration = std::chrono::duration_cast<Duration>(
                end_time - phases_.back().start);
        }
        
        active_ = false;
        emit_info("BootTimeline shut down");
    }
    
    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        phases_.clear();
        boot_start_time_ = Timestamp::min();
        current_boot_state_ = BootState::UNKNOWN;
        total_boot_duration_ = 0;
        event_count_ = 0;
        stall_history_.clear();
    }
    
    void on_event(const DiagnosticEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) return;
        
        // Check event count limit
        if (event_count_ >= max_events_) {
            return;
        }
        
        // Process relevant events
        if (event.event_type == "boot_state_change" || 
            event.event_type == "phase_complete" ||
            event.source_plugin == "BootStateMachine") {
            
            auto state_it = event.numeric_data.find("boot_state");
            if (state_it != event.numeric_data.end()) {
                BootState new_state = static_cast<BootState>(state_it->second);
                on_state_changed(new_state, event.timestamp);
            }
            
            auto phase_name_it = event.metadata.find("phase_name");
            if (phase_name_it != event.metadata.end()) {
                record_boot_phase_internal(phase_name_it->second, event.timestamp);
            }
        }
        
        event_count_++;
    }
    
    std::string generate_report() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return generate_timeline_report_internal();
    }
    
    void export_json(const std::string& path) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ofstream file(path);
        if (!file.is_open()) {
            return;
        }
        
        file << generate_timeline_report_internal();
        file.close();
    }
    
    //=========================================================================
    // Public API Methods
    //=========================================================================
    
    /**
     * @brief Record a boot phase transition
     * @param phase_name Name of the boot phase (e.g., "ELF_LOADED", "PRX_LOADED")
     * @param timestamp Optional timestamp (defaults to now)
     */
    void record_boot_phase(const std::string& phase_name, 
                          Timestamp timestamp = Timestamp{}) {
        std::lock_guard<std::mutex> lock(mutex_);
        record_boot_phase_internal(phase_name, timestamp.time_since_epoch().count() == 0 
                                   ? now() : timestamp);
    }
    
    /**
     * @brief Get the timestamp for a specific boot phase
     * @param phase_name The phase to look up
     * @return Timestamp of the phase start, or Timestamp::min() if not found
     */
    Timestamp get_phase_timestamp(const std::string& phase_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& phase : phases_) {
            if (phase.name == phase_name) {
                return phase.start;
            }
        }
        return Timestamp::min();
    }
    
    /**
     * @brief Get complete phase information by name
     * @param phase_name The phase to look up
     * @return Pointer to BootPhase or nullptr if not found
     */
    const BootPhase* get_phase(const std::string& phase_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& phase : phases_) {
            if (phase.name == phase_name) {
                return &phase;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Detect if boot has stalled (current phase taking too long)
     * @return StallDetectionResult with details about any detected stall
     */
    StallDetectionResult detect_stall() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return detect_stall_internal();
    }
    
    /**
     * @brief Generate a comprehensive timeline report as JSON
     * @return JSON string containing full timeline data
     */
    std::string generate_timeline_report() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return generate_timeline_report_internal();
    }
    
    /**
     * @brief Get the number of recorded boot phases
     */
    size_t phase_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return phases_.size();
    }
    
    /**
     * @brief Get total boot duration in microseconds
     */
    Duration total_duration() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_boot_duration_;
    }
    
    /**
     * @brief Check if boot is complete
     */
    bool is_boot_complete() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_boot_state_ >= BootState::BOOT_COMPLETE;
    }
    
    /**
     * @brief Get current boot state
     */
    BootState current_state() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_boot_state_;
    }
    
    /**
     * @brief Get all recorded phases (copy)
     */
    std::vector<BootPhase> get_all_phases() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return phases_;
    }
    
    /**
     * @brief Set stall detection timeout threshold
     * @param timeout_ms Timeout in milliseconds before a phase is considered stalled
     */
    void set_stall_timeout(double timeout_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        stall_timeout_ms_ = timeout_ms;
    }
    
private:
    //=========================================================================
    // Internal Implementation Methods
    //=========================================================================
    
    /**
     * @brief Internal method to record a boot phase (assumes lock held)
     */
    void record_boot_phase_internal(const std::string& phase_name, Timestamp timestamp) {
        // Finalize previous phase if it exists and is still open
        if (!phases_.empty()) {
            auto& prev_phase = phases_.back();
            if (prev_phase.duration.count() == 0) {
                prev_phase.end = timestamp;
                prev_phase.duration = std::chrono::duration_cast<Duration>(
                    timestamp - prev_phase.start);
                
                // Update total duration
                total_boot_duration_ += prev_phase.duration;
            }
        }
        
        // Create new phase entry
        BootPhase new_phase(phase_name, timestamp, timestamp);
        new_phase.duration = Duration(0);  // Mark as ongoing
        
        phases_.push_back(std::move(new_phase));
        
        // Emit diagnostic event
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "boot_phase_recorded";
        event.severity = Severity::INFO;
        event.message = "Boot phase recorded: " + phase_name;
        event.metadata["phase_name"] = phase_name;
        event.numeric_data["timestamp_ms"] = static_cast<int64_t>(timestamp_to_ms(timestamp));
        event.numeric_data["phase_index"] = static_cast<int64_t>(phases_.size() - 1);
        
        emit_event(event);
    }
    
    /**
     * @brief Handle boot state change events
     */
    void on_state_changed(BootState new_state, Timestamp timestamp) {
        // Map boot states to phase names
        std::string phase_name;
        switch (new_state) {
            case BootState::POWER_ON:           phase_name = "POWER_ON"; break;
            case BootState::ELF_LOADED:         phase_name = "ELF_LOADED"; break;
            case BootState::PRX_LOADED:         phase_name = "PRX_LOADED"; break;
            case BootState::SEGMENTS_MAPPED:    phase_name = "SEGMENTS_MAPPED"; break;
            case BootState::RELOCATIONS_APPLIED: phase_name = "RELOCATIONS_APPLIED"; break;
            case BootState::IMPORTS_RESOLVED:   phase_name = "IMPORTS_RESOLVED"; break;
            case BootState::RUNTIME_INITIALIZED: phase_name = "RUNTIME_INITIALIZED"; break;
            case BootState::THREAD_STARTED:     phase_name = "THREAD_STARTED"; break;
            case BootState::MAIN_ENTRY:         phase_name = "MAIN_ENTRY"; break;
            case BootState::FIRST_RENDER:       phase_name = "FIRST_RENDER"; break;
            case BootState::BOOT_COMPLETE:      phase_name = "BOOT_COMPLETE"; break;
            case BootState::CRASHED:            phase_name = "CRASHED"; break;
            default:                            phase_name = "UNKNOWN_STATE"; break;
        }
        
        current_boot_state_ = new_state;
        record_boot_phase_internal(phase_name, timestamp);
        
        // If boot complete, finalize last phase
        if (new_state == BootState::BOOT_COMPLETE && !phases_.empty()) {
            auto& last_phase = phases_.back();
            if (last_phase.duration.count() == 0) {
                last_phase.end = timestamp;
                last_phase.duration = std::chrono::duration_cast<Duration>(
                    timestamp - last_phase.start);
                total_boot_duration_ += last_phase.duration;
            }
        }
    }
    
    /**
     * @brief Internal stall detection (assumes lock held)
     */
    StallDetectionResult detect_stall_internal() const {
        StallDetectionResult result;
        
        if (phases_.empty()) {
            result.message = "No phases recorded yet";
            return result;
        }
        
        const auto& current_phase = phases_.back();
        
        // Check if current phase is still ongoing (duration == 0 means ongoing)
        if (current_phase.duration.count() == 0) {
            Timestamp now_ts = now();
            Duration elapsed = std::chrono::duration_cast<Duration>(now_ts - current_phase.start);
            double elapsed_ms = std::chrono::duration<double, std::milli>(elapsed).count();
            
            if (elapsed_ms > stall_timeout_ms_) {
                result.stalled = true;
                result.stalled_phase = current_phase.name;
                result.stall_duration_ms = elapsed_ms;
                result.timeout_threshold_ms = stall_timeout_ms_;
                result.severity = elapsed_ms > stall_timeout_ms_ * 3.0 
                    ? Severity::CRITICAL : Severity::WARNING;
                result.message = "Boot phase '" + current_phase.name + "' has been running for " +
                    std::to_string(static_cast<int>(elapsed_ms)) + "ms (threshold: " +
                    std::to_string(static_cast<int>(stall_timeout_ms_)) + "ms)";
            }
        } else {
            result.message = "All phases completed, no active phase to check";
        }
        
        return result;
    }
    
    /**
     * @brief Generate comprehensive timeline report (assumes lock held)
     */
    std::string generate_timeline_report_internal() const {
        std::ostringstream json;
        
        json << "{\n";
        json << "  \"plugin\": \"" << name() << "\",\n";
        json << "  \"version\": \"" << version() << "\",\n";
        json << "  \"generated_at_ms\": " << std::fixed << std::setprecision(3) 
             << timestamp_to_ms(now()) << ",\n";
        json << "  \"current_state\": " << static_cast<int>(current_boot_state_) << ",\n";
        json << "  \"total_phases\": " << phases_.size() << ",\n";
        json << "  \"total_boot_duration_us\": " << total_boot_duration_.count() << ",\n";
        json << "  \"total_boot_duration_ms\": " << std::fixed << std::setprecision(3)
             << std::chrono::duration<double, std::milli>(total_boot_duration_).count() << ",\n";
        
        // Phases array
        json << "  \"phases\": [\n";
        for (size_t i = 0; i < phases_.size(); ++i) {
            json << "    " << phases_[i].to_json();
            if (i < phases_.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ],\n";
        
        // Phase statistics
        json << "  \"statistics\": {\n";
        if (!phases_.empty()) {
            // Find longest and shortest phases
            size_t longest_idx = 0, shortest_idx = 0;
            Duration longest_dur(0), shortest_dur(Duration::max());
            
            for (size_t i = 0; i < phases_.size(); ++i) {
                if (phases_[i].duration > longest_dur) {
                    longest_dur = phases_[i].duration;
                    longest_idx = i;
                }
                if (phases_[i].duration.count() > 0 && phases_[i].duration < shortest_dur) {
                    shortest_dur = phases_[i].duration;
                    shortest_idx = i;
                }
            }
            
            json << "    \"longest_phase\": \"" << phases_[longest_idx].name << "\",\n";
            json << "    \"longest_duration_ms\": " << std::fixed << std::setprecision(3)
                 << phases_[longest_idx].duration_ms() << ",\n";
            json << "    \"shortest_phase\": \"" << (shortest_dur.count() > 0 ? phases_[shortest_idx].name : "N/A") << "\",\n";
            json << "    \"shortest_duration_ms\": " << std::fixed << std::setprecision(3)
                 << (shortest_dur.count() > 0 ? phases_[shortest_idx].duration_ms() : 0.0) << ",\n";
            json << "    \"average_phase_duration_ms\": " << std::fixed << std::setprecision(3)
                 << (phases_.size() > 0 
                     ? std::chrono::duration<double, std::milli>(total_boot_duration_).count() / phases_.size() 
                     : 0.0) << "\n";
        } else {
            json << "    \"longest_phase\": null,\n";
            json << "    \"longest_duration_ms\": 0,\n";
            json << "    \"shortest_phase\": null,\n";
            json << "    \"shortest_duration_ms\": 0,\n";
            json << "    \"average_phase_duration_ms\": 0\n";
        }
        json << "  },\n";
        
        // Stall detection
        json << "  \"stall_status\": " << detect_stall_internal().to_json() << "\n";
        
        json << "}\n";
        
        return json.str();
    }
    
    /**
     * @brief Emit an info-level diagnostic event
     */
    void emit_info(const std::string& message) {
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "info";
        event.severity = Severity::INFO;
        event.message = message;
        emit_event(event);
    }
    
    /**
     * @ Emit a warning diagnostic event
     */
    void emit_warning(const std::string& message) {
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "warning";
        event.severity = Severity::WARNING;
        event.message = message;
        emit_event(event);
    }
    
    //=========================================================================
    // Member Variables
    //=========================================================================
    
    std::vector<BootPhase> phases_;           ///< Complete boot phase history
    Timestamp boot_start_time_;              ///< When boot started
    BootState current_boot_state_;           ///< Current boot state
    double stall_timeout_ms_;                ///< Threshold for stall detection (ms)
    Duration total_boot_duration_;           ///< Cumulative boot time
    size_t max_events_{10000};               ///< Configurable event limit
    mutable std::vector<StallDetectionResult> stall_history_; ///< Historical stall detections
};

} // namespace diagnostics
} // namespace prosper
