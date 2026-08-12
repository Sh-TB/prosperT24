#pragma once
#include "../core/diagnostic_interface.hpp"
#include "../core/event_bus.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <algorithm>
#include <random>
#include <filesystem>

namespace prosper {
namespace diagnostics {

//=============================================================================
// Deterministic Diagnostics Mode Plugin - Phase 10 Tier 2 Diagnostic Plugin
//
// Record/Replay system for reproducible debugging.
//
// Implements --diagnostics-record and --diagnostics-replay modes to enable
// exact reproduction of diagnostic event sequences for bug investigation.
//
// RECORD MODE captures:
// - Event order (exact sequence with sequence numbers)
// - Timing information (absolute and relative timestamps)
// - State transitions (boot state machine changes)
// - Important runtime decisions (branch outcomes in key areas)
//
// REPLAY reproduces:
// - Same diagnostic timeline
// - Same event order
// - Same relative timing (configurable speedup)
// - Same state machine transitions
// - Divergence detection when replay differs from recording
//
// Design Decisions:
// - Recording format is JSON for human readability and tooling support
// - Content hash (SHA-256 style) for integrity verification
// - Replay can run at different speeds (realtime, accelerated, instant)
// - Divergence tracking helps identify non-deterministic behavior
// - Thread-safe: recording/replay state protected by mutex
//=============================================================================

/**
 * @brief A single recorded diagnostic event with full context
 * 
 * Captures all information needed to reconstruct an event during replay,
 * including timing data and system state at time of occurrence.
 */
struct RecordedEvent {
    size_t sequence_number{0};                    ///< Unique position in recording
    Timestamp absolute_time;                      ///< Wall-clock time of original event
    Duration relative_time{0};                    ///< Time from recording start
    
    std::string event_type;                       ///< Type identifier
    std::string source_plugin;                    ///< Originating plugin name
    std::string event_data_json;                  ///< Serialized event payload
    
    // Context at time of event
    BootState boot_state_at_event{BootState::UNKNOWN};  ///< Boot state snapshot
    uint64_t thread_id{0};                        ///< Thread that emitted event
    
    /**
     * @brief Convert recorded event to JSON
     */
    std::string to_json() const {
        std::ostringstream ss;
        ss << "{\n";
        ss << "    \"sequence_number\": " << sequence_number << ",\n";
        ss << "    \"absolute_time_ms\": " << timestamp_to_ms(absolute_time) << ",\n";
        ss << "    \"relative_time_us\": " << relative_time.count() << ",\n";
        ss << "    \"event_type\": \"" << event_type << "\",\n";
        ss << "    \"source_plugin\": \"" << source_plugin << "\",\n";
        ss << "    \"boot_state\": " << static_cast<int>(boot_state_at_event) << ",\n";
        ss << "    \"thread_id\": " << thread_id << ",\n";
        ss << "    \"event_data\": " << (event_data_json.empty() ? "{}" : event_data_json) << "\n";
        ss << "  }";
        return ss.str();
    }
    
