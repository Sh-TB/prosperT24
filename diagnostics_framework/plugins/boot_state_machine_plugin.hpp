#pragma once
/**
 * @file boot_state_machine_plugin.hpp
 * @brief Boot State Machine Plugin - EXPLICIT STATE MACHINE for PS4 Boot Sequence
 * 
 * Phase 10 Tier 1 - CRITICAL PRIORITY (Wave 1 Upstream Submission Target)
 * 
 * ============================================================================
 * WHY THIS PLUGIN EXISTS:
 * ============================================================================
 * 
 * The #1 question in PS4 emulation debugging is: "Where exactly did boot stop?"
 * 
 * Traditional approaches use scattered log messages that are:
 * - Hard to correlate temporally
 * - Inconsistent between modules
 * - Missing when crashes occur mid-sequence
 * - Difficult to analyze programmatically
 * 
 * This plugin solves this by implementing a PROPER FINITE STATE MACHINE that:
 * 1. Enforces valid state transitions (catches logic errors)
 * 2. Timestamps every state entry/exit (precise timing)
 * 3. Tracks time spent in each state (performance profiling)
 * 4. Captures failure context (what happened, why, related events)
 * 5. Generates visual diagrams of actual boot path taken
 * 
 * ============================================================================
 * STATE MACHINE DESIGN:
 * ============================================================================
 * 
 * Normal Boot Path:
 *   POWER_ON → ELF_LOADED → PRX_LOADED → SEGMENTS_MAPPED → RELOCATIONS_APPLIED
 *   → IMPORTS_RESOLVED → RUNTIME_INITIALIZED → THREAD_STARTED → MAIN_ENTRY
 *   → FIRST_RENDER → BOOT_COMPLETE
 * 
 * Crash Handling:
 *   ANY_STATE → CRASHED (from any point)
 * 
 * Each transition is validated against allowed transitions. Invalid transitions
 * are rejected and logged as errors. This catches bugs where code attempts to
 * skip states or transition backwards unexpectedly.
 * 
 * ============================================================================
 * USAGE EXAMPLE:
 * ============================================================================
 * 
 *   auto& bsm = get_boot_state_machine();
 *   bsm.initialize();
 *   
 *   // During boot sequence
 *   if (!bsm.request_state_transition(BootState::ELF_LOADED)) {
 *       LOG_ERROR("Invalid state transition to ELF_LOADED");
 *       return false;
 *   }
 *   
 *   // After crash or for analysis
 *   auto report = bsm.get_report();
 *   std::cout << bsm.analyze_failure() << std::endl;
 *   std::cout << bsm.generate_state_diagram() << std::endl;
 * 
 * @author Prosper Diagnostics Team
 * @version 1.0.0
 * @priority CRITICAL (Tier 1)
 */

#include "../core/diagnostic_interface.hpp"
#include "../core/event_bus.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <set>
#include <map>
#include <array>

namespace prosper {
namespace diagnostics {

//=============================================================================
// State Machine Configuration Constants
//=============================================================================

namespace boot_state_config {
    constexpr size_t MAX_TRANSITION_HISTORY = 256;      // Max transitions to store
    constexpr size_t MAX_RELATED_EVENTS_PER_TRANSITION = 50;  // Events to capture on failure
    constexpr const char* DEFAULT_OUTPUT_DIR = "./diagnostics/boot_state";
    constexpr const char* REPORT_FILENAME = "boot_state_report.json";
}

//=============================================================================
// State Entry/Exit Tracking
//=============================================================================

/**
 * @struct StateTimestamps
 * @brief Records timing information for a single state's lifecycle
 * 
 * Every time we enter and exit a state, we record precise timestamps.
 * This allows us to answer questions like:
 * - "How long did we spend in RELOCATIONS_APPLIED?"
 * - "When did we enter PRX_LOADED?"
 * - "Which state took the longest?"
 */
struct StateTimestamps {
    Timestamp entry_time;           // When we entered this state
    Timestamp exit_time;            // When we left this state (nullopt if current)
    Duration duration_in_state{0};  // Total time spent in this state
    
    bool has_entered{false};
    bool has_exited{false};
    
    void record_entry() {
        entry_time = now();
        has_entered = true;
        has_exited = false;
    }
    
    void record_exit() {
        if (has_entered && !has_exited) {
            exit_time = now();
            has_exited = true;
            auto diff = std::chrono::duration_cast<Duration>(exit_time - entry_time);
            duration_in_state = diff;
        }
    }
    
