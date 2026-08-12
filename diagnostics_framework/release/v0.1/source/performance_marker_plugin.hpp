#pragma once
#include "../core/diagnostic_interface.hpp"
#include "../core/event_bus.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_set>

/**
 * @file performance_marker_plugin.hpp
 * @brief Performance Marker Plugin - Frame/time-based performance tracking
 * 
 * Phase 9.5 Diagnostic Plugin for Prosper PS4 Emulator
 * 
 * Features:
 * - Marks performance points (frame start/end, vsync, etc.)
 * - Calculates FPS and frame times
 * - Detects frame spikes and performance regressions
 * - Uses atomic frame counter (CRITICAL FIX: NOT static variable)
 */

namespace prosper {
namespace diagnostics {

//=============================================================================
// Frame Data Structure
//=============================================================================

struct FrameData {
    uint64_t frame_number{0};
    Timestamp start{};
    Timestamp end{};
    Duration duration{0};  // Frame duration in microseconds
    double fps{0.0};       // Instantaneous FPS for this frame
    
    // Additional metrics
    double cpu_time_ms{0.0};
    double gpu_time_ms{0.0};
    size_t draw_calls{0};
    size_t triangle_count{0};
    
    // Vsync info
    bool vsynced{false};
    Timestamp vsync_time{};
    
    // Marker info
    std::vector<std::pair<Timestamp, std::string>> markers;
    
    // Serialization
    std::string to_json(bool detailed = false) const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"frame_number\":" << frame_number << ",";
        oss << "\"start_ms\":" << std::fixed << std::setprecision(3) << timestamp_to_ms(start) << ",";
        oss << "\"end_ms\:" << timestamp_to_ms(end) << ",";
        oss << "\"duration_us\:" << duration.count() << ",";
        oss << "\"duration_ms\:" << (duration.count() / 1000.0) << ",";
        oss << "\"fps\:" << std::setprecision(2) << fps << ",";
        oss << "\"cpu_time_ms\:" << cpu_time_ms << ",";
        oss << "\"gpu_time_ms\:" << gpu_time_ms << ",";
        oss << "\"draw_calls\:" << draw_calls << ",";
        oss << "\"triangle_count\:" << triangle_count << ",";
        oss << "\"vsynced\:" << (vsynced ? "true" : "false");
        
