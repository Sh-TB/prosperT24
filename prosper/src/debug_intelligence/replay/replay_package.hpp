/**
 * Replay Debug Package - Priority 7
 * 
 * PROBLEM SOLVED:
 * Many bugs require reproducing exact previous state.
 * Create deterministic debug package that captures everything needed
 * to reopen an investigation.
 * 
 * PACKAGE CONTENTS:
 *   debug_replay/
 *   ├── commit.json           Git/build state
 *   ├── environment.json      System environment
 *   ├── timeline.json         State timeline events
 *   ├── memory_events.json    Memory provenance data
 *   ├── hle_contracts.json    HLE contract violations
 *   ├── diagnostics.json      Diagnostic execution status
 *   ├── hypotheses.json       Investigation hypotheses
 *   ├── causal_graph.json    Dependency analysis
 *   ├── logs/                 Captured log files
 *   ├── screenshots/          Visual state captures
 *   └── crash.json            Crash/corruption details
 * 
 * GOAL:
 * A developer or AI agent can:
 * 1. Open this package months later
 * 2. Understand exactly what happened
 * 3. Continue investigation from where it left off
 * 4. Verify fixes against captured evidence
 */

#pragma once

#include "../core/foundation.hpp"
#include <mutex>
#include <atomic>
#include <fstream>

namespace prosper_debug {
namespace replay {

// ============================================================================
// Package Metadata
// ============================================================================

struct PackageMetadata {
    std::string package_id;
    std::string title;
    std::string description;
    
    // Versioning
    std::string platform_version;   // Debug Intelligence version
    std::string emulator_version;   // Prosper version
    
    // Timing
    DebugTimestamp created_at;
    DebugTimestamp updated_at;
    DebugTimestamp completed_at;
    
    // Status
    enum class Status {
        InProgress,
        Completed,
        Archived,
        Superseded
    } status{Status::InProgress};
    
    // Scope
    std::vector<std::string> components_included;
    size_t total_size_bytes{0};
    
    // Provenance
    std::string creator;
    std::string machine_info;
    
    static std::string generateId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "replay_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    PackageMetadata() {
        package_id = generateId();
        created_at = DebugTimestamp::now();
    }
    
    void markCompleted() {
        status = Status::Completed;
        completed_at = DebugTimestamp::now();
        updated_at = completed_at;
    }
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"package_id\": \"" << JsonUtils::escapeJson(package_id) << "\",\n";
        json << "  \"title\": \"" << JsonUtils::escapeJson(title) << "\",\n";
        json << "  \"description\": \"" << JsonUtils::escapeJson(description) << "\",\n";
        json << "  \"platform_version\": \"" << JsonUtils::escapeJson(platform_version) << "\",\n";
        json << "  \"created\": \"" << created_at.toISO8601() << "\",\n";
        
        const char* status_str = "";
        switch (status) {
            case Status::InProgress: status_str = "InProgress"; break;
            case Status::Completed: status_str = "Completed"; break;
            case Status::Archived: status_str = "Archived"; break;
            case Status::Superseded: status_str = "Superseded"; break;
        }
        json << "  \"status\": \"" << status_str << "\"\n";
        
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Package Components
// ============================================================================

/**
 * Build/commit information for reproducibility
 */
struct CommitInfo {
    std::string hash;
    std::string short_hash;
    std::string branch;
    std::string message;
    std::string author;
    std::string date;
    bool is_dirty{false};  // Uncommitted changes
    std::vector<std::string> modified_files;
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"hash\": \"" << JsonUtils::escapeJson(hash) << "\",\n";
        json << "  \"branch\": \"" << JsonUtils::escapeJson(branch) << "\",\n";
        json << "  \"message\": \"" << JsonUtils::escapeJson(message) << "\",\n";
        json << "  \"is_dirty\": " << (is_dirty ? "true" : "false") << "\n";
        json << "}";
        return json.str();
    }
};

/**
 * Environment snapshot
 */
struct EnvironmentSnapshot {
    std::string os_name;
    std::string os_version;
    std::string architecture;
    std::string hostname;
    int cpu_count{0};
    size_t total_memory_mb{0};
    std::map<std::string, std::string> relevant_variables;  // Non-sensitive ones only
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"os\": \"" << JsonUtils::escapeJson(os_name) << " " 
             << JsonUtils::escapeJson(os_version) << "\",\n";
        json << "  \"arch\": \"" << JsonUtils::escapeJson(architecture) << "\",\n";
        json << "  \"cpus\": " << cpu_count << ",\n";
        json << "  \"memory_mb\": " << total_memory_mb << "\n";
        json << "}";
        return json.str();
    }
};

/**
 * Crash information
 */