    double duration_ms() const {
        return static_cast<double>(duration_in_state.count()) / 1000.0;
    }
};

//=============================================================================
// State Transition Record
//=============================================================================

/**
 * @struct StateTransition
 * @brief Complete record of a single state transition
 * 
 * This is the core data structure for understanding WHAT happened during boot.
 * Each transition captures:
 * - Source and destination states
 * - When it happened
 * - How long we spent in the source state
 * - Whether it succeeded
 * - Why it failed (if applicable)
 * - Related diagnostic events for context
 */
struct StateTransition {
    BootState from_state;
    BootState to_state;
    Timestamp timestamp;
    Duration duration_in_source_state{0};
    bool success{true};
    std::string failure_reason;
    std::vector<DiagnosticEvent> related_events;
    
    /// Convert to JSON for serialization
    std::string to_json() const {
        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"from_state\": " << static_cast<int>(from_state) << ",\n";
        ss << "  \"to_state\": " << static_cast<int>(to_state) << ",\n";
        ss << "  \"timestamp_ms\": " << timestamp_to_ms(timestamp) << ",\n";
        ss << "  \"duration_in_source_us\": " << duration_in_source_state.count() << ",\n";
        ss << "  \"success\": " << (success ? "true" : "false") << ",\n";
        ss << "  \"failure_reason\": \"" << escape_json(failure_reason) << "\",\n";
        ss << "  \"related_event_count\": " << related_events.size() << "\n";
        ss << "}";
        return ss.str();
    }
    
private:
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
// State Machine Report
//=============================================================================

/**
 * @struct StateMachineReport
 * @brief Complete report of the entire boot sequence
 * 
 * This is the primary output structure used for:
 * - Post-mortem analysis after crashes
 * - Performance profiling of boot sequence
 * - Generating visual diagrams
 * - Exporting to external tools
 */
struct StateMachineReport {
    BootState current_state{BootState::POWER_ON};
    BootState final_state{BootState::UNKNOWN};
    std::vector<StateTransition> transition_history;
    BootState crash_state{BootState::UNKNOWN};  // If crashed, which state
    std::string failure_analysis;               // Human-readable analysis
    Timestamp boot_start_time;
    Timestamp boot_end_time_or_crash;
    Duration total_boot_duration{0};
    
    /// Serialize complete report to JSON
    std::string to_json() const {
        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"report_type\": \"boot_state_machine\",\n";
        ss << "  \"current_state\": " << static_cast<int>(current_state) << ",\n";
        ss << "  \"final_state\": " << static_cast<int>(final_state) << ",\n";
        ss << "  \"crash_state\": " << static_cast<int>(crash_state) << ",\n";
        ss << "  \"boot_start_ms\": " << timestamp_to_ms(boot_start_time) << ",\n";
        ss << "  \"boot_end_ms\": " << timestamp_to_ms(boot_end_time_or_crash) << ",\n";
        ss << "  \"total_duration_us\": " << total_boot_duration.count() << ",\n";
        ss << "  \"transition_count\": " << transition_history.size() << ",\n";
        ss << "  \"transitions\": [\n";
        
        for (size_t i = 0; i < transition_history.size(); ++i) {
            ss << "    " << transition_history[i].to_json();
            if (i < transition_history.size() - 1) ss << ",";
            ss << "\n";
        }
        
        ss << "  ],\n";
        ss << "  \"failure_analysis\": \"" << escape_json(failure_analysis) << "\"\n";
        ss << "}\n";
        return ss.str();
    }
    
private:
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
// Boot State Name Helper
//=============================================================================

/**
 * @brief Convert BootState enum to human-readable string
 */
inline const char* boot_state_name(BootState state) {
    switch (state) {
        case BootState::POWER_ON:            return "POWER_ON";
        case BootState::ELF_LOADED:          return "ELF_LOADED";
        case BootState::PRX_LOADED:          return "PRX_LOADED";
        case BootState::SEGMENTS_MAPPED:     return "SEGMENTS_MAPPED";
        case BootState::RELOCATIONS_APPLIED: return "RELOCATIONS_APPLIED";
        case BootState::IMPORTS_RESOLVED:    return "IMPORTS_RESOLVED";
        case BootState::RUNTIME_INITIALIZED: return "RUNTIME_INITIALIZED";
        case BootState::THREAD_STARTED:      return "THREAD_STARTED";
        case BootState::MAIN_ENTRY:          return "MAIN_ENTRY";
        case BootState::FIRST_RENDER:        return "FIRST_RENDER";
        case BootState::BOOT_COMPLETE:       return "BOOT_COMPLETE";
        case BootState::CRASHED:             return "CRASHED";
        case BootState::UNKNOWN:             return "UNKNOWN";
        default:                             return "INVALID";
    }
}

//=============================================================================
// Valid Transitions Definition
//=============================================================================

/**
 * @class ValidTransitions
 * @brief Defines the complete set of valid state transitions
 * 
 * This is the authoritative source for what transitions are allowed.
 * The state machine will reject any transition not in this set.
 * 
 * Design decisions:
 * - CRASHED can be reached from ANY state (emergency handling)
 * - No backward transitions in normal flow (prevents confusion)
 * - Sequential only with one exception: CRASHED from anywhere
 */
class ValidTransitions {
public:
    /// Check if a transition is valid
    static bool is_valid(BootState from, BootState to) {
        // CRASHED is always valid as a target (emergency)
        if (to == BootState::CRASHED) {
            return from != BootState::CRASHED && from != BootState::UNKNOWN;
        }
        
        // Look up in our valid transitions map
        auto it = valid_transitions_.find(from);
        if (it != valid_transitions_.end()) {
            return it->second.count(to) > 0;
        }
        return false;
    }
    