        if (detailed && !markers.empty()) {
            oss << ",\"markers\:[";
            for (size_t i = 0; i < markers.size(); ++i) {
                if (i > 0) oss << ",";
                oss << "{";
                oss << "\"time_ms\":" << timestamp_to_ms(markers[i].first) << ",";
                oss << "\"name\":\"" << escape_json(markers[i].second) << "\"";
                oss << "}";
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
// Performance Spike Detection Result
//=============================================================================

struct SpikeDetectionResult {
    bool spike_detected{false};
    uint64_t frame_number{0};
    double frame_time_ms{0.0};
    double expected_frame_time_ms{0.0};
    double deviation_percent{0.0};
    Severity severity{Severity::INFO};
    std::string description;
    Timestamp detected_at{};
    
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"spike_detected\":" << (spike_detected ? "true" : "false") << ",";
        oss << "\"frame_number\":" << frame_number << ",";
        oss << "\"frame_time_ms\":" << std::fixed << std::setprecision(3) << frame_time_ms << ",";
        oss << "\"expected_frame_time_ms\:" << expected_frame_time_ms << ",";
        oss << "\"deviation_percent\:" << std::setprecision(1) << deviation_percent << ",";
        oss << "\"severity\":\"" << to_string(severity) << "\",";
        oss << "\"description\":\"" << FrameData::escape_json(description) << "\",";
        oss << "\"detected_at_ms\:" << timestamp_to_ms(detected_at);
        oss << "}";
        return oss.str();
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
// FPS Statistics
//=============================================================================

struct FPSStatistics {
    double current_fps{0.0};
    double average_fps{0.0};
    double min_fps{std::numeric_limits<double>::max()};
    double max_fps{0.0};
    double p1_fps{0.0};   // 1st percentile (worst 1%)
    double p99_fps{0.0};  // 99th percentile (best 1%)
    uint64_t total_frames{0};
    Duration total_duration{0};
    
    void update(double instant_fps, Duration frame_duration) {
        current_fps = instant_fps;
        total_frames++;
        total_duration += frame_duration;
        
        if (instant_fps > 0) {
            min_fps = std::min(min_fps, instant_fps);
            max_fps = std::max(max_fps, instant_fps);
            
            if (total_frames > 0) {
                double total_sec = std::chrono::duration<double>(total_duration).count();
                if (total_sec > 0) {
                    average_fps = total_frames / total_sec;
                }
            }
        }
    }
    
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"current_fps\":" << std::fixed << std::setprecision(2) << current_fps << ",";
        oss << "\"average_fps\:" << average_fps << ",";
        oss << "\"min_fps\:" << (min_fps == std::numeric_limits<double>::max() ? 0 : min_fps) << ",";
        oss << "\"max_fps\:" << max_fps << ",";
        oss << "\"p1_fps\:" << p1_fps << ",";
        oss << "\"p99_fps\:" << p99_fps << ",";
        oss << "\"total_frames\:" << total_frames << ",";
        oss << "\"total_duration_ms\:" << (std::chrono::duration_cast<std::chrono::milliseconds>(total_duration).count());
        oss << "}";
        return oss.str();
    }
};

//=============================================================================
// Performance Marker Plugin Implementation
//=============================================================================

class PerformanceMarkerPlugin final : public DiagnosticPlugin {
public:
    PerformanceMarkerPlugin() = default;
    ~PerformanceMarkerPlugin() override { shutdown(); }
    
    //-------------------------------------------------------------------------
    // DiagnosticPlugin Interface
    //-------------------------------------------------------------------------
    
    std::string name() const override { return "performance_marker"; }
    std::string version() const override { return "1.0.0"; }
    std::string description() const override {
        return "Tracks frame-based performance markers, FPS, and detects spikes";
    }
    
    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        frames_.clear();
        spike_history_.clear();
        active_ = true;
        
        // CRITICAL FIX: Use atomic member variable, NOT static!
        frame_counter_.store(0);
        
        last_frame_end_ = {};
        fps_stats_ = FPSStatistics{};
        
        // Subscribe to relevant events
        if (global::is_initialized()) {
            auto* bus = global::get_event_bus();
            event_subscription_id_ = bus->subscribe(
                name(),
                [this](const DiagnosticEvent& evt) { on_event(evt); },
                [](const DiagnosticEvent& evt) {
                    return evt.event_type.find("frame") != std::string::npos ||
                           evt.event_type.find("vsync") != std::string::npos ||
                           evt.event_type.find("render") != std::string::npos ||
                           evt.category == "performance";
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
        frames_.clear();
        spike_history_.clear();
        
        // CRITICAL FIX: Reset the atomic counter properly
        frame_counter_.store(0);
        
        last_frame_end_ = {};
        fps_stats_ = FPSStatistics{};
        event_count_ = 0;
        current_frame_start_ = {};
        frame_in_progress_ = false;
    }
    
    void on_event(const DiagnosticEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return;
        
        enforce_event_limit();
        
        if (event.event_type == "frame_start") {
            mark_frame_start_internal(event.timestamp);
        } else if (event.event_type == "frame_end") {
            mark_frame_end_internal(event.timestamp);
        } else if (event.event_type == "vsync") {
            mark_vsync_internal(event.timestamp);
        }
    }
    
    std::string generate_report() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ostringstream oss;
        oss << "{";
        oss << "\"plugin\":\"" << name() << "\",";
        oss << "\"version\":\"" << version() << "\",";
        oss << "\"generated_at_ms\":" << timestamp_to_ms(now()) << ",";
        oss << "\"current_frame\":" << frame_counter_.load() << ",";
        
        // FPS statistics
        oss << "\"fps_statistics\":" << fps_stats_.to_json() << ",";
        
        // Recent frames
        oss << "\"recent_frames\":[";
        size_t frame_count = 0;
        for (auto it = frames_.rbegin(); it != frames_.rend() && frame_count < 30; ++it, ++frame_count) {
            if (frame_count > 0) oss << ",";
            oss << it->to_json(false);  // Non-detailed for report
        }
        oss << "],";
        
        // Spike summary
        size_t recent_spikes = count_recent_spikes(100);  // Last 100 frames
        oss << "\"recent_spike_count\:" << recent_spikes << ",";
        oss << "\"spike_threshold_ms\:" << spike_threshold_ms_ << ",";
        
        // Target FPS info
        oss << "\"target_fps\:" << target_fps_ << ",";
        oss << "\"target_frame_ms\:" << target_frame_ms_;
        
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
    // Frame Marking Methods
    //-------------------------------------------------------------------------
    
    /**
     * @brief Mark the beginning of a new frame
     * @param time Optional timestamp (uses now() if not provided)
     * @return The frame number that was started
     */
    uint64_t mark_frame_start(Timestamp time = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return 0;
        
        enforce_event_limit();
        return mark_frame_start_internal(time.time_since_epoch().count() ? time : now());
    }
    
    /**
     * @brief Mark the end of the current frame
     * @param time Optional timestamp (uses now() if not provided)
     * @return The frame number that ended
     */
    uint64_t mark_frame_end(Timestamp time = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return 0;
        
        enforce_event_limit();
        return mark_frame_end_internal(time.time_since_epoch().count() ? time : now());
    }
    
    /**
     * @brief Mark a vsync point
     * @param time Optional timestamp (uses now() if not provided)
     */
    void mark_vsync(Timestamp time = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return;
        
        enforce_event_limit();
        mark_vsync_internal(time.time_since_epoch().count() ? time : now());
    }
    
    /**
     * @brief Add a custom marker within the current frame
     * @param name Marker name/identifier
     * @param time Optional timestamp (uses now() if not provided)
     */
    void add_marker(const std::string& name, Timestamp time = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_ || !frame_in_progress_) return;
        
        enforce_event_limit();
        
        auto marker_time = time.time_since_epoch().count() ? time : now();
        
        // Add to current frame's markers (we need access to modify current frame)
        // Since we're tracking in frames_, find or create current frame entry
        uint64_t current_frame = frame_counter_.load();
        if (!frames_.empty() && frames_.back().frame_number == current_frame) {
            frames_.back().markers.emplace_back(marker_time, name);
        }
    }
    
    //-------------------------------------------------------------------------
    // Query Methods
    //-------------------------------------------------------------------------
    
    /**
     * @brief Get current FPS estimate
     * @return Current FPS based on most recent frame
     */
    double get_fps() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return fps_stats_.current_fps;
    }
    
    /**
     * @brief Get average FPS over all recorded frames
     * @return Average FPS
     */
    double get_average_fps() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return fps_stats_.average_fps;
    }
    
    /**
     * @brief Get the most recent frame time in milliseconds
     * @return Frame time in ms, or 0 if no frames recorded
     */
    double get_frame_time() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (frames_.empty()) return 0.0;
        return frames_.back().duration.count() / 1000.0;
    }
    
    /**
     * @brief Get average frame time over recent frames
     * @param window_size Number of frames to average over
     * @return Average frame time in ms
     */
    double get_average_frame_time(size_t window_size = 60) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (frames_.empty()) return 0.0;
        
        double sum = 0.0;
        size_t count = 0;
        
        for (auto it = frames_.rbegin(); it != frames_.rend() && count < window_size; ++it, ++count) {
            sum += it->duration.count();
        }
        
        return count > 0 ? (sum / count) / 1000.0 : 0.0;
    }
    
