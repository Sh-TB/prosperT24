#pragma once
#include "../core/diagnostic_interface.hpp"
#include "../core/event_bus.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_set>
#include <algorithm>

/**
 * @file file_access_plugin.hpp
 * @brief File Access Plugin - Tracks file system operations
 * 
 * Phase 9.5 Diagnostic Plugin for Prosper PS4 Emulator
 * 
 * Features:
 * - Records file open/read/write/close operations
 * - Tracks file paths and access patterns
 * - Detects unusual access patterns (missing files, permission issues)
 * - Provides comprehensive access log queries
 */

namespace prosper {
namespace diagnostics {

//=============================================================================
// File Access Record Structure
//=============================================================================

struct FileAccess {
    std::string path;
    std::string operation;  // "open", "read", "write", "close", "seek", "stat"
    Timestamp time{};
    size_t bytes{0};
    bool success{true};
    int error_code{0};
    std::string error_message;
    uint32_t thread_id{0};
    int file_descriptor{-1};  // -1 if not applicable
    double duration_us{0.0};  // Operation duration in microseconds
    std::string mode;  // Open mode: "r", "w", "rb", "wb", etc.
    uint64_t offset{0};  // File offset for read/write operations
    
    // Serialization
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"path\":\"" << escape_json(path) << "\",";
        oss << "\"operation\":\"" << operation << "\",";
        oss << "\"time_ms\":" << std::fixed << std::setprecision(3) << timestamp_to_ms(time) << ",";
        oss << "\"bytes\":" << bytes << ",";
        oss << "\"success\":" << (success ? "true" : "false") << ",";
        oss << "\"error_code\":" << error_code << ",";
        oss << "\"error_message\":\"" << escape_json(error_message) << "\",";
        oss << "\"thread_id\":" << thread_id << ",";
        oss << "\"file_descriptor\":" << file_descriptor << ",";
        oss << "\"duration_us\":" << std::setprecision(2) << duration_us << ",";
        oss << "\"mode\":\"" << escape_json(mode) << "\",";
        oss << "\"offset\":" << offset;
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
// File Statistics Structure
//=============================================================================

struct FileStatistics {
    std::string path;
    size_t open_count{0};
    size_t read_count{0};
    size_t write_count{0};
    size_t close_count{0};
    size_t total_bytes_read{0};
    size_t total_bytes_written{0};
    double total_read_time_us{0.0};
    double total_write_time_us{0.0};
    bool ever_failed{false};
    int last_error_code{0};
    Timestamp first_access{};
    Timestamp last_access{};
    std::unordered_set<int> file_descriptors_used;
    