    /// Get all valid target states from a given state
    static std::vector<BootState> get_valid_targets(BootState from) {
        std::vector<BootState> targets;
        
        // CRASHED is always an option (except if already crashed/unknown)
        if (from != BootState::CRASHED && from != BootState::UNKNOWN) {
            targets.push_back(BootState::CRASHED);
        }
        
        auto it = valid_transitions_.find(from);
        if (it != valid_transitions_.end()) {
            for (BootState state : it->second) {
                targets.push_back(state);
            }
        }
        
        return targets;
    }
    
    /// Get total number of normal (non-crash) states
    static size_t normal_state_count() { return 11; }  // POWER_ON through BOOT_COMPLETE
    
private:
    // Define valid transitions: from -> set of valid 'to' states
    static const std::map<BootState, std::set<BootState>>& get_valid_transitions() {
        static const std::map<BootState, std::set<BootState>> transitions = {
            {BootState::POWER_ON, {BootState::ELF_LOADED}},
            {BootState::ELF_LOADED, {BootState::PRX_LOADED}},
            {BootState::PRX_LOADED, {BootState::SEGMENTS_MAPPED}},
            {BootState::SEGMENTS_MAPPED, {BootState::RELOCATIONS_APPLIED}},
            {BootState::RELOCATIONS_APPLIED, {BootState::IMPORTS_RESOLVED}},
            {BootState::IMPORTS_RESOLVED, {BootState::RUNTIME_INITIALIZED}},
            {BootState::RUNTIME_INITIALIZED, {BootState::THREAD_STARTED}},
            {BootState::THREAD_STARTED, {BootState::MAIN_ENTRY}},
            {BootState::MAIN_ENTRY, {BootState::FIRST_RENDER}},
            {BootState::FIRST_RENDER, {BootState::BOOT_COMPLETE}},
            // BOOT_COMPLETE is terminal (no outgoing transitions)
            // CRASHED is terminal (no outgoing transitions)
        };
        return transitions;
    }
    
    static const std::map<BootState, std::set<BootState>> valid_transitions_;
};

// Static member definition
const std::map<BootState, std::set<BootState>> ValidTransitions::valid_transitions_ = 
    ValidTransitions::get_valid_transitions();

//=============================================================================
// Boot State Machine Plugin - Main Implementation
//=============================================================================

/**
 * @class BootStateMachinePlugin
 * @brief Primary implementation of the boot sequence finite state machine
 * 
 * Thread Safety:
 * All public methods are thread-safe. Internal state is protected by mutex.
 * The state machine is designed to be called from multiple emulator threads
 * during the boot sequence.
 * 
 * Error Handling:
 * This plugin NEVER throws exceptions. All errors are handled internally
 * and reported via the event bus. Invalid transitions return false but
 * don't crash the system.
 * 
 * Performance:
 * State transitions are O(1) for validation and recording. Report generation
 * is O(n) where n is the number of transitions (typically < 20 for boot).
 */
class BootStateMachinePlugin : public DiagnosticPlugin {
public:
    //=========================================================================
    // Construction / Destruction
    //=========================================================================
    
    BootStateMachinePlugin()
        : current_state_(BootState::UNKNOWN)
        , initial_state_(BootState::POWER_ON)
        , crash_detected_(false)
        , transition_count_(0) {
        
        // Initialize state timestamps for all states
        for (int i = 0; i <= static_cast<int>(BootState::BOOT_COMPLETE); ++i) {
            state_timestamps_[static_cast<BootState>(i)] = StateTimestamps{};
        }
        state_timestamps_[BootState::CRASHED] = StateTimestamps{};
        state_timestamps_[BootState::UNKNOWN] = StateTimestamps{};
    }
    
    ~BootStateMachinePlugin() override = default;
    
    //=========================================================================
    // DiagnosticPlugin Interface Implementation
    //=========================================================================
    
    std::string name() const override {
        return "BootStateMachine";
    }
    