    /**
     * @brief Get data for a specific frame
     * @param frame_number Frame number to query
     * @return Frame data, or default if not found
     */
    FrameData get_frame_data(uint64_t frame_number) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // For efficiency with large datasets, we could use a map, but deque is fine for sequential access
        for (const auto& frame : frames_) {
            if (frame.frame_number == frame_number) {
                return frame;
            }
        }
        return FrameData{};
    }
    
    /**
     * @brief Get recent frame data
     * @param count Number of recent frames to return
     * @return Vector of frame data (most recent first)
     */
    std::vector<FrameData> get_recent_frames(size_t count = 60) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<FrameData> result;
        
        for (auto it = frames_.rbegin(); it != frames_.rend() && result.size() < count; ++it) {
            result.push_back(*it);
        }
        
        return result;
    }
    
    /**
     * @brief Detect frame time spikes in recent history
     * @param threshold_multiplier Multiplier for expected frame time threshold
     * @return Detection result with details about any detected spikes
     */
    SpikeDetectionResult detect_spikes(double threshold_multiplier = 2.0) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        SpikeDetectionResult result;
        result.detected_at = now();
        
        if (frames_.size() < 10) {
            result.description = "Insufficient data for spike detection";
            return result;
        }
        
        // Calculate rolling average of recent frames (excluding very recent ones which might be incomplete)
        double avg_frame_time = get_average_frame_time(std::min(frames_.size(), size_t{60}));
        double threshold = avg_frame_time * threshold_multiplier;
        if (threshold < spike_threshold_ms_) {
            threshold = spike_threshold_ms_;  // Use minimum threshold
        }
        