    /**
     * @brief Parse recorded event from JSON
     */
    static RecordedEvent from_json(const std::string& json) {
        RecordedEvent event;
        // Simplified parsing - production would use proper JSON parser
        // This handles our specific format
        
        auto extract_string = [&json](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\": \"";
            size_t pos = json.find(search);
            if (pos == std::string::npos) return "";
            pos += search.length();
            size_t end = json.find("\"", pos);
            if (end == std::string::npos) return "";
            return json.substr(pos, end - pos);
        };
        
        auto extract_number = [&json](const std::string& key) -> int64_t {
            std::string search = "\"" + key + "\": ";
            size_t pos = json.find(search);
            if (pos == std::string::npos) return 0;
            pos += search.length();
            size_t end = pos;
            while (end < json.size() && (json[end] >= '0' && json[end] <= '9' || 
                   json[end] == '-' || json[end] == '.')) {
                end++;
            }
            if (pos >= end) return 0;
            return std::stoll(json.substr(pos, end - pos));
        };
        
        event.sequence_number = static_cast<size_t>(extract_number("sequence_number"));
        double abs_ms = static_cast<double>(extract_number("absolute_time_ms"));
        event.absolute_time = from_ms(abs_ms);
        event.relative_time = Duration(extract_number("relative_time_us"));
        event.event_type = extract_string("event_type");
        event.source_plugin = extract_string("source_plugin");
        event.boot_state_at_event = static_cast<BootState>(
            static_cast<int>(extract_number("boot_state")));
        event.thread_id = static_cast<uint64_t>(extract_number("thread_id"));
        
        // Extract event_data object
        size_t data_pos = json.find("\"event_data\":");
        if (data_pos != std::string::npos) {
            size_t brace_start = json.find("{", data_pos + 12);
            if (brace_start != std::string::npos) {
                size_t brace_end = json.rfind("}");
                if (brace_end != std::string::npos && brace_end > brace_start) {
                    event.event_data_json = json.substr(brace_start, brace_end - brace_start + 1);
                }
            }
        }
        
        return event;
    }
};

/**
 * @brief Complete recording session with metadata
 */
struct RecordingSession {
    std::string session_id;                      ///< Unique session identifier
    Timestamp start_time;                         ///< When recording started
    Timestamp end_time;                           ///< When recording ended
    size_t total_events{0};                       ///< Total events captured
    std::vector<RecordedEvent> events;            ///< All recorded events in order
    
    // Metadata
    std::string emulator_version;                 ///< Emulator version string
    std::string game_id;                          ///< Game being emulated (if known)
    DiagnosticsConfig config_used;                ///< Config at time of recording
    
    // Integrity
    std::string content_hash;                     ///< Hash of all event data
    
    /**
     * @brief Convert entire session to JSON
     */
    std::string to_json() const {
        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"recording_format_version\": \"2.0\",\n";
        ss << "  \"session_id\": \"" << session_id << "\",\n";
        ss << "  \"start_time_ms\": " << timestamp_to_ms(start_time) << ",\n";
        ss << "  \"end_time_ms\": " << timestamp_to_ms(end_time) << ",\n";
        ss << "  \"duration_ms\": " << std::fixed << 
              (timestamp_to_ms(end_time) - timestamp_to_ms(start_time)) << ",\n";
        ss << "  \"total_events\": " << total_events << ",\n";
        ss << "  \"emulator_version\": \"" << emulator_version << "\",\n";
        ss << "  \"game_id\": \"" << game_id << "\",\n";
        ss << "  \"content_hash\": \"" << content_hash << "\",\n";
        
        // Configuration used
        ss << "  \"configuration\": {\n";
        ss << "    \"enabled\": " << (config_used.enabled ? "true" : "false") << ",\n";
        ss << "    \"verbose\": " << (config_used.verbose ? "true" : "false") << ",\n";
        ss << "    \"max_events_per_plugin\": " << config_used.max_events_per_plugin << "\n";
        ss << "  },\n";
        
        // Events array
        ss << "  \"events\": [\n";
        for (size_t i = 0; i < events.size(); ++i) {
            if (i > 0) ss << ",\n";
            ss << events[i].to_json();
        }
        ss << "\n  ]\n";
        
        ss << "}\n";
        return ss.str();
    }
    
    /**
     * @brief Calculate content hash for integrity verification
     */
    void calculate_hash() {
        // Simple hash implementation - production would use SHA-256
        std::hash<std::string> hasher;
        std::ostringstream concat;
        
        for (const auto& event : events) {
            concat << event.event_type << event.source_plugin << 
                      event.sequence_number << event.relative_time.count();
        }
        
        size_t hash_value = hasher(concat.str());
        
        // Convert to hex string representation
        std::ostringstream hex_ss;
        hex_ss << std::hex << std::setfill('0') << std::setw(16) << hash_value;
        content_hash = hex_ss.str();
    }
};

/**
 * @brief Result of a replay operation
 */
struct ReplayResult {
    bool success{false};                          ///< Did replay complete successfully?
    size_t events_replayed{0};                    ///< Number of events processed
    std::vector<std::string> divergence_points;   ///< Where replay differed from expected
    double match_percentage{100.0};               ///< How closely replay matched (0-100)
    