    std::string version() const override {
        return "1.0.0";
    }
    
    std::string description() const override {
        return "Explicit Finite State Machine for PS4 boot sequence tracking. "
               "Answers: Where exactly did boot stop?";
    }
    
    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            // Reset to initial state
            current_state_ = BootState::UNKNOWN;
            initial_state_ = BootState::POWER_ON;
            crash_detected_ = false;
            transition_count_ = 0;
            
            transition_history_.clear();
            report_ = StateMachineReport{};
            
            // Clear all state timestamps
            for (auto& pair : state_timestamps_) {
                pair.second = StateTimestamps{};
            }
            
            // Record boot start time
            report_.boot_start_time = now();
            
            active_ = true;
            
            // Emit initialization event
            emit_info_event("plugin_initialized", "Boot State Machine plugin initialized");
            
            return true;
        }
        catch (...) {
            active_ = false;
            return false;
        }
    }
    
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Finalize report
        report_.current_state = current_state_;
        report_.final_state = current_state_;
        report_.boot_end_time_or_crash = now();
        if (report_.boot_start_time.time_since_epoch().count() > 0) {
            report_.total_boot_duration = std::chrono::duration_cast<Duration>(
                report_.boot_end_time_or_crash - report_.boot_start_time);
        }
        
        active_ = false;
        
        emit_info_event("plugin_shutdown", "Boot State Machine plugin shut down");
    }
    
    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        current_state_ = BootState::UNKNOWN;
        initial_state_ = BootState::POWER_ON;
        crash_detected_ = false;
        transition_count_ = 0;
        
        transition_history_.clear();
        report_ = StateMachineReport{};
        
        for (auto& pair : state_timestamps_) {
            pair.second = StateTimestamps{};
        }
        
        report_.boot_start_time = now();
        event_count_ = 0;
    }
    
    void on_event(const DiagnosticEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Listen for crash events to auto-transition to CRASHED state
        if (event.event_type == "crash" || event.event_type == "fatal_error" ||
            event.event_type == "signal_received" || event.severity == Severity::CRITICAL) {
            
            if (current_state_ != BootState::CRASHED && 
                current_state_ != BootState::UNKNOWN &&
                current_state_ != BootState::BOOT_COMPLETE) {
                
                // Auto-transition to crashed state
                handle_crash(event);
            }
        }
        
        // Store recent events for correlation with transitions
        recent_events_.push_back(event);
        if (recent_events_.size() > boot_state_config::MAX_RELATED_EVENTS_PER_TRANSITION * 10) {
            recent_events_.erase(recent_events_.begin());
        }
        
        event_count_++;
    }
    