        // Check last few frames for spikes
        size_t check_count = std::min(frames_.size(), size_t{10});
        auto it = frames_.rbegin();
        
        for (size_t i = 0; i < check_count && it != frames_.rend(); ++i, ++it) {
            double frame_time_ms = it->duration.count() / 1000.0;
            
            if (frame_time_ms > threshold) {
                result.spike_detected = true;
                result.frame_number = it->frame_number;
                result.frame_time_ms = frame_time_ms;
                result.expected_frame_time_ms = avg_frame_time;
                result.deviation_percent = ((frame_time_ms - avg_frame_time) / 
                                           avg_frame_time) * 100.0;
                
                // Determine severity based on deviation
                if (result.deviation_percent > 500.0) {
                    result.severity = Severity::CRITICAL;
                } else if (result.deviation_percent > 200.0) {
                    result.severity = Severity::ERROR;
                } else if (result.deviation_percent > 100.0) {
                    result.severity = Severity::WARNING;
                } else {
                    result.severity = Severity::INFO;
                }
                
                result.description = "Frame " + std::to_string(it->frame_number) + 
                    " spike: " + std::to_string(frame_time_ms, 1) + "ms (" +
                    std::to_string(result.deviation_percent, 1) + "% above average)";
                
                // Record spike
                spike_history_.push_back(result);
                while (spike_history_.size() > MAX_SPIKE_HISTORY) {
                    spike_history_.pop_front();
                }
                
                // Emit diagnostic event
                DiagnosticEvent event;
                event.source_plugin = name();
                event.event_type = "frame_spike";
                event.severity = result.severity;
                event.message = result.description;
                event.category = "performance";
                event.numeric_data["frame_number"] = it->frame_number;
                event.float_data["frame_time_ms"] = frame_time_ms;
                event.float_data["avg_frame_time_ms"] = avg_frame_time;
                event.float_data["deviation_percent"] = result.deviation_percent;
                
                emit_event(event);
                
                break;  // Report only the most recent spike
            }
        }
        
        if (!result.spike_detected) {
            result.description = "No frame spikes detected in recent frames";
        }
        
        return result;
    }
    
    /**
     * @brief Get spike detection history
     * @param max_results Maximum results to return
     * @return Vector of past spike detections
     */
    std::vector<SpikeDetectionResult> get_spike_history(size_t max_results = 50) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<SpikeDetectionResult> result;
        
        for (auto it = spike_history_.rbegin(); 
             it != spike_history_.rend() && result.size() < max_results; ++it) {
            result.push_back(*it);
        }
        
        return result;
    }
    
    /**
     * @brief Get complete FPS statistics
     * @return Current FPS statistics structure
     */
    FPSStatistics get_fps_statistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return fps_stats_;
    }
    
    /**
     * @brief Set target FPS for frame time calculations
     * @param target_fps Target frames per second (e.g., 30 or 60)
     */
    void set_target_fps(double target_fps) {
        std::lock_guard<std::mutex> lock(mutex_);
        target_fps_ = target_fps;
        target_frame_ms_ = (target_fps > 0) ? (1000.0 / target_fps) : 16.67;  // Default ~60fps
    }
    
    /**
     * @brief Set spike detection threshold
     * @param threshold_ms Minimum frame time in ms to consider a spike
     */
    void set_spike_threshold(double threshold_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        spike_threshold_ms_ = threshold_ms;
    }