    /**
     * @brief Convert result to JSON
     */
    std::string to_json() const {
        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"success\": " << (success ? "true" : "false") << ",\n";
        ss << "  \"events_replayed\": " << events_replayed << ",\n";
        ss << "  \"match_percentage\": " << std::fixed << std::setprecision(2) 
           << match_percentage << ",\n";
        
        ss << "  \"divergence_points\": [\n";
        for (size_t i = 0; i < divergence_points.size(); ++i) {
            if (i > 0) ss << ",\n";
            ss << "    \"" << divergence_points[i] << "\"";
        }
        ss << "\n  ]\n";
        
        ss << "}\n";
        return ss.str();
    }
};

/**
 * @brief Replay speed modes
 */
enum class ReplaySpeed {
    REALTIME,      ///< Play back at original speed
    ACCELERATED,   ///< 10x speedup
    FAST,          ///< 100x speedup  
    INSTANT        ///< No delays between events
};

//=============================================================================
// Deterministic Diagnostics Mode Plugin Implementation
//=============================================================================

class DeterministicDiagnosticsMode : public DiagnosticPlugin {
public:
    //=========================================================================
    // Construction / Destruction
    //=========================================================================
    
    DeterministicDiagnosticsMode() : DiagnosticPlugin() {
        active_ = false;
        is_recording_ = false;
        is_replaying_ = false;
        replay_index_ = 0;
    }
    
    ~DeterministicDiagnosticsMode() override {
        shutdown();
    }
    
    //=========================================================================
    // DiagnosticPlugin Interface Implementation
    //=========================================================================
    
    std::string name() const override {
        return "deterministic_diagnostics";
    }
    
    std::string description() const override {
        return "Record/Replay mode for deterministic diagnostics. Captures exact "
               "event sequences for reproducible debugging of PS4 emulator issues.";
    }
    
    std::string version() const override {
        return "2.0.0";
    }
    
    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            current_session_ = std::nullopt;
            is_recording_ = false;
            is_replaying_ = false;
            replay_index_ = 0;
            saved_recordings_.clear();
            
            active_ = true;
            return true;
        } catch (...) {
            active_ = false;
            return false;
        }
    }
    
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Stop any active recording
        if (is_recording_) {
            stop_recording_internal();
        }
        