struct CrashRecord {
    std::string crash_id;
    DebugTimestamp timestamp;
    uint64_t frame_number{0};
    
    enum class CrashType {
        Segfault,
        Abort,
        Assertion,
        Exception,
        Hang,
        Unknown
    } type{CrashType::Unknown};
    
    std::string signal_name;
    std::string signal_number;
    std::string fault_address;
    
    // Stack trace
    std::vector<std::string> stack_trace;
    
    // Registers (if available)
    std::map<std::string, std::string> registers;
    
    // Thread info
    std::string thread_id;
    std::string thread_name;
    
    // Context
    Subsystem crash_subsystem{Subsystem::Unknown};
    SourceLocation location;
    
    // Related evidence
    std::vector<std::string> related_evidence_ids;
    
    static std::string generateId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "crash_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    CrashRecord() {
        crash_id = generateId();
        timestamp = DebugTimestamp::now();
    }
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"id\": \"" << JsonUtils::escapeJson(crash_id) << "\",\n";
        json << "  \"timestamp\": \"" << timestamp.toISO8601() << "\",\n";
        json << "  \"frame\": " << frame_number << ",\n";
        
        const char* type_str = "";
        switch (type) {
            case CrashType::Segfault: type_str = "Segfault"; break;
            case CrashType::Abort: type_str = "Abort"; break;
            case CrashType::Assertion: type_str = "Assertion"; break;
            case CrashType::Exception: type_str = "Exception"; break;
            case CrashType::Hang: type_str = "Hang"; break;
            case CrashType::Unknown: type_str = "Unknown"; break;
        }
        json << "  \"type\": \"" << type_str << "\",\n";
        json << "  \"signal\": \"" << JsonUtils::escapeJson(signal_name) << "\",\n";
        if (!fault_address.empty()) {
            json << "  \"address\": \"" << JsonUtils::escapeJson(fault_address) << "\",\n";
        }
        json << "  \"stack_frames\": " << stack_trace.size() << "\n";
        
        json << "}";
        return json.str();
    }
};

/**
 * Investigation summary (for quick loading)
 */
struct InvestigationSummary {
    std::string experiment_title;
    std::string goal;
    std::string result;
    bool root_cause_found{false};
    std::string root_cause_description;
    
    // Key findings
    std::vector<std::string> key_evidence_ids;
    std::string confirmed_hypothesis_id;
    
    // Statistics
    size_t total_evidence_count{0};
    size_t total_hypotheses_count{0};
    size_t investigation_duration_hours{0};
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"title\": \"" << JsonUtils::escapeJson(experiment_title) << "\",\n";
        json << "  \"goal\": \"" << JsonUtils::escapeJson(goal) << "\",\n";
        json << "  \"result\": \"" << JsonUtils::escapeJson(result) << "\",\n";
        json << "  \"root_cause_found\": " << (root_cause_found ? "true" : "false") << ",\n";
        if (root_cause_found) {
            json << "  \"root_cause\": \"" << JsonUtils::escapeJson(root_cause_description) << "\"\n";
        } else {
            json << "  \"evidence_count\": " << total_evidence_count << "\n";
        }
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Replay Package Builder
// ============================================================================

class ReplayPackageBuilder {
public:
    struct Config {
        fs::path output_directory;
        bool include_logs{true};
        bool include_screenshots{true};
        bool compress_output{false};
        bool auto_generate_summary{true};
        
        Config() : output_directory("debug_replay") {}
        
        static Config defaultConfig() {
            Config c;
            return c;
        }
    };
    
    explicit ReplayPackageBuilder(const Config& config = Config())
        : m_config(config), m_initialized(false) {}
    
    /**
     * Initialize builder and create package directory
     */
    bool initialize(const std::string& title, const std::string& description = "") {
        try {
            m_metadata.title = title;
            m_metadata.description = description;
            m_metadata.platform_version = PLATFORM_VERSION;
            
            // Create package directory with ID
            m_package_dir = m_config.output_directory / 
                            ("package_" + DebugTimestamp::now().toISO8601().substr(0, 19));
            
            fs::create_directories(m_package_dir / "logs");
            fs::create_directories(m_package_dir / "screenshots");
            
            m_initialized = true;
            return true;
        } catch (const std::exception& e) {
            m_last_error = e.what();
            return false;
        }
    }
    
    bool isInitialized() const { return m_initialized; }
    std::string getLastError() const { return m_last_error; }
    fs::path getPackagePath() const { return m_package_dir; }
    
    // ========================================================================
    // Data Collection Interface
    // ========================================================================
    
    /**
     * Set commit/build information
     */
    void setCommitInfo(const CommitInfo& info) {
        m_commit_info = info;
    }
    