private:
    //-------------------------------------------------------------------------
    // Internal Data Structures
    //-------------------------------------------------------------------------
    
    // CRITICAL FIX: Use atomic<uint64_t> as a class member, NOT a static variable!
    // This fixes the known bug where static variables cause issues with multiple instances
    // or reset scenarios.
    std::atomic<uint64_t> frame_counter_{0};
    
    // Frame storage (ring buffer behavior via size limit)
    std::deque<FrameData> frames_;
    static constexpr size_t MAX_FRAMES_STORED = 3600;  // Store up to 1 minute at 60fps
    
    // Spike detection history
    std::deque<SpikeDetectionResult> spike_history_;
    static constexpr size_t MAX_SPIKE_HISTORY = 1000;
    
    // Timing state
    Timestamp last_frame_end_{};
    Timestamp current_frame_start_{};
    bool frame_in_progress_{false};
    
    // Statistics
    FPSStatistics fps_stats_;
    
    // Configuration
    double target_fps_{60.0};
    double target_frame_ms_{16.67};  // ~60fps
    double spike_threshold_ms_{50.0};  // Frames longer than 50ms are potential spikes
    
    std::string event_subscription_id_;
    
    //-------------------------------------------------------------------------
    // Internal Helper Methods
    //-------------------------------------------------------------------------
    
    uint64_t mark_frame_start_internal(Timestamp time) {
        // CRITICAL FIX: Use atomic increment for thread-safe frame counting
        uint64_t frame_num = frame_counter_.fetch_add(1);
        
        FrameData frame;
        frame.frame_number = frame_num;
        frame.start = time;
        frame.vsynced = false;
        
        frames_.push_back(frame);
        
        // Enforce bounded storage
        while (frames_.size() > MAX_FRAMES_STORED) {
            frames_.pop_front();
        }
        
        current_frame_start_ = time;
        frame_in_progress_ = true;
        event_count_++;
        
        // Emit diagnostic event
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "frame_start";
        event.severity = Severity::DEBUG;
        event.message = "Frame " + std::to_string(frame_num) + " started";
        event.category = "performance";
        event.numeric_data["frame_number"] = frame_num;
        
        emit_event(event);
        
        return frame_num;
    }
    
    uint64_t mark_frame_end_internal(Timestamp time) {
        if (frames_.empty()) return 0;
        
        FrameData& frame = frames_.back();
        frame.end = time;
        frame.duration = std::chrono::duration_cast<Duration>(time - frame.start);
        
        // Calculate instantaneous FPS
        if (frame.duration.count() > 0) {
            frame.fps = 1000000.0 / frame.duration.count();  // Convert us to FPS
        }
        
        // Update FPS statistics
        fps_stats_.update(frame.fps, frame.duration);
        
        frame_in_progress_ = false;
        last_frame_end_ = time;
        event_count_++;
        
        // Emit diagnostic event (throttled - only for slow frames)
        double frame_ms = frame.duration.count() / 1000.0;
        if (frame_ms > target_frame_ms_ * 2.0) {
            DiagnosticEvent event;
            event.source_plugin = name();
            event.event_type = "frame_end_slow";
            event.severity = frame_ms > target_frame_ms_ * 4.0 ? 
                             Severity::WARNING : Severity::INFO;
            event.message = "Frame " + std::to_string(frame.frame_number) + 
                           " ended: " + std::to_string(frame_ms, 1) + "ms";
            event.category = "performance";
            event.numeric_data["frame_number"] = frame.frame_number;
            event.float_data["frame_time_ms"] = frame_ms;
            event.float_data["fps"] = frame.fps;
            
            emit_event(event);
        }
        
        return frame.frame_number;
    }
    
    void mark_vsync_internal(Timestamp time) {
        if (!frames_.empty()) {
            frames_.back().vsynced = true;
            frames_.back().vsync_time = time;
        }
        
        event_count_++;
        
        // Emit vsync event (low priority)
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "vsync";
        event.severity = Severity::DEBUG;
        event.message = "VSync at " + std::to_string(timestamp_to_ms(time), 1) + "ms";
        event.category = "performance";
        
        emit_event(event);
    }
    
    size_t count_recent_spikes(size_t window) const {
        size_t count = 0;
        size_t checked = 0;
        
        for (auto it = spike_history_.rbegin(); 
             it != spike_history_.rend() && checked < window; ++it, ++checked) {
            if (it->spike_detected) count++;
        }
        
        return count;
    }
    
    void enforce_event_limit() {
        if (global::get_config()) {
            size_t max_events = global::get_config()->max_events_per_plugin;
            
            while (frames_.size() >= max_events) {
                frames_.pop_front();
            }
        }
    }
};

} // namespace diagnostics
} // namespace prosper