    void update(const FileAccess& access) {
        if (access.operation == "open") {
            open_count++;
            if (first_access == Timestamp{}) first_access = access.time;
        } else if (access.operation == "read") {
            read_count++;
            total_bytes_read += access.bytes;
            total_read_time_us += access.duration_us;
        } else if (access.operation == "write") {
            write_count++;
            total_bytes_written += access.bytes;
            total_write_time_us += access.duration_us;
        } else if (access.operation == "close") {
            close_count++;
        }
        
        if (!access.success) {
            ever_failed = true;
            last_error_code = access.error_code;
        }
        
        last_access = access.time;
        
        if (access.file_descriptor >= 0) {
            file_descriptors_used.insert(access.file_descriptor);
        }
    }
    
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"path\":\"" << FileAccess::escape_json(path) << "\",";
        oss << "\"open_count\":" << open_count << ",";
        oss << "\"read_count\":" << read_count << ",";
        oss << "\"write_count\":" << write_count << ",";
        oss << "\"close_count\:" << close_count << ",";
        oss << "\"total_bytes_read\":" << total_bytes_read << ",";
        oss << "\"total_bytes_written\":" << total_bytes_written << ",";
        oss << "\"total_read_time_us\":" << std::fixed << std::setprecision(2) << total_read_time_us << ",";
        oss << "\"total_write_time_us\":" << total_write_time_us << ",";
        oss << "\"ever_failed\":" << (ever_failed ? "true" : "false") << ",";
        oss << "\"last_error_code\":" << last_error_code << ",";
        oss << "\"first_access_ms\":" << timestamp_to_ms(first_access) << ",";
        oss << "\"last_access_ms\":" << timestamp_to_ms(last_access);
        oss << "}";
        return oss.str();
    }
};

//=============================================================================
// Access Pattern Anomaly
//=============================================================================

struct AccessAnomaly {
    std::string type;  // "missing_file", "permission_denied", "unusual_pattern", "too_many_opens"
    std::string path;
    std::string description;
    double severity_score{0.0};  // 0.0 to 1.0
    Timestamp detected_at{};
    size_t occurrence_count{1};
    std::vector<std::string> related_operations;
    
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"type\":\"" << FileAccess::escape_json(type) << "\",";
        oss << "\"path\":\"" << FileAccess::escape_json(path) << "\",";
        oss << "\"description\":\"" << FileAccess::escape_json(description) << "\",";
        oss << "\"severity_score\":" << std::fixed << std::setprecision(2) << severity_score << ",";
        oss << "\"detected_at_ms\":" << timestamp_to_ms(detected_at) << ",";
        oss << "\"occurrence_count\:" << occurrence_count << ",";
        oss << "\"related_operations\":[";
        for (size_t i = 0; i < related_operations.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << FileAccess::escape_json(related_operations[i]) << "\"";
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

//=============================================================================
// File Access Plugin Implementation
//=============================================================================

class FileAccessPlugin final : public DiagnosticPlugin {
public:
    FileAccessPlugin() = default;
    ~FileAccessPlugin() override { shutdown(); }
    
    //-------------------------------------------------------------------------
    // DiagnosticPlugin Interface
    //-------------------------------------------------------------------------
    
    std::string name() const override { return "file_access"; }
    std::string version() const override { return "1.0.0"; }
    std::string description() const override {
        return "Tracks file system operations and detects unusual access patterns";
    }
    
    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        access_log_.clear();
        file_stats_.clear();
        anomalies_.clear();
        active_open_files_.clear();
        active_ = true;
        
        // Subscribe to relevant events
        if (global::is_initialized()) {
            auto* bus = global::get_event_bus();
            event_subscription_id_ = bus->subscribe(
                name(),
                [this](const DiagnosticEvent& evt) { on_event(evt); },
                [](const DiagnosticEvent& evt) {
                    return evt.event_type.find("file") != std::string::npos ||
                           evt.event_type.find("io") != std::string::npos ||
                           evt.category == "filesystem";
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
        access_log_.clear();
        file_stats_.clear();
        anomalies_.clear();
        active_open_files_.clear();
        event_count_ = 0;
    }
    
    void on_event(const DiagnosticEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return;
        
        // Check event limit
        enforce_event_limit();
        
        // Process file-related events
        if (event.event_type == "file_opened") {
            std::string path = event.metadata.count("path") ?
                               event.metadata.at("path") : "";
            std::string mode = event.metadata.count("mode") ?
                               event.metadata.at("mode") : "r";
            bool success = event.numeric_data.count("success") ?
                          event.numeric_data.at("success") != 0 : true;
            int fd = event.numeric_data.count("fd") ?
                    static_cast<int>(event.numeric_data.at("fd")) : -1;
            
            FileAccess access;
            access.path = path;
            access.operation = "open";
            access.time = event.timestamp;
            access.success = success;
            access.error_code = static_cast<int>(
                event.numeric_data.count("error_code") ? 
                event.numeric_data.at("error_code") : 0);
            access.error_message = event.metadata.count("error") ?
                                   event.metadata.at("error") : "";
            access.mode = mode;
            access.file_descriptor = fd;
            
            record_access(access);
            
            if (success && fd >= 0) {
                active_open_files_[fd] = path;
            }
        }
        else if (event.event_type == "file_read") {
            std::string path = event.metadata.count("path") ?
                               event.metadata.at("path") : "";
            size_t bytes = event.numeric_data.count("bytes") ?
                          static_cast<size_t>(event.numeric_data.at("bytes")) : 0;
            int fd = event.numeric_data.count("fd") ?
                    static_cast<int>(event.numeric_data.at("fd")) : -1;
            
            if (path.empty() && fd >= 0) {
                auto it = active_open_files_.find(fd);
                if (it != active_open_files_.end()) path = it->second;
            }
            
            FileAccess access;
            access.path = path;
            access.operation = "read";
            access.time = event.timestamp;
            access.bytes = bytes;
            access.success = true;
            access.file_descriptor = fd;
            access.duration_us = event.float_data.count("duration_us") ?
                                event.float_data.at("duration_us") : 0.0;
            
            record_access(access);
        }
        else if (event.event_type == "file_write") {
            std::string path = event.metadata.count("path") ?
                               event.metadata.at("path") : "";
            size_t bytes = event.numeric_data.count("bytes") ?
                          static_cast<size_t>(event.numeric_data.at("bytes")) : 0;
            int fd = event.numeric_data.count("fd") ?
                    static_cast<int>(event.numeric_data.at("fd")) : -1;
            
            if (path.empty() && fd >= 0) {
                auto it = active_open_files_.find(fd);
                if (it != active_open_files_.end()) path = it->second;
            }
            
            FileAccess access;
            access.path = path;
            access.operation = "write";
            access.time = event.timestamp;
            access.bytes = bytes;
            access.success = true;
            access.file_descriptor = fd;
            access.duration_us = event.float_data.count("duration_us") ?
                                event.float_data.at("duration_us") : 0.0;
            
            record_access(access);
        }
        else if (event.event_type == "file_closed") {
            int fd = event.numeric_data.count("fd") ?
                    static_cast<int>(event.numeric_data.at("fd")) : -1;
            std::string path = event.metadata.count("path") ?
                               event.metadata.at("path") : "";
            
            if (path.empty() && fd >= 0) {
                auto it = active_open_files_.find(fd);
                if (it != active_open_files_.end()) {
                    path = it->second;
                    active_open_files_.erase(it);
                }
            }
            
            FileAccess access;
            access.path = path;
            access.operation = "close";
            access.time = event.timestamp;
            access.success = true;
            access.file_descriptor = fd;
            
            record_access(access);
        }
    }
    
    std::string generate_report() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ostringstream oss;
        oss << "{";
        oss << "\"plugin\":\"" << name() << "\",";
        oss << "\"version\":\"" << version() << "\",";
        oss << "\"generated_at_ms\":" << timestamp_to_ms(now()) << ",";
        oss << "\"total_accesses\":" << access_log_.size() << ",";
        oss << "\"unique_files\":" << file_stats_.size() << ",";
        oss << "\"anomaly_count\":" << anomalies_.size() << ",";
        oss << "\"currently_open_files\":" << active_open_files_.size() << ",";
        
        // Summary statistics
        size_t total_reads = 0, total_writes = 0, total_bytes_read = 0, total_bytes_written = 0;
        size_t failed_ops = 0;
        
        for (const auto& pair : file_stats_) {
            total_reads += pair.second.read_count;
            total_writes += pair.second.write_count;
            total_bytes_read += pair.second.total_bytes_read;
            total_bytes_written += pair.second.total_bytes_written;
            if (pair.second.ever_failed) failed_ops++;
        }
        
        oss << "\"summary\":{";
        oss << "\"total_reads\":" << total_reads << ",";
        oss << "\"total_writes\":" << total_writes << ",";
        oss << "\"total_bytes_read\":" << total_bytes_read << ",";
        oss << "\"total_bytes_written\":" << total_bytes_written << ",";
        oss << "\"failed_file_count\":" << failed_ops;
        oss << "},";
        
        // Top files by access count
        oss << "\"top_files\":[";
        auto top_files = get_top_files_by_access(10);
        for (size_t i = 0; i < top_files.size(); ++i) {
            if (i > 0) oss << ",";
            oss << top_files[i].to_json();
        }
        oss << "],";
        
        // Recent anomalies
        oss << "\"anomalies\":[";
        size_t anomaly_count = 0;
        for (auto it = anomalies_.rbegin(); 
             it != anomalies_.rend() && anomaly_count < 20; ++it, ++anomaly_count) {
            if (anomaly_count > 0) oss << ",";
            oss << it->to_json();
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
    // File Operation Recording Methods
    //-------------------------------------------------------------------------
    
    /**
     * @brief Record a file open operation
     * @param path File path being opened
     * @param mode Open mode ("r", "w", "rb", "wb", "r+", etc.)
     * @param success Whether the operation succeeded
     * @param error_code Error code if failed
     * @param error_message Human-readable error message
     * @param fd File descriptor assigned (-1 if failed)
     * @return The file descriptor or -1 on failure
     */
    int on_file_opened(const std::string& path, const std::string& mode = "r",
                       bool success = true, int error_code = 0,
                       const std::string& error_message = "", int fd = -1) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return fd;
        
        enforce_event_limit();
        
        FileAccess access;
        access.path = path;
        access.operation = "open";
        access.time = now();
        access.success = success;
        access.error_code = error_code;
        access.error_message = error_message;
        access.mode = mode;
        access.file_descriptor = fd;
        
        record_access_internal(access);
        
        if (success && fd >= 0) {
            active_open_files_[fd] = path;
        }
        
        // Detect anomalies for failed opens
        if (!success) {
            detect_anomaly(access);
        }
        
        // Emit diagnostic event
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "file_opened";
        event.severity = success ? Severity::DEBUG : Severity::ERROR;
        event.message = std::string("File ") + (success ? "opened" : "open failed") + ": " + path;
        event.category = "filesystem";
        event.metadata["path"] = path;
        event.metadata["mode"] = mode;
        event.numeric_data["fd"] = fd;
        event.numeric_data["success"] = success ? 1 : 0;
        event.numeric_data["error_code"] = error_code;
        if (!error_message.empty()) {
            event.metadata["error"] = error_message;
        }
        
        emit_event(event);
        
        return fd;
    }
    
    /**
     * @brief Record a file read operation
     * @param path File path (or empty if using fd)
     * @param bytes Number of bytes read
     * @param fd File descriptor
     * @param duration_us Operation duration in microseconds
     * @return Number of bytes actually recorded
     */
    size_t on_file_read(const std::string& path, size_t bytes, int fd = -1,
                        double duration_us = 0.0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return bytes;
        
        enforce_event_limit();
        
        std::string resolved_path = path;
        if (resolved_path.empty() && fd >= 0) {
            auto it = active_open_files_.find(fd);
            if (it != active_open_files_.end()) resolved_path = it->second;
        }
        
        FileAccess access;
        access.path = resolved_path;
        access.operation = "read";
        access.time = now();
        access.bytes = bytes;
        access.success = true;
        access.file_descriptor = fd;
        access.duration_us = duration_us;
        
        record_access_internal(access);
        
        // Emit diagnostic event for large reads
        if (bytes > 1024 * 1024) {  // > 1MB
            DiagnosticEvent event;
            event.source_plugin = name();
            event.event_type = "file_read";
            event.severity = Severity::DEBUG;
            event.message = "Large read: " + std::to_string(bytes) + " bytes from " + resolved_path;
            event.category = "filesystem";
            event.metadata["path"] = resolved_path;
            event.numeric_data["bytes"] = bytes;
            event.numeric_data["fd"] = fd;
            event.float_data["duration_us"] = duration_us;
            
            emit_event(event);
        }
        
        return bytes;
    }
    
    /**
     * @brief Record a file write operation
     * @param path File path (or empty if using fd)
     * @param bytes Number of bytes written
     * @param fd File descriptor
     * @param duration_us Operation duration in microseconds
     * @return Number of bytes actually recorded
     */
    size_t on_file_written(const std::string& path, size_t bytes, int fd = -1,
                           double duration_us = 0.0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return bytes;
        
        enforce_event_limit();
        
        std::string resolved_path = path;
        if (resolved_path.empty() && fd >= 0) {
            auto it = active_open_files_.find(fd);
            if (it != active_open_files_.end()) resolved_path = it->second;
        }
        
        FileAccess access;
        access.path = resolved_path;
        access.operation = "write";
        access.time = now();
        access.bytes = bytes;
        access.success = true;
        access.file_descriptor = fd;
        access.duration_us = duration_us;
        
        record_access_internal(access);
        
        // Emit diagnostic event for large writes
        if (bytes > 1024 * 1024) {  // > 1MB
            DiagnosticEvent event;
            event.source_plugin = name();
            event.event_type = "file_write";
            event.severity = Severity::DEBUG;
            event.message = "Large write: " + std::to_string(bytes) + " bytes to " + resolved_path;
            event.category = "filesystem";
            event.metadata["path"] = resolved_path;
            event.numeric_data["bytes"] = bytes;
            event.numeric_data["fd"] = fd;
            event.float_data["duration_us"] = duration_us;
            
            emit_event(event);
        }
        
        return bytes;
    }
    
    /**
     * @brief Record a file close operation
     * @param fd File descriptor being closed
     * @param path File path (optional, will be looked up from fd)
     */
    void on_file_closed(int fd, const std::string& path = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return;
        
        enforce_event_limit();
        
        std::string resolved_path = path;
        if (resolved_path.empty() && fd >= 0) {
            auto it = active_open_files_.find(fd);
            if (it != active_open_files_.end()) {
                resolved_path = it->second;
                active_open_files_.erase(it);
            }
        }
        
        FileAccess access;
        access.path = resolved_path;
        access.operation = "close";
        access.time = now();
        access.success = true;
        access.file_descriptor = fd;
        
        record_access_internal(access);
    }
    
    //-------------------------------------------------------------------------
    // Query Methods
    //-------------------------------------------------------------------------
    
    /**
     * @brief Get the complete access log
     * @param max_entries Maximum number of entries to return (0 = all)
     * @param filter_operation Optional operation type filter
     * @return Vector of file access records
     */
    std::vector<FileAccess> get_access_log(size_t max_entries = 0,
                                           const std::string& filter_operation = "") const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<FileAccess> result;
        
        if (filter_operation.empty()) {
            result.assign(access_log_.begin(), access_log_.end());
        } else {
            for (const auto& access : access_log_) {
                if (access.operation == filter_operation) {
                    result.push_back(access);
                }
            }
        }
        
        if (max_entries > 0 && result.size() > max_entries) {
            // Return most recent entries
            result.erase(result.begin(), result.end() - max_entries);
        }
        
        return result;
    }
    
    /**
     * @brief Get access log for a specific file
     * @param path File path to filter by
     * @return Vector of access records for that file
     */
    std::vector<FileAccess> get_file_access_history(const std::string& path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<FileAccess> result;
        
        for (const auto& access : access_log_) {
            if (access.path == path) {
                result.push_back(access);
            }
        }
        
        return result;
    }
    
    /**
     * @brief Get statistics for a specific file
     * @param path File path
     * @return File statistics, or default if not found
     */
    FileStatistics get_file_statistics(const std::string& path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = file_stats_.find(path);
        if (it != file_stats_.end()) {
            return it->second;
        }
        return FileStatistics{};
    }
    
    /**
     * @brief Get statistics for all tracked files
     * @return Vector of file statistics sorted by total access count
     */
    std::vector<FileStatistics> get_all_file_statistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<FileStatistics> result;
        result.reserve(file_stats_.size());
        
        for (const auto& pair : file_stats_) {
            result.push_back(pair.second);
        }
        
        // Sort by total access count (descending)
        std::sort(result.begin(), result.end(), [](const FileStatistics& a, const FileStatistics& b) {
            size_t total_a = a.open_count + a.read_count + a.write_count + a.close_count;
            size_t total_b = b.open_count + b.read_count + b.write_count + b.close_count;
            return total_a > total_b;
        });
        
        return result;
    }
    
    /**
     * @brief Get top N files by access count
     * @param n Number of files to return
     * @return Vector of file statistics
     */
    std::vector<FileStatistics> get_top_files_by_access(size_t n) const {
        auto all_stats = get_all_file_statistics();
        
        if (all_stats.size() > n) {
            all_stats.resize(n);
        }
        
        return all_stats;
    }
    
    /**
     * @brief Get detected anomalies
     * @param max_results Maximum number of results
     * @return Vector of detected anomalies
     */
    std::vector<AccessAnomaly> get_anomalies(size_t max_results = 50) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<AccessAnomaly> result;
        
        for (auto it = anomalies_.rbegin(); 
             it != anomalies_.rend() && result.size() < max_results; ++it) {
            result.push_back(*it);
        }
        
        return result;
    }
    
    /**
     * @brief Get currently open files
     * @return Map of fd -> path
     */
    std::unordered_map<int, std::string> get_open_files() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_open_files_;
    }
    