    std::string generate_report() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return report_.to_json();
    }
    
    void export_json(const std::string& path) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            std::ofstream file(path);
            if (file.is_open()) {
                file << report_.to_json();
                file.close();
            }
        }
        catch (...) {
            // Silently handle export failures - never throw from diagnostics
        }
    }
    
    //=========================================================================
    // Core State Machine Operations
    //=========================================================================
    
    /**
     * @brief Request a state transition
     * 
     * This is the PRIMARY method for advancing the boot state machine.
     * It validates the transition, records timing, and updates all internal state.
     * 
     * @param target The desired next state
     * @return true if transition was successful, false if invalid or failed
     * 
     * Usage:
     *   if (!bsm.request_state_transition(BootState::ELF_LOADED)) {
     *       // Handle error - transition was invalid
     *   }
     */
    bool request_state_transition(BootState target) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Handle special case: first transition from UNKNOWN to POWER_ON
        if (current_state_ == BootState::UNKNOWN && target == initial_state_) {
            return execute_transition(target, true, "");
        }
        
        // Validate transition
        if (!ValidTransitions::is_valid(current_state_, target)) {
            // Log the invalid transition attempt
            std::string reason = "Invalid transition from " + 
                                std::string(boot_state_name(current_state_)) + 
                                " to " + 
                                std::string(boot_state_name(target));
            
            emit_error_event("invalid_transition", reason);
            
            // Still record the failed attempt for debugging
            record_failed_transition(target, reason);
            
            return false;
        }
        
        return execute_transition(target, true, "");
    }
    
    /**
     * @brief Force a state transition (bypasses validation - USE WITH CAUTION)
     * 
     * This should only be used for:
     * - Recovery scenarios
     * - Testing/debugging
     * - Emergency crash handling
     * 
     * @param target The forced target state
     * @param reason Why the force was necessary
     * @return true always (force always succeeds)
     */
    bool force_state_transition(BootState target, const std::string& reason = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        return execute_transition(target, true, reason);
    }
    
    /**
     * @brief Get the current boot state
     * @return Current state in the boot sequence
     */
    BootState current_state() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_state_;
    }
    
    /**
     * @brief Check if boot has completed successfully
     */
    bool is_boot_complete() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_state_ == BootState::BOOT_COMPLETE;
    }
    
    /**
     * @brief Check if a crash occurred during boot
     */
    bool has_crashed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return crash_detected_;
    }
    
    /**
     * @brief Mark the system as crashed (call from signal handler or crash handler)
     * 
     * This will transition to CRASHED state regardless of current state.
     * Additional context about the crash can be provided.
     */
    void mark_crashed(const std::string& crash_details = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (current_state_ != BootState::CRASHED) {
            crash_details_ = crash_details;
            handle_crash_from_mark(crash_details);
        }
    }
    
    //=========================================================================
    // Reporting & Analysis Methods
    //=========================================================================
    
    /**
     * @brief Get the complete state machine report
     * @return Const reference to the full report
     */
    const StateMachineReport& get_report() const {
        std::lock_guard<std::mutex> lock(mutex_);
        update_report_locked();
        return report_;
    }
    
    /**
     * @brief Analyze why boot failed or stalled
     * 
     * Generates human-readable analysis of:
     * - Where boot stopped
     * - What likely caused the failure
     * - What was happening at the time of failure
     * - Suggestions for further investigation
     * 
     * @return Multi-line analysis string
     */
    std::string analyze_failure() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return generate_failure_analysis_locked();
    }
    
    /**
     * @brief Generate ASCII art state diagram showing actual path taken
     * 
     * Creates a visual representation like:
     * 
     *   [POWER_ON] --> [ELF_LOADED] --> [PRX_LOADED] ==>X [SEGMENTS_MAPPED]
     *       ✓              ✓               ✗
     *     0.1ms          5.2ms          12.8ms
     * 
     * States with checkmarks succeeded, X marks show failures,
     * arrows show actual path taken.
     * 
     * @return String containing ASCII art diagram
     */
    std::string generate_state_diagram() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return generate_ascii_diagram_locked();
    }
    
    /**
     * @brief Get list of valid transitions from a given state
     * @param from Starting state
     * @return Vector of valid target states
     */
    std::vector<BootState> get_valid_transitions(BootState from) const {
        return ValidTransitions::get_valid_targets(from);
    }
    
    /**
     * @brief Get timing information for a specific state
     * @param state The state to query
     * @return Timing info, or default if state never entered
     */
    StateTimestamps get_state_timing(BootState state) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = state_timestamps_.find(state);
        if (it != state_timestamps_.end()) {
            return it->second;
        }
        return StateTimestamps{};
    }
    
    /**
     * @brief Get total time spent in boot sequence so far
     * @return Duration in microseconds
     */
    Duration get_total_elapsed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (report_.boot_start_time.time_since_epoch().count() > 0) {
            return std::chrono::duration_cast<Duration>(now() - report_.boot_start_time);
        }
        return Duration{0};
    }
    
    /**
     * @brief Get the transition history
     * @return Copy of all recorded transitions
     */
    std::vector<StateTransition> get_transition_history() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return transition_history_;
    }
    
    /**
     * @brief Get count of completed transitions
     */
    size_t get_transition_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return transition_count_;
    }