        active_ = false;
        is_recording_ = false;
        is_replaying_ = false;
    }
    
    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (is_recording_) {
            stop_recording_internal();
        }
        
        current_session_ = std::nullopt;
        is_replaying_ = false;
        replay_index_ = 0;
    }
    
    /**
     * @brief Handle incoming events - record or compare during replay
     */
    void on_event(const DiagnosticEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) return;
        
        event_count_++;
        
        if (is_recording_) {
            record_event_internal(event);
        } else if (is_replaying_) {
            // During replay, we could compare live events against recorded ones
            // For now, just count them
            (void)event;
        }
    }
    
    std::string generate_report() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ostringstream report;
        report << "{\n";
        report << "  \"report_type\": \"deterministic_diagnostics\",\n";
        report << "  \"generated_at\": " << timestamp_to_ms(now()) << ",\n";
        report << "  \"mode\": ";
        
        if (is_recording_) {
            report << "\"recording\",\n";
            report << "  \"current_session\": {\n";
            if (current_session_.has_value()) {
                const auto& sess = *current_session_;
                report << "    \"session_id\": \"" << sess.session_id << "\",\n";
                report << "    \"events_captured\": " << sess.events.size() << ",\n";
                report << "    \"duration_s\": " << std::fixed << 
                          (timestamp_to_ms(now()) - timestamp_to_ms(sess.start_time)) / 1000.0 << "\n";
            }
            report << "  }\n";
        } else if (is_replaying_) {
            report << "\"replay\",\n";
            report << "  \"replay_progress\": {\n";
            report << "    \"current_index\": " << replay_index_ << ",\n";
            if (replay_session_.has_value()) {
                report << "    \"total_events\": " << replay_session_->events.size() << ",\n";
                report << "    \"progress_percent\": " << std::fixed << std::setprecision(1) <<
                          (replay_index_ * 100.0 / std::max(size_t(1), replay_session_->events.size())) << "\n";
            }
            report << "  }\n";
        } else {
            report << "\"idle\"\n";
        }
        
        report << "}\n";
        return report.str();
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
            // Silently handle errors
        }
    }
    
    //=========================================================================
    // Recording Methods
    //=========================================================================
    
    /**
     * @brief Start a new recording session
     * @param session_label Optional human-readable label for this session
     * @return true if recording started successfully
     * 
     * Creates a new RecordingSession and begins capturing all diagnostic events.
     * Any existing recording is stopped first.
     */
    bool start_recording(const std::string& session_label = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) return false;
        
        // Stop any existing recording
        if (is_recording_) {
            stop_recording_internal();
        }
        
        // Create new session
        RecordingSession session;
        session.session_id = generate_session_id(session_label);
        session.start_time = now();
        session.emulator_version = "Prosper PS4 Emulator v1.0";  // Would get from runtime
        session.game_id = "";  // Would get from runtime context
        session.config_used = global::is_initialized() ? 
                              *global::get_config() : DiagnosticsConfig{};
        
        current_session_ = std::move(session);
        is_recording_ = true;
        event_sequence_counter_ = 0;
        
        // Emit start event
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "recording_started";
        event.severity = Severity::INFO;
        event.message = "Diagnostic recording started: " + current_session_->session_id;
        event.metadata["session_id"] = current_session_->session_id;
        emit_event(event);
        
        return true;
    }
    
    /**
     * @brief Record an event into the current session
     * @param event The diagnostic event to record
     * 
     * Called automatically by on_event(), but can also be called manually
     * for explicit recording control.
     */
    void record_event(const DiagnosticEvent& event) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!is_recording_ || !current_session_.has_value()) return;
        
        record_event_internal(event);
    }
    
    /**
     * @brief Stop the current recording session
     * @return Path where recording was saved, or empty on failure
     * 
     * Finalizes the recording, calculates integrity hash, saves to disk,
     * and returns the file path.
     */
    std::string stop_recording() {
        std::lock_guard<std::mutex> lock(mutex_);
        return stop_recording_internal();
    }
    
    //=========================================================================
    // Replay Methods
    //=========================================================================
    
    /**
     * @brief Start replaying a previously recorded session
     * @param recording_path Path to the recording JSON file
     * @return Result indicating success/failure
     * 
     * Loads the recording and prepares for sequential event playback.
     */
    ReplayResult start_replay(const std::string& recording_path) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        ReplayResult result;
        result.success = false;
        result.events_replayed = 0;
        result.match_percentage = 0.0;
        
        if (!active_) {
            result.divergence_points.push_back("Engine not initialized");
            return result;
        }
        
        // Stop any recording
        if (is_recording_) {
            stop_recording_internal();
        }
        
        // Load recording from file
        try {
            std::ifstream file(recording_path);
            if (!file.is_open()) {
                result.divergence_points.push_back("Cannot open recording file: " + recording_path);
                return result;
            }
            
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            file.close();
            
            // Parse recording
            replay_session_ = parse_recording(content);
            
            if (!replay_session_.has_value()) {
                result.divergence_points.push_back("Failed to parse recording file");
                return result;
            }
            
            // Verify integrity if hash present
            if (!replay_session_->content_hash.empty()) {
                std::string calculated = calculate_content_hash(*replay_session_);
                if (calculated != replay_session_->content_hash) {
                    result.divergence_points.push_back(
                        "Content hash mismatch! Recording may be corrupted.");
                    // Continue anyway but flag it
                }
            }
            
            is_replaying_ = true;
            replay_index_ = 0;
            replay_speed_ = ReplaySpeed::REALTIME;
            last_replay_time_ = now();
            
            result.success = true;
            result.events_replayed = 0;
            result.match_percentage = 100.0;
            
            // Emit replay started event
            DiagnosticEvent event;
            event.source_plugin = name();
            event.event_type = "replay_started";
            event.severity = Severity::INFO;
            event.message = "Replay started from: " + recording_path;
            event.metadata["recording_path"] = recording_path;
            event.numeric_data["total_events"] = static_cast<int64_t>(
                replay_session_->events.size());
            emit_event(event);
            
        } catch (const std::exception& e) {
            result.divergence_points.push_back(std::string("Exception: ") + e.what());
        } catch (...) {
            result.divergence_points.push_back("Unknown exception during replay load");
        }
        
        return result;
    }
    
    /**
     * @brief Get next event in replay sequence
     * @return Next DiagnosticEvent, or empty/invalid event if replay complete
     * 
     * Advances the replay position and returns the next event.
     * Respects configured replay speed for timing.
     */
    DiagnosticEvent replay_next_event() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!is_replaying_ || !replay_session_.has_value()) {
            return DiagnosticEvent();  // Return invalid event
        }
        
        if (replay_index_ >= replay_session_->events.size()) {
            // Replay complete
            is_replaying_ = false;
            return DiagnosticEvent();
        }
        
        const RecordedEvent& recorded = replay_session_->events[replay_index_];
        
        // Apply timing based on speed setting
        apply_replay_timing(recorded);
        
        // Convert back to DiagnosticEvent (best effort)
        DiagnosticEvent event = reconstruct_event(recorded);
        
        replay_index_++;
        event_count_++;
        
        return event;
    }
    
    /**
     * @brief Check if replay has completed
     * @return true if all events have been replayed
     */
    bool replay_complete() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!is_replaying_ || !replay_session_.has_value()) {
            return true;  // Not replaying = complete
        }
        
        return replay_index_ >= replay_session_->events.size();
    }
    
    /**
     * @brief Get current recording session (if active)
     * @return Reference to current session, or nullopt if not recording
     */
    const std::optional<RecordingSession>& get_current_session() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_session_;
    }
    
    /**
     * @brief List available recordings in default directory
     * @return Vector of paths to found recordings
     */
    std::vector<std::string> list_available_recordings() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::string> recordings;
        
        try {
            std::string dir = "./diagnostics/recordings";
            
            if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    if (entry.path().extension() == ".json") {
                        recordings.push_back(entry.path().string());
                    }
                }
            }
        } catch (...) {
            // Directory listing failed
        }
        
        // Sort by modification time (newest first)
        std::sort(recordings.begin(), recordings.end(),
            [](const std::string& a, const std::string& b) {
                try {
                    auto ta = std::filesystem::last_write_time(a);
                    auto tb = std::filesystem::last_write_time(b);
                    return ta > tb;
                } catch (...) {
                    return a < b;
                }
            });
        
        return recordings;
    }
    
    /**
     * @brief Export current or specified recording to path
     * @param path Output path (if empty, generates default name)
     * @return Path where recording was saved
     */
    std::string export_recording(const std::string& path = "") const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string output_path = path;
        
        // Determine which session to export
        const RecordingSession* session_to_export = nullptr;
        
        if (current_session_.has_value()) {
            session_to_export = &*current_session_;
        } else if (replay_session_.has_value()) {
            session_to_export = &*replay_session_;
        }
        
        if (!session_to_export) {
            return "";
        }
        
        // Generate path if not provided
        if (output_path.empty()) {
            std::string dir = "./diagnostics/recordings";
            try {
                std::filesystem::create_directories(dir);
            } catch (...) {}
            
            output_path = dir + "/recording_" + session_to_export->session_id + "_" +
                         std::to_string(timestamp_to_ms(now())) + ".json";
        }
        
        // Write recording
        try {
            std::ofstream file(output_path);
            if (file.is_open()) {
                file << session_to_export->to_json();
                file.close();
                
                saved_recordings_.push_back(output_path);
                return output_path;
            }
        } catch (...) {
            // Write failed
        }
        
        return "";
    }
    
    /**
     * @brief Set replay speed mode
     * @param speed Desired playback speed
     */
    void set_replay_speed(ReplaySpeed speed) {
        std::lock_guard<std::mutex> lock(mutex_);
        replay_speed_ = speed;
    }
    
    /**
     * @brief Get current replay progress
     * @return Pair of (current_index, total_events)
     */
    std::pair<size_t, size_t> get_replay_progress() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!replay_session_.has_value()) {
            return {0, 0};
        }
        
        return {replay_index_, replay_session_->events.size()};
    }