    /**
     * @brief Find files with failures
     * @return Vector of paths that had failed operations
     */
    std::vector<std::string> get_failed_files() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::string> result;
        
        for (const auto& pair : file_stats_) {
            if (pair.second.ever_failed) {
                result.push_back(pair.first);
            }
        }
        
        return result;
    }

private:
    //-------------------------------------------------------------------------
    // Internal Data Structures
    //-------------------------------------------------------------------------
    
    // Ring buffer for access log (bounded memory)
    std::deque<FileAccess> access_log_;
    static constexpr size_t MAX_ACCESS_LOG_SIZE = 50000;
    
    std::unordered_map<std::string, FileStatistics> file_stats_;
    std::deque<AccessAnomaly> anomalies_;
    static constexpr size_t MAX_ANOMALIES = 1000;
    
    std::unordered_map<int, std::string> active_open_files_;
    std::string event_subscription_id_;
    
    //-------------------------------------------------------------------------
    // Internal Helper Methods
    //-------------------------------------------------------------------------
    
    void record_access(const FileAccess& access) {
        record_access_internal(access);
    }
    
    void record_access_internal(const FileAccess& access) {
        // Add to access log with bounded storage
        access_log_.push_back(access);
        while (access_log_.size() > MAX_ACCESS_LOG_SIZE) {
            access_log_.pop_front();
        }
        
        // Update file statistics
        auto it = file_stats_.find(access.path);
        if (it == file_stats_.end()) {
            FileStats stats;
            stats.path = access.path;
            stats.update(access);
            file_stats_[access.path] = stats;
        } else {
            it->second.update(access);
        }
        
        event_count_++;
    }
    