private:
    //=========================================================================
    // Internal State
    //=========================================================================
    
    BootState current_state_;
    BootState initial_state_;
    bool crash_detected_;
    std::string crash_details_;
    size_t transition_count_;
    
    // History of all transitions
    std::vector<StateTransition> transition_history_;
    
    // Per-state timing information
    std::map<BootState, StateTimestamps> state_timestamps_;
    
    // Complete report (updated on demand)
    mutable StateMachineReport report_;
    
    // Recent events for correlation
    std::vector<DiagnosticEvent> recent_events_;
    
    //=========================================================================
    // Internal Methods
    //=========================================================================
    
    /**
     * @brief Execute a validated state transition
     */
    bool execute_transition(BootState target, bool success, const std::string& force_reason) {
        BootState from = current_state_;
        Timestamp now_ts = now();
        
        // Calculate duration in source state
        Duration duration_in_source{0};
        if (from != BootState::UNKNOWN) {
            auto it = state_timestamps_.find(from);
            if (it != state_timestamps_.end() && it->second.has_entered) {
                it->second.record_exit();
                duration_in_source = it->second.duration_in_state;
            }
        }
        
        // Create transition record
        StateTransition transition;
        transition.from_state = from;
        transition.to_state = target;
        transition.timestamp = now_ts;
        transition.duration_in_source_state = duration_in_source;
        transition.success = success;
        
        if (!success || !force_reason.empty()) {
            transition.failure_reason = force_reason.empty() ? "Unknown failure" : force_reason;
            // Capture recent events for context
            capture_related_events(transition);
        }
        
        // Handle crash state specially
        if (target == BootState::CRASHED) {
            crash_detected_ = true;
            report_.crash_state = from;
            transition.failure_reason = crash_details_.empty() ? 
                "Crash detected" : crash_details_;
        }
        
        // Update current state
        current_state_ = target;
        
        // Record entry into new state
        auto target_it = state_timestamps_.find(target);
        if (target_it != state_timestamps_.end()) {
            target_it->second.record_entry();
        }
        
        // Store transition in history
        transition_history_.push_back(transition);
        transition_count_++;
        
        // Enforce max history size
        if (transition_history_.size() > boot_state_config::MAX_TRANSITION_HISTORY) {
            transition_history_.erase(transition_history_.begin());
        }
        
        // Emit event for this transition
        emit_transition_event(transition);
        
        return true;
    }
    
    /**
     * @brief Record a failed (invalid) transition attempt
     */
    void record_failed_transition(BootState target, const std::string& reason) {
        StateTransition transition;
        transition.from_state = current_state_;
        transition.to_state = target;
        transition.timestamp = now();
        transition.success = false;
        transition.failure_reason = reason;
        
        capture_related_events(transition);
        transition_history_.push_back(transition);
    }
    
    /**
     * @brief Capture events related to a transition for context
     */
    void capture_related_events(StateTransition& transition) {
        // Get events from shortly before this transition
        size_t count = std::min(recent_events_.size(), 
                               boot_state_config::MAX_RELATED_EVENTS_PER_TRANSITION);
        
        if (count > 0) {
            auto start = recent_events_.end() - count;
            transition.related_events.assign(start, recent_events_.end());
        }
    }
    
    /**
     * @brief Handle crash from incoming event
     */
    void handle_crash(const DiagnosticEvent& event) {
        crash_detected_ = true;
        crash_details_ = event.message;
        
        // Execute transition to CRASHED
        BootState previous = current_state_;
        execute_transition(BootState::CRASHED, false, 
                          "Crash event received: " + event.message);
        
        emit_critical_event("boot_crash", 
            "Boot sequence crashed at state: " + std::string(boot_state_name(previous)) +
            ". Reason: " + event.message);
    }
    
    /**
     * @brief Handle explicit crash marking
     */
    void handle_crash_from_mark(const std::string& details) {
        crash_detected_ = true;
        
        BootState previous = current_state_;
        execute_transition(BootState::CRASHED, false,
                          details.empty() ? "Crash explicitly marked" : details);
        
        emit_critical_event("boot_crash",
            "Boot sequence crashed at state: " + std::string(boot_state_name(previous)));
    }
    
    /**
     * @brief Update the report structure from current state
     */
    void update_report_locked() const {
        report_.current_state = current_state_;
        report_.final_state = current_state_;  // Final = current until boot ends
        report_.transition_history = transition_history_;
        report_.boot_end_time_or_crash = now();
        
        if (report_.boot_start_time.time_since_epoch().count() > 0) {
            report_.total_boot_duration = std::chrono::duration_cast<Duration>(
                report_.boot_end_time_or_crash - report_.boot_start_time);
        }
        
        // Generate failure analysis
        report_.failure_analysis = generate_failure_analysis_locked();
    }
    
    /**
     * @brief Generate human-readable failure analysis
     */
    std::string generate_failure_analysis_locked() const {
        std::ostringstream analysis;
        
        analysis << "=== BOOT STATE MACHINE ANALYSIS ===\n\n";
        
        // Current status
        analysis << "Current State: " << boot_state_name(current_state_) << "\n";
        analysis << "Total Transitions: " << transition_count_ << "\n";
        
        if (crash_detected_) {
            analysis << "STATUS: CRASHED\n";
            analysis << "Crash State: " << boot_state_name(report_.crash_state) << "\n";
            if (!crash_details_.empty()) {
                analysis << "Crash Details: " << crash_details_ << "\n";
            }
        } else if (current_state_ == BootState::BOOT_COMPLETE) {
            analysis << "STATUS: BOOT COMPLETE (Success)\n";
        } else if (transition_count_ == 0) {
            analysis << "STATUS: NOT STARTED\n";
            analysis << "No transitions have occurred yet.\n";
        } else {
            analysis << "STATUS: IN PROGRESS / STALLED\n";
            analysis << "Boot appears to have stalled at: " 
                    << boot_state_name(current_state_) << "\n";
        }
        
        // Show transition path
        analysis << "\n--- Transition Path ---\n";
        for (size_t i = 0; i < transition_history_.size(); ++i) {
            const auto& t = transition_history_[i];
            analysis << "[" << i << "] "
                    << boot_state_name(t.from_state) << " -> "
                    << boot_state_name(t.to_state);
            
            if (t.success) {
                analysis << " OK";
            } else {
                analysis << " FAILED: " << t.failure_reason;
            }
            
            analysis << " (+" << (t.duration_in_source_state.count() / 1000.0) << "ms)\n";
        }
        
        // Time analysis per state
        analysis << "\n--- Time Per State ---\n";
        for (const auto& pair : state_timestamps_) {
            if (pair.second.has_entered) {
                analysis << boot_state_name(pair.first) << ": ";
                if (pair.second.has_exited) {
                    analysis << (pair.second.duration_ms()) << "ms";
                } else {
                    analysis << "still in state...";
                }
                analysis << "\n";
            }
        }
        
        // Suggest next steps based on current state
        analysis << "\n--- Suggestions ---\n";
        auto valid_next = ValidTransitions::get_valid_targets(current_state_);
        if (!valid_next.empty() && current_state_ != BootState::BOOT_COMPLETE &&
            current_state_ != BootState::CRASHED) {
            analysis << "Expected next state(s): ";
            for (size_t i = 0; i < valid_next.size(); ++i) {
                if (valid_next[i] == BootState::CRASHED) continue;
                analysis << boot_state_name(valid_next[i]);
                if (i < valid_next.size() - 1) analysis << ", ";
            }
            analysis << "\n";
        }
        
        if (crash_detected_) {
            analysis << "\nACTION REQUIRED: Analyze crash snapshot and stack trace.\n";
            analysis << "The crash occurred during: " << boot_state_name(report_.crash_state) << "\n";
            analysis << "Check relocation logs if crash was during RELOCATIONS_APPLIED.\n";
            analysis << "Check import resolution logs if crash was during IMPORTS_RESOLVED.\n";
        }
        
        return analysis.str();
    }
    
    /**
     * @brief Generate ASCII art state diagram
     */
    std::string generate_ascii_diagram_locked() const {
        std::ostringstream diagram;
        
        diagram << "\n";
        diagram << "╔══════════════════════════════════════════════════════════╗\n";
        diagram << "║              BOOT STATE MACHINE DIAGRAM                 ║\n";
        diagram << "╚══════════════════════════════════════════════════════════╝\n\n";
        
        // Define the canonical order of states
        std::array<BootState, 11> canonical_states = {
            BootState::POWER_ON,
            BootState::ELF_LOADED,
            BootState::PRX_LOADED,
            BootState::SEGMENTS_MAPPED,
            BootState::RELOCATIONS_APPLIED,
            BootState::IMPORTS_RESOLVED,
            BootState::RUNTIME_INITIALIZED,
            BootState::THREAD_STARTED,
            BootState::MAIN_ENTRY,
            BootState::FIRST_RENDER,
            BootState::BOOT_COMPLETE
        };
        
        // Build the diagram line by line
        // Line 1: State boxes
        diagram << "  ";
        for (size_t i = 0; i < canonical_states.size(); ++i) {
            BootState state = canonical_states[i];
            const char* name = boot_state_name(state);
            
            // Determine status character
            char status = ' ';
            if (state == current_state_) {
                status = crash_detected_ ? '✗' : '◄';
            } else if (was_state_visited(state)) {
                status = '✓';
            }
            
            // Shortened names for display
            const char* short_name = get_short_state_name(state);
            
            diagram << "[" << short_name << "]" << status;
            
            if (i < canonical_states.size() - 1) {
                // Check if this transition was taken
                bool transition_taken = was_transition_taken(state, canonical_states[i + 1]);
                diagram << (transition_taken ? "──►" : "───");
            }
        }
        diagram << "\n";
        
        // Line 2: Timing information
        diagram << "  ";
        for (size_t i = 0; i < canonical_states.size(); ++i) {
            BootState state = canonical_states[i];
            auto it = state_timestamps_.find(state);
            
            if (it != state_timestamps_.end() && it->second.has_entered) {
                if (it->second.has_exited) {
                    diagram << std::fixed << std::setprecision(1) 
                            << std::setw(6) << it->second.duration_ms() << "ms";
                } else {
                    diagram << "  ... ";
                }
            } else {
                diagram << "      ";
            }
            
            if (i < canonical_states.size() - 1) {
                diagram << "   ";
            }
        }
        diagram << "\n";
        
        // Legend
        diagram << "\n  Legend:\n";
        diagram << "    ✓ = State completed successfully\n";
        diagram << "    ◄ = Current state (in progress)\n";
        diagram << "    ✗ = Crash location\n";
        diagram << "    ──► = Transition taken\n";
        diagram << "    ─── = Transition not yet taken\n\n";
        
        // Summary statistics
        diagram << "  Summary:\n";
        diagram << "    Total elapsed: " << (get_total_elapsed().count() / 1000.0) << "ms\n";
        diagram << "    States visited: " << count_visited_states() << "/11\n";
        diagram << "    Transitions: " << transition_count_ << "\n";
        
        if (crash_detected_) {
            diagram << "\n  ⚠ CRASH DETECTED at: " << boot_state_name(report_.crash_state) << "\n";
        }
        
        return diagram.str();
    }
    
    /**
     * @brief Get shortened state name for diagram display
     */
    static const char* get_short_state_name(BootState state) {
        switch (state) {
            case BootState::POWER_ON:            return "PWR";
            case BootState::ELF_LOADED:          return "ELF";
            case BootState::PRX_LOADED:          return "PRX";
            case BootState::SEGMENTS_MAPPED:     return "MAP";
            case BootState::RELOCATIONS_APPLIED: return "REL";
            case BootState::IMPORTS_RESOLVED:    return "IMP";
            case BootState::RUNTIME_INITIALIZED: return "RTM";
            case BootState::THREAD_STARTED:      return "THR";
            case BootState::MAIN_ENTRY:          return "MAIN";
            case BootState::FIRST_RENDER:        return "RND";
            case BootState::BOOT_COMPLETE:       return "DONE";
            default:                             return "???";
        }
    }
    
    /**
     * @brief Check if a state was ever visited
     */
    bool was_state_visited(BootState state) const {
        auto it = state_timestamps_.find(state);
        return it != state_timestamps_.end() && it->second.has_entered;
    }
    
    /**
     * @brief Check if a specific transition was taken
     */
    bool was_transition_taken(BootState from, BootState to) const {
        for (const auto& t : transition_history_) {
            if (t.from_state == from && t.to_state == to && t.success) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief Count how many states were visited
     */
    size_t count_visited_states() const {
        size_t count = 0;
        for (int i = 0; i <= static_cast<int>(BootState::BOOT_COMPLETE); ++i) {
            if (was_state_visited(static_cast<BootState>(i))) {
                count++;
            }
        }
        return count;
    }
    
    //=========================================================================
    // Event Emission Helpers
    //=========================================================================
    
    void emit_info_event(const std::string& type, const std::string& message) {
        try {
            DiagnosticEvent event;
            event.source_plugin = name();
            event.event_type = type;
            event.severity = Severity::INFO;
            event.message = message;
            event.category = "boot_state_machine";
            emit_event(event);
        } catch (...) {}
    }
    
    void emit_warning_event(const std::string& type, const std::string& message) {
        try {
            DiagnosticEvent event;
            event.source_plugin = name();
            event.event_type = type;
            event.severity = Severity::WARNING;
            event.message = message;
            event.category = "boot_state_machine";
            emit_event(event);
        } catch (...) {}
    }
    
    void emit_error_event(const std::string& type, const std::string& message) {
        try {
            DiagnosticEvent event;
            event.source_plugin = name();
            event.event_type = type;
            event.severity = Severity::ERROR;
            event.message = message;
            event.category = "boot_state_machine";
            emit_event(event);
        } catch (...) {}
    }
    
    void emit_critical_event(const std::string& type, const std::string& message) {
        try {
            DiagnosticEvent event;
            event.source_plugin = name();
            event.event_type = type;
            event.severity = Severity::CRITICAL;
            event.message = message;
            event.category = "boot_state_machine";
            emit_event(event);
        } catch (...) {}
    }
    
    void emit_transition_event(const StateTransition& transition) {
        try {
            DiagnosticEvent event;
            event.source_plugin = name();
            event.event_type = "state_transition";
            event.severity = transition.success ? Severity::INFO : Severity::ERROR;
            event.message = std::string("State: ") + boot_state_name(transition.from_state) +
                           " -> " + boot_state_name(transition.to_state);
            event.category = "boot_state_machine";
            event.numeric_data["from_state"] = static_cast<int64_t>(transition.from_state);
            event.numeric_data["to_state"] = static_cast<int64_t>(transition.to_state);
            event.numeric_data["success"] = transition.success ? 1 : 0;
            event.float_data["duration_us"] = static_cast<double>(transition.duration_in_source_state.count());
            if (!transition.failure_reason.empty()) {
                event.metadata["failure_reason"] = transition.failure_reason;
            }
            emit_event(event);
        } catch (...) {}
    }
};

//=============================================================================
// Factory Registration
//=============================================================================

inline std::unique_ptr<DiagnosticPlugin> create_boot_state_machine_plugin() {
    return std::make_unique<BootStateMachinePlugin>();
}

} // namespace diagnostics
} // namespace prosper