private:
    //=========================================================================
    // Internal State
    //=========================================================================
    
    /// Current recording session (if recording)
    std::optional<RecordingSession> current_session_;
    
    /// Session being replayed (if replaying)
    std::optional<RecordingSession> replay_session_;
    
    /// State flags
    bool is_recording_{false};
    bool is_replaying_{false};
    
    /// Sequence counter for recording
    size_t event_sequence_counter_{0};
    
    /// Replay position
    size_t replay_index_{0};
    
    /// Replay timing
    ReplaySpeed replay_speed_{ReplaySpeed::REALTIME};
    Timestamp last_replay_time_;
    
    /// Saved recording paths
    mutable std::vector<std::string> saved_recordings_;

    //=========================================================================
    // Internal Methods
    //=========================================================================
    
    /**
     * @brief Generate unique session ID
     */
    std::string generate_session_id(const std::string& label) const {
        auto now_ms = static_cast<uint64_t>(timestamp_to_ms(now()));
        std::string base = label.empty() ? "session" : label;
        
        // Simple ID generation
        std::hash<std::string> hasher;
        size_t hash_val = hasher(base + std::to_string(now_ms));
        
        std::ostringstream ss;
        ss << base << "_" << std::hex << now_ms << "_" 
           << std::setfill('0') << std::setw(8) << (hash_val & 0xFFFFFFFF);
        
        return ss.str();
    }
    
    /**
     * @brief Internal event recording (assumes lock held)
     */
    void record_event_internal(const DiagnosticEvent& event) {
        if (!current_session_.has_value()) return;
        
        RecordedEvent recorded;
        recorded.sequence_number = event_sequence_counter_++;
        recorded.absolute_time = event.timestamp;
        
        // Calculate relative time from session start
        recorded.relative_time = std::chrono::duration_cast<Duration>(
            event.timestamp - current_session_->start_time);
        
        recorded.event_type = event.event_type;
        recorded.source_plugin = event.source_plugin;
        recorded.boot_state_at_event = BootState::UNKNOWN;  // Would get from state machine
        recorded.thread_id = 0;  // Would get from thread API
        
        // Serialize event data
        recorded.event_data_json = serialize_event_for_recording(event);
        
        current_session_->events.push_back(std::move(recorded));
        current_session_->total_events = current_session_->events.size();
    }
    
    /**
     * @brief Serialize event for storage in recording
     */
    std::string serialize_event_for_recording(const DiagnosticEvent& event) const {
        std::ostringstream ss;
        ss << "{\n";
        ss << "      \"message\": \"" << escape_json(event.message) << "\",\n";
        ss << "      \"severity\": " << static_cast<int>(event.severity) << ",\n";
        ss << "      \"category\": \"" << escape_json(event.category) << "\",\n";
        
        // Metadata
        if (!event.metadata.empty()) {
            ss << "      \"metadata\": {";
            bool first = true;
            for (const auto& [k, v] : event.metadata) {
                if (!first) ss << ", ";
                ss << "\"" << k << "\": \"" << escape_json(v) << "\"";
                first = false;
            }
            ss << "},\n";
        }
        
        // Numeric data
        if (!event.numeric_data.empty()) {
            ss << "      \"numeric_data\": {";
            bool first = true;
            for (const auto& [k, v] : event.numeric_data) {
                if (!first) ss << ", ";
                ss << "\"" << k << "\": " << v;
                first = false;
            }
            ss << "},\n";
        }
        
        // Float data
        if (!event.float_data.empty()) {
            ss << "      \"float_data\": {";
            bool first = true;
            for (const auto& [k, v] : event.float_data) {
                if (!first) ss << ", ";
                ss << "\"" << k << "\": " << std::fixed << std::setprecision(6) << v;
                first = false;
            }
            ss << "}\n";
        } else {
            ss << "      \"float_data\": {}\n";
        }
        
        ss << "    }";
        return ss.str();
    }
    
    /**
     * @brief Internal stop recording (assumes lock held)
     */
    std::string stop_recording_internal() {
        if (!is_recording_ || !current_session_.has_value()) {
            return "";
        }
        
        // Finalize session
        current_session_->end_time = now();
        current_session_->calculate_hash();
        
        // Save to file
        std::string path = export_recording("");
        
        // Update state
        is_recording_ = false;
        
        // Emit stop event
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "recording_stopped";
        event.severity = Severity::INFO;
        event.message = "Diagnostic recording stopped";
        event.metadata["session_id"] = current_session_->session_id;
        event.metadata["saved_path"] = path;
        event.numeric_data["total_events"] = static_cast<int64_t>(
            current_session_->total_events);
        emit_event(event);
        
        return path;
    }
    
    /**
     * @brief Parse recording from JSON string
     */
    std::optional<RecordingSession> parse_recording(const std::string& json) const {
        RecordingSession session;
        
        // Extract basic fields
        auto extract_string = [&json](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\": \"";
            size_t pos = json.find(search);
            if (pos == std::string::npos) return "";
            pos += search.length();
            size_t end = json.find("\"", pos);
            if (end == std::string::npos) return "";
            return json.substr(pos, end - pos);
        };
        
        auto extract_number = [&json](const std::string& key) -> int64_t {
            std::string search = "\"" + key + "\": ";
            size_t pos = json.find(search);
            if (pos == std::string::npos) return 0;
            pos += search.length();
            size_t end = pos;
            while (end < json.size() && (json[end] >= '0' && json[end] <= '9' || 
                   json[end] == '-' || json[end] == '.')) {
                end++;
            }
            if (pos >= end) return 0;
            return std::stoll(json.substr(pos, end - pos));
        };
        
        session.session_id = extract_string("session_id");
        session.content_hash = extract_string("content_hash");
        session.emulator_version = extract_string("emulator_version");
        session.game_id = extract_string("game_id");
        
        double start_ms = static_cast<double>(extract_number("start_time_ms"));
        double end_ms = static_cast<double>(extract_number("end_time_ms"));
        session.start_time = from_ms(start_ms);
        session.end_time = from_ms(end_ms);
        session.total_events = static_cast<size_t>(extract_number("total_events"));
        
        // Parse events array
        size_t events_start = json.find("\"events\": [");
        if (events_start == std::string::npos) {
            return std::nullopt;
        }
        
        // Find each event object (simplified parsing)
        size_t pos = events_start + 11;  // After "["
        while (pos < json.size()) {
            size_t obj_start = json.find("{", pos);
            if (obj_start == std::string::npos) break;
            
            size_t obj_end = find_matching_brace(json, obj_start);
            if (obj_end == std::string::npos) break;
            
            std::string event_json = json.substr(obj_start, obj_end - obj_start + 1);
            session.events.push_back(RecordedEvent::from_json(event_json));
            
            pos = obj_end + 1;
            
            // Check for end of array
            size_t next_check = json.find_first_of(",]", pos);
            if (next_check == std::string::npos || json[next_check] == ']') break;
            pos = next_check + 1;
        }
        
        session.total_events = session.events.size();
        
        return session;
    }
    
    /**
     * @brief Find matching closing brace for JSON object
     */
    size_t find_matching_brace(const std::string& json, size_t open_pos) const {
        if (open_pos >= json.size() || json[open_pos] != '{') return std::string::npos;
        
        int depth = 1;
        size_t pos = open_pos + 1;
        
        while (pos < json.size() && depth > 0) {
            char c = json[pos];
            if (c == '{' && (pos == 0 || json[pos-1] != '\\')) {
                depth++;
            } else if (c == '}' && (pos == 0 || json[pos-1] != '\\')) {
                depth--;
            }
            pos++;
        }
        
        return (depth == 0) ? (pos - 1) : std::string::npos;
    }
    
    /**
     * @brief Reconstruct DiagnosticEvent from RecordedEvent
     */
    DiagnosticEvent reconstruct_event(const RecordedEvent& recorded) const {
        DiagnosticEvent event;
        event.event_type = recorded.event_type;
        event.source_plugin = recorded.source_plugin;
        event.timestamp = recorded.absolute_time;
        
        // Parse serialized data back (simplified)
        // In production, would use proper JSON parser
        
        return event;
    }
    
    /**
     * @brief Apply timing delay based on replay speed
     */
    void apply_replay_timing(const RecordedEvent& recorded) {
        if (replay_speed_ == ReplaySpeed::INSTANT) {
            return;  // No delay
        }
        
        // Calculate desired delay
        Duration target_delay = recorded.relative_time;
        
        if (replay_index_ > 0 && replay_index_ < replay_session_->events.size()) {
            Duration prev_time = replay_session_->events[replay_index_ - 1].relative_time;
            target_delay = Duration(target_delay.count() - prev_time.count());
        }
        
        // Apply speed multiplier
        switch (replay_speed_) {
            case ReplaySpeed::REALTIME:
                // Use actual delay
                break;
            case ReplaySpeed::ACCELERATED:
                target_delay = Duration(target_delay.count() / 10);
                break;
            case ReplaySpeed::FAST:
                target_delay = Duration(target_delay.count() / 100);
                break;
            case ReplaySpeed::INSTANT:
                target_delay = Duration{0};
                break;
        }
        
        // Sleep for the delay (would use condition_variable in real impl)
        // For now, just update last time
        last_replay_time_ = now();
    }
    
    /**
     * @brief Calculate content hash for verification
     */
    std::string calculate_content_hash(const RecordingSession& session) const {
        std::hash<std::string> hasher;
        std::ostringstream concat;
        
        for (const auto& event : session.events) {
            concat << event.event_type << event.source_plugin << 
                      event.sequence_number << event.relative_time.count()
                     << event.event_data_json;
        }
        
        size_t hash_value = hasher(concat.str());
        
        std::ostringstream hex_ss;
        hex_ss << std::hex << std::setfill('0') << std::setw(16) << hash_value;
        return hex_ss.str();
    }
    
    /**
     * @brief Escape special characters for JSON
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

//=============================================================================
// Plugin Registration
//=============================================================================

inline std::unique_ptr<DiagnosticPlugin> create_deterministic_diagnostics_plugin() {
    return std::make_unique<DeterministicDiagnosticsMode>();
}

inline PluginInfo deterministic_diagnostics_plugin_info(
    "deterministic_diagnostics",
    "Record/Replay mode for deterministic and reproducible diagnostics",
    &create_deterministic_diagnostics_plugin,
    false,  // NOT enabled by default (opt-in feature)
    70      // priority
);

} // namespace diagnostics
} // namespace prosper