    void detect_anomaly(const FileAccess& access) {
        AccessAnomaly anomaly;
        anomaly.detected_at = now();
        anomaly.path = access.path;
        
        // Classify anomaly type
        if (access.operation == "open" && !access.success) {
            if (access.error_code == ENOENT || access.error_code == 2) {  // No such file
                anomaly.type = "missing_file";
                anomaly.description = "File not found: " + access.path;
                anomaly.severity_score = 0.8;
            } else if (access.error_code == EACCES || access.error_code == 13) {  // Permission denied
                anomaly.type = "permission_denied";
                anomaly.description = "Permission denied: " + access.path;
                anomaly.severity_score = 0.7;
            } else if (access.error_code == EMFILE || access.error_code == 24) {  // Too many open files
                anomaly.type = "too_many_opens";
                anomaly.description = "Too many open files when trying to open: " + access.path;
                anomaly.severity_score = 0.9;
            } else {
                anomaly.type = "open_failure";
                anomaly.description = "Failed to open " + access.path + 
                                     " (error " + std::to_string(access.error_code) + ")";
                anomaly.severity_score = 0.5;
            }
        }
        
        if (!anomaly.type.empty()) {
            // Check for duplicate anomaly
            bool is_duplicate = false;
            for (auto& existing : anomalies_) {
                if (existing.type == anomaly.type && existing.path == anomaly.path) {
                    existing.occurrence_count++;
                    existing.detected_at = now();
                    is_duplicate = true;
                    break;
                }
            }
            
            if (!is_duplicate) {
                anomaly.related_operations.push_back(access.operation);
                anomalies_.push_back(anomaly);
                
                while (anomalies_.size() > MAX_ANOMALIES) {
                    anomalies_.pop_front();
                }
                
                // Emit warning event
                DiagnosticEvent event;
                event.source_plugin = name();
                event.event_type = "access_anomaly";
                event.severity = anomaly.severity_score > 0.7 ? Severity::WARNING : Severity::INFO;
                event.message = anomaly.description;
                event.category = "anomaly_detection";
                event.metadata["anomaly_type"] = anomaly.type;
                event.metadata["path"] = anomaly.path;
                event.float_data["severity"] = anomaly.severity_score;
                
                emit_event(event);
            }
        }
    }
    
    void enforce_event_limit() {
        if (global::get_config()) {
            size_t max_events = global::get_config()->max_events_per_plugin;
            
            while (access_log_.size() >= max_events) {
                access_log_.pop_front();
            }
        }
    }
};

} // namespace diagnostics
} // namespace prosper