    /**
     * Set environment snapshot
     */
    void setEnvironmentSnapshot(const EnvironmentSnapshot& env) {
        m_environment = env;
    }
    
    /**
     * Add crash record
     */
    void addCrashRecord(const CrashRecord& crash) {
        m_crashes.push_back(crash);
    }
    
    /**
     * Set investigation summary
     */
    void setInvestigationSummary(const InvestigationSummary& summary) {
        m_summary = summary;
    }
    
    /**
     * Add a JSON component file (timeline, memory events, etc.)
     */
    bool addComponentFile(
        const std::string& component_name,
        const std::string& json_content) {
        
        if (!m_initialized) return false;
        
        try {
            fs::path file_path = m_package_dir / (component_name + ".json");
            std::ofstream file(file_path);
            if (!file.is_open()) return false;
            
            file << json_content;
            file.close();
            
            m_component_files.push_back(component_name);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    /**
     * Copy a log file into the package
     */
    bool addLogFile(const fs::path& source_log, const std::string& name = "") {
        if (!m_initialized || !fs::exists(source_log)) return false;
        
        try {
            std::string filename = name.empty() ? source_log.filename().string() : name;
            fs::path dest = m_package_dir / "logs" / filename;
            
            fs::copy_file(source_log, dest);
            m_log_files.push_back(filename);
            
            return true;
        } catch (...) {
            return false;
        }
    }
    
    /**
     * Copy a screenshot into the package
     */
    bool addScreenshot(const fs::path& source_image, const std::string& name = "") {
        if (!m_initialized || !fs::exists(source_image)) return false;
        
        try {
            std::string filename = name.empty() ? source_image.filename().string() : name;
            fs::path dest = m_package_dir / "screenshots" / filename;
            
            fs::copy_file(source_image, dest);
            m_screenshot_files.push_back(filename);
            
            return true;
        } catch (...) {
            return false;
        }
    }
    
    // ========================================================================
    // Package Generation
    // ========================================================================
    
    /**
     * Generate complete replay package
     * Returns path to manifest file
     */
    std::optional<fs::path> buildPackage() {
        if (!m_initialized) return std::nullopt;
        
        try {
            // Update metadata
            m_metadata.updated_at = DebugTimestamp::now();
            m_metadata.components_included = m_component_files;
            
            // Write manifest
            writeManifest();
            
            // Write commit info
            if (!m_commit_info.hash.empty()) {
                writeFile("commit", m_commit_info.toJson());
            }
            
            // Write environment
            if (!m_environment.os_name.empty()) {
                writeFile("environment", m_environment.toJson());
            }
            
            // Write crashes
            if (!m_crashes.empty()) {
                std::stringstream crash_json;
                crash_json << "[\n";
                for (size_t i = 0; i < m_crashes.size(); ++i) {
                    crash_json << "  " << m_crashes[i].toJson();
                    if (i < m_crashes.size() - 1) crash_json << ",";
                    crash_json << "\n";
                }
                crash_json << "]\n";
                writeFile("crashes", crash_json.str());
            }
            
            // Write summary
            if (m_config.auto_generate_summary || !m_summary.goal.empty()) {
                writeFile("summary", m_summary.toJson());
            }
            
            // Calculate total size
            m_metadata.total_size_bytes = calculateTotalSize();
            
            // Mark as completed
            m_metadata.markCompleted();
            
            // Rewrite manifest with final metadata
            writeManifest();
            
            return m_package_dir / "manifest.json";
            
        } catch (const std::exception& e) {
            m_last_error = e.what();
            return std::nullopt;
        }
    }

private:
    Config m_config;
    bool m_initialized;
    mutable std::string m_last_error;
    
    fs::path m_package_dir;
    PackageMetadata m_metadata;
    
    CommitInfo m_commit_info;
    EnvironmentSnapshot m_environment;
    std::vector<CrashRecord> m_crashes;
    InvestigationSummary m_summary;
    
    std::vector<std::string> m_component_files;
    std::vector<std::string> m_log_files;
    std::vector<std::string> m_screenshot_files;
    
    // ========================================================================
    // Internal Methods
    // ========================================================================
    
    void writeManifest() {
        fs::path manifest_path = m_package_dir / "manifest.json";
        
        std::ofstream file(manifest_path);
        if (!file.is_open()) return;
        
        file << "{\n";
        file << "  \"metadata\": " << m_metadata.toJson() << ",\n";
        
        // File listing
        file << "  \"files\": {\n";
        file << "    \"components\": [";
        for (size_t i = 0; i < m_component_files.size(); ++i) {
            file << "\"" << JsonUtils::escapeJson(m_component_files[i]) << ".json\"";
            if (i < m_component_files.size() - 1) file << ", ";
        }
        file << "],\n";
        
        file << "    \"logs\": [";
        for (size_t i = 0; i < m_log_files.size(); ++i) {
            file << "\"" << JsonUtils::escapeJson(m_log_files[i]) << "\"";
            if (i < m_log_files.size() - 1) file << ", ";
        }
        file << "],\n";
        
        file << "    \"screenshots\": [";
        for (size_t i = 0; i < m_screenshot_files.size(); ++i) {
            file << "\"" << JsonUtils::escapeJson(m_screenshot_files[i]) << "\"";
            if (i < m_screenshot_files.size() - 1) file << ", ";
        }
        file << "]\n";
        
        file << "  }\n";
        
        file << "}\n";
    }
    
    void writeFile(const std::string& name, const std::string& content) {
        fs::path file_path = m_package_dir / (name + ".json");
        std::ofstream file(file_path);
        if (file.is_open()) {
            file << content;
        }
    }
    
    size_t calculateTotalSize() const {
        size_t total = 0;
        
        // Component files
        for (const auto& name : m_component_files) {
            fs::path p = m_package_dir / (name + ".json");
            if (fs::exists(p)) total += fs::file_size(p);
        }
        
        // Log files
        for (const auto& name : m_log_files) {
            fs::path p = m_package_dir / "logs" / name;
            if (fs::exists(p)) total += fs::file_size(p);
        }
        
        // Screenshots
        for (const auto& name : m_screenshot_files) {
            fs::path p = m_package_dir / "screenshots" / name;
            if (fs::exists(p)) total += fs::file_size(p);
        }
        
        return total;
    }
};

// ============================================================================
// Package Loader - For Reopening Investigations
// ============================================================================

class ReplayPackageLoader {
public:
    /**
     * Load an existing replay package
     */
    static std::optional<PackageMetadata> loadManifest(const fs::path& package_dir) {
        fs::path manifest_path = package_dir / "manifest.json";
        
        if (!fs::exists(manifest_path)) return std::nullopt;
        
        // Parse JSON (simplified - in production use proper parser)
        std::ifstream file(manifest_path);
        if (!file.is_open()) return std::nullopt;
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        
        PackageMetadata meta;
        
        // Extract basic fields
        auto extractString = [&content](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\": \"";
            auto pos = content.find(search);
            if (pos == std::string::npos) return "";
            
            pos += search.length();
            auto end = content.find("\"", pos);
            if (end == std::string::npos) return "";
            
            return content.substr(pos, end - pos);
        };
        
        meta.package_id = extractString("package_id");
        meta.title = extractString("title");
        meta.description = extractString("description");
        meta.platform_version = extractString("platform_version");
        
        if (meta.package_id.empty()) return std::nullopt;
        
        meta.package_id = package_dir.filename().string();  // Use directory name
        
        return meta;
    }
    
    /**
     * Load a specific component JSON file
     */
    static std::optional<std::string> loadComponent(
        const fs::path& package_dir,
        const std::string& component_name) {
        
        fs::path file_path = package_dir / (component_name + ".json");
        
        if (!fs::exists(file_path)) return std::nullopt;
        
        std::ifstream file(file_path);
        if (!file.is_open()) return std::nullopt;
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    /**
     * List all packages in a directory
     */
    static std::vector<PackageMetadata> listPackages(const fs::path& base_dir) {
        std::vector<PackageMetadata> packages;
        
        if (!fs::exists(base_dir)) return packages;
        
        for (const auto& entry : fs::directory_iterator(base_dir)) {
            if (entry.is_directory()) {
                auto meta = loadManifest(entry.path());
                if (meta) {
                    packages.push_back(*meta);
                }
            }
        }
        
        // Sort by creation time (newest first)
        std::sort(packages.begin(), packages.end(),
            [](const PackageMetadata& a, const PackageMetadata& b) {
                return a.created_at > b.created_at;
            });
        
        return packages;
    }
    
    /**
     * Find packages matching criteria
     */
    static std::vector<PackageMetadata> findPackages(
        const fs::path& base_dir,
        const std::string& query_text,
        size_t limit = 20) {
        
        auto all_packages = listPackages(base_dir);
        std::vector<PackageMetadata> results;
        
        // Simple text search
        std::string lower_query = query_text;
        std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
        
        for (const auto& pkg : all_packages) {
            std::string combined = pkg.title + " " + pkg.description;
            std::transform(combined.begin(), combined.end(), combined.begin(), ::tolower);
            
            if (combined.find(lower_query) != std::string::npos) {
                results.push_back(pkg);
                
                if (results.size() >= limit) break;
            }
        }
        
        return results;
    }
};

} // namespace replay
} // namespace prosper_debug


